#pragma once

#include "xenon/generation_types.hpp"

#include <string>

namespace xenon {

class RevisionCompiler {
public:
    [[nodiscard]] GenerationRequest compile(
        const GenerationRequest& previous_request,
        std::string critique_hint,
        std::string user_feedback = {}) const;
};

} // namespace xenon
