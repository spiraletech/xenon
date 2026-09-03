#include "xenon/vst_host.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

class TestPlugin final : public xenon::IHostedPlugin {
public:
    explicit TestPlugin(xenon::PluginDescriptor descriptor, bool crash = false)
        : descriptor_(std::move(descriptor)), crash_(crash) {
        for (const auto& parameter : descriptor_.parameters)
            parameters_[parameter.id] = parameter.default_value;
    }

    const xenon::PluginDescriptor& descriptor() const noexcept override { return descriptor_; }
    void reset(std::uint32_t sample_rate) override { sample_rate_ = sample_rate; }

    void set_parameter(std::uint32_t id, double value) override {
        if (!parameters_.contains(id)) throw std::invalid_argument("unknown test parameter");
        parameters_[id] = std::clamp(value, 0.0, 1.0);
    }

    double parameter(std::uint32_t id) const override {
        const auto it = parameters_.find(id);
        if (it == parameters_.end()) throw std::invalid_argument("unknown test parameter");
        return it->second;
    }

    void process(std::span<float> stereo, const xenon::PluginProcessContext& context) override {
        if (crash_) throw std::runtime_error("intentional plugin crash");
        if (sample_rate_ == 0 || context.sample_rate != sample_rate_)
            throw std::runtime_error("plugin reset/process sample rate mismatch");

        const double amount = parameters_.empty() ? 0.5 : parameters_.begin()->second;
        const float gain = static_cast<float>(0.75 + amount * 0.50);
        for (auto& sample : stereo) sample = std::clamp(sample * gain, -1.0f, 1.0f);
    }

private:
    xenon::PluginDescriptor descriptor_;
    std::unordered_map<std::uint32_t, double> parameters_;
    bool crash_{false};
    std::uint32_t sample_rate_{0};
};

class TestFactory final : public xenon::IPluginFactory {
public:
    std::unique_ptr<xenon::IHostedPlugin> create(const xenon::PluginDescriptor& descriptor) override {
        return std::make_unique<TestPlugin>(descriptor, descriptor.plugin_id == "crash");
    }
};

xenon::PluginDescriptor make_plugin(std::string id, xenon::PluginKind kind) {
    xenon::PluginDescriptor descriptor;
    descriptor.plugin_id = std::move(id);
    descriptor.name = descriptor.plugin_id;
    descriptor.vendor = "XENON test";
    descriptor.kind = kind;
    descriptor.parameters = {
        {1, "activity", 0.40, true},
        {2, "density", 0.50, true},
        {3, "variation", 0.25, true}
    };
    return descriptor;
}

} // namespace

int main() {
    try {
        auto factory = std::make_shared<TestFactory>();
        xenon::VstHost host(factory);

        const auto instrument_index = host.add_plugin(make_plugin("instrument", xenon::PluginKind::Instrument));
        const auto crash_index = host.add_plugin(make_plugin("crash", xenon::PluginKind::Effect));
        const auto effect_index = host.add_plugin(make_plugin("effect", xenon::PluginKind::Effect));
        require(instrument_index == 0 && crash_index == 1 && effect_index == 2,
            "L33 plugin order incorrect");

        host.set_parameter(instrument_index, 1, 0.70);
        host.save_preset(instrument_index, "bright");
        require(host.slots()[0].preset.name == "bright", "L33 preset save failed");

        xenon::PluginPreset recalled;
        recalled.name = "recalled";
        recalled.parameters = {{1, 0.20}, {2, 0.90}, {3, 0.80}};
        host.recall_preset(instrument_index, recalled);
        require(host.slots()[0].preset.name == "recalled", "L33 preset recall failed");

        xenon::MusicianIntent musician;
        musician.role = xenon::MusicianRole::Melody;
        musician.global_activity = 0.82;
        musician.global_density = 0.33;
        musician.variation = 0.61;
        auto musician_automation = host.map_musician_intent(musician, host.slots()[0].descriptor);
        require(musician_automation.size() == 3, "L33 musician automation mapping incomplete");
        for (const auto& point : musician_automation) host.automate(instrument_index, point);

        musician.locked = true;
        require(host.map_musician_intent(musician, host.slots()[0].descriptor).empty(),
            "L33 locked musician emitted automation");

        xenon::JamResponse jam;
        jam.should_respond = true;
        jam.state.energy = 0.76;
        jam.state.beat_phase = 0.42;
        jam.state.confidence = 0.91;
        auto jam_automation = host.map_jam_response(jam, host.slots()[2].descriptor);
        require(jam_automation.size() == 3, "L33 jam automation mapping incomplete");
        for (const auto& point : jam_automation) host.automate(effect_index, point);

        std::vector<float> audio(256 * 2, 0.20f);
        host.process(audio, 48000, 102.0, 0.25, "D minor");

        require(host.faults().size() == 1, "L33 fault isolation did not record crashing plugin");
        require(host.faults().front().plugin_id == "crash", "L33 fault attributed to wrong plugin");
        require(host.slots()[1].faulted, "L33 crashing plugin was not quarantined");
        require(!host.slots()[0].faulted && !host.slots()[2].faulted,
            "L33 healthy plugins incorrectly faulted");

        bool changed = false;
        for (float sample : audio) {
            require(std::isfinite(sample), "L33 plugin chain produced non-finite output");
            require(sample >= -1.0f && sample <= 1.0f, "L33 plugin chain exceeded normalized output range");
            if (std::abs(sample - 0.20f) > 1e-5f) changed = true;
        }
        require(changed, "L33 plugin chain did not process audio");

        // A quarantined plugin remains skipped on subsequent buffers while healthy plugins continue.
        const auto faults_before = host.faults().size();
        std::vector<float> second(64 * 2, 0.10f);
        host.process(second, 44100, 95.0, 0.80, "E minor");
        require(host.faults().size() == faults_before, "L33 faulted plugin was executed again");

        xenon::Vst3ModuleValidator validator;
        require(validator.is_candidate("Example.vst3"), "L33 .vst3 candidate recognition failed");
        require(!validator.is_candidate("Example.dll"), "L33 non-VST3 module accepted as candidate");
        require(!validator.exposes_factory(std::filesystem::path("missing.vst3")),
            "L33 missing module incorrectly exposed factory");

        bool rejected_bad_parameter = false;
        try { host.set_parameter(instrument_index, 999, 0.5); }
        catch (const std::invalid_argument&) { rejected_bad_parameter = true; }
        require(rejected_bad_parameter, "L33 accepted unknown parameter id");

        bool rejected_odd_buffer = false;
        try {
            std::vector<float> odd(3, 0.0f);
            host.process(odd, 44100, 120.0, 0.0);
        } catch (const std::invalid_argument&) { rejected_odd_buffer = true; }
        require(rejected_odd_buffer, "L33 accepted malformed stereo buffer");

        host.set_enabled(effect_index, false);
        require(!host.slots()[2].enabled, "L33 plugin enable state failed");
        host.remove_plugin(crash_index);
        require(host.slots().size() == 2, "L33 plugin removal failed");

        std::cout << "L33 VST host smoke passed\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "L33 smoke failed: " << ex.what() << '\n';
        return 1;
    }
}
