#include "xenon/generation_pipeline.hpp"

#include <utility>

namespace xenon {

GenerationPipeline::GenerationPipeline(ModelRouter router)
    : router_(std::move(router)) {}

GenerationResult GenerationPipeline::generate(
    const GenerationRequest& input,
    const std::filesystem::path& output_directory,
    const std::string& parent_fingerprint) {

    const auto controlled = control_.compile(input);
    const auto plan = composer_.compose(controlled.request);
    auto compiled = composer_.compile(controlled.request, plan);

    const auto route = router_.route(compiled);
    auto artifact = router_.generate(compiled, output_directory);
    const auto dna = dna_.capture(compiled, plan, parent_fingerprint);

    return GenerationResult{
        std::move(artifact),
        plan,
        dna,
        route
    };
}

const ModelRouter& GenerationPipeline::router() const noexcept {
    return router_;
}

} // namespace xenon
