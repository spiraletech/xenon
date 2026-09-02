#pragma once

#include "xenon/media_analyzer.hpp"
#include "xenon/stem_types.hpp"
#include "xenon/synesthesia_scorer.hpp"

namespace xenon {

class StemAnalyzer {
public:
    [[nodiscard]] StemAnalysis analyze(const std::filesystem::path& audio_path) const;
    [[nodiscard]] StemRole classify(const TrackAnalysis& analysis, double& confidence) const;

private:
    MediaAnalyzer analyzer_;
    SynesthesiaScorer synesthesia_;
};

} // namespace xenon
