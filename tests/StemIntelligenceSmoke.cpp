#include "xenon/engine.hpp"
#include "xenon/stem_engine.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::filesystem::path render_stem(
    xenon::Engine& engine,
    const std::filesystem::path& output,
    const std::string& project,
    std::uint64_t seed,
    double drum_density,
    double bass_weight,
    double texture_grit) {

    xenon::music::ProductionIntentV1 intent;
    intent.request_id = project;
    intent.project_id = project;
    intent.prompt = project;
    intent.duration_seconds = 2.0;
    intent.seed = seed;
    intent.drum_density = drum_density;
    intent.bass_weight = bass_weight;
    intent.texture_grit = texture_grit;
    return engine.render(intent, output).audio_path;
}

class FakeSeparator final : public xenon::IStemSeparator {
public:
    explicit FakeSeparator(std::vector<xenon::ImportedStem> stems)
        : stems_(std::move(stems)) {}

    std::string_view name() const noexcept override { return "l24-fake-separator"; }

    xenon::StemSet separate(
        const std::filesystem::path& source_mix,
        const std::filesystem::path&,
        std::string dna,
        std::string parent) override {

        xenon::StemSet set;
        set.stem_set_id = "separator-set";
        set.source_mix = source_mix;
        set.ether_dna_fingerprint = std::move(dna);
        set.parent_ether_dna_fingerprint = std::move(parent);
        for (std::size_t i = 0; i < stems_.size(); ++i) {
            xenon::StemArtifact stem;
            stem.stem_id = "sep:" + std::to_string(i + 1);
            stem.role = stems_[i].role;
            stem.audio_path = stems_[i].audio_path;
            stem.source_backend = std::string{name()};
            set.stems.push_back(std::move(stem));
        }
        return set;
    }

private:
    std::vector<xenon::ImportedStem> stems_;
};

bool contains_role(const std::vector<xenon::StemRole>& roles, xenon::StemRole role) {
    for (const auto value : roles) if (value == role) return true;
    return false;
}

} // namespace

int main() {
    namespace fs = std::filesystem;
    const fs::path output = fs::temp_directory_path() / "xenon_l24_stem_smoke";
    fs::remove_all(output);
    fs::create_directories(output);

    xenon::Engine renderer;
    const auto mix = render_stem(renderer, output, "mix", 240000, 0.55, 0.55, 0.30);
    const auto drums = render_stem(renderer, output, "drums", 240001, 0.95, 0.05, 0.05);
    const auto bass = render_stem(renderer, output, "bass", 240002, 0.05, 0.95, 0.05);
    const auto melody = render_stem(renderer, output, "melody", 240003, 0.10, 0.15, 0.10);
    const auto harmony = render_stem(renderer, output, "harmony", 240004, 0.10, 0.35, 0.20);
    const auto drums_v2 = render_stem(renderer, output, "drums-v2", 240099, 0.65, 0.05, 0.10);

    xenon::StemEngine engine;
    std::vector<xenon::ImportedStem> imported{
        {xenon::StemRole::Drums, drums, "fixture"},
        {xenon::StemRole::Bass, bass, "fixture"},
        {xenon::StemRole::Melody, melody, "fixture"},
        {xenon::StemRole::Harmony, harmony, "fixture"}
    };

    auto set = engine.import_stems("l24-import", mix, imported, "dna-child", "dna-parent");
    if (set.stems.size() != 4 || set.ether_dna_fingerprint != "dna-child" ||
        set.parent_ether_dna_fingerprint != "dna-parent") {
        std::cerr << "L24 import or lineage failed\n";
        return 1;
    }
    for (const auto& stem : set.stems) {
        if (stem.analysis.role_confidence < 0.0 || stem.analysis.role_confidence > 1.0 ||
            stem.analysis.synesthesia.overall < 0.0 || stem.analysis.synesthesia.overall > 1.0) {
            std::cerr << "L24 per-stem analysis escaped normalized range\n";
            return 2;
        }
    }

    engine.set_locked(set, xenon::StemRole::Bass, true);
    engine.set_locked(set, xenon::StemRole::Melody, true);
    if (!engine.is_locked(set, xenon::StemRole::Bass) || !engine.is_locked(set, xenon::StemRole::Melody)) {
        std::cerr << "L24 stem locks failed\n";
        return 3;
    }

    const auto plan = engine.replace(set, {xenon::StemRole::Drums, drums_v2, "replacement-backend"});
    if (plan.replaced_role != xenon::StemRole::Drums || plan.ordered_stems.size() != 4 ||
        !contains_role(plan.preserved_locked_roles, xenon::StemRole::Bass) ||
        !contains_role(plan.preserved_locked_roles, xenon::StemRole::Melody)) {
        std::cerr << "L24 reconstruction plan failed to preserve locks\n";
        return 4;
    }

    bool rejected_locked = false;
    try {
        (void)engine.replace(set, {xenon::StemRole::Bass, drums_v2, "illegal"});
    } catch (const std::runtime_error&) {
        rejected_locked = true;
    }
    if (!rejected_locked) {
        std::cerr << "L24 allowed replacement of locked bass\n";
        return 5;
    }

    if (xenon::stem_role_control_component(xenon::StemRole::Drums) != xenon::ControlComponent::Drums ||
        xenon::stem_role_control_component(xenon::StemRole::Harmony) != xenon::ControlComponent::Harmony) {
        std::cerr << "L24 generation-control bridge failed\n";
        return 6;
    }

    FakeSeparator separator{imported};
    const auto separated = engine.separate(separator, mix, output, "sep-dna", "sep-parent");
    if (separated.stems.size() != 4 || separated.ether_dna_fingerprint != "sep-dna" ||
        separated.stems.front().source_backend != "l24-fake-separator") {
        std::cerr << "L24 separator backend contract failed\n";
        return 7;
    }

    fs::remove_all(output);
    return 0;
}
