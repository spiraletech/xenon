#pragma once

#include "xenon/project_graph.hpp"

#include <string>

namespace xenon {

class SessionOS {
public:
    explicit SessionOS(std::string project_id);

    [[nodiscard]] ProjectGraph& graph() noexcept;
    [[nodiscard]] const ProjectGraph& graph() const noexcept;

    [[nodiscard]] std::string add_reference(std::string label, std::string asset_uri);
    [[nodiscard]] std::string add_prompt(const std::string& reference_id, std::string prompt);
    [[nodiscard]] std::string add_ether_dna(const std::string& prompt_id, std::string fingerprint);
    [[nodiscard]] std::string add_generation(const std::string& dna_id, std::string asset_uri, std::string fingerprint);
    [[nodiscard]] std::string add_stem(const std::string& generation_id, std::string role, std::string asset_uri);
    [[nodiscard]] std::string add_revision(const std::string& source_id, std::string label, std::string asset_uri);
    [[nodiscard]] std::string add_mix(const std::string& source_id, std::string asset_uri);
    [[nodiscard]] std::string add_master(const std::string& mix_id, std::string asset_uri);

private:
    [[nodiscard]] std::string append(const std::string& parent, ProjectNode node, ProjectEdgeType relation);
    ProjectGraph graph_;
};

} // namespace xenon
