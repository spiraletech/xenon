#include "xenon/self_listening_loop.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace xenon {
namespace {

double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

} // namespace

SelfListeningLoop::SelfListeningLoop(GenerationPipeline pipeline)
    : pipeline_(std::move(pipeline)) {}

ListeningCritique SelfListeningLoop::critique(const TrackAnalysis& analysis) const {
    if (analysis.frames.empty()) {
        throw std::runtime_error("XENON self-listening received no analysis frames");
    }

    ListeningProfile profile;
    for (const auto& frame : analysis.frames) {
        profile.rms += frame.rms;
        profile.brightness += frame.brightness;
        profile.spectral_density += frame.spectral_density;
        profile.transient_density += frame.transient_density;
    }

    const double count = static_cast<double>(analysis.frames.size());
    profile.rms /= count;
    profile.brightness /= count;
    profile.spectral_density /= count;
    profile.transient_density /= count;
    profile.analyzed_frames = analysis.frames.size();

    ListeningCritique out;
    out.profile = profile;

    // L21 v1 heuristic score: reward balanced energy, usable density, and controlled transients.
    const double energy = 1.0 - std::min(1.0, std::abs(profile.rms - 0.35) / 0.35);
    const double density = 1.0 - std::min(1.0, std::abs(profile.spectral_density - 0.50) / 0.50);
    const double transients = 1.0 - std::min(1.0, std::abs(profile.transient_density - 0.45) / 0.45);
    const double brightness = 1.0 - std::min(1.0, std::abs(profile.brightness - 0.50) / 0.50);
    out.score = clamp01((energy * 0.30) + (density * 0.25) + (transients * 0.25) + (brightness * 0.20));

    if (profile.transient_density > 0.68) {
        out.observations.emplace_back("transients are crowded");
        out.revision_hint = "reduce drum and hat activity while preserving the musical identity";
    } else if (profile.transient_density < 0.20) {
        out.observations.emplace_back("transients are too soft");
        out.revision_hint = "increase rhythmic definition and transient presence";
    }

    if (profile.brightness > 0.72) {
        out.observations.emplace_back("spectral balance is bright");
        if (out.revision_hint.empty()) out.revision_hint = "darken the upper spectrum slightly";
    } else if (profile.brightness < 0.22) {
        out.observations.emplace_back("spectral balance is dark");
        if (out.revision_hint.empty()) out.revision_hint = "add a little upper-frequency clarity";
    }

    if (profile.spectral_density > 0.76) {
        out.observations.emplace_back("arrangement feels spectrally dense");
        if (out.revision_hint.empty()) out.revision_hint = "create more negative space";
    }

    if (out.observations.empty()) {
        out.observations.emplace_back("spectral and transient balance are within the L21 target window");
        out.revision_hint = "preserve the current balance and make only a subtle mutation";
    }

    return out;
}

SelfListeningResult SelfListeningLoop::generate_and_listen(
    const GenerationRequest& request,
    const std::filesystem::path& output_directory,
    const std::string& parent_fingerprint) {

    SelfListeningResult result;
    result.generation = pipeline_.generate(request, output_directory, parent_fingerprint);

    if (result.generation.artifact.audio_path.empty()) {
        throw std::runtime_error("XENON backend returned no audio artifact for self-listening");
    }

    const auto analysis = analyzer_.analyzeFile(result.generation.artifact.audio_path);
    result.critique = critique(analysis);
    return result;
}

GenerationRequest SelfListeningLoop::make_revision_request(
    const GenerationRequest& previous_request,
    const ListeningCritique& critique_result,
    std::string user_feedback) const {

    auto revision = previous_request;
    revision.mode = previous_request.reference_audio.empty()
        ? GenerationMode::TextToInstrumental
        : GenerationMode::Variation;
    revision.render_intent = RenderIntent::Control;
    revision.mutation_amount = std::clamp(previous_request.mutation_amount * 0.75, 0.05, 0.80);

    if (!user_feedback.empty()) {
        revision.prompt += ". User revision: " + user_feedback;
    } else if (!critique_result.revision_hint.empty()) {
        revision.prompt += ". XENON self-critique: " + critique_result.revision_hint;
    }

    return revision;
}

SelfListeningResult SelfListeningLoop::revise(
    const GenerationRequest& previous_request,
    const SelfListeningResult& previous_result,
    const std::filesystem::path& output_directory,
    std::string user_feedback) {

    auto revision = make_revision_request(previous_request, previous_result.critique, std::move(user_feedback));
    revision.reference_audio = previous_result.generation.artifact.audio_path;

    return generate_and_listen(
        revision,
        output_directory,
        previous_result.generation.dna.fingerprint);
}

} // namespace xenon
