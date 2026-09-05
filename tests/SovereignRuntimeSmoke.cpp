#include "xenon/model_router.hpp"
#include "xenon/sovereign_runtime.hpp"
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace {
std::vector<std::byte> as_bytes(const std::string& s){std::vector<std::byte>b(s.size());for(std::size_t i=0;i<s.size();++i)b[i]=static_cast<std::byte>(s[i]);return b;}
class FakeHttp final : public xenon::IHttpTransport {
public:
    xenon::HttpResponse send(const xenon::HttpRequest& request) override {
        if (request.url.find("/health") != std::string::npos) return {200, {}, "application/json"};
        if (request.url.find("/v1/user/account") != std::string::npos) return {200,as_bytes("{\"id\":\"fixture\"}"),"application/json"};
        if (request.url.find("/generate") != std::string::npos) {
            auto p=std::filesystem::temp_directory_path()/"xenon_l41_fake_ace.wav";
            std::ofstream f(p,std::ios::binary);f.write("RIFF0000WAVE",12);f.close();
            return {200,as_bytes("{\"output_path\":\""+p.generic_string()+"\"}"),"application/json"};
        }
        if (request.url.find("text-to-audio") != std::string::npos) return {200,as_bytes("RIFF0000WAVE"),"audio/wav"};
        return {500,{},"text/plain"};
    }
};
}

int main(){
    namespace fs=std::filesystem;
    auto http=std::make_shared<FakeHttp>();
    xenon::SovereignRuntime rt(http,"http://127.0.0.1:8000","C:/models/ace-step","fake-key");
    auto d=rt.diagnose();if(!d.ace_step.configured||!d.ace_step.reachable)return 1;if(!d.stable_audio.configured||!d.stable_audio.reachable)return 2;
    const auto manifest=rt.diagnostics_manifest();if(manifest.find("XENON_SOVEREIGN_RUNTIME|1")==std::string::npos||manifest.find("ace_step=1,1")==std::string::npos||manifest.find("stable_audio=1,1")==std::string::npos)return 3;
    auto registry=rt.registry();auto names=registry.names();if(names.size()!=2||names[0]!="ACE-Step"||names[1]!="Stable Audio")return 4;
    xenon::GenerationRequest r;r.prompt="L41 runtime smoke";r.duration_seconds=1;r.seed=41;
    auto out=fs::temp_directory_path()/"xenon_l41_runtime";fs::remove_all(out);
    auto stable=registry.create("Stable Audio");auto stable_artifact=stable->generate(r,out/"stable");if(!fs::exists(stable_artifact.audio_path)||stable_artifact.backend_name!="Stable Audio")return 5;
    auto ace=registry.create("ACE-Step");auto ace_artifact=ace->generate(r,out/"ace");if(!fs::exists(ace_artifact.audio_path)||ace_artifact.backend_name!="ACE-Step")return 6;

    xenon::RuntimeConfig config;
    config.ace_url="http://localhost:8000";
    config.ace_checkpoint="C:/models/ace";
    config.stability_model="stable-audio-2.5";
    config.preferred_backend="Stable Audio";
    config.enabled_backends={"ACE-Step","Stable Audio"};
    config.midi_input=1;config.midi_output=2;
    config.plugin_paths={"C:/VST3","D:/Audio/Plugins"};
    config.midi_cc_mappings={{"filter_cutoff",74},{"expression",11}};
    config.devices.sigil_guitar_enabled=true;
    config.devices.glyph_pad_enabled=false;
    config.devices.thread_deck_enabled=true;
    auto config_path=out/"runtime.cfg";config.save(config_path);auto restored=xenon::RuntimeConfig::load(config_path);
    if(restored.ace_url!=config.ace_url||restored.ace_checkpoint!=config.ace_checkpoint||restored.preferred_backend!="Stable Audio")return 7;
    if(!restored.midi_input||*restored.midi_input!=1||!restored.midi_output||*restored.midi_output!=2||restored.plugin_paths.size()!=2)return 8;
    if(restored.midi_cc_mappings.size()!=2||restored.midi_cc_mappings.at("filter_cutoff")!=74||restored.devices.glyph_pad_enabled)return 9;
    xenon::ModelRouter boot_router;const auto registered=rt.configure_router(boot_router,restored);if(registered!=2||boot_router.provider_count()!=2)return 10;
    if(boot_router.route(r).provider_name!="Stable Audio")return 11;

    bool invalid_mapping=false;
    try{xenon::RuntimeConfig bad=config;bad.midi_cc_mappings["bad"]=128;bad.save(out/"bad.cfg");}catch(const std::invalid_argument&){invalid_mapping=true;}
    if(!invalid_mapping)return 12;

    auto vst=xenon::vst3_sdk_status();
#ifdef XENON_VST3_SDK_ENABLED
    if(!vst.runtime_ready||!vst.sdk_enabled)return 13;
#else
    if(vst.runtime_ready||vst.sdk_enabled)return 14;
#endif
    xenon::NativeMidiRuntime midi;(void)midi.enumerate();
    fs::remove(fs::temp_directory_path()/"xenon_l41_fake_ace.wav");fs::remove_all(out);return 0;
}
