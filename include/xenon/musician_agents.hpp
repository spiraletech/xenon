#pragma once

#include "xenon/composer_agent.hpp"
#include "xenon/ether_dna.hpp"
#include "xenon/generation_types.hpp"

#include <string>
#include <vector>

namespace xenon {

enum class MusicianRole {
    Drummer,
    Bass,
    Melody,
    Harmony,
    Texture
};

struct SectionPerformanceIntent {
    std::string section;
    double activity{0.5};
    double density{0.5};
    double variation{0.5};
    double space{0.5};
};

struct MusicianIntent {
    MusicianRole role{MusicianRole::Texture};
    bool locked{false};
    double global_activity{0.5};
    double global_density{0.5};
    double variation{0.5};
    std::vector<SectionPerformanceIntent> sections;
    std::string instruction;
};

struct EnsemblePlan {
    std::vector<MusicianIntent> musicians;
    std::string coordination_summary;
};

struct MusicianAgentContext {
    const ComposerAgentPlan* composer_plan{nullptr};
    const EtherDNARecord* dna{nullptr};
    ControlComponents locks{0};
};

class DrummerAgent {
public:
    [[nodiscard]] MusicianIntent perform(const MusicianAgentContext& context) const;
};

class BassAgent {
public:
    [[nodiscard]] MusicianIntent perform(const MusicianAgentContext& context) const;
};

class MelodyAgent {
public:
    [[nodiscard]] MusicianIntent perform(const MusicianAgentContext& context) const;
};

class HarmonyAgent {
public:
    [[nodiscard]] MusicianIntent perform(const MusicianAgentContext& context) const;
};

class TextureAgent {
public:
    [[nodiscard]] MusicianIntent perform(const MusicianAgentContext& context) const;
};

class MusicianEnsemble {
public:
    [[nodiscard]] EnsemblePlan conduct(const MusicianAgentContext& context) const;
    [[nodiscard]] GenerationRequest compile(
        const GenerationRequest& request,
        const EnsemblePlan& ensemble) const;
    void validate(const EnsemblePlan& ensemble) const;

private:
    DrummerAgent drummer_;
    BassAgent bass_;
    MelodyAgent melody_;
    HarmonyAgent harmony_;
    TextureAgent texture_;
};

[[nodiscard]] const char* musician_role_name(MusicianRole role) noexcept;

} // namespace xenon
