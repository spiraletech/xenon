#include "xenon/musician_agents.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace xenon {
namespace {

double clamp01(double v) { return std::clamp(v, 0.0, 1.0); }

bool locked(ControlComponents locks, ControlComponent component) {
    return has_control_component(locks, component);
}

MusicianIntent build_intent(
    MusicianRole role,
    const MusicianAgentContext& context,
    ControlComponent lock_component,
    double density_scale,
    double activity_scale,
    double variation_bias,
    double dna_density) {

    if (!context.composer_plan) throw std::invalid_argument("L31 musician agent requires composer plan");

    MusicianIntent out;
    out.role = role;
    out.locked = locked(context.locks, lock_component);
    out.global_density = clamp01(dna_density * density_scale);
    out.global_activity = clamp01((1.0 - context.composer_plan->global_negative_space) * activity_scale);
    out.variation = out.locked ? 0.0 : clamp01(context.composer_plan->mutation_amount * 0.70 + variation_bias);

    for (const auto& section : context.composer_plan->sections) {
        SectionPerformanceIntent perf;
        perf.section = section.name;
        perf.activity = out.locked ? 0.0 : clamp01(section.energy * activity_scale * (1.0 - section.negative_space * 0.35));
        perf.density = out.locked ? 0.0 : clamp01(out.global_density * (0.65 + section.energy * 0.55));
        perf.variation = out.locked ? 0.0 : clamp01(out.variation * (0.55 + section.tension * 0.60));
        perf.space = clamp01(section.negative_space);
        out.sections.push_back(perf);
    }
    return out;
}

std::string summarize(const MusicianIntent& intent) {
    std::ostringstream out;
    out << musician_role_name(intent.role)
        << " locked=" << (intent.locked ? "yes" : "no")
        << ", activity=" << intent.global_activity
        << ", density=" << intent.global_density
        << ", variation=" << intent.variation;
    return out.str();
}

} // namespace

const char* musician_role_name(MusicianRole role) noexcept {
    switch (role) {
    case MusicianRole::Drummer: return "drummer";
    case MusicianRole::Bass: return "bass";
    case MusicianRole::Melody: return "melody";
    case MusicianRole::Harmony: return "harmony";
    case MusicianRole::Texture: return "texture";
    }
    return "unknown";
}

MusicianIntent DrummerAgent::perform(const MusicianAgentContext& context) const {
    const double dna_density = context.dna ? context.dna->rhythm.density : 0.55;
    auto out = build_intent(MusicianRole::Drummer, context, ControlComponent::Drums, 1.05, 1.00, 0.08, dna_density);
    out.instruction = "shape kick/snare/hat activity to section energy; leave negative space between accents";
    return out;
}

MusicianIntent BassAgent::perform(const MusicianAgentContext& context) const {
    const double dna_density = context.dna ? clamp01(context.dna->rhythm.density * 0.70 + context.dna->harmony.tension * 0.30) : 0.48;
    auto out = build_intent(MusicianRole::Bass, context, ControlComponent::Bass, 0.82, 0.88, 0.03, dna_density);
    out.instruction = "anchor harmony, follow kick selectively, and preserve low-end breathing room";
    return out;
}

MusicianIntent MelodyAgent::perform(const MusicianAgentContext& context) const {
    const double dna_density = context.dna ? clamp01(0.35 + context.dna->harmony.tension * 0.45) : 0.48;
    auto out = build_intent(MusicianRole::Melody, context, ControlComponent::Melody, 0.78, 0.92, 0.14, dna_density);
    out.instruction = "reuse composer motifs across anchor sections and vary contour without crowding vocals";
    return out;
}

MusicianIntent HarmonyAgent::perform(const MusicianAgentContext& context) const {
    const double dna_density = context.dna ? clamp01(0.30 + context.dna->harmony.tension * 0.50) : 0.46;
    auto out = build_intent(MusicianRole::Harmony, context, ControlComponent::Harmony, 0.76, 0.72, 0.02, dna_density);
    out.instruction = "pace chord movement from composer harmonic pacing and support tension curve";
    return out;
}

MusicianIntent TextureAgent::perform(const MusicianAgentContext& context) const {
    const double dna_density = context.dna ? context.dna->texture.density : 0.50;
    auto out = build_intent(MusicianRole::Texture, context, ControlComponent::Texture, 0.95, 0.74, 0.10, dna_density);
    out.instruction = "fill spectral gaps with restrained atmosphere and increase motion at transitions";
    return out;
}

EnsemblePlan MusicianEnsemble::conduct(const MusicianAgentContext& context) const {
    if (!context.composer_plan) throw std::invalid_argument("L31 ensemble requires composer plan");
    EnsemblePlan out;
    out.musicians = {
        drummer_.perform(context),
        bass_.perform(context),
        melody_.perform(context),
        harmony_.perform(context),
        texture_.perform(context)
    };

    std::ostringstream summary;
    summary << "shared EtherDNA ensemble; ";
    for (std::size_t i = 0; i < out.musicians.size(); ++i) {
        if (i) summary << " | ";
        summary << summarize(out.musicians[i]);
    }
    out.coordination_summary = summary.str();
    validate(out);
    return out;
}

void MusicianEnsemble::validate(const EnsemblePlan& ensemble) const {
    if (ensemble.musicians.size() != 5) throw std::invalid_argument("L31 ensemble must contain five musician agents");
    bool seen[5] = {false, false, false, false, false};
    for (const auto& musician : ensemble.musicians) {
        const auto idx = static_cast<std::size_t>(musician.role);
        if (idx >= 5 || seen[idx]) throw std::invalid_argument("L31 ensemble role missing or duplicated");
        seen[idx] = true;
        auto normalized = [](double v) { return v >= 0.0 && v <= 1.0; };
        if (!normalized(musician.global_activity) || !normalized(musician.global_density) || !normalized(musician.variation))
            throw std::invalid_argument("L31 musician scalar out of range");
        for (const auto& section : musician.sections) {
            if (section.section.empty() || !normalized(section.activity) || !normalized(section.density) ||
                !normalized(section.variation) || !normalized(section.space))
                throw std::invalid_argument("L31 section performance intent invalid");
        }
    }
}

GenerationRequest MusicianEnsemble::compile(
    const GenerationRequest& request,
    const EnsemblePlan& ensemble) const {
    validate(ensemble);
    GenerationRequest out = request;

    std::ostringstream prompt;
    prompt << ". MUSICIAN ENSEMBLE: ";
    for (std::size_t i = 0; i < ensemble.musicians.size(); ++i) {
        const auto& musician = ensemble.musicians[i];
        if (i) prompt << " | ";
        prompt << musician_role_name(musician.role)
               << "{locked=" << (musician.locked ? "1" : "0")
               << ",activity=" << musician.global_activity
               << ",density=" << musician.global_density
               << ",variation=" << musician.variation
               << ",instruction=" << musician.instruction << '}';
    }
    prompt << "; coordination=" << ensemble.coordination_summary;
    out.prompt += prompt.str();
    return out;
}

} // namespace xenon
