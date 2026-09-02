#include "xenon/cortex_critic.hpp"

#include <algorithm>
#include <stdexcept>

namespace xenon {

CortexCritique CortexCritic::critique(
    const TrackAnalysis& analysis,
    const SynesthesiaScore& synesthesia) const {

    if (analysis.frames.empty()) throw std::runtime_error("CortexCritic received no analysis frames");

    double brightness = 0.0;
    double density = 0.0;
    double transients = 0.0;
    for (const auto& frame : analysis.frames) {
        brightness += frame.brightness;
        density += frame.spectral_density;
        transients += frame.transient_density;
    }
    const double count = static_cast<double>(analysis.frames.size());
    brightness /= count;
    density /= count;
    transients /= count;

    CortexCritique out;
    out.score = synesthesia.overall;

    if (transients > 0.68) {
        out.observations.emplace_back("transients are crowded");
        out.revision_hint = "reduce drum and hat activity while preserving bass, melody, harmony, texture, and arrangement";
    } else if (transients < 0.20) {
        out.observations.emplace_back("transients are too soft");
        out.revision_hint = "increase rhythmic definition while preserving the core musical identity";
    }

    if (brightness > 0.72) {
        out.observations.emplace_back("spectral balance is bright");
        if (out.revision_hint.empty()) out.revision_hint = "darken the upper spectrum slightly";
    } else if (brightness < 0.22) {
        out.observations.emplace_back("spectral balance is dark");
        if (out.revision_hint.empty()) out.revision_hint = "add restrained upper-frequency clarity";
    }

    if (density > 0.76) {
        out.observations.emplace_back("arrangement is spectrally dense");
        if (out.revision_hint.empty()) out.revision_hint = "create more negative space without changing the core motif";
    }

    if (synesthesia.motion < 0.08) {
        out.observations.emplace_back("spectral motion is static");
        if (out.revision_hint.empty()) out.revision_hint = "introduce subtle timbral motion without increasing arrangement density";
    }

    if (out.observations.empty()) {
        out.observations.emplace_back("balance is within the L21 target window");
        out.revision_hint = "preserve the current balance and apply only a subtle mutation";
    }

    return out;
}

} // namespace xenon
