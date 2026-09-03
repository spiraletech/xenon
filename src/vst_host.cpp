#include "xenon/vst_host.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>
#include <utility>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace xenon {
namespace {

bool has_parameter(const PluginDescriptor& descriptor, std::uint32_t id) {
    return std::any_of(descriptor.parameters.begin(), descriptor.parameters.end(),
        [id](const PluginParameterDescriptor& p) { return p.id == id; });
}

const PluginParameterDescriptor* parameter_at(const PluginDescriptor& descriptor, std::size_t index) {
    return index < descriptor.parameters.size() ? &descriptor.parameters[index] : nullptr;
}

} // namespace

bool Vst3ModuleValidator::is_candidate(const std::filesystem::path& module_path) const {
    if (module_path.empty()) return false;
    auto ext = module_path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return ext == ".vst3";
}

bool Vst3ModuleValidator::exposes_factory(const std::filesystem::path& module_path) const {
    if (!is_candidate(module_path) || !std::filesystem::exists(module_path)) return false;
#ifdef _WIN32
    HMODULE module = LoadLibraryW(module_path.wstring().c_str());
    if (!module) return false;
    const auto symbol = GetProcAddress(module, "GetPluginFactory");
    FreeLibrary(module);
    return symbol != nullptr;
#else
    return false;
#endif
}

VstHost::VstHost(std::shared_ptr<IPluginFactory> factory)
    : factory_(std::move(factory)) {
    if (!factory_) throw std::invalid_argument("L33 plugin factory is required");
}

double VstHost::clamp01(double value) { return std::clamp(value, 0.0, 1.0); }

void VstHost::validate_descriptor(const PluginDescriptor& descriptor) {
    if (descriptor.plugin_id.empty()) throw std::invalid_argument("L33 plugin id is required");
    if (descriptor.name.empty()) throw std::invalid_argument("L33 plugin name is required");
    for (const auto& parameter : descriptor.parameters) {
        if (parameter.name.empty()) throw std::invalid_argument("L33 parameter name is required");
        if (parameter.default_value < 0.0 || parameter.default_value > 1.0)
            throw std::invalid_argument("L33 parameter default must be normalized");
    }
}

std::size_t VstHost::add_plugin(PluginDescriptor descriptor) {
    validate_descriptor(descriptor);
    auto instance = factory_->create(descriptor);
    if (!instance) throw std::runtime_error("L33 plugin factory returned null instance");

    PluginPreset initial;
    initial.name = "Initial";
    for (const auto& parameter : descriptor.parameters) {
        instance->set_parameter(parameter.id, parameter.default_value);
        initial.parameters[parameter.id] = parameter.default_value;
    }

    Slot slot_value;
    slot_value.state.descriptor = std::move(descriptor);
    slot_value.state.preset = std::move(initial);
    slot_value.instance = std::move(instance);
    plugins_.push_back(std::move(slot_value));
    return plugins_.size() - 1;
}

void VstHost::remove_plugin(std::size_t index) {
    if (index >= plugins_.size()) throw std::out_of_range("L33 plugin index out of range");
    plugins_.erase(plugins_.begin() + static_cast<std::ptrdiff_t>(index));
}

void VstHost::clear() {
    plugins_.clear();
    faults_.clear();
    slot_cache_.clear();
}

VstHost::Slot& VstHost::slot(std::size_t index) {
    if (index >= plugins_.size()) throw std::out_of_range("L33 plugin index out of range");
    return plugins_[index];
}

const VstHost::Slot& VstHost::slot(std::size_t index) const {
    if (index >= plugins_.size()) throw std::out_of_range("L33 plugin index out of range");
    return plugins_[index];
}

void VstHost::set_enabled(std::size_t index, bool enabled) { slot(index).state.enabled = enabled; }

void VstHost::set_parameter(std::size_t index, std::uint32_t parameter_id, double value) {
    auto& s = slot(index);
    if (!has_parameter(s.state.descriptor, parameter_id)) throw std::invalid_argument("L33 unknown parameter id");
    const double normalized = clamp01(value);
    s.instance->set_parameter(parameter_id, normalized);
    s.state.preset.parameters[parameter_id] = normalized;
}

void VstHost::automate(std::size_t index, ParameterAutomationPoint point) {
    auto& s = slot(index);
    if (!has_parameter(s.state.descriptor, point.parameter_id)) throw std::invalid_argument("L33 unknown automation parameter");
    point.value = clamp01(point.value);
    s.pending_automation.push_back(point);
}

void VstHost::save_preset(std::size_t index, std::string name) {
    if (name.empty()) throw std::invalid_argument("L33 preset name is required");
    auto& s = slot(index);
    PluginPreset preset;
    preset.name = std::move(name);
    for (const auto& parameter : s.state.descriptor.parameters)
        preset.parameters[parameter.id] = s.instance->parameter(parameter.id);
    s.state.preset = std::move(preset);
}

void VstHost::recall_preset(std::size_t index, const PluginPreset& preset) {
    if (preset.name.empty()) throw std::invalid_argument("L33 preset name is required");
    auto& s = slot(index);
    for (const auto& [id, value] : preset.parameters) {
        if (!has_parameter(s.state.descriptor, id)) continue;
        s.instance->set_parameter(id, clamp01(value));
    }
    s.state.preset = preset;
    for (auto& [id, value] : s.state.preset.parameters) value = clamp01(value);
}

std::vector<ParameterAutomationPoint> VstHost::map_musician_intent(
    const MusicianIntent& musician,
    const PluginDescriptor& plugin) const {
    std::vector<ParameterAutomationPoint> out;
    if (musician.locked || plugin.parameters.empty()) return out;
    if (const auto* p = parameter_at(plugin, 0); p && p->automatable)
        out.push_back({p->id, clamp01(musician.global_activity), 0});
    if (const auto* p = parameter_at(plugin, 1); p && p->automatable)
        out.push_back({p->id, clamp01(musician.global_density), 0});
    if (const auto* p = parameter_at(plugin, 2); p && p->automatable)
        out.push_back({p->id, clamp01(musician.variation), 0});
    return out;
}

std::vector<ParameterAutomationPoint> VstHost::map_jam_response(
    const JamResponse& response,
    const PluginDescriptor& plugin) const {
    std::vector<ParameterAutomationPoint> out;
    if (!response.should_respond || plugin.parameters.empty()) return out;
    if (const auto* p = parameter_at(plugin, 0); p && p->automatable)
        out.push_back({p->id, clamp01(response.state.energy), 0});
    if (const auto* p = parameter_at(plugin, 1); p && p->automatable)
        out.push_back({p->id, clamp01(response.state.beat_phase), 0});
    if (const auto* p = parameter_at(plugin, 2); p && p->automatable)
        out.push_back({p->id, clamp01(response.state.confidence), 0});
    return out;
}

void VstHost::process(std::span<float> interleaved_stereo,
                      std::uint32_t sample_rate,
                      double tempo_bpm,
                      double beat_phase,
                      std::string key) {
    if (interleaved_stereo.size() % 2 != 0) throw std::invalid_argument("L33 stereo buffer must be interleaved pairs");
    if (sample_rate < 8000 || sample_rate > 384000) throw std::invalid_argument("L33 sample rate out of range");
    if (tempo_bpm <= 0.0 || tempo_bpm > 400.0) throw std::invalid_argument("L33 tempo out of range");
    if (beat_phase < 0.0 || beat_phase > 1.0) throw std::invalid_argument("L33 beat phase out of range");

    PluginProcessContext context;
    context.sample_rate = sample_rate;
    context.frames = static_cast<std::uint32_t>(interleaved_stereo.size() / 2);
    context.tempo_bpm = tempo_bpm;
    context.beat_phase = beat_phase;
    context.key = std::move(key);

    for (auto& s : plugins_) {
        if (!s.state.enabled || s.state.faulted) continue;
        context.automation = std::move(s.pending_automation);
        s.pending_automation.clear();
        try {
            s.instance->reset(sample_rate);
            for (const auto& point : context.automation) {
                if (point.sample_offset <= context.frames)
                    s.instance->set_parameter(point.parameter_id, clamp01(point.value));
            }
            s.instance->process(interleaved_stereo, context);
        } catch (const std::exception& ex) {
            s.state.faulted = true;
            faults_.push_back({s.state.descriptor.plugin_id, ex.what()});
        } catch (...) {
            s.state.faulted = true;
            faults_.push_back({s.state.descriptor.plugin_id, "unknown plugin fault"});
        }
    }
}

const std::vector<PluginSlotState>& VstHost::slots() const noexcept {
    slot_cache_.clear();
    slot_cache_.reserve(plugins_.size());
    for (const auto& plugin : plugins_) slot_cache_.push_back(plugin.state);
    return slot_cache_;
}

const std::vector<PluginFault>& VstHost::faults() const noexcept { return faults_; }

} // namespace xenon
