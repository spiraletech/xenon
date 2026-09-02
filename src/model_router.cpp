#include "xenon/model_router.hpp"

#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace xenon {
namespace {

ProviderCapability mode_capability(GenerationMode mode) {
    switch (mode) {
    case GenerationMode::TextToInstrumental: return ProviderCapability::TextToInstrumental;
    case GenerationMode::Variation: return ProviderCapability::Variation;
    case GenerationMode::Extend: return ProviderCapability::Extend;
    case GenerationMode::AudioToAudio: return ProviderCapability::AudioToAudio;
    case GenerationMode::ReplaceSection: return ProviderCapability::ReplaceSection;
    }
    return ProviderCapability::None;
}

ProviderCapability role_capability(RenderIntent intent) {
    switch (intent) {
    case RenderIntent::Draft: return ProviderCapability::DraftRole;
    case RenderIntent::Quality: return ProviderCapability::QualityRole;
    case RenderIntent::Control: return ProviderCapability::ControlRole;
    case RenderIntent::Vocal: return ProviderCapability::VocalRole;
    case RenderIntent::Auto: return ProviderCapability::None;
    }
    return ProviderCapability::None;
}

RenderIntent resolve_intent(const GenerationRequest& request) {
    if (request.render_intent != RenderIntent::Auto) return request.render_intent;
    return request.mode == GenerationMode::TextToInstrumental
        ? RenderIntent::Quality
        : RenderIntent::Control;
}

bool needs_temporal_control(const GenerationRequest& request) {
    return request.mode == GenerationMode::ReplaceSection ||
        request.control.edit_start_seconds >= 0.0 ||
        request.control.edit_end_seconds >= 0.0;
}

bool supports_details(ProviderCapabilities caps, const GenerationRequest& request) {
    if (request.control.locks != 0 && !has_capability(caps, ProviderCapability::ComponentLocks)) return false;
    if (!request.control.drum_reference.empty() && !has_capability(caps, ProviderCapability::DrumConditioning)) return false;
    if (!request.control.melody_reference.empty() && !has_capability(caps, ProviderCapability::MelodyConditioning)) return false;
    if (!request.control.chord_progression.empty() && !has_capability(caps, ProviderCapability::HarmonyConditioning)) return false;
    if (needs_temporal_control(request) && !has_capability(caps, ProviderCapability::TemporalControl)) return false;
    return true;
}

} // namespace

void ModelRouter::add_provider(std::unique_ptr<IModelBackend> backend, int priority) {
    if (!backend) throw std::invalid_argument("XENON cannot register a null model backend");
    providers_.push_back(ProviderSlot{std::move(backend), priority});
}

std::size_t ModelRouter::provider_count() const noexcept {
    return providers_.size();
}

std::vector<ProviderInfo> ModelRouter::providers() const {
    std::vector<ProviderInfo> result;
    result.reserve(providers_.size());
    for (const auto& slot : providers_) {
        result.push_back(ProviderInfo{
            std::string{slot.backend->name()},
            slot.backend->capabilities(),
            slot.backend->runtime_type(),
            slot.priority
        });
    }
    return result;
}

const ModelRouter::ProviderSlot& ModelRouter::select_provider(
    const GenerationRequest& request,
    RenderIntent resolved_intent) const {

    if (providers_.empty()) throw std::runtime_error("XENON has no model backends registered");

    const auto required_mode = mode_capability(request.mode);
    const auto required_role = role_capability(resolved_intent);
    const bool needs_reference = !request.reference_audio.empty();

    const ProviderSlot* best = nullptr;
    int best_score = std::numeric_limits<int>::min();

    for (const auto& slot : providers_) {
        const auto caps = slot.backend->capabilities();
        if (!has_capability(caps, required_mode)) continue;
        if (required_role != ProviderCapability::None && !has_capability(caps, required_role)) continue;
        if (needs_reference && !has_capability(caps, ProviderCapability::ReferenceAudio)) continue;
        if (!supports_details(caps, request)) continue;

        int score = slot.priority;
        if (has_capability(caps, required_role)) score += 1000;
        if (needs_reference && has_capability(caps, ProviderCapability::ReferenceAudio)) score += 100;
        if (has_capability(caps, ProviderCapability::LocalRuntime)) score += 10;

        if (!best || score > best_score) {
            best = &slot;
            best_score = score;
        }
    }

    if (!best) {
        std::ostringstream error;
        error << "No XENON backend supports the requested generation mode/control requirements";
        throw std::runtime_error(error.str());
    }

    return *best;
}

RouteDecision ModelRouter::route(const GenerationRequest& request) const {
    const auto resolved = resolve_intent(request);
    const auto& slot = select_provider(request, resolved);
    return RouteDecision{
        std::string{slot.backend->name()},
        request.render_intent,
        resolved,
        slot.backend->capabilities(),
        slot.backend->runtime_type()
    };
}

GenerationArtifact ModelRouter::generate(
    const GenerationRequest& request,
    const std::filesystem::path& output_directory) {

    if (request.prompt.empty()) throw std::invalid_argument("Generation prompt cannot be empty");
    if (request.duration_seconds <= 0.0 || request.duration_seconds > 600.0)
        throw std::invalid_argument("Generation duration must be within (0, 600] seconds");

    const auto resolved = resolve_intent(request);
    const auto& slot = select_provider(request, resolved);
    return slot.backend->generate(request, output_directory);
}

} // namespace xenon
