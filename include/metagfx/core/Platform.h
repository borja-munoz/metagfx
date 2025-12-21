// ============================================================================
// include/metagfx/core/Platform.h
// ============================================================================
#pragma once

namespace metagfx {

enum class Platform {
    Windows,
    macOS,
    Linux,
    Unknown
};

class PlatformUtils {
public:
    static Platform GetPlatform();
    static const char* GetPlatformName();
};

} // namespace metagfx
