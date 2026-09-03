#include "xenon/midi_intelligence.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace xenon {
namespace {

std::uint8_t clamp7(int value) {
    return static_cast<std::uint8_t>(std::clamp(value, 0, 127));
}

int pitch_class(int note) {
    int pc = note % 12;
    if (pc < 0) pc += 12;
    return pc;
}

const char* note_name(int pc) {
    static constexpr std::array<const char*, 12> names{
        "C","C#","D","D#","E","F","F#","G","G#","A","A#","B"
    };
    return names[static_cast<std::size_t>(pc % 12)];
}

} // namespace

MidiEvent MidiIntelligence::translate_note(
    std::uint8_t channel,
    int note,
    int velocity,
    double beat,
    bool on) const {
    if (channel > 15) throw std::invalid_argument("L34 MIDI channel out of range");
    if (beat < 0.0) throw std::invalid_argument("L34 negative MIDI beat");
    MidiEvent event;
    event.type = on ? MidiEventType::NoteOn : MidiEventType::NoteOff;
    event.channel = channel;
    event.data1 = clamp7(note);
    event.data2 = on ? clamp7(velocity) : 0;
    event.beat = beat;
    return event;
}

std::unordered_map<std::uint8_t, std::vector<MidiEvent>> MidiIntelligence::split_by_channel(
    const MidiPhrase& phrase) const {
    std::unordered_map<std::uint8_t, std::vector<MidiEvent>> out;
    for (const auto& event : phrase.events) {
        if (event.channel > 15) throw std::invalid_argument("L34 MIDI phrase contains invalid channel");
        out[event.channel].push_back(event);
    }
    return out;
}

MidiPhrase MidiIntelligence::quantize(const MidiPhrase& phrase, double grid_beats, double strength) const {
    if (grid_beats <= 0.0) throw std::invalid_argument("L34 quantize grid must be positive");
    if (strength < 0.0 || strength > 1.0) throw std::invalid_argument("L34 quantize strength out of range");
    MidiPhrase out = phrase;
    for (auto& event : out.events) {
        const double snapped = std::round(event.beat / grid_beats) * grid_beats;
        event.beat += (snapped - event.beat) * strength;
        if (event.beat < 0.0) event.beat = 0.0;
    }
    std::stable_sort(out.events.begin(), out.events.end(), [](const MidiEvent& a, const MidiEvent& b) {
        return a.beat < b.beat;
    });
    return out;
}

ChordEstimate MidiIntelligence::infer_chord(const std::vector<int>& midi_notes) const {
    if (midi_notes.size() < 2) return {};
    std::array<bool, 12> present{};
    for (const int note : midi_notes) present[static_cast<std::size_t>(pitch_class(note))] = true;

    struct Candidate { int root; const char* quality; int third; int fifth; };
    for (int root = 0; root < 12; ++root) {
        const Candidate candidates[] = {
            {root, "major", (root + 4) % 12, (root + 7) % 12},
            {root, "minor", (root + 3) % 12, (root + 7) % 12}
        };
        for (const auto& c : candidates) {
            const int hits = static_cast<int>(present[c.root]) + static_cast<int>(present[c.third]) + static_cast<int>(present[c.fifth]);
            if (hits == 3) return {note_name(c.root), c.quality, 1.0};
        }
    }

    const int root = pitch_class(*std::min_element(midi_notes.begin(), midi_notes.end()));
    return {note_name(root), "unknown", 0.40};
}

MidiPhrase MidiIntelligence::generate_phrase(
    const ComposerAgentPlan& composer,
    const MusicianIntent& musician) const {
    ComposerAgent validator;
    validator.validate(composer);
    MidiPhrase phrase;
    phrase.bpm = composer.bpm;

    if (musician.locked) return phrase;

    int base_note = 60;
    double step = 1.0;
    switch (musician.role) {
        case MusicianRole::Drummer: base_note = 36; step = 0.5; break;
        case MusicianRole::Bass: base_note = 36; step = 1.0; break;
        case MusicianRole::Melody: base_note = 60; step = 0.5; break;
        case MusicianRole::Harmony: base_note = 48; step = 2.0; break;
        case MusicianRole::Texture: base_note = 72; step = 2.0; break;
    }

    double beat_cursor = 0.0;
    for (std::size_t section_index = 0; section_index < composer.sections.size(); ++section_index) {
        const auto& section = composer.sections[section_index];
        const double activity = section_index < musician.sections.size()
            ? musician.sections[section_index].activity
            : musician.global_activity;
        const int total_beats = section.bars * 4;
        const int stride = std::max(1, static_cast<int>(std::round(step / std::max(0.20, activity))));
        for (int beat = 0; beat < total_beats; beat += stride) {
            const int note = base_note + static_cast<int>((section_index + beat) % 7);
            const int velocity = 45 + static_cast<int>(std::clamp(activity, 0.0, 1.0) * 75.0);
            phrase.events.push_back(translate_note(static_cast<std::uint8_t>(musician.role), note, velocity, beat_cursor + beat, true));
            phrase.events.push_back(translate_note(static_cast<std::uint8_t>(musician.role), note, 0, beat_cursor + beat + 0.35, false));
        }
        beat_cursor += total_beats;
    }
    return phrase;
}

std::vector<MidiEvent> MidiIntelligence::map_device_gesture(
    const DeviceGesture& gesture,
    std::uint8_t channel) const {
    if (channel > 15) throw std::invalid_argument("L34 device MIDI channel out of range");
    const double x = std::clamp(gesture.x, 0.0, 1.0);
    const double y = std::clamp(gesture.y, 0.0, 1.0);
    const double pressure = std::clamp(gesture.pressure, 0.0, 1.0);

    std::vector<MidiEvent> out;
    if (gesture.device == "Sigil Guitar") {
        const int note = 40 + static_cast<int>(std::round(x * 36.0));
        out.push_back(translate_note(channel, note, 30 + static_cast<int>(pressure * 97.0), 0.0, gesture.gate));
        out.push_back({MidiEventType::PitchBend, channel, clamp7(static_cast<int>(y * 127.0)), 0, 0.0});
    } else if (gesture.device == "GlyphPad") {
        const int note = 48 + static_cast<int>(std::round(x * 24.0));
        out.push_back(translate_note(channel, note, 35 + static_cast<int>(pressure * 92.0), 0.0, gesture.gate));
        out.push_back({MidiEventType::ControlChange, channel, 74, clamp7(static_cast<int>(y * 127.0)), 0.0});
    } else {
        throw std::invalid_argument("L34 unsupported device gesture source");
    }
    return out;
}

void MidiIntelligence::route_to_vst(
    const MidiPhrase& phrase,
    const std::vector<MidiRoute>& routes,
    VstHost& host) const {
    const auto split = split_by_channel(phrase);
    const auto& slots = host.slots();
    for (const auto& route : routes) {
        if (route.input_channel > 15) throw std::invalid_argument("L34 route channel out of range");
        if (route.plugin_slot >= slots.size()) throw std::out_of_range("L34 route plugin slot out of range");
        const auto it = split.find(route.input_channel);
        if (it == split.end()) continue;
        const auto& descriptor = slots[route.plugin_slot].descriptor;
        if (descriptor.parameters.empty()) continue;
        const auto parameter_id = descriptor.parameters.front().id;
        for (const auto& event : it->second) {
            if (event.type != MidiEventType::NoteOn || event.data2 == 0) continue;
            host.automate(route.plugin_slot, {parameter_id, static_cast<double>(event.data2) / 127.0, 0});
        }
    }
}

} // namespace xenon
