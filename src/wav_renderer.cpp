#include "xenon/wav_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace xenon {
namespace {

constexpr int sample_rate = 44100;
constexpr int channels = 1;
constexpr int bits_per_sample = 16;

void write_u16(std::ofstream& out, std::uint16_t value) {
    const char bytes[2] = {
        static_cast<char>(value & 0xff),
        static_cast<char>((value >> 8) & 0xff)
    };
    out.write(bytes, 2);
}

void write_u32(std::ofstream& out, std::uint32_t value) {
    const char bytes[4] = {
        static_cast<char>(value & 0xff),
        static_cast<char>((value >> 8) & 0xff),
        static_cast<char>((value >> 16) & 0xff),
        static_cast<char>((value >> 24) & 0xff)
    };
    out.write(bytes, 4);
}

double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

} // namespace

std::filesystem::path WavRenderer::render_preview(
    const music::ProductionIntentV1& intent,
    const std::filesystem::path& output_path) const {

    if (intent.duration_seconds <= 0.0) {
        throw std::invalid_argument("duration_seconds must be greater than zero");
    }

    std::filesystem::create_directories(output_path.parent_path());

    const auto sample_count = static_cast<std::size_t>(intent.duration_seconds * sample_rate);
    std::vector<std::int16_t> pcm(sample_count, 0);

    const double bpm = intent.bpm > 0.0 ? intent.bpm : 86.0;
    const double beat_seconds = 60.0 / bpm;
    const double drum_density = clamp01(intent.drum_density);
    const double grit = clamp01(intent.texture_grit);
    const double bass_weight = clamp01(intent.bass_weight);

    std::uint32_t noise_state = static_cast<std::uint32_t>(intent.seed == 0 ? 0x58454e4fu : intent.seed);

    for (std::size_t i = 0; i < sample_count; ++i) {
        const double t = static_cast<double>(i) / sample_rate;
        const double beat_position = std::fmod(t, beat_seconds);
        const double beat_index = std::floor(t / beat_seconds);

        double sample = 0.0;

        // Kick pulse on each beat. Density controls how much off-beat material appears.
        if (beat_position < 0.16) {
            const double envelope = std::exp(-beat_position * 24.0);
            const double frequency = 56.0 - 24.0 * (beat_position / 0.16);
            sample += std::sin(2.0 * std::numbers::pi * frequency * t) * envelope * 0.72;
        }

        // Sparse snare on beats 2 and 4.
        const auto quarter = static_cast<int>(beat_index) % 4;
        if ((quarter == 1 || quarter == 3) && beat_position < 0.09) {
            noise_state = noise_state * 1664525u + 1013904223u;
            const double noise = (static_cast<double>((noise_state >> 8) & 0xffffu) / 32767.5) - 1.0;
            const double envelope = std::exp(-beat_position * 38.0);
            sample += noise * envelope * (0.22 + drum_density * 0.24);
        }

        // Optional eighth-note hat texture.
        const double eighth = beat_seconds * 0.5;
        const double eighth_position = std::fmod(t, eighth);
        if (drum_density > 0.30 && eighth_position < 0.025) {
            noise_state = noise_state * 1664525u + 1013904223u;
            const double noise = (static_cast<double>((noise_state >> 8) & 0xffffu) / 32767.5) - 1.0;
            sample += noise * std::exp(-eighth_position * 110.0) * (drum_density - 0.30) * 0.18;
        }

        // Minimal bass bed keeps the preview musical while leaving vocal space.
        const double bass_frequency = 43.65; // F1-ish neutral prototype tone.
        sample += std::sin(2.0 * std::numbers::pi * bass_frequency * t) * 0.08 * bass_weight;

        // Texture grit is intentionally subtle in v0.1.
        noise_state = noise_state * 1664525u + 1013904223u;
        const double texture_noise = (static_cast<double>((noise_state >> 8) & 0xffffu) / 32767.5) - 1.0;
        sample += texture_noise * grit * 0.018;

        sample = std::clamp(sample, -1.0, 1.0);
        pcm[i] = static_cast<std::int16_t>(sample * 32767.0);
    }

    std::ofstream out(output_path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("failed to open output WAV");
    }

    const std::uint32_t data_size = static_cast<std::uint32_t>(pcm.size() * sizeof(std::int16_t));
    const std::uint32_t byte_rate = sample_rate * channels * bits_per_sample / 8;
    const std::uint16_t block_align = channels * bits_per_sample / 8;

    out.write("RIFF", 4);
    write_u32(out, 36u + data_size);
    out.write("WAVE", 4);
    out.write("fmt ", 4);
    write_u32(out, 16u);
    write_u16(out, 1u);
    write_u16(out, channels);
    write_u32(out, sample_rate);
    write_u32(out, byte_rate);
    write_u16(out, block_align);
    write_u16(out, bits_per_sample);
    out.write("data", 4);
    write_u32(out, data_size);
    out.write(reinterpret_cast<const char*>(pcm.data()), static_cast<std::streamsize>(data_size));

    return output_path;
}

} // namespace xenon
