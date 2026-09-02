#include "xenon/stem_engine.hpp"

#include <stdexcept>
#include <utility>

namespace xenon {

StemArtifact* StemEngine::find(StemSet& set, StemRole role) const noexcept {
    for (auto& stem : set.stems) if (stem.role == role) return &stem;
    return nullptr;
}

const StemArtifact* StemEngine::find(const StemSet& set, StemRole role) const noexcept {
    for (const auto& stem : set.stems) if (stem.role == role) return &stem;
    return nullptr;
}

StemSet StemEngine::import_stems(
    std::string stem_set_id,
    std::filesystem::path source_mix,
    std::vector<ImportedStem> stems,
    std::string ether_dna_fingerprint,
    std::string parent_ether_dna_fingerprint) const {

    if (stem_set_id.empty()) throw std::invalid_argument("XENON stem set id cannot be empty");
    if (stems.empty()) throw std::invalid_argument("XENON requires at least one imported stem");

    StemSet set;
    set.stem_set_id = std::move(stem_set_id);
    set.source_mix = std::move(source_mix);
    set.ether_dna_fingerprint = std::move(ether_dna_fingerprint);
    set.parent_ether_dna_fingerprint = std::move(parent_ether_dna_fingerprint);

    for (std::size_t i = 0; i < stems.size(); ++i) {
        if (stems[i].audio_path.empty() || !std::filesystem::exists(stems[i].audio_path))
            throw std::invalid_argument("XENON imported stem path does not exist");
        if (stems[i].role != StemRole::Unknown && find(set, stems[i].role))
            throw std::invalid_argument("XENON stem set cannot contain duplicate explicit roles");

        StemArtifact artifact;
        artifact.stem_id = set.stem_set_id + ":" + std::to_string(i + 1);
        artifact.role = stems[i].role;
        artifact.audio_path = std::move(stems[i].audio_path);
        artifact.source_backend = std::move(stems[i].source_backend);
        set.stems.push_back(std::move(artifact));
    }

    analyze(set);
    return set;
}

StemSet StemEngine::separate(
    IStemSeparator& separator,
    const std::filesystem::path& source_mix,
    const std::filesystem::path& output_directory,
    std::string ether_dna_fingerprint,
    std::string parent_ether_dna_fingerprint) const {

    if (source_mix.empty() || !std::filesystem::exists(source_mix))
        throw std::invalid_argument("XENON source mix does not exist");

    auto set = separator.separate(
        source_mix,
        output_directory,
        std::move(ether_dna_fingerprint),
        std::move(parent_ether_dna_fingerprint));
    if (set.stems.empty()) throw std::runtime_error("XENON stem separator returned no stems");
    analyze(set);
    return set;
}

void StemEngine::analyze(StemSet& set) const {
    for (auto& stem : set.stems) {
        stem.analysis = analyzer_.analyze(stem.audio_path);
        if (stem.role == StemRole::Unknown) stem.role = stem.analysis.inferred_role;
    }
}

void StemEngine::set_locked(StemSet& set, StemRole role, bool locked) const {
    auto* stem = find(set, role);
    if (!stem) throw std::invalid_argument("XENON cannot lock a missing stem role");
    stem->locked = locked;
}

bool StemEngine::is_locked(const StemSet& set, StemRole role) const noexcept {
    const auto* stem = find(set, role);
    return stem && stem->locked;
}

ReconstructionPlan StemEngine::replace(
    StemSet& set,
    const StemReplacementRequest& request) const {

    if (request.target == StemRole::Unknown)
        throw std::invalid_argument("XENON replacement requires an explicit target stem role");
    if (request.replacement_audio.empty() || !std::filesystem::exists(request.replacement_audio))
        throw std::invalid_argument("XENON replacement stem path does not exist");

    auto* target = find(set, request.target);
    if (!target) throw std::invalid_argument("XENON replacement target is missing from stem set");
    if (target->locked) throw std::runtime_error("XENON refuses to replace a locked stem");

    target->audio_path = request.replacement_audio;
    target->source_backend = request.source_backend;
    target->analysis = analyzer_.analyze(target->audio_path);

    ReconstructionPlan plan;
    plan.source_stem_set_id = set.stem_set_id;
    plan.output_stem_set_id = set.stem_set_id + ":revision";
    plan.replaced_role = request.target;

    for (const auto& stem : set.stems) {
        plan.ordered_stems.push_back(stem.audio_path);
        plan.roles.push_back(stem.role);
        if (stem.locked) plan.preserved_locked_roles.push_back(stem.role);
    }

    return plan;
}

} // namespace xenon
