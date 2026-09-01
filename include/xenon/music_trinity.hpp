#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace xenon::music {

inline constexpr std::uint32_t protocol_version_v1 = 1;

struct MusicFrameV1 {
    std::uint32_t schema_version{protocol_version_v1};

    std::string track_id;
    std::string title;
    std::string artist;

    double position_seconds{0.0};
    double bpm{0.0};
    std::string estimated_key;

    double loudness{0.0};
    double brightness{0.0};
    double warmth{0.0};
    double spectral_density{0.0};
    double transient_density{0.0};
    double stereo_width{0.0};
    double harmonicity{0.0};

    double beat_phase{0.0};
    std::string section;
};

struct ProductionIntentV1 {
    std::uint32_t schema_version{protocol_version_v1};

    std::string request_id;
    std::string project_id;
    std::string prompt;

    double bpm{0.0};
    std::string key;
    double duration_seconds{10.0};
    std::uint64_t seed{0};
    double mutation_amount{0.35};

    double drum_density{0.5};
    double bass_weight{0.5};
    double vocal_space{0.5};
    double texture_grit{0.5};
    double transient_density{0.5};

    bool keep_drums{false};
    bool keep_bass{false};
    bool keep_melody{false};
    bool keep_harmony{false};
    bool keep_texture{false};
    bool keep_arrangement{false};

    std::string chord_progression;
    std::vector<std::string> arrangement;
    std::filesystem::path reference_audio;
};

struct RenderArtifactV1 {
    std::uint32_t schema_version{protocol_version_v1};

    std::string request_id;
    std::string project_id;
    std::uint32_t revision{0};

    std::filesystem::path audio_path;
    std::filesystem::path metadata_path;
    std::string renderer;
    std::uint64_t resolved_seed{0};
};

struct MusicContinuityV1 {
    std::uint32_t schema_version{protocol_version_v1};

    std::string project_id;
    std::string current_revision;
    std::string currently_auditioning;
    std::vector<std::string> user_preferences;
    std::vector<std::string> revision_notes;
};

} // namespace xenon::music
