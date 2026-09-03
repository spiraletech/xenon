#include "xenon/midi_intelligence.hpp"

#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <unordered_map>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

class TestPlugin final : public xenon::IHostedPlugin {
public:
    explicit TestPlugin(xenon::PluginDescriptor descriptor) : descriptor_(std::move(descriptor)) {
        for (const auto& p : descriptor_.parameters) values_[p.id] = p.default_value;
    }
    const xenon::PluginDescriptor& descriptor() const noexcept override { return descriptor_; }
    void reset(std::uint32_t) override {}
    void set_parameter(std::uint32_t id, double value) override { values_[id] = value; }
    double parameter(std::uint32_t id) const override {
        const auto it = values_.find(id);
        return it == values_.end() ? 0.0 : it->second;
    }
    void process(std::span<float>, const xenon::PluginProcessContext&) override {}
private:
    xenon::PluginDescriptor descriptor_;
    std::unordered_map<std::uint32_t, double> values_;
};

class TestFactory final : public xenon::IPluginFactory {
public:
    std::unique_ptr<xenon::IHostedPlugin> create(const xenon::PluginDescriptor& descriptor) override {
        return std::make_unique<TestPlugin>(descriptor);
    }
};

} // namespace

int main() {
    try {
        xenon::MidiIntelligence midi;

        const auto on = midi.translate_note(2, 64, 100, 1.25, true);
        require(on.channel == 2 && on.data1 == 64 && on.data2 == 100, "L34 note translation failed");

        xenon::MidiPhrase phrase;
        phrase.bpm = 96.0;
        phrase.events = {
            midi.translate_note(1, 60, 90, 0.13, true),
            midi.translate_note(1, 60, 0, 0.47, false),
            midi.translate_note(3, 67, 110, 1.11, true)
        };
        const auto split = midi.split_by_channel(phrase);
        require(split.size() == 2 && split.at(1).size() == 2 && split.at(3).size() == 1, "L34 splitter failed");

        const auto quantized = midi.quantize(phrase, 0.25, 1.0);
        require(std::abs(quantized.events[0].beat - 0.25) < 1e-9, "L34 quantizer failed first event");
        require(std::abs(quantized.events[1].beat - 0.50) < 1e-9, "L34 quantizer failed second event");

        const auto c_major = midi.infer_chord({60, 64, 67});
        require(c_major.root == "C" && c_major.quality == "major" && c_major.confidence == 1.0, "L34 chord inference failed");

        xenon::ComposerAgentPlan composer;
        composer.bpm = 90.0;
        composer.key = "C minor";
        composer.sections = {{"verse", 4, 0.6, 0.5, 0.6, 0.5, "motif_a"}};
        composer.motif_recurrence = {"motif_a"};

        xenon::MusicianIntent melody;
        melody.role = xenon::MusicianRole::Melody;
        melody.global_activity = 0.7;
        melody.sections = {{"verse", 0.8, 0.5, 0.5, 0.5}};
        const auto generated = midi.generate_phrase(composer, melody);
        require(!generated.events.empty(), "L34 generated MIDI phrase is empty");

        melody.locked = true;
        const auto locked_phrase = midi.generate_phrase(composer, melody);
        require(locked_phrase.events.empty(), "L34 locked musician generated MIDI");

        const auto sigil = midi.map_device_gesture({"Sigil Guitar", 0.5, 0.75, 0.8, true}, 4);
        require(sigil.size() == 2 && sigil[0].type == xenon::MidiEventType::NoteOn && sigil[1].type == xenon::MidiEventType::PitchBend,
                "L34 Sigil Guitar mapping failed");
        const auto glyph = midi.map_device_gesture({"GlyphPad", 0.25, 0.6, 0.4, true}, 5);
        require(glyph.size() == 2 && glyph[1].type == xenon::MidiEventType::ControlChange,
                "L34 GlyphPad mapping failed");

        auto factory = std::make_shared<TestFactory>();
        xenon::VstHost host(factory);
        xenon::PluginDescriptor descriptor;
        descriptor.plugin_id = "l34.test.instrument";
        descriptor.name = "L34 Test Instrument";
        descriptor.kind = xenon::PluginKind::Instrument;
        descriptor.parameters = {{7, "Expression", 0.0, true}};
        host.add_plugin(descriptor);

        xenon::MidiPhrase routed;
        routed.events.push_back(midi.translate_note(1, 60, 127, 0.0, true));
        midi.route_to_vst(routed, {{1, 0}}, host);
        std::vector<float> buffer(64, 0.0f);
        host.process(buffer, 44100, 120.0, 0.0, "C major");
        require(host.faults().empty(), "L34 VST routing faulted host");

        bool bad_channel = false;
        try { (void)midi.translate_note(16, 60, 100, 0.0, true); }
        catch (const std::invalid_argument&) { bad_channel = true; }
        require(bad_channel, "L34 invalid MIDI channel was accepted");

        bool bad_grid = false;
        try { (void)midi.quantize(phrase, 0.0, 1.0); }
        catch (const std::invalid_argument&) { bad_grid = true; }
        require(bad_grid, "L34 invalid quantize grid was accepted");

        bool bad_device = false;
        try { (void)midi.map_device_gesture({"Unknown", 0.0, 0.0, 0.0, true}); }
        catch (const std::invalid_argument&) { bad_device = true; }
        require(bad_device, "L34 unknown device was accepted");

        std::cout << "L34 MIDI Intelligence smoke passed\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "L34 smoke failed: " << ex.what() << '\n';
        return 1;
    }
}
