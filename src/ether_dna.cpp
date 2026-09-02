#include "xenon/ether_dna.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace xenon {
namespace {

std::uint64_t fnv1a64(const std::string& text) {
    std::uint64_t hash = 1469598103934665603ull;
    for (unsigned char c : text) {
        hash ^= static_cast<std::uint64_t>(c);
        hash *= 1099511628211ull;
    }
    return hash;
}

double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

std::string fingerprint_for(const GenerationRequest& request, const CompositionPlan& plan,
                            const std::string& parent) {
    std::ostringstream material;
    material << "xdna2|" << parent << '|'
             << request.prompt << '|'
             << static_cast<int>(request.mode) << '|'
             << request.seed << '|'
             << plan.bpm << '|'
             << plan.key << '|'
             << plan.mutation_amount << '|'
             << request.control.locks << '|'
             << request.control.reference_strength << '|'
             << request.control.chord_progression;
    for (const auto& section : plan.sections) {
        material << '|' << section.name << ':' << section.bars << ':' << section.energy;
    }

    const auto hash = fnv1a64(material.str());
    std::ostringstream fingerprint;
    fingerprint << "xdna2-" << std::hex << std::setw(16) << std::setfill('0') << hash;
    return fingerprint.str();
}

std::string escape(std::string value) {
    std::string out;
    out.reserve(value.size());
    for (const char c : value) {
        if (c == '\\' || c == '|' || c == '\n' || c == ':') out.push_back('\\');
        if (c == '\n') out.push_back('n');
        else out.push_back(c);
    }
    return out;
}

void append_component_ancestry(EtherDNARecord& record, const std::string& source) {
    const struct Entry { const char* name; ControlComponent flag; } entries[] = {
        {"drums", ControlComponent::Drums}, {"bass", ControlComponent::Bass},
        {"melody", ControlComponent::Melody}, {"harmony", ControlComponent::Harmony},
        {"texture", ControlComponent::Texture}, {"arrangement", ControlComponent::Arrangement}
    };
    for (const auto& entry : entries) {
        record.component_ancestry.push_back(ComponentAncestor{
            entry.name,
            source,
            has_control_component(record.locks, entry.flag)
        });
    }
}

} // namespace

EtherDNARecord EtherDNA::capture(
    const GenerationRequest& request,
    const CompositionPlan& plan,
    const std::string& parent_fingerprint) const {

    EtherDNARecord record;
    record.fingerprint = fingerprint_for(request, plan, parent_fingerprint);
    record.parent_fingerprint = parent_fingerprint;
    record.seed = request.seed;
    record.bpm = plan.bpm;
    record.key = plan.key;
    record.mutation_amount = plan.mutation_amount;
    record.locks = request.control.locks;

    record.rhythm.tempo_bpm = plan.bpm;
    record.rhythm.density = clamp01(0.5 + (plan.mutation_amount - 0.35) * 0.35);
    record.rhythm.transient_bias = clamp01(0.45 + plan.mutation_amount * 0.20);
    record.rhythm.syncopation = clamp01(plan.mutation_amount * 0.40);

    record.harmony.key = plan.key;
    record.harmony.chord_progression = request.control.chord_progression;
    record.harmony.tension = clamp01(0.35 + plan.mutation_amount * 0.45);

    record.timbre.brightness = clamp01(0.50 + plan.mutation_amount * 0.10);
    record.timbre.warmth = clamp01(0.55 - plan.mutation_amount * 0.08);
    record.timbre.grit = clamp01(0.30 + plan.mutation_amount * 0.45);
    record.timbre.stereo_width = clamp01(0.45 + plan.mutation_amount * 0.20);

    record.texture.density = clamp01(0.40 + plan.mutation_amount * 0.40);
    record.texture.noise = clamp01(plan.mutation_amount * 0.35);
    record.texture.motion = clamp01(0.25 + plan.mutation_amount * 0.50);

    for (const auto& section : plan.sections) {
        record.arrangement.push_back(ArrangementGene{section.name, section.bars, clamp01(section.energy)});
    }
    append_component_ancestry(record, parent_fingerprint);
    if (!parent_fingerprint.empty()) {
        record.mutations.push_back(MutationEvent{"generation", plan.mutation_amount, "derived from parent EtherDNA"});
    }
    return record;
}

EtherDNARecord EtherDNA::evolve(
    const EtherDNARecord& parent,
    const GenerationRequest& request,
    const CompositionPlan& plan,
    std::vector<MutationEvent> mutations) const {

    auto child = capture(request, plan, parent.fingerprint);

    // Locked genes inherit the parent's musical identity rather than being re-derived.
    if (has_control_component(request.control.locks, ControlComponent::Drums)) {
        child.rhythm = parent.rhythm;
    }
    if (has_control_component(request.control.locks, ControlComponent::Harmony)) {
        child.harmony = parent.harmony;
    }
    if (has_control_component(request.control.locks, ControlComponent::Texture)) {
        child.texture = parent.texture;
        child.timbre.grit = parent.timbre.grit;
    }
    if (has_control_component(request.control.locks, ControlComponent::Arrangement)) {
        child.arrangement = parent.arrangement;
    }

    child.component_ancestry.clear();
    append_component_ancestry(child, parent.fingerprint);
    child.mutations = std::move(mutations);
    if (child.mutations.empty()) {
        child.mutations.push_back(MutationEvent{"generation", plan.mutation_amount, "evolved from parent EtherDNA"});
    }
    return child;
}

std::string EtherDNA::serialize(const EtherDNARecord& record) const {
    std::ostringstream out;
    out << "XENON_ETHERDNA|2\n";
    out << "fingerprint|" << escape(record.fingerprint) << '\n';
    out << "parent|" << escape(record.parent_fingerprint) << '\n';
    out << "seed|" << record.seed << '\n';
    out << "bpm|" << record.bpm << '\n';
    out << "key|" << escape(record.key) << '\n';
    out << "mutation|" << record.mutation_amount << '\n';
    out << "locks|" << record.locks << '\n';
    out << "rhythm|" << record.rhythm.tempo_bpm << '|' << record.rhythm.density << '|'
        << record.rhythm.transient_bias << '|' << record.rhythm.syncopation << '\n';
    out << "harmony|" << escape(record.harmony.key) << '|' << escape(record.harmony.chord_progression)
        << '|' << record.harmony.tension << '\n';
    out << "timbre|" << record.timbre.brightness << '|' << record.timbre.warmth << '|'
        << record.timbre.grit << '|' << record.timbre.stereo_width << '\n';
    out << "texture|" << record.texture.density << '|' << record.texture.noise << '|'
        << record.texture.motion << '\n';
    for (const auto& child : record.child_fingerprints) out << "child|" << escape(child) << '\n';
    for (const auto& gene : record.arrangement) {
        out << "section|" << escape(gene.section) << '|' << gene.bars << '|' << gene.energy << '\n';
    }
    for (const auto& ancestor : record.component_ancestry) {
        out << "ancestor|" << escape(ancestor.component) << '|' << escape(ancestor.source_fingerprint)
            << '|' << (ancestor.preserved ? 1 : 0) << '\n';
    }
    for (const auto& mutation : record.mutations) {
        out << "mutation_event|" << escape(mutation.component) << '|' << mutation.amount << '|'
            << escape(mutation.note) << '\n';
    }
    return out.str();
}

} // namespace xenon
