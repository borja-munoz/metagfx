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