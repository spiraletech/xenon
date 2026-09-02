#pragma once

#include "xenon/media_analyzer.hpp"

#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace xenon {

struct AudioFingerprint {
    std::string digest;
    std::array<double, 12> spectral_signature{};
    double brightness{0.0};
    double spectral_density{0.0};
    double transient_density{0.0};
};

struct SimilarityReport {
    double fingerprint_similarity{0.0};
    double melodic_similarity{0.0};
    double harmonic_similarity{0.0};
    double spectral_similarity{0.0};
    double overall_similarity{0.0};
};

struct ProvenanceRecord {
    std::string candidate_id;
    std::string backend_name;
    std::string ether_dna_fingerprint;
    std::string parent_ether_dna_fingerprint;
    std::uint64_t seed{0};
    std::filesystem::path audio_path;
};

struct OriginalityAssessment {
    AudioFingerprint fingerprint;
    ProvenanceRecord provenance;
    SimilarityReport nearest_match;
    std::string nearest_candidate_id;
    double release_risk{0.0};
    bool duplicate_suspected{false};
    bool release_blocked{false};
    std::vector<std::string> reasons;
};

class OriginalityGuard {
public:
    [[nodiscard]] AudioFingerprint fingerprint(const TrackAnalysis& analysis) const;

    [[nodiscard]] SimilarityReport compare(
        const AudioFingerprint& a,
        const AudioFingerprint& b) const;

    [[nodiscard]] OriginalityAssessment assess(
        std::string candidate_id,
        std::string backend_name,
        std::string ether_dna_fingerprint,
        std::string parent_ether_dna_fingerprint,
        std::uint64_t seed,
        const std::filesystem::path& audio_path,
        const TrackAnalysis& analysis,
        const std::vector<std::pair<std::string, AudioFingerprint>>& prior_candidates) const;
};

} // namespace xenon
