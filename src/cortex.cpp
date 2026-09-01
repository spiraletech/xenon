#include "xenon/cortex.hpp"

#include <algorithm>
#include <cctype>

namespace xenon {
namespace {

bool contains_ci(const std::string& text, const std::string& needle) {
    auto lower = [](std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    };
    return lower(text).find(lower(needle)) != std::string::npos;
}

} // namespace

music::ProductionIntentV1 Cortex::interpret(
    const std::string& user_request,
    const music::MusicFrameV1& frame,
    const CortexContext& context) const {

    music::ProductionIntentV1 intent;
    intent.request_id = "xenon-cortex";
    intent.project_id = frame.track_id.empty() ? "xenon_project" : frame.track_id + "_xenon";
    intent.prompt = user_request;
    intent.bpm = frame.bpm > 0.0 ? frame.bpm : 86.0;
    intent.key = frame.estimated_key;
    intent.drum_density = std::clamp(0.45 + context.spectrum.transient_density * 0.25, 0.0, 1.0);
    intent.bass_weight = std::clamp(0.55 + (1.0 - context.spectrum.brightness) * 0.20, 0.0, 1.0);
    intent.vocal_space = 0.72;
    intent.texture_grit = std::clamp(0.35 + frame.warmth * 0.25, 0.0, 1.0);
    intent.transient_density = context.spectrum.transient_density;

    if (contains_ci(user_request, "colder")) intent.texture_grit = std::min(1.0, intent.texture_grit + 0.18);
    if (contains_ci(user_request, "grimy") || contains_ci(user_request, "grittier") || contains_ci(user_request, "uglier"))
        intent.texture_grit = std::min(1.0, intent.texture_grit + 0.28);
    if (contains_ci(user_request, "space") || contains_ci(user_request, "rap")) {
        intent.vocal_space = 0.88;
        intent.drum_density = std::max(0.20, intent.drum_density - 0.16);
    }
    if (contains_ci(user_request, "drums are too busy") || contains_ci(user_request, "hats are too crowded")) {
        intent.drum_density = std::max(0.15, intent.drum_density - 0.25);
        intent.keep_bass = true;
        intent.keep_melody = true;
        intent.keep_harmony = true;
        intent.keep_texture = true;
        intent.keep_arrangement = true;
    }

    intent.arrangement = {"intro", "verse", "hook", "verse", "outro"};
    return intent;
}

} // namespace xenon
