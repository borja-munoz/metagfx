// ============================================================================
// src/rhi/vulkan/VulkanBuffer.cpp
// ============================================================================
#include "metagfx/core/Logger.h"
#include "metagfx/rhi/vulkan/VulkanBuffer.h"
#include "metagfx/rhi/vulkan/VulkanDevice.h"

namespace metagfx {
namespace rhi {

VulkanBuffer::VulkanBuffer(VulkanContext& context, const BufferDesc& desc)
    : m_Context(context), m_Size(desc.size), m_Usage(desc.usage), m_MemoryUsage(desc.memoryUsage) {
    
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = desc.size;
    bufferInfo.usage = ToVulkanBufferUsage(desc.usage);
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VK_CHECK(vkCreateBuffer(m_Context.device, &bufferInfo, nullptr, &m_Buffer));
    
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_Context.device, m_Buffer, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    
    // Find memory type
    VkMemoryPropertyFlags properties = ToVulkanMemoryUsage(desc.memoryUsage);
    allocInfo.memoryTypeIndex = 0;
    for (uint32 i = 0; i < m_Context.memoryProperties.memoryTypeCount; i++) {
        if ((memRequirements.memoryTypeBits & (1 << i)) &&
            (m_Context.memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            allocInfo.memoryTypeIndex = i;
            break;
        }
    }
    
    VK_CHECK(vkAllocateMemory(m_Context.device, &allocInfo, nullptr, &m_Memory));
    VK_CHECK(vkBindBufferMemory(m_Context.device, m_Buffer, m_Memory, 0));
}

VulkanBuffer::~VulkanBuffer() {
    if (m_MappedData != nullptr) {
        Unmap();
    }
    
    if (m_Buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_Context.device, m_Buffer, nullptr);
    }
    
    if (m_Memory != VK_NULL_HANDLE) {
        vkFreeMemory(m_Context.device, m_Memory, nullptr);
    }
}

void* VulkanBuffer::Map() {
    if (m_MappedData == nullptr) {
        VK_CHECK(vkMapMemory(m_Context.device, m_Memory, 0, m_Size, 0, &m_MappedData));
    }
    return m_MappedData;
}

void VulkanBuffer::Unmap() {
    if (m_MappedData != nullptr) {
        vkUnmapMemory(m_Context.device, m_Memory);
        m_MappedData = nullptr;
    }
}

void VulkanBuffer::CopyData(const void* data, uint64 size, uint64 offset) {
    void* mapped = Map();
    memcpy(static_cast<char*>(mapped) + offset, data, size);

    // Flush mapped memory to make writes visible to GPU
    VkMappedMemoryRange range{};
    range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range.memory = m_Memory;
    range.offset = offset;
    range.size = size;
    VK_CHECK(vkFlushMappedMemoryRanges(m_Context.device, 1, &range));

    Unmap();
}

} // namespace rhi
} // namespace metagfx

