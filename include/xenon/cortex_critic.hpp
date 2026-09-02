#pragma once

#include "xenon/synesthesia_scorer.hpp"

#include <string>
#include <vector>

namespace xenon {

struct CortexCritique {
    double score{0.0};
    std::vector<std::string> observations;
    std::string revision_hint;
};

class CortexCritic {
public:
    [[nodiscard]] CortexCritique critique(
        const TrackAnalysis& analysis,
        const SynesthesiaScore& synesthesia) const;
};

} // namespace xenon
