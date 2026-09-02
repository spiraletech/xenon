#pragma once

#include "xenon/ether_composer.hpp"
#include "xenon/organic_music_memory.hpp"
#include "xenon/synesthesia_engine.hpp"

#include <optional>
#include <string>
#include <vector>

namespace xenon {

struct ComposerSection {
    std::string name;
    int bars{0};
    double energy{0.5};
    double tension{0.5};
    double negative_space{0.5};
    double harmonic_pacing{0.5};
    std::string motif_id;
};

struct ComposerAgentPlan {
    double bpm{0.0};
    std::string key;
    double mutation_amount{0.35};
    std::vector<ComposerSection> sections;
    std::vector<std::string> motif_recurrence;
    double global_negative_space{0.5};
    double global_harmonic_pacing{0.5};
};

struct ComposerAgentContext {
    const OrganicMusicMemory* memory{nullptr};
    std::optional<SynesthesiaState> perceptual_target;
};

class ComposerAgent {
public:
    [[nodiscard]] ComposerAgentPlan plan(
        const GenerationRequest& request,
        const ComposerAgentContext& context = {}) const;

    void validate(const ComposerAgentPlan& plan) const;

    [[nodiscard]] CompositionPlan to_composition_plan(const ComposerAgentPlan& plan) const;

    [[nodiscard]] GenerationRequest compile(
        const GenerationRequest& request,
        const ComposerAgentPlan& plan) const;
};

} // namespace xenon
