#include "xenon/ether_composer.hpp"

#include <algorithm>
#include <cmath>

namespace xenon {

CompositionPlan EtherComposer::compose(const GenerationRequest& request) const {
    CompositionPlan plan;
    plan.bpm = request.bpm > 0.0 ? request.bpm : 86.0;
    plan.key = request.key.empty() ? "C minor" : request.key;
    plan.mutation_amount = std::clamp(request.mutation_amount, 0.0, 1.0);

    const double beats = request.duration_seconds * plan.bpm / 60.0;
    const int total_bars = std::max(4, static_cast<int>(std::round(beats / 4.0)));

    if (total_bars < 12) {
        plan.sections = {{"loop", total_bars, 0.62}};
        return plan;
    }

    const int intro = std::max(2, total_bars / 8);
    const int outro = std::max(2, total_bars / 8);
    const int middle = std::max(4, total_bars - intro - outro);
    const int first = middle / 2;
    const int second = middle - first;

    plan.sections = {
        {"intro", intro, 0.34},
        {"body_a", first, 0.68},
        {"body_b", second, 0.82},
        {"outro", outro, 0.42}
    };
    return plan;
}

GenerationRequest EtherComposer::compile(
    const GenerationRequest& request,
    const CompositionPlan& plan) const {

    GenerationRequest compiled = request;
    compiled.bpm = plan.bpm;
    compiled.key = plan.key;
    compiled.mutation_amount = plan.mutation_amount;
    return compiled;
}

} // namespace xenon
