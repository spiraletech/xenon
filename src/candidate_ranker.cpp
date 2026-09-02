#include "xenon/candidate_ranker.hpp"

#include <algorithm>

namespace xenon {

void CandidateRanker::rank(CandidatePool& pool) const {
    for (auto& candidate : pool.candidates) {
        const double quality =
            (candidate.synesthesia.overall * 0.75) +
            (candidate.critique.score * 0.25);
        const double originality_multiplier = 1.0 - (candidate.originality.release_risk * 0.45);
        candidate.ranking_score = candidate.originality.release_blocked
            ? 0.0
            : std::clamp(quality * originality_multiplier, 0.0, 1.0);
    }

    std::stable_sort(pool.candidates.begin(), pool.candidates.end(),
        [](const CandidateRecord& a, const CandidateRecord& b) {
            if (a.originality.release_blocked != b.originality.release_blocked) {
                return !a.originality.release_blocked;
            }
            if (a.ranking_score == b.ranking_score) {
                if (a.originality.release_risk == b.originality.release_risk) {
                    return a.candidate_id < b.candidate_id;
                }
                return a.originality.release_risk < b.originality.release_risk;
            }
            return a.ranking_score > b.ranking_score;
        });

    pool.winner_index = pool.candidates.size();
    for (std::size_t i = 0; i < pool.candidates.size(); ++i) {
        if (!pool.candidates[i].originality.release_blocked) {
            pool.winner_index = i;
            break;
        }
    }
}

} // namespace xenon
