#include "xenon/backend_policy.hpp"
#include "xenon/model_router.hpp"

#include <cassert>
#include <filesystem>
#include <memory>
#include <string_view>

namespace {

class FakeBackend final : public xenon::IModelBackend {
public:
    FakeBackend(std::string_view name, xenon::RuntimeType runtime)
        : name_(name), runtime_(runtime) {}

    std::string_view name() const noexcept override { return name_; }

    xenon::ProviderCapabilities capabilities() const noexcept override {
        auto caps = xenon::ProviderCapability::TextToInstrumental |
                    xenon::ProviderCapability::QualityRole;
        if (runtime_ == xenon::RuntimeType::Local) {
            caps = caps | xenon::ProviderCapability::LocalRuntime;
        }
        return caps;
    }

    xenon::RuntimeType runtime_type() const noexcept override { return runtime_; }

    xenon::GenerationArtifact generate(
        const xenon::GenerationRequest& request,
        const std::filesystem::path& output_directory) override {
        xenon::GenerationArtifact artifact;
        artifact.audio_path = output_directory / "fake.wav";
        artifact.backend_name = std::string{name_};
        artifact.resolved_seed = request.seed;
        return artifact;
    }

private:
    std::string_view name_;
    xenon::RuntimeType runtime_;
};

} // namespace

int main() {
    xenon::ModelRouter router;
    router.add_provider(std::make_unique<FakeBackend>("local", xenon::RuntimeType::Local));
    router.add_provider(std::make_unique<FakeBackend>("remote", xenon::RuntimeType::RemoteApi));

    xenon::GenerationRequest request;
    request.prompt = "dark ethereal instrumental";
    request.mode = xenon::GenerationMode::TextToInstrumental;
    request.render_intent = xenon::RenderIntent::Quality;

    router.set_policy(xenon::BackendPolicy{{xenon::RuntimePreference::LocalOnly, 100, 100}});
    auto decision = router.route(request);
    assert(decision.provider_name == "local");
    assert(decision.runtime == xenon::RuntimeType::Local);

    router.set_policy(xenon::BackendPolicy{{xenon::RuntimePreference::RemoteOnly, 100, 100}});
    decision = router.route(request);
    assert(decision.provider_name == "remote");
    assert(decision.runtime == xenon::RuntimeType::RemoteApi);

    router.set_policy(xenon::BackendPolicy{{xenon::RuntimePreference::PreferLocal, 500, 500}});
    decision = router.route(request);
    assert(decision.provider_name == "local");

    router.set_policy(xenon::BackendPolicy{{xenon::RuntimePreference::PreferRemote, 500, 500}});
    decision = router.route(request);
    assert(decision.provider_name == "remote");

    return 0;
}
