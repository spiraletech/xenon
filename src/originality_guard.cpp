#include "xenon/originality_guard.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>

namespace xenon {
namespace {

double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

double cosine_similarity(const std::array<double, 12>& a, const std::array<double, 12>& b) {
    double dot = 0.0;
    double aa = 0.0;
    double bb = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        dot += a[i] * b[i];
        aa += a[i] * a[i];
        bb += b[i] * b[i];
    }
    if (aa <= 1e-12 || bb <= 1e-12) return 0.0;
    return clamp01(dot / (std::sqrt(aa) * std::sqrt(bb)));
}

double grouped_similarity(
    const std::array<double, 12>& a,
    const std::array<double, 12>& b,
    std::size_t begin,
    std::size_t end) {

    double error = 0.0;
    for (std::size_t i = begin; i < end; ++i) {
        error += std::abs(a[i] - b[i]);
    }
    const double count = static_cast<double>(end - begin);
    return clamp01(1.0 - (error / std::max(1.0, count)));
}

std::string digest_signature(const AudioFingerprint& fingerprint) {
    std::uint64_t hash = 1469598103934665603ull;
    const auto mix = [&hash](std::uint64_t value) {
        for (int i = 0; i < 8; ++i) {
            hash ^= static_cast<std::uint8_t>((value >> (i * 8)) & 0xffu);
            hash *= 1099511628211ull;
        }
    };

    for (double value : fingerprint.spectral_signature) {
        mix(static_cast<std::uint64_t>(std::llround(clamp01(value) * 10000.0)));
    }
    mix(static_cast<std::uint64_t>(std::llround(clamp01(fingerprint.brightness) * 10000.0)));
    mix(static_cast<std::uint64_t>(std::llround(clamp01(fingerprint.spectral_density) * 10000.0)));
    mix(static_cast<std::uint64_t>(std::llround(clamp01(fingerprint.transient_density) * 10000.0)));

    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << hash;
    return out.str();
}

} // namespace

AudioFingerprint OriginalityGuard::fingerprint(const TrackAnalysis& analysis) const {
    AudioFingerprint result;
    if (analysis.frames.empty()) return result;

    for (const auto& frame : analysis.frames) {
        result.brightness += frame.brightness;
        result.spectral_density += frame.spectral_density;
        result.transient_density += frame.transient_density;

        for (std::size_t group = 0; group < result.spectral_signature.size(); ++group) {
            double sum = 0.0;
            const std::size_t first = group * 6;
            for (std::size_t bar = first; bar < first + 6; ++bar) {
                sum += frame.bars[bar];
            }
            result.spectral_signature[group] += sum / 6.0;
        }
    }

    const double count = static_cast<double>(analysis.frames.size());
    result.brightness /= count;
    result.spectral_density /= count;
    result.transient_density /= count;
    for (double& value : result.spectral_signature) value /= count;

    const double maximum = *std::max_element(
        result.spectral_signature.begin(), result.spectral_signature.end());
    if (maximum > 1e-12) {
        for (double& value : result.spectral_signature) value = clamp01(value / maximum);
    }

    result.digest = digest_signature(result);
    return result;
}

SimilarityReport OriginalityGuard::compare(
    const AudioFingerprint& a,
    const AudioFingerprint& b) const {

    SimilarityReport report;
    report.spectral_similarity = cosine_similarity(a.spectral_signature, b.spectral_signature);

    // L23 heuristic proxies until dedicated pitch/chroma extraction lands:
    // upper contour approximates melodic shape; low/mid contour approximates harmonic bed.
    report.melodic_similarity = grouped_similarity(a.spectral_signature, b.spectral_signature, 5, 12);
    report.harmonic_similarity = grouped_similarity(a.spectral_signature, b.spectral_signature, 0, 7);

    const double feature_distance = (
        std::abs(a.brightness - b.brightness) +
        std::abs(a.spectral_density - b.spectral_density) +
        std::abs(a.transient_density - b.transient_density)) / 3.0;
    report.fingerprint_similarity = a.digest == b.digest
        ? 1.0
        : clamp01((report.spectral_similarity * 0.85) + ((1.0 - feature_distance) * 0.15));

    report.overall_similarity = clamp01(
        (report.fingerprint_similarity * 0.35) +
        (report.melodic_similarity * 0.20) +
        (report.harmonic_similarity * 0.20) +
        (report.spectral_similarity * 0.25));
    return report;
}

OriginalityAssessment OriginalityGuard::assess(
    std::string candidate_id,
    std::string backend_name,
    std::string ether_dna_fingerprint,
    std::string parent_ether_dna_fingerprint,
    std::uint64_t seed,
    const std::filesystem::path& audio_path,
    const TrackAnalysis& analysis,
    const std::vector<std::pair<std::string, AudioFingerprint>>& prior_candidates) const {

    OriginalityAssessment result;
    result.fingerprint = fingerprint(analysis);
    result.provenance = ProvenanceRecord{
        std::move(candidate_id),
        std::move(backend_name),
        std::move(ether_dna_fingerprint),
        std::move(parent_ether_dna_fingerprint),
        seed,
        audio_path
    };

    double nearest = 0.0;
    for (const auto& [id, prior] : prior_candidates) {
        const auto similarity = compare(result.fingerprint, prior);
        if (similarity.overall_similarity > nearest) {
            nearest = similarity.overall_similarity;
            result.nearest_match = similarity;
            result.nearest_candidate_id = id;
        }
    }

    result.release_risk = clamp01(nearest);
    const bool exact = result.nearest_match.fingerprint_similarity >= 0.9999;
    result.duplicate_suspected = exact || result.nearest_match.overall_similarity >= 0.985;
    result.release_blocked = exact || result.nearest_match.overall_similarity >= 0.995;

    if (result.duplicate_suspected) {
        result.reasons.emplace_back("candidate is highly similar to another XENON candidate");
    }
    if (result.release_blocked) {
        result.reasons.emplace_back("release gate blocked a near-duplicate candidate");
    }
    if (!result.nearest_candidate_id.empty() && result.release_risk >= 0.90) {
        result.reasons.emplace_back("high internal similarity requires review");
    }
    if (result.reasons.empty()) {
        result.reasons.emplace_back("no high-risk internal candidate similarity detected");
    }

    return result;
}

} // namespace xenon
