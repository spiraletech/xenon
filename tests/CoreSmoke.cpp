#include "xenon/engine.hpp"

#include <cassert>
#include <filesystem>

int main() {
    const auto output_dir = std::filesystem::temp_directory_path() / "xenon_core_smoke";
    std::filesystem::remove_all(output_dir);

    xenon::music::ProductionIntentV1 intent;
    intent.request_id = "smoke-1";
    intent.project_id = "smoke_project";
    intent.prompt = "sparse dusty rap beat";
    intent.bpm = 86.0;
    intent.duration_seconds = 1.0;
    intent.seed = 42;
    intent.drum_density = 0.30;
    intent.bass_weight = 0.55;
    intent.vocal_space = 0.85;
    intent.texture_grit = 0.40;

    xenon::Engine engine;
    const auto first = engine.render(intent, output_dir);

    assert(first.schema_version == xenon::music::protocol_version_v1);
    assert(first.revision == 1);
    assert(first.project_id == intent.project_id);
    assert(std::filesystem::exists(first.audio_path));
    assert(std::filesystem::file_size(first.audio_path) > 44);

    intent.request_id = "smoke-2";
    intent.drum_density = 0.15;
    intent.keep_bass = true;
    intent.keep_harmony = true;
    intent.keep_arrangement = true;

    const auto second = engine.render(intent, output_dir);
    assert(second.revision == 2);
    assert(std::filesystem::exists(second.audio_path));
    assert(engine.state().last_intent.has_value());
    assert(engine.state().last_artifact.has_value());

    std::filesystem::remove_all(output_dir);
    return 0;
}
