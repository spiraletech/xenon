#include "xenon/backends/native_preview_backend.hpp"
#include "xenon/generation_pipeline.hpp"
#include "xenon/model_router.hpp"
#include "xenon/self_listening_loop.hpp"

#include <filesystem>
#include <iostream>
#include <memory>

int main() {
    namespace fs = std::filesystem;

    xenon::ModelRouter router;
    router.add_provider(std::make_unique<xenon::NativePreviewBackend>(), 100);
    router.set_backend_policy(xenon::BackendPolicy{xenon::RuntimePolicy::LocalOnly});

    xenon::SelfListeningLoop loop{xenon::GenerationPipeline{std::move(router)}};

    xenon::GenerationRequest request;
    request.prompt = "L21 self-listening smoke test instrumental";
    request.mode = xenon::GenerationMode::TextToInstrumental;
    request.render_intent = xenon::RenderIntent::Quality;
    request.duration_seconds = 2.0;
    request.seed = 210021;
    request.bpm = 92.0;
    request.key = "D minor";

    const fs::path output = fs::temp_directory_path() / "xenon_l21_smoke";
    const auto result = loop.generate_and_listen(request, output);

    if (result.generation.artifact.audio_path.empty() ||
        !fs::exists(result.generation.artifact.audio_path)) {
        std::cerr << "L21 failed to generate an artifact\n";
        return 1;
    }

    if (result.critique.profile.analyzed_frames == 0) {
        std::cerr << "L21 failed to hear its generated artifact\n";
        return 2;
    }

    if (result.critique.score < 0.0 || result.critique.score > 1.0) {
        std::cerr << "L21 critique score escaped normalized range\n";
        return 3;
    }

    if (result.critique.observations.empty() || result.critique.revision_hint.empty()) {
        std::cerr << "L21 critique did not produce revision guidance\n";
        return 4;
    }

    const auto revision = loop.make_revision_request(request, result.critique, "make the hats less crowded");
    if (revision.prompt.find("make the hats less crowded") == std::string::npos) {
        std::cerr << "L21 failed to compile user feedback into a revision request\n";
        return 5;
    }

    fs::remove_all(output);
    return 0;
}
