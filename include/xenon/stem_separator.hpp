#pragma once

#include "xenon/stem_types.hpp"

#include <filesystem>
#include <string_view>

namespace xenon {

class IStemSeparator {
public:
    virtual ~IStemSeparator() = default;

    [[nodiscard]] virtual std::string_view name() const noexcept = 0;

    virtual StemSet separate(
        const std::filesystem::path& source_mix,
        const std::filesystem::path& output_directory,
        std::string ether_dna_fingerprint = {},
        std::string parent_ether_dna_fingerprint = {}) = 0;
};

} // namespace xenon
