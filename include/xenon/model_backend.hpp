#pragma once

#include "xenon/generation_types.hpp"

#include <cstdint>
#include <filesystem>
#include <string_view>

namespace xenon {

enum class ProviderCapability : std::uint32_t {
    None                = 0,
    TextToInstrumental  = 1u << 0,
    Variation           = 1u << 1,
    Extend              = 1u << 2,
    AudioToAudio        = 1u << 3,
    ReferenceAudio      = 1u << 4,
    ReplaceSection      = 1u << 5,
    DraftRole           = 1u << 8,
    QualityRole         = 1u << 9,
    ControlRole         = 1u << 10,
    VocalRole           = 1u << 11,
    LocalRuntime        = 1u << 16,
    DrumConditioning    = 1u << 17,
    MelodyConditioning  = 1u << 18,
    HarmonyConditioning = 1u << 19,
    ComponentLocks      = 1u << 20,
    TemporalControl     = 1u << 21
};

using ProviderCapabilities = std::uint32_t;

[[nodiscard]] constexpr ProviderCapabilities capability(ProviderCapability value) noexcept {
    return static_cast<ProviderCapabilities>(value);
}

[[nodiscard]] constexpr ProviderCapabilities operator|(ProviderCapability a, ProviderCapability b) noexcept {
    return capability(a) | capability(b);
}

[[nodiscard]] constexpr ProviderCapabilities operator|(ProviderCapabilities a, ProviderCapability b) noexcept {
    return a | capability(b);
}

[[nodiscard]] constexpr bool has_capability(ProviderCapabilities value, ProviderCapability flag) noexcept {
    return (value & capability(flag)) != 0;
}

class IModelBackend {
public:
    virtual ~IModelBackend() = default;

    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    [[nodiscard]] virtual ProviderCapabilities capabilities() const noexcept = 0;

    virtual GenerationArtifact generate(
        const GenerationRequest& request,
        const std::filesystem::path& output_directory) = 0;
};

} // namespace xenon
