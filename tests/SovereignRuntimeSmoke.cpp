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
    auto http=std::make_shared<FakeHttp>();
    xenon::SovereignRuntime rt(http,"http://127.0.0.1:8000","C:/models/ace-step","fake-key");
    auto d=rt.diagnose();
    if(!d.ace_step.configured||!d.ace_step.reachable)return 1;
    if(!d.stable_audio.configured)return 2;
    auto stable=rt.make_stable_audio();
    xenon::GenerationRequest r;r.prompt="L41 runtime smoke";r.duration_seconds=1;r.seed=41;
    auto out=std::filesystem::temp_directory_path()/"xenon_l41_runtime";std::filesystem::remove_all(out);
    auto a=stable->generate(r,out);if(!std::filesystem::exists(a.audio_path)||a.backend_name!="Stable Audio")return 3;
    auto vst=xenon::vst3_sdk_status();
#ifndef XENON_VST3_SDK_ENABLED
    if(vst.runtime_ready||vst.sdk_enabled)return 4;
#endif
    xenon::NativeMidiRuntime midi;(void)midi.enumerate();
    std::filesystem::remove_all(out);return 0;
}
