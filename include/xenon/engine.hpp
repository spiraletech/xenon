#pragma once

#include "xenon/music_trinity.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace xenon {

struct ProjectState {
    std::string project_id;
    std::uint32_t revision{0};
    std::optional<music::ProductionIntentV1> last_intent;
    std::optional<music::RenderArtifactV1> last_artifact;
};

class Engine {
public:
    Engine();

    [[nodiscard]] music::RenderArtifactV1 render(
        const music::ProductionIntentV1& intent,
        const std::filesystem::path& output_directory);

    [[nodiscard]] const ProjectState& state() const noexcept;

private:
    ProjectState state_;
};

} // namespace xenon
