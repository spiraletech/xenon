#include "xenon/engine.hpp"
#include "xenon/surgical_revision_engine.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

std::filesystem::path render_fixture(
    xenon::Engine& engine,
    const std::filesystem::path& output,
    const std::string& project,
    std::uint64_t seed) {
    xenon::music::ProductionIntentV1 intent;
    intent.request_id = project;
    intent.project_id = project;
    intent.prompt = project;
    intent.duration_seconds = 2.0;
    intent.seed = seed;
    return engine.render(intent, output).audio_path;
}

bool has_component(const std::vector<xenon::SurgicalComponent>& values, xenon::SurgicalComponent value) {
    for (const auto item : values) if (item == value) return true;
    return false;
}

} // namespace

int main() {
    namespace fs = std::filesystem;
    const fs::path output = fs::temp_directory_path() / "xenon_l25_surgical_smoke";
    fs::remove_all(output);
    fs::create_directories(output);

    xenon::Engine renderer;
    const auto mix = render_fixture(renderer, output, "l25-mix", 250001);
    const auto drums = render_fixture(renderer, output, "l25-drums", 250002);
    const auto bass = render_fixture(renderer, output, "l25-bass", 250003);
    const auto bass_v2 = render_fixture(renderer, output, "l25-bass-v2", 250004);
    const auto melody = render_fixture(renderer, output, "l25-melody", 250005);

    xenon::GenerationRequest base;
    base.prompt = "dark ethereal instrumental";
    base.mode = xenon::GenerationMode::TextToInstrumental;
    base.render_intent = xenon::RenderIntent::Quality;
    base.duration_seconds = 60.0;
    base.reference_audio = mix;
    base.seed = 250025;

    xenon::SurgicalRevisionEngine surgical;
    xenon::SurgicalRevisionRequest hats;
    hats.instruction = "make the hats less crowded";
    hats.target = xenon::SurgicalComponent::Hats;
    hats.region = {42.0, 51.0};
    hats.mask.mutable_components = {xenon::SurgicalComponent::Hats};
    hats.mask.preserved_components = {
        xenon::SurgicalComponent::Bass,
        xenon::SurgicalComponent::Melody,
        xenon::SurgicalComponent::Harmony,
        xenon::SurgicalComponent::Texture
    };
    hats.mask.preserve_arrangement = true;
    hats.source_audio = mix;
    hats.mutation_amount = 0.18;

    const auto plan = surgical.compile(base, hats);
    if (plan.generation_request.mode != xenon::GenerationMode::ReplaceSection ||
        plan.generation_request.render_intent != xenon::RenderIntent::Control ||
        plan.generation_request.control.edit_start_seconds != 42.0 ||
        plan.generation_request.control.edit_end_seconds != 51.0) {
        std::cerr << "L25 temporal local-regeneration compilation failed\n";
        return 1;
    }
    if (!xenon::has_control_component(plan.compiled_locks, xenon::ControlComponent::Bass) ||
        !xenon::has_control_component(plan.compiled_locks, xenon::ControlComponent::Melody) ||
        !xenon::has_control_component(plan.compiled_locks, xenon::ControlComponent::Harmony) ||
        !xenon::has_control_component(plan.compiled_locks, xenon::ControlComponent::Texture) ||
        !xenon::has_control_component(plan.compiled_locks, xenon::ControlComponent::Arrangement) ||
        xenon::has_control_component(plan.compiled_locks, xenon::ControlComponent::Drums)) {
        std::cerr << "L25 preservation mask compiled incorrectly\n";
        return 2;
    }
    if (plan.diff.target != xenon::SurgicalComponent::Hats ||
        !has_component(plan.diff.changed_components, xenon::SurgicalComponent::Hats) ||
        !plan.diff.arrangement_preserved || plan.uses_direct_stem_replacement) {
        std::cerr << "L25 revision diff lost sub-drum semantics\n";
        return 3;
    }

    bool rejected_subdrum_file = false;
    try {
        auto invalid = hats;
        invalid.replacement_audio = drums;
        (void)surgical.compile(base, invalid);
    } catch (const std::invalid_argument&) {
        rejected_subdrum_file = true;
    }
    if (!rejected_subdrum_file) {
        std::cerr << "L25 incorrectly widened hats-only replacement to whole drums stem\n";
        return 4;
    }

    xenon::StemEngine stem_engine;
    auto set = stem_engine.import_stems(
        "l25-stems", mix,
        {
            {xenon::StemRole::Drums, drums, "fixture"},
            {xenon::StemRole::Bass, bass, "fixture"},
            {xenon::StemRole::Melody, melody, "fixture"}
        },
        "dna-l25", "dna-l24");
    stem_engine.set_locked(set, xenon::StemRole::Melody, true);

    xenon::SurgicalRevisionRequest bass_edit;
    bass_edit.instruction = "replace only the bass stem";
    bass_edit.target = xenon::SurgicalComponent::Bass;
    bass_edit.region = {0.25, 1.75};
    bass_edit.mask.mutable_components = {xenon::SurgicalComponent::Bass};
    bass_edit.mask.preserved_components = {
        xenon::SurgicalComponent::Drums,
        xenon::SurgicalComponent::Melody
    };
    bass_edit.source_audio = mix;
    bass_edit.replacement_audio = bass_v2;

    auto short_base = base;
    short_base.duration_seconds = 2.0;
    const auto bass_plan = surgical.compile(short_base, bass_edit);
    if (!bass_plan.uses_direct_stem_replacement ||
        bass_plan.stem_replacement.target != xenon::StemRole::Bass) {
        std::cerr << "L25 direct stem replacement plan failed\n";
        return 5;
    }

    const auto reconstruction = surgical.apply_direct_replacement(stem_engine, set, bass_plan);
    if (reconstruction.replaced_role != xenon::StemRole::Bass ||
        reconstruction.ordered_stems.size() != 3 ||
        set.ether_dna_fingerprint != "dna-l25" ||
        set.parent_ether_dna_fingerprint != "dna-l24") {
        std::cerr << "L25 reconstruction or lineage preservation failed\n";
        return 6;
    }

    bool rejected_invalid_region = false;
    try {
        auto invalid = hats;
        invalid.region = {51.0, 42.0};
        (void)surgical.compile(base, invalid);
    } catch (const std::invalid_argument&) {
        rejected_invalid_region = true;
    }
    if (!rejected_invalid_region) {
        std::cerr << "L25 accepted an invalid temporal region\n";
        return 7;
    }

    fs::remove_all(output);
    return 0;
}
