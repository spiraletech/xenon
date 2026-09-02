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

} // namespace xenon
