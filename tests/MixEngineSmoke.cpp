#include "xenon/mix_engine.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::vector<float> tone(std::size_t frames, double freq, double amp, std::uint32_t sample_rate, double phase = 0.0) {
    std::vector<float> out(frames * 2);
    for (std::size_t i = 0; i < frames; ++i) {
        const double s = amp * std::sin(6.283185307179586 * freq * static_cast<double>(i) / sample_rate + phase);
        out[i * 2] = static_cast<float>(s);
        out[i * 2 + 1] = static_cast<float>(s);
    }
    return out;
}
}

int main() {
    try {
        constexpr std::uint32_t sr = 48000;
        constexpr std::size_t frames = 4096;

        xenon::StemMixInput drums;
        drums.stem_id = "drums";
        drums.role = xenon::StemRole::Drums;
        drums.analysis.rms = 0.34;
        drums.analysis.brightness = 0.62;
        drums.analysis.spectral_density = 0.72;
        drums.analysis.transient_density = 0.88;
        drums.interleaved_stereo = tone(frames, 1800.0, 0.35, sr);

        xenon::StemMixInput bass;
        bass.stem_id = "bass";
        bass.role = xenon::StemRole::Bass;
        bass.analysis.rms = 0.32;
        bass.analysis.brightness = 0.18;
        bass.analysis.spectral_density = 0.66;
        bass.analysis.transient_density = 0.24;
        bass.interleaved_stereo = tone(frames, 110.0, 0.32, sr, 0.4);

        xenon::StemMixInput melody;
        melody.stem_id = "melody";
        melody.role = xenon::StemRole::Melody;
        melody.analysis.rms = 0.28;
        melody.analysis.brightness = 0.59;
        melody.analysis.spectral_density = 0.70;
        melody.analysis.transient_density = 0.48;
        melody.interleaved_stereo = tone(frames, 1600.0, 0.28, sr, 0.8);

        xenon::StemMixInput vocals;
        vocals.stem_id = "vocals";
        vocals.role = xenon::StemRole::Vocals;
        vocals.analysis.rms = 0.26;
        vocals.analysis.brightness = 0.48;
        vocals.analysis.spectral_density = 0.50;
        vocals.analysis.transient_density = 0.34;
        vocals.locked = true;
        vocals.interleaved_stereo = tone(frames, 440.0, 0.20, sr, 1.1);

        std::vector<xenon::StemMixInput> stems{drums, bass, melody, vocals};

        xenon::MixEngine engine;
        xenon::MixReferenceTarget target;
        target.rms = 0.16;
        target.brightness = 0.42;
        target.spectral_density = 0.48;
        target.stereo_width = 0.55;

        const auto plan = engine.plan(stems, target);
        require(plan.stems.size() == stems.size(), "L35 lost stem decisions");

        const auto& vocal_decision = plan.stems[3];
        require(vocal_decision.locked, "L35 lost locked stem state");
        require(vocal_decision.gain_db == 0.0 && vocal_decision.pan == 0.0,
                "L35 altered locked stem controls");
        require(vocal_decision.compressor_ratio == 1.0,
                "L35 compressed locked stem");

        bool found_masking = false;
        bool found_pan = false;
        bool reference_changed_tilt = false;
        for (const auto& d : plan.stems) {
            if (d.masking_risk >= engine.policy().masking_threshold && !d.locked) found_masking = true;
            if (std::abs(d.pan) > 0.01) found_pan = true;
            if (std::abs(d.high_tilt_db) > 0.01) reference_changed_tilt = true;
        }
        require(found_masking, "L35 failed to detect masking");
        require(found_pan, "L35 did not create stereo placement");
        require(reference_changed_tilt, "L35 reference target did not affect EQ target");

        const auto result = engine.mix(stems, sr, target);
        require(!result.interleaved_stereo.empty(), "L35 produced no mix PCM");
        require(result.passes >= 1 && result.passes <= engine.policy().max_passes,
                "L35 invalid mix pass count");
        require(result.after.peak <= std::pow(10.0, -engine.policy().target_headroom_db / 20.0) + 1e-4,
                "L35 violated configured headroom");
        require(result.after.rms > 0.0, "L35 post-mix RMS invalid");
        require(result.after.stereo_width > 0.0, "L35 panning failed to create stereo width");
        require(!result.revision_hints.empty(), "L35 masking did not create targeted revision hint");

        bool changed_pcm = false;
        for (std::size_t i = 0; i < result.interleaved_stereo.size(); ++i) {
            const float raw = stems[0].interleaved_stereo[i] + stems[1].interleaved_stereo[i] +
                              stems[2].interleaved_stereo[i] + stems[3].interleaved_stereo[i];
            if (std::abs(result.interleaved_stereo[i] - raw) > 1e-5f) { changed_pcm = true; break; }
        }
        require(changed_pcm, "L35 mix controls did not alter audio samples");

        bool rejected = false;
        try {
            auto bad = stems;
            bad[0].interleaved_stereo.pop_back();
            (void)engine.mix(bad, sr, target);
        } catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "L35 accepted malformed stem buffer");

        bool bad_policy_rejected = false;
        try {
            xenon::MixPolicy p;
            p.max_passes = 0;
            xenon::MixEngine invalid{p};
            (void)invalid;
        } catch (const std::invalid_argument&) { bad_policy_rejected = true; }
        require(bad_policy_rejected, "L35 accepted invalid policy");

        std::cout << "L35 Mix Engine smoke passed\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "L35 smoke failed: " << ex.what() << '\n';
        return 1;
    }
}
