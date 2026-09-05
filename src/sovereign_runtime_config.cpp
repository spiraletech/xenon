#include "xenon/sovereign_runtime.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <stdexcept>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#endif

namespace xenon {

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
    std::vector<std::string> out;
    out.reserve(factories_.size());
    for (const auto& [name, _] : factories_) out.push_back(name);
    std::sort(out.begin(), out.end());
    return out;
}

void RuntimeConfig::save(const std::filesystem::path& path) const {
    const auto parent = path.parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent);
    const auto temp = path.string() + ".tmp";
    std::ofstream o(temp, std::ios::trunc);
    if (!o) throw std::runtime_error("L41 cannot open runtime config for save");
    o << "XENON_RUNTIME|1\n";
    o << std::quoted(ace_url) << '\n' << std::quoted(ace_checkpoint) << '\n' << std::quoted(stability_model) << '\n';
    o << (midi_input ? static_cast<long long>(*midi_input) : -1LL) << ' '
      << (midi_output ? static_cast<long long>(*midi_output) : -1LL) << '\n';
    o << plugin_paths.size() << '\n';
    for (const auto& p : plugin_paths) o << std::quoted(p.string()) << '\n';
    o.close();
    if (!o) throw std::runtime_error("L41 runtime config write failed");
    std::error_code ec;
    std::filesystem::remove(path, ec);
    ec.clear();
    std::filesystem::rename(temp, path, ec);
    if (ec) throw std::runtime_error("L41 runtime config atomic rename failed: " + ec.message());
}

RuntimeConfig RuntimeConfig::load(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("L41 cannot open runtime config");
    std::string header;
    std::getline(in, header);
    if (header != "XENON_RUNTIME|1") throw std::invalid_argument("L41 unsupported runtime config schema");
    RuntimeConfig c;
    if (!(in >> std::quoted(c.ace_url) >> std::quoted(c.ace_checkpoint) >> std::quoted(c.stability_model)))
        throw std::invalid_argument("L41 corrupt runtime config strings");
    long long midi_in=-1, midi_out=-1;
    if (!(in >> midi_in >> midi_out)) throw std::invalid_argument("L41 corrupt runtime MIDI config");
    if (midi_in >= 0) c.midi_input = static_cast<unsigned int>(midi_in);
    if (midi_out >= 0) c.midi_output = static_cast<unsigned int>(midi_out);
    std::size_t count=0;
    if (!(in >> count) || count > 1024) throw std::invalid_argument("L41 corrupt runtime plugin path count");
    for (std::size_t i=0;i<count;++i) {
        std::string value;
        if (!(in >> std::quoted(value))) throw std::invalid_argument("L41 corrupt runtime plugin path");
        c.plugin_paths.emplace_back(value);
    }
    return c;
}

void NativeMidiRuntime::send_short(unsigned int output_index, unsigned char status, unsigned char data1, unsigned char data2) const {
#ifdef _WIN32
    HMIDIOUT handle=nullptr;
    const auto open=midiOutOpen(&handle, output_index, 0, 0, CALLBACK_NULL);
    if (open != MMSYSERR_NOERROR) throw std::runtime_error("L41 failed to open MIDI output device");
    const DWORD message = static_cast<DWORD>(status) |
        (static_cast<DWORD>(data1 & 0x7Fu) << 8u) |
        (static_cast<DWORD>(data2 & 0x7Fu) << 16u);
    const auto sent=midiOutShortMsg(handle, message);
    midiOutClose(handle);
    if (sent != MMSYSERR_NOERROR) throw std::runtime_error("L41 failed to send MIDI short message");
#else
    (void)output_index; (void)status; (void)data1; (void)data2;
    throw std::runtime_error("L41 native MIDI output currently requires Windows WinMM");
#endif
}

BackendRegistry SovereignRuntime::registry() const {
    BackendRegistry r;
    const auto http=http_;
    const auto ace_url=ace_url_;
    const auto ace_checkpoint=ace_checkpoint_;
    const auto stability_key=stability_key_;
    r.register_factory("ACE-Step", [http,ace_url,ace_checkpoint]{
        return std::make_unique<AceStepBackend>(http, ace_url, ace_checkpoint);
    });
    r.register_factory("Stable Audio", [http,stability_key]{
        return std::make_unique<StableAudioBackend>(http, stability_key);
    });
    return r;
}

} // namespace xenon
