#include "xenon/surgical_revision.hpp"

namespace xenon {

const char* surgical_component_name(SurgicalComponent component) noexcept {
    switch (component) {
    case SurgicalComponent::Kick: return "kick";
    case SurgicalComponent::Snare: return "snare";
    case SurgicalComponent::Hats: return "hats";
    case SurgicalComponent::Drums: return "drums";
    case SurgicalComponent::Bass: return "bass";
    case SurgicalComponent::Vocals: return "vocals";
    case SurgicalComponent::Melody: return "melody";
    case SurgicalComponent::Harmony: return "harmony";
    case SurgicalComponent::Texture: return "texture";
    case SurgicalComponent::Arrangement: return "arrangement";
    case SurgicalComponent::Unknown: return "unknown";
    }
    return "unknown";
}

ControlComponent control_component_for(SurgicalComponent component) noexcept {
    switch (component) {
    case SurgicalComponent::Kick:
    case SurgicalComponent::Snare:
    case SurgicalComponent::Hats:
    case SurgicalComponent::Drums: return ControlComponent::Drums;
    case SurgicalComponent::Bass: return ControlComponent::Bass;
    case SurgicalComponent::Melody: return ControlComponent::Melody;
    case SurgicalComponent::Harmony: return ControlComponent::Harmony;
    case SurgicalComponent::Texture: return ControlComponent::Texture;
    case SurgicalComponent::Arrangement: return ControlComponent::Arrangement;
    case SurgicalComponent::Vocals:
    case SurgicalComponent::Unknown: return ControlComponent::None;
    }
    return ControlComponent::None;
}

StemRole stem_role_for(SurgicalComponent component) noexcept {
    switch (component) {
    case SurgicalComponent::Kick:
    case SurgicalComponent::Snare:
    case SurgicalComponent::Hats:
    case SurgicalComponent::Drums: return StemRole::Drums;
    case SurgicalComponent::Bass: return StemRole::Bass;
    case SurgicalComponent::Vocals: return StemRole::Vocals;
    case SurgicalComponent::Melody: return StemRole::Melody;
    case SurgicalComponent::Harmony: return StemRole::Harmony;
    case SurgicalComponent::Texture: return StemRole::Texture;
    case SurgicalComponent::Arrangement:
    case SurgicalComponent::Unknown: return StemRole::Unknown;
    }
    return StemRole::Unknown;
}

} // namespace xenon
