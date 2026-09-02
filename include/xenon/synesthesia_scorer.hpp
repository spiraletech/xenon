#pragma once

#include "xenon/media_analyzer.hpp"

namespace xenon {

struct SynesthesiaScore {
    double overall{0.0};
    double energy{0.0};
    double brightness_balance{0.0};
    double density_balance{0.0};
    double rhythmic_balance{0.0};
    double warmth{0.0};
    double motion{0.0};
};

class SynesthesiaScorer {
public:
    [[nodiscard]] SynesthesiaScore score(const TrackAnalysis& analysis) const;
};

} // namespace xenon
