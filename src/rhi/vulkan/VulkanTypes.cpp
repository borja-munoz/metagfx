// ============================================================================
// src/rhi/vulkan/VulkanTypes.cpp
// ============================================================================
#include "metagfx/rhi/vulkan/VulkanTypes.h"

namespace metagfx {
namespace rhi {

VkFormat ToVulkanFormat(Format format) {
    switch (format) {
        case Format::R8_UNORM: return VK_FORMAT_R8_UNORM;
        case Format::R8G8B8A8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
        case Format::R8G8B8A8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
        case Format::B8G8R8A8_UNORM: return VK_FORMAT_B8G8R8A8_UNORM;
        case Format::B8G8R8A8_SRGB: return VK_FORMAT_B8G8R8A8_SRGB;
        case Format::R32_SFLOAT: return VK_FORMAT_R32_SFLOAT;
        case Format::R32G32_SFLOAT: return VK_FORMAT_R32G32_SFLOAT;
        case Format::R32G32B32_SFLOAT: return VK_FORMAT_R32G32B32_SFLOAT;
        case Format::R32G32B32A32_SFLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case Format::D32_SFLOAT: return VK_FORMAT_D32_SFLOAT;
        case Format::D24_UNORM_S8_UINT: return VK_FORMAT_D24_UNORM_S8_UINT;
        default: return VK_FORMAT_UNDEFINED;
    }
}

Format FromVulkanFormat(VkFormat format) {
    switch (format) {
        case VK_FORMAT_R8_UNORM: return Format::R8_UNORM;
        case VK_FORMAT_R8G8B8A8_UNORM: return Format::R8G8B8A8_UNORM;
        case VK_FORMAT_R8G8B8A8_SRGB: return Format::R8G8B8A8_SRGB;
        case VK_FORMAT_B8G8R8A8_UNORM: return Format::B8G8R8A8_UNORM;
        case VK_FORMAT_B8G8R8A8_SRGB: return Format::B8G8R8A8_SRGB;
        case VK_FORMAT_R32_SFLOAT: return Format::R32_SFLOAT;
        case VK_FORMAT_R32G32_SFLOAT: return Format::R32G32_SFLOAT;
        case VK_FORMAT_R32G32B32_SFLOAT: return Format::R32G32B32_SFLOAT;
        case VK_FORMAT_R32G32B32A32_SFLOAT: return Format::R32G32B32A32_SFLOAT;
        case VK_FORMAT_D32_SFLOAT: return Format::D32_SFLOAT;
        case VK_FORMAT_D24_UNORM_S8_UINT: return Format::D24_UNORM_S8_UINT;
        default: return Format::Undefined;
    }
}

VkBufferUsageFlags ToVulkanBufferUsage(BufferUsage usage) {
    VkBufferUsageFlags flags = 0;
    if ((usage & BufferUsage::Vertex) != BufferUsage{})
        flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if ((usage & BufferUsage::Index) != BufferUsage{})
        flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if ((usage & BufferUsage::Uniform) != BufferUsage{})
        flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if ((usage & BufferUsage::Storage) != BufferUsage{})
        flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if ((usage & BufferUsage::TransferSrc) != BufferUsage{})
        flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if ((usage & BufferUsage::TransferDst) != BufferUsage{})
        flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    return flags;
}

VkMemoryPropertyFlags ToVulkanMemoryUsage(MemoryUsage usage) {
    switch (usage) {
        case MemoryUsage::GPUOnly:
            return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        case MemoryUsage::CPUToGPU:
            return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        case MemoryUsage::GPUToCPU:
            return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        case MemoryUsage::CPUOnly:
            return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        default:
            return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    }
}

VkShaderStageFlagBits ToVulkanShaderStage(ShaderStage stage) {
    switch (stage) {
        case ShaderStage::Vertex: return VK_SHADER_STAGE_VERTEX_BIT;
        case ShaderStage::Fragment: return VK_SHADER_STAGE_FRAGMENT_BIT;
        case ShaderStage::Compute: return VK_SHADER_STAGE_COMPUTE_BIT;
        case ShaderStage::Geometry: return VK_SHADER_STAGE_GEOMETRY_BIT;
        case ShaderStage::TessellationControl: return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        case ShaderStage::TessellationEvaluation: return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        default: return VK_SHADER_STAGE_VERTEX_BIT;
    }
}

VkPrimitiveTopology ToVulkanTopology(PrimitiveTopology topology) {
    switch (topology) {
        case PrimitiveTopology::TriangleList: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case PrimitiveTopology::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case PrimitiveTopology::LineList: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case PrimitiveTopology::LineStrip: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case PrimitiveTopology::PointList: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        default: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }
}

VkPolygonMode ToVulkanPolygonMode(PolygonMode mode) {
    switch (mode) {
        case PolygonMode::Fill: return VK_POLYGON_MODE_FILL;
        case PolygonMode::Line: return VK_POLYGON_MODE_LINE;
        case PolygonMode::Point: return VK_POLYGON_MODE_POINT;
        default: return VK_POLYGON_MODE_FILL;
    }
}

VkCullModeFlags ToVulkanCullMode(CullMode mode) {
    switch (mode) {
        case CullMode::None: return VK_CULL_MODE_NONE;
        case CullMode::Front: return VK_CULL_MODE_FRONT_BIT;
        case CullMode::Back: return VK_CULL_MODE_BACK_BIT;
        case CullMode::FrontAndBack: return VK_CULL_MODE_FRONT_AND_BACK;
        default: return VK_CULL_MODE_BACK_BIT;
    }
}

VkFrontFace ToVulkanFrontFace(FrontFace face) {
    switch (face) {
        case FrontFace::Clockwise: return VK_FRONT_FACE_CLOCKWISE;
        case FrontFace::CounterClockwise: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
        default: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
    }
}

VkCompareOp ToVulkanCompareOp(CompareOp op) {
    switch (op) {
        case CompareOp::Never: return VK_COMPARE_OP_NEVER;
        case CompareOp::Less: return VK_COMPARE_OP_LESS;
        case CompareOp::Equal: return VK_COMPARE_OP_EQUAL;
        case CompareOp::LessOrEqual: return VK_COMPARE_OP_LESS_OR_EQUAL;
        case CompareOp::Greater: return VK_COMPARE_OP_GREATER;
        case CompareOp::NotEqual: return VK_COMPARE_OP_NOT_EQUAL;
        case CompareOp::GreaterOrEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case CompareOp::Always: return VK_COMPARE_OP_ALWAYS;
        default: return VK_COMPARE_OP_LESS;
    }
}

} // namespace rhi
} // namespace metagfx
