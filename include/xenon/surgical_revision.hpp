#pragma once

#include "xenon/generation_types.hpp"
#include "xenon/stem_types.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace xenon {

enum class SurgicalComponent : std::uint32_t {
    Unknown,
    Kick,
    Snare,
    Hats,
    Drums,
    Bass,
    Vocals,
    Melody,
    Harmony,
    Texture,
    Arrangement
};

[[nodiscard]] const char* surgical_component_name(SurgicalComponent component) noexcept;
[[nodiscard]] ControlComponent control_component_for(SurgicalComponent component) noexcept;
[[nodiscard]] StemRole stem_role_for(SurgicalComponent component) noexcept;

struct TemporalEditRegion {
    double start_seconds{0.0};
    double end_seconds{0.0};

    [[nodiscard]] bool valid() const noexcept {
        return start_seconds >= 0.0 && end_seconds > start_seconds;
    }
};

struct MutationMask {
    std::vector<SurgicalComponent> mutable_components;
    std::vector<SurgicalComponent> preserved_components;
    bool preserve_arrangement{true};
};

struct SurgicalRevisionRequest {
    std::string instruction;
    SurgicalComponent target{SurgicalComponent::Unknown};
    TemporalEditRegion region{};
    MutationMask mask{};
    std::filesystem::path source_audio;
    std::filesystem::path replacement_audio;
    double mutation_amount{0.20};
};

struct RevisionDiff {
    SurgicalComponent target{SurgicalComponent::Unknown};
    TemporalEditRegion region{};
    std::vector<SurgicalComponent> changed_components;
    std::vector<SurgicalComponent> preserved_components;
    bool arrangement_preserved{true};
    std::string summary;
};

struct SurgicalRevisionPlan {
    GenerationRequest generation_request;
    RevisionDiff diff;
    ControlComponents compiled_locks{0};
    bool uses_direct_stem_replacement{false};
    StemReplacementRequest stem_replacement{};
};

} // namespace xenon
