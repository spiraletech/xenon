#include "xenon/ether_composer.hpp"
#include "xenon/ether_dna.hpp"
#include "xenon/ether_dna_store.hpp"

#include <filesystem>
#include <iostream>

namespace {

bool has_child(const xenon::EtherDNARecord& parent, const std::string& fingerprint) {
    for (const auto& child : parent.child_fingerprints) {
        if (child == fingerprint) return true;
    }
    return false;
}

bool has_preserved_ancestor(const xenon::EtherDNARecord& record, const std::string& component) {
    for (const auto& ancestor : record.component_ancestry) {
        if (ancestor.component == component && ancestor.preserved) return true;
    }
    return false;
}

} // namespace

int main() {
    namespace fs = std::filesystem;

    xenon::EtherComposer composer;
    xenon::EtherDNA dna;

    xenon::GenerationRequest parent_request;
    parent_request.prompt = "L26 parent genome";
    parent_request.seed = 260026;
    parent_request.bpm = 91.0;
    parent_request.key = "D minor";
    parent_request.mutation_amount = 0.20;
    parent_request.control.chord_progression = "Dm|Bb|F|C";

    const auto parent_plan = composer.compose(parent_request);
    auto parent = dna.capture(parent_request, parent_plan);
    parent.mutations.push_back({"origin", 0.0, "initial genome"});

    if (parent.schema_version != 2 || parent.fingerprint.rfind("xdna2-", 0) != 0 ||
        parent.arrangement.empty() || parent.component_ancestry.size() < 6) {
        std::cerr << "L26 parent genome was not fully captured\n";
        return 1;
    }

    xenon::GenerationRequest child_request = parent_request;
    child_request.prompt = "L26 child genome with brighter texture";
    child_request.seed = 260027;
    child_request.mutation_amount = 0.65;
    child_request.control.locks =
        xenon::control_component(xenon::ControlComponent::Drums) |
        xenon::ControlComponent::Harmony |
        xenon::ControlComponent::Arrangement;

    const auto child_plan = composer.compose(child_request);
    auto child = dna.evolve(
        parent,
        child_request,
        child_plan,
        {{"texture", 0.65, "increase motion and grit"}});

    if (child.parent_fingerprint != parent.fingerprint || child.fingerprint == parent.fingerprint) {
        std::cerr << "L26 child lineage fingerprint failed\n";
        return 2;
    }
    if (child.rhythm.density != parent.rhythm.density ||
        child.harmony.chord_progression != parent.harmony.chord_progression ||
        child.arrangement.size() != parent.arrangement.size()) {
        std::cerr << "L26 locked genes did not inherit from parent\n";
        return 3;
    }
    if (!has_preserved_ancestor(child, "drums") ||
        !has_preserved_ancestor(child, "harmony") ||
        !has_preserved_ancestor(child, "arrangement")) {
        std::cerr << "L26 component ancestry did not mark preserved genes\n";
        return 4;
    }
    if (child.mutations.size() != parent.mutations.size() + 1 ||
        child.mutations.back().component != "texture") {
        std::cerr << "L26 mutation history did not accumulate\n";
        return 5;
    }

    dna.register_child(parent, child);
    dna.register_child(parent, child);
    if (!has_child(parent, child.fingerprint) || parent.child_fingerprints.size() != 1) {
        std::cerr << "L26 bidirectional child registration failed\n";
        return 6;
    }

    const fs::path path = fs::temp_directory_path() / "xenon_l26" / "genome.xdna";
    fs::remove_all(path.parent_path());

    xenon::EtherDNAStore store;
    store.save(parent, path);
    const auto restored = store.load(path);

    if (restored.schema_version != 2 || restored.fingerprint != parent.fingerprint ||
        restored.child_fingerprints != parent.child_fingerprints ||
        restored.arrangement.size() != parent.arrangement.size() ||
        restored.component_ancestry.size() != parent.component_ancestry.size() ||
        restored.mutations.size() != parent.mutations.size() ||
        restored.harmony.chord_progression != parent.harmony.chord_progression) {
        std::cerr << "L26 persistent genome round-trip failed\n";
        return 7;
    }

    const auto serialized = dna.serialize(child);
    if (serialized.find("XENON_ETHERDNA|2") != 0 ||
        serialized.find("mutation_event|texture") == std::string::npos ||
        serialized.find("ancestor|drums") == std::string::npos) {
        std::cerr << "L26 serialization omitted structured genome data\n";
        return 8;
    }

    fs::remove_all(path.parent_path());
    return 0;
}
