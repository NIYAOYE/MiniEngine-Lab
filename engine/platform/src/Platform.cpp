#include "me/platform/Platform.h"

namespace me::platform {

const char* PlatformName() {
#if defined(_WIN32)
    return "Windows";
#elif defined(__linux__)
    return "Linux";
#else
    return "Unknown";
#endif
}

} // namespace me::platform
