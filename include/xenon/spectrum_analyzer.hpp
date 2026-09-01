#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace xenon {

struct SpectrumFrame {
    double rms{0.0};
    double brightness{0.0};
    double spectral_density{0.0};
    double transient_density{0.0};
    std::vector<float> bars;
};

class SpectrumAnalyzer {
public:
    explicit SpectrumAnalyzer(std::size_t bar_count = 64);

    [[nodiscard]] SpectrumFrame analyze(
        std::span<const float> mono_samples,
        double sample_rate) const;

private:
    std::size_t bar_count_;
};

} // namespace xenon
