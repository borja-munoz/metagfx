# Physically Based Renderer - Milestone 1.1

## Project Structure

```
metagfx/
├── CMakeLists.txt
├── README.md
├── .gitignore
├── setup.sh          (Linux/macOS)
├── setup.bat         (Windows)
├── assets/
│   ├── models/
│   ├── textures/
│   └── scenes/
├── external/
│   ├── CMakeLists.txt
│   ├── SDL/          (clone SDL3 here)
│   ├── glm/          (clone GLM here)
│   └── stb/          (download stb_image.h here)
├── include/
│   └── metagfx/
│       ├── core/
│       │   ├── Logger.h
│       │   ├── Platform.h
│       │   └── Types.h
│       ├── rhi/
│       │   ├── GraphicsDevice.h
│       │   └── Types.h
│       ├── scene/
│       │   └── Scene.h
│       └── renderer/
│           └── Renderer.h
└── src/
    ├── core/
    │   ├── Logger.cpp
    │   └── Platform.cpp
    ├── rhi/
    │   └── GraphicsDevice.cpp
    ├── scene/
    │   └── Scene.cpp
    ├── renderer/
    │   └── Renderer.cpp
    └── app/
        ├── Application.h
        ├── Application.cpp
        └── main.cpp
```

---

## Prerequisites

### All Platforms
- CMake 3.20 or higher
- C++17 compliant compiler
- Git

### Windows
- Visual Studio 2019/2022 OR
- MinGW-w64 with GCC 9+
- Vulkan SDK (from LunarG)

### Linux
- GCC 9+ or Clang 10+
- Vulkan SDK or vulkan-headers package
- Development libraries:
  ```bash
  sudo apt install build-essential cmake git
  sudo apt install libvulkan-dev
  sudo apt install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
  ```

### macOS
- Xcode Command Line Tools
- CMake (via Homebrew)
- Vulkan SDK (MoltenVK)
  ```bash
  brew install cmake
  ```

---

## Quick Setup (Recommended)

### Linux/macOS

Create a file named `setup.sh`:

```bash
#!/bin/bash

set -e

echo "========================================"
echo "MetaGFX - Project Setup"
echo "========================================"

# Create directory structure
echo "Creating directory structure..."
mkdir -p external
mkdir -p assets/models
mkdir -p assets/textures
mkdir -p assets/scenes
mkdir -p include/metagfx/{core,rhi,scene,renderer}
mkdir -p src/{core,rhi,scene,renderer,app}
mkdir -p tests

# Clone dependencies
echo "Cloning dependencies..."
cd external

if [ ! -d "SDL" ]; then
    echo "Cloning SDL3..."
    git clone https://github.com/libsdl-org/SDL.git
    cd SDL
    git checkout main
    cd ..
else
    echo "SDL3 already exists, skipping..."
fi

if [ ! -d "glm" ]; then
    echo "Cloning GLM..."
    git clone https://github.com/g-truc/glm.git
else
    echo "GLM already exists, skipping..."
fi

if [ ! -d "stb" ]; then
    echo "Downloading stb..."
    mkdir -p stb
    cd stb
    curl -O https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
    cd ..
else
    echo "stb already exists, skipping..."
fi

cd ..

echo "========================================"
echo "Setup complete!"
echo "========================================"
echo ""
echo "Next steps:"
echo "1. Create your source files according to the project structure"
echo "2. Run: mkdir build && cd build"
echo "3. Run: cmake .."
echo "4. Run: make -j\$(nproc)"
echo ""
```

Then run:
```bash
chmod +x setup.sh
./setup.sh
```

### Windows

Create a file named `setup.bat`:

```batch
@echo off
setlocal enabledelayedexpansion

echo ========================================
echo MetaGFX - Project Setup
echo ========================================

:: Create directory structure
echo Creating directory structure...
if not exist external mkdir external
if not exist assets\models mkdir assets\models
if not exist assets\textures mkdir assets\textures
if not exist assets\scenes mkdir assets\scenes
if not exist include\metagfx\core mkdir include\metagfx\core
if not exist include\metagfx\rhi mkdir include\metagfx\rhi
if not exist include\metagfx\scene mkdir include\metagfx\scene
if not exist include\metagfx\renderer mkdir include\metagfx\renderer
if not exist src\core mkdir src\core
if not exist src\rhi mkdir src\rhi
if not exist src\scene mkdir src\scene
if not exist src\renderer mkdir src\renderer
if not exist src\app mkdir src\app
if not exist tests mkdir tests

:: Clone dependencies
echo Cloning dependencies...
cd external

if not exist "SDL" (
    echo Cloning SDL3...
    git clone https://github.com/libsdl-org/SDL.git
    cd SDL
    git checkout main
    cd ..
) else (
    echo SDL3 already exists, skipping...
)

if not exist "glm" (
    echo Cloning GLM...
    git clone https://github.com/g-truc/glm.git
) else (
    echo GLM already exists, skipping...
)

if not exist "stb" (
    echo Downloading stb...
    mkdir stb
    cd stb
    curl -O https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
    cd ..
) else (
    echo stb already exists, skipping...
)

cd ..

echo ========================================
echo Setup complete!
echo ========================================
echo.
echo Next steps:
echo 1. Create your source files according to the project structure
echo 2. Run: mkdir build ^&^& cd build
echo 3. Run: cmake .. -G "Visual Studio 17 2022" -A x64
echo 4. Run: cmake --build . --config Release
echo.
pause
```

Then run:
```batch
setup.bat
```

---

## Manual Setup (Alternative)

### 1. Create external directory and navigate to it
```bash
mkdir external
cd external
```

### 2. Clone SDL3
```bash
git clone https://github.com/libsdl-org/SDL.git
cd SDL
git checkout main
cd ..
```

### 3. Clone GLM
```bash
git clone https://github.com/g-truc/glm.git
```

### 4. Download stb
```bash
mkdir stb
cd stb
curl -O https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
cd ..
```

---

## Building the Project

### Windows (Visual Studio)

```bash
# From project root
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

Or open `build/PhysicallyBasedRenderer.sln` in Visual Studio.

### Windows (MinGW)

```bash
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
cmake --build . --config Release
```

### Linux

```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### macOS

```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)
```

---

## Running

After building, the executable will be in `build/bin/`:

### Windows
```bash
cd build\bin
MetaGFX.exe
```

### Linux/macOS
```bash
cd build/bin
./MetaGFX
```

---

## Expected Behavior (Milestone 1.1)

When you run the application, you should see:
- A console window with log output showing initialization steps
- A window titled "MetaGFX - Milestone 1.1" (1280x720)
- The window can be resized
- Press ESC or close the window to exit
- Clean shutdown with log messages

---

## CMake Build Options

```bash
cmake .. -DMETAGFX_USE_VULKAN=ON      # Enable Vulkan support (default: ON)
cmake .. -DMETAGFX_USE_D3D12=ON       # Enable D3D12 support (default: OFF)
cmake .. -DMETAGFX_USE_METAL=ON       # Enable Metal support (default: OFF)
cmake .. -DMETAGFX_USE_WEBGPU=ON      # Enable WebGPU support (default: OFF)
cmake .. -DMETAGFX_BUILD_TESTS=ON     # Build tests (default: OFF)
```

---

## .gitignore

Create a `.gitignore` file in the project root:

```
# Build directories
build/
out/
cmake-build-*/

# IDE files
.vs/
.vscode/
.idea/
*.swp
*.swo
*~

# Compiled files
*.o
*.obj
*.a
*.lib
*.dll
*.so
*.dylib
*.exe

# CMake
CMakeCache.txt
CMakeFiles/
cmake_install.cmake
Makefile

# OS files
.DS_Store
Thumbs.db

# Assets (large files)
assets/*.exr
assets/*.hdr
assets/models/*.obj
assets/models/*.fbx
```

---

## Troubleshooting

### SDL3 not found
Make sure SDL3 is properly cloned in the `external/SDL` directory.

### Vulkan SDK not found (Windows)
Install the Vulkan SDK from https://vulkan.lunarg.com/ and ensure the `VULKAN_SDK` environment variable is set.

### Vulkan headers not found (Linux)
```bash
sudo apt install libvulkan-dev vulkan-tools
```

### Compiler not found
Ensure your compiler is in PATH and CMake can detect it.

### Link errors with SDL3
SDL3 might require additional system libraries. Check CMake output for missing dependencies.

---

## Next Steps

After successfully building and running Milestone 1.1:
- Move to Milestone 1.2: Define RHI (Render Hardware Interface) abstract interfaces
- Prepare for Milestone 1.3: Implement Vulkan backend and render the first triangle

---

## License

(Add your license here)

---

**Milestone 1.1 Status**: ✅ Complete when:
- ✅ Project compiles on your platform
- ✅ Window opens successfully
- ✅ SDL3 integration working
- ✅ Logging system functional
- ✅ Clean application lifecycle (startup/shutdown)
- ✅ Ready for RHI implementation