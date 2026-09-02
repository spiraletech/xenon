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

GenerationResult GenerationPipeline::generate_evolved(
    const GenerationRequest& input,
    const std::filesystem::path& output_directory,
    const EtherDNARecord& parent,
    std::vector<MutationEvent> mutations) {

    const auto controlled = control_.compile(input);
    const auto plan = composer_.compose(controlled.request);
    auto compiled = composer_.compile(controlled.request, plan);

    const auto route = router_.route(compiled);
    auto artifact = router_.generate(compiled, output_directory);
    const auto dna = dna_.evolve(parent, compiled, plan, std::move(mutations));

    return GenerationResult{
        std::move(artifact),
        plan,
        dna,
        route
    };
}

GenerationBatch GenerationPipeline::generate_candidates(
    const GenerationRequest& input,
    const std::filesystem::path& output_directory,
    const std::string& parent_fingerprint) {

    const auto controlled = control_.compile(input);
    const auto plan = composer_.compose(controlled.request);
    auto compiled = composer_.compile(controlled.request, plan);
    const auto dna = dna_.capture(compiled, plan, parent_fingerprint);

    GenerationBatch batch;
    auto attempts = router_.generate_all(compiled, output_directory);
    batch.successes.reserve(attempts.size());
    batch.failures.reserve(attempts.size());

    for (auto& attempt : attempts) {
        if (attempt.success) {
            batch.successes.push_back(GenerationResult{
                std::move(attempt.artifact),
                plan,
                dna,
                attempt.route
            });
        } else {
            batch.failures.push_back(GenerationFailure{
                attempt.route,
                std::move(attempt.error)
            });
        }
    }

    return batch;
}

const ModelRouter& GenerationPipeline::router() const noexcept {
    return router_;
}

} // namespace xenon
