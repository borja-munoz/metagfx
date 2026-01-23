// ============================================================================
// src/rhi/webgpu/WebGPUSurfaceBridge.cpp
// ============================================================================
// This file serves as a central include point for platform-specific surface
// bridge implementations. The actual implementation is provided by one of:
// - WebGPUSurfaceBridge_Windows.cpp (Windows)
// - WebGPUSurfaceBridge_Metal.mm (macOS)
// - WebGPUSurfaceBridge_Linux.cpp (Linux)
// - WebGPUSurfaceBridge_Web.cpp (Emscripten)
//
// CMake selects the appropriate platform-specific file based on the target platform.
