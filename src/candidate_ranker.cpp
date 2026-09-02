#include "xenon/candidate_ranker.hpp"

#include <algorithm>

namespace xenon {

void CandidateRanker::rank(CandidatePool& pool) const {
    for (auto& candidate : pool.candidates) {
        candidate.ranking_score = std::clamp(
            (candidate.synesthesia.overall * 0.75) +
            (candidate.critique.score * 0.25),
            0.0,
            1.0);
    }

    std::stable_sort(pool.candidates.begin(), pool.candidates.end(),
        [](const CandidateRecord& a, const CandidateRecord& b) {
            if (a.ranking_score == b.ranking_score) {
                return a.candidate_id < b.candidate_id;
            }
            return a.ranking_score > b.ranking_score;
        });

    pool.winner_index = 0;
}

} // namespace xenon
