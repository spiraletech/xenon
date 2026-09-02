#include "xenon/synesthesia_scorer.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace xenon {
namespace {

double balance(double value, double target, double radius) {
    return std::clamp(1.0 - (std::abs(value - target) / radius), 0.0, 1.0);
}

} // namespace

SynesthesiaScore SynesthesiaScorer::score(const TrackAnalysis& analysis) const {
    if (analysis.frames.empty()) throw std::runtime_error("SynesthesiaScorer received no analysis frames");

    double rms = 0.0;
    double brightness = 0.0;
    double density = 0.0;
    double transients = 0.0;
    double motion_accum = 0.0;
    double previous_density = analysis.frames.front().spectral_density;

    for (const auto& frame : analysis.frames) {
        rms += frame.rms;
        brightness += frame.brightness;
        density += frame.spectral_density;
        transients += frame.transient_density;
        motion_accum += std::abs(frame.spectral_density - previous_density);
        previous_density = frame.spectral_density;
    }

    const double count = static_cast<double>(analysis.frames.size());
    rms /= count;
    brightness /= count;
    density /= count;
    transients /= count;
    motion_accum /= count;

    SynesthesiaScore out;
    out.energy = balance(rms, 0.35, 0.35);
    out.brightness_balance = balance(brightness, 0.50, 0.50);
    out.density_balance = balance(density, 0.50, 0.50);
    out.rhythmic_balance = balance(transients, 0.45, 0.45);
    out.warmth = std::clamp(1.0 - brightness, 0.0, 1.0);
    out.motion = std::clamp(motion_accum * 4.0, 0.0, 1.0);
    out.overall = std::clamp(
        out.energy * 0.25 +
        out.brightness_balance * 0.20 +
        out.density_balance * 0.25 +
        out.rhythmic_balance * 0.25 +
        out.motion * 0.05,
        0.0,
        1.0);
    return out;
}

} // namespace xenon
