#pragma once

#include "xenon/generation_types.hpp"
#include "xenon/synesthesia_scorer.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace xenon {

enum class StemRole {
    Unknown,
    Drums,
    Bass,
    Vocals,
    Melody,
    Harmony,
    Texture
};

[[nodiscard]] const char* stem_role_name(StemRole role) noexcept;
[[nodiscard]] ControlComponent stem_role_control_component(StemRole role) noexcept;

struct StemAnalysis {
    double rms{0.0};
    double brightness{0.0};
    double spectral_density{0.0};
    double transient_density{0.0};
    SynesthesiaScore synesthesia{};
    StemRole inferred_role{StemRole::Unknown};
    double role_confidence{0.0};
};

struct StemArtifact {
    std::string stem_id;
    StemRole role{StemRole::Unknown};
    std::filesystem::path audio_path;
    StemAnalysis analysis{};
    bool locked{false};
    std::string source_backend;
};

struct StemSet {
    std::string stem_set_id;
    std::filesystem::path source_mix;
    std::string ether_dna_fingerprint;
    std::string parent_ether_dna_fingerprint;
    std::vector<StemArtifact> stems;
};

struct StemReplacementRequest {
    StemRole target{StemRole::Unknown};
    std::filesystem::path replacement_audio;
    std::string source_backend;
};

struct ReconstructionPlan {
    std::string source_stem_set_id;
    std::string output_stem_set_id;
    std::vector<std::filesystem::path> ordered_stems;
    std::vector<StemRole> roles;
    std::vector<StemRole> preserved_locked_roles;
    StemRole replaced_role{StemRole::Unknown};
};

} // namespace xenon
