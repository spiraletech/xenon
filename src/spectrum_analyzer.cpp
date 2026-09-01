#include "xenon/spectrum_analyzer.hpp"

#include <algorithm>
#include <cmath>
#include <complex>

namespace xenon {
namespace {

constexpr float kPi = 3.14159265358979323846f;

void fft(std::array<std::complex<float>, kSpectrumFftSize>& values) {
    for (std::size_t i = 1, j = 0; i < kSpectrumFftSize; ++i) {
        std::size_t bit = kSpectrumFftSize >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(values[i], values[j]);
    }

    for (std::size_t len = 2; len <= kSpectrumFftSize; len <<= 1) {
        const float angle = -2.0f * kPi / static_cast<float>(len);
        const std::complex<float> wlen(std::cos(angle), std::sin(angle));
        for (std::size_t i = 0; i < kSpectrumFftSize; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            for (std::size_t j = 0; j < len / 2; ++j) {
                const auto u = values[i + j];
                const auto v = values[i + j + len / 2] * w;
                values[i + j] = u + v;
                values[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

} // namespace

SpectrumFrame SpectrumAnalyzer::analyzeWindow(
    const std::array<float, kSpectrumFftSize>& samples,
    std::uint32_t sample_rate) const {
    SpectrumFrame frame{};
    if (sample_rate == 0) return frame;

    std::array<std::complex<float>, kSpectrumFftSize> bins{};
    double energy = 0.0;
    double transient = 0.0;

    for (std::size_t i = 0; i < kSpectrumFftSize; ++i) {
        const float hann = 0.5f - 0.5f * std::cos(
            2.0f * kPi * static_cast<float>(i) /
            static_cast<float>(kSpectrumFftSize - 1));
        bins[i] = std::complex<float>(samples[i] * hann, 0.0f);
        energy += static_cast<double>(samples[i]) * samples[i];
        if (i != 0) transient += std::abs(samples[i] - samples[i - 1]);
    }

    fft(bins);

    const auto averageBand = [&](float lo, float hi) {
        const auto first = static_cast<std::size_t>(std::clamp(
            static_cast<int>(lo * static_cast<float>(kSpectrumFftSize) / static_cast<float>(sample_rate)),
            1,
            static_cast<int>(kSpectrumFftSize / 2 - 1)));
        const auto last = static_cast<std::size_t>(std::clamp(
            static_cast<int>(hi * static_cast<float>(kSpectrumFftSize) / static_cast<float>(sample_rate)),
            static_cast<int>(first + 1),
            static_cast<int>(kSpectrumFftSize / 2)));

        double total = 0.0;
        for (std::size_t i = first; i < last; ++i) total += std::abs(bins[i]);
        return static_cast<float>(total / std::max<std::size_t>(1, last - first));
    };

    constexpr float low_hz = 35.0f;
    constexpr float high_hz = 18000.0f;
    constexpr float ratio = high_hz / low_hz;

    double weighted = 0.0;
    double magnitude = 0.0;
    std::size_t active_bands = 0;

    for (std::size_t band = 0; band < kSpectrumBars; ++band) {
        const float t0 = static_cast<float>(band) / static_cast<float>(kSpectrumBars);
        const float t1 = static_cast<float>(band + 1) / static_cast<float>(kSpectrumBars);
        const float lo = low_hz * std::pow(ratio, t0);
        const float hi = low_hz * std::pow(ratio, t1);
        const float value = averageBand(lo, hi);
        frame.bars[band] = value;
        magnitude += value;
        weighted += value * ((lo + hi) * 0.5);
        if (value > 0.0001f) ++active_bands;
    }

    frame.rms = std::sqrt(energy / static_cast<double>(kSpectrumFftSize));
    frame.transient_density = std::clamp(
        transient / static_cast<double>(kSpectrumFftSize) * 4.0,
        0.0,
        1.0);
    frame.brightness = magnitude > 0.0
        ? std::clamp((weighted / magnitude) / high_hz, 0.0, 1.0)
        : 0.0;
    frame.spectral_density = static_cast<double>(active_bands) /
        static_cast<double>(kSpectrumBars);
    return frame;
}

void SpectrumAnalyzer::normalizeTrack(std::vector<SpectrumFrame>& frames) const {
    std::array<float, kSpectrumBars> maxima{};
    maxima.fill(0.0001f);

    for (const auto& frame : frames) {
        for (std::size_t i = 0; i < kSpectrumBars; ++i) {
            maxima[i] = std::max(maxima[i], frame.bars[i]);
        }
    }

    for (auto& frame : frames) {
        for (std::size_t i = 0; i < kSpectrumBars; ++i) {
            frame.bars[i] = std::clamp(
                std::pow(frame.bars[i] / maxima[i], 0.42f),
                0.0f,
                1.0f);
        }
    }
}

SmoothedSpectrum SpectrumAnalyzer::smooth(
    const SpectrumFrame& frame,
    SmoothedSpectrum previous) const {
    for (std::size_t i = 0; i < kSpectrumBars; ++i) {
        const float target = frame.bars[i];
        const float attack = target > previous.bars[i] ? 0.50f : 0.14f;
        previous.bars[i] += (target - previous.bars[i]) * attack;
        previous.peaks[i] = std::max(previous.bars[i], previous.peaks[i] - 0.018f);
    }
    return previous;
}

} // namespace xenon
