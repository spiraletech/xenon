#include "xenon/ether_dna_store.hpp"

#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace xenon {
namespace {

std::vector<std::string> split_escaped(const std::string& line) {
    std::vector<std::string> fields;
    std::string current;
    bool escaped = false;
    for (const char c : line) {
        if (escaped) {
            current.push_back(c == 'n' ? '\n' : c);
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
        } else if (c == '|') {
            fields.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    if (escaped) current.push_back('\\');
    fields.push_back(current);
    return fields;
}

void require_fields(const std::vector<std::string>& fields, std::size_t count, const char* key) {
    if (fields.size() < count) {
        throw std::runtime_error(std::string{"Malformed EtherDNA field: "} + key);
    }
}

} // namespace

void EtherDNAStore::save(const EtherDNARecord& record, const std::filesystem::path& path) const {
    if (path.empty()) throw std::invalid_argument("EtherDNA persistence path cannot be empty");
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("Could not open EtherDNA persistence path for writing");
    out << EtherDNA{}.serialize(record);
    if (!out) throw std::runtime_error("Could not persist EtherDNA record");
}

EtherDNARecord EtherDNAStore::load(const std::filesystem::path& path) const {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Could not open EtherDNA persistence path for reading");

    std::string line;
    if (!std::getline(in, line) || line != "XENON_ETHERDNA|2") {
        throw std::runtime_error("Unsupported or malformed EtherDNA schema");
    }

    EtherDNARecord record;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        const auto fields = split_escaped(line);
        if (fields.empty()) continue;
        const auto& key = fields[0];

        if (key == "fingerprint") {
            require_fields(fields, 2, "fingerprint"); record.fingerprint = fields[1];
        } else if (key == "parent") {
            require_fields(fields, 2, "parent"); record.parent_fingerprint = fields[1];
        } else if (key == "seed") {
            require_fields(fields, 2, "seed"); record.seed = std::stoull(fields[1]);
        } else if (key == "bpm") {
            require_fields(fields, 2, "bpm"); record.bpm = std::stod(fields[1]);
        } else if (key == "key") {
            require_fields(fields, 2, "key"); record.key = fields[1];
        } else if (key == "mutation") {
            require_fields(fields, 2, "mutation"); record.mutation_amount = std::stod(fields[1]);
        } else if (key == "locks") {
            require_fields(fields, 2, "locks"); record.locks = static_cast<ControlComponents>(std::stoul(fields[1]));
        } else if (key == "rhythm") {
            require_fields(fields, 5, "rhythm");
            record.rhythm.tempo_bpm = std::stod(fields[1]);
            record.rhythm.density = std::stod(fields[2]);
            record.rhythm.transient_bias = std::stod(fields[3]);
            record.rhythm.syncopation = std::stod(fields[4]);
        } else if (key == "harmony") {
            require_fields(fields, 4, "harmony");
            record.harmony.key = fields[1];
            record.harmony.chord_progression = fields[2];
            record.harmony.tension = std::stod(fields[3]);
        } else if (key == "timbre") {
            require_fields(fields, 5, "timbre");
            record.timbre.brightness = std::stod(fields[1]);
            record.timbre.warmth = std::stod(fields[2]);
            record.timbre.grit = std::stod(fields[3]);
            record.timbre.stereo_width = std::stod(fields[4]);
        } else if (key == "texture") {
            require_fields(fields, 4, "texture");
            record.texture.density = std::stod(fields[1]);
            record.texture.noise = std::stod(fields[2]);
            record.texture.motion = std::stod(fields[3]);
        } else if (key == "child") {
            require_fields(fields, 2, "child"); record.child_fingerprints.push_back(fields[1]);
        } else if (key == "section") {
            require_fields(fields, 4, "section");
            record.arrangement.push_back(ArrangementGene{fields[1], std::stoi(fields[2]), std::stod(fields[3])});
        } else if (key == "ancestor") {
            require_fields(fields, 4, "ancestor");
            record.component_ancestry.push_back(ComponentAncestor{fields[1], fields[2], fields[3] == "1"});
        } else if (key == "mutation_event") {
            require_fields(fields, 4, "mutation_event");
            record.mutations.push_back(MutationEvent{fields[1], std::stod(fields[2]), fields[3]});
        }
    }

    if (record.fingerprint.empty()) throw std::runtime_error("EtherDNA record has no fingerprint");
    record.schema_version = 2;
    return record;
}

} // namespace xenon
