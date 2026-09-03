#include "xenon/trinity_device_bus.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace xenon {
namespace {
int key(TrinityDeviceKind kind) { return static_cast<int>(kind); }
std::string kind_name(TrinityDeviceKind kind) {
    switch (kind) {
        case TrinityDeviceKind::SigilGuitar: return "sigil";
        case TrinityDeviceKind::GlyphPad: return "glyph";
        case TrinityDeviceKind::ThreadDeck: return "thread";
    }
    return "sigil";
}
TrinityDeviceKind parse_kind(const std::string& value) {
    if (value == "glyph") return TrinityDeviceKind::GlyphPad;
    if (value == "thread") return TrinityDeviceKind::ThreadDeck;
    return TrinityDeviceKind::SigilGuitar;
}
}

TrinityDeviceBus::TrinityDeviceBus() {
    devices_.emplace(key(TrinityDeviceKind::SigilGuitar), TrinityDeviceState{TrinityDeviceKind::SigilGuitar, id_for(TrinityDeviceKind::SigilGuitar)});
    devices_.emplace(key(TrinityDeviceKind::GlyphPad), TrinityDeviceState{TrinityDeviceKind::GlyphPad, id_for(TrinityDeviceKind::GlyphPad)});
    devices_.emplace(key(TrinityDeviceKind::ThreadDeck), TrinityDeviceState{TrinityDeviceKind::ThreadDeck, id_for(TrinityDeviceKind::ThreadDeck)});
}

std::string TrinityDeviceBus::id_for(TrinityDeviceKind kind) {
    switch (kind) {
        case TrinityDeviceKind::SigilGuitar: return "sigil-guitar";
        case TrinityDeviceKind::GlyphPad: return "glyphpad";
        case TrinityDeviceKind::ThreadDeck: return "threaddeck";
    }
    return "unknown";
}

std::string TrinityDeviceBus::gesture_name_for(TrinityDeviceKind kind) {
    switch (kind) {
        case TrinityDeviceKind::SigilGuitar: return "Sigil Guitar";
        case TrinityDeviceKind::GlyphPad: return "GlyphPad";
        case TrinityDeviceKind::ThreadDeck: return "ThreadDeck";
    }
    return "unknown";
}

void TrinityDeviceBus::validate_emotion(const TrinityEmotionState& e) {
    auto valid=[](double v){ return std::isfinite(v) && v>=0.0 && v<=1.0; };
    if (!valid(e.intensity) || !valid(e.tension) || !valid(e.warmth) || !valid(e.motion))
        throw std::invalid_argument("L38 emotion state must be normalized");
}

void TrinityDeviceBus::validate_gesture(const DeviceGesture& g) {
    auto valid=[](double v){ return std::isfinite(v) && v>=0.0 && v<=1.0; };
    if (!valid(g.x) || !valid(g.y) || !valid(g.pressure))
        throw std::invalid_argument("L38 gesture values must be normalized");
}

TrinityDeviceState& TrinityDeviceBus::mutable_device(TrinityDeviceKind kind) {
    return devices_.at(key(kind));
}

void TrinityDeviceBus::connect(TrinityDeviceKind kind) { mutable_device(kind).connected = true; }
void TrinityDeviceBus::disconnect(TrinityDeviceKind kind) { mutable_device(kind).connected = false; }
bool TrinityDeviceBus::connected(TrinityDeviceKind kind) const { return devices_.at(key(kind)).connected; }
const TrinityDeviceState& TrinityDeviceBus::device(TrinityDeviceKind kind) const { return devices_.at(key(kind)); }

void TrinityDeviceBus::set_transport(double bpm, double beat) {
    if (!std::isfinite(bpm) || bpm < 20.0 || bpm > 400.0) throw std::invalid_argument("L38 transport bpm out of range");
    if (!std::isfinite(beat) || beat < 0.0) throw std::invalid_argument("L38 transport beat out of range");
    transport_.bpm = bpm;
    transport_.beat = beat;
    ++transport_.revision;
}

void TrinityDeviceBus::apply_transport(TransportCommand command, std::optional<double> seek_beat) {
    switch (command) {
        case TransportCommand::Play: transport_.playing = true; break;
        case TransportCommand::Pause: transport_.playing = false; break;
        case TransportCommand::Stop: transport_.playing = false; transport_.recording = false; transport_.beat = 0.0; break;
        case TransportCommand::Record: transport_.recording = true; transport_.playing = true; break;
        case TransportCommand::Seek:
            if (!seek_beat || !std::isfinite(*seek_beat) || *seek_beat < 0.0) throw std::invalid_argument("L38 seek beat invalid");
            transport_.beat = *seek_beat;
            break;
        case TransportCommand::None: break;
    }
    ++transport_.revision;
}

const TrinityTransportState& TrinityDeviceBus::transport() const noexcept { return transport_; }

TrinityEvent TrinityDeviceBus::make_event(TrinityDeviceKind kind, double beat, DeviceGesture gesture, TrinityEmotionState emotion) {
    if (!connected(kind)) throw std::runtime_error("L38 device is not connected");
    if (!std::isfinite(beat) || beat < 0.0) throw std::invalid_argument("L38 event beat invalid");
    validate_gesture(gesture);
    validate_emotion(emotion);
    gesture.device = gesture_name_for(kind);

    TrinityEvent event;
    event.sequence = next_sequence_++;
    event.device_id = id_for(kind);
    event.kind = kind;
    event.beat = beat;
    event.gesture = gesture;
    event.emotion = emotion;
    event.performance.pressure = gesture.pressure;
    event.performance.velocity = gesture.pressure;
    event.performance.expression = 0.5 * gesture.y + 0.5 * emotion.intensity;
    event.performance.timing_confidence = 1.0;

    if (kind == TrinityDeviceKind::SigilGuitar || kind == TrinityDeviceKind::GlyphPad)
        event.midi = midi_.map_device_gesture(gesture);
    else {
        if (gesture.gate) {
            event.transport = gesture.x < 0.33 ? TransportCommand::Play : (gesture.x < 0.66 ? TransportCommand::Record : TransportCommand::Stop);
        } else if (gesture.y > 0.0) {
            event.transport = TransportCommand::Seek;
            event.seek_beat = gesture.y * 256.0;
        }
    }
    return event;
}

TrinityEvent TrinityDeviceBus::ingest(TrinityDeviceKind kind, DeviceGesture gesture, TrinityEmotionState emotion) {
    auto event = make_event(kind, transport_.beat, std::move(gesture), emotion);
    auto& d = mutable_device(kind);
    d.emotion = emotion;
    d.performance = event.performance;
    ++d.event_count;
    if (event.transport != TransportCommand::None) apply_transport(event.transport, event.seek_beat);
    memory_.push_back(event);
    return event;
}

TrinityEvent TrinityDeviceBus::schedule(TrinityDeviceKind kind, double beat, DeviceGesture gesture, TrinityEmotionState emotion) {
    auto event = make_event(kind, beat, std::move(gesture), emotion);
    scheduled_.push_back(event);
    std::stable_sort(scheduled_.begin(), scheduled_.end(), [](const TrinityEvent& a, const TrinityEvent& b) {
        if (a.beat == b.beat) return a.sequence < b.sequence;
        return a.beat < b.beat;
    });
    return event;
}

std::vector<TrinityEvent> TrinityDeviceBus::advance_to(double beat) {
    if (!std::isfinite(beat) || beat < transport_.beat) throw std::invalid_argument("L38 transport cannot advance backwards");
    transport_.beat = beat;
    ++transport_.revision;
    std::vector<TrinityEvent> due;
    auto it = scheduled_.begin();
    while (it != scheduled_.end() && it->beat <= beat) {
        auto event = *it;
        auto& d = mutable_device(event.kind);
        d.emotion = event.emotion;
        d.performance = event.performance;
        ++d.event_count;
        if (event.transport != TransportCommand::None) apply_transport(event.transport, event.seek_beat);
        memory_.push_back(event);
        due.push_back(std::move(event));
        ++it;
    }
    scheduled_.erase(scheduled_.begin(), it);
    return due;
}

const std::vector<TrinityEvent>& TrinityDeviceBus::memory() const noexcept { return memory_; }

TrinitySnapshot TrinityDeviceBus::snapshot() const {
    TrinitySnapshot out;
    out.transport = transport_;
    out.devices = {device(TrinityDeviceKind::SigilGuitar), device(TrinityDeviceKind::GlyphPad), device(TrinityDeviceKind::ThreadDeck)};
    out.memory = memory_;
    return out;
}

std::string TrinityDeviceBus::serialize() const {
    std::ostringstream out;
    out << "XENON_TRINITY_BUS|1\n";
    out << transport_.playing << ' ' << transport_.recording << ' ' << transport_.beat << ' ' << transport_.bpm << ' ' << transport_.revision << ' ' << next_sequence_ << '\n';
    out << 3 << '\n';
    for (auto kind : {TrinityDeviceKind::SigilGuitar, TrinityDeviceKind::GlyphPad, TrinityDeviceKind::ThreadDeck}) {
        const auto& d = device(kind);
        out << kind_name(kind) << ' ' << d.connected << ' ' << d.emotion.intensity << ' ' << d.emotion.tension << ' ' << d.emotion.warmth << ' ' << d.emotion.motion << ' '
            << d.performance.pressure << ' ' << d.performance.velocity << ' ' << d.performance.expression << ' ' << d.performance.timing_confidence << ' ' << d.event_count << '\n';
    }
    out << memory_.size() << '\n';
    for (const auto& e : memory_) {
        out << e.sequence << ' ' << kind_name(e.kind) << ' ' << e.beat << ' ' << e.gesture.x << ' ' << e.gesture.y << ' ' << e.gesture.pressure << ' ' << e.gesture.gate << ' '
            << e.emotion.intensity << ' ' << e.emotion.tension << ' ' << e.emotion.warmth << ' ' << e.emotion.motion << ' ' << static_cast<int>(e.transport) << '\n';
    }
    return out.str();
}

TrinityDeviceBus TrinityDeviceBus::deserialize(const std::string& text) {
    std::istringstream in(text);
    std::string header;
    std::getline(in, header);
    if (header != "XENON_TRINITY_BUS|1") throw std::invalid_argument("L38 unsupported Trinity schema");
    TrinityDeviceBus bus;
    if (!(in >> bus.transport_.playing >> bus.transport_.recording >> bus.transport_.beat >> bus.transport_.bpm >> bus.transport_.revision >> bus.next_sequence_))
        throw std::invalid_argument("L38 corrupt transport state");
    std::size_t device_count = 0;
    in >> device_count;
    if (device_count != 3) throw std::invalid_argument("L38 Trinity snapshot missing device");
    for (std::size_t i = 0; i < device_count; ++i) {
        std::string name;
        TrinityDeviceState state;
        if (!(in >> name >> state.connected >> state.emotion.intensity >> state.emotion.tension >> state.emotion.warmth >> state.emotion.motion
                 >> state.performance.pressure >> state.performance.velocity >> state.performance.expression >> state.performance.timing_confidence >> state.event_count))
            throw std::invalid_argument("L38 corrupt device state");
        state.kind = parse_kind(name);
        state.device_id = id_for(state.kind);
        validate_emotion(state.emotion);
        bus.devices_[key(state.kind)] = state;
    }
    std::size_t memory_count = 0;
    in >> memory_count;
    for (std::size_t i = 0; i < memory_count; ++i) {
        TrinityEvent event;
        std::string kind_string;
        int transport = 0;
        if (!(in >> event.sequence >> kind_string >> event.beat >> event.gesture.x >> event.gesture.y >> event.gesture.pressure >> event.gesture.gate
                 >> event.emotion.intensity >> event.emotion.tension >> event.emotion.warmth >> event.emotion.motion >> transport))
            throw std::invalid_argument("L38 corrupt event memory");
        event.kind = parse_kind(kind_string);
        event.device_id = id_for(event.kind);
        event.gesture.device = gesture_name_for(event.kind);
        event.transport = static_cast<TransportCommand>(transport);
        event.performance.pressure = event.gesture.pressure;
        event.performance.velocity = event.gesture.pressure;
        event.performance.expression = 0.5 * event.gesture.y + 0.5 * event.emotion.intensity;
        event.performance.timing_confidence = 1.0;
        if (event.kind == TrinityDeviceKind::SigilGuitar || event.kind == TrinityDeviceKind::GlyphPad)
            event.midi = bus.midi_.map_device_gesture(event.gesture);
        bus.memory_.push_back(std::move(event));
    }
    return bus;
}

} // namespace xenon
