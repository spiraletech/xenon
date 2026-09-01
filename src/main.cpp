#include "xenon/engine.hpp"

#include <filesystem>
#include <iostream>

int main() {
    std::cout << "XENON v" << XENON_VERSION << "\n";
    std::cout << "Music Trinity native engine\n\n";

    xenon::music::ProductionIntentV1 intent;
    intent.request_id = "boot-demo";
    intent.project_id = "xenon_boot";
    intent.prompt = "native foundation preview";
    intent.bpm = 86.0;
    intent.duration_seconds = 8.0;
    intent.seed = 0x58454e4f;
    intent.drum_density = 0.35;
    intent.bass_weight = 0.55;
    intent.vocal_space = 0.80;
    intent.texture_grit = 0.45;
    intent.transient_density = 0.35;

    try {
        xenon::Engine engine;
        const auto artifact = engine.render(intent, std::filesystem::path{"renders"});
        std::cout << "Rendered: " << artifact.audio_path.string() << "\n";
        std::cout << "Revision: " << artifact.revision << "\n";
        std::cout << "Renderer: " << artifact.renderer << "\n";
        std::cout << "\nXENON foundation online.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "XENON fatal: " << error.what() << "\n";
        return 1;
    }
}
