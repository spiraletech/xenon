#include "xenon/autonomous_producer.hpp"
#include "xenon/engine.hpp"
#include "xenon/model_router.hpp"

#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace {
class ProducerBackend final : public xenon::IModelBackend {
public:
    std::string_view name() const noexcept override { return "l39-producer"; }
    xenon::ProviderCapabilities capabilities() const noexcept override { return xenon::ProviderCapability::TextToInstrumental|xenon::ProviderCapability::Variation|xenon::ProviderCapability::ReferenceAudio|xenon::ProviderCapability::QualityRole|xenon::ProviderCapability::LocalRuntime; }
    xenon::RuntimeType runtime_type() const noexcept override { return xenon::RuntimeType::Local; }
    xenon::GenerationArtifact generate(const xenon::GenerationRequest& r,const std::filesystem::path& out) override {
        xenon::music::ProductionIntentV1 i; i.request_id="l39"; i.project_id="autonomous"; i.prompt=r.prompt; i.duration_seconds=r.duration_seconds; i.bpm=r.bpm; i.key=r.key; i.seed=r.seed; i.mutation_amount=r.mutation_amount;
        const auto a=engine_.render(i,out); return {a.audio_path,a.metadata_path,"l39-producer",a.resolved_seed};
    }
private: xenon::Engine engine_;
};
void require(bool c,const char* m){if(!c)throw std::runtime_error(m);}
}
int main(){
    try{
        xenon::ModelRouter router; router.add_provider(std::make_unique<ProducerBackend>(),100);
        xenon::AutonomousProducer producer{xenon::GenerationPipeline{std::move(router)}};
        xenon::AutonomousProducerGoal goal; goal.project_id="l39-ep"; goal.goal="three-track dark ether grunge EP with cohesive DNA"; goal.track_count=3; goal.duration_seconds=0.5; goal.bpm=108; goal.key="C# minor"; goal.seed=3900; goal.max_revision_passes=1; goal.minimum_candidate_score=1.0; goal.require_user_approval=true;
        const auto plans=producer.plan(goal); require(plans.size()==3,"L39 did not plan three tracks"); require(plans[0].request.seed!=plans[1].request.seed,"L39 track seeds not diversified");
        const auto out=std::filesystem::temp_directory_path()/"xenon_l39_autonomous"; std::filesystem::remove_all(out);
        auto result=producer.produce(goal,out); require(result.tracks.size()==3,"L39 did not produce full EP"); require(result.awaiting_user_approval&&result.stage==xenon::ProducerStage::AwaitingApproval,"L39 bypassed user approval gate"); require(!result.session_snapshot.empty(),"L39 session graph snapshot missing");
        for(const auto& t:result.tracks){ require(!t.winner.generation.artifact.audio_path.empty(),"L39 winner missing"); require(t.revision_passes==1,"L39 bounded revision did not execute"); require(!t.mix.interleaved_stereo.empty()&&t.mix.passes>0,"L39 MixEngine did not execute"); require(!t.master.interleaved_stereo.empty()&&t.master.passes>0,"L39 MasteringEngine did not execute"); require(!t.generation_node_id.empty()&&!t.revision_node_id.empty()&&!t.mix_node_id.empty()&&!t.master_node_id.empty(),"L39 project graph lineage incomplete"); }
        bool blocked=false; try{producer.finalize(result);}catch(const std::runtime_error&){blocked=true;} require(blocked,"L39 finalized without user approval");
        for(std::size_t i=0;i<result.tracks.size();++i)producer.approve(result,i,true); producer.finalize(result); require(result.stage==xenon::ProducerStage::Complete&&!result.awaiting_user_approval,"L39 approved project did not finalize");
        bool invalid=false; try{auto bad=goal;bad.track_count=0;producer.validate(bad);}catch(const std::invalid_argument&){invalid=true;} require(invalid,"L39 accepted empty project plan");
        std::filesystem::remove_all(out); std::cout<<"L39 Autonomous Producer smoke passed\n"; return 0;
    }catch(const std::exception& ex){std::cerr<<"L39 smoke failed: "<<ex.what()<<'\n';return 1;}
}
