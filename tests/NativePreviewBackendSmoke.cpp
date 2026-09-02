#include "xenon/backends/native_preview_backend.hpp"

#include <cassert>
#include <filesystem>

int main() {
    xenon::NativePreviewBackend backend;

    assert(backend.runtime_type() == xenon::RuntimeType::Local);
    assert(xenon::has_capability(
        backend.capabilities(),
        xenon::ProviderCapability::TextToInstrumental));

    xenon::GenerationRequest request;
    request.prompt = "dark ethereal preview";
    request.render_intent = xenon::RenderIntent::Draft;
    request.duration_seconds = 1.0;
    request.bpm = 120.0;
    request.key = "C minor";
    request.seed = 42;

    const auto output_directory = std::filesystem::temp_directory_path() / "xenon-native-preview-smoke";
    const auto artifact = backend.generate(request, output_directory);

    assert(artifact.backend_name == "xenon-native-preview");
    assert(!artifact.audio_path.empty());
    assert(std::filesystem::exists(artifact.audio_path));
    assert(std::filesystem::file_size(artifact.audio_path) > 44);

    std::error_code ec;
    std::filesystem::remove_all(output_directory, ec);
    return 0;
}
