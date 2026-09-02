#include "xenon/backend_policy.hpp"
#include "xenon/backends/native_preview_backend.hpp"
#include "xenon/composer_agent.hpp"
#include "xenon/generation_pipeline.hpp"
#include "xenon/model_router.hpp"
#include "xenon/musician_agents.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

const xenon::MusicianIntent& find_role(const xenon::EnsemblePlan& ensemble, xenon::MusicianRole role) {
    const auto it = std::find_if(ensemble.musicians.begin(), ensemble.musicians.end(),
        [&](const xenon::MusicianIntent& musician) { return musician.role == role; });
    if (it == ensemble.musicians.end()) throw std::runtime_error("L31 role missing from ensemble");
    return *it;
}

xenon::ModelRouter make_router() {
    xenon::ModelRouter router;
    router.add_provider(std::make_unique<xenon::NativePreviewBackend>(), 100);
    router.set_policy(xenon::BackendPolicy{{xenon::RuntimePreference::LocalOnly, 100, 100}});
    return router;
}
}

int main() {
    try {
        xenon::GenerationRequest request;
        request.prompt = "dark spacious instrumental with recurring glass motif";
        request.duration_seconds = 64.0;
        request.bpm = 92.0;
        request.key = "E minor";
        request.mutation_amount = 0.42;
        request.render_intent = xenon::RenderIntent::Quality;

        xenon::ComposerAgent composer;
        const auto composer_plan = composer.plan(request);
        const auto composition_plan = composer.to_composition_plan(composer_plan);

        xenon::EtherDNA dna_engine;
        const auto dna = dna_engine.capture(request, composition_plan);

        xenon::MusicianEnsemble ensemble_engine;
        xenon::MusicianAgentContext context;
        context.composer_plan = &composer_plan;
        context.dna = &dna;
        context.locks = xenon::control_component(xenon::ControlComponent::Bass);
        const auto ensemble = ensemble_engine.conduct(context);

        require(ensemble.musicians.size() == 5, "L31 did not instantiate five musician agents");
        require(!ensemble.coordination_summary.empty(), "L31 ensemble coordination summary missing");

        const auto& drummer = find_role(ensemble, xenon::MusicianRole::Drummer);
        const auto& bass = find_role(ensemble, xenon::MusicianRole::Bass);
        const auto& melody = find_role(ensemble, xenon::MusicianRole::Melody);
        const auto& harmony = find_role(ensemble, xenon::MusicianRole::Harmony);
        const auto& texture = find_role(ensemble, xenon::MusicianRole::Texture);

        require(!drummer.locked && bass.locked && !melody.locked && !harmony.locked && !texture.locked,
            "L31 component locks were not isolated to the correct musician");
        require(bass.variation == 0.0, "L31 locked bass agent attempted variation");
        require(!drummer.sections.empty(), "L31 drummer emitted no section performance intents");
        require(drummer.sections.size() == composer_plan.sections.size(), "L31 drummer lost composer topology");
        require(melody.sections.size() == composer_plan.sections.size(), "L31 melody lost composer topology");

        double drummer_min = 1.0;
        double drummer_max = 0.0;
        for (const auto& section : drummer.sections) {
            drummer_min = std::min(drummer_min, section.activity);
            drummer_max = std::max(drummer_max, section.activity);
        }
        require(drummer_max - drummer_min > 0.10, "L31 drummer did not respond to section energy trajectory");

        const auto compiled = ensemble_engine.compile(request, ensemble);
        require(compiled.prompt.find("MUSICIAN ENSEMBLE") != std::string::npos,
            "L31 compile omitted ensemble metadata");
        require(compiled.prompt.find("drummer") != std::string::npos &&
                compiled.prompt.find("bass") != std::string::npos &&
                compiled.prompt.find("melody") != std::string::npos &&
                compiled.prompt.find("harmony") != std::string::npos &&
                compiled.prompt.find("texture") != std::string::npos,
            "L31 compile omitted musician role instructions");

        xenon::EnsemblePlan invalid = ensemble;
        invalid.musicians.pop_back();
        bool rejected_invalid = false;
        try { ensemble_engine.validate(invalid); }
        catch (const std::invalid_argument&) { rejected_invalid = true; }
        require(rejected_invalid, "L31 validation accepted incomplete ensemble");

        xenon::GenerationPipeline pipeline{make_router()};
        const auto output = std::filesystem::temp_directory_path() / "xenon_l31_smoke";
        xenon::GenerationRequest render_request = request;
        render_request.duration_seconds = 4.0;
        const auto result = pipeline.generate(render_request, output / "parent");
        require(std::filesystem::exists(result.artifact.audio_path), "L31 pipeline failed to render ensemble generation");
        require(result.ensemble.musicians.size() == 5, "L31 generation result lost ensemble plan");
        require(result.dna.fingerprint.rfind("xdna2-", 0) == 0, "L31 generation lost shared EtherDNA");

        xenon::GenerationRequest child_request = render_request;
        child_request.prompt = "evolve the same piece with slightly more movement";
        child_request.mutation_amount = 0.30;
        const auto child = pipeline.generate_evolved(child_request, output / "child", result.dna);
        require(child.dna.parent_fingerprint == result.dna.fingerprint, "L31 evolved generation lost parent DNA");
        require(child.ensemble.musicians.size() == 5, "L31 evolved generation lost ensemble");

        const auto batch = pipeline.generate_candidates(render_request, output / "candidates");
        require(!batch.successes.empty(), "L31 candidate generation produced no successes");
        require(batch.successes.front().ensemble.musicians.size() == 5, "L31 candidate path lost musician ensemble");

        std::filesystem::remove_all(output);
        std::cout << "L31 Musician Agents smoke passed\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "L31 smoke failed: " << ex.what() << '\n';
        return 1;
    }
}
