#include "xenon/backend_policy.hpp"

namespace xenon {

BackendPolicy::BackendPolicy(BackendPolicyConfig config) noexcept
    : config_(config) {}

void BackendPolicy::set_config(BackendPolicyConfig config) noexcept {
    config_ = config;
}

const BackendPolicyConfig& BackendPolicy::config() const noexcept {
    return config_;
}

bool BackendPolicy::allows(RuntimeType runtime) const noexcept {
    switch (config_.runtime_preference) {
    case RuntimePreference::Any:
    case RuntimePreference::PreferLocal:
    case RuntimePreference::PreferRemote:
        return true;
    case RuntimePreference::LocalOnly:
        return runtime == RuntimeType::Local;
    case RuntimePreference::RemoteOnly:
        return runtime == RuntimeType::RemoteApi;
    }
    return true;
}

int BackendPolicy::runtime_score(RuntimeType runtime) const noexcept {
    switch (config_.runtime_preference) {
    case RuntimePreference::PreferLocal:
        return runtime == RuntimeType::Local ? config_.local_bonus : 0;
    case RuntimePreference::PreferRemote:
        return runtime == RuntimeType::RemoteApi ? config_.remote_bonus : 0;
    case RuntimePreference::Any:
    case RuntimePreference::LocalOnly:
    case RuntimePreference::RemoteOnly:
        return 0;
    }
    return 0;
}

} // namespace xenon
