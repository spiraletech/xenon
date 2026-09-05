#pragma once

#include "xenon/sovereign_runtime.hpp"

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace xenon {

enum class RuntimeBackendState { Unloaded, Loaded, Faulted };

struct RuntimeBackendStatus {
    std::string name;
    RuntimeBackendState state{RuntimeBackendState::Unloaded};
    std::string last_error;
};

class RuntimeJobControl {
public:
    using ProgressCallback = std::function<void(double, const std::string&)>;
    explicit RuntimeJobControl(ProgressCallback callback = {});
    void cancel() noexcept;
    [[nodiscard]] bool cancelled() const noexcept;
    void report(double progress, std::string stage) const;
private:
    std::shared_ptr<std::atomic_bool> cancelled_;
    ProgressCallback callback_;
};

class RuntimeBackendManager {
public:
    explicit RuntimeBackendManager(BackendRegistry registry);

    void load(const std::string& name);
    void unload(const std::string& name) noexcept;
    [[nodiscard]] bool loaded(const std::string& name) const;
    [[nodiscard]] std::vector<RuntimeBackendStatus> statuses() const;
    [[nodiscard]] std::shared_ptr<IModelBackend> backend(const std::string& name) const;

    [[nodiscard]] GenerationArtifact generate(
        const std::string& name,
        const GenerationRequest& request,
        const std::filesystem::path& output_directory,
        const RuntimeJobControl& control = {});

private:
    BackendRegistry registry_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<IModelBackend>> loaded_;
    std::unordered_map<std::string, RuntimeBackendStatus> status_;
};

} // namespace xenon
