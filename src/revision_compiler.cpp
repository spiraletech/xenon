#include "xenon/revision_compiler.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

namespace xenon {
namespace {

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool contains(const std::string& haystack, const char* needle) {
    return haystack.find(needle) != std::string::npos;
}

void lock(ControlComponents& locks, ControlComponent component) {
    locks = locks | component;
}

} // namespace

GenerationRequest RevisionCompiler::compile(
    const GenerationRequest& previous_request,
    std::string critique_hint,
    std::string user_feedback) const {

    GenerationRequest revision = previous_request;
    revision.render_intent = RenderIntent::Control;
    revision.mode = GenerationMode::Variation;
    revision.mutation_amount = std::clamp(previous_request.mutation_amount * 0.72, 0.05, 0.75);

    const std::string instruction = user_feedback.empty() ? critique_hint : user_feedback;
    if (!instruction.empty()) {
        revision.prompt += user_feedback.empty()
            ? ". XENON self-critique: " + instruction
            : ". User revision: " + instruction;
    }

    const auto normalized = lower(instruction);

    if (contains(normalized, "hat") || contains(normalized, "drum") || contains(normalized, "transient")) {
        lock(revision.control.locks, ControlComponent::Bass);
        lock(revision.control.locks, ControlComponent::Melody);
        lock(revision.control.locks, ControlComponent::Harmony);
        lock(revision.control.locks, ControlComponent::Texture);
        lock(revision.control.locks, ControlComponent::Arrangement);
    }

    if (contains(normalized, "bass") && (contains(normalized, "keep") || contains(normalized, "preserve"))) {
        lock(revision.control.locks, ControlComponent::Bass);
    }
    if (contains(normalized, "melody") && (contains(normalized, "keep") || contains(normalized, "preserve"))) {
        lock(revision.control.locks, ControlComponent::Melody);
    }
    if (contains(normalized, "harmony") && (contains(normalized, "keep") || contains(normalized, "preserve"))) {
        lock(revision.control.locks, ControlComponent::Harmony);
    }
    if (contains(normalized, "arrangement") && (contains(normalized, "keep") || contains(normalized, "preserve"))) {
        lock(revision.control.locks, ControlComponent::Arrangement);
    }

    return revision;
}

} // namespace xenon
