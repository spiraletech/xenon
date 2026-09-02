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
    const auto cycle = loop.run_revision_cycle(request, output, "make the hats less crowded and keep bass");

    if (cycle.original.generation.artifact.audio_path.empty() ||
        !fs::exists(cycle.original.generation.artifact.audio_path)) {
        std::cerr << "L21 failed to generate the original artifact\n";
        return 1;
    }

    if (cycle.revision.generation.artifact.audio_path.empty() ||
        !fs::exists(cycle.revision.generation.artifact.audio_path)) {
        std::cerr << "L21 failed to render a revision artifact\n";
        return 2;
    }

    if (cycle.original.critique.profile.analyzed_frames == 0 ||
        cycle.revision.critique.profile.analyzed_frames == 0) {
        std::cerr << "L21 failed to hear both generations\n";
        return 3;
    }

    if (cycle.original.critique.synesthesia.overall < 0.0 ||
        cycle.original.critique.synesthesia.overall > 1.0 ||
        cycle.revision.critique.synesthesia.overall < 0.0 ||
        cycle.revision.critique.synesthesia.overall > 1.0) {
        std::cerr << "L21 synesthesia score escaped normalized range\n";
        return 4;
    }

    if (cycle.original.critique.observations.empty() ||
        cycle.original.critique.revision_hint.empty()) {
        std::cerr << "L21 CortexCritic produced no revision guidance\n";
        return 5;
    }

    if (cycle.revision_request.prompt.find("make the hats less crowded and keep bass") == std::string::npos) {
        std::cerr << "L21 failed to compile user feedback\n";
        return 6;
    }

    if (!xenon::has_control_component(cycle.revision_request.control.locks, xenon::ControlComponent::Bass) ||
        !xenon::has_control_component(cycle.revision_request.control.locks, xenon::ControlComponent::Melody) ||
        !xenon::has_control_component(cycle.revision_request.control.locks, xenon::ControlComponent::Arrangement)) {
        std::cerr << "L21 failed to compile preservation locks\n";
        return 7;
    }

    if (cycle.revision.generation.dna.parent_fingerprint != cycle.original.generation.dna.fingerprint) {
        std::cerr << "L21 revision lost EtherDNA lineage\n";
        return 8;
    }

    if (loop.memory().revision_notes().size() < 3) {
        std::cerr << "L21 Organic feedback memory did not record the cycle\n";
        return 9;
    }

    // NativePreview has no reference-conditioned variation capability, so this
    // test also proves L21's capability-aware fallback keeps the loop executable.
    if (cycle.used_controlled_revision) {
        std::cerr << "L21 unexpectedly reported controlled revision on NativePreview\n";
        return 10;
    }

    fs::remove_all(output);
    return 0;
}
