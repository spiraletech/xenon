#pragma once

#include "xenon/music_trinity.hpp"
#include "xenon/spectrum_analyzer.hpp"

#include <string>
#include <vector>

namespace xenon {

struct CortexContext {
    SpectrumFrame spectrum;
    std::vector<std::string> preferences;
    std::vector<std::string> revision_notes;
};

class Cortex {
public:
    [[nodiscard]] music::ProductionIntentV1 interpret(
        const std::string& user_request,
        const music::MusicFrameV1& frame,
        const CortexContext& context) const;
};

} // namespace xenon
