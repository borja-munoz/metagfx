// ============================================================================
// src/core/Platform.cpp
// ============================================================================
#include "metagfx/core/Platform.h"

namespace metagfx {

Platform PlatformUtils::GetPlatform() {
#if defined(_WIN32) || defined(_WIN64)
    return Platform::Windows;
#elif defined(__APPLE__)
    return Platform::macOS;
#elif defined(__linux__)
    return Platform::Linux;
#else
    return Platform::Unknown;
#endif
}

const char* PlatformUtils::GetPlatformName() {
    switch (GetPlatform()) {
        case Platform::Windows: return "Windows";
        case Platform::macOS:   return "macOS";
        case Platform::Linux:   return "Linux";
        default:                return "Unknown";
    }
}

} // namespace metagfx
