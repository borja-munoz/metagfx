// ============================================================================
// include/metagfx/rhi/vulkan/VulkanBuffer.h
// ============================================================================
#pragma once

#include "metagfx/rhi/Buffer.h"
#include "VulkanTypes.h"

namespace metagfx {
namespace rhi {

class VulkanBuffer : public Buffer {
public:
    VulkanBuffer(VulkanContext& context, const BufferDesc& desc);
    ~VulkanBuffer() override;

    void* Map() override;
    void Unmap() override;
    void CopyData(const void* data, uint64 size, uint64 offset = 0) override;
    
    uint64 GetSize() const override { return m_Size; }
    BufferUsage GetUsage() const override { return m_Usage; }
    
    // Vulkan-specific
    VkBuffer GetHandle() const { return m_Buffer; }

private:
    VulkanContext& m_Context;
    VkBuffer m_Buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_Memory = VK_NULL_HANDLE;
    
    uint64 m_Size = 0;
    BufferUsage m_Usage;
    MemoryUsage m_MemoryUsage;
    
    void* m_MappedData = nullptr;
};

} // namespace rhi
} // namespace metagfx
