#include "xenon/candidate_swarm_engine.hpp"

#include <stdexcept>
#include <utility>
#include <vector>

namespace xenon {

CandidateSwarmEngine::CandidateSwarmEngine(GenerationPipeline pipeline)
    : pipeline_(std::move(pipeline)) {}

CandidatePool CandidateSwarmEngine::generate_ranked(
    const GenerationRequest& request,
    const std::filesystem::path& output_directory,
    const std::string& parent_fingerprint) {

    auto batch = pipeline_.generate_candidates(request, output_directory, parent_fingerprint);
    CandidatePool pool;
    pool.failures.reserve(batch.failures.size());

    for (auto& failure : batch.failures) {
        pool.failures.push_back(CandidateFailure{
            failure.route,
            "generation",
            std::move(failure.error)
        });
    }

    std::vector<std::pair<std::string, AudioFingerprint>> prior_fingerprints;
    std::size_t ordinal = 0;
    for (auto& generation : batch.successes) {
        try {
            const auto analysis = analyzer_.analyzeFile(generation.artifact.audio_path);
            SynesthesiaEngine perceptual_engine;
            const auto perceptual_state = perceptual_engine.perceive(analysis);
            const auto synesthesia = perceptual_engine.score(perceptual_state);
            const auto critique = critic_.critique(analysis, synesthesia);
            const std::string candidate_id = generation.route.provider_name + "#" + std::to_string(++ordinal);

            auto originality = originality_.assess(
                candidate_id,
                generation.route.provider_name,
                generation.dna.fingerprint,
                generation.dna.parent_fingerprint,
                generation.artifact.resolved_seed,
                generation.artifact.audio_path,
                analysis,
                prior_fingerprints);

            prior_fingerprints.emplace_back(candidate_id, originality.fingerprint);

            CandidateRecord candidate;
            candidate.candidate_id = candidate_id;
            candidate.generation = std::move(generation);
            candidate.synesthesia = synesthesia;
            candidate.synesthesia_state = perceptual_state;
            candidate.critique = critique;
            candidate.originality = std::move(originality);
            pool.candidates.push_back(std::move(candidate));
        } catch (const std::exception& ex) {
            pool.failures.push_back(CandidateFailure{
                generation.route,
                "evaluation",
                ex.what()
            });
        } catch (...) {
            pool.failures.push_back(CandidateFailure{
                generation.route,
                "evaluation",
                "unknown candidate evaluation failure"
            });
        }
    }

    if (pool.candidates.empty()) {
        throw std::runtime_error("XENON candidate swarm produced no evaluable candidates");
    }

    ranker_.rank(pool);
    if (!pool.has_winner()) {
        throw std::runtime_error("XENON originality guard blocked every candidate from release selection");
    }
    return pool;
}

} // namespace xenon
