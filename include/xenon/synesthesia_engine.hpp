#pragma once

#include "xenon/media_analyzer.hpp"
#include "xenon/synesthesia_scorer.hpp"

#include <cstddef>
#include <vector>

namespace xenon {

struct EmotionalContourPoint {
    double position{0.0};
    double energy{0.0};
    double tension{0.0};
    double warmth{0.0};
    double motion{0.0};
};

struct SynesthesiaState {
    double hue{0.0};
    double luminance{0.0};
    double warmth{0.0};
    double density{0.0};
    double motion{0.0};
    double texture{0.0};
    double tension{0.0};
    double psychedelic_index{0.0};
    double energy{0.0};
    double brightness{0.0};
    std::vector<EmotionalContourPoint> emotional_contour;
};

class SynesthesiaEngine {
public:
    [[nodiscard]] SynesthesiaState perceive(
        const TrackAnalysis& analysis,
        std::size_t contour_points = 12) const;

    [[nodiscard]] SynesthesiaScore score(const SynesthesiaState& state) const;
};

} // namespace xenon
