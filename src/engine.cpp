#include "xenon/engine.hpp"
#include "xenon/wav_renderer.hpp"

#include <filesystem>
#include <stdexcept>

namespace xenon {

Engine::Engine() = default;

music::RenderArtifactV1 Engine::render(
    const music::ProductionIntentV1& intent,
    const std::filesystem::path& output_directory) {

    if (intent.schema_version != music::protocol_version_v1) {
        throw std::invalid_argument("unsupported ProductionIntent schema version");
    }
    if (intent.request_id.empty()) {
        throw std::invalid_argument("request_id must not be empty");
    }
    if (intent.project_id.empty()) {
        throw std::invalid_argument("project_id must not be empty");
    }

    if (state_.project_id != intent.project_id) {
        state_ = {};
        state_.project_id = intent.project_id;
    }

    ++state_.revision;

    const auto filename = intent.project_id + "_r" + std::to_string(state_.revision) + ".wav";
    const auto output_path = output_directory / filename;

    WavRenderer renderer;
    renderer.render_preview(intent, output_path);

    music::RenderArtifactV1 artifact;
    artifact.request_id = intent.request_id;
    artifact.project_id = intent.project_id;
    artifact.revision = state_.revision;
    artifact.audio_path = output_path;
    artifact.renderer = "xenon.native.preview.v1";
    artifact.resolved_seed = intent.seed == 0 ? 0x58454e4full : intent.seed;

    state_.last_intent = intent;
    state_.last_artifact = artifact;
    return artifact;
}

const ProjectState& Engine::state() const noexcept {
    return state_;
}

} // namespace xenon
