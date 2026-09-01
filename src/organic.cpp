#include "xenon/organic.hpp"

#include <utility>

namespace xenon {

void Organic::set_project(std::string project_id) { project_id_ = std::move(project_id); }
void Organic::remember_preference(std::string preference) { preferences_.push_back(std::move(preference)); }
void Organic::remember_revision(std::string note) { revision_notes_.push_back(std::move(note)); }
const std::string& Organic::project_id() const noexcept { return project_id_; }
const std::vector<std::string>& Organic::preferences() const noexcept { return preferences_; }
const std::vector<std::string>& Organic::revision_notes() const noexcept { return revision_notes_; }

} // namespace xenon
