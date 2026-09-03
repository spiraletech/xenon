#include "xenon/session_os.hpp"

#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
}

int main() {
    try {
        xenon::SessionOS session{"spiral-spore"};
        const auto ref = session.add_reference("reference", "ref.wav");
        const auto prompt = session.add_prompt(ref, "dark ether grunge");
        const auto dna = session.add_ether_dna(prompt, "xdna2-parent");
        const auto gen = session.add_generation(dna, "generation.wav", "xdna2-child");
        const auto stem = session.add_stem(gen, "hats", "hats.wav");
        const auto revision = session.add_revision(stem, "less crowded hats", "hats-r2.wav");
        const auto mix = session.add_mix(revision, "mix.wav");
        const auto master = session.add_master(mix, "master.wav");

        auto& graph = session.graph();
        require(graph.node_count() == 8, "L37 production chain node count wrong");
        require(graph.edge_count() == 7, "L37 production chain edge count wrong");
        require(graph.head_id() == master, "L37 master is not session head");
        require(graph.lineage(master).size() == 8, "L37 master lineage incomplete");
        require(graph.parents(master).size() == 1 && graph.parents(master)[0].id == mix, "L37 master parent wrong");

        require(graph.can_undo(), "L37 undo unavailable");
        require(graph.undo() == mix, "L37 undo did not restore mix head");
        require(graph.can_redo(), "L37 redo unavailable");
        require(graph.redo() == master, "L37 redo did not restore master head");

        graph.checkout(gen);
        xenon::ProjectNode alt; alt.type=xenon::ProjectNodeType::Revision; alt.label="alternate texture"; alt.asset_uri="alt.wav";
        const auto alternate = graph.branch_from(gen, alt, xenon::ProjectEdgeType::Revises);
        require(graph.head_id() == alternate, "L37 branch did not become head");
        require(graph.children(gen).size() == 2, "L37 branch topology missing sibling");
        require(!graph.can_redo(), "L37 branch did not invalidate redo future");

        const auto serialized = graph.serialize();
        const auto restored = xenon::ProjectGraph::deserialize(serialized);
        require(restored.project_id() == graph.project_id(), "L37 project id persistence failed");
        require(restored.node_count() == graph.node_count(), "L37 node persistence failed");
        require(restored.edge_count() == graph.edge_count(), "L37 edge persistence failed");
        require(restored.head_id() == graph.head_id(), "L37 head persistence failed");
        require(restored.node(alternate).asset_uri == "alt.wav", "L37 asset persistence failed");

        bool cycle_rejected=false;
        try { auto invalid=restored; invalid.add_edge({alternate,ref,xenon::ProjectEdgeType::DerivedFrom}); }
        catch(const std::invalid_argument&) { cycle_rejected=true; }
        require(cycle_rejected, "L37 accepted cyclic project lineage");

        bool missing_rejected=false;
        try { auto invalid=restored; invalid.add_edge({"missing",alternate,xenon::ProjectEdgeType::DerivedFrom}); }
        catch(const std::invalid_argument&) { missing_rejected=true; }
        require(missing_rejected, "L37 accepted missing-node edge");

        std::cout << "L37 Project Graph / Session OS smoke passed\n";
        return 0;
    } catch(const std::exception& ex) {
        std::cerr << "L37 smoke failed: " << ex.what() << '\n';
        return 1;
    }
}
