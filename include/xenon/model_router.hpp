#pragma once

#include "xenon/model_backend.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace xenon {

struct ProviderInfo {
    std::string name;
    ProviderCapabilities capabilities{0};
    int priority{0};
};

struct RouteDecision {
    std::string provider_name;
    RenderIntent requested_intent{RenderIntent::Auto};
    RenderIntent resolved_intent{RenderIntent::Quality};
    ProviderCapabilities capabilities{0};
};

class ModelRouter {
public:
    void add_provider(std::unique_ptr<IModelBackend> backend, int priority = 0);

    [[nodiscard]] std::size_t provider_count() const noexcept;
    [[nodiscard]] std::vector<ProviderInfo> providers() const;
    [[nodiscard]] RouteDecision route(const GenerationRequest& request) const;

    GenerationArtifact generate(
        const GenerationRequest& request,
        const std::filesystem::path& output_directory);

private:
    struct ProviderSlot {
        std::unique_ptr<IModelBackend> backend;
        int priority{0};
    };

    [[nodiscard]] const ProviderSlot& select_provider(
        const GenerationRequest& request,
        RenderIntent resolved_intent) const;

    std::vector<ProviderSlot> providers_;
};

} // namespace xenon
