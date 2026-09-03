#pragma once

#include "xenon/stem_types.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace xenon {

struct MixReferenceTarget {
    double rms{0.18};
    double brightness{0.50};
    double spectral_density{0.50};
    double stereo_width{0.55};
};

struct StemMixInput {
    std::string stem_id;
    StemRole role{StemRole::Unknown};
    StemAnalysis analysis{};
    bool locked{false};
    std::vector<float> interleaved_stereo;
};

struct StemMixDecision {
    std::string stem_id;
    StemRole role{StemRole::Unknown};
    bool locked{false};
    double gain_db{0.0};
    double pan{0.0};
    double low_tilt_db{0.0};
    double high_tilt_db{0.0};
    double compressor_threshold{0.85};
    double compressor_ratio{1.0};
    double masking_risk{0.0};
    std::string revision_hint;
};

struct MixMetrics {
    double rms{0.0};
    double peak{0.0};
    double brightness{0.0};
    double spectral_density{0.0};
    double stereo_width{0.0};
    double masking_score{0.0};
};

struct MixPlan {
    std::vector<StemMixDecision> stems;
    MixReferenceTarget target{};
    double headroom_db{6.0};
};

struct MixResult {
    MixPlan plan;
    MixMetrics before{};
    MixMetrics after{};
    std::vector<float> interleaved_stereo;
    std::vector<std::string> revision_hints;
    std::uint32_t passes{0};
};

struct MixPolicy {
    double target_headroom_db{6.0};
    double max_gain_adjust_db{12.0};
    double max_pan{0.85};
    double masking_threshold{0.62};
    std::uint32_t max_passes{2};
};

class MixEngine {
public:
    explicit MixEngine(MixPolicy policy = {});

    [[nodiscard]] MixPlan plan(
        const std::vector<StemMixInput>& stems,
        std::optional<MixReferenceTarget> reference = std::nullopt) const;

    [[nodiscard]] MixResult mix(
        const std::vector<StemMixInput>& stems,
        std::uint32_t sample_rate,
        std::optional<MixReferenceTarget> reference = std::nullopt) const;

    void validate(const std::vector<StemMixInput>& stems, std::uint32_t sample_rate) const;
    [[nodiscard]] const MixPolicy& policy() const noexcept;

private:
    [[nodiscard]] MixMetrics metrics(std::span<const float> stereo) const;
    [[nodiscard]] double masking_risk(const StemMixInput& a, const StemMixInput& b) const;
    void render_pass(const std::vector<StemMixInput>& stems, const MixPlan& plan,
                     std::vector<float>& output) const;

    MixPolicy policy_{};
};

} // namespace xenon
