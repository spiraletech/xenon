#include "xenon/runtime_backend_manager.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace xenon {

RuntimeJobControl::RuntimeJobControl(ProgressCallback callback)
    : cancelled_(std::make_shared<std::atomic_bool>(false)), callback_(std::move(callback)) {}
void RuntimeJobControl::cancel() noexcept { cancelled_->store(true, std::memory_order_relaxed); }
bool RuntimeJobControl::cancelled() const noexcept { return cancelled_->load(std::memory_order_relaxed); }
void RuntimeJobControl::report(double progress, std::string stage) const {
    if (callback_) callback_(std::clamp(progress, 0.0, 1.0), stage);
}

RuntimeBackendManager::RuntimeBackendManager(BackendRegistry registry) : registry_(std::move(registry)) {
    for (const auto& name : registry_.names()) status_.emplace(name, RuntimeBackendStatus{name, RuntimeBackendState::Unloaded, {}});
}

void RuntimeBackendManager::load(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (loaded_.contains(name)) return;
    try {
        auto unique=registry_.create(name);
        loaded_[name]=std::shared_ptr<IModelBackend>(std::move(unique));
        status_[name]={name,RuntimeBackendState::Loaded,{}};
    } catch (const std::exception& ex) {
        status_[name]={name,RuntimeBackendState::Faulted,ex.what()};
        throw;
    }
}

void RuntimeBackendManager::unload(const std::string& name) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    loaded_.erase(name);
    status_[name]={name,RuntimeBackendState::Unloaded,{}};
}

bool RuntimeBackendManager::loaded(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return loaded_.contains(name);
}

std::vector<RuntimeBackendStatus> RuntimeBackendManager::statuses() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<RuntimeBackendStatus> out;
    out.reserve(status_.size());
    for (const auto& [_, status] : status_) out.push_back(status);
    std::sort(out.begin(), out.end(), [](const auto& a,const auto& b){return a.name<b.name;});
    return out;
}

std::shared_ptr<IModelBackend> RuntimeBackendManager::backend(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it=loaded_.find(name);
    if (it==loaded_.end()) throw std::runtime_error("L41 backend is not loaded: "+name);
    return it->second;
}

GenerationArtifact RuntimeBackendManager::generate(const std::string& name,const GenerationRequest& request,const std::filesystem::path& out,const RuntimeJobControl& control) {
    if (control.cancelled()) throw std::runtime_error("L41 generation cancelled before dispatch");
    control.report(0.05,"dispatch");
    auto provider=backend(name);
    control.report(0.15,"backend-running");
    GenerationArtifact artifact;
    try {
        artifact=provider->generate(request,out);
    } catch (const std::exception& ex) {
        std::lock_guard<std::mutex> lock(mutex_);
        status_[name]={name,RuntimeBackendState::Faulted,ex.what()};
        throw;
    }
    if (control.cancelled()) throw std::runtime_error("L41 generation cancelled; backend result discarded");
    control.report(0.90,"validating-artifact");
    if (artifact.audio_path.empty() || !std::filesystem::exists(artifact.audio_path))
        throw std::runtime_error("L41 backend returned a missing audio artifact");
    control.report(1.0,"complete");
    return artifact;
}

} // namespace xenon
