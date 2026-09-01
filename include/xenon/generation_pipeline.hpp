#pragma once

#include "xenon/ether_control.hpp"
#include "xenon/ether_composer.hpp"
#include "xenon/ether_dna.hpp"
#include "xenon/model_router.hpp"

#include <filesystem>
#include <string>

namespace xenon {

struct GenerationResult {
    GenerationArtifact artifact;
    CompositionPlan plan;
    EtherDNARecord dna;
    RouteDecision route;
};

class GenerationPipeline {
public:
    explicit GenerationPipeline(ModelRouter router);

    [[nodiscard]] GenerationResult generate(
        const GenerationRequest& request,
        const std::filesystem::path& output_directory,
        const std::string& parent_fingerprint = {});

    [[nodiscard]] const ModelRouter& router() const noexcept;

private:
    EtherControl control_;
    EtherComposer composer_;
    EtherDNA dna_;
    ModelRouter router_;
};

} // namespace xenon
