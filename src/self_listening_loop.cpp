#include "xenon/self_listening_loop.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace xenon {

SelfListeningLoop::SelfListeningLoop(GenerationPipeline pipeline)
    : pipeline_(std::move(pipeline)) {}

ListeningProfile SelfListeningLoop::profile(const TrackAnalysis& analysis) const {
    if (analysis.frames.empty()) {
        throw std::runtime_error("XENON self-listening received no analysis frames");
    }

    ListeningProfile out;
    for (const auto& frame : analysis.frames) {
        out.rms += frame.rms;
        out.brightness += frame.brightness;
        out.spectral_density += frame.spectral_density;
        out.transient_density += frame.transient_density;
    }

    const double count = static_cast<double>(analysis.frames.size());
    out.rms /= count;
    out.brightness /= count;
    out.spectral_density /= count;
    out.transient_density /= count;
    out.analyzed_frames = analysis.frames.size();
    return out;
}

ListeningCritique SelfListeningLoop::critique(const TrackAnalysis& analysis) const {
    ListeningCritique out;
    out.profile = profile(analysis);
    out.synesthesia = synesthesia_.score(analysis);
    const auto cortex = critic_.critique(analysis, out.synesthesia);
    out.score = cortex.score;
    out.observations = cortex.observations;
    out.revision_hint = cortex.revision_hint;
    return out;
}

void SelfListeningLoop::remember(const SelfListeningResult& result, const std::string& note) {
    organic_.set_project("xenon_self_listening");
    std::ostringstream entry;
    entry << note
          << " | dna=" << result.generation.dna.fingerprint
          << " | score=" << result.critique.score
          << " | backend=" << result.generation.artifact.backend_name;
    organic_.remember_revision(entry.str());
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
    remember(result, "heard generation");
    return result;
}

GenerationRequest SelfListeningLoop::make_revision_request(
    const GenerationRequest& previous_request,
    const ListeningCritique& critique_result,
    std::string user_feedback) const {

    return revision_compiler_.compile(
        previous_request,
        critique_result.revision_hint,
        std::move(user_feedback));
}

SelfListeningResult SelfListeningLoop::revise(
    const GenerationRequest& previous_request,
    const SelfListeningResult& previous_result,
    const std::filesystem::path& output_directory,
    std::string user_feedback) {

    const std::string feedback_note = user_feedback;
    auto revision = make_revision_request(previous_request, previous_result.critique, std::move(user_feedback));
    revision.reference_audio = previous_result.generation.artifact.audio_path;

    auto generate_evolved_and_listen = [&](const GenerationRequest& request, const std::string& note) {
        SelfListeningResult result;
        std::vector<MutationEvent> mutations;
        mutations.push_back(MutationEvent{
            "revision",
            request.mutation_amount,
            feedback_note.empty() ? previous_result.critique.revision_hint : feedback_note
        });
        result.generation = pipeline_.generate_evolved(
            request, output_directory, previous_result.generation.dna, std::move(mutations));
        if (result.generation.artifact.audio_path.empty()) {
            throw std::runtime_error("XENON backend returned no audio artifact for evolved revision");
        }
        const auto analysis = analyzer_.analyzeFile(result.generation.artifact.audio_path);
        result.critique = critique(analysis);
        remember(result, note);
        return result;
    };

    try {
        return generate_evolved_and_listen(revision, "controlled revision completed");
    } catch (const std::runtime_error&) {
        // Preserve EtherDNA inheritance even when degrading to a backend that cannot
        // perform reference-conditioned control.
        revision.mode = GenerationMode::TextToInstrumental;
        revision.render_intent = RenderIntent::Quality;
        revision.reference_audio.clear();
        revision.control = {};
        return generate_evolved_and_listen(revision, "revision fallback completed");
    }
}

RevisionCycleResult SelfListeningLoop::run_revision_cycle(
    const GenerationRequest& request,
    const std::filesystem::path& output_directory,
    std::string user_feedback) {

    RevisionCycleResult cycle;
    cycle.original = generate_and_listen(request, output_directory);
    cycle.revision_request = make_revision_request(request, cycle.original.critique, user_feedback);

    auto controlled = cycle.revision_request;
    controlled.reference_audio = cycle.original.generation.artifact.audio_path;

    const std::string feedback_note = user_feedback;
    auto generate_evolved_and_listen = [&](const GenerationRequest& revision, const std::string& note) {
        SelfListeningResult result;
        result.generation = pipeline_.generate_evolved(
            revision,
            output_directory,
            cycle.original.generation.dna,
            {MutationEvent{
                "revision",
                revision.mutation_amount,
                feedback_note.empty() ? cycle.original.critique.revision_hint : feedback_note
            }});
        const auto analysis = analyzer_.analyzeFile(result.generation.artifact.audio_path);
        result.critique = critique(analysis);
        remember(result, note);
        return result;
    };

    try {
        cycle.revision = generate_evolved_and_listen(controlled, "revision cycle controlled pass");
        cycle.used_controlled_revision = true;
    } catch (const std::runtime_error&) {
        auto fallback = cycle.revision_request;
        fallback.mode = GenerationMode::TextToInstrumental;
        fallback.render_intent = RenderIntent::Quality;
        fallback.reference_audio.clear();
        fallback.control = {};
        cycle.revision = generate_evolved_and_listen(fallback, "revision cycle fallback pass");
        cycle.used_controlled_revision = false;
    }

    return cycle;
}

const Organic& SelfListeningLoop::memory() const noexcept {
    return organic_;
}

} // namespace xenon
