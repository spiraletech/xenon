#include "xenon/native_midi_input.hpp"
#include "xenon/runtime_backend_manager.hpp"
#include "xenon/trinity_device_adapter.hpp"
#include "xenon/vst3_native_host.hpp"

#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
class FixtureBackend final : public xenon::IModelBackend {
public:
    std::string_view name() const noexcept override { return "fixture"; }
    xenon::ProviderCapabilities capabilities() const noexcept override {
        return xenon::ProviderCapability::TextToInstrumental | xenon::ProviderCapability::QualityRole | xenon::ProviderCapability::LocalRuntime;
    }
    xenon::RuntimeType runtime_type() const noexcept override { return xenon::RuntimeType::Local; }
    xenon::GenerationArtifact generate(const xenon::GenerationRequest& r,const std::filesystem::path& out) override {
        std::filesystem::create_directories(out);auto p=out/"fixture.wav";
        std::ofstream f(p,std::ios::binary);f.write("RIFF0000WAVE",12);f.close();
        xenon::GenerationArtifact a;a.audio_path=p;a.backend_name="fixture";a.resolved_seed=r.seed;return a;
    }
};
class MissingArtifactBackend final : public xenon::IModelBackend {
public:
    std::string_view name() const noexcept override { return "missing"; }
    xenon::ProviderCapabilities capabilities() const noexcept override {
        return xenon::ProviderCapability::TextToInstrumental | xenon::ProviderCapability::QualityRole | xenon::ProviderCapability::LocalRuntime;
    }
    xenon::RuntimeType runtime_type() const noexcept override { return xenon::RuntimeType::Local; }
    xenon::GenerationArtifact generate(const xenon::GenerationRequest& r,const std::filesystem::path&) override {
        xenon::GenerationArtifact a;a.audio_path="definitely-not-created.wav";a.backend_name="missing";a.resolved_seed=r.seed;return a;
    }
};
}

int main(){
    namespace fs=std::filesystem;
    xenon::BackendRegistry registry;
    registry.register_factory("fixture",[]{return std::make_unique<FixtureBackend>();});
    registry.register_factory("missing",[]{return std::make_unique<MissingArtifactBackend>();});
    xenon::RuntimeBackendManager manager(std::move(registry));
    if(manager.loaded("fixture"))return 1;
    manager.load("fixture");if(!manager.loaded("fixture"))return 2;

    std::vector<double> progress;
    xenon::RuntimeJobControl control([&](double p,const std::string&){progress.push_back(p);});
    xenon::GenerationRequest req;req.prompt="runtime systems";req.duration_seconds=1;req.seed=4141;
    auto out=fs::temp_directory_path()/"xenon_l41_runtime_systems";fs::remove_all(out);
    auto artifact=manager.generate("fixture",req,out,control);
    if(!fs::exists(artifact.audio_path)||progress.empty()||progress.front()>.05||progress.back()!=1.0)return 3;
    for(std::size_t i=1;i<progress.size();++i)if(progress[i]<progress[i-1])return 4;

    xenon::RuntimeJobControl cancelled;cancelled.cancel();bool cancelled_rejected=false;
    try{(void)manager.generate("fixture",req,out,cancelled);}catch(const std::runtime_error&){cancelled_rejected=true;}
    if(!cancelled_rejected)return 5;

    manager.load("missing");bool invalid_artifact=false;
    try{(void)manager.generate("missing",req,out,control);}catch(const std::runtime_error&){invalid_artifact=true;}
    if(!invalid_artifact)return 6;
    bool saw_fault=false;for(const auto& s:manager.statuses())if(s.name=="missing"&&s.state==xenon::RuntimeBackendState::Faulted&&!s.last_error.empty())saw_fault=true;
    if(!saw_fault)return 7;

    manager.unload("fixture");if(manager.loaded("fixture"))return 8;

    xenon::NativeMidiMessage note{0x90,64,100,0};
    auto sigil=xenon::MidiTrinityAdapter::map_message(xenon::TrinityDeviceKind::SigilGuitar,note);
    if(sigil.device!="Sigil Guitar"||!sigil.gate||sigil.pressure<0.7)return 9;
    xenon::NativeMidiMessage cc{0xB0,74,32,0};
    auto glyph=xenon::MidiTrinityAdapter::map_message(xenon::TrinityDeviceKind::GlyphPad,cc);
    if(glyph.device!="GlyphPad"||glyph.y<=0.0||!glyph.gate)return 10;

    xenon::NativeMidiInput input;bool invalid_input=false;
    try{input.open(std::numeric_limits<unsigned int>::max());}catch(const std::out_of_range&){invalid_input=true;}
    if(!invalid_input||input.is_open())return 11;

#ifdef XENON_VST3_SDK_ENABLED
    if(!xenon::Vst3NativeHost::sdk_enabled())return 12;
#else
    if(xenon::Vst3NativeHost::sdk_enabled())return 13;
#endif

    fs::remove_all(out);return 0;
}
