// ============================================================================
// include/metagfx/rhi/metal/MetalTypes.h
// ============================================================================
#pragma once

#include "metagfx/core/Types.h"
#include "metagfx/rhi/Types.h"

// metal-cpp headers
// Note: NS/MTL/CA_PRIVATE_IMPLEMENTATION are defined in MetalTypes.cpp
#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

struct SDL_Window;
typedef void* SDL_MetalView;

namespace metagfx {
namespace rhi {

// Helper to log Metal errors
#define MTL_LOG_ERROR(msg) \
    METAGFX_ERROR << "Metal error: " << msg << " at " << __FILE__ << ":" << __LINE__

// Metal context shared across all Metal objects
struct MetalContext {
    MTL::Device* device = nullptr;
    MTL::CommandQueue* commandQueue = nullptr;
    CA::MetalLayer* metalLayer = nullptr;
    SDL_MetalView metalView = nullptr;

    // Device capabilities
    bool supportsArgumentBuffers = false;
    bool supportsRayTracing = false;
};

// Format conversion utilities
MTL::PixelFormat ToMetalPixelFormat(Format format);
Format FromMetalPixelFormat(MTL::PixelFormat format);

MTL::ResourceOptions ToMetalResourceOptions(MemoryUsage usage);
MTL::PrimitiveType ToMetalPrimitiveType(PrimitiveTopology topology);
MTL::TriangleFillMode ToMetalPolygonMode(PolygonMode mode);
MTL::CullMode ToMetalCullMode(CullMode mode);
MTL::Winding ToMetalFrontFace(FrontFace face);
MTL::CompareFunction ToMetalCompareFunction(CompareOp op);
MTL::VertexFormat ToMetalVertexFormat(Format format);
MTL::SamplerMinMagFilter ToMetalFilter(Filter filter);
MTL::SamplerMipFilter ToMetalMipFilter(Filter filter);
MTL::SamplerAddressMode ToMetalAddressMode(SamplerAddressMode mode);

} // namespace rhi
} // namespace metagfx
