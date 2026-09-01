#include "xenon/producer.hpp"

#include <algorithm>

namespace xenon {

ProductionPlan Producer::compile(const music::ProductionIntentV1& intent) const {
    ProductionPlan plan;
    plan.bpm = intent.bpm > 0.0 ? intent.bpm : 86.0;
    plan.key = intent.key;
    plan.drum_density = std::clamp(intent.drum_density, 0.0, 1.0);
    plan.bass_weight = std::clamp(intent.bass_weight, 0.0, 1.0);
    plan.vocal_space = std::clamp(intent.vocal_space, 0.0, 1.0);
    plan.texture_grit = std::clamp(intent.texture_grit, 0.0, 1.0);
    plan.transient_density = std::clamp(intent.transient_density, 0.0, 1.0);

    if (intent.arrangement.empty()) {
        plan.sections = {{"intro", 4, 0.35}, {"verse", 16, 0.55}, {"hook", 8, 0.80}, {"verse", 16, 0.60}, {"outro", 4, 0.30}};
    } else {
        for (const auto& name : intent.arrangement) {
            int bars = 8;
            double energy = 0.55;
            if (name == "intro" || name == "outro") { bars = 4; energy = 0.30; }
            else if (name == "verse") { bars = 16; energy = 0.55; }
            else if (name == "hook" || name == "chorus") { bars = 8; energy = 0.82; }
            plan.sections.push_back({name, bars, energy});
        }
    }
    return plan;
}

} // namespace xenon
