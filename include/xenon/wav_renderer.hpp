#pragma once

#include "xenon/music_trinity.hpp"

#include <filesystem>

namespace xenon {

class WavRenderer {
public:
    [[nodiscard]] std::filesystem::path render_preview(
        const music::ProductionIntentV1& intent,
        const std::filesystem::path& output_path) const;
};

} // namespace xenon
