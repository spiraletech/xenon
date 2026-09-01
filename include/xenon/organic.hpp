#pragma once

#include <string>
#include <vector>

namespace xenon {

class Organic {
public:
    void set_project(std::string project_id);
    void remember_preference(std::string preference);
    void remember_revision(std::string note);

    [[nodiscard]] const std::string& project_id() const noexcept;
    [[nodiscard]] const std::vector<std::string>& preferences() const noexcept;
    [[nodiscard]] const std::vector<std::string>& revision_notes() const noexcept;

private:
    std::string project_id_;
    std::vector<std::string> preferences_;
    std::vector<std::string> revision_notes_;
};

} // namespace xenon
