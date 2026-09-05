#include "xenon/sovereign_runtime.hpp"
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
class FakeHttp final : public xenon::IHttpTransport {
public:
    xenon::HttpResponse send(const xenon::HttpRequest& request) override {
        if (request.url.find("/health") != std::string::npos) return {200, {}, "application/json"};
        if (request.url.find("text-to-audio") != std::string::npos) {
            const char wav[]={'R','I','F','F','0','0','0','0','W','A','V','E'};
            std::vector<std::byte> b(sizeof(wav)); for(std::size_t i=0;i<sizeof(wav);++i)b[i]=static_cast<std::byte>(wav[i]);
            return {200,std::move(b),"audio/wav"};
        }
        return {500,{},"text/plain"};
    }
};
}

int main(){
    namespace fs=std::filesystem;
    auto http=std::make_shared<FakeHttp>();
    xenon::SovereignRuntime rt(http,"http://127.0.0.1:8000","C:/models/ace-step","fake-key");
    auto d=rt.diagnose();
    if(!d.ace_step.configured||!d.ace_step.reachable)return 1;
    if(!d.stable_audio.configured)return 2;
    auto registry=rt.registry();
    auto names=registry.names();
    if(names.size()!=2||names[0]!="ACE-Step"||names[1]!="Stable Audio")return 3;
    auto stable=registry.create("Stable Audio");
    xenon::GenerationRequest r;r.prompt="L41 runtime smoke";r.duration_seconds=1;r.seed=41;
    auto out=fs::temp_directory_path()/"xenon_l41_runtime";fs::remove_all(out);
    auto a=stable->generate(r,out);if(!fs::exists(a.audio_path)||a.backend_name!="Stable Audio")return 4;

    xenon::RuntimeConfig config;config.ace_url="http://localhost:8000";config.ace_checkpoint="C:/models/ace";config.stability_model="stable-audio-2.5";config.midi_output=2;config.plugin_paths={"C:/VST3","D:/Audio/Plugins"};
    auto config_path=out/"runtime.cfg";config.save(config_path);auto restored=xenon::RuntimeConfig::load(config_path);
    if(restored.ace_url!=config.ace_url||restored.ace_checkpoint!=config.ace_checkpoint||!restored.midi_output||*restored.midi_output!=2||restored.plugin_paths.size()!=2)return 5;

    auto vst=xenon::vst3_sdk_status();
#ifndef XENON_VST3_SDK_ENABLED
    if(vst.runtime_ready||vst.sdk_enabled)return 6;
#endif
    xenon::NativeMidiRuntime midi;(void)midi.enumerate();
    fs::remove_all(out);return 0;
}
