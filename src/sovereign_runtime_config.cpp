#include "xenon/sovereign_runtime.hpp"
#include "xenon/model_router.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#endif

namespace xenon {
namespace {
bool contains_backend(const RuntimeConfig& config,const std::string& name){
    return std::find(config.enabled_backends.begin(),config.enabled_backends.end(),name)!=config.enabled_backends.end();
}
void validate_config(const RuntimeConfig& c){
    if(c.ace_url.empty()) throw std::invalid_argument("L41 runtime ACE-Step URL cannot be empty");
    if(c.stability_model.empty()) throw std::invalid_argument("L41 runtime Stability model cannot be empty");
    if(c.preferred_backend.empty()) throw std::invalid_argument("L41 preferred backend cannot be empty");
    if(c.enabled_backends.size()>128||c.plugin_paths.size()>1024||c.midi_cc_mappings.size()>1024)
        throw std::invalid_argument("L41 runtime config collection exceeds safety bound");
    for(const auto& name:c.enabled_backends) if(name.empty()) throw std::invalid_argument("L41 enabled backend name cannot be empty");
    if(!contains_backend(c,c.preferred_backend)) throw std::invalid_argument("L41 preferred backend must be enabled");
    for(const auto& [name,cc]:c.midi_cc_mappings){
        if(name.empty()||cc>127) throw std::invalid_argument("L41 MIDI mapping must have a name and CC 0..127");
    }
}
}

void BackendRegistry::register_factory(std::string name, Factory factory) {
    if (name.empty() || !factory) throw std::invalid_argument("L41 backend registry requires name and factory");
    if (!factories_.emplace(std::move(name), std::move(factory)).second)
        throw std::invalid_argument("L41 duplicate backend registry name");
}
std::unique_ptr<IModelBackend> BackendRegistry::create(const std::string& name) const {
    const auto it = factories_.find(name);
    if (it == factories_.end()) throw std::out_of_range("L41 backend factory not registered: " + name);
    auto backend = it->second();
    if (!backend) throw std::runtime_error("L41 backend factory returned null");
    return backend;
}
std::vector<std::string> BackendRegistry::names() const {
    std::vector<std::string> out;out.reserve(factories_.size());
    for (const auto& [name, _] : factories_) out.push_back(name);
    std::sort(out.begin(), out.end());return out;
}

void RuntimeConfig::save(const std::filesystem::path& path) const {
    validate_config(*this);
    const auto parent=path.parent_path();if(!parent.empty())std::filesystem::create_directories(parent);
    const auto temp=path.string()+".tmp";std::ofstream o(temp,std::ios::trunc);
    if(!o)throw std::runtime_error("L41 cannot open runtime config for save");
    o<<"XENON_RUNTIME|2\n";
    o<<std::quoted(ace_url)<<'\n'<<std::quoted(ace_checkpoint)<<'\n'<<std::quoted(stability_model)<<'\n'<<std::quoted(preferred_backend)<<'\n';
    o<<enabled_backends.size()<<'\n';for(const auto& name:enabled_backends)o<<std::quoted(name)<<'\n';
    o<<(midi_input?static_cast<long long>(*midi_input):-1LL)<<' '<<(midi_output?static_cast<long long>(*midi_output):-1LL)<<'\n';
    o<<plugin_paths.size()<<'\n';for(const auto& p:plugin_paths)o<<std::quoted(p.string())<<'\n';
    o<<midi_cc_mappings.size()<<'\n';for(const auto& [name,cc]:midi_cc_mappings)o<<std::quoted(name)<<' '<<cc<<'\n';
    o<<(devices.sigil_guitar_enabled?1:0)<<' '<<(devices.glyph_pad_enabled?1:0)<<' '<<(devices.thread_deck_enabled?1:0)<<'\n';
    o.close();if(!o)throw std::runtime_error("L41 runtime config write failed");
    std::error_code ec;std::filesystem::remove(path,ec);ec.clear();std::filesystem::rename(temp,path,ec);
    if(ec)throw std::runtime_error("L41 runtime config atomic rename failed: "+ec.message());
}

RuntimeConfig RuntimeConfig::load(const std::filesystem::path& path) {
    std::ifstream in(path);if(!in)throw std::runtime_error("L41 cannot open runtime config");
    std::string header;std::getline(in,header);
    RuntimeConfig c;
    if(header=="XENON_RUNTIME|1"){
        if(!(in>>std::quoted(c.ace_url)>>std::quoted(c.ace_checkpoint)>>std::quoted(c.stability_model)))throw std::invalid_argument("L41 corrupt v1 runtime config strings");
        long long midi_in=-1,midi_out=-1;if(!(in>>midi_in>>midi_out))throw std::invalid_argument("L41 corrupt v1 runtime MIDI config");
        if(midi_in>=0)c.midi_input=static_cast<unsigned int>(midi_in);if(midi_out>=0)c.midi_output=static_cast<unsigned int>(midi_out);
        std::size_t count=0;if(!(in>>count)||count>1024)throw std::invalid_argument("L41 corrupt v1 runtime plugin path count");
        for(std::size_t i=0;i<count;++i){std::string value;if(!(in>>std::quoted(value)))throw std::invalid_argument("L41 corrupt v1 runtime plugin path");c.plugin_paths.emplace_back(value);}
        return c;
    }
    if(header!="XENON_RUNTIME|2")throw std::invalid_argument("L41 unsupported runtime config schema");
    if(!(in>>std::quoted(c.ace_url)>>std::quoted(c.ace_checkpoint)>>std::quoted(c.stability_model)>>std::quoted(c.preferred_backend)))throw std::invalid_argument("L41 corrupt runtime config strings");
    std::size_t enabled_count=0;if(!(in>>enabled_count)||enabled_count>128)throw std::invalid_argument("L41 corrupt enabled backend count");
    c.enabled_backends.clear();for(std::size_t i=0;i<enabled_count;++i){std::string name;if(!(in>>std::quoted(name)))throw std::invalid_argument("L41 corrupt enabled backend name");c.enabled_backends.push_back(std::move(name));}
    long long midi_in=-1,midi_out=-1;if(!(in>>midi_in>>midi_out))throw std::invalid_argument("L41 corrupt runtime MIDI config");
    if(midi_in>=0)c.midi_input=static_cast<unsigned int>(midi_in);if(midi_out>=0)c.midi_output=static_cast<unsigned int>(midi_out);
    std::size_t path_count=0;if(!(in>>path_count)||path_count>1024)throw std::invalid_argument("L41 corrupt runtime plugin path count");
    for(std::size_t i=0;i<path_count;++i){std::string value;if(!(in>>std::quoted(value)))throw std::invalid_argument("L41 corrupt runtime plugin path");c.plugin_paths.emplace_back(value);}
    std::size_t map_count=0;if(!(in>>map_count)||map_count>1024)throw std::invalid_argument("L41 corrupt MIDI mapping count");
    for(std::size_t i=0;i<map_count;++i){std::string name;unsigned int cc=0;if(!(in>>std::quoted(name)>>cc))throw std::invalid_argument("L41 corrupt MIDI mapping");c.midi_cc_mappings.emplace(std::move(name),cc);}
    int sigil=0,glyph=0,deck=0;if(!(in>>sigil>>glyph>>deck)||sigil<0||sigil>1||glyph<0||glyph>1||deck<0||deck>1)throw std::invalid_argument("L41 corrupt Trinity device state");
    c.devices.sigil_guitar_enabled=sigil!=0;c.devices.glyph_pad_enabled=glyph!=0;c.devices.thread_deck_enabled=deck!=0;
    validate_config(c);return c;
}

void NativeMidiRuntime::send_short(unsigned int output_index,unsigned char status,unsigned char data1,unsigned char data2) const {
#ifdef _WIN32
    HMIDIOUT handle=nullptr;const auto open=midiOutOpen(&handle,output_index,0,0,CALLBACK_NULL);
    if(open!=MMSYSERR_NOERROR)throw std::runtime_error("L41 failed to open MIDI output device");
    const DWORD message=static_cast<DWORD>(status)|(static_cast<DWORD>(data1&0x7Fu)<<8u)|(static_cast<DWORD>(data2&0x7Fu)<<16u);
    const auto sent=midiOutShortMsg(handle,message);midiOutClose(handle);if(sent!=MMSYSERR_NOERROR)throw std::runtime_error("L41 failed to send MIDI short message");
#else
    (void)output_index;(void)status;(void)data1;(void)data2;throw std::runtime_error("L41 native MIDI output currently requires Windows WinMM");
#endif
}

BackendRegistry SovereignRuntime::registry() const {
    BackendRegistry r;const auto http=http_;const auto ace_url=ace_url_;const auto ace_checkpoint=ace_checkpoint_;const auto stability_key=stability_key_;
    r.register_factory("ACE-Step",[http,ace_url,ace_checkpoint]{return std::make_unique<AceStepBackend>(http,ace_url,ace_checkpoint);});
    r.register_factory("Stable Audio",[http,stability_key]{return std::make_unique<StableAudioBackend>(http,stability_key);});
    return r;
}

std::size_t SovereignRuntime::configure_router(ModelRouter& router,const RuntimeConfig& config,int local_priority,int remote_priority) const {
    validate_config(config);std::size_t count=0;
    const auto priority=[&](const std::string& name,int base){return base+(config.preferred_backend==name?10000:0);};
    if(contains_backend(config,"ACE-Step")&&!config.ace_checkpoint.empty()){router.add_provider(std::make_unique<AceStepBackend>(http_,config.ace_url,config.ace_checkpoint),priority("ACE-Step",local_priority));++count;}
    if(contains_backend(config,"Stable Audio")&&!stability_key_.empty()){router.add_provider(std::make_unique<StableAudioBackend>(http_,stability_key_,config.stability_model),priority("Stable Audio",remote_priority));++count;}
    return count;
}

std::string SovereignRuntime::diagnostics_manifest() {
    const auto d=diagnose();std::ostringstream o;
    o<<"XENON_SOVEREIGN_RUNTIME|1\n";
    o<<"ace_step="<<(d.ace_step.configured?1:0)<<','<<(d.ace_step.reachable?1:0)<<','<<d.ace_step.detail<<'\n';
    o<<"stable_audio="<<(d.stable_audio.configured?1:0)<<','<<(d.stable_audio.reachable?1:0)<<','<<d.stable_audio.detail<<'\n';
    o<<"vst3="<<(d.vst3.sdk_enabled?1:0)<<','<<(d.vst3.runtime_ready?1:0)<<','<<d.vst3.detail<<'\n';
    o<<"midi_endpoints="<<d.midi.size()<<'\n';return o.str();
}

} // namespace xenon
