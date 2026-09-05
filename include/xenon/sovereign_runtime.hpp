#pragma once

#include "xenon/model_backend.hpp"

#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace xenon {
struct HttpRequest { std::string method{"GET"}; std::string url; std::vector<std::pair<std::string,std::string>> headers; std::vector<std::byte> body; std::string content_type; };
struct HttpResponse { int status{0}; std::vector<std::byte> body; std::string content_type; };
class IHttpTransport { public: virtual ~IHttpTransport()=default; virtual HttpResponse send(const HttpRequest& request)=0; };
class NativeHttpTransport final : public IHttpTransport { public: HttpResponse send(const HttpRequest& request) override; };
struct BackendHealth { std::string name; bool configured{false}; bool reachable{false}; std::string detail; };

class AceStepBackend final : public IModelBackend {
public: AceStepBackend(std::shared_ptr<IHttpTransport>,std::string base_url="http://127.0.0.1:8000",std::string checkpoint_path={}); [[nodiscard]] std::string_view name() const noexcept override; [[nodiscard]] ProviderCapabilities capabilities() const noexcept override; [[nodiscard]] RuntimeType runtime_type() const noexcept override; GenerationArtifact generate(const GenerationRequest&,const std::filesystem::path&) override; [[nodiscard]] BackendHealth health();
private: std::shared_ptr<IHttpTransport> http_; std::string base_url_,checkpoint_path_;
};
class StableAudioBackend final : public IModelBackend {
public: StableAudioBackend(std::shared_ptr<IHttpTransport>,std::string api_key={},std::string model="stable-audio-2.5"); [[nodiscard]] std::string_view name() const noexcept override; [[nodiscard]] ProviderCapabilities capabilities() const noexcept override; [[nodiscard]] RuntimeType runtime_type() const noexcept override; GenerationArtifact generate(const GenerationRequest&,const std::filesystem::path&) override; [[nodiscard]] BackendHealth health() const;
private: std::shared_ptr<IHttpTransport> http_; std::string api_key_,model_;
};

class BackendRegistry {
public:
 using Factory=std::function<std::unique_ptr<IModelBackend>()>;
 void register_factory(std::string name,Factory factory);
 [[nodiscard]] std::unique_ptr<IModelBackend> create(const std::string& name) const;
 [[nodiscard]] std::vector<std::string> names() const;
private: std::unordered_map<std::string,Factory> factories_;
};

struct RuntimeConfig {
 std::string ace_url{"http://127.0.0.1:8000"};
 std::string ace_checkpoint;
 std::string stability_model{"stable-audio-2.5"};
 std::vector<std::filesystem::path> plugin_paths;
 std::optional<unsigned int> midi_input;
 std::optional<unsigned int> midi_output;
 void save(const std::filesystem::path& path) const;
 static RuntimeConfig load(const std::filesystem::path& path);
};

struct MidiEndpoint { unsigned int index{0}; std::string name; bool input{false}; bool output{false}; };
class NativeMidiRuntime {
public:
 [[nodiscard]] std::vector<MidiEndpoint> enumerate() const;
 void send_short(unsigned int output_index,unsigned char status,unsigned char data1,unsigned char data2) const;
};

struct Vst3SdkStatus { bool sdk_enabled{false}; bool runtime_ready{false}; std::string detail; };
[[nodiscard]] Vst3SdkStatus vst3_sdk_status() noexcept;
struct SovereignRuntimeDiagnostics { BackendHealth ace_step; BackendHealth stable_audio; std::vector<MidiEndpoint> midi; Vst3SdkStatus vst3; };
class SovereignRuntime {
public:
 SovereignRuntime(std::shared_ptr<IHttpTransport>,std::string ace_url,std::string ace_checkpoint,std::string stability_key);
 [[nodiscard]] SovereignRuntimeDiagnostics diagnose(); [[nodiscard]] std::unique_ptr<IModelBackend> make_ace_step() const; [[nodiscard]] std::unique_ptr<IModelBackend> make_stable_audio() const; [[nodiscard]] BackendRegistry registry() const;
private: std::shared_ptr<IHttpTransport> http_; std::string ace_url_,ace_checkpoint_,stability_key_;
};
} // namespace xenon
