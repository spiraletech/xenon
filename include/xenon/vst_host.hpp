#pragma once

#include "xenon/musician_agents.hpp"
#include "xenon/realtime_jam_engine.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace xenon {

enum class PluginKind {
    Instrument,
    Effect
};

struct PluginParameterDescriptor {
    std::uint32_t id{0};
    std::string name;
    double default_value{0.0};
    bool automatable{true};
};

struct PluginDescriptor {
    std::string plugin_id;
    std::string name;
    std::string vendor;
    PluginKind kind{PluginKind::Effect};
    std::filesystem::path module_path;
    std::vector<PluginParameterDescriptor> parameters;
};

struct ParameterAutomationPoint {
    std::uint32_t parameter_id{0};
    double value{0.0};
    std::uint32_t sample_offset{0};
};

struct PluginPreset {
    std::string name;
    std::unordered_map<std::uint32_t, double> parameters;
};

struct PluginProcessContext {
    std::uint32_t sample_rate{44100};
    std::uint32_t frames{0};
    double tempo_bpm{120.0};
    double beat_phase{0.0};
    std::string key;
    std::vector<ParameterAutomationPoint> automation;
};

class IHostedPlugin {
public:
    virtual ~IHostedPlugin() = default;
    [[nodiscard]] virtual const PluginDescriptor& descriptor() const noexcept = 0;
    virtual void reset(std::uint32_t sample_rate) = 0;
    virtual void set_parameter(std::uint32_t id, double value) = 0;
    [[nodiscard]] virtual double parameter(std::uint32_t id) const = 0;
    virtual void process(std::span<float> interleaved_stereo, const PluginProcessContext& context) = 0;
};

class IPluginFactory {
public:
    virtual ~IPluginFactory() = default;
    [[nodiscard]] virtual std::unique_ptr<IHostedPlugin> create(const PluginDescriptor& descriptor) = 0;
};

struct PluginFault {
    std::string plugin_id;
    std::string message;
};

struct PluginSlotState {
    PluginDescriptor descriptor;
    bool enabled{true};
    bool faulted{false};
    PluginPreset preset;
};

class Vst3ModuleValidator {
public:
    [[nodiscard]] bool is_candidate(const std::filesystem::path& module_path) const;
    [[nodiscard]] bool exposes_factory(const std::filesystem::path& module_path) const;
};

class VstHost {
public:
    explicit VstHost(std::shared_ptr<IPluginFactory> factory);

    std::size_t add_plugin(PluginDescriptor descriptor);
    void remove_plugin(std::size_t index);
    void clear();

    void set_enabled(std::size_t index, bool enabled);
    void set_parameter(std::size_t index, std::uint32_t parameter_id, double value);
    void automate(std::size_t index, ParameterAutomationPoint point);
    void save_preset(std::size_t index, std::string name);
    void recall_preset(std::size_t index, const PluginPreset& preset);

    [[nodiscard]] std::vector<ParameterAutomationPoint> map_musician_intent(
        const MusicianIntent& musician,
        const PluginDescriptor& plugin) const;
    [[nodiscard]] std::vector<ParameterAutomationPoint> map_jam_response(
        const JamResponse& response,
        const PluginDescriptor& plugin) const;

    void process(std::span<float> interleaved_stereo,
                 std::uint32_t sample_rate,
                 double tempo_bpm,
                 double beat_phase,
                 std::string key = {});

    [[nodiscard]] const std::vector<PluginSlotState>& slots() const noexcept;
    [[nodiscard]] const std::vector<PluginFault>& faults() const noexcept;

private:
    struct Slot {
        PluginSlotState state;
        std::unique_ptr<IHostedPlugin> instance;
        std::vector<ParameterAutomationPoint> pending_automation;
    };

    Slot& slot(std::size_t index);
    const Slot& slot(std::size_t index) const;
    static void validate_descriptor(const PluginDescriptor& descriptor);
    static double clamp01(double value);

    std::shared_ptr<IPluginFactory> factory_;
    std::vector<Slot> plugins_;
    mutable std::vector<PluginSlotState> slot_cache_;
    std::vector<PluginFault> faults_;
};

} // namespace xenon
