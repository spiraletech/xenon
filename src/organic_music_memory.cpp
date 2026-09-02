#include "xenon/organic_music_memory.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace xenon {
namespace {

double clamp01(double value) { return std::clamp(value, 0.0, 1.0); }

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string encode(const std::string& value) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (unsigned char c : value) out << std::setw(2) << static_cast<int>(c);
    return out.str();
}

std::string decode(const std::string& value) {
    if (value.size() % 2 != 0) throw std::runtime_error("L27 memory contains invalid encoded string");
    std::string out;
    out.reserve(value.size() / 2);
    for (std::size_t i = 0; i < value.size(); i += 2) {
        unsigned int byte = 0;
        std::istringstream in(value.substr(i, 2));
        in >> std::hex >> byte;
        if (!in) throw std::runtime_error("L27 memory contains invalid encoded byte");
        out.push_back(static_cast<char>(byte));
    }
    return out;
}

std::vector<std::string> split(const std::string& line, char delimiter = '|') {
    std::vector<std::string> parts;
    std::string item;
    std::istringstream in(line);
    while (std::getline(in, item, delimiter)) parts.push_back(item);
    return parts;
}

int verdict_value(MemoryVerdict verdict) { return static_cast<int>(verdict); }

MemoryVerdict verdict_from(int value) {
    if (value == 1) return MemoryVerdict::Accepted;
    if (value == 2) return MemoryVerdict::Rejected;
    return MemoryVerdict::Neutral;
}

const char* verdict_name(MemoryVerdict verdict) {
    switch (verdict) {
    case MemoryVerdict::Accepted: return "accepted";
    case MemoryVerdict::Rejected: return "rejected";
    case MemoryVerdict::Neutral: return "neutral";
    }
    return "neutral";
}

} // namespace

void OrganicMusicMemory::set_project(std::string project_id) {
    if (project_id.empty()) throw std::invalid_argument("L27 project id cannot be empty");
    memory_.project_id = std::move(project_id);
}

const std::string& OrganicMusicMemory::project_id() const noexcept { return memory_.project_id; }

void OrganicMusicMemory::remember_preference(std::string text, double strength) {
    if (text.empty()) return;
    memory_.preferences.push_back({std::move(text), clamp01(strength)});
}

void OrganicMusicMemory::remember_rejection(std::string text, double strength) {
    if (text.empty()) return;
    memory_.rejections.push_back({std::move(text), clamp01(strength)});
}

void OrganicMusicMemory::set_numeric_preference(std::string key, double value, double confidence) {
    if (key.empty()) throw std::invalid_argument("L27 numeric preference key cannot be empty");
    auto it = std::find_if(memory_.numeric_preferences.begin(), memory_.numeric_preferences.end(),
        [&](const NumericPreference& item) { return item.key == key; });
    NumericPreference next{std::move(key), clamp01(value), clamp01(confidence)};
    if (it == memory_.numeric_preferences.end()) memory_.numeric_preferences.push_back(std::move(next));
    else *it = std::move(next);
}

void OrganicMusicMemory::remember_revision(RevisionMemory revision) {
    memory_.revisions.push_back(std::move(revision));
}

void OrganicMusicMemory::protect_motif(ProtectedMotif motif) {
    if (motif.motif_id.empty()) throw std::invalid_argument("L27 protected motif id cannot be empty");
    auto it = std::find_if(memory_.protected_motifs.begin(), memory_.protected_motifs.end(),
        [&](const ProtectedMotif& item) { return item.motif_id == motif.motif_id; });
    if (it == memory_.protected_motifs.end()) memory_.protected_motifs.push_back(std::move(motif));
    else *it = std::move(motif);
}

std::optional<NumericPreference> OrganicMusicMemory::numeric_preference(const std::string& key) const {
    const auto it = std::find_if(memory_.numeric_preferences.begin(), memory_.numeric_preferences.end(),
        [&](const NumericPreference& item) { return item.key == key; });
    if (it == memory_.numeric_preferences.end()) return std::nullopt;
    return *it;
}

bool OrganicMusicMemory::has_preference_containing(const std::string& token) const {
    const auto needle = lower(token);
    return std::any_of(memory_.preferences.begin(), memory_.preferences.end(), [&](const TasteMemory& item) {
        return lower(item.text).find(needle) != std::string::npos;
    });
}

bool OrganicMusicMemory::has_rejection_containing(const std::string& token) const {
    const auto needle = lower(token);
    return std::any_of(memory_.rejections.begin(), memory_.rejections.end(), [&](const TasteMemory& item) {
        return lower(item.text).find(needle) != std::string::npos;
    });
}

std::vector<std::string> OrganicMusicMemory::recall_context(std::size_t max_items) const {
    std::vector<std::string> out;
    auto add = [&](std::string value) {
        if (out.size() < max_items) out.push_back(std::move(value));
    };
    for (auto it = memory_.preferences.rbegin(); it != memory_.preferences.rend() && out.size() < max_items; ++it)
        add("prefer: " + it->text);
    for (auto it = memory_.rejections.rbegin(); it != memory_.rejections.rend() && out.size() < max_items; ++it)
        add("avoid: " + it->text);
    for (auto it = memory_.numeric_preferences.rbegin(); it != memory_.numeric_preferences.rend() && out.size() < max_items; ++it) {
        std::ostringstream item;
        item << "target " << it->key << '=' << it->value << " confidence=" << it->confidence;
        add(item.str());
    }
    for (auto it = memory_.protected_motifs.rbegin(); it != memory_.protected_motifs.rend() && out.size() < max_items; ++it)
        add("protect motif " + it->motif_id + ": " + it->description);
    for (auto it = memory_.revisions.rbegin(); it != memory_.revisions.rend() && out.size() < max_items; ++it) {
        if (it->verdict == MemoryVerdict::Neutral) continue;
        add(std::string{"revision "} + verdict_name(it->verdict) + ": " + it->note);
    }
    return out;
}

const ProjectMemory& OrganicMusicMemory::state() const noexcept { return memory_; }

GenerationRequest OrganicMusicMemory::apply_to(GenerationRequest request) const {
    if (const auto mutation = numeric_preference("mutation_amount")) {
        request.mutation_amount = request.mutation_amount * (1.0 - mutation->confidence)
            + mutation->value * mutation->confidence;
    }
    if (const auto reference = numeric_preference("reference_strength")) {
        request.control.reference_strength = request.control.reference_strength * (1.0 - reference->confidence)
            + reference->value * reference->confidence;
    }

    const auto context = recall_context(12);
    if (!context.empty()) {
        request.prompt += ". ORGANIC project memory: ";
        for (std::size_t i = 0; i < context.size(); ++i) {
            if (i) request.prompt += "; ";
            request.prompt += context[i];
        }
    }
    return request;
}

void OrganicMusicMemory::save(const std::filesystem::path& path) const {
    if (memory_.project_id.empty()) throw std::runtime_error("L27 cannot save memory without project id");
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::trunc);
    if (!out) throw std::runtime_error("L27 failed to open memory store for writing");
    out << "XENON_ORGANIC_MUSIC|1\n";
    out << "project|" << encode(memory_.project_id) << '\n';
    for (const auto& item : memory_.preferences)
        out << "prefer|" << item.strength << '|' << encode(item.text) << '\n';
    for (const auto& item : memory_.rejections)
        out << "reject|" << item.strength << '|' << encode(item.text) << '\n';
    for (const auto& item : memory_.numeric_preferences)
        out << "numeric|" << encode(item.key) << '|' << item.value << '|' << item.confidence << '\n';
    for (const auto& item : memory_.revisions)
        out << "revision|" << encode(item.fingerprint) << '|' << encode(item.parent_fingerprint) << '|'
            << verdict_value(item.verdict) << '|' << item.score << '|' << encode(item.note) << '\n';
    for (const auto& item : memory_.protected_motifs)
        out << "motif|" << encode(item.motif_id) << '|' << encode(item.description) << '|'
            << encode(item.source_fingerprint) << '\n';
}

void OrganicMusicMemory::load(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("L27 failed to open memory store for reading");
    std::string line;
    if (!std::getline(in, line) || line != "XENON_ORGANIC_MUSIC|1")
        throw std::runtime_error("L27 unsupported ORGANIC music memory schema");

    ProjectMemory loaded;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        const auto parts = split(line);
        if (parts.empty()) continue;
        if (parts[0] == "project" && parts.size() == 2) loaded.project_id = decode(parts[1]);
        else if (parts[0] == "prefer" && parts.size() == 3) loaded.preferences.push_back({decode(parts[2]), std::stod(parts[1])});
        else if (parts[0] == "reject" && parts.size() == 3) loaded.rejections.push_back({decode(parts[2]), std::stod(parts[1])});
        else if (parts[0] == "numeric" && parts.size() == 4) loaded.numeric_preferences.push_back({decode(parts[1]), std::stod(parts[2]), std::stod(parts[3])});
        else if (parts[0] == "revision" && parts.size() == 6) loaded.revisions.push_back({decode(parts[1]), decode(parts[2]), decode(parts[5]), verdict_from(std::stoi(parts[3])), std::stod(parts[4])});
        else if (parts[0] == "motif" && parts.size() == 4) loaded.protected_motifs.push_back({decode(parts[1]), decode(parts[2]), decode(parts[3])});
        else throw std::runtime_error("L27 malformed ORGANIC music memory record");
    }
    if (loaded.project_id.empty()) throw std::runtime_error("L27 memory store is missing project id");
    memory_ = std::move(loaded);
}

} // namespace xenon
