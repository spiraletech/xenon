#pragma once

#include "xenon/engine.hpp"
#include "xenon/model_backend.hpp"

namespace xenon {

class NativePreviewBackend final : public IModelBackend {
public:
    [[nodiscard]] std::string_view name() const noexcept override;
    [[nodiscard]] ProviderCapabilities capabilities() const noexcept override;
    [[nodiscard]] RuntimeType runtime_type() const noexcept override;

    GenerationArtifact generate(
        const GenerationRequest& request,
        const std::filesystem::path& output_directory) override;

private:
    Engine engine_;
};

} // namespace xenon
