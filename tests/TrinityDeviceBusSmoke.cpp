#include "xenon/trinity_device_bus.hpp"

#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
}

int main() {
    try {
        xenon::TrinityDeviceBus bus;
        bus.connect(xenon::TrinityDeviceKind::SigilGuitar);
        bus.connect(xenon::TrinityDeviceKind::GlyphPad);
        bus.connect(xenon::TrinityDeviceKind::ThreadDeck);
        require(bus.connected(xenon::TrinityDeviceKind::SigilGuitar), "L38 Sigil did not connect");
        require(bus.connected(xenon::TrinityDeviceKind::GlyphPad), "L38 GlyphPad did not connect");
        require(bus.connected(xenon::TrinityDeviceKind::ThreadDeck), "L38 ThreadDeck did not connect");

        bus.set_transport(136.0, 4.0);
        bus.apply_transport(xenon::TransportCommand::Play);
        require(bus.transport().playing && bus.transport().bpm == 136.0, "L38 shared transport state failed");

        xenon::DeviceGesture sigil{"", 0.45, 0.75, 0.82, true};
        xenon::TrinityEmotionState emotion{0.8, 0.7, 0.3, 0.9};
        const auto sigil_event = bus.ingest(xenon::TrinityDeviceKind::SigilGuitar, sigil, emotion);
        require(!sigil_event.midi.empty(), "L38 Sigil did not emit MIDI");
        require(sigil_event.performance.pressure == sigil.pressure, "L38 performance pressure not captured");
        require(bus.device(xenon::TrinityDeviceKind::SigilGuitar).emotion.intensity == emotion.intensity, "L38 emotion state not shared");

        xenon::DeviceGesture glyph{"", 0.25, 0.60, 0.55, true};
        const auto scheduled_glyph = bus.schedule(xenon::TrinityDeviceKind::GlyphPad, 8.0, glyph, {0.4,0.2,0.7,0.3});
        require(!scheduled_glyph.midi.empty(), "L38 GlyphPad scheduled MIDI missing");

        xenon::DeviceGesture deck_play{"", 0.10, 0.0, 0.5, true};
        const auto scheduled_deck = bus.schedule(xenon::TrinityDeviceKind::ThreadDeck, 6.0, deck_play, {0.2,0.1,0.5,0.2});
        require(scheduled_deck.transport == xenon::TransportCommand::Play, "L38 ThreadDeck transport mapping wrong");

        const auto due6 = bus.advance_to(6.0);
        require(due6.size() == 1 && due6[0].kind == xenon::TrinityDeviceKind::ThreadDeck, "L38 scheduler order wrong at beat 6");
        const auto due8 = bus.advance_to(8.0);
        require(due8.size() == 1 && due8[0].kind == xenon::TrinityDeviceKind::GlyphPad, "L38 scheduler order wrong at beat 8");
        require(bus.memory().size() == 3, "L38 event memory count wrong");

        xenon::DeviceGesture deck_seek{"", 0.5, 0.5, 0.2, false};
        const auto seek = bus.ingest(xenon::TrinityDeviceKind::ThreadDeck, deck_seek);
        require(seek.transport == xenon::TransportCommand::Seek, "L38 ThreadDeck seek mapping missing");
        require(bus.transport().beat == 128.0, "L38 ThreadDeck seek did not update transport");

        const auto encoded = bus.serialize();
        const auto restored = xenon::TrinityDeviceBus::deserialize(encoded);
        require(restored.transport().beat == bus.transport().beat, "L38 transport persistence failed");
        require(restored.memory().size() == bus.memory().size(), "L38 event memory persistence failed");
        require(restored.device(xenon::TrinityDeviceKind::SigilGuitar).event_count == bus.device(xenon::TrinityDeviceKind::SigilGuitar).event_count,
                "L38 device state persistence failed");

        bus.disconnect(xenon::TrinityDeviceKind::SigilGuitar);
        bool disconnected_rejected = false;
        try { (void)bus.ingest(xenon::TrinityDeviceKind::SigilGuitar, sigil); }
        catch (const std::runtime_error&) { disconnected_rejected = true; }
        require(disconnected_rejected, "L38 accepted input from disconnected device");

        bool invalid_emotion_rejected = false;
        try { (void)bus.ingest(xenon::TrinityDeviceKind::GlyphPad, glyph, {1.2,0.0,0.5,0.0}); }
        catch (const std::invalid_argument&) { invalid_emotion_rejected = true; }
        require(invalid_emotion_rejected, "L38 accepted invalid emotion metadata");

        bool backwards_rejected = false;
        try { (void)bus.advance_to(2.0); }
        catch (const std::invalid_argument&) { backwards_rejected = true; }
        require(backwards_rejected, "L38 transport allowed backwards advance");

        std::cout << "L38 Trinity Device Bus smoke passed\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "L38 smoke failed: " << ex.what() << '\n';
        return 1;
    }
}
