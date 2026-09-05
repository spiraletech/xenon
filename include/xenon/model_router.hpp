#pragma once

#include "xenon/backend_policy.hpp"
#include "xenon/model_backend.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace xenon {
struct ProviderInfo { std::string name; ProviderCapabilities capabilities{0}; RuntimeType runtime{RuntimeType::Unknown}; int priority{0}; };
struct RouteDecision { std::string provider_name; RenderIntent requested_intent{RenderIntent::Auto}; RenderIntent resolved_intent{RenderIntent::Quality}; ProviderCapabilities capabilities{0}; RuntimeType runtime{RuntimeType::Unknown}; };
struct BackendGenerationAttempt { RouteDecision route; GenerationArtifact artifact; bool success{false}; std::string error; };
class ModelRouter {
public:
    void add_provider(std::unique_ptr<IModelBackend> backend,int priority=0);
    void add_provider(std::shared_ptr<IModelBackend> backend,int priority=0);
    void set_policy(BackendPolicy policy) noexcept; [[nodiscard]] const BackendPolicy& policy() const noexcept;
    [[nodiscard]] std::size_t provider_count() const noexcept; [[nodiscard]] std::vector<ProviderInfo> providers() const; [[nodiscard]] RouteDecision route(const GenerationRequest& request) const;
    GenerationArtifact generate(const GenerationRequest& request,const std::filesystem::path& output_directory);
    [[nodiscard]] std::vector<BackendGenerationAttempt> generate_all(const GenerationRequest& request,const std::filesystem::path& output_directory);
private:
    struct ProviderSlot { std::shared_ptr<IModelBackend> backend; int priority{0}; };
    [[nodiscard]] const ProviderSlot& select_provider(const GenerationRequest& request,RenderIntent resolved_intent) const;
    [[nodiscard]] bool eligible(const ProviderSlot& slot,const GenerationRequest& request,RenderIntent resolved_intent) const;
    BackendPolicy policy_{}; std::vector<ProviderSlot> providers_;
};
} // namespace xenon
