#pragma once

#include "xenon/spectrum_analyzer.hpp"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace xenon {

struct TrackAnalysis {
    std::uint32_t sample_rate{0};
    std::vector<SpectrumFrame> frames;

    [[nodiscard]] const SpectrumFrame& frameAtSeconds(double seconds) const;
};

class MediaAnalyzer {
public:
    [[nodiscard]] TrackAnalysis analyzeFile(const std::filesystem::path& path) const;
};

} // namespace xenon
