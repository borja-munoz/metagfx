// ============================================================================
// include/metagfx/rhi/Types.h - Core RHI Types and Enums
// ============================================================================
#pragma once

#include "metagfx/core/Types.h"
#include <string>
#include <vector>

namespace metagfx {
namespace rhi {

// Forward declarations
class GraphicsDevice;
class SwapChain;
class CommandBuffer;
class Buffer;
class Texture;
class Shader;
class Pipeline;
class RenderPass;

// ============================================================================
// Enumerations
// ============================================================================

enum class GraphicsAPI {
    Vulkan,
    Direct3D12,
    Metal,
    WebGPU
};

enum class BufferUsage {
    Vertex      = 1 << 0,
    Index       = 1 << 1,
    Uniform     = 1 << 2,
    Storage     = 1 << 3,
    TransferSrc = 1 << 4,
    TransferDst = 1 << 5
};

inline BufferUsage operator|(BufferUsage a, BufferUsage b) {
    return static_cast<BufferUsage>(static_cast<int>(a) | static_cast<int>(b));
}

inline BufferUsage operator&(BufferUsage a, BufferUsage b) {
    return static_cast<BufferUsage>(static_cast<int>(a) & static_cast<int>(b));
}

enum class MemoryUsage {
    GPUOnly,        // Device local, not visible to CPU
    CPUToGPU,       // CPU writes, GPU reads (staging, dynamic)
    GPUToCPU,       // GPU writes, CPU reads (readback)
    CPUOnly         // CPU only (for staging)
};

enum class ShaderStage {
    Vertex,
    Fragment,
    Compute,
    Geometry,
    TessellationControl,
    TessellationEvaluation
};

enum class PrimitiveTopology {
    TriangleList,
    TriangleStrip,
    LineList,
    LineStrip,
    PointList
};

enum class PolygonMode {
    Fill,
    Line,
    Point
};

enum class CullMode {
    None,
    Front,
    Back,
    FrontAndBack
};

enum class FrontFace {
    Clockwise,
    CounterClockwise
};

enum class CompareOp {
    Never,
    Less,
    Equal,
    LessOrEqual,
    Greater,
    NotEqual,
    GreaterOrEqual,
    Always
};

enum class Format {
    Undefined,
    
    // 8-bit formats
    R8_UNORM,
    R8_SNORM,
    R8_UINT,
    R8_SINT,
    
    // 16-bit formats
    R16_UNORM,
    R16_SNORM,
    R16_UINT,
    R16_SINT,
    R16_SFLOAT,
    
    // 32-bit formats
    R32_UINT,
    R32_SINT,
    R32_SFLOAT,
    
    // Two component 8-bit
    R8G8_UNORM,
    R8G8_SNORM,
    R8G8_UINT,
    R8G8_SINT,
    
    // Two component 16-bit
    R16G16_UNORM,
    R16G16_SNORM,
    R16G16_UINT,
    R16G16_SINT,
    R16G16_SFLOAT,
    
    // Two component 32-bit
    R32G32_UINT,
    R32G32_SINT,
    R32G32_SFLOAT,
    
    // Three component 32-bit
    R32G32B32_UINT,
    R32G32B32_SINT,
    R32G32B32_SFLOAT,
    
    // Four component 8-bit
    R8G8B8A8_UNORM,
    R8G8B8A8_SNORM,
    R8G8B8A8_UINT,
    R8G8B8A8_SINT,
    R8G8B8A8_SRGB,
    
    B8G8R8A8_UNORM,
    B8G8R8A8_SRGB,
    
    // Four component 16-bit
    R16G16B16A16_UNORM,
    R16G16B16A16_SNORM,
    R16G16B16A16_UINT,
    R16G16B16A16_SINT,
    R16G16B16A16_SFLOAT,
    
    // Four component 32-bit
    R32G32B32A32_UINT,
    R32G32B32A32_SINT,
    R32G32B32A32_SFLOAT,
    
    // Depth/Stencil formats
    D16_UNORM,
    D32_SFLOAT,
    D24_UNORM_S8_UINT,
    D32_SFLOAT_S8_UINT
};

enum class TextureType {
    Texture2D,
    TextureCube,
    Texture3D
};

enum class TextureUsage {
    Sampled         = 1 << 0,
    Storage         = 1 << 1,
    ColorAttachment = 1 << 2,
    DepthStencilAttachment = 1 << 3,
    TransferSrc     = 1 << 4,
    TransferDst     = 1 << 5
};

inline TextureUsage operator|(TextureUsage a, TextureUsage b) {
    return static_cast<TextureUsage>(static_cast<int>(a) | static_cast<int>(b));
}

enum class Filter {
    Nearest,
    Linear
};

enum class SamplerAddressMode {
    Repeat,
    MirroredRepeat,
    ClampToEdge,
    ClampToBorder
};

// ============================================================================
// Structures
// ============================================================================

struct DeviceInfo {
    std::string deviceName;
    GraphicsAPI api;
    uint32 apiVersion;
    uint64 deviceMemory;
};

struct BufferDesc {
    uint64 size = 0;
    BufferUsage usage;
    MemoryUsage memoryUsage = MemoryUsage::GPUOnly;
    const char* debugName = nullptr;
};

struct TextureDesc {
    TextureType type = TextureType::Texture2D;
    uint32 width = 1;
    uint32 height = 1;
    uint32 depth = 1;
    uint32 mipLevels = 1;
    uint32 arrayLayers = 1;
    Format format = Format::R8G8B8A8_UNORM;
    TextureUsage usage;
    const char* debugName = nullptr;
};

struct SamplerDesc {
    Filter minFilter = Filter::Linear;
    Filter magFilter = Filter::Linear;
    Filter mipmapMode = Filter::Linear;
    SamplerAddressMode addressModeU = SamplerAddressMode::Repeat;
    SamplerAddressMode addressModeV = SamplerAddressMode::Repeat;
    SamplerAddressMode addressModeW = SamplerAddressMode::Repeat;
    float mipLodBias = 0.0f;
    float minLod = 0.0f;
    float maxLod = 1000.0f;
    bool anisotropyEnable = false;
    float maxAnisotropy = 1.0f;
};

struct ShaderDesc {
    ShaderStage stage;
    std::vector<uint8> code;  // Shader bytecode (SPIRV, DXIL, etc.)
    std::string entryPoint = "main";
    const char* debugName = nullptr;
};

struct VertexAttribute {
    uint32 location;
    Format format;
    uint32 offset;
    uint32 binding;
};

struct VertexInputLayout {
    std::vector<VertexAttribute> attributes;
    uint32 stride;
};

// Vertex input rate (per-vertex or per-instance)
enum class VertexInputRate {
    Vertex,
    Instance
};

// Vertex input binding description
struct VertexInputBinding {
    uint32_t binding = 0;
    uint32_t stride = 0;
    VertexInputRate inputRate = VertexInputRate::Vertex;
};

// Vertex input state for pipeline
struct VertexInputState {
    std::vector<VertexInputBinding> bindings;
    std::vector<VertexAttribute> attributes;
};

struct RasterizationState {
    PolygonMode polygonMode = PolygonMode::Fill;
    CullMode cullMode = CullMode::Back;
    FrontFace frontFace = FrontFace::CounterClockwise;
    bool depthClampEnable = false;
    bool depthBiasEnable = false;
    float depthBiasConstantFactor = 0.0f;
    float depthBiasSlopeFactor = 0.0f;
    float lineWidth = 1.0f;
};

struct DepthStencilState {
    bool depthTestEnable = true;
    bool depthWriteEnable = true;
    CompareOp depthCompareOp = CompareOp::Less;
    bool stencilTestEnable = false;
};

struct ColorAttachmentState {
    bool blendEnable = false;
    // Blend operations will be added when needed
};

struct PipelineDesc {
    Ref<Shader> vertexShader;
    Ref<Shader> fragmentShader;
    VertexInputLayout vertexInput;
    PrimitiveTopology topology = PrimitiveTopology::TriangleList;
    RasterizationState rasterization;
    DepthStencilState depthStencil;
    std::vector<ColorAttachmentState> colorAttachments;
    const char* debugName = nullptr;
    VertexInputState vertexInputState;
};

struct Viewport {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float minDepth = 0.0f;
    float maxDepth = 1.0f;
};

struct Rect2D {
    int32 x = 0;
    int32 y = 0;
    uint32 width = 0;
    uint32 height = 0;
};

struct ClearValue {
    struct DepthStencilValue {
        float depth;
        uint32 stencil;
    };
    
    union {
        float color[4];
        DepthStencilValue depthStencil;
    };
};

} // namespace rhi
} // namespace metagfx