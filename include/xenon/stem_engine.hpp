#pragma once

#include "xenon/stem_analyzer.hpp"
#include "xenon/stem_separator.hpp"
#include "xenon/stem_types.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace xenon {

struct ImportedStem {
    StemRole role{StemRole::Unknown};
    std::filesystem::path audio_path;
    std::string source_backend{"import"};
};

class StemEngine {
public:
    [[nodiscard]] StemSet import_stems(
        std::string stem_set_id,
        std::filesystem::path source_mix,
        std::vector<ImportedStem> stems,
        std::string ether_dna_fingerprint = {},
        std::string parent_ether_dna_fingerprint = {}) const;

    [[nodiscard]] StemSet separate(
        IStemSeparator& separator,
        const std::filesystem::path& source_mix,
        const std::filesystem::path& output_directory,
        std::string ether_dna_fingerprint = {},
        std::string parent_ether_dna_fingerprint = {}) const;

    void analyze(StemSet& set) const;
    void set_locked(StemSet& set, StemRole role, bool locked) const;
    [[nodiscard]] bool is_locked(const StemSet& set, StemRole role) const noexcept;

    [[nodiscard]] ReconstructionPlan replace(
        StemSet& set,
        const StemReplacementRequest& request) const;

private:
    [[nodiscard]] StemArtifact* find(StemSet& set, StemRole role) const noexcept;
    [[nodiscard]] const StemArtifact* find(const StemSet& set, StemRole role) const noexcept;

    StemAnalyzer analyzer_;
};

} // namespace xenon
