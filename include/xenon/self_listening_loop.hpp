#pragma once

#include "xenon/cortex_critic.hpp"
#include "xenon/generation_pipeline.hpp"
#include "xenon/media_analyzer.hpp"
#include "xenon/organic.hpp"
#include "xenon/revision_compiler.hpp"
#include "xenon/synesthesia_scorer.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace xenon {

struct ListeningProfile {
    double rms{0.0};
    double brightness{0.0};
    double spectral_density{0.0};
    double transient_density{0.0};
    std::size_t analyzed_frames{0};
};

struct ListeningCritique {
    ListeningProfile profile;
    SynesthesiaScore synesthesia;
    double score{0.0};
    std::vector<std::string> observations;
    std::string revision_hint;
};

struct SelfListeningResult {
    GenerationResult generation;
    ListeningCritique critique;
};

struct RevisionCycleResult {
    SelfListeningResult original;
    SelfListeningResult revision;
    GenerationRequest revision_request;
    bool used_controlled_revision{false};
};

class SelfListeningLoop {
public:
    explicit SelfListeningLoop(GenerationPipeline pipeline);

    [[nodiscard]] SelfListeningResult generate_and_listen(
        const GenerationRequest& request,
        const std::filesystem::path& output_directory,
        const std::string& parent_fingerprint = {});

    [[nodiscard]] GenerationRequest make_revision_request(
        const GenerationRequest& previous_request,
        const ListeningCritique& critique,
        std::string user_feedback = {}) const;

    [[nodiscard]] SelfListeningResult revise(
        const GenerationRequest& previous_request,
        const SelfListeningResult& previous_result,
        const std::filesystem::path& output_directory,
        std::string user_feedback = {});

    [[nodiscard]] RevisionCycleResult run_revision_cycle(
        const GenerationRequest& request,
        const std::filesystem::path& output_directory,
        std::string user_feedback = {});

    [[nodiscard]] const Organic& memory() const noexcept;

private:
    [[nodiscard]] ListeningProfile profile(const TrackAnalysis& analysis) const;
    [[nodiscard]] ListeningCritique critique(const TrackAnalysis& analysis) const;
    void remember(const SelfListeningResult& result, const std::string& note);

    GenerationPipeline pipeline_;
    MediaAnalyzer analyzer_;
    SynesthesiaScorer synesthesia_;
    CortexCritic critic_;
    RevisionCompiler revision_compiler_;
    Organic organic_;
};

} // namespace xenon
