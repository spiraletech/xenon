#include "xenon/composer_agent.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace xenon {
namespace {

double clamp01(double v) { return std::clamp(v, 0.0, 1.0); }

int total_bars(const GenerationRequest& request, double bpm) {
    const double beats = request.duration_seconds * bpm / 60.0;
    return std::max(4, static_cast<int>(std::round(beats / 4.0)));
}

bool prompt_contains(const std::string& prompt, const std::string& token) {
    auto lower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        return s;
    };
    return lower(prompt).find(lower(token)) != std::string::npos;
}

} // namespace

ComposerAgentPlan ComposerAgent::plan(
    const GenerationRequest& request,
    const ComposerAgentContext& context) const {
    ComposerAgentPlan out;
    out.bpm = request.bpm > 0.0 ? request.bpm : 86.0;
    out.key = request.key.empty() ? "C minor" : request.key;
    out.mutation_amount = clamp01(request.mutation_amount);

    double space = prompt_contains(request.prompt, "negative space") || prompt_contains(request.prompt, "sparse") ? 0.78 : 0.48;
    double pacing = prompt_contains(request.prompt, "slow harmonic") ? 0.30 : 0.55;

    if (context.memory) {
        if (context.memory->has_preference_containing("negative space") || context.memory->has_preference_containing("sparse")) space = std::max(space, 0.80);
        if (context.memory->has_rejection_containing("crowded")) space = std::max(space, 0.75);
        if (const auto pref = context.memory->numeric_preference("harmonic_pacing")) pacing = pref->value;
    }

    double target_tension = 0.55;
    double target_energy = 0.65;
    if (context.perceptual_target) {
        target_tension = context.perceptual_target->tension;
        target_energy = context.perceptual_target->energy;
        space = clamp01((space + (1.0 - context.perceptual_target->density)) * 0.5);
    }
    out.global_negative_space = clamp01(space);
    out.global_harmonic_pacing = clamp01(pacing);

    const int bars = total_bars(request, out.bpm);
    if (bars < 12) {
        out.sections.push_back({"loop", bars, target_energy, target_tension, out.global_negative_space, out.global_harmonic_pacing, "motif_a"});
        out.motif_recurrence = {"motif_a"};
        validate(out);
        return out;
    }

    auto add = [&](std::string name, int section_bars, double energy, double tension, double local_space, double local_pacing, std::string motif) {
        out.sections.push_back({std::move(name), section_bars, clamp01(energy), clamp01(tension), clamp01(local_space), clamp01(local_pacing), std::move(motif)});
    };

    if (bars < 28) {
        const int intro = std::max(2, bars / 6);
        const int hook = std::max(4, bars / 3);
        const int outro = std::max(2, bars / 6);
        const int verse = std::max(4, bars - intro - hook - outro);
        add("intro", intro, target_energy * .50, target_tension * .55, space + .12, pacing * .80, "motif_a");
        add("verse", verse, target_energy * .82, target_tension * .78, space, pacing, "motif_b");
        add("hook", hook, std::min(1.0, target_energy * 1.15), std::min(1.0, target_tension * 1.10), space - .15, pacing + .12, "motif_a");
        add("outro", outro, target_energy * .48, target_tension * .42, space + .18, pacing * .75, "motif_a");
    } else {
        const int intro = std::max(4, bars / 10);
        const int hook = std::max(4, bars / 6);
        const int bridge = std::max(4, bars / 8);
        const int outro = std::max(4, bars / 10);
        const int remaining = std::max(8, bars - intro - hook * 2 - bridge - outro);
        const int verse1 = remaining / 2;
        const int verse2 = remaining - verse1;
        add("intro", intro, target_energy * .42, target_tension * .48, space + .15, pacing * .75, "motif_a");
        add("verse_1", verse1, target_energy * .78, target_tension * .72, space, pacing, "motif_b");
        add("hook_1", hook, std::min(1.0, target_energy * 1.12), std::min(1.0, target_tension * 1.08), space - .16, pacing + .12, "motif_a");
        add("verse_2", verse2, target_energy * .86, target_tension * .82, space - .04, pacing + .04, "motif_b");
        add("bridge", bridge, target_energy * .58, std::min(1.0, target_tension * 1.18), space + .20, pacing * .60, "motif_c");
        add("hook_2", hook, std::min(1.0, target_energy * 1.18), std::min(1.0, target_tension * 1.14), space - .20, pacing + .15, "motif_a");
        add("outro", outro, target_energy * .38, target_tension * .35, space + .22, pacing * .65, "motif_a");
    }

    out.motif_recurrence.clear();
    for (const auto& section : out.sections) if (!section.motif_id.empty()) out.motif_recurrence.push_back(section.motif_id);
    validate(out);
    return out;
}

void ComposerAgent::validate(const ComposerAgentPlan& plan) const {
    if (plan.bpm <= 0.0 || plan.bpm > 400.0) throw std::invalid_argument("L30 composer plan BPM invalid");
    if (plan.key.empty()) throw std::invalid_argument("L30 composer plan key missing");
    if (plan.sections.empty()) throw std::invalid_argument("L30 composer plan has no sections");
    for (const auto& s : plan.sections) {
        if (s.name.empty() || s.bars <= 0) throw std::invalid_argument("L30 composer section invalid");
        if (s.energy < 0 || s.energy > 1 || s.tension < 0 || s.tension > 1 ||
            s.negative_space < 0 || s.negative_space > 1 || s.harmonic_pacing < 0 || s.harmonic_pacing > 1)
            throw std::invalid_argument("L30 composer section scalar out of range");
    }
}

CompositionPlan ComposerAgent::to_composition_plan(const ComposerAgentPlan& plan) const {
    validate(plan);
    CompositionPlan out;
    out.bpm = plan.bpm;
    out.key = plan.key;
    out.mutation_amount = plan.mutation_amount;
    out.sections.reserve(plan.sections.size());
    for (const auto& section : plan.sections) out.sections.push_back({section.name, section.bars, section.energy});
    return out;
}

GenerationRequest ComposerAgent::compile(
    const GenerationRequest& request,
    const ComposerAgentPlan& plan) const {
    validate(plan);
    GenerationRequest out = request;
    out.bpm = plan.bpm;
    out.key = plan.key;
    out.mutation_amount = plan.mutation_amount;

    std::ostringstream form;
    form << ". COMPOSER AGENT form: ";
    for (std::size_t i = 0; i < plan.sections.size(); ++i) {
        const auto& s = plan.sections[i];
        if (i) form << " | ";
        form << s.name << ':' << s.bars << " bars"
             << ", energy=" << s.energy
             << ", tension=" << s.tension
             << ", negative_space=" << s.negative_space
             << ", harmonic_pacing=" << s.harmonic_pacing
             << ", motif=" << s.motif_id;
    }
    form << "; motif recurrence=";
    for (std::size_t i = 0; i < plan.motif_recurrence.size(); ++i) {
        if (i) form << ',';
        form << plan.motif_recurrence[i];
    }
    out.prompt += form.str();
    return out;
}

} // namespace xenon
