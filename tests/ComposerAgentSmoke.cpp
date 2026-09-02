#include "xenon/backend_policy.hpp"
#include "xenon/backends/native_preview_backend.hpp"
#include "xenon/composer_agent.hpp"
#include "xenon/generation_pipeline.hpp"
#include "xenon/model_router.hpp"

#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    try {
        xenon::GenerationRequest request;
        request.prompt = "dark sparse instrumental with negative space";
        request.duration_seconds = 96.0;
        request.bpm = 90.0;
        request.key = "F minor";
        request.mutation_amount = 0.33;
        request.render_intent = xenon::RenderIntent::Quality;

        xenon::OrganicMusicMemory memory;
        memory.set_project("l30-smoke");
        memory.remember_preference("sparse hats and negative space", 1.0);
        memory.remember_rejection("crowded arrangements", 1.0);
        memory.set_numeric_preference("harmonic_pacing", 0.28, 1.0);

        xenon::SynesthesiaState target;
        target.energy = 0.78;
        target.tension = 0.66;
        target.density = 0.30;

        xenon::ComposerAgent agent;
        xenon::ComposerAgentContext context;
        context.memory = &memory;
        context.perceptual_target = target;
        const auto plan = agent.plan(request, context);

        require(plan.sections.size() >= 5, "L30 failed to create multi-section topology");
        require(plan.global_negative_space > 0.70, "L30 memory/perception did not bias negative space");
        require(plan.global_harmonic_pacing == 0.28, "L30 memory did not control harmonic pacing");
        require(!plan.motif_recurrence.empty(), "L30 motif recurrence missing");

        bool found_bridge = false;
        bool repeated_motif_a = false;
        int motif_a_count = 0;
        double max_energy = 0.0;
        double min_energy = 1.0;
        for (const auto& section : plan.sections) {
            if (section.name == "bridge") found_bridge = true;
            if (section.motif_id == "motif_a") ++motif_a_count;
            max_energy = std::max(max_energy, section.energy);
            min_energy = std::min(min_energy, section.energy);
            require(section.energy >= 0.0 && section.energy <= 1.0, "L30 section energy out of range");
            require(section.tension >= 0.0 && section.tension <= 1.0, "L30 section tension out of range");
        }
        repeated_motif_a = motif_a_count >= 3;
        require(found_bridge, "L30 long-form plan omitted bridge");
        require(repeated_motif_a, "L30 did not recur primary motif");
        require(max_energy - min_energy > 0.25, "L30 energy trajectory is too flat");

        const auto compiled = agent.compile(request, plan);
        require(compiled.prompt.find("COMPOSER AGENT form") != std::string::npos, "L30 compile omitted form metadata");
        require(compiled.prompt.find("motif recurrence") != std::string::npos, "L30 compile omitted motif recurrence");

        xenon::ComposerAgentPlan invalid = plan;
        invalid.sections.front().bars = 0;
        bool rejected_invalid = false;
        try { agent.validate(invalid); }
        catch (const std::invalid_argument&) { rejected_invalid = true; }
        require(rejected_invalid, "L30 validation accepted invalid section length");

        xenon::ModelRouter router;
        router.add_provider(std::make_unique<xenon::NativePreviewBackend>(), 100);
        router.set_policy(xenon::BackendPolicy{{xenon::RuntimePreference::LocalOnly, 100, 100}});
        xenon::GenerationPipeline pipeline{std::move(router)};
        pipeline.set_music_memory(memory);
        pipeline.set_composer_perceptual_target(target);

        xenon::GenerationRequest render_request = request;
        render_request.duration_seconds = 8.0;
        const auto output = std::filesystem::temp_directory_path() / "xenon_l30_smoke";
        const auto result = pipeline.generate(render_request, output);
        require(std::filesystem::exists(result.artifact.audio_path), "L30 pipeline failed to render through Composer Agent");
        require(!result.plan.sections.empty(), "L30 pipeline lost composition plan");
        require(result.dna.arrangement.size() == result.plan.sections.size(), "L30 EtherDNA lost arrangement topology");

        std::filesystem::remove_all(output);
        std::cout << "L30 Composer Agent smoke passed\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "L30 smoke failed: " << ex.what() << '\n';
        return 1;
    }
}
