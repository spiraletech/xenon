#pragma once

#include "xenon/generation_types.hpp"
#include "xenon/ether_composer.hpp"

#include <cstdint>
#include <string>

namespace xenon {

struct EtherDNARecord {
    std::string fingerprint;
    std::string parent_fingerprint;
    std::uint64_t seed{0};
    double bpm{0.0};
    std::string key;
    double mutation_amount{0.35};
    ControlComponents locks{0};
};

class EtherDNA {
public:
    [[nodiscard]] EtherDNARecord capture(
        const GenerationRequest& request,
        const CompositionPlan& plan,
        const std::string& parent_fingerprint = {}) const;
};

} // namespace xenon
