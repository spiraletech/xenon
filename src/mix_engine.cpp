#include "xenon/mix_engine.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace xenon {
namespace {

constexpr double kPi = 3.14159265358979323846;

double clamp01(double v) { return std::clamp(v, 0.0, 1.0); }
double db_to_gain(double db) { return std::pow(10.0, db / 20.0); }

double role_base_gain(StemRole role) {
    switch (role) {
        case StemRole::Drums: return -2.0;
        case StemRole::Bass: return -3.0;
        case StemRole::Vocals: return -1.0;
        case StemRole::Melody: return -4.0;
        case StemRole::Harmony: return -5.0;
        case StemRole::Texture: return -7.0;
        default: return -6.0;
    }
}

double role_pan(StemRole role) {
    switch (role) {
        case StemRole::Melody: return -0.18;
        case StemRole::Harmony: return 0.22;
        case StemRole::Texture: return 0.35;
        default: return 0.0;
    }
}

} // namespace

MixEngine::MixEngine(MixPolicy policy) : policy_(policy) {
    if (policy_.target_headroom_db < 0.0 || policy_.target_headroom_db > 18.0)
        throw std::invalid_argument("L35 headroom out of range");
    if (policy_.max_gain_adjust_db <= 0.0 || policy_.max_gain_adjust_db > 24.0)
        throw std::invalid_argument("L35 max gain adjustment out of range");
    if (policy_.max_pan < 0.0 || policy_.max_pan > 1.0)
        throw std::invalid_argument("L35 max pan out of range");
    if (policy_.masking_threshold < 0.0 || policy_.masking_threshold > 1.0)
        throw std::invalid_argument("L35 masking threshold out of range");
    if (policy_.max_passes == 0 || policy_.max_passes > 4)
        throw std::invalid_argument("L35 pass count out of range");
}

const MixPolicy& MixEngine::policy() const noexcept { return policy_; }

void MixEngine::validate(const std::vector<StemMixInput>& stems, std::uint32_t sample_rate) const {
    if (stems.empty()) throw std::invalid_argument("L35 requires at least one stem");
    if (sample_rate < 8000 || sample_rate > 384000) throw std::invalid_argument("L35 sample rate out of range");
    const auto frames = stems.front().interleaved_stereo.size();
    if (frames == 0 || frames % 2 != 0) throw std::invalid_argument("L35 stereo stem buffer invalid");
    for (const auto& stem : stems) {
        if (stem.stem_id.empty()) throw std::invalid_argument("L35 stem id missing");
        if (stem.interleaved_stereo.size() != frames || stem.interleaved_stereo.size() % 2 != 0)
            throw std::invalid_argument("L35 stem buffers must have matching stereo length");
    }
}

double MixEngine::masking_risk(const StemMixInput& a, const StemMixInput& b) const {
    const double bright_similarity = 1.0 - std::min(1.0, std::abs(a.analysis.brightness - b.analysis.brightness));
    const double density_similarity = 1.0 - std::min(1.0, std::abs(a.analysis.spectral_density - b.analysis.spectral_density));
    const double loud_overlap = clamp01((a.analysis.rms + b.analysis.rms) * 2.5);
    return clamp01(bright_similarity * 0.35 + density_similarity * 0.35 + loud_overlap * 0.30);
}

MixPlan MixEngine::plan(const std::vector<StemMixInput>& stems,
                        std::optional<MixReferenceTarget> reference) const {
    if (stems.empty()) throw std::invalid_argument("L35 requires stems for planning");
    MixPlan out;
    out.target = reference.value_or(MixReferenceTarget{});
    out.headroom_db = policy_.target_headroom_db;
    out.stems.reserve(stems.size());

    for (std::size_t i = 0; i < stems.size(); ++i) {
        const auto& stem = stems[i];
        StemMixDecision d;
        d.stem_id = stem.stem_id;
        d.role = stem.role;
        d.locked = stem.locked;
        d.gain_db = role_base_gain(stem.role);
        d.pan = std::clamp(role_pan(stem.role), -policy_.max_pan, policy_.max_pan);

        const double brightness_error = out.target.brightness - stem.analysis.brightness;
        d.high_tilt_db = std::clamp(brightness_error * 5.0, -4.0, 4.0);
        d.low_tilt_db = std::clamp(-brightness_error * 2.5, -3.0, 3.0);

        const double transient = clamp01(stem.analysis.transient_density);
        d.compressor_threshold = std::clamp(0.92 - transient * 0.35, 0.45, 0.95);
        d.compressor_ratio = 1.0 + transient * 3.0;

        for (std::size_t j = 0; j < stems.size(); ++j) {
            if (i == j) continue;
            d.masking_risk = std::max(d.masking_risk, masking_risk(stem, stems[j]));
        }
        if (d.masking_risk >= policy_.masking_threshold && !d.locked) {
            d.gain_db -= 1.5;
            d.high_tilt_db -= 0.8;
            d.revision_hint = std::string("reduce masking on ") + stem_role_name(stem.role);
        }
        if (d.locked) {
            d.gain_db = 0.0;
            d.pan = 0.0;
            d.low_tilt_db = 0.0;
            d.high_tilt_db = 0.0;
            d.compressor_ratio = 1.0;
            d.compressor_threshold = 1.0;
            d.revision_hint.clear();
        }
        d.gain_db = std::clamp(d.gain_db, -policy_.max_gain_adjust_db, policy_.max_gain_adjust_db);
        out.stems.push_back(std::move(d));
    }
    return out;
}

void MixEngine::render_pass(const std::vector<StemMixInput>& stems, const MixPlan& plan,
                            std::vector<float>& output) const {
    std::fill(output.begin(), output.end(), 0.0f);
    for (std::size_t s = 0; s < stems.size(); ++s) {
        const auto& input = stems[s].interleaved_stereo;
        const auto& d = plan.stems[s];
        const double gain = db_to_gain(d.gain_db);
        const double pan = std::clamp(d.pan, -1.0, 1.0);
        const double left_pan = std::cos((pan + 1.0) * kPi * 0.25);
        const double right_pan = std::sin((pan + 1.0) * kPi * 0.25);
        const double low = db_to_gain(d.low_tilt_db * 0.25);
        const double high = db_to_gain(d.high_tilt_db * 0.25);

        double prev_l = 0.0;
        double prev_r = 0.0;
        for (std::size_t i = 0; i < input.size(); i += 2) {
            const double l = input[i];
            const double r = input[i + 1];
            const double low_l = 0.82 * prev_l + 0.18 * l;
            const double low_r = 0.82 * prev_r + 0.18 * r;
            const double high_l = l - low_l;
            const double high_r = r - low_r;
            prev_l = low_l;
            prev_r = low_r;

            double xl = (low_l * low + high_l * high) * gain * left_pan;
            double xr = (low_r * low + high_r * high) * gain * right_pan;
            const double threshold = d.compressor_threshold;
            if (d.compressor_ratio > 1.0) {
                auto compress = [threshold, ratio=d.compressor_ratio](double x) {
                    const double a = std::abs(x);
                    if (a <= threshold) return x;
                    const double reduced = threshold + (a - threshold) / ratio;
                    return std::copysign(reduced, x);
                };
                xl = compress(xl);
                xr = compress(xr);
            }
            output[i] += static_cast<float>(xl);
            output[i + 1] += static_cast<float>(xr);
        }
    }

    const double ceiling = db_to_gain(-policy_.target_headroom_db);
    double peak = 0.0;
    for (float v : output) peak = std::max(peak, std::abs(static_cast<double>(v)));
    if (peak > ceiling && peak > 0.0) {
        const double trim = ceiling / peak;
        for (auto& v : output) v = static_cast<float>(v * trim);
    }
}

MixMetrics MixEngine::metrics(std::span<const float> stereo) const {
    MixMetrics m;
    if (stereo.empty()) return m;
    double sum_sq = 0.0;
    double sum_diff = 0.0;
    double sum_abs = 0.0;
    double high_motion = 0.0;
    double prev = 0.0;
    for (std::size_t i = 0; i < stereo.size(); i += 2) {
        const double l = stereo[i];
        const double r = stereo[i + 1];
        const double mono = (l + r) * 0.5;
        sum_sq += mono * mono;
        sum_abs += std::abs(mono);
        sum_diff += std::abs(l - r);
        high_motion += std::abs(mono - prev);
        prev = mono;
        m.peak = std::max({m.peak, std::abs(l), std::abs(r)});
    }
    const double frames = static_cast<double>(stereo.size() / 2);
    m.rms = std::sqrt(sum_sq / std::max(1.0, frames));
    m.stereo_width = clamp01((sum_diff / std::max(1.0, frames)) * 2.0);
    m.brightness = clamp01((high_motion / std::max(1.0, frames)) * 4.0);
    m.spectral_density = clamp01((sum_abs / std::max(1.0, frames)) * 3.0);
    return m;
}

MixResult MixEngine::mix(const std::vector<StemMixInput>& stems,
                         std::uint32_t sample_rate,
                         std::optional<MixReferenceTarget> reference) const {
    validate(stems, sample_rate);
    MixResult result;
    result.plan = plan(stems, reference);

    std::vector<float> raw(stems.front().interleaved_stereo.size(), 0.0f);
    MixPlan unity = result.plan;
    for (auto& d : unity.stems) {
        d.gain_db = 0.0; d.pan = 0.0; d.low_tilt_db = 0.0; d.high_tilt_db = 0.0;
        d.compressor_ratio = 1.0; d.compressor_threshold = 1.0;
    }
    render_pass(stems, unity, raw);
    result.before = metrics(raw);

    result.interleaved_stereo.resize(raw.size());
    for (std::uint32_t pass = 0; pass < policy_.max_passes; ++pass) {
        render_pass(stems, result.plan, result.interleaved_stereo);
        result.after = metrics(result.interleaved_stereo);
        result.after.masking_score = 0.0;
        for (const auto& d : result.plan.stems)
            result.after.masking_score = std::max(result.after.masking_score, d.masking_risk);
        result.passes = pass + 1;

        const double rms_error = result.after.rms - result.plan.target.rms;
        if (pass + 1 >= policy_.max_passes || std::abs(rms_error) < 0.025) break;
        for (auto& d : result.plan.stems) {
            if (d.locked) continue;
            d.gain_db = std::clamp(d.gain_db - rms_error * 12.0,
                                   -policy_.max_gain_adjust_db,
                                   policy_.max_gain_adjust_db);
        }
    }

    for (const auto& d : result.plan.stems)
        if (!d.revision_hint.empty()) result.revision_hints.push_back(d.revision_hint);

    if (result.after.masking_score >= policy_.masking_threshold)
        result.revision_hints.push_back("target only the highest-masking unlocked stem on the next revision");
    if (result.after.peak > db_to_gain(-policy_.target_headroom_db) + 1e-5)
        result.revision_hints.push_back("restore mix headroom before mastering");
    return result;
}

} // namespace xenon
