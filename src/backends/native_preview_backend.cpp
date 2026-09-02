#include "xenon/backends/native_preview_backend.hpp"

#include "xenon/music_trinity.hpp"

#include <string>

namespace xenon {
namespace {

music::ProductionIntentV1 to_production_intent(const GenerationRequest& request) {
    music::ProductionIntentV1 intent;
    intent.request_id = "native-preview";
    intent.project_id = "xenon";
    intent.prompt = request.prompt;
    intent.bpm = request.bpm;
    intent.key = request.key;
    intent.duration_seconds = request.duration_seconds;
    intent.seed = request.seed;
    intent.mutation_amount = request.mutation_amount;
    intent.reference_audio = request.reference_audio;
    intent.chord_progression = request.control.chord_progression;

    intent.keep_drums = has_control_component(request.control.locks, ControlComponent::Drums);
    intent.keep_bass = has_control_component(request.control.locks, ControlComponent::Bass);
    intent.keep_melody = has_control_component(request.control.locks, ControlComponent::Melody);
    intent.keep_harmony = has_control_component(request.control.locks, ControlComponent::Harmony);
    intent.keep_texture = has_control_component(request.control.locks, ControlComponent::Texture);
    intent.keep_arrangement = has_control_component(request.control.locks, ControlComponent::Arrangement);

    return intent;
}

} // namespace

std::string_view NativePreviewBackend::name() const noexcept {
    return "xenon-native-preview";
}

ProviderCapabilities NativePreviewBackend::capabilities() const noexcept {
    return ProviderCapability::TextToInstrumental |
        ProviderCapability::DraftRole |
        ProviderCapability::QualityRole |
        ProviderCapability::LocalRuntime;
}

RuntimeType NativePreviewBackend::runtime_type() const noexcept {
    return RuntimeType::Local;
}

GenerationArtifact NativePreviewBackend::generate(
    const GenerationRequest& request,
    const std::filesystem::path& output_directory) {

    const auto rendered = engine_.render(to_production_intent(request), output_directory);

    GenerationArtifact artifact;
    artifact.audio_path = rendered.audio_path;
    artifact.metadata_path = rendered.metadata_path;
    artifact.backend_name = std::string{name()};
    artifact.resolved_seed = rendered.resolved_seed;
    return artifact;
}

} // namespace xenon
