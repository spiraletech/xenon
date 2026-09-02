#include "xenon/etherhue_dsp.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace xenon {
namespace {
constexpr double kPi = 3.14159265358979323846;
double clamp01(double v) { return std::clamp(v, 0.0, 1.0); }
double db_to_gain(double db) { return std::pow(10.0, db / 20.0); }
}

EtherHueDSP::EtherHueDSP(EtherHuePolicy policy) { set_policy(policy); reset(44100); }

void EtherHueDSP::validate_policy(const EtherHuePolicy& p) const {
    if (p.max_band_gain_db <= 0.0 || p.max_band_gain_db > 18.0) throw std::invalid_argument("ETHERHUE invalid max band gain");
    if (p.max_saturation < 0.0 || p.max_saturation > 1.0) throw std::invalid_argument("ETHERHUE invalid saturation bound");
    if (p.min_stereo_width <= 0.0 || p.max_stereo_width < p.min_stereo_width) throw std::invalid_argument("ETHERHUE invalid stereo width bounds");
    if (p.max_transient_emphasis < 0.0 || p.max_transient_emphasis > 1.0) throw std::invalid_argument("ETHERHUE invalid transient bound");
    if (p.smoothing <= 0.0 || p.smoothing > 1.0) throw std::invalid_argument("ETHERHUE smoothing must be in (0,1]");
}

void EtherHueDSP::set_policy(EtherHuePolicy policy) { validate_policy(policy); policy_ = policy; }
const EtherHuePolicy& EtherHueDSP::policy() const noexcept { return policy_; }

void EtherHueDSP::reset(std::uint32_t sample_rate) {
    if (sample_rate < 8000) throw std::invalid_argument("ETHERHUE sample rate too low");
    sample_rate_ = sample_rate;
    low_l_ = low_r_ = prev_l_ = prev_r_ = 0.0;
    has_current_ = false;
    current_ = {};
}

double EtherHueDSP::smooth_value(double current, double target, double amount) {
    return current + (target - current) * amount;
}

EtherHueParameters EtherHueDSP::map(const SynesthesiaState& s) const {
    EtherHueParameters p;
    const double maxg = policy_.max_band_gain_db;
    p.low.gain_db = std::clamp((s.warmth - 0.5) * maxg * 1.6, -maxg, maxg);
    p.mid.gain_db = std::clamp((s.texture - 0.5) * maxg * 1.2, -maxg, maxg);
    p.high.gain_db = std::clamp((s.brightness - 0.5) * maxg * 1.6, -maxg, maxg);
    p.low.drive = clamp01(s.density * 0.55 + s.warmth * 0.45);
    p.mid.drive = clamp01(s.texture * 0.60 + s.tension * 0.40);
    p.high.drive = clamp01(s.psychedelic_index * 0.55 + s.brightness * 0.45);
    p.low_cut_hz = 25.0 + s.density * 55.0;
    p.high_cut_hz = 20000.0 - s.tension * 5000.0;
    p.saturation = std::clamp((s.texture * 0.45 + s.psychedelic_index * 0.35 + s.tension * 0.20) * policy_.max_saturation, 0.0, policy_.max_saturation);
    p.stereo_width = std::clamp(1.0 + (s.motion - 0.5) * 0.7 + (s.psychedelic_index - 0.5) * 0.3, policy_.min_stereo_width, policy_.max_stereo_width);
    p.transient_emphasis = std::clamp((s.tension * 0.65 + s.motion * 0.35) * policy_.max_transient_emphasis, 0.0, policy_.max_transient_emphasis);
    p.wet = std::clamp(0.55 + s.luminance * 0.25 + s.psychedelic_index * 0.20, 0.0, 1.0);
    return p;
}

EtherHueParameters EtherHueDSP::update(const SynesthesiaState& state) {
    const auto t = map(state);
    if (!has_current_) { current_ = t; has_current_ = true; return current_; }
    const double a = policy_.smoothing;
    current_.low.gain_db = smooth_value(current_.low.gain_db, t.low.gain_db, a);
    current_.mid.gain_db = smooth_value(current_.mid.gain_db, t.mid.gain_db, a);
    current_.high.gain_db = smooth_value(current_.high.gain_db, t.high.gain_db, a);
    current_.low.drive = smooth_value(current_.low.drive, t.low.drive, a);
    current_.mid.drive = smooth_value(current_.mid.drive, t.mid.drive, a);
    current_.high.drive = smooth_value(current_.high.drive, t.high.drive, a);
    current_.low_cut_hz = smooth_value(current_.low_cut_hz, t.low_cut_hz, a);
    current_.high_cut_hz = smooth_value(current_.high_cut_hz, t.high_cut_hz, a);
    current_.saturation = smooth_value(current_.saturation, t.saturation, a);
    current_.stereo_width = smooth_value(current_.stereo_width, t.stereo_width, a);
    current_.transient_emphasis = smooth_value(current_.transient_emphasis, t.transient_emphasis, a);
    current_.wet = smooth_value(current_.wet, t.wet, a);
    return current_;
}

const EtherHueParameters& EtherHueDSP::current() const noexcept { return current_; }

void EtherHueDSP::process_interleaved_stereo(std::span<float> samples) {
    if (samples.size() % 2 != 0) throw std::invalid_argument("ETHERHUE requires interleaved stereo samples");
    const double alpha = 1.0 - std::exp(-2.0 * kPi * 320.0 / static_cast<double>(sample_rate_));
    const double low_g = db_to_gain(current_.low.gain_db);
    const double mid_g = db_to_gain(current_.mid.gain_db);
    const double high_g = db_to_gain(current_.high.gain_db);
    for (std::size_t i = 0; i < samples.size(); i += 2) {
        const double dry_l = samples[i], dry_r = samples[i+1];
        low_l_ += alpha * (dry_l - low_l_); low_r_ += alpha * (dry_r - low_r_);
        const double high_l = dry_l - low_l_, high_r = dry_r - low_r_;
        double l = low_l_ * low_g + (dry_l - low_l_ - high_l * 0.35) * mid_g + high_l * high_g;
        double r = low_r_ * low_g + (dry_r - low_r_ - high_r * 0.35) * mid_g + high_r * high_g;
        const double tr_l = l - prev_l_, tr_r = r - prev_r_; prev_l_ = l; prev_r_ = r;
        l += tr_l * current_.transient_emphasis; r += tr_r * current_.transient_emphasis;
        const double drive = 1.0 + current_.saturation * 4.0;
        l = std::tanh(l * drive) / std::tanh(drive); r = std::tanh(r * drive) / std::tanh(drive);
        const double mid = (l + r) * 0.5, side = (l - r) * 0.5 * current_.stereo_width;
        l = mid + side; r = mid - side;
        samples[i] = static_cast<float>(std::clamp(dry_l * (1.0-current_.wet) + l * current_.wet, -1.0, 1.0));
        samples[i+1] = static_cast<float>(std::clamp(dry_r * (1.0-current_.wet) + r * current_.wet, -1.0, 1.0));
    }
}

} // namespace xenon
