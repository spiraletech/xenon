#include "xenon/music_os.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace xenon {

MusicOS::MusicOS(ModelRouter router)
    : router_(std::move(router)), pipeline_(std::move(router_)) {}

void MusicOS::open_project(std::string id) {
    if (id.empty()) throw std::invalid_argument("XENON MusicOS project id is required");
    project_id_ = std::move(id);
    memory_.set_project(project_id_);
    pipeline_.set_music_memory(memory_);
    session_ = std::make_unique<SessionOS>(project_id_);
}

const std::string& MusicOS::project_id() const noexcept { return project_id_; }
void MusicOS::require_project() const { if (!session_) throw std::runtime_error("XENON MusicOS project is not open"); }
SessionOS& MusicOS::session() { require_project(); return *session_; }
OrganicMusicMemory& MusicOS::memory() noexcept { return memory_; }
TrinityDeviceBus& MusicOS::devices() noexcept { return devices_; }
GenerationPipeline& MusicOS::generation() noexcept { return pipeline_; }
ModelRouter& MusicOS::models() noexcept { return pipeline_.mutable_router(); }

void MusicOS::open_audio(const std::filesystem::path& path) {
    player_.open(path); audio_open_ = true;
}
TrackAnalysis MusicOS::analyze_audio(const std::filesystem::path& path) const { return analyzer_.analyzeFile(path); }
SpectrumFrame MusicOS::current_spectrum() const {
    if (!audio_open_) throw std::runtime_error("XENON MusicOS has no open audio");
    return player_.currentSpectrum();
}

GenerationResult MusicOS::create(const GenerationRequest& request, const std::filesystem::path& out) {
    require_project();
    pipeline_.set_music_memory(memory_);
    return pipeline_.generate(request, out);
}
MixResult MusicOS::mix(const std::vector<StemMixInput>& stems, std::uint32_t sr, std::optional<MixReferenceTarget> ref) const {
    return mixer_.mix(stems, sr, ref);
}
MasteringResult MusicOS::master(const std::vector<float>& stereo) const { return mastering_.master(stereo); }
AutonomousProducerResult MusicOS::produce(const AutonomousProducerGoal& goal, const std::filesystem::path& out) {
    require_project(); autonomous_active_ = true;
    try {
        pipeline_.set_music_memory(memory_);
        AutonomousProducer producer(pipeline_.clone());
        auto result = producer.produce(goal, out);
        autonomous_active_ = false;
        return result;
    } catch (...) { autonomous_active_ = false; throw; }
}

MusicOSState MusicOS::state() const {
    MusicOSState s; s.version = XENON_VERSION; s.project_id = project_id_; s.project_open = session_ != nullptr;
    s.audio_open = audio_open_; s.autonomous_job_active = autonomous_active_; s.model_provider_count = pipeline_.router().provider_count();
    auto add=[&](MusicOSDomain d,const char* n,bool a,const char* detail){s.capabilities.push_back({d,n,a,detail});};
    add(MusicOSDomain::Ears,"playback",true,"PlayerEngine"); add(MusicOSDomain::Ears,"analysis",true,"MediaAnalyzer + SpectrumAnalyzer + Synesthesia");
    add(MusicOSDomain::Mind,"cortex-composer",true,"Cortex + ComposerAgent + MusicianEnsemble"); add(MusicOSDomain::Mind,"autonomous-producer",true,"bounded user-directed producer orchestration");
    add(MusicOSDomain::Hands,"generation",s.model_provider_count>0,"GenerationPipeline + ModelRouter"); add(MusicOSDomain::Hands,"mix-master",true,"MixEngine + MasteringEngine");
    add(MusicOSDomain::Memory,"organic",true,"OrganicMusicMemory"); add(MusicOSDomain::Memory,"project-graph",s.project_open,"SessionOS + EtherDNA lineage");
    add(MusicOSDomain::Models,"provider-router",s.model_provider_count>0,"capability-aware local/remote routing");
    add(MusicOSDomain::Devices,"trinity",true,"Sigil Guitar + GlyphPad + ThreadDeck bus");
    return s;
}

std::string MusicOS::manifest() const {
    const auto s=state(); std::ostringstream o;
    o << "XENON_MUSIC_OS|1\nversion=" << s.version << "\nproject=" << s.project_id << "\nproviders=" << s.model_provider_count << "\n";
    for (const auto& c:s.capabilities) o << "cap=" << static_cast<int>(c.domain) << '|' << c.name << '|' << (c.available?1:0) << '|' << c.detail << "\n";
    return o.str();
}

} // namespace xenon
