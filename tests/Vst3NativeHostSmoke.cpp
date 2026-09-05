#include "xenon/vst3_native_host.hpp"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

#ifndef XENON_TEST_VST3_PATH
#error XENON_TEST_VST3_PATH must point to the Steinberg ADelay fixture
#endif

int main(){
#ifdef XENON_VST3_SDK_ENABLED
    namespace fs=std::filesystem;
    fs::path binary=XENON_TEST_VST3_PATH;
    fs::path module=binary;
    std::vector<fs::path> candidates{binary};
    auto p=binary.parent_path();
    for(int i=0;i<4 && !p.empty();++i){candidates.push_back(p);p=p.parent_path();}

    std::vector<xenon::Vst3ClassInfo> classes;
    std::string last_error;
    for(const auto& candidate:candidates){
        try{classes=xenon::Vst3NativeHost::inspect(candidate);module=candidate;if(!classes.empty())break;}
        catch(const std::exception& ex){last_error=ex.what();}
    }
    if(classes.empty()){
        std::cerr<<"L41 VST3 fixture inspection failed: "<<last_error<<'\n';
        return 1;
    }

    xenon::Vst3NativeHost host;
    try{host.open(module,44100.0,256);}
    catch(const std::exception& ex){std::cerr<<"L41 VST3 open failed: "<<ex.what()<<'\n';return 2;}
    if(!host.is_open()||host.plugin_name().empty())return 3;
    if(!host.has_controller())return 4;

    // Steinberg ADelay defaults to a normalized delay of 1.0, which its processor
    // maps to one second at the active sample rate. Process beyond 44,100 frames
    // so the impulse must emerge from the real plug-in DSP instead of falsely
    // treating the expected initial silence as a host failure.
    constexpr std::size_t sample_rate=44100;
    constexpr std::size_t tail_frames=512;
    std::vector<float> audio((sample_rate+tail_frames)*2,0.0f);
    audio[0]=0.5f;audio[1]=0.5f;
    try{host.process(audio);}
    catch(const std::exception& ex){std::cerr<<"L41 VST3 process failed: "<<ex.what()<<'\n';return 5;}
    bool finite=true;double energy=0.0;
    for(float x:audio){finite=finite&&std::isfinite(x);energy+=std::abs(x);}
    if(!finite||energy<=0.0){std::cerr<<"L41 VST3 DSP produced no delayed impulse\n";return 6;}
    host.close();if(host.is_open()||host.has_controller())return 7;
#endif
    return 0;
}
