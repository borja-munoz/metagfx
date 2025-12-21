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