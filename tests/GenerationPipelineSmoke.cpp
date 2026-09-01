#include "xenon/generation_pipeline.hpp"

#include <cassert>
#include <filesystem>
#include <memory>
#include <string_view>

namespace {

class FakeBackend final : public xenon::IModelBackend {
public:
    [[nodiscard]] std::string_view name() const noexcept override { return "fake-control"; }
    [[nodiscard]] xenon::ProviderCapabilities capabilities() const noexcept override {
        return xenon::capability(xenon::ProviderCapability::Variation)
            | xenon::ProviderCapability::ControlRole
            | xenon::ProviderCapability::ReferenceAudio
            | xenon::ProviderCapability::ComponentLocks;
    }

    xenon::GenerationArtifact generate(
        const xenon::GenerationRequest& request,
        const std::filesystem::path& output_directory) override {
        xenon::GenerationArtifact artifact;
        artifact.audio_path = output_directory / "fake.wav";
        artifact.backend_name = std::string{name()};
        artifact.resolved_seed = request.seed;
        return artifact;
    }
};

} // namespace

int main() {
    xenon::ModelRouter router;
    router.add_provider(std::make_unique<FakeBackend>(), 100);
    xenon::GenerationPipeline pipeline(std::move(router));

    xenon::GenerationRequest request;
    request.prompt = "keep harmony, make drums less busy";
    request.mode = xenon::GenerationMode::Variation;
    request.render_intent = xenon::RenderIntent::Control;
    request.seed = 42;
    request.duration_seconds = 16.0;
    request.bpm = 92.0;
    request.key = "D minor";
    request.reference_audio = "reference.wav";
    request.control.locks = xenon::control_component(xenon::ControlComponent::Harmony);

    const auto result = pipeline.generate(request, "renders", "xdna-parent");

    assert(result.route.provider_name == "fake-control");
    assert(result.plan.bpm == 92.0);
    assert(result.plan.key == "D minor");
    assert(result.dna.parent_fingerprint == "xdna-parent");
    assert(!result.dna.fingerprint.empty());
    assert(result.dna.locks == request.control.locks);
    assert(result.artifact.backend_name == "fake-control");
    assert(result.artifact.resolved_seed == 42);
    return 0;
}
