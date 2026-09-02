#include "xenon/backend_policy.hpp"
#include "xenon/backends/native_preview_backend.hpp"
#include "xenon/generation_pipeline.hpp"
#include "xenon/model_router.hpp"
#include "xenon/organic_music_memory.hpp"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>

int main() {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "xenon_l27_organic_memory";
    fs::remove_all(root);
    fs::create_directories(root);
    const fs::path store = root / "project.organic";

    xenon::OrganicMusicMemory memory;
    memory.set_project("l27-project");
    memory.remember_preference("sparse hats and negative space", 0.95);
    memory.remember_rejection("overly bright crowded mixes", 0.90);
    memory.set_numeric_preference("mutation_amount", 0.18, 1.0);
    memory.set_numeric_preference("drum_density", 0.38, 0.92);
    memory.set_numeric_preference("reference_strength", 0.88, 1.0);
    memory.remember_revision({"xdna2-child", "xdna2-parent", "hats revision kept the pocket", xenon::MemoryVerdict::Accepted, 0.91});
    memory.remember_revision({"xdna2-bad", "xdna2-parent", "too bright and crowded", xenon::MemoryVerdict::Rejected, 0.33});
    memory.protect_motif({"motif-a", "descending glass phrase", "xdna2-parent"});

    if (!memory.has_preference_containing("sparse hats") ||
        !memory.has_rejection_containing("bright")) {
        std::cerr << "L27 taste/rejection query failed\n";
        return 1;
    }

    memory.save(store);
    xenon::OrganicMusicMemory loaded;
    loaded.load(store);

    const auto& state = loaded.state();
    if (state.project_id != "l27-project" || state.preferences.size() != 1 ||
        state.rejections.size() != 1 || state.numeric_preferences.size() != 3 ||
        state.revisions.size() != 2 || state.protected_motifs.size() != 1) {
        std::cerr << "L27 persistent memory round-trip failed\n";
        return 2;
    }

    xenon::GenerationRequest request;
    request.prompt = "dark minimal instrumental";
    request.duration_seconds = 1.0;
    request.seed = 270027;
    request.bpm = 90.0;
    request.key = "C minor";
    request.mutation_amount = 0.70;
    request.control.reference_strength = 0.40;

    const auto recalled = loaded.apply_to(request);
    if (std::abs(recalled.mutation_amount - 0.18) > 1e-9 ||
        std::abs(recalled.control.reference_strength - 0.88) > 1e-9 ||
        recalled.prompt.find("sparse hats and negative space") == std::string::npos ||
        recalled.prompt.find("overly bright crowded mixes") == std::string::npos ||
        recalled.prompt.find("target drum_density=0.38") == std::string::npos ||
        recalled.prompt.find("protect motif motif-a") == std::string::npos ||
        recalled.prompt.find("revision accepted") == std::string::npos ||
        recalled.prompt.find("revision rejected") == std::string::npos) {
        std::cerr << "L27 recall did not compile project memory into production intent\n";
        return 3;
    }

    xenon::ModelRouter router;
    router.add_provider(std::make_unique<xenon::NativePreviewBackend>(), 100);
    router.set_policy(xenon::BackendPolicy{{xenon::RuntimePreference::LocalOnly, 100, 100}});
    xenon::GenerationPipeline pipeline{std::move(router)};
    pipeline.set_music_memory(loaded);

    if (!pipeline.music_memory() || pipeline.music_memory()->project_id() != "l27-project") {
        std::cerr << "L27 generation pipeline did not retain project memory\n";
        return 4;
    }

    const auto result = pipeline.generate(request, root / "render");
    if (std::abs(result.dna.mutation_amount - 0.18) > 1e-9 ||
        result.dna.fingerprint.empty() || result.artifact.audio_path.empty() ||
        !fs::exists(result.artifact.audio_path)) {
        std::cerr << "L27 project memory did not affect real generation/EtherDNA\n";
        return 5;
    }

    pipeline.clear_music_memory();
    if (pipeline.music_memory() != nullptr) {
        std::cerr << "L27 failed to detach project memory\n";
        return 6;
    }

    fs::remove_all(root);
    return 0;
}
