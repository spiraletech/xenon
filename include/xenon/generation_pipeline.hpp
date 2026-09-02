#pragma once

#include "xenon/composer_agent.hpp"
#include "xenon/ether_control.hpp"
#include "xenon/ether_composer.hpp"
#include "xenon/ether_dna.hpp"
#include "xenon/model_router.hpp"
#include "xenon/musician_agents.hpp"
#include "xenon/organic_music_memory.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace xenon {

struct GenerationResult {
    GenerationArtifact artifact;
    CompositionPlan plan;
    EtherDNARecord dna;
    RouteDecision route;
    EnsemblePlan ensemble;
};

struct GenerationFailure {
    RouteDecision route;
    std::string error;
};

struct GenerationBatch {
    std::vector<GenerationResult> successes;
    std::vector<GenerationFailure> failures;
};

class GenerationPipeline {
public:
    explicit GenerationPipeline(ModelRouter router);

    void set_music_memory(OrganicMusicMemory memory);
    void clear_music_memory() noexcept;
    [[nodiscard]] const OrganicMusicMemory* music_memory() const noexcept;

    void set_composer_perceptual_target(SynesthesiaState state);
    void clear_composer_perceptual_target() noexcept;
    [[nodiscard]] const SynesthesiaState* composer_perceptual_target() const noexcept;

    [[nodiscard]] GenerationResult generate(
        const GenerationRequest& request,
        const std::filesystem::path& output_directory,
        const std::string& parent_fingerprint = {});

    [[nodiscard]] GenerationResult generate_evolved(
        const GenerationRequest& request,
        const std::filesystem::path& output_directory,
        const EtherDNARecord& parent,
        std::vector<MutationEvent> mutations = {});

    [[nodiscard]] GenerationBatch generate_candidates(
        const GenerationRequest& request,
        const std::filesystem::path& output_directory,
        const std::string& parent_fingerprint = {});

    [[nodiscard]] const ModelRouter& router() const noexcept;

private:
    [[nodiscard]] GenerationRequest recall(GenerationRequest request) const;
    [[nodiscard]] ComposerAgentContext composer_context() const;
    [[nodiscard]] EnsemblePlan conduct(
        const GenerationRequest& request,
        const ComposerAgentPlan& agent_plan,
        const CompositionPlan& plan,
        const EtherDNARecord* parent_dna,
        const std::string& parent_fingerprint) const;

    EtherControl control_;
    EtherComposer composer_;
    ComposerAgent composer_agent_;
    MusicianEnsemble musicians_;
    EtherDNA dna_;
    ModelRouter router_;
    std::optional<OrganicMusicMemory> memory_;
    std::optional<SynesthesiaState> composer_target_;
};

} // namespace xenon
