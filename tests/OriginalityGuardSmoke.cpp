#include "xenon/originality_guard.hpp"

#include <iostream>
#include <utility>
#include <vector>

namespace {

xenon::TrackAnalysis make_analysis(double low, double high, double brightness, double density, double transients) {
    xenon::TrackAnalysis analysis;
    analysis.sample_rate = 44100;

    for (int frame_index = 0; frame_index < 8; ++frame_index) {
        xenon::SpectrumFrame frame;
        frame.rms = 0.30;
        frame.brightness = brightness;
        frame.spectral_density = density;
        frame.transient_density = transients;

        for (std::size_t i = 0; i < frame.bars.size(); ++i) {
            const double t = static_cast<double>(i) / static_cast<double>(frame.bars.size() - 1);
            frame.bars[i] = static_cast<float>(low + ((high - low) * t));
        }
        analysis.frames.push_back(frame);
    }
    return analysis;
}

} // namespace

int main() {
    xenon::OriginalityGuard guard;

    const auto original = make_analysis(0.90, 0.10, 0.35, 0.48, 0.42);
    const auto exact_copy = make_analysis(0.90, 0.10, 0.35, 0.48, 0.42);
    const auto distinct = make_analysis(0.08, 0.95, 0.78, 0.28, 0.70);

    const auto original_fingerprint = guard.fingerprint(original);
    if (original_fingerprint.digest.empty()) {
        std::cerr << "L23 fingerprint digest is empty\n";
        return 1;
    }

    const auto exact_similarity = guard.compare(original_fingerprint, guard.fingerprint(exact_copy));
    if (exact_similarity.overall_similarity < 0.999 || exact_similarity.fingerprint_similarity < 0.999) {
        std::cerr << "L23 failed to identify an exact fingerprint match\n";
        return 2;
    }

    std::vector<std::pair<std::string, xenon::AudioFingerprint>> prior{
        {"candidate-a", original_fingerprint}
    };

    const auto duplicate = guard.assess(
        "candidate-b", "backend-b", "dna-b", "dna-parent", 23,
        "candidate-b.wav", exact_copy, prior);

    if (!duplicate.duplicate_suspected || !duplicate.release_blocked ||
        duplicate.nearest_candidate_id != "candidate-a") {
        std::cerr << "L23 did not block the duplicate candidate\n";
        return 3;
    }
    if (duplicate.provenance.backend_name != "backend-b" ||
        duplicate.provenance.ether_dna_fingerprint != "dna-b" ||
        duplicate.provenance.parent_ether_dna_fingerprint != "dna-parent" ||
        duplicate.provenance.seed != 23) {
        std::cerr << "L23 provenance record lost generation lineage\n";
        return 4;
    }

    const auto safe = guard.assess(
        "candidate-c", "backend-c", "dna-c", {}, 24,
        "candidate-c.wav", distinct, prior);

    if (safe.release_blocked || safe.duplicate_suspected) {
        std::cerr << "L23 incorrectly blocked a distinct candidate\n";
        return 5;
    }
    if (safe.release_risk < 0.0 || safe.release_risk > 1.0) {
        std::cerr << "L23 release risk escaped normalized range\n";
        return 6;
    }
    if (safe.nearest_match.melodic_similarity < 0.0 || safe.nearest_match.melodic_similarity > 1.0 ||
        safe.nearest_match.harmonic_similarity < 0.0 || safe.nearest_match.harmonic_similarity > 1.0 ||
        safe.nearest_match.spectral_similarity < 0.0 || safe.nearest_match.spectral_similarity > 1.0) {
        std::cerr << "L23 similarity components escaped normalized range\n";
        return 7;
    }

    return 0;
}
