# Dawn Dependencies

## Required Dependencies for Building Dawn

### System Requirements
- **CMake 3.13+**
- **C++20 compatible compiler**
  - Clang 13+ (macOS default)
  - GCC 11+
  - MSVC 2019+
- **Python 3** (for build scripts)
- **Git**

### Core Dependencies (auto-fetched by DAWN_FETCH_DEPENDENCIES=ON)

When you set `DAWN_FETCH_DEPENDENCIES=ON`, Dawn's CMake will automatically fetch:

1. **Abseil** (Google's C++ library)
   - Provides utilities like `absl::Span`
   - Repository: https://github.com/abseil/abseil-cpp

2. **SPIRV-Headers** (Khronos SPIR-V headers)
   - Repository: https://github.com/KhronosGroup/SPIRV-Headers

3. **SPIRV-Tools** (SPIR-V optimizer and validator)
   - Repository: https://github.com/KhronosGroup/SPIRV-Tools

4. **GLSLANG** (GLSL to SPIR-V compiler)
   - Repository: https://github.com/KhronosGroup/glslang

5. **Tint** (SPIR-V ↔ WGSL transpiler, built-in to Dawn)
   - Part of Dawn repository

6. **Vulkan Headers** (for Vulkan backend)
   - Repository: https://github.com/KhronosGroup/Vulkan-Headers

7. **GoogleTest** (if building tests - we disable this)
   - Repository: https://github.com/google/googletest

### Platform-Specific Dependencies

**macOS:**
- Metal framework (built-in)
- CoreGraphics framework (built-in)
- IOKit framework (built-in)
- Xcode Command Line Tools

**Linux:**
- X11 development headers: `libx11-dev`
- Wayland development headers: `libwayland-dev`
- Vulkan SDK (optional, for Vulkan backend)

**Windows:**
- Windows SDK (for D3D12 backend)

## Manual Build Approach (Without DAWN_FETCH_DEPENDENCIES)

If you want to build Dawn without auto-fetching, you need to manually install:

```bash
# macOS (using Homebrew)
brew install cmake python git

# Clone dependencies manually to specific locations
mkdir -p ~/dawn-deps
cd ~/dawn-deps

# 1. Abseil
git clone https://github.com/abseil/abseil-cpp.git
cd abseil-cpp
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=~/dawn-deps/install -DCMAKE_CXX_STANDARD=20
make -j$(sysctl -n hw.ncpu) install

# 2. SPIRV-Headers
cd ~/dawn-deps
git clone https://github.com/KhronosGroup/SPIRV-Headers.git
cd SPIRV-Headers
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=~/dawn-deps/install
make -j$(sysctl -n hw.ncpu) install

# 3. SPIRV-Tools
cd ~/dawn-deps
git clone https://github.com/KhronosGroup/SPIRV-Tools.git
cd SPIRV-Tools
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=~/dawn-deps/install \
         -DSPIRV-Headers_SOURCE_DIR=~/dawn-deps/SPIRV-Headers
make -j$(sysctl -n hw.ncpu) install

# 4. glslang
cd ~/dawn-deps
git clone https://github.com/KhronosGroup/glslang.git
cd glslang
./update_glslang_sources.py  # Fetches additional dependencies
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=~/dawn-deps/install -DCMAKE_CXX_STANDARD=20
make -j$(sysctl -n hw.ncpu) install

# 5. Vulkan Headers (optional)
cd ~/dawn-deps
git clone https://github.com/KhronosGroup/Vulkan-Headers.git
cd Vulkan-Headers
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=~/dawn-deps/install
make -j$(sysctl -n hw.ncpu) install
```

Then build Dawn with:

```bash
cd ~/dev/dawn/build
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_STANDARD=20 \
    -DCMAKE_INSTALL_PREFIX=~/dawn-install \
    -DDAWN_FETCH_DEPENDENCIES=OFF \
    -DCMAKE_PREFIX_PATH=~/dawn-deps/install \
    -DDAWN_BUILD_SAMPLES=OFF \
    -DDAWN_ENABLE_INSTALL=ON \
    -DDAWN_BUILD_MONOLITHIC_LIBRARY=STATIC

make -j$(sysctl -n hw.ncpu)
make install
```

## Recommended Approach: Use System Package Manager

**Homebrew (macOS) - RECOMMENDED:**

```bash
# Install build tools
brew install cmake python git

# Install Dawn dependencies via Homebrew
brew install abseil spirv-tools glslang

# Then build Dawn with:
cd ~/dev/dawn/build
rm -rf *

cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_STANDARD=20 \
    -DCMAKE_CXX_STANDARD_REQUIRED=ON \
    -DCMAKE_INSTALL_PREFIX=~/dawn-install \
    -DDAWN_FETCH_DEPENDENCIES=OFF \
    -DDAWN_BUILD_SAMPLES=OFF \
    -DDAWN_ENABLE_INSTALL=ON \
    -DDAWN_USE_GLFW=OFF \
    -DDAWN_ENABLE_NULL=OFF \
    -DDAWN_ENABLE_DESKTOP_GL=OFF \
    -DDAWN_ENABLE_OPENGLES=OFF \
    -DDAWN_BUILD_NODE_BINDINGS=OFF \
    -DDAWN_BUILD_MONOLITHIC_LIBRARY=STATIC \
    -DTINT_BUILD_SPV_READER=ON \
    -DTINT_BUILD_WGSL_WRITER=ON \
    -DTINT_BUILD_TESTS=OFF

make -j$(sysctl -n hw.ncpu)
make install
```

## Why the C++20 std::span Error Occurs

The error happens because:

1. **Dawn's main branch uses C++20 features** like `std::span`
2. **CMake's CMAKE_CXX_STANDARD** setting doesn't always propagate to all subprojects
3. **Some Dawn subprojects** (like `dawn/wire/client`) might override or ignore the C++ standard

## Potential Solutions

### Option 1: Use Homebrew dependencies (cleanest)
- Avoids the massive DAWN_FETCH_DEPENDENCIES download
- Pre-built packages with correct C++ standards
- Faster build

### Option 2: Use depot_tools (Google's approach)
Dawn is designed to work with Google's `depot_tools`:

```bash
# Install depot_tools
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
export PATH="$PATH:$(pwd)/depot_tools"

# Fetch Dawn with dependencies
mkdir dawn-gclient && cd dawn-gclient
gclient config https://dawn.googlesource.com/dawn
gclient sync

# Build
cd dawn
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_INSTALL_PREFIX=~/dawn-install \
         -DDAWN_BUILD_MONOLITHIC_LIBRARY=STATIC
make -j$(sysctl -n hw.ncpu)
make install
```

### Option 3: Pin to older stable Dawn tag
Use an older stable tag that doesn't require C++20:
- `chromium/6312` (older, might work with C++17)
- But this would require updating your WebGPUShader.cpp to use old Tint API

## Recommendation

**Try Homebrew approach first** - it's the cleanest and avoids dependency hell:

```bash
brew install cmake python git abseil spirv-tools glslang

cd ~/dev/dawn/build
rm -rf *

cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_STANDARD=20 \
    -DCMAKE_CXX_STANDARD_REQUIRED=ON \
    -DCMAKE_INSTALL_PREFIX=~/dawn-install \
    -DDAWN_FETCH_DEPENDENCIES=OFF \
    -DDAWN_BUILD_MONOLITHIC_LIBRARY=STATIC \
    -DDAWN_BUILD_SAMPLES=OFF \
    -DDAWN_ENABLE_INSTALL=ON

make -j$(sysctl -n hw.ncpu)
make install
```

If that fails, try the **depot_tools approach** which is what Google officially supports.
