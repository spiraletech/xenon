#include "xenon/backend_policy.hpp"
#include "xenon/candidate_swarm_engine.hpp"
#include "xenon/engine.hpp"
#include "xenon/generation_pipeline.hpp"
#include "xenon/model_router.hpp"
#include "xenon/music_trinity.hpp"

#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

class SwarmPreviewBackend final : public xenon::IModelBackend {
public:
    SwarmPreviewBackend(std::string name, std::uint64_t seed_bias, bool fail = false)
        : name_(std::move(name)), seed_bias_(seed_bias), fail_(fail) {}

    std::string_view name() const noexcept override { return name_; }

    xenon::ProviderCapabilities capabilities() const noexcept override {
        return xenon::ProviderCapability::TextToInstrumental |
            xenon::ProviderCapability::QualityRole |
            xenon::ProviderCapability::LocalRuntime;
    }

    xenon::RuntimeType runtime_type() const noexcept override {
        return xenon::RuntimeType::Local;
    }

    xenon::GenerationArtifact generate(
        const xenon::GenerationRequest& request,
        const std::filesystem::path& output_directory) override {

        if (fail_) throw std::runtime_error("intentional L22 backend failure");

        xenon::music::ProductionIntentV1 intent;
        intent.request_id = "l22-swarm";
        intent.project_id = name_;
        intent.prompt = request.prompt;
        intent.bpm = request.bpm;
        intent.key = request.key;
        intent.duration_seconds = request.duration_seconds;
        intent.seed = request.seed + seed_bias_;
        intent.mutation_amount = request.mutation_amount;

        const auto rendered = engine_.render(intent, output_directory);
        xenon::GenerationArtifact artifact;
        artifact.audio_path = rendered.audio_path;
        artifact.metadata_path = rendered.metadata_path;
        artifact.backend_name = name_;
        artifact.resolved_seed = rendered.resolved_seed;
        return artifact;
    }

private:
    std::string name_;
    std::uint64_t seed_bias_{0};
    bool fail_{false};
    xenon::Engine engine_;
};

} // namespace

int main() {
    namespace fs = std::filesystem;

    xenon::ModelRouter router;
    router.add_provider(std::make_unique<SwarmPreviewBackend>("swarm-a", 0), 100);
    router.add_provider(std::make_unique<SwarmPreviewBackend>("swarm-b", 777), 90);
    router.add_provider(std::make_unique<SwarmPreviewBackend>("swarm-broken", 0, true), 80);
    router.set_policy(xenon::BackendPolicy{{xenon::RuntimePreference::LocalOnly, 100, 100}});

    xenon::CandidateSwarmEngine swarm{xenon::GenerationPipeline{std::move(router)}};

    xenon::GenerationRequest request;
    request.prompt = "L22 candidate swarm dark ethereal instrumental";
    request.mode = xenon::GenerationMode::TextToInstrumental;
    request.render_intent = xenon::RenderIntent::Quality;
    request.duration_seconds = 2.0;
    request.seed = 220022;
    request.bpm = 96.0;
    request.key = "F minor";

    const fs::path output = fs::temp_directory_path() / "xenon_l22_swarm_smoke";
    fs::remove_all(output);

    const auto pool = swarm.generate_ranked(request, output);

    if (pool.candidates.size() != 2) {
        std::cerr << "L22 did not retain both successful candidates\n";
        return 1;
    }
    if (pool.failures.size() != 1 || pool.failures.front().stage != "generation") {
        std::cerr << "L22 did not isolate the failed backend\n";
        return 2;
    }
    if (!pool.has_winner()) {
        std::cerr << "L22 did not select a winner\n";
        return 3;
    }
    if (pool.candidates[0].ranking_score < pool.candidates[1].ranking_score) {
        std::cerr << "L22 candidates are not ranked descending\n";
        return 4;
    }
    if (pool.candidates[0].candidate_id == pool.candidates[1].candidate_id) {
        std::cerr << "L22 candidate identities are not unique\n";
        return 5;
    }
    if (!fs::exists(pool.candidates[0].generation.artifact.audio_path) ||
        !fs::exists(pool.candidates[1].generation.artifact.audio_path)) {
        std::cerr << "L22 candidate artifacts do not exist\n";
        return 6;
    }
    if (pool.candidates[0].synesthesia.overall < 0.0 || pool.candidates[0].synesthesia.overall > 1.0 ||
        pool.candidates[1].synesthesia.overall < 0.0 || pool.candidates[1].synesthesia.overall > 1.0) {
        std::cerr << "L22 candidate hearing scores escaped normalized range\n";
        return 7;
    }
    if (pool.candidates[0].generation.dna.fingerprint.empty() ||
        pool.candidates[0].generation.dna.fingerprint != pool.candidates[1].generation.dna.fingerprint) {
        std::cerr << "L22 candidates lost shared composition DNA\n";
        return 8;
    }

    fs::remove_all(output);
    return 0;
}
