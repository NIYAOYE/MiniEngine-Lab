#include "me/platform/Input.h"

#include <windows.h>

namespace me::platform::detail {

std::optional<KeyCode> MapPlatformKey(unsigned int vk) {
    switch (vk) {
    case VK_ESCAPE: return KeyCode::Escape;
    case VK_SPACE:  return KeyCode::Space;
    case 'W':       return KeyCode::W;
    case 'A':       return KeyCode::A;
    case 'S':       return KeyCode::S;
    case 'D':       return KeyCode::D;
    case 'Q':       return KeyCode::Q;
    case 'E':       return KeyCode::E;
    default:        return std::nullopt;
    }
}

} // namespace me::platform::detail
