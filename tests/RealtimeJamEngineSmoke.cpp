#include "xenon/composer_agent.hpp"
#include "xenon/ether_dna.hpp"
#include "xenon/realtime_jam_engine.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    try {
        xenon::GenerationRequest request;
        request.prompt = "live sparse dark instrumental";
        request.duration_seconds = 96.0;
        request.bpm = 100.0;
        request.key = "D minor";
        request.mutation_amount = 0.30;

        xenon::ComposerAgent composer;
        const auto agent_plan = composer.plan(request);
        const auto composition_plan = composer.to_composition_plan(agent_plan);
        xenon::EtherDNA dna_builder;
        const auto dna = dna_builder.capture(request, composition_plan);

        xenon::RealtimeJamEngine jam;
        jam.set_context(agent_plan, dna, xenon::control_component(xenon::ControlComponent::Harmony));

        xenon::JamResponse first{};
        for (int i = 0; i < 12; ++i) {
            xenon::LiveInputFrame frame;
            frame.timestamp_seconds = i * 0.05;
            frame.rms = 0.18 + i * 0.015;
            frame.transient_density = (i % 2 == 0) ? 0.70 : 0.35;
            frame.spectral_density = 0.42;
            frame.brightness = 0.38;
            frame.estimated_bpm = 100.0 + (i % 3 - 1) * 1.5;
            frame.beat_phase = std::fmod(i * 0.20, 1.0);
            frame.estimated_key = "D minor";
            first = jam.process(frame);
        }

        require(first.state.locked, "L32 failed to lock to stable live input");
        require(first.state.tempo_bpm > 98.0 && first.state.tempo_bpm < 102.0, "L32 tempo tracking drifted");
        require(first.state.key == "D minor", "L32 key following failed");
        require(first.ensemble.musicians.size() == 5, "L32 did not route through L31 ensemble");
        require(first.should_respond, "L32 did not schedule accompaniment");
        require(first.scheduled_response_seconds - 0.55 <= jam.policy().max_response_latency_seconds + 1e-9,
            "L32 exceeded response latency bound");

        bool harmony_locked = false;
        for (const auto& musician : first.ensemble.musicians) {
            if (musician.role == xenon::MusicianRole::Harmony) {
                harmony_locked = musician.locked;
                require(musician.variation == 0.0, "L32 live modulation broke Harmony lock");
            }
        }
        require(harmony_locked, "L32 Harmony lock did not propagate into live ensemble");

        xenon::LiveInputFrame dropout;
        dropout.timestamp_seconds = 0.70;
        dropout.dropout = true;
        auto held = jam.process(dropout);
        require(held.state.recovering, "L32 dropout did not enter recovery");
        require(held.call_response_mode == "hold", "L32 short dropout did not hold accompaniment");

        dropout.timestamp_seconds = 1.20;
        auto lost = jam.process(dropout);
        require(!lost.state.locked, "L32 prolonged dropout did not unlock");
        require(lost.call_response_mode == "listen", "L32 prolonged dropout did not return to listen mode");

        xenon::LiveInputFrame recovery;
        recovery.timestamp_seconds = 1.25;
        recovery.rms = 0.30;
        recovery.transient_density = 0.65;
        recovery.spectral_density = 0.50;
        recovery.brightness = 0.45;
        recovery.estimated_bpm = 104.0;
        recovery.beat_phase = 0.25;
        recovery.estimated_key = "E minor";
        const auto recovered = jam.process(recovery);
        require(recovered.state.locked, "L32 did not reacquire after dropout");
        require(!recovered.state.recovering, "L32 remained in recovery after valid signal");
        require(recovered.state.key == "E minor", "L32 did not adapt to live key change");
        require(recovered.state.tempo_bpm > first.state.tempo_bpm, "L32 tempo did not adapt upward");

        bool rejected = false;
        try {
            xenon::RealtimeJamPolicy bad;
            bad.max_response_latency_seconds = 0.0;
            xenon::RealtimeJamEngine invalid{bad};
            (void)invalid;
        } catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "L32 accepted invalid latency policy");

        std::cout << "L32 Realtime Jam Engine smoke passed\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "L32 smoke failed: " << ex.what() << '\n';
        return 1;
    }
}
