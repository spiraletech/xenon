#include "xenon/mastering_engine.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    try {
        std::vector<float> mix(4800 * 2);
        for (std::size_t i = 0; i < mix.size(); i += 2) {
            const double t = static_cast<double>(i / 2) / 48000.0;
            const float tone = static_cast<float>(0.11 * std::sin(6.283185307179586 * 220.0 * t));
            const float air = static_cast<float>(0.018 * std::sin(6.283185307179586 * 4200.0 * t));
            mix[i] = tone + air;
            mix[i + 1] = tone - air * 0.35f;
        }

        xenon::MasteringEngine engine{xenon::MasteringPreset::StreamingBalanced};
        const auto before = engine.analyze(mix);
        const auto plan = engine.plan(mix);
        const auto result = engine.master(mix);

        require(result.interleaved_stereo.size() == mix.size(), "L36 changed audio length");
        require(result.quantized_pcm.size() == mix.size(), "L36 export quantization size mismatch");
        require(result.passes >= 1 && result.passes <= 2, "L36 pass count invalid");
        require(plan.input_gain_db > 0.0, "L36 quiet input was not raised toward target");
        require(result.after.loudness_lufs_estimate > before.loudness_lufs_estimate, "L36 did not increase quiet mix loudness");
        require(result.after.oversampled_peak_dbfs <= engine.target().ceiling_dbfs + 0.051,
                "L36 exceeded configured ceiling");
        require(result.after.stereo_width <= engine.target().max_width + 0.021,
                "L36 exceeded configured stereo width");
        require(result.quality_gate_passed, "L36 balanced master failed quality gate");
        require(engine.target().export_profile.bit_depth == 24, "L36 streaming export depth wrong");

        auto loud_target = xenon::MasteringEngine::preset(xenon::MasteringPreset::StreamingLoud);
        require(loud_target.loudness_lufs > engine.target().loudness_lufs, "L36 loud preset target not louder");
        require(loud_target.export_profile.sample_rate == 48000, "L36 loud export sample rate wrong");

        std::vector<float> hot(400, 1.35f);
        xenon::MasteringEngine dynamic{xenon::MasteringPreset::Dynamic};
        const auto hot_result = dynamic.master(hot);
        require(hot_result.after.oversampled_peak_dbfs <= dynamic.target().ceiling_dbfs + 0.051,
                "L36 limiter did not contain hot input");

        bool bad_audio = false;
        try {
            std::vector<float> odd{0.1f, 0.2f, 0.3f};
            (void)engine.master(odd);
        } catch (const std::invalid_argument&) { bad_audio = true; }
        require(bad_audio, "L36 accepted malformed stereo buffer");

        bool bad_target = false;
        try {
            auto invalid = xenon::MasteringEngine::preset(xenon::MasteringPreset::StreamingBalanced);
            invalid.ceiling_dbfs = 0.0;
            xenon::MasteringEngine rejected{invalid};
            (void)rejected;
        } catch (const std::invalid_argument&) { bad_target = true; }
        require(bad_target, "L36 accepted unsafe ceiling target");

        std::cout << "L36 Mastering Engine smoke passed\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "L36 smoke failed: " << ex.what() << '\n';
        return 1;
    }
}
