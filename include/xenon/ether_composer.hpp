#pragma once

#include "xenon/generation_types.hpp"

#include <string>
#include <vector>

namespace xenon {

struct CompositionSection {
    std::string name;
    int bars{0};
    double energy{0.5};
};

struct CompositionPlan {
    double bpm{0.0};
    std::string key;
    double mutation_amount{0.35};
    std::vector<CompositionSection> sections;
};

class EtherComposer {
public:
    [[nodiscard]] CompositionPlan compose(const GenerationRequest& request) const;
    [[nodiscard]] GenerationRequest compile(
        const GenerationRequest& request,
        const CompositionPlan& plan) const;
};

} // namespace xenon
