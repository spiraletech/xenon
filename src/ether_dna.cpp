#include "xenon/ether_dna.hpp"

#include <iomanip>
#include <sstream>

namespace xenon {
namespace {

std::uint64_t fnv1a64(const std::string& text) {
    std::uint64_t hash = 1469598103934665603ull;
    for (unsigned char c : text) {
        hash ^= static_cast<std::uint64_t>(c);
        hash *= 1099511628211ull;
    }
    return hash;
}

} // namespace

EtherDNARecord EtherDNA::capture(
    const GenerationRequest& request,
    const CompositionPlan& plan,
    const std::string& parent_fingerprint) const {

    std::ostringstream material;
    material << request.prompt << '|'
             << static_cast<int>(request.mode) << '|'
             << request.seed << '|'
             << plan.bpm << '|'
             << plan.key << '|'
             << plan.mutation_amount << '|'
             << request.control.locks << '|'
             << request.control.reference_strength;
    for (const auto& section : plan.sections) {
        material << '|' << section.name << ':' << section.bars << ':' << section.energy;
    }

    const auto hash = fnv1a64(material.str());
    std::ostringstream fingerprint;
    fingerprint << "xdna-" << std::hex << std::setw(16) << std::setfill('0') << hash;

    return EtherDNARecord{
        fingerprint.str(),
        parent_fingerprint,
        request.seed,
        plan.bpm,
        plan.key,
        plan.mutation_amount,
        request.control.locks
    };
}

} // namespace xenon
