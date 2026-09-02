#include "xenon/stem_analyzer.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace xenon {

StemRole StemAnalyzer::classify(const TrackAnalysis& analysis, double& confidence) const {
    if (analysis.frames.empty()) throw std::runtime_error("XENON stem analyzer received no frames");

    double rms = 0.0;
    double brightness = 0.0;
    double density = 0.0;
    double transients = 0.0;
    for (const auto& frame : analysis.frames) {
        rms += frame.rms;
        brightness += frame.brightness;
        density += frame.spectral_density;
        transients += frame.transient_density;
    }
    const double count = static_cast<double>(analysis.frames.size());
    rms /= count;
    brightness /= count;
    density /= count;
    transients /= count;

    struct ScoredRole { StemRole role; double score; };
    const ScoredRole scores[] = {
        {StemRole::Drums, std::clamp(transients * 0.75 + brightness * 0.15 + rms * 0.10, 0.0, 1.0)},
        {StemRole::Bass, std::clamp((1.0 - brightness) * 0.60 + rms * 0.25 + (1.0 - transients) * 0.15, 0.0, 1.0)},
        {StemRole::Melody, std::clamp(brightness * 0.35 + (1.0 - density) * 0.30 + (1.0 - transients) * 0.35, 0.0, 1.0)},
        {StemRole::Harmony, std::clamp(density * 0.45 + (1.0 - transients) * 0.35 + (1.0 - std::abs(brightness - 0.5)) * 0.20, 0.0, 1.0)},
        {StemRole::Texture, std::clamp(density * 0.35 + brightness * 0.25 + (1.0 - rms) * 0.20 + transients * 0.20, 0.0, 1.0)}
    };

    auto best = scores[0];
    auto second = ScoredRole{StemRole::Unknown, 0.0};
    for (const auto& candidate : scores) {
        if (candidate.score > best.score) {
            second = best;
            best = candidate;
        } else if (candidate.role != best.role && candidate.score > second.score) {
            second = candidate;
        }
    }

    confidence = std::clamp(0.55 + (best.score - second.score), 0.0, 1.0);
    return best.role;
}

StemAnalysis StemAnalyzer::analyze(const std::filesystem::path& audio_path) const {
    const auto analysis = analyzer_.analyzeFile(audio_path);
    if (analysis.frames.empty()) throw std::runtime_error("XENON stem analyzer produced no frames");

    StemAnalysis out;
    for (const auto& frame : analysis.frames) {
        out.rms += frame.rms;
        out.brightness += frame.brightness;
        out.spectral_density += frame.spectral_density;
        out.transient_density += frame.transient_density;
    }
    const double count = static_cast<double>(analysis.frames.size());
    out.rms /= count;
    out.brightness /= count;
    out.spectral_density /= count;
    out.transient_density /= count;
    out.synesthesia = synesthesia_.score(analysis);
    out.inferred_role = classify(analysis, out.role_confidence);
    return out;
}

} // namespace xenon
