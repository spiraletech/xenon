#include "xenon/cortex.hpp"
#include "xenon/engine.hpp"
#include "xenon/organic.hpp"
#include "xenon/producer.hpp"
#include "xenon/spectrum_analyzer.hpp"

#include <filesystem>
#include <iostream>
#include <vector>

int main() {
    std::cout << "XENON v" << XENON_VERSION << "\n";
    std::cout << "Hybrid AI music engine\n\n";

    xenon::Organic organic;
    organic.set_project("xenon_boot");
    organic.remember_preference("sparse drums");
    organic.remember_preference("negative space");

    std::vector<float> probe(1024, 0.0f);
    xenon::SpectrumAnalyzer analyzer(64);
    const auto spectrum = analyzer.analyze(probe, 48000.0);

    xenon::music::MusicFrameV1 frame;
    frame.track_id = "xenon_boot";
    frame.bpm = 86.0;
    frame.estimated_key = "F# minor";
    frame.warmth = 0.76;
    frame.brightness = spectrum.brightness;
    frame.spectral_density = spectrum.spectral_density;
    frame.transient_density = spectrum.transient_density;

    xenon::CortexContext context;
    context.spectrum = spectrum;
    context.preferences = organic.preferences();
    context.revision_notes = organic.revision_notes();

    xenon::Cortex cortex;
    auto intent = cortex.interpret("make me a sparse grimy beat with space to rap", frame, context);
    intent.duration_seconds = 8.0;
    intent.seed = 0x58454e4f;

    xenon::Producer producer;
    const auto plan = producer.compile(intent);

    try {
        xenon::Engine engine;
        const auto artifact = engine.render(intent, std::filesystem::path{"renders"});
        std::cout << "BPM: " << plan.bpm << "\n";
        std::cout << "Sections: " << plan.sections.size() << "\n";
        std::cout << "Spectrum bars: " << spectrum.bars.size() << "\n";
        std::cout << "Rendered: " << artifact.audio_path.string() << "\n";
        std::cout << "Revision: " << artifact.revision << "\n";
        std::cout << "Renderer: " << artifact.renderer << "\n";
        std::cout << "\nXENON hybrid core online.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "XENON fatal: " << error.what() << "\n";
        return 1;
    }
}
