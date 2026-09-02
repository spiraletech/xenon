#pragma once

#include "xenon/generation_types.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace xenon {

enum class MemoryVerdict {
    Neutral,
    Accepted,
    Rejected
};

struct NumericPreference {
    std::string key;
    double value{0.0};
    double confidence{0.5};
};

struct TasteMemory {
    std::string text;
    double strength{0.5};
};

struct RevisionMemory {
    std::string fingerprint;
    std::string parent_fingerprint;
    std::string note;
    MemoryVerdict verdict{MemoryVerdict::Neutral};
    double score{0.0};
};

struct ProtectedMotif {
    std::string motif_id;
    std::string description;
    std::string source_fingerprint;
};

struct ProjectMemory {
    std::uint32_t schema_version{1};
    std::string project_id;
    std::vector<TasteMemory> preferences;
    std::vector<TasteMemory> rejections;
    std::vector<NumericPreference> numeric_preferences;
    std::vector<RevisionMemory> revisions;
    std::vector<ProtectedMotif> protected_motifs;
};

class OrganicMusicMemory {
public:
    void set_project(std::string project_id);
    [[nodiscard]] const std::string& project_id() const noexcept;

    void remember_preference(std::string text, double strength = 0.7);
    void remember_rejection(std::string text, double strength = 0.8);
    void set_numeric_preference(std::string key, double value, double confidence = 0.8);
    void remember_revision(RevisionMemory revision);
    void protect_motif(ProtectedMotif motif);

    [[nodiscard]] std::optional<NumericPreference> numeric_preference(const std::string& key) const;
    [[nodiscard]] bool has_preference_containing(const std::string& token) const;
    [[nodiscard]] bool has_rejection_containing(const std::string& token) const;
    [[nodiscard]] std::vector<std::string> recall_context(std::size_t max_items = 8) const;
    [[nodiscard]] const ProjectMemory& state() const noexcept;

    [[nodiscard]] GenerationRequest apply_to(GenerationRequest request) const;

    void save(const std::filesystem::path& path) const;
    void load(const std::filesystem::path& path);

private:
    ProjectMemory memory_;
};

} // namespace xenon
