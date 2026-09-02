#pragma once

#include "xenon/synesthesia_engine.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace xenon {

struct EtherHueBandState {
    double gain_db{0.0};
    double drive{0.0};
};

struct EtherHueParameters {
    EtherHueBandState low{};
    EtherHueBandState mid{};
    EtherHueBandState high{};
    double low_cut_hz{35.0};
    double high_cut_hz{18000.0};
    double saturation{0.0};
    double stereo_width{1.0};
    double transient_emphasis{0.0};
    double wet{1.0};
};

struct EtherHuePolicy {
    double max_band_gain_db{6.0};
    double max_saturation{0.75};
    double min_stereo_width{0.65};
    double max_stereo_width{1.60};
    double max_transient_emphasis{0.65};
    double smoothing{0.12};
};

class EtherHueDSP {
public:
    explicit EtherHueDSP(EtherHuePolicy policy = {});

    void reset(std::uint32_t sample_rate);
    void set_policy(EtherHuePolicy policy);
    [[nodiscard]] const EtherHuePolicy& policy() const noexcept;

    [[nodiscard]] EtherHueParameters map(const SynesthesiaState& state) const;
    [[nodiscard]] EtherHueParameters update(const SynesthesiaState& state);
    [[nodiscard]] const EtherHueParameters& current() const noexcept;

    void process_interleaved_stereo(std::span<float> samples);

private:
    void validate_policy(const EtherHuePolicy& policy) const;
    static double smooth_value(double current, double target, double amount);

    EtherHuePolicy policy_{};
    EtherHueParameters current_{};
    bool has_current_{false};
    std::uint32_t sample_rate_{44100};
    double low_l_{0.0};
    double low_r_{0.0};
    double prev_l_{0.0};
    double prev_r_{0.0};
};

} // namespace xenon
