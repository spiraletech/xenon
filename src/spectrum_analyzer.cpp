#include "xenon/spectrum_analyzer.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>

namespace xenon {
namespace {

std::vector<std::complex<double>> dft(std::span<const float> samples) {
    const std::size_t n = samples.size();
    const std::size_t bins = n / 2 + 1;
    std::vector<std::complex<double>> out(bins);
    for (std::size_t k = 0; k < bins; ++k) {
        std::complex<double> sum{};
        for (std::size_t i = 0; i < n; ++i) {
            const double w = 0.5 - 0.5 * std::cos((2.0 * std::numbers::pi * i) / std::max<std::size_t>(1, n - 1));
            const double phase = -2.0 * std::numbers::pi * static_cast<double>(k * i) / static_cast<double>(n);
            sum += static_cast<double>(samples[i]) * w * std::complex<double>(std::cos(phase), std::sin(phase));
        }
        out[k] = sum;
    }
    return out;
}

} // namespace

SpectrumAnalyzer::SpectrumAnalyzer(std::size_t bar_count)
    : bar_count_(std::max<std::size_t>(8, bar_count)) {}

SpectrumFrame SpectrumAnalyzer::analyze(std::span<const float> mono_samples, double sample_rate) const {
    SpectrumFrame frame;
    frame.bars.assign(bar_count_, 0.0f);
    if (mono_samples.size() < 32 || sample_rate <= 0.0) return frame;

    double energy = 0.0;
    double delta = 0.0;
    for (std::size_t i = 0; i < mono_samples.size(); ++i) {
        const double s = mono_samples[i];
        energy += s * s;
        if (i > 0) delta += std::abs(s - mono_samples[i - 1]);
    }
    frame.rms = std::sqrt(energy / mono_samples.size());
    frame.transient_density = std::clamp(delta / mono_samples.size() * 4.0, 0.0, 1.0);

    const auto bins = dft(mono_samples);
    const double nyquist = sample_rate * 0.5;
    double weighted_hz = 0.0;
    double mag_sum = 0.0;
    std::size_t active = 0;

    for (std::size_t k = 1; k < bins.size(); ++k) {
        const double mag = std::abs(bins[k]);
        const double hz = static_cast<double>(k) * sample_rate / mono_samples.size();
        mag_sum += mag;
        weighted_hz += hz * mag;
        if (mag > 0.01) ++active;

        const double norm_hz = std::clamp(hz / nyquist, 0.0, 1.0);
        const double curved = std::log1p(norm_hz * 31.0) / std::log(32.0);
        const auto bar = std::min(bar_count_ - 1, static_cast<std::size_t>(curved * bar_count_));
        frame.bars[bar] = std::max(frame.bars[bar], static_cast<float>(mag));
    }

    float peak = 0.0f;
    for (float v : frame.bars) peak = std::max(peak, v);
    if (peak > 0.0f) for (float& v : frame.bars) v = std::clamp(v / peak, 0.0f, 1.0f);

    frame.brightness = mag_sum > 0.0 ? std::clamp((weighted_hz / mag_sum) / nyquist, 0.0, 1.0) : 0.0;
    frame.spectral_density = bins.size() > 1 ? static_cast<double>(active) / (bins.size() - 1) : 0.0;
    return frame;
}

} // namespace xenon
