#pragma once

#include "xenon/composer_agent.hpp"
#include "xenon/musician_agents.hpp"
#include "xenon/vst_host.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace xenon {

enum class MidiEventType {
    NoteOn,
    NoteOff,
    ControlChange,
    PitchBend
};

struct MidiEvent {
    MidiEventType type{MidiEventType::NoteOn};
    std::uint8_t channel{0};
    std::uint8_t data1{0};
    std::uint8_t data2{0};
    double beat{0.0};
};

struct MidiPhrase {
    double bpm{120.0};
    int ppq{480};
    std::vector<MidiEvent> events;
};

struct MidiRoute {
    std::uint8_t input_channel{0};
    std::size_t plugin_slot{0};
};

struct ChordEstimate {
    std::string root;
    std::string quality;
    double confidence{0.0};
};

struct DeviceGesture {
    std::string device;
    double x{0.0};
    double y{0.0};
    double pressure{0.0};
    bool gate{false};
};

class MidiIntelligence {
public:
    [[nodiscard]] MidiEvent translate_note(std::uint8_t channel, int note, int velocity, double beat, bool on) const;
    [[nodiscard]] std::unordered_map<std::uint8_t, std::vector<MidiEvent>> split_by_channel(const MidiPhrase& phrase) const;
    [[nodiscard]] MidiPhrase quantize(const MidiPhrase& phrase, double grid_beats, double strength = 1.0) const;
    [[nodiscard]] ChordEstimate infer_chord(const std::vector<int>& midi_notes) const;
    [[nodiscard]] MidiPhrase generate_phrase(const ComposerAgentPlan& composer, const MusicianIntent& musician) const;
    [[nodiscard]] std::vector<MidiEvent> map_device_gesture(const DeviceGesture& gesture, std::uint8_t channel = 0) const;
    void route_to_vst(const MidiPhrase& phrase, const std::vector<MidiRoute>& routes, VstHost& host) const;
};

} // namespace xenon
