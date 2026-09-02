#pragma once

#include "xenon/ether_dna.hpp"

#include <filesystem>

namespace xenon {

class EtherDNAStore {
public:
    void save(const EtherDNARecord& record, const std::filesystem::path& path) const;
    [[nodiscard]] EtherDNARecord load(const std::filesystem::path& path) const;
};

} // namespace xenon
