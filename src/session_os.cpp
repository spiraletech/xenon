#include "xenon/session_os.hpp"

#include <utility>

namespace xenon {

SessionOS::SessionOS(std::string project_id) : graph_(std::move(project_id)) {}
ProjectGraph& SessionOS::graph() noexcept { return graph_; }
const ProjectGraph& SessionOS::graph() const noexcept { return graph_; }

std::string SessionOS::append(const std::string& parent, ProjectNode node, ProjectEdgeType relation) {
    return graph_.branch_from(parent, std::move(node), relation);
}
std::string SessionOS::add_reference(std::string label, std::string asset_uri) {
    ProjectNode n; n.type=ProjectNodeType::Reference; n.label=std::move(label); n.asset_uri=std::move(asset_uri);
    const auto id=graph_.add_node(std::move(n)); graph_.set_head(id); return id;
}
std::string SessionOS::add_prompt(const std::string& reference_id, std::string prompt) {
    ProjectNode n; n.type=ProjectNodeType::Prompt; n.label=std::move(prompt); return append(reference_id,std::move(n),ProjectEdgeType::References);
}
std::string SessionOS::add_ether_dna(const std::string& prompt_id, std::string fingerprint) {
    ProjectNode n; n.type=ProjectNodeType::EtherDNA; n.label="EtherDNA"; n.fingerprint=std::move(fingerprint); return append(prompt_id,std::move(n),ProjectEdgeType::DerivedFrom);
}
std::string SessionOS::add_generation(const std::string& dna_id, std::string asset_uri, std::string fingerprint) {
    ProjectNode n; n.type=ProjectNodeType::Generation; n.label="Generation"; n.asset_uri=std::move(asset_uri); n.fingerprint=std::move(fingerprint); return append(dna_id,std::move(n),ProjectEdgeType::DerivedFrom);
}
std::string SessionOS::add_stem(const std::string& generation_id, std::string role, std::string asset_uri) {
    ProjectNode n; n.type=ProjectNodeType::Stem; n.label=std::move(role); n.asset_uri=std::move(asset_uri); return append(generation_id,std::move(n),ProjectEdgeType::Contains);
}
std::string SessionOS::add_revision(const std::string& source_id, std::string label, std::string asset_uri) {
    ProjectNode n; n.type=ProjectNodeType::Revision; n.label=std::move(label); n.asset_uri=std::move(asset_uri); return append(source_id,std::move(n),ProjectEdgeType::Revises);
}
std::string SessionOS::add_mix(const std::string& source_id, std::string asset_uri) {
    ProjectNode n; n.type=ProjectNodeType::Mix; n.label="Mix"; n.asset_uri=std::move(asset_uri); return append(source_id,std::move(n),ProjectEdgeType::MixedFrom);
}
std::string SessionOS::add_master(const std::string& mix_id, std::string asset_uri) {
    ProjectNode n; n.type=ProjectNodeType::Master; n.label="Master"; n.asset_uri=std::move(asset_uri); return append(mix_id,std::move(n),ProjectEdgeType::MasteredFrom);
}

} // namespace xenon
