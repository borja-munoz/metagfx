// ============================================================================
// src/rhi/metal/MetalTypes.cpp
// ============================================================================

// metal-cpp private implementation macros
// These must be defined exactly once in the project before including metal-cpp headers
#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION

#include "metagfx/rhi/metal/MetalTypes.h"

namespace metagfx {
namespace rhi {

MTL::PixelFormat ToMetalPixelFormat(Format format) {
    switch (format) {
        // 8-bit formats
        case Format::R8_UNORM: return MTL::PixelFormatR8Unorm;
        case Format::R8_SNORM: return MTL::PixelFormatR8Snorm;
        case Format::R8_UINT: return MTL::PixelFormatR8Uint;
        case Format::R8_SINT: return MTL::PixelFormatR8Sint;

        // 16-bit formats
        case Format::R16_UNORM: return MTL::PixelFormatR16Unorm;
        case Format::R16_SNORM: return MTL::PixelFormatR16Snorm;
        case Format::R16_UINT: return MTL::PixelFormatR16Uint;
        case Format::R16_SINT: return MTL::PixelFormatR16Sint;
        case Format::R16_SFLOAT: return MTL::PixelFormatR16Float;

        // 32-bit formats
        case Format::R32_UINT: return MTL::PixelFormatR32Uint;
        case Format::R32_SINT: return MTL::PixelFormatR32Sint;
        case Format::R32_SFLOAT: return MTL::PixelFormatR32Float;

        // Two component 8-bit
        case Format::R8G8_UNORM: return MTL::PixelFormatRG8Unorm;
        case Format::R8G8_SNORM: return MTL::PixelFormatRG8Snorm;
        case Format::R8G8_UINT: return MTL::PixelFormatRG8Uint;
        case Format::R8G8_SINT: return MTL::PixelFormatRG8Sint;

        // Two component 16-bit
        case Format::R16G16_UNORM: return MTL::PixelFormatRG16Unorm;
        case Format::R16G16_SNORM: return MTL::PixelFormatRG16Snorm;
        case Format::R16G16_UINT: return MTL::PixelFormatRG16Uint;
        case Format::R16G16_SINT: return MTL::PixelFormatRG16Sint;
        case Format::R16G16_SFLOAT: return MTL::PixelFormatRG16Float;

        // Two component 32-bit
        case Format::R32G32_UINT: return MTL::PixelFormatRG32Uint;
        case Format::R32G32_SINT: return MTL::PixelFormatRG32Sint;
        case Format::R32G32_SFLOAT: return MTL::PixelFormatRG32Float;

        // Three component 32-bit (Metal doesn't have RGB32, use RGBA32)
        case Format::R32G32B32_UINT: return MTL::PixelFormatRGBA32Uint;
        case Format::R32G32B32_SINT: return MTL::PixelFormatRGBA32Sint;
        case Format::R32G32B32_SFLOAT: return MTL::PixelFormatRGBA32Float;

        // Four component 8-bit
        case Format::R8G8B8A8_UNORM: return MTL::PixelFormatRGBA8Unorm;
        case Format::R8G8B8A8_SNORM: return MTL::PixelFormatRGBA8Snorm;
        case Format::R8G8B8A8_UINT: return MTL::PixelFormatRGBA8Uint;
        case Format::R8G8B8A8_SINT: return MTL::PixelFormatRGBA8Sint;
        case Format::R8G8B8A8_SRGB: return MTL::PixelFormatRGBA8Unorm_sRGB;

        case Format::B8G8R8A8_UNORM: return MTL::PixelFormatBGRA8Unorm;
        case Format::B8G8R8A8_SRGB: return MTL::PixelFormatBGRA8Unorm_sRGB;

        // Four component 16-bit
        case Format::R16G16B16A16_UNORM: return MTL::PixelFormatRGBA16Unorm;
        case Format::R16G16B16A16_SNORM: return MTL::PixelFormatRGBA16Snorm;
        case Format::R16G16B16A16_UINT: return MTL::PixelFormatRGBA16Uint;
        case Format::R16G16B16A16_SINT: return MTL::PixelFormatRGBA16Sint;
        case Format::R16G16B16A16_SFLOAT: return MTL::PixelFormatRGBA16Float;

        // Four component 32-bit
        case Format::R32G32B32A32_UINT: return MTL::PixelFormatRGBA32Uint;
        case Format::R32G32B32A32_SINT: return MTL::PixelFormatRGBA32Sint;
        case Format::R32G32B32A32_SFLOAT: return MTL::PixelFormatRGBA32Float;

        // Depth/Stencil formats
        case Format::D16_UNORM: return MTL::PixelFormatDepth16Unorm;
        case Format::D32_SFLOAT: return MTL::PixelFormatDepth32Float;
#if TARGET_OS_OSX
        case Format::D24_UNORM_S8_UINT: return MTL::PixelFormatDepth24Unorm_Stencil8;
#else
        case Format::D24_UNORM_S8_UINT: return MTL::PixelFormatDepth32Float_Stencil8;
#endif
        case Format::D32_SFLOAT_S8_UINT: return MTL::PixelFormatDepth32Float_Stencil8;

        case Format::Undefined:
        default:
            return MTL::PixelFormatInvalid;
    }
}

Format FromMetalPixelFormat(MTL::PixelFormat format) {
    switch (format) {
        case MTL::PixelFormatR8Unorm: return Format::R8_UNORM;
        case MTL::PixelFormatR8Snorm: return Format::R8_SNORM;
        case MTL::PixelFormatR8Uint: return Format::R8_UINT;
        case MTL::PixelFormatR8Sint: return Format::R8_SINT;

        case MTL::PixelFormatR16Unorm: return Format::R16_UNORM;
        case MTL::PixelFormatR16Snorm: return Format::R16_SNORM;
        case MTL::PixelFormatR16Uint: return Format::R16_UINT;
        case MTL::PixelFormatR16Sint: return Format::R16_SINT;
        case MTL::PixelFormatR16Float: return Format::R16_SFLOAT;

        case MTL::PixelFormatR32Uint: return Format::R32_UINT;
        case MTL::PixelFormatR32Sint: return Format::R32_SINT;
        case MTL::PixelFormatR32Float: return Format::R32_SFLOAT;

        case MTL::PixelFormatRG8Unorm: return Format::R8G8_UNORM;
        case MTL::PixelFormatRG8Snorm: return Format::R8G8_SNORM;
        case MTL::PixelFormatRG8Uint: return Format::R8G8_UINT;
        case MTL::PixelFormatRG8Sint: return Format::R8G8_SINT;

        case MTL::PixelFormatRG16Unorm: return Format::R16G16_UNORM;
        case MTL::PixelFormatRG16Snorm: return Format::R16G16_SNORM;
        case MTL::PixelFormatRG16Uint: return Format::R16G16_UINT;
        case MTL::PixelFormatRG16Sint: return Format::R16G16_SINT;
        case MTL::PixelFormatRG16Float: return Format::R16G16_SFLOAT;

        case MTL::PixelFormatRG32Uint: return Format::R32G32_UINT;
        case MTL::PixelFormatRG32Sint: return Format::R32G32_SINT;
        case MTL::PixelFormatRG32Float: return Format::R32G32_SFLOAT;

        case MTL::PixelFormatRGBA8Unorm: return Format::R8G8B8A8_UNORM;
        case MTL::PixelFormatRGBA8Snorm: return Format::R8G8B8A8_SNORM;
        case MTL::PixelFormatRGBA8Uint: return Format::R8G8B8A8_UINT;
        case MTL::PixelFormatRGBA8Sint: return Format::R8G8B8A8_SINT;
        case MTL::PixelFormatRGBA8Unorm_sRGB: return Format::R8G8B8A8_SRGB;

        case MTL::PixelFormatBGRA8Unorm: return Format::B8G8R8A8_UNORM;
        case MTL::PixelFormatBGRA8Unorm_sRGB: return Format::B8G8R8A8_SRGB;

        case MTL::PixelFormatRGBA16Unorm: return Format::R16G16B16A16_UNORM;
        case MTL::PixelFormatRGBA16Snorm: return Format::R16G16B16A16_SNORM;
        case MTL::PixelFormatRGBA16Uint: return Format::R16G16B16A16_UINT;
        case MTL::PixelFormatRGBA16Sint: return Format::R16G16B16A16_SINT;
        case MTL::PixelFormatRGBA16Float: return Format::R16G16B16A16_SFLOAT;

        case MTL::PixelFormatRGBA32Uint: return Format::R32G32B32A32_UINT;
        case MTL::PixelFormatRGBA32Sint: return Format::R32G32B32A32_SINT;
        case MTL::PixelFormatRGBA32Float: return Format::R32G32B32A32_SFLOAT;

        case MTL::PixelFormatDepth16Unorm: return Format::D16_UNORM;
        case MTL::PixelFormatDepth32Float: return Format::D32_SFLOAT;
#if TARGET_OS_OSX
        case MTL::PixelFormatDepth24Unorm_Stencil8: return Format::D24_UNORM_S8_UINT;
#endif
        case MTL::PixelFormatDepth32Float_Stencil8: return Format::D32_SFLOAT_S8_UINT;

        default:
            return Format::Undefined;
    }
}

MTL::ResourceOptions ToMetalResourceOptions(MemoryUsage usage) {
    switch (usage) {
        case MemoryUsage::GPUOnly:
            return MTL::ResourceStorageModePrivate;
        case MemoryUsage::CPUToGPU:
            return MTL::ResourceStorageModeShared | MTL::ResourceCPUCacheModeWriteCombined;
        case MemoryUsage::GPUToCPU:
            return MTL::ResourceStorageModeShared;
        case MemoryUsage::CPUOnly:
            return MTL::ResourceStorageModeShared;
        default:
            return MTL::ResourceStorageModePrivate;
    }
}

MTL::PrimitiveType ToMetalPrimitiveType(PrimitiveTopology topology) {
    switch (topology) {
        case PrimitiveTopology::TriangleList: return MTL::PrimitiveTypeTriangle;
        case PrimitiveTopology::TriangleStrip: return MTL::PrimitiveTypeTriangleStrip;
        case PrimitiveTopology::LineList: return MTL::PrimitiveTypeLine;
        case PrimitiveTopology::LineStrip: return MTL::PrimitiveTypeLineStrip;
        case PrimitiveTopology::PointList: return MTL::PrimitiveTypePoint;
        default: return MTL::PrimitiveTypeTriangle;
    }
}

MTL::TriangleFillMode ToMetalPolygonMode(PolygonMode mode) {
    switch (mode) {
        case PolygonMode::Fill: return MTL::TriangleFillModeFill;
        case PolygonMode::Line: return MTL::TriangleFillModeLines;
        case PolygonMode::Point: return MTL::TriangleFillModeLines; // Metal doesn't have point mode
        default: return MTL::TriangleFillModeFill;
    }
}

MTL::CullMode ToMetalCullMode(CullMode mode) {
    switch (mode) {
        case CullMode::None: return MTL::CullModeNone;
        case CullMode::Front: return MTL::CullModeFront;
        case CullMode::Back: return MTL::CullModeBack;
        case CullMode::FrontAndBack: return MTL::CullModeNone; // Metal doesn't support both
        default: return MTL::CullModeBack;
    }
}

MTL::Winding ToMetalFrontFace(FrontFace face) {
    switch (face) {
        case FrontFace::Clockwise: return MTL::WindingClockwise;
        case FrontFace::CounterClockwise: return MTL::WindingCounterClockwise;
        default: return MTL::WindingCounterClockwise;
    }
}

MTL::CompareFunction ToMetalCompareFunction(CompareOp op) {
    switch (op) {
        case CompareOp::Never: return MTL::CompareFunctionNever;
        case CompareOp::Less: return MTL::CompareFunctionLess;
        case CompareOp::Equal: return MTL::CompareFunctionEqual;
        case CompareOp::LessOrEqual: return MTL::CompareFunctionLessEqual;
        case CompareOp::Greater: return MTL::CompareFunctionGreater;
        case CompareOp::NotEqual: return MTL::CompareFunctionNotEqual;
        case CompareOp::GreaterOrEqual: return MTL::CompareFunctionGreaterEqual;
        case CompareOp::Always: return MTL::CompareFunctionAlways;
        default: return MTL::CompareFunctionLess;
    }
}

MTL::VertexFormat ToMetalVertexFormat(Format format) {
    switch (format) {
        case Format::R32_SFLOAT: return MTL::VertexFormatFloat;
        case Format::R32G32_SFLOAT: return MTL::VertexFormatFloat2;
        case Format::R32G32B32_SFLOAT: return MTL::VertexFormatFloat3;
        case Format::R32G32B32A32_SFLOAT: return MTL::VertexFormatFloat4;

        case Format::R32_SINT: return MTL::VertexFormatInt;
        case Format::R32G32_SINT: return MTL::VertexFormatInt2;
        case Format::R32G32B32_SINT: return MTL::VertexFormatInt3;
        case Format::R32G32B32A32_SINT: return MTL::VertexFormatInt4;

        case Format::R32_UINT: return MTL::VertexFormatUInt;
        case Format::R32G32_UINT: return MTL::VertexFormatUInt2;
        case Format::R32G32B32_UINT: return MTL::VertexFormatUInt3;
        case Format::R32G32B32A32_UINT: return MTL::VertexFormatUInt4;

        case Format::R8G8B8A8_UNORM: return MTL::VertexFormatUChar4Normalized;
        case Format::R8G8B8A8_SNORM: return MTL::VertexFormatChar4Normalized;

        case Format::R16G16_SFLOAT: return MTL::VertexFormatHalf2;
        case Format::R16G16B16A16_SFLOAT: return MTL::VertexFormatHalf4;

        default: return MTL::VertexFormatInvalid;
    }
}

MTL::SamplerMinMagFilter ToMetalFilter(Filter filter) {
    switch (filter) {
        case Filter::Nearest: return MTL::SamplerMinMagFilterNearest;
        case Filter::Linear: return MTL::SamplerMinMagFilterLinear;
        default: return MTL::SamplerMinMagFilterLinear;
    }
}

MTL::SamplerMipFilter ToMetalMipFilter(Filter filter) {
    switch (filter) {
        case Filter::Nearest: return MTL::SamplerMipFilterNearest;
        case Filter::Linear: return MTL::SamplerMipFilterLinear;
        default: return MTL::SamplerMipFilterLinear;
    }
}

MTL::SamplerAddressMode ToMetalAddressMode(SamplerAddressMode mode) {
    switch (mode) {
        case SamplerAddressMode::Repeat: return MTL::SamplerAddressModeRepeat;
        case SamplerAddressMode::MirroredRepeat: return MTL::SamplerAddressModeMirrorRepeat;
        case SamplerAddressMode::ClampToEdge: return MTL::SamplerAddressModeClampToEdge;
        case SamplerAddressMode::ClampToBorder: return MTL::SamplerAddressModeClampToBorderColor;
        default: return MTL::SamplerAddressModeRepeat;
    }
}

} // namespace rhi
} // namespace metagfx
