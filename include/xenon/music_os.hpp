#pragma once

#include "xenon/autonomous_producer.hpp"
#include "xenon/mastering_engine.hpp"
#include "xenon/mix_engine.hpp"
#include "xenon/organic_music_memory.hpp"
#include "xenon/player_engine.hpp"
#include "xenon/session_os.hpp"
#include "xenon/trinity_device_bus.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace xenon {

enum class MusicOSDomain { Ears, Mind, Hands, Memory, Models, Devices };

struct MusicOSCapability {
    MusicOSDomain domain{MusicOSDomain::Ears};
    std::string name;
    bool available{false};
    std::string detail;
};

struct MusicOSState {
    std::string version;
    std::string project_id;
    std::vector<MusicOSCapability> capabilities;
    std::size_t model_provider_count{0};
    bool project_open{false};
    bool audio_open{false};
    bool autonomous_job_active{false};
};

class MusicOS {
public:
    explicit MusicOS(ModelRouter router);

    void open_project(std::string project_id);
    [[nodiscard]] const std::string& project_id() const noexcept;
    [[nodiscard]] SessionOS& session();
    [[nodiscard]] OrganicMusicMemory& memory() noexcept;
    [[nodiscard]] TrinityDeviceBus& devices() noexcept;
    [[nodiscard]] GenerationPipeline& generation() noexcept;
    [[nodiscard]] ModelRouter& models() noexcept;

    void open_audio(const std::filesystem::path& path);
    [[nodiscard]] TrackAnalysis analyze_audio(const std::filesystem::path& path) const;
    [[nodiscard]] SpectrumFrame current_spectrum() const;

    [[nodiscard]] GenerationResult create(const GenerationRequest& request, const std::filesystem::path& output_directory);
    [[nodiscard]] MixResult mix(const std::vector<StemMixInput>& stems, std::uint32_t sample_rate,
                                std::optional<MixReferenceTarget> reference = std::nullopt) const;
    [[nodiscard]] MasteringResult master(const std::vector<float>& stereo) const;
    [[nodiscard]] AutonomousProducerResult produce(const AutonomousProducerGoal& goal,
                                                    const std::filesystem::path& output_directory);

    [[nodiscard]] MusicOSState state() const;
    [[nodiscard]] std::string manifest() const;

private:
    void require_project() const;
    ModelRouter router_;
    GenerationPipeline pipeline_;
    OrganicMusicMemory memory_;
    TrinityDeviceBus devices_;
    std::unique_ptr<SessionOS> session_;
    mutable MediaAnalyzer analyzer_;
    PlayerEngine player_;
    MixEngine mixer_;
    MasteringEngine mastering_;
    std::string project_id_;
    bool audio_open_{false};
    bool autonomous_active_{false};
};

} // namespace xenon
