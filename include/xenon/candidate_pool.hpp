#pragma once

#include "xenon/cortex_critic.hpp"
#include "xenon/generation_pipeline.hpp"
#include "xenon/originality_guard.hpp"
#include "xenon/synesthesia_scorer.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace xenon {

struct CandidateRecord {
    std::string candidate_id;
    GenerationResult generation;
    SynesthesiaScore synesthesia;
    CortexCritique critique;
    OriginalityAssessment originality;
    double ranking_score{0.0};
};

struct CandidateFailure {
    RouteDecision route;
    std::string stage;
    std::string error;
};

struct CandidatePool {
    std::vector<CandidateRecord> candidates;
    std::vector<CandidateFailure> failures;
    std::size_t winner_index{0};

    [[nodiscard]] bool has_winner() const noexcept {
        return !candidates.empty() && winner_index < candidates.size() &&
            !candidates[winner_index].originality.release_blocked;
    }

    [[nodiscard]] const CandidateRecord& winner() const {
        if (!has_winner()) throw std::runtime_error("XENON candidate pool has no releasable winner");
        return candidates[winner_index];
    }
};

} // namespace xenon
