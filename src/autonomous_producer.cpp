#include "xenon/autonomous_producer.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace xenon {
namespace {
void checkpoint(AutonomousProducerResult& r, ProducerStage s, std::size_t i, std::string node, std::string note) { r.stage=s; r.checkpoints.push_back({s,i,std::move(node),std::move(note)}); }
std::vector<float> load_pcm16_as_stereo(const std::filesystem::path& path) {
    std::ifstream in(path,std::ios::binary); if(!in) throw std::runtime_error("L39 cannot open generated audio for mix");
    in.seekg(0,std::ios::end); const auto size=in.tellg(); if(size<44) throw std::runtime_error("L39 generated WAV is too small"); in.seekg(44);
    std::vector<std::int16_t> pcm(static_cast<std::size_t>(size-std::streamoff(44))/2); in.read(reinterpret_cast<char*>(pcm.data()),static_cast<std::streamsize>(pcm.size()*2));
    std::vector<float> stereo; stereo.reserve(pcm.size()*2); for(auto s:pcm){const float v=static_cast<float>(s)/32768.0f; stereo.push_back(v); stereo.push_back(v);} return stereo;
}
}
AutonomousProducer::AutonomousProducer(GenerationPipeline pipeline) : pipeline_(std::move(pipeline)) {}
void AutonomousProducer::validate(const AutonomousProducerGoal& g) const {
    if(g.project_id.empty()||g.goal.empty()) throw std::invalid_argument("L39 project id and goal are required");
    if(g.track_count==0||g.track_count>32) throw std::invalid_argument("L39 track count must be 1..32");
    if(g.duration_seconds<=0.0||g.duration_seconds>1800.0) throw std::invalid_argument("L39 duration must be >0 and <=1800 seconds");
    if(g.bpm<0.0||g.bpm>400.0) throw std::invalid_argument("L39 bpm out of range");
    if(g.max_revision_passes>8) throw std::invalid_argument("L39 revision budget exceeds safety bound");
    if(g.minimum_candidate_score<0.0||g.minimum_candidate_score>1.0) throw std::invalid_argument("L39 candidate threshold out of range");
}
std::vector<AutonomousTrackPlan> AutonomousProducer::plan(const AutonomousProducerGoal& g) const {
    validate(g); std::vector<AutonomousTrackPlan> out; out.reserve(g.track_count);
    for(std::size_t i=0;i<g.track_count;++i){ AutonomousTrackPlan p; p.track_index=i; p.title="Track "+std::to_string(i+1); p.request.prompt=g.goal+" | "+p.title+" | cohesive project sequence "+std::to_string(i+1)+"/"+std::to_string(g.track_count); p.request.duration_seconds=g.duration_seconds; p.request.bpm=g.bpm; p.request.key=g.key; p.request.seed=g.seed+i; p.request.render_intent=RenderIntent::Quality; p.request.mutation_amount=g.track_count>1?std::clamp(0.25+(static_cast<double>(i)/static_cast<double>(g.track_count))*0.20,0.0,1.0):0.30; out.push_back(std::move(p)); } return out;
}
GenerationRequest AutonomousProducer::revision_request(const AutonomousTrackPlan& track,const CandidateRecord& winner,const CortexCritique& critique,std::uint32_t pass) const {
    auto r=track.request; r.mode=GenerationMode::Variation; r.reference_audio=winner.generation.artifact.audio_path; r.seed=winner.generation.artifact.resolved_seed+pass+1; r.mutation_amount=std::clamp(0.12+0.06*pass,0.0,0.45); r.control.reference_strength=0.88; r.prompt+=" | surgical revision: "+(critique.revision_hint.empty()?std::string("refine weakest musical dimension"):critique.revision_hint); return r;
}
AutonomousProducerResult AutonomousProducer::produce(const AutonomousProducerGoal& g,const std::filesystem::path& output_directory) {
    validate(g); AutonomousProducerResult result; result.project_id=g.project_id; SessionOS session(g.project_id); const auto root=session.add_reference("Autonomous Producer Goal",g.goal); checkpoint(result,ProducerStage::Planned,0,root,"project goal accepted; user remains release authority");
    const auto plans=plan(g); result.tracks.reserve(plans.size());
    for(const auto& p:plans){
        const auto prompt_node=session.add_prompt(root,p.request.prompt); CandidateSwarmEngine swarm(std::move(pipeline_)); auto pool=swarm.generate_ranked(p.request,output_directory/("track-"+std::to_string(p.track_index+1))); pipeline_=std::move(swarm).release_pipeline();
        auto winner=pool.winner(); AutonomousTrackResult tr; tr.plan=p; tr.winner=winner; tr.critiques.push_back(winner.critique); std::filesystem::path active_audio=winner.generation.artifact.audio_path;
        const auto dna=session.add_ether_dna(prompt_node,winner.generation.dna.fingerprint); tr.generation_node_id=session.add_generation(dna,active_audio.string(),winner.generation.dna.fingerprint); checkpoint(result,ProducerStage::Generated,p.track_index,tr.generation_node_id,"candidate swarm generated releasable winner"); checkpoint(result,ProducerStage::Auditioned,p.track_index,tr.generation_node_id,"winner auditioned and ranked; score="+std::to_string(winner.ranking_score));
        for(std::uint32_t pass=0;pass<g.max_revision_passes&&tr.winner.ranking_score<g.minimum_candidate_score;++pass){ const auto rr=revision_request(p,tr.winner,tr.winner.critique,pass); try{ auto revised=pipeline_.generate(rr,output_directory/("track-"+std::to_string(p.track_index+1))/("revision-"+std::to_string(pass+1)),tr.winner.generation.dna.fingerprint); active_audio=revised.artifact.audio_path; tr.revision_passes++; tr.revision_node_id=session.add_revision(tr.revision_node_id.empty()?tr.generation_node_id:tr.revision_node_id,"Autonomous surgical revision "+std::to_string(pass+1),active_audio.string()); checkpoint(result,ProducerStage::Revised,p.track_index,tr.revision_node_id,"bounded revision rendered"); }catch(const std::exception& ex){ checkpoint(result,ProducerStage::Revised,p.track_index,tr.generation_node_id,std::string("revision backend unavailable; preserved winner: ")+ex.what()); break; } }
        const auto source=tr.revision_node_id.empty()?tr.generation_node_id:tr.revision_node_id;
        StemMixInput stem; stem.stem_id="autonomous-fullmix"; stem.role=StemRole::Texture; stem.analysis.rms=winner.synesthesia.energy; stem.analysis.brightness=winner.synesthesia.brightness_balance; stem.analysis.spectral_density=winner.synesthesia.density_balance; stem.analysis.transient_density=winner.synesthesia.rhythmic_balance; stem.interleaved_stereo=load_pcm16_as_stereo(active_audio);
        MixEngine mixer; tr.mix=mixer.mix({stem},44100); tr.mix_node_id=session.add_mix(source,"memory://track-"+std::to_string(p.track_index+1)+"/mix"); checkpoint(result,ProducerStage::Mixed,p.track_index,tr.mix_node_id,"L35 MixEngine executed on selected production audio");
        MasteringEngine mastering; tr.master=mastering.master(tr.mix.interleaved_stereo); tr.master_node_id=session.add_master(tr.mix_node_id,"memory://track-"+std::to_string(p.track_index+1)+"/master"); checkpoint(result,ProducerStage::Mastered,p.track_index,tr.master_node_id,"L36 MasteringEngine executed; quality gate="+std::string(tr.master.quality_gate_passed?"pass":"review")); result.tracks.push_back(std::move(tr));
    }
    result.session_snapshot=session.graph().serialize(); result.awaiting_user_approval=g.require_user_approval; if(g.require_user_approval) checkpoint(result,ProducerStage::AwaitingApproval,0,session.graph().head_id(),"autonomous work paused for explicit user approval"); else {for(auto& t:result.tracks)t.approved=true; checkpoint(result,ProducerStage::Complete,0,session.graph().head_id(),"autonomous project completed within policy");} return result;
}
void AutonomousProducer::approve(AutonomousProducerResult& r,std::size_t i,bool approved) const { if(i>=r.tracks.size()) throw std::out_of_range("L39 track approval index out of range"); r.tracks[i].approved=approved; }
void AutonomousProducer::finalize(AutonomousProducerResult& r) const { if(r.tracks.empty()) throw std::runtime_error("L39 cannot finalize empty project"); if(std::any_of(r.tracks.begin(),r.tracks.end(),[](const auto& t){return !t.approved;})) throw std::runtime_error("L39 user approval required for every track"); r.awaiting_user_approval=false; r.stage=ProducerStage::Complete; r.checkpoints.push_back({ProducerStage::Complete,0,{},"user-approved autonomous project finalized"}); }
} // namespace xenon
