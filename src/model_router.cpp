#include "xenon/model_router.hpp"

#include <algorithm>
#include <cctype>
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

std::string safe_component(std::string value) {
    for (auto& ch : value) {
        const auto c = static_cast<unsigned char>(ch);
        if (!std::isalnum(c) && ch != '-' && ch != '_') ch = '_';
    }
    if (value.empty()) value = "backend";
    return value;
}

} // namespace

void ModelRouter::add_provider(std::unique_ptr<IModelBackend> backend, int priority) {
    if (!backend) throw std::invalid_argument("XENON cannot register a null model backend");
    providers_.push_back(ProviderSlot{std::move(backend), priority});
}

void ModelRouter::set_policy(BackendPolicy policy) noexcept {
    policy_ = policy;
}

const BackendPolicy& ModelRouter::policy() const noexcept {
    return policy_;
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

bool ModelRouter::eligible(
    const ProviderSlot& slot,
    const GenerationRequest& request,
    RenderIntent resolved_intent) const {

    const auto caps = slot.backend->capabilities();
    const auto runtime = slot.backend->runtime_type();
    const auto required_mode = mode_capability(request.mode);
    const auto required_role = role_capability(resolved_intent);
    const bool needs_reference = !request.reference_audio.empty();

    if (!policy_.allows(runtime)) return false;
    if (!has_capability(caps, required_mode)) return false;
    if (required_role != ProviderCapability::None && !has_capability(caps, required_role)) return false;
    if (needs_reference && !has_capability(caps, ProviderCapability::ReferenceAudio)) return false;
    if (!supports_details(caps, request)) return false;
    return true;
}

const ModelRouter::ProviderSlot& ModelRouter::select_provider(
    const GenerationRequest& request,
    RenderIntent resolved_intent) const {

    if (providers_.empty()) throw std::runtime_error("XENON has no model backends registered");

    const auto required_role = role_capability(resolved_intent);
    const bool needs_reference = !request.reference_audio.empty();

    const ProviderSlot* best = nullptr;
    int best_score = std::numeric_limits<int>::min();

    for (const auto& slot : providers_) {
        if (!eligible(slot, request, resolved_intent)) continue;
        const auto caps = slot.backend->capabilities();
        const auto runtime = slot.backend->runtime_type();

        int score = slot.priority + policy_.runtime_score(runtime);
        if (has_capability(caps, required_role)) score += 1000;
        if (needs_reference && has_capability(caps, ProviderCapability::ReferenceAudio)) score += 100;

        if (!best || score > best_score) {
            best = &slot;
            best_score = score;
        }
    }

    if (!best) {
        std::ostringstream error;
        error << "No XENON backend satisfies the generation requirements and runtime policy";
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

std::vector<BackendGenerationAttempt> ModelRouter::generate_all(
    const GenerationRequest& request,
    const std::filesystem::path& output_directory) {

    if (request.prompt.empty()) throw std::invalid_argument("Generation prompt cannot be empty");
    if (request.duration_seconds <= 0.0 || request.duration_seconds > 600.0)
        throw std::invalid_argument("Generation duration must be within (0, 600] seconds");

    const auto resolved = resolve_intent(request);
    std::vector<BackendGenerationAttempt> attempts;

    for (auto& slot : providers_) {
        if (!eligible(slot, request, resolved)) continue;

        BackendGenerationAttempt attempt;
        attempt.route = RouteDecision{
            std::string{slot.backend->name()},
            request.render_intent,
            resolved,
            slot.backend->capabilities(),
            slot.backend->runtime_type()
        };

        try {
            const auto provider_dir = output_directory / safe_component(attempt.route.provider_name);
            attempt.artifact = slot.backend->generate(request, provider_dir);
            attempt.success = true;
        } catch (const std::exception& ex) {
            attempt.error = ex.what();
        } catch (...) {
            attempt.error = "unknown backend generation failure";
        }
        attempts.push_back(std::move(attempt));
    }

    if (attempts.empty()) {
        throw std::runtime_error("No XENON backend is eligible for candidate fan-out");
    }

    return attempts;
}

} // namespace xenon
