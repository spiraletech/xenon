#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace xenon {

enum class ProjectNodeType {
    Reference,
    Prompt,
    EtherDNA,
    Generation,
    Stem,
    Revision,
    Mix,
    Master
};

enum class ProjectEdgeType {
    DerivedFrom,
    References,
    Contains,
    Revises,
    MixedFrom,
    MasteredFrom
};

struct ProjectNode {
    std::string id;
    ProjectNodeType type{ProjectNodeType::Reference};
    std::string label;
    std::string asset_uri;
    std::string fingerprint;
    std::uint64_t revision{0};
    bool active{true};
};

struct ProjectEdge {
    std::string from;
    std::string to;
    ProjectEdgeType type{ProjectEdgeType::DerivedFrom};
};

struct ProjectGraphSnapshot {
    std::uint32_t schema_version{1};
    std::string project_id;
    std::string head_id;
    std::vector<ProjectNode> nodes;
    std::vector<ProjectEdge> edges;
};

class ProjectGraph {
public:
    explicit ProjectGraph(std::string project_id = {});

    [[nodiscard]] const std::string& project_id() const noexcept;
    [[nodiscard]] const std::string& head_id() const noexcept;
    [[nodiscard]] std::size_t node_count() const noexcept;
    [[nodiscard]] std::size_t edge_count() const noexcept;

    [[nodiscard]] std::string add_node(ProjectNode node);
    void add_edge(ProjectEdge edge);
    void set_head(const std::string& node_id);

    [[nodiscard]] const ProjectNode& node(const std::string& node_id) const;
    [[nodiscard]] std::vector<ProjectNode> children(const std::string& node_id) const;
    [[nodiscard]] std::vector<ProjectNode> parents(const std::string& node_id) const;
    [[nodiscard]] std::vector<ProjectNode> lineage(const std::string& node_id) const;

    [[nodiscard]] bool can_undo() const noexcept;
    [[nodiscard]] bool can_redo() const noexcept;
    [[nodiscard]] std::string undo();
    [[nodiscard]] std::string redo();
    [[nodiscard]] std::string checkout(const std::string& node_id);
    [[nodiscard]] std::string branch_from(const std::string& node_id, ProjectNode node,
                                          ProjectEdgeType relation = ProjectEdgeType::DerivedFrom);

    [[nodiscard]] ProjectGraphSnapshot snapshot() const;
    [[nodiscard]] std::string serialize() const;
    [[nodiscard]] static ProjectGraph deserialize(const std::string& text);

private:
    [[nodiscard]] bool has_node(const std::string& node_id) const noexcept;
    [[nodiscard]] bool would_cycle(const std::string& from, const std::string& to) const;
    void push_history(const std::string& node_id);

    std::string project_id_;
    std::string head_id_;
    std::unordered_map<std::string, ProjectNode> nodes_;
    std::vector<std::string> insertion_order_;
    std::vector<ProjectEdge> edges_;
    std::vector<std::string> history_;
    std::size_t history_index_{0};
    std::uint64_t next_id_{1};
};

} // namespace xenon
