#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace xenon {

inline constexpr std::size_t kSpectrumFftSize = 1024;
inline constexpr std::size_t kSpectrumBars = 72;

struct SpectrumFrame {
    double rms{0.0};
    double brightness{0.0};
    double spectral_density{0.0};
    double transient_density{0.0};
    std::array<float, kSpectrumBars> bars{};
};

struct SmoothedSpectrum {
    std::array<float, kSpectrumBars> bars{};
    std::array<float, kSpectrumBars> peaks{};
};

class SpectrumAnalyzer {
public:
    [[nodiscard]] SpectrumFrame analyzeWindow(
        const std::array<float, kSpectrumFftSize>& mono_samples,
        std::uint32_t sample_rate) const;

    void normalizeTrack(std::vector<SpectrumFrame>& frames) const;

    [[nodiscard]] SmoothedSpectrum smooth(
        const SpectrumFrame& frame,
        SmoothedSpectrum previous) const;
};

} // namespace xenon
