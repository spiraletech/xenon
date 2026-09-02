#pragma once

#include "xenon/composer_agent.hpp"
#include "xenon/ether_dna.hpp"
#include "xenon/musician_agents.hpp"
#include "xenon/synesthesia_engine.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace xenon {

struct LiveInputFrame {
    double timestamp_seconds{0.0};
    double rms{0.0};
    double transient_density{0.0};
    double spectral_density{0.0};
    double brightness{0.0};
    double estimated_bpm{0.0};
    double beat_phase{0.0};
    std::string estimated_key;
    bool dropout{false};
};

struct JamState {
    bool locked{false};
    bool recovering{false};
    double tempo_bpm{0.0};
    double beat_phase{0.0};
    std::string key;
    double energy{0.0};
    double confidence{0.0};
    std::uint64_t processed_frames{0};
    std::uint64_t dropout_frames{0};
};

struct JamResponse {
    JamState state;
    EnsemblePlan ensemble;
    double scheduled_response_seconds{0.0};
    bool should_respond{false};
    std::string call_response_mode;
};

struct RealtimeJamPolicy {
    double tempo_smoothing{0.18};
    double phase_smoothing{0.30};
    double min_confidence_to_lock{0.35};
    double max_response_latency_seconds{0.120};
    double dropout_hold_seconds{0.350};
};

class RealtimeJamEngine {
public:
    explicit RealtimeJamEngine(RealtimeJamPolicy policy = {});

    void set_context(ComposerAgentPlan composer_plan, EtherDNARecord dna, ControlComponents locks = 0);
    void clear_context();
    void reset();

    [[nodiscard]] JamResponse process(const LiveInputFrame& frame);
    [[nodiscard]] const JamState& state() const noexcept;
    [[nodiscard]] const RealtimeJamPolicy& policy() const noexcept;

private:
    void validate_policy(const RealtimeJamPolicy& policy) const;
    [[nodiscard]] EnsemblePlan live_ensemble(double energy, double beat_phase) const;
    [[nodiscard]] double quantized_response_time(const LiveInputFrame& frame) const;

    RealtimeJamPolicy policy_{};
    JamState state_{};
    std::optional<ComposerAgentPlan> composer_plan_;
    std::optional<EtherDNARecord> dna_;
    ControlComponents locks_{0};
    MusicianEnsemble ensemble_{};
    double last_non_dropout_seconds_{0.0};
};

} // namespace xenon
