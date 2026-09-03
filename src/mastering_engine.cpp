#include "xenon/mastering_engine.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace xenon {
namespace {

constexpr double kEps = 1e-12;

double clamp01(double v) { return std::clamp(v, 0.0, 1.0); }
double db_to_gain(double db) { return std::pow(10.0, db / 20.0); }
double gain_to_db(double gain) { return 20.0 * std::log10(std::max(gain, kEps)); }

} // namespace

MasteringTarget MasteringEngine::preset(MasteringPreset p) {
    MasteringTarget t;
    switch (p) {
        case MasteringPreset::StreamingBalanced:
            t.loudness_lufs = -14.0; t.ceiling_dbfs = -1.0; t.brightness = 0.50; t.stereo_width = 0.55;
            t.export_profile = {"streaming-balanced", 44100, 24, true};
            break;
        case MasteringPreset::StreamingLoud:
            t.loudness_lufs = -9.0; t.ceiling_dbfs = -0.8; t.brightness = 0.54; t.stereo_width = 0.58;
            t.export_profile = {"streaming-loud", 48000, 24, true};
            break;
        case MasteringPreset::Dynamic:
            t.loudness_lufs = -16.0; t.ceiling_dbfs = -1.5; t.brightness = 0.48; t.stereo_width = 0.52;
            t.export_profile = {"dynamic", 48000, 24, true};
            break;
        case MasteringPreset::Archive:
            t.loudness_lufs = -18.0; t.ceiling_dbfs = -2.0; t.brightness = 0.50; t.stereo_width = 0.50;
            t.export_profile = {"archive", 96000, 24, false};
            break;
    }
    return t;
}

MasteringEngine::MasteringEngine(MasteringPreset p) : target_(preset(p)) { validate_target(target_); }
MasteringEngine::MasteringEngine(MasteringTarget target) : target_(std::move(target)) { validate_target(target_); }

const MasteringTarget& MasteringEngine::target() const noexcept { return target_; }

void MasteringEngine::validate_target(const MasteringTarget& target) const {
    if (target.loudness_lufs < -36.0 || target.loudness_lufs > -5.0)
        throw std::invalid_argument("L36 loudness target out of range");
    if (target.ceiling_dbfs < -6.0 || target.ceiling_dbfs > -0.05)
        throw std::invalid_argument("L36 ceiling out of range");
    if (target.brightness < 0.0 || target.brightness > 1.0)
        throw std::invalid_argument("L36 brightness target out of range");
    if (target.stereo_width < 0.0 || target.stereo_width > 1.0 || target.max_width <= 0.0 || target.max_width > 1.0)
        throw std::invalid_argument("L36 stereo target out of range");
    if (target.max_gain_db <= 0.0 || target.max_gain_db > 24.0)
        throw std::invalid_argument("L36 max gain out of range");
    if (target.loudness_tolerance_lu <= 0.0 || target.loudness_tolerance_lu > 4.0)
        throw std::invalid_argument("L36 loudness tolerance out of range");
    if (target.export_profile.sample_rate < 8000 || target.export_profile.sample_rate > 384000)
        throw std::invalid_argument("L36 export sample rate out of range");
    if (target.export_profile.bit_depth != 16 && target.export_profile.bit_depth != 24 && target.export_profile.bit_depth != 32)
        throw std::invalid_argument("L36 export bit depth unsupported");
}

void MasteringEngine::validate_audio(std::span<const float> stereo) const {
    if (stereo.empty() || stereo.size() % 2 != 0)
        throw std::invalid_argument("L36 stereo buffer invalid");
    for (float v : stereo) {
        if (!std::isfinite(v)) throw std::invalid_argument("L36 non-finite audio sample");
    }
}

MasteringMetrics MasteringEngine::analyze(std::span<const float> stereo) const {
    validate_audio(stereo);
    MasteringMetrics m;
    double sum_sq = 0.0;
    double sum_diff = 0.0;
    double motion = 0.0;
    double prev_mono = 0.0;
    double peak = 0.0;
    double over_peak = 0.0;

    for (std::size_t i = 0; i < stereo.size(); i += 2) {
        const double l = stereo[i];
        const double r = stereo[i + 1];
        const double mono = 0.5 * (l + r);
        sum_sq += mono * mono;
        sum_diff += std::abs(l - r);
        motion += std::abs(mono - prev_mono);
        prev_mono = mono;
        peak = std::max({peak, std::abs(l), std::abs(r)});

        if (i >= 2) {
            const double pl = stereo[i - 2];
            const double pr = stereo[i - 1];
            for (int k = 1; k <= 3; ++k) {
                const double a = static_cast<double>(k) / 4.0;
                const double il = pl + (l - pl) * a;
                const double ir = pr + (r - pr) * a;
                over_peak = std::max({over_peak, std::abs(il), std::abs(ir)});
            }
        }
    }

    const double frames = static_cast<double>(stereo.size() / 2);
    m.rms = std::sqrt(sum_sq / std::max(1.0, frames));
    m.loudness_lufs_estimate = gain_to_db(m.rms) - 0.691;
    m.sample_peak_dbfs = gain_to_db(peak);
    m.oversampled_peak_dbfs = gain_to_db(std::max(peak, over_peak));
    m.brightness = clamp01((motion / std::max(1.0, frames)) * 4.0);
    m.stereo_width = clamp01((sum_diff / std::max(1.0, frames)) * 2.0);
    return m;
}

MasteringPlan MasteringEngine::plan(std::span<const float> stereo) const {
    const auto metrics = analyze(stereo);
    MasteringPlan p;
    p.target = target_;
    p.input_gain_db = std::clamp(target_.loudness_lufs - metrics.loudness_lufs_estimate,
                                 -target_.max_gain_db, target_.max_gain_db);
    const double bright_error = target_.brightness - metrics.brightness;
    p.high_tilt_db = std::clamp(bright_error * 3.5, -2.5, 2.5);
    p.low_tilt_db = std::clamp(-bright_error * 1.75, -1.5, 1.5);
    const double width_ratio = metrics.stereo_width > 0.01 ? target_.stereo_width / metrics.stereo_width : 1.0;
    p.stereo_width = std::clamp(width_ratio, 0.60, target_.max_width / std::max(0.05, metrics.stereo_width));
    p.stereo_width = std::clamp(p.stereo_width, 0.60, 1.40);
    p.limiter_ceiling = db_to_gain(target_.ceiling_dbfs);
    return p;
}

void MasteringEngine::render_pass(std::span<const float> input,
                                  const MasteringPlan& plan,
                                  std::vector<float>& output) const {
    output.resize(input.size());
    const double gain = db_to_gain(plan.input_gain_db);
    const double low_gain = db_to_gain(plan.low_tilt_db * 0.25);
    const double high_gain = db_to_gain(plan.high_tilt_db * 0.25);
    double prev_mid = 0.0;

    for (std::size_t i = 0; i < input.size(); i += 2) {
        double l = input[i] * gain;
        double r = input[i + 1] * gain;
        double mid = 0.5 * (l + r);
        double side = 0.5 * (l - r) * plan.stereo_width;

        const double low = 0.88 * prev_mid + 0.12 * mid;
        const double high = mid - low;
        prev_mid = low;
        mid = low * low_gain + high * high_gain;

        l = mid + side;
        r = mid - side;

        const double ceiling = plan.limiter_ceiling;
        auto limit = [ceiling](double x) {
            const double ax = std::abs(x);
            if (ax <= ceiling) return x;
            const double over = ax - ceiling;
            const double softened = ceiling + over / (1.0 + over * 12.0);
            return std::copysign(std::min(softened, ceiling), x);
        };
        output[i] = static_cast<float>(limit(l));
        output[i + 1] = static_cast<float>(limit(r));
    }
}

std::vector<std::int32_t> MasteringEngine::quantize(
    std::span<const float> input, const MasterExportProfile& profile) const {
    const int bits = static_cast<int>(profile.bit_depth);
    const double scale = static_cast<double>((std::uint64_t{1} << (bits - 1)) - 1);
    std::vector<std::int32_t> out;
    out.reserve(input.size());
    std::uint32_t rng = 0x6d2b79f5u;
    auto next_noise = [&]() {
        rng = rng * 1664525u + 1013904223u;
        const double a = static_cast<double>(rng & 0xffffu) / 65535.0;
        rng = rng * 1664525u + 1013904223u;
        const double b = static_cast<double>(rng & 0xffffu) / 65535.0;
        return (a - b) / scale;
    };
    for (float sample : input) {
        double x = std::clamp(static_cast<double>(sample), -1.0, 1.0);
        if (profile.dither && bits < 32) x += next_noise();
        const double q = std::round(std::clamp(x, -1.0, 1.0) * scale);
        out.push_back(static_cast<std::int32_t>(q));
    }
    return out;
}

MasteringResult MasteringEngine::master(std::span<const float> stereo) const {
    validate_audio(stereo);
    MasteringResult result;
    result.before = analyze(stereo);
    result.plan = plan(stereo);

    render_pass(stereo, result.plan, result.interleaved_stereo);
    result.after = analyze(result.interleaved_stereo);
    result.passes = 1;

    const double loudness_error = target_.loudness_lufs - result.after.loudness_lufs_estimate;
    if (std::abs(loudness_error) > target_.loudness_tolerance_lu) {
        result.plan.input_gain_db = std::clamp(result.plan.input_gain_db + loudness_error,
                                               -target_.max_gain_db, target_.max_gain_db);
        render_pass(stereo, result.plan, result.interleaved_stereo);
        result.after = analyze(result.interleaved_stereo);
        result.passes = 2;
    }

    if (result.after.oversampled_peak_dbfs > target_.ceiling_dbfs + 0.05)
        result.quality_notes.push_back("oversampled peak exceeds configured mastering ceiling");
    if (std::abs(result.after.loudness_lufs_estimate - target_.loudness_lufs) > target_.loudness_tolerance_lu)
        result.quality_notes.push_back("loudness estimate remains outside configured tolerance");
    if (result.after.stereo_width > target_.max_width + 0.02)
        result.quality_notes.push_back("stereo width exceeds configured maximum");
    if (std::abs(result.after.brightness - target_.brightness) > 0.30)
        result.quality_notes.push_back("tonal brightness remains far from configured target");

    result.quality_gate_passed = result.quality_notes.empty();
    result.quantized_pcm = quantize(result.interleaved_stereo, target_.export_profile);
    return result;
}

} // namespace xenon
