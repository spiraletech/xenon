#pragma once

#include "xenon/generation_types.hpp"

namespace xenon {

struct ControlCompileResult {
    GenerationRequest request;
    bool uses_reference{false};
    bool uses_component_locks{false};
    bool uses_temporal_control{false};
};

class EtherControl {
public:
    [[nodiscard]] ControlCompileResult compile(const GenerationRequest& request) const;
};

} // namespace xenon
