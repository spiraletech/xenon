#include "xenon/autonomous_producer.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace xenon {
namespace {
std::string stage_name(ProducerStage s) {
    switch(s) {
        case ProducerStage::Planned:return "planned"; case ProducerStage::Generated:return "generated";
        case ProducerStage::Auditioned:return "auditioned"; case ProducerStage::Revised:return "revised";
        case ProducerStage::Mixed:return "mixed"; case ProducerStage::Mastered:return "mastered";
        case ProducerStage::AwaitingApproval:return "awaiting approval"; case ProducerStage::Complete:return "complete";
        case ProducerStage::Failed:return "failed";
    } return "unknown";
}
void checkpoint(AutonomousProducerResult& r, ProducerStage s, std::size_t i, std::string node, std::string note) {
    r.stage=s; r.checkpoints.push_back({s,i,std::move(node),std::move(note)});
}
}

AutonomousProducer::AutonomousProducer(GenerationPipeline pipeline) : pipeline_(std::move(pipeline)) {}

void AutonomousProducer::validate(const AutonomousProducerGoal& g) const {
    if(g.project_id.empty() || g.goal.empty()) throw std::invalid_argument("L39 project id and goal are required");
    if(g.track_count==0 || g.track_count>32) throw std::invalid_argument("L39 track count must be 1..32");
    if(g.duration_seconds<=0.0 || g.duration_seconds>1800.0) throw std::invalid_argument("L39 duration must be >0 and <=1800 seconds");
    if(g.bpm<0.0 || g.bpm>400.0) throw std::invalid_argument("L39 bpm out of range");
    if(g.max_revision_passes>8) throw std::invalid_argument("L39 revision budget exceeds safety bound");
    if(g.minimum_candidate_score<0.0 || g.minimum_candidate_score>1.0) throw std::invalid_argument("L39 candidate threshold out of range");
}

std::vector<AutonomousTrackPlan> AutonomousProducer::plan(const AutonomousProducerGoal& g) const {
    validate(g); std::vector<AutonomousTrackPlan> out; out.reserve(g.track_count);
    for(std::size_t i=0;i<g.track_count;++i) {
        AutonomousTrackPlan p; p.track_index=i; p.title="Track " + std::to_string(i+1);
        p.request.prompt=g.goal + " | " + p.title + " | cohesive project sequence " + std::to_string(i+1) + "/" + std::to_string(g.track_count);
        p.request.duration_seconds=g.duration_seconds; p.request.bpm=g.bpm; p.request.key=g.key; p.request.seed=g.seed+i; p.request.render_intent=RenderIntent::Quality;
        p.request.mutation_amount = g.track_count>1 ? std::clamp(0.25 + (static_cast<double>(i)/static_cast<double>(g.track_count))*0.20,0.0,1.0) : 0.30;
        out.push_back(std::move(p));
    } return out;
}

GenerationRequest AutonomousProducer::revision_request(const AutonomousTrackPlan& track, const CandidateRecord& winner, const CortexCritique& critique, std::uint32_t pass) const {
    auto r=track.request; r.mode=GenerationMode::Variation; r.reference_audio=winner.generation.artifact.audio_path; r.seed=winner.generation.artifact.resolved_seed + pass + 1;
    r.mutation_amount=std::clamp(0.12 + 0.06*pass,0.0,0.45); r.control.reference_strength=0.88;
    r.prompt += " | surgical revision: " + (critique.revision_hint.empty() ? std::string("refine weakest musical dimension") : critique.revision_hint);
    return r;
}

AutonomousProducerResult AutonomousProducer::produce(const AutonomousProducerGoal& g, const std::filesystem::path& output_directory) {
    validate(g); AutonomousProducerResult result; result.project_id=g.project_id;
    SessionOS session(g.project_id); const auto root=session.add_reference("Autonomous Producer Goal", g.goal);
    checkpoint(result,ProducerStage::Planned,0,root,"project goal accepted; user remains release authority");
    const auto plans=plan(g); result.tracks.reserve(plans.size());
    for(const auto& p:plans) {
        const auto prompt_node=session.add_prompt(root,p.request.prompt);
        CandidateSwarmEngine swarm(std::move(pipeline_));
        auto pool=swarm.generate_ranked(p.request,output_directory/("track-"+std::to_string(p.track_index+1)));
        auto winner=pool.winner();
        AutonomousTrackResult tr; tr.plan=p; tr.winner=winner; tr.critiques.push_back(winner.critique);
        const auto dna=session.add_ether_dna(prompt_node,winner.generation.dna.fingerprint);
        tr.generation_node_id=session.add_generation(dna,winner.generation.artifact.audio_path.string(),winner.generation.dna.fingerprint);
        checkpoint(result,ProducerStage::Generated,p.track_index,tr.generation_node_id,"candidate swarm generated releasable winner");
        checkpoint(result,ProducerStage::Auditioned,p.track_index,tr.generation_node_id,"winner auditioned and ranked; score="+std::to_string(winner.ranking_score));

        // Revision planning is bounded and inspectable. The current pipeline may reject Variation
        // when no installed backend supports reference audio; in that case preserve the winner.
        for(std::uint32_t pass=0; pass<g.max_revision_passes && tr.winner.ranking_score<g.minimum_candidate_score; ++pass) {
            const auto rr=revision_request(p,tr.winner,tr.winner.critique,pass);
            try {
                auto revised=pipeline_.generate(rr,output_directory/("track-"+std::to_string(p.track_index+1))/("revision-"+std::to_string(pass+1)),tr.winner.generation.dna.fingerprint);
                tr.revision_passes++;
                tr.revision_node_id=session.add_revision(tr.generation_node_id,"Autonomous surgical revision "+std::to_string(pass+1),revised.artifact.audio_path.string());
                checkpoint(result,ProducerStage::Revised,p.track_index,tr.revision_node_id,"bounded revision rendered");
            } catch(const std::exception& ex) {
                checkpoint(result,ProducerStage::Revised,p.track_index,tr.generation_node_id,std::string("revision unavailable; preserved approved candidate boundary: ")+ex.what());
                break;
            }
        }
        const auto source=tr.revision_node_id.empty()?tr.generation_node_id:tr.revision_node_id;
        // L35/L36 are represented as explicit session stages here. Audio execution remains owned by
        // MixEngine/MasteringEngine when stems/PCM are supplied by the production backend.
        tr.mix_node_id=session.add_mix(source,"session://track-"+std::to_string(p.track_index+1)+"/mix");
        checkpoint(result,ProducerStage::Mixed,p.track_index,tr.mix_node_id,"mix stage scheduled in project graph");
        tr.master_node_id=session.add_master(tr.mix_node_id,"session://track-"+std::to_string(p.track_index+1)+"/master");
        checkpoint(result,ProducerStage::Mastered,p.track_index,tr.master_node_id,"master stage scheduled in project graph");
        result.tracks.push_back(std::move(tr));
        pipeline_=std::move(swarm).release_pipeline();
    }
    result.session_snapshot=session.graph().serialize();
    result.awaiting_user_approval=g.require_user_approval;
    if(g.require_user_approval) checkpoint(result,ProducerStage::AwaitingApproval,0,session.graph().head_id(),"autonomous work paused for explicit user approval");
    else { for(auto& t:result.tracks)t.approved=true; checkpoint(result,ProducerStage::Complete,0,session.graph().head_id(),"autonomous project completed within policy"); }
    return result;
}

void AutonomousProducer::approve(AutonomousProducerResult& r,std::size_t i,bool approved) const {
    if(i>=r.tracks.size()) throw std::out_of_range("L39 track approval index out of range"); r.tracks[i].approved=approved;
}
void AutonomousProducer::finalize(AutonomousProducerResult& r) const {
    if(r.tracks.empty()) throw std::runtime_error("L39 cannot finalize empty project");
    if(std::any_of(r.tracks.begin(),r.tracks.end(),[](const auto& t){return !t.approved;})) throw std::runtime_error("L39 user approval required for every track");
    r.awaiting_user_approval=false; r.stage=ProducerStage::Complete; r.checkpoints.push_back({ProducerStage::Complete,0,{},"user-approved autonomous project finalized"});
}

} // namespace xenon
