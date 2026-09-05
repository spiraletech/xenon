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

    std::vector<float> audio(512*2,0.0f);
    audio[0]=0.5f;audio[1]=0.5f;
    try{host.process(audio);}
    catch(const std::exception& ex){std::cerr<<"L41 VST3 process failed: "<<ex.what()<<'\n';return 4;}
    bool finite=true;double energy=0.0;
    for(float x:audio){finite=finite&&std::isfinite(x);energy+=std::abs(x);}
    if(!finite||energy<=0.0)return 5;
    host.close();if(host.is_open())return 6;
#endif
    return 0;
}
