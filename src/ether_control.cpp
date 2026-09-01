#include "xenon/ether_control.hpp"

#include <algorithm>
#include <stdexcept>

namespace xenon {

ControlCompileResult EtherControl::compile(const GenerationRequest& input) const {
    GenerationRequest request = input;

    if (request.prompt.empty()) throw std::invalid_argument("Generation prompt cannot be empty");
    if (request.duration_seconds <= 0.0 || request.duration_seconds > 600.0)
        throw std::invalid_argument("Generation duration must be within (0, 600] seconds");
    if (request.bpm < 0.0 || request.bpm > 400.0)
        throw std::invalid_argument("BPM must be zero (auto) or within a sensible range");

    request.mutation_amount = std::clamp(request.mutation_amount, 0.0, 1.0);
    request.control.reference_strength = std::clamp(request.control.reference_strength, 0.0, 1.0);

    const bool control_mode = request.mode != GenerationMode::TextToInstrumental;
    const bool uses_reference = !request.reference_audio.empty();
    const bool uses_locks = request.control.locks != 0;
    const bool uses_temporal = request.mode == GenerationMode::ReplaceSection ||
        request.control.edit_start_seconds >= 0.0 || request.control.edit_end_seconds >= 0.0;

    if (control_mode && !uses_reference)
        throw std::invalid_argument("Control generation modes require reference audio");

    if (request.mode == GenerationMode::ReplaceSection) {
        if (request.control.edit_start_seconds < 0.0 || request.control.edit_end_seconds <= request.control.edit_start_seconds)
            throw std::invalid_argument("ReplaceSection requires a valid edit time range");
    }

    if (request.control.edit_start_seconds >= 0.0 && request.control.edit_end_seconds >= 0.0 &&
        request.control.edit_end_seconds <= request.control.edit_start_seconds)
        throw std::invalid_argument("Control edit end must be greater than edit start");

    return ControlCompileResult{request, uses_reference, uses_locks, uses_temporal};
}

} // namespace xenon
