#include "xenon/project_graph.hpp"

#include <algorithm>
#include <iomanip>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace xenon {
namespace {
std::string type_name(ProjectNodeType t) {
    switch (t) {
        case ProjectNodeType::Reference: return "reference";
        case ProjectNodeType::Prompt: return "prompt";
        case ProjectNodeType::EtherDNA: return "etherdna";
        case ProjectNodeType::Generation: return "generation";
        case ProjectNodeType::Stem: return "stem";
        case ProjectNodeType::Revision: return "revision";
        case ProjectNodeType::Mix: return "mix";
        case ProjectNodeType::Master: return "master";
    }
    return "reference";
}
ProjectNodeType parse_type(const std::string& s) {
    if (s == "prompt") return ProjectNodeType::Prompt;
    if (s == "etherdna") return ProjectNodeType::EtherDNA;
    if (s == "generation") return ProjectNodeType::Generation;
    if (s == "stem") return ProjectNodeType::Stem;
    if (s == "revision") return ProjectNodeType::Revision;
    if (s == "mix") return ProjectNodeType::Mix;
    if (s == "master") return ProjectNodeType::Master;
    return ProjectNodeType::Reference;
}
std::string edge_name(ProjectEdgeType t) {
    switch (t) {
        case ProjectEdgeType::DerivedFrom: return "derived";
        case ProjectEdgeType::References: return "references";
        case ProjectEdgeType::Contains: return "contains";
        case ProjectEdgeType::Revises: return "revises";
        case ProjectEdgeType::MixedFrom: return "mixed";
        case ProjectEdgeType::MasteredFrom: return "mastered";
    }
    return "derived";
}
ProjectEdgeType parse_edge(const std::string& s) {
    if (s == "references") return ProjectEdgeType::References;
    if (s == "contains") return ProjectEdgeType::Contains;
    if (s == "revises") return ProjectEdgeType::Revises;
    if (s == "mixed") return ProjectEdgeType::MixedFrom;
    if (s == "mastered") return ProjectEdgeType::MasteredFrom;
    return ProjectEdgeType::DerivedFrom;
}
}

ProjectGraph::ProjectGraph(std::string project_id) : project_id_(std::move(project_id)) {
    if (project_id_.empty()) project_id_ = "xenon-project";
}
const std::string& ProjectGraph::project_id() const noexcept { return project_id_; }
const std::string& ProjectGraph::head_id() const noexcept { return head_id_; }
std::size_t ProjectGraph::node_count() const noexcept { return nodes_.size(); }
std::size_t ProjectGraph::edge_count() const noexcept { return edges_.size(); }
bool ProjectGraph::has_node(const std::string& id) const noexcept { return nodes_.find(id) != nodes_.end(); }

std::string ProjectGraph::add_node(ProjectNode n) {
    if (n.id.empty()) n.id = "node-" + std::to_string(next_id_++);
    if (has_node(n.id)) throw std::invalid_argument("L37 duplicate project node id");
    if (n.label.empty()) n.label = type_name(n.type);
    insertion_order_.push_back(n.id);
    nodes_.emplace(n.id, std::move(n));
    if (head_id_.empty()) {
        head_id_ = insertion_order_.back();
        push_history(head_id_);
    }
    return insertion_order_.back();
}

bool ProjectGraph::would_cycle(const std::string& from, const std::string& to) const {
    if (from == to) return true;
    std::queue<std::string> q;
    std::unordered_set<std::string> seen;
    q.push(to);
    while (!q.empty()) {
        auto cur = q.front(); q.pop();
        if (!seen.insert(cur).second) continue;
        if (cur == from) return true;
        for (const auto& e : edges_) if (e.from == cur) q.push(e.to);
    }
    return false;
}

void ProjectGraph::add_edge(ProjectEdge e) {
    if (!has_node(e.from) || !has_node(e.to)) throw std::invalid_argument("L37 edge references missing node");
    if (would_cycle(e.from, e.to)) throw std::invalid_argument("L37 project graph must remain acyclic");
    const auto duplicate = std::find_if(edges_.begin(), edges_.end(), [&](const ProjectEdge& x) {
        return x.from == e.from && x.to == e.to && x.type == e.type;
    });
    if (duplicate == edges_.end()) edges_.push_back(std::move(e));
}

void ProjectGraph::push_history(const std::string& id) {
    if (!history_.empty() && history_[history_index_] == id) return;
    if (!history_.empty() && history_index_ + 1 < history_.size()) history_.erase(history_.begin() + static_cast<std::ptrdiff_t>(history_index_ + 1), history_.end());
    history_.push_back(id);
    history_index_ = history_.size() - 1;
}
void ProjectGraph::set_head(const std::string& id) {
    if (!has_node(id)) throw std::out_of_range("L37 head node missing");
    head_id_ = id; push_history(id);
}
const ProjectNode& ProjectGraph::node(const std::string& id) const {
    const auto it = nodes_.find(id); if (it == nodes_.end()) throw std::out_of_range("L37 project node missing"); return it->second;
}
std::vector<ProjectNode> ProjectGraph::children(const std::string& id) const {
    (void)node(id); std::vector<ProjectNode> out;
    for (const auto& e : edges_) if (e.from == id) out.push_back(node(e.to));
    return out;
}
std::vector<ProjectNode> ProjectGraph::parents(const std::string& id) const {
    (void)node(id); std::vector<ProjectNode> out;
    for (const auto& e : edges_) if (e.to == id) out.push_back(node(e.from));
    return out;
}
std::vector<ProjectNode> ProjectGraph::lineage(const std::string& id) const {
    (void)node(id); std::vector<ProjectNode> out; std::unordered_set<std::string> seen; std::queue<std::string> q; q.push(id);
    while (!q.empty()) { auto cur=q.front(); q.pop(); if (!seen.insert(cur).second) continue; out.push_back(node(cur)); for (const auto& e:edges_) if(e.to==cur) q.push(e.from); }
    return out;
}
bool ProjectGraph::can_undo() const noexcept { return !history_.empty() && history_index_ > 0; }
bool ProjectGraph::can_redo() const noexcept { return !history_.empty() && history_index_ + 1 < history_.size(); }
std::string ProjectGraph::undo() { if (!can_undo()) throw std::runtime_error("L37 no undo state"); head_id_=history_[--history_index_]; return head_id_; }
std::string ProjectGraph::redo() { if (!can_redo()) throw std::runtime_error("L37 no redo state"); head_id_=history_[++history_index_]; return head_id_; }
std::string ProjectGraph::checkout(const std::string& id) { set_head(id); return head_id_; }
std::string ProjectGraph::branch_from(const std::string& parent, ProjectNode n, ProjectEdgeType relation) {
    (void)node(parent); const auto id=add_node(std::move(n)); add_edge({parent,id,relation}); set_head(id); return id;
}

ProjectGraphSnapshot ProjectGraph::snapshot() const {
    ProjectGraphSnapshot s; s.project_id=project_id_; s.head_id=head_id_; s.edges=edges_;
    for (const auto& id:insertion_order_) s.nodes.push_back(node(id));
    return s;
}
std::string ProjectGraph::serialize() const {
    std::ostringstream o; o << "XENON_PROJECT_GRAPH|1\n" << std::quoted(project_id_) << ' ' << std::quoted(head_id_) << ' ' << next_id_ << '\n';
    o << insertion_order_.size() << '\n';
    for (const auto& id:insertion_order_) { const auto& n=node(id); o << std::quoted(n.id)<<' '<<type_name(n.type)<<' '<<std::quoted(n.label)<<' '<<std::quoted(n.asset_uri)<<' '<<std::quoted(n.fingerprint)<<' '<<n.revision<<' '<<n.active<<'\n'; }
    o << edges_.size() << '\n'; for(const auto& e:edges_) o<<std::quoted(e.from)<<' '<<std::quoted(e.to)<<' '<<edge_name(e.type)<<'\n';
    return o.str();
}
ProjectGraph ProjectGraph::deserialize(const std::string& text) {
    std::istringstream in(text); std::string header; std::getline(in,header); if(header!="XENON_PROJECT_GRAPH|1") throw std::invalid_argument("L37 unsupported project graph schema");
    std::string project,head; std::uint64_t next=1; if(!(in>>std::quoted(project)>>std::quoted(head)>>next)) throw std::invalid_argument("L37 corrupt graph header");
    ProjectGraph g(project); g.next_id_=next; std::size_t count=0; in>>count;
    for(std::size_t i=0;i<count;++i){ ProjectNode n; std::string t; if(!(in>>std::quoted(n.id)>>t>>std::quoted(n.label)>>std::quoted(n.asset_uri)>>std::quoted(n.fingerprint)>>n.revision>>n.active)) throw std::invalid_argument("L37 corrupt graph node"); n.type=parse_type(t); g.add_node(std::move(n)); }
    std::size_t ec=0; in>>ec; for(std::size_t i=0;i<ec;++i){ ProjectEdge e; std::string t; if(!(in>>std::quoted(e.from)>>std::quoted(e.to)>>t)) throw std::invalid_argument("L37 corrupt graph edge"); e.type=parse_edge(t); g.add_edge(std::move(e)); }
    if(!head.empty()) g.checkout(head); return g;
}

} // namespace xenon
