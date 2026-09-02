#include "xenon/realtime_jam_engine.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace xenon {
namespace {

double clamp01(double v) { return std::clamp(v, 0.0, 1.0); }

double circular_lerp(double current, double target, double amount) {
    double delta = target - current;
    if (delta > 0.5) delta -= 1.0;
    if (delta < -0.5) delta += 1.0;
    double out = current + delta * amount;
    while (out < 0.0) out += 1.0;
    while (out >= 1.0) out -= 1.0;
    return out;
}

} // namespace

RealtimeJamEngine::RealtimeJamEngine(RealtimeJamPolicy policy)
    : policy_(policy) {
    validate_policy(policy_);
}

void RealtimeJamEngine::validate_policy(const RealtimeJamPolicy& policy) const {
    if (policy.tempo_smoothing <= 0.0 || policy.tempo_smoothing > 1.0)
        throw std::invalid_argument("L32 tempo smoothing out of range");
    if (policy.phase_smoothing <= 0.0 || policy.phase_smoothing > 1.0)
        throw std::invalid_argument("L32 phase smoothing out of range");
    if (policy.min_confidence_to_lock < 0.0 || policy.min_confidence_to_lock > 1.0)
        throw std::invalid_argument("L32 lock confidence out of range");
    if (policy.max_response_latency_seconds <= 0.0 || policy.max_response_latency_seconds > 0.5)
        throw std::invalid_argument("L32 response latency out of range");
    if (policy.dropout_hold_seconds < 0.0 || policy.dropout_hold_seconds > 5.0)
        throw std::invalid_argument("L32 dropout hold out of range");
}

void RealtimeJamEngine::set_context(
    ComposerAgentPlan composer_plan,
    EtherDNARecord dna,
    ControlComponents locks) {
    ComposerAgent validator;
    validator.validate(composer_plan);
    composer_plan_ = std::move(composer_plan);
    dna_ = std::move(dna);
    locks_ = locks;
}

void RealtimeJamEngine::clear_context() {
    composer_plan_.reset();
    dna_.reset();
    locks_ = 0;
}

void RealtimeJamEngine::reset() {
    state_ = {};
    last_non_dropout_seconds_ = 0.0;
}

const JamState& RealtimeJamEngine::state() const noexcept { return state_; }
const RealtimeJamPolicy& RealtimeJamEngine::policy() const noexcept { return policy_; }

EnsemblePlan RealtimeJamEngine::live_ensemble(double energy, double beat_phase) const {
    if (!composer_plan_ || !dna_) return {};
    MusicianAgentContext context;
    context.composer_plan = &*composer_plan_;
    context.dna = &*dna_;
    context.locks = locks_;
    auto plan = ensemble_.conduct(context);

    // Live modulation never breaks locks; it only scales unlocked intents.
    for (auto& musician : plan.musicians) {
        if (musician.locked) continue;
        const double pulse = 0.75 + 0.25 * std::cos(beat_phase * 6.283185307179586);
        musician.global_activity = clamp01(musician.global_activity * (0.55 + energy * 0.70) * pulse);
        musician.global_density = clamp01(musician.global_density * (0.70 + energy * 0.45));
        musician.variation = clamp01(musician.variation * (0.65 + (1.0 - energy) * 0.35));
        for (auto& section : musician.sections) {
            section.activity = clamp01(section.activity * (0.55 + energy * 0.70) * pulse);
            section.density = clamp01(section.density * (0.70 + energy * 0.45));
        }
    }
    return plan;
}

double RealtimeJamEngine::quantized_response_time(const LiveInputFrame& frame) const {
    if (state_.tempo_bpm <= 0.0) return frame.timestamp_seconds + policy_.max_response_latency_seconds;
    const double beat_seconds = 60.0 / state_.tempo_bpm;
    const double until_next_beat = (1.0 - state_.beat_phase) * beat_seconds;
    return frame.timestamp_seconds + std::min(until_next_beat, policy_.max_response_latency_seconds);
}

JamResponse RealtimeJamEngine::process(const LiveInputFrame& frame) {
    if (frame.timestamp_seconds < 0.0) throw std::invalid_argument("L32 negative timestamp");
    if (!frame.dropout && (frame.estimated_bpm < 0.0 || frame.beat_phase < 0.0 || frame.beat_phase > 1.0))
        throw std::invalid_argument("L32 live frame values invalid");

    ++state_.processed_frames;

    if (frame.dropout) {
        ++state_.dropout_frames;
        state_.recovering = true;
        if (frame.timestamp_seconds - last_non_dropout_seconds_ > policy_.dropout_hold_seconds) {
            state_.locked = false;
            state_.confidence *= 0.65;
        }
        JamResponse response;
        response.state = state_;
        response.ensemble = live_ensemble(state_.energy, state_.beat_phase);
        response.scheduled_response_seconds = frame.timestamp_seconds + policy_.max_response_latency_seconds;
        response.should_respond = state_.locked;
        response.call_response_mode = state_.locked ? "hold" : "listen";
        return response;
    }

    last_non_dropout_seconds_ = frame.timestamp_seconds;
    state_.recovering = false;

    if (frame.estimated_bpm > 0.0) {
        if (state_.tempo_bpm <= 0.0) state_.tempo_bpm = frame.estimated_bpm;
        else state_.tempo_bpm += (frame.estimated_bpm - state_.tempo_bpm) * policy_.tempo_smoothing;
    }

    state_.beat_phase = circular_lerp(state_.beat_phase, frame.beat_phase, policy_.phase_smoothing);
    if (!frame.estimated_key.empty()) state_.key = frame.estimated_key;
    else if (state_.key.empty() && dna_) state_.key = dna_->key;

    state_.energy = clamp01(frame.rms * 1.6 + frame.transient_density * 0.25 + frame.spectral_density * 0.15);
    const double tempo_conf = frame.estimated_bpm > 0.0 ? 1.0 : 0.25;
    const double key_conf = !state_.key.empty() ? 1.0 : 0.25;
    const double signal_conf = clamp01(frame.rms * 2.5 + frame.transient_density * 0.5);
    state_.confidence = clamp01(tempo_conf * 0.40 + key_conf * 0.25 + signal_conf * 0.35);
    state_.locked = state_.confidence >= policy_.min_confidence_to_lock;

    JamResponse response;
    response.state = state_;
    response.ensemble = live_ensemble(state_.energy, state_.beat_phase);
    response.scheduled_response_seconds = quantized_response_time(frame);
    response.should_respond = state_.locked && state_.energy > 0.08;

    if (!response.should_respond) response.call_response_mode = "listen";
    else if (state_.energy > 0.68) response.call_response_mode = "support";
    else if (state_.energy < 0.28) response.call_response_mode = "answer";
    else response.call_response_mode = "interlock";

    return response;
}

} // namespace xenon
