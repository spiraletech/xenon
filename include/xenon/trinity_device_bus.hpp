#pragma once

#include "xenon/midi_intelligence.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace xenon {

enum class TrinityDeviceKind {
    SigilGuitar,
    GlyphPad,
    ThreadDeck
};

enum class TransportCommand {
    None,
    Play,
    Pause,
    Stop,
    Seek,
    Record
};

struct TrinityEmotionState {
    double intensity{0.0};
    double tension{0.0};
    double warmth{0.5};
    double motion{0.0};
};

struct TrinityPerformanceState {
    double pressure{0.0};
    double velocity{0.0};
    double expression{0.0};
    double timing_confidence{0.0};
};

struct TrinityTransportState {
    bool playing{false};
    bool recording{false};
    double beat{0.0};
    double bpm{120.0};
    std::uint64_t revision{0};
};

struct TrinityDeviceState {
    TrinityDeviceKind kind{TrinityDeviceKind::SigilGuitar};
    std::string device_id;
    bool connected{false};
    TrinityEmotionState emotion{};
    TrinityPerformanceState performance{};
    std::uint64_t event_count{0};
};

struct TrinityEvent {
    std::uint64_t sequence{0};
    std::string device_id;
    TrinityDeviceKind kind{TrinityDeviceKind::SigilGuitar};
    double beat{0.0};
    DeviceGesture gesture{};
    TransportCommand transport{TransportCommand::None};
    std::optional<double> seek_beat;
    TrinityEmotionState emotion{};
    TrinityPerformanceState performance{};
    std::vector<MidiEvent> midi;
};

struct TrinitySnapshot {
    std::uint32_t schema_version{1};
    TrinityTransportState transport{};
    std::vector<TrinityDeviceState> devices;
    std::vector<TrinityEvent> memory;
};

class TrinityDeviceBus {
public:
    TrinityDeviceBus();

    void connect(TrinityDeviceKind kind);
    void disconnect(TrinityDeviceKind kind);
    [[nodiscard]] bool connected(TrinityDeviceKind kind) const;
    [[nodiscard]] const TrinityDeviceState& device(TrinityDeviceKind kind) const;

    void set_transport(double bpm, double beat = 0.0);
    void apply_transport(TransportCommand command, std::optional<double> seek_beat = std::nullopt);
    [[nodiscard]] const TrinityTransportState& transport() const noexcept;

    [[nodiscard]] TrinityEvent ingest(TrinityDeviceKind kind, DeviceGesture gesture,
                                      TrinityEmotionState emotion = {});
    [[nodiscard]] TrinityEvent schedule(TrinityDeviceKind kind, double beat, DeviceGesture gesture,
                                        TrinityEmotionState emotion = {});
    [[nodiscard]] std::vector<TrinityEvent> advance_to(double beat);
    [[nodiscard]] const std::vector<TrinityEvent>& memory() const noexcept;

    [[nodiscard]] TrinitySnapshot snapshot() const;
    [[nodiscard]] std::string serialize() const;
    [[nodiscard]] static TrinityDeviceBus deserialize(const std::string& text);

private:
    static std::string id_for(TrinityDeviceKind kind);
    static std::string gesture_name_for(TrinityDeviceKind kind);
    static void validate_emotion(const TrinityEmotionState& emotion);
    static void validate_gesture(const DeviceGesture& gesture);
    TrinityDeviceState& mutable_device(TrinityDeviceKind kind);
    TrinityEvent make_event(TrinityDeviceKind kind, double beat, DeviceGesture gesture,
                            TrinityEmotionState emotion);

    MidiIntelligence midi_;
    TrinityTransportState transport_{};
    std::unordered_map<int, TrinityDeviceState> devices_;
    std::vector<TrinityEvent> scheduled_;
    std::vector<TrinityEvent> memory_;
    std::uint64_t next_sequence_{1};
};

} // namespace xenon
