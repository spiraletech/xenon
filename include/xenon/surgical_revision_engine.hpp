#pragma once

#include "xenon/surgical_revision.hpp"
#include "xenon/stem_engine.hpp"

namespace xenon {

class SurgicalRevisionEngine {
public:
    [[nodiscard]] SurgicalRevisionPlan compile(
        const GenerationRequest& base_request,
        const SurgicalRevisionRequest& request) const;

    [[nodiscard]] ReconstructionPlan apply_direct_replacement(
        StemEngine& stems,
        StemSet& set,
        const SurgicalRevisionPlan& plan) const;

private:
    [[nodiscard]] ControlComponents compile_preservation_locks(
        const MutationMask& mask,
        SurgicalComponent target) const noexcept;

    void validate(
        const GenerationRequest& base_request,
        const SurgicalRevisionRequest& request) const;
};

} // namespace xenon
