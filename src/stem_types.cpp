#include "xenon/stem_types.hpp"

namespace xenon {

const char* stem_role_name(StemRole role) noexcept {
    switch (role) {
    case StemRole::Drums: return "drums";
    case StemRole::Bass: return "bass";
    case StemRole::Vocals: return "vocals";
    case StemRole::Melody: return "melody";
    case StemRole::Harmony: return "harmony";
    case StemRole::Texture: return "texture";
    case StemRole::Unknown: return "unknown";
    }
    return "unknown";
}

ControlComponent stem_role_control_component(StemRole role) noexcept {
    switch (role) {
    case StemRole::Drums: return ControlComponent::Drums;
    case StemRole::Bass: return ControlComponent::Bass;
    case StemRole::Melody: return ControlComponent::Melody;
    case StemRole::Harmony: return ControlComponent::Harmony;
    case StemRole::Texture: return ControlComponent::Texture;
    case StemRole::Vocals:
    case StemRole::Unknown:
        return ControlComponent::None;
    }
    return ControlComponent::None;
}

} // namespace xenon
