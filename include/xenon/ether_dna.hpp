#pragma once

#include "xenon/ether_composer.hpp"
#include "xenon/generation_types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace xenon {

struct RhythmGenome {
    double tempo_bpm{0.0};
    double density{0.5};
    double transient_bias{0.5};
    double syncopation{0.0};
};

struct HarmonyGenome {
    std::string key;
    std::string chord_progression;
    double tension{0.5};
};

struct TimbreGenome {
    double brightness{0.5};
    double warmth{0.5};
    double grit{0.5};
    double stereo_width{0.5};
};

struct TextureGenome {
    double density{0.5};
    double noise{0.0};
    double motion{0.0};
};

struct ArrangementGene {
    std::string section;
    int bars{0};
    double energy{0.5};
};

struct ComponentAncestor {
    std::string component;
    std::string source_fingerprint;
    bool preserved{false};
};

struct MutationEvent {
    std::string component;
    double amount{0.0};
    std::string note;
};

struct EtherDNARecord {
    std::uint32_t schema_version{2};
    std::string fingerprint;
    std::string parent_fingerprint;
    std::vector<std::string> child_fingerprints;
    std::uint64_t seed{0};
    double bpm{0.0};
    std::string key;
    double mutation_amount{0.35};
    ControlComponents locks{0};

    RhythmGenome rhythm{};
    HarmonyGenome harmony{};
    TimbreGenome timbre{};
    TextureGenome texture{};
    std::vector<ArrangementGene> arrangement;
    std::vector<ComponentAncestor> component_ancestry;
    std::vector<MutationEvent> mutations;
};

class EtherDNA {
public:
    [[nodiscard]] EtherDNARecord capture(
        const GenerationRequest& request,
        const CompositionPlan& plan,
        const std::string& parent_fingerprint = {}) const;

    [[nodiscard]] EtherDNARecord evolve(
        const EtherDNARecord& parent,
        const GenerationRequest& request,
        const CompositionPlan& plan,
        std::vector<MutationEvent> mutations = {}) const;

    void register_child(EtherDNARecord& parent, const EtherDNARecord& child) const;

    [[nodiscard]] std::string serialize(const EtherDNARecord& record) const;
};

} // namespace xenon
