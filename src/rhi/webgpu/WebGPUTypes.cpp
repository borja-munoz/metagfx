// ============================================================================
// src/rhi/webgpu/WebGPUTypes.cpp
// ============================================================================
#include "metagfx/rhi/webgpu/WebGPUTypes.h"
#include "metagfx/core/Logger.h"

namespace metagfx {
namespace rhi {

wgpu::TextureFormat ToWebGPUTextureFormat(Format format) {
    switch (format) {
        // 8-bit formats
        case Format::R8_UNORM:    return wgpu::TextureFormat::R8Unorm;
        case Format::R8_SNORM:    return wgpu::TextureFormat::R8Snorm;
        case Format::R8_UINT:     return wgpu::TextureFormat::R8Uint;
        case Format::R8_SINT:     return wgpu::TextureFormat::R8Sint;

        // 16-bit formats
        case Format::R16_UINT:    return wgpu::TextureFormat::R16Uint;
        case Format::R16_SINT:    return wgpu::TextureFormat::R16Sint;
        case Format::R16_SFLOAT:  return wgpu::TextureFormat::R16Float;

        // 32-bit formats
        case Format::R32_UINT:    return wgpu::TextureFormat::R32Uint;
        case Format::R32_SINT:    return wgpu::TextureFormat::R32Sint;
        case Format::R32_SFLOAT:  return wgpu::TextureFormat::R32Float;

        // Two component 8-bit
        case Format::R8G8_UNORM:  return wgpu::TextureFormat::RG8Unorm;
        case Format::R8G8_SNORM:  return wgpu::TextureFormat::RG8Snorm;
        case Format::R8G8_UINT:   return wgpu::TextureFormat::RG8Uint;
        case Format::R8G8_SINT:   return wgpu::TextureFormat::RG8Sint;

        // Two component 16-bit
        case Format::R16G16_UINT:   return wgpu::TextureFormat::RG16Uint;
        case Format::R16G16_SINT:   return wgpu::TextureFormat::RG16Sint;
        case Format::R16G16_SFLOAT: return wgpu::TextureFormat::RG16Float;

        // Two component 32-bit
        case Format::R32G32_UINT:   return wgpu::TextureFormat::RG32Uint;
        case Format::R32G32_SINT:   return wgpu::TextureFormat::RG32Sint;
        case Format::R32G32_SFLOAT: return wgpu::TextureFormat::RG32Float;

        // Four component 8-bit
        case Format::R8G8B8A8_UNORM: return wgpu::TextureFormat::RGBA8Unorm;
        case Format::R8G8B8A8_SNORM: return wgpu::TextureFormat::RGBA8Snorm;
        case Format::R8G8B8A8_UINT:  return wgpu::TextureFormat::RGBA8Uint;
        case Format::R8G8B8A8_SINT:  return wgpu::TextureFormat::RGBA8Sint;
        case Format::R8G8B8A8_SRGB:  return wgpu::TextureFormat::RGBA8UnormSrgb;

        case Format::B8G8R8A8_UNORM: return wgpu::TextureFormat::BGRA8Unorm;
        case Format::B8G8R8A8_SRGB:  return wgpu::TextureFormat::BGRA8UnormSrgb;

        // Four component 16-bit
        case Format::R16G16B16A16_UINT:   return wgpu::TextureFormat::RGBA16Uint;
        case Format::R16G16B16A16_SINT:   return wgpu::TextureFormat::RGBA16Sint;
        case Format::R16G16B16A16_SFLOAT: return wgpu::TextureFormat::RGBA16Float;

        // Four component 32-bit
        case Format::R32G32B32A32_UINT:   return wgpu::TextureFormat::RGBA32Uint;
        case Format::R32G32B32A32_SINT:   return wgpu::TextureFormat::RGBA32Sint;
        case Format::R32G32B32A32_SFLOAT: return wgpu::TextureFormat::RGBA32Float;

        // Depth/Stencil formats
        case Format::D16_UNORM:         return wgpu::TextureFormat::Depth16Unorm;
        case Format::D32_SFLOAT:        return wgpu::TextureFormat::Depth32Float;
        case Format::D24_UNORM_S8_UINT: return wgpu::TextureFormat::Depth24PlusStencil8;
        case Format::D32_SFLOAT_S8_UINT: return wgpu::TextureFormat::Depth32FloatStencil8;

        // Unsupported formats - return undefined and log warning
        case Format::R16_UNORM:
        case Format::R16_SNORM:
        case Format::R16G16_UNORM:
        case Format::R16G16_SNORM:
        case Format::R16G16B16A16_UNORM:
        case Format::R16G16B16A16_SNORM:
        case Format::R32G32B32_UINT:
        case Format::R32G32B32_SINT:
        case Format::R32G32B32_SFLOAT:
            WEBGPU_LOG_ERROR("Unsupported format: " << static_cast<int>(format));
            return wgpu::TextureFormat::Undefined;

        case Format::Undefined:
        default:
            return wgpu::TextureFormat::Undefined;
    }
}

wgpu::IndexFormat ToWebGPUIndexFormat(Format format) {
    switch (format) {
        case Format::R16_UINT:
            return wgpu::IndexFormat::Uint16;
        case Format::R32_UINT:
            return wgpu::IndexFormat::Uint32;
        default:
            WEBGPU_LOG_ERROR("Invalid index format: " << static_cast<int>(format));
            return wgpu::IndexFormat::Uint32;
    }
}

wgpu::BufferUsage ToWebGPUBufferUsage(BufferUsage usage) {
    wgpu::BufferUsage result = wgpu::BufferUsage::None;

    if (static_cast<int>(usage) & static_cast<int>(BufferUsage::Vertex))
        result |= wgpu::BufferUsage::Vertex;
    if (static_cast<int>(usage) & static_cast<int>(BufferUsage::Index))
        result |= wgpu::BufferUsage::Index;
    if (static_cast<int>(usage) & static_cast<int>(BufferUsage::Uniform))
        result |= wgpu::BufferUsage::Uniform;
    if (static_cast<int>(usage) & static_cast<int>(BufferUsage::Storage))
        result |= wgpu::BufferUsage::Storage;
    if (static_cast<int>(usage) & static_cast<int>(BufferUsage::TransferSrc))
        result |= wgpu::BufferUsage::CopySrc;
    if (static_cast<int>(usage) & static_cast<int>(BufferUsage::TransferDst))
        result |= wgpu::BufferUsage::CopyDst;

    return result;
}

wgpu::AddressMode ToWebGPUAddressMode(SamplerAddressMode mode) {
    switch (mode) {
        case SamplerAddressMode::Repeat:         return wgpu::AddressMode::Repeat;
        case SamplerAddressMode::MirroredRepeat: return wgpu::AddressMode::MirrorRepeat;
        case SamplerAddressMode::ClampToEdge:    return wgpu::AddressMode::ClampToEdge;
        default:
            WEBGPU_LOG_ERROR("Unsupported address mode: " << static_cast<int>(mode));
            return wgpu::AddressMode::Repeat;
    }
}

wgpu::FilterMode ToWebGPUFilterMode(Filter filter) {
    switch (filter) {
        case Filter::Nearest: return wgpu::FilterMode::Nearest;
        case Filter::Linear:  return wgpu::FilterMode::Linear;
        default:
            WEBGPU_LOG_ERROR("Unsupported filter mode: " << static_cast<int>(filter));
            return wgpu::FilterMode::Linear;
    }
}

wgpu::MipmapFilterMode ToWebGPUMipMapFilterMode(Filter filter) {
    switch (filter) {
        case Filter::Nearest: return wgpu::MipmapFilterMode::Nearest;
        case Filter::Linear:  return wgpu::MipmapFilterMode::Linear;
        default:
            WEBGPU_LOG_ERROR("Unsupported mipmap filter mode: " << static_cast<int>(filter));
            return wgpu::MipmapFilterMode::Linear;
    }
}

wgpu::CompareFunction ToWebGPUCompareFunction(CompareOp op) {
    switch (op) {
        case CompareOp::Never:          return wgpu::CompareFunction::Never;
        case CompareOp::Less:           return wgpu::CompareFunction::Less;
        case CompareOp::Equal:          return wgpu::CompareFunction::Equal;
        case CompareOp::LessOrEqual:    return wgpu::CompareFunction::LessEqual;
        case CompareOp::Greater:        return wgpu::CompareFunction::Greater;
        case CompareOp::NotEqual:       return wgpu::CompareFunction::NotEqual;
        case CompareOp::GreaterOrEqual: return wgpu::CompareFunction::GreaterEqual;
        case CompareOp::Always:         return wgpu::CompareFunction::Always;
        default:
            WEBGPU_LOG_ERROR("Unsupported compare function: " << static_cast<int>(op));
            return wgpu::CompareFunction::Always;
    }
}

wgpu::PrimitiveTopology ToWebGPUPrimitiveTopology(PrimitiveTopology topology) {
    switch (topology) {
        case PrimitiveTopology::TriangleList:  return wgpu::PrimitiveTopology::TriangleList;
        case PrimitiveTopology::TriangleStrip: return wgpu::PrimitiveTopology::TriangleStrip;
        case PrimitiveTopology::LineList:      return wgpu::PrimitiveTopology::LineList;
        case PrimitiveTopology::LineStrip:     return wgpu::PrimitiveTopology::LineStrip;
        case PrimitiveTopology::PointList:     return wgpu::PrimitiveTopology::PointList;
        default:
            WEBGPU_LOG_ERROR("Unsupported primitive topology: " << static_cast<int>(topology));
            return wgpu::PrimitiveTopology::TriangleList;
    }
}

wgpu::CullMode ToWebGPUCullMode(CullMode mode) {
    switch (mode) {
        case CullMode::None:  return wgpu::CullMode::None;
        case CullMode::Front: return wgpu::CullMode::Front;
        case CullMode::Back:  return wgpu::CullMode::Back;
        case CullMode::FrontAndBack:
            // WebGPU doesn't support culling both faces, default to back
            WEBGPU_LOG_ERROR("WebGPU doesn't support FrontAndBack cull mode, using Back");
            return wgpu::CullMode::Back;
        default:
            WEBGPU_LOG_ERROR("Unsupported cull mode: " << static_cast<int>(mode));
            return wgpu::CullMode::None;
    }
}

wgpu::FrontFace ToWebGPUFrontFace(FrontFace face) {
    switch (face) {
        case FrontFace::Clockwise:        return wgpu::FrontFace::CW;
        case FrontFace::CounterClockwise: return wgpu::FrontFace::CCW;
        default:
            WEBGPU_LOG_ERROR("Unsupported front face: " << static_cast<int>(face));
            return wgpu::FrontFace::CCW;
    }
}

wgpu::VertexFormat ToWebGPUVertexFormat(Format format) {
    switch (format) {
        // 8-bit formats
        case Format::R8_UINT:  return wgpu::VertexFormat::Uint8x2;
        case Format::R8_SINT:  return wgpu::VertexFormat::Sint8x2;
        case Format::R8_UNORM: return wgpu::VertexFormat::Unorm8x2;
        case Format::R8_SNORM: return wgpu::VertexFormat::Snorm8x2;

        // 16-bit formats
        case Format::R16_UINT:  return wgpu::VertexFormat::Uint16x2;
        case Format::R16_SINT:  return wgpu::VertexFormat::Sint16x2;
        case Format::R16_UNORM: return wgpu::VertexFormat::Unorm16x2;
        case Format::R16_SNORM: return wgpu::VertexFormat::Snorm16x2;
        case Format::R16_SFLOAT: return wgpu::VertexFormat::Float16x2;

        // 32-bit formats
        case Format::R32_UINT:   return wgpu::VertexFormat::Uint32;
        case Format::R32_SINT:   return wgpu::VertexFormat::Sint32;
        case Format::R32_SFLOAT: return wgpu::VertexFormat::Float32;

        // Two component formats
        case Format::R8G8_UINT:  return wgpu::VertexFormat::Uint8x2;
        case Format::R8G8_SINT:  return wgpu::VertexFormat::Sint8x2;
        case Format::R8G8_UNORM: return wgpu::VertexFormat::Unorm8x2;
        case Format::R8G8_SNORM: return wgpu::VertexFormat::Snorm8x2;

        case Format::R16G16_UINT:  return wgpu::VertexFormat::Uint16x2;
        case Format::R16G16_SINT:  return wgpu::VertexFormat::Sint16x2;
        case Format::R16G16_UNORM: return wgpu::VertexFormat::Unorm16x2;
        case Format::R16G16_SNORM: return wgpu::VertexFormat::Snorm16x2;
        case Format::R16G16_SFLOAT: return wgpu::VertexFormat::Float16x2;

        case Format::R32G32_UINT:   return wgpu::VertexFormat::Uint32x2;
        case Format::R32G32_SINT:   return wgpu::VertexFormat::Sint32x2;
        case Format::R32G32_SFLOAT: return wgpu::VertexFormat::Float32x2;

        // Three component formats
        case Format::R32G32B32_UINT:   return wgpu::VertexFormat::Uint32x3;
        case Format::R32G32B32_SINT:   return wgpu::VertexFormat::Sint32x3;
        case Format::R32G32B32_SFLOAT: return wgpu::VertexFormat::Float32x3;

        // Four component formats
        case Format::R8G8B8A8_UINT:  return wgpu::VertexFormat::Uint8x4;
        case Format::R8G8B8A8_SINT:  return wgpu::VertexFormat::Sint8x4;
        case Format::R8G8B8A8_UNORM: return wgpu::VertexFormat::Unorm8x4;
        case Format::R8G8B8A8_SNORM: return wgpu::VertexFormat::Snorm8x4;

        case Format::R16G16B16A16_UINT:  return wgpu::VertexFormat::Uint16x4;
        case Format::R16G16B16A16_SINT:  return wgpu::VertexFormat::Sint16x4;
        case Format::R16G16B16A16_UNORM: return wgpu::VertexFormat::Unorm16x4;
        case Format::R16G16B16A16_SNORM: return wgpu::VertexFormat::Snorm16x4;
        case Format::R16G16B16A16_SFLOAT: return wgpu::VertexFormat::Float16x4;

        case Format::R32G32B32A32_UINT:   return wgpu::VertexFormat::Uint32x4;
        case Format::R32G32B32A32_SINT:   return wgpu::VertexFormat::Sint32x4;
        case Format::R32G32B32A32_SFLOAT: return wgpu::VertexFormat::Float32x4;

        default:
            WEBGPU_LOG_ERROR("Unsupported vertex format: " << static_cast<int>(format));
            return wgpu::VertexFormat::Float32x3;
    }
}

wgpu::ShaderStage ToWebGPUShaderStage(ShaderStage stages) {
    wgpu::ShaderStage result = wgpu::ShaderStage::None;

    if (static_cast<int>(stages) & static_cast<int>(ShaderStage::Vertex))
        result |= wgpu::ShaderStage::Vertex;
    if (static_cast<int>(stages) & static_cast<int>(ShaderStage::Fragment))
        result |= wgpu::ShaderStage::Fragment;
    if (static_cast<int>(stages) & static_cast<int>(ShaderStage::Compute))
        result |= wgpu::ShaderStage::Compute;

    // WebGPU doesn't support geometry or tessellation shaders yet
    if (static_cast<int>(stages) & (static_cast<int>(ShaderStage::Geometry) |
                                    static_cast<int>(ShaderStage::TessellationControl) |
                                    static_cast<int>(ShaderStage::TessellationEvaluation))) {
        WEBGPU_LOG_ERROR("WebGPU doesn't support geometry or tessellation shaders");
    }

    return result;
}

wgpu::LoadOp ToWebGPULoadOp(bool shouldLoad) {
    return shouldLoad ? wgpu::LoadOp::Load : wgpu::LoadOp::Clear;
}

wgpu::StoreOp ToWebGPUStoreOp(bool shouldStore) {
    return shouldStore ? wgpu::StoreOp::Store : wgpu::StoreOp::Discard;
}

} // namespace rhi
} // namespace metagfx
