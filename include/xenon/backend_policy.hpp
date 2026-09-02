#pragma once

#include "xenon/model_backend.hpp"

namespace xenon {

enum class RuntimePreference {
    Any,
    PreferLocal,
    LocalOnly,
    PreferRemote,
    RemoteOnly
};

struct BackendPolicyConfig {
    RuntimePreference runtime_preference{RuntimePreference::PreferLocal};
    int local_bonus{100};
    int remote_bonus{100};
};

class BackendPolicy {
public:
    BackendPolicy() = default;
    explicit BackendPolicy(BackendPolicyConfig config) noexcept;

    void set_config(BackendPolicyConfig config) noexcept;
    [[nodiscard]] const BackendPolicyConfig& config() const noexcept;

    [[nodiscard]] bool allows(RuntimeType runtime) const noexcept;
    [[nodiscard]] int runtime_score(RuntimeType runtime) const noexcept;

private:
    BackendPolicyConfig config_{};
};

} // namespace xenon
