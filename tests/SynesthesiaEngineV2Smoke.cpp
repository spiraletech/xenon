#include "xenon/synesthesia_engine.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    try {
        xenon::TrackAnalysis analysis;
        analysis.sample_rate = 44100;
        for (int i = 0; i < 24; ++i) {
            xenon::SpectrumFrame frame;
            const double phase = static_cast<double>(i) / 23.0;
            frame.rms = 0.18 + 0.20 * std::sin(phase * 3.141592653589793);
            frame.brightness = 0.25 + 0.45 * phase;
            frame.spectral_density = 0.30 + 0.25 * std::sin(phase * 6.283185307179586);
            frame.transient_density = (i % 4 == 0) ? 0.75 : 0.28;
            analysis.frames.push_back(frame);
        }

        xenon::SynesthesiaEngine engine;
        const auto state = engine.perceive(analysis, 8);
        const auto score = engine.score(state);

        auto normalized = [](double value) { return value >= 0.0 && value <= 1.0; };
        require(normalized(state.hue), "hue not normalized");
        require(normalized(state.luminance), "luminance not normalized");
        require(normalized(state.warmth), "warmth not normalized");
        require(normalized(state.density), "density not normalized");
        require(normalized(state.motion), "motion not normalized");
        require(normalized(state.texture), "texture not normalized");
        require(normalized(state.tension), "tension not normalized");
        require(normalized(state.psychedelic_index), "psychedelic index not normalized");
        require(state.emotional_contour.size() == 8, "wrong emotional contour resolution");
        require(state.emotional_contour.front().position == 0.0, "contour must start at zero");
        require(state.emotional_contour.back().position == 1.0, "contour must end at one");
        require(state.emotional_contour.front().warmth > state.emotional_contour.back().warmth,
            "warming/cooling contour did not track brightness evolution");
        require(normalized(score.overall), "v2 score not normalized");

        bool rejected_empty = false;
        try { xenon::TrackAnalysis empty; (void)engine.perceive(empty); }
        catch (const std::runtime_error&) { rejected_empty = true; }
        require(rejected_empty, "empty analysis must be rejected");

        bool rejected_zero_contour = false;
        try { (void)engine.perceive(analysis, 0); }
        catch (const std::invalid_argument&) { rejected_zero_contour = true; }
        require(rejected_zero_contour, "zero contour resolution must be rejected");

        std::cout << "L28 Synesthesia Engine v2 smoke passed\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "L28 smoke failed: " << ex.what() << '\n';
        return 1;
    }
}
