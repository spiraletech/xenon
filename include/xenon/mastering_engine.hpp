#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace xenon {

enum class MasteringPreset {
    StreamingBalanced,
    StreamingLoud,
    Dynamic,
    Archive
};

struct MasterExportProfile {
    std::string name{"streaming"};
    std::uint32_t sample_rate{44100};
    std::uint16_t bit_depth{24};
    bool dither{true};
};

struct MasteringTarget {
    double loudness_lufs{-14.0};
    double ceiling_dbfs{-1.0};
    double brightness{0.50};
    double stereo_width{0.55};
    double max_gain_db{12.0};
    double max_width{0.95};
    double loudness_tolerance_lu{1.0};
    MasterExportProfile export_profile{};
};

struct MasteringMetrics {
    double loudness_lufs_estimate{-120.0};
    double sample_peak_dbfs{-120.0};
    double oversampled_peak_dbfs{-120.0};
    double rms{0.0};
    double brightness{0.0};
    double stereo_width{0.0};
};

struct MasteringPlan {
    MasteringTarget target{};
    double input_gain_db{0.0};
    double low_tilt_db{0.0};
    double high_tilt_db{0.0};
    double stereo_width{1.0};
    double limiter_ceiling{0.8912509381};
};

struct MasteringResult {
    MasteringPlan plan{};
    MasteringMetrics before{};
    MasteringMetrics after{};
    std::vector<float> interleaved_stereo;
    std::vector<std::int32_t> quantized_pcm;
    bool quality_gate_passed{false};
    std::vector<std::string> quality_notes;
    std::uint32_t passes{0};
};

class MasteringEngine {
public:
    explicit MasteringEngine(MasteringPreset preset = MasteringPreset::StreamingBalanced);
    explicit MasteringEngine(MasteringTarget target);

    [[nodiscard]] static MasteringTarget preset(MasteringPreset preset);
    [[nodiscard]] MasteringPlan plan(std::span<const float> interleaved_stereo) const;
    [[nodiscard]] MasteringMetrics analyze(std::span<const float> interleaved_stereo) const;
    [[nodiscard]] MasteringResult master(std::span<const float> interleaved_stereo) const;

    void validate_audio(std::span<const float> interleaved_stereo) const;
    [[nodiscard]] const MasteringTarget& target() const noexcept;

private:
    void validate_target(const MasteringTarget& target) const;
    void render_pass(std::span<const float> input, const MasteringPlan& plan,
                     std::vector<float>& output) const;
    [[nodiscard]] std::vector<std::int32_t> quantize(
        std::span<const float> input, const MasterExportProfile& profile) const;

    MasteringTarget target_{};
};

} // namespace xenon
