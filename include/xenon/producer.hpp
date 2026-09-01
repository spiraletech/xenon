#pragma once

#include "xenon/music_trinity.hpp"

#include <string>
#include <vector>

namespace xenon {

struct ProductionSection {
    std::string name;
    int bars{0};
    double energy{0.5};
};

struct ProductionPlan {
    double bpm{0.0};
    std::string key;
    double drum_density{0.5};
    double bass_weight{0.5};
    double vocal_space{0.5};
    double texture_grit{0.5};
    double transient_density{0.5};
    std::vector<ProductionSection> sections;
};

class Producer {
public:
    [[nodiscard]] ProductionPlan compile(const music::ProductionIntentV1& intent) const;
};

} // namespace xenon
