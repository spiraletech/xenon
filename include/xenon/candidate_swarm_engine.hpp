#pragma once

#include "xenon/candidate_pool.hpp"
#include "xenon/candidate_ranker.hpp"
#include "xenon/generation_pipeline.hpp"
#include "xenon/media_analyzer.hpp"
#include "xenon/originality_guard.hpp"
#include "xenon/synesthesia_scorer.hpp"
#include "xenon/cortex_critic.hpp"

#include <filesystem>
#include <string>

namespace xenon {

class CandidateSwarmEngine {
public:
    explicit CandidateSwarmEngine(GenerationPipeline pipeline);

    [[nodiscard]] CandidatePool generate_ranked(
        const GenerationRequest& request,
        const std::filesystem::path& output_directory,
        const std::string& parent_fingerprint = {});

private:
    GenerationPipeline pipeline_;
    MediaAnalyzer analyzer_;
    SynesthesiaScorer synesthesia_;
    CortexCritic critic_;
    OriginalityGuard originality_;
    CandidateRanker ranker_;
};

} // namespace xenon
