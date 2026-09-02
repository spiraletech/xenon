#include "xenon/surgical_revision_engine.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace xenon {
namespace {

bool contains(const std::vector<SurgicalComponent>& values, SurgicalComponent value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}

void append_lock(ControlComponents& locks, SurgicalComponent component) {
    const auto mapped = control_component_for(component);
    if (mapped != ControlComponent::None) {
        locks = locks | mapped;
    }
}

} // namespace

ControlComponents SurgicalRevisionEngine::compile_preservation_locks(
    const MutationMask& mask,
    SurgicalComponent target) const noexcept {

    ControlComponents locks = 0;
    for (const auto component : mask.preserved_components) {
        append_lock(locks, component);
    }

    if (mask.preserve_arrangement) {
        locks = locks | ControlComponent::Arrangement;
    }

    const auto target_control = control_component_for(target);
    if (target_control != ControlComponent::None) {
        locks &= ~control_component(target_control);
    }
    return locks;
}

void SurgicalRevisionEngine::validate(
    const GenerationRequest& base_request,
    const SurgicalRevisionRequest& request) const {

    if (base_request.prompt.empty()) {
        throw std::invalid_argument("L25 base generation request requires a prompt");
    }
    if (request.target == SurgicalComponent::Unknown) {
        throw std::invalid_argument("L25 surgical revision requires a target component");
    }
    if (!request.region.valid()) {
        throw std::invalid_argument("L25 surgical revision requires a valid temporal edit region");
    }
    if (request.region.end_seconds > base_request.duration_seconds) {
        throw std::invalid_argument("L25 temporal edit region exceeds source duration");
    }
    if (request.mutation_amount < 0.0 || request.mutation_amount > 1.0) {
        throw std::invalid_argument("L25 mutation amount must be normalized");
    }
    if (contains(request.mask.preserved_components, request.target)) {
        throw std::invalid_argument("L25 target component cannot also be preserved");
    }
    if (!request.mask.mutable_components.empty() &&
        !contains(request.mask.mutable_components, request.target)) {
        throw std::invalid_argument("L25 target component must be included in mutable components");
    }
    if (request.source_audio.empty() && base_request.reference_audio.empty()) {
        throw std::invalid_argument("L25 surgical revision requires source audio");
    }
}

SurgicalRevisionPlan SurgicalRevisionEngine::compile(
    const GenerationRequest& base_request,
    const SurgicalRevisionRequest& request) const {

    validate(base_request, request);

    SurgicalRevisionPlan plan;
    plan.compiled_locks = compile_preservation_locks(request.mask, request.target);

    auto generated = base_request;
    generated.mode = GenerationMode::ReplaceSection;
    generated.render_intent = RenderIntent::Control;
    generated.reference_audio = request.source_audio.empty()
        ? base_request.reference_audio
        : request.source_audio;
    generated.mutation_amount = request.mutation_amount;
    generated.control.locks = plan.compiled_locks;
    generated.control.edit_start_seconds = request.region.start_seconds;
    generated.control.edit_end_seconds = request.region.end_seconds;
    generated.control.reference_strength = std::clamp(1.0 - request.mutation_amount, 0.55, 0.98);

    std::ostringstream prompt;
    prompt << base_request.prompt
           << ". Surgical revision: change only "
           << surgical_component_name(request.target)
           << " from " << request.region.start_seconds
           << "s to " << request.region.end_seconds << "s";
    if (!request.instruction.empty()) {
        prompt << ". " << request.instruction;
    }
    if (request.mask.preserve_arrangement) {
        prompt << ". Preserve arrangement and all locked components outside the edit region";
    }
    generated.prompt = prompt.str();

    plan.generation_request = std::move(generated);
    plan.diff.target = request.target;
    plan.diff.region = request.region;
    plan.diff.changed_components.push_back(request.target);
    plan.diff.preserved_components = request.mask.preserved_components;
    plan.diff.arrangement_preserved = request.mask.preserve_arrangement;

    std::ostringstream summary;
    summary << "replace " << surgical_component_name(request.target)
            << " @ " << request.region.start_seconds
            << "-" << request.region.end_seconds << "s";
    if (request.mask.preserve_arrangement) summary << "; arrangement locked";
    plan.diff.summary = summary.str();

    const auto target_stem = stem_role_for(request.target);
    if (!request.replacement_audio.empty() && target_stem != StemRole::Unknown) {
        plan.uses_direct_stem_replacement = true;
        plan.stem_replacement.target = target_stem;
        plan.stem_replacement.replacement_audio = request.replacement_audio;
        plan.stem_replacement.source_backend = "xenon.l25.direct";
    }

    return plan;
}

ReconstructionPlan SurgicalRevisionEngine::apply_direct_replacement(
    StemEngine& stems,
    StemSet& set,
    const SurgicalRevisionPlan& plan) const {

    if (!plan.uses_direct_stem_replacement) {
        throw std::invalid_argument("L25 plan does not contain a direct stem replacement");
    }
    return stems.replace(set, plan.stem_replacement);
}

} // namespace xenon
