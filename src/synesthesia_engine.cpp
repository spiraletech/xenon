#include "xenon/synesthesia_engine.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace xenon {
namespace {

double clamp01(double v) { return std::clamp(v, 0.0, 1.0); }

double balance(double value, double target, double radius) {
    return clamp01(1.0 - std::abs(value - target) / radius);
}

struct FrameMeans {
    double rms{0.0};
    double brightness{0.0};
    double density{0.0};
    double transients{0.0};
    double motion{0.0};
    double texture{0.0};
};

FrameMeans means(const TrackAnalysis& analysis) {
    FrameMeans out;
    double previous_density = analysis.frames.front().spectral_density;
    double previous_brightness = analysis.frames.front().brightness;
    for (const auto& frame : analysis.frames) {
        out.rms += frame.rms;
        out.brightness += frame.brightness;
        out.density += frame.spectral_density;
        out.transients += frame.transient_density;
        out.motion += std::abs(frame.spectral_density - previous_density);
        out.texture += std::abs(frame.brightness - previous_brightness) +
            std::abs(frame.spectral_density - previous_density);
        previous_density = frame.spectral_density;
        previous_brightness = frame.brightness;
    }
    const auto count = static_cast<double>(analysis.frames.size());
    out.rms /= count;
    out.brightness /= count;
    out.density /= count;
    out.transients /= count;
    out.motion /= count;
    out.texture /= count;
    return out;
}

} // namespace

SynesthesiaState SynesthesiaEngine::perceive(
    const TrackAnalysis& analysis,
    std::size_t contour_points) const {
    if (analysis.frames.empty()) throw std::runtime_error("SynesthesiaEngine received no analysis frames");
    if (contour_points == 0) throw std::invalid_argument("SynesthesiaEngine contour point count must be positive");

    const auto avg = means(analysis);
    SynesthesiaState out;
    out.energy = clamp01(avg.rms / 0.50);
    out.brightness = clamp01(avg.brightness);
    out.luminance = clamp01(avg.rms * 1.25 + avg.brightness * 0.35);
    out.warmth = clamp01(1.0 - avg.brightness);
    out.density = clamp01(avg.density);
    out.motion = clamp01(avg.motion * 4.0);
    out.texture = clamp01(avg.texture * 3.0 + avg.density * 0.20);
    out.tension = clamp01(avg.transients * 0.45 + avg.brightness * 0.25 + avg.density * 0.30);

    // Circular perceptual hue: warmth rotates toward amber/red, brightness toward cyan/blue.
    out.hue = std::fmod(35.0 + out.brightness * 190.0 + out.motion * 85.0, 360.0) / 360.0;
    out.psychedelic_index = clamp01(
        out.motion * 0.30 + out.texture * 0.25 + out.tension * 0.20 +
        std::abs(out.warmth - 0.5) * 0.20 + out.density * 0.15);

    const std::size_t points = std::min(contour_points, analysis.frames.size());
    out.emotional_contour.reserve(points);
    for (std::size_t p = 0; p < points; ++p) {
        const std::size_t begin = p * analysis.frames.size() / points;
        const std::size_t end = std::max(begin + 1, (p + 1) * analysis.frames.size() / points);
        double rms = 0.0, bright = 0.0, density = 0.0, transient = 0.0, motion = 0.0;
        double previous = analysis.frames[begin].spectral_density;
        for (std::size_t i = begin; i < end; ++i) {
            const auto& frame = analysis.frames[i];
            rms += frame.rms;
            bright += frame.brightness;
            density += frame.spectral_density;
            transient += frame.transient_density;
            motion += std::abs(frame.spectral_density - previous);
            previous = frame.spectral_density;
        }
        const double n = static_cast<double>(end - begin);
        EmotionalContourPoint point;
        point.position = points == 1 ? 0.0 : static_cast<double>(p) / static_cast<double>(points - 1);
        point.energy = clamp01((rms / n) / 0.50);
        point.warmth = clamp01(1.0 - bright / n);
        point.motion = clamp01((motion / n) * 4.0);
        point.tension = clamp01((transient / n) * 0.45 + (bright / n) * 0.25 + (density / n) * 0.30);
        out.emotional_contour.push_back(point);
    }
    return out;
}

SynesthesiaScore SynesthesiaEngine::score(const SynesthesiaState& state) const {
    SynesthesiaScore out;
    out.energy = balance(state.energy, 0.70, 0.70);
    out.brightness_balance = balance(state.brightness, 0.50, 0.50);
    out.density_balance = balance(state.density, 0.50, 0.50);
    out.rhythmic_balance = balance(state.tension, 0.45, 0.55);
    out.warmth = clamp01(state.warmth);
    out.motion = clamp01(state.motion);
    out.overall = clamp01(out.energy * 0.20 + out.brightness_balance * 0.15 +
        out.density_balance * 0.20 + out.rhythmic_balance * 0.15 +
        out.motion * 0.10 + balance(state.texture, 0.45, 0.55) * 0.10 +
        balance(state.psychedelic_index, 0.45, 0.55) * 0.10);
    return out;
}

} // namespace xenon
