#include "xenon/model_router.hpp"

#include <cassert>
#include <memory>
#include <string_view>

namespace {

class TestBackend final : public xenon::IModelBackend {
public:
    std::string_view name() const noexcept override { return "test-control"; }

    xenon::ProviderCapabilities capabilities() const noexcept override {
        return xenon::capability(xenon::ProviderCapability::Variation)
            | xenon::ProviderCapability::ControlRole
            | xenon::ProviderCapability::ReferenceAudio
            | xenon::ProviderCapability::ComponentLocks
            | xenon::ProviderCapability::LocalRuntime;
    }

    xenon::GenerationArtifact generate(
        const xenon::GenerationRequest& request,
        const std::filesystem::path& output_directory) override {
        return xenon::GenerationArtifact{
            output_directory / "router-smoke.wav",
            {},
            "test-control",
            request.seed
        };
    }
};

} // namespace

int main() {
    xenon::ModelRouter router;
    router.add_provider(std::make_unique<TestBackend>(), 100);

    xenon::GenerationRequest request;
    request.prompt = "make a controlled variation";
    request.mode = xenon::GenerationMode::Variation;
    request.render_intent = xenon::RenderIntent::Auto;
    request.reference_audio = "reference.wav";
    request.control.locks = xenon::control_component(xenon::ControlComponent::Harmony);
    request.seed = 42;

    const auto decision = router.route(request);
    assert(decision.provider_name == "test-control");
    assert(decision.resolved_intent == xenon::RenderIntent::Control);

    const auto artifact = router.generate(request, "renders");
    assert(artifact.backend_name == "test-control");
    assert(artifact.resolved_seed == 42);
    return 0;
}
