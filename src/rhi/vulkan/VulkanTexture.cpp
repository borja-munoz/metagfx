// ============================================================================
// src/rhi/vulkan/VulkanTexture.cpp
// ============================================================================
#include "metagfx/core/Logger.h"
#include "metagfx/rhi/vulkan/VulkanTexture.h"

namespace metagfx {
namespace rhi {

// Helper function to get the size in bytes of a format
static uint32 GetFormatSize(Format format) {
    switch (format) {
        case Format::R8_UNORM:
            return 1;
        case Format::R8G8B8A8_UNORM:
        case Format::R8G8B8A8_SRGB:
        case Format::B8G8R8A8_UNORM:
        case Format::B8G8R8A8_SRGB:
            return 4;
        case Format::R16G16_SFLOAT:
            return 4; // 2 channels * 2 bytes
        case Format::R16G16B16A16_SFLOAT:
            return 8; // 4 channels * 2 bytes
        case Format::R32_SFLOAT:
            return 4;
        case Format::R32G32_SFLOAT:
            return 8;
        case Format::R32G32B32_SFLOAT:
            return 12;
        case Format::R32G32B32A32_SFLOAT:
            return 16;
        case Format::D32_SFLOAT:
            return 4;
        case Format::D24_UNORM_S8_UINT:
            return 4;
        default:
            return 4; // Default to 4 bytes
    }
}

VulkanTexture::VulkanTexture(VulkanContext& context, VkImage image, VkImageView imageView,
                            uint32 width, uint32 height, VkFormat format)
    : m_Context(context), m_Image(image), m_ImageView(imageView),
      m_Width(width), m_Height(height), m_VkFormat(format), m_OwnsImage(false) {
    m_Format = FromVulkanFormat(format);
}

VulkanTexture::VulkanTexture(VulkanContext& context, const TextureDesc& desc)
    : m_Context(context), m_Width(desc.width), m_Height(desc.height),
      m_MipLevels(desc.mipLevels), m_ArrayLayers(desc.arrayLayers),
      m_Type(desc.type), m_Format(desc.format), m_OwnsImage(true) {

    m_VkFormat = ToVulkanFormat(desc.format);

    // Create VkImage
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = desc.width;
    imageInfo.extent.height = desc.height;
    imageInfo.extent.depth = desc.depth;
    imageInfo.mipLevels = desc.mipLevels;
    imageInfo.arrayLayers = desc.arrayLayers;
    imageInfo.format = m_VkFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    // Add cube map flag if texture type is cube
    if (desc.type == TextureType::TextureCube) {
        imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    // Convert usage flags
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;  // For uploading data
    if (static_cast<uint32>(desc.usage) & static_cast<uint32>(TextureUsage::Sampled)) {
        imageInfo.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }
    if (static_cast<uint32>(desc.usage) & static_cast<uint32>(TextureUsage::ColorAttachment)) {
        imageInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }
    if (static_cast<uint32>(desc.usage) & static_cast<uint32>(TextureUsage::DepthStencilAttachment)) {
        imageInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }

    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateImage(m_Context.device, &imageInfo, nullptr, &m_Image));

    // Allocate memory
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_Context.device, m_Image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK(vkAllocateMemory(m_Context.device, &allocInfo, nullptr, &m_Memory));
    VK_CHECK(vkBindImageMemory(m_Context.device, m_Image, m_Memory, 0));

    // Create image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_Image;

    // Set view type based on texture type
    if (desc.type == TextureType::TextureCube) {
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    } else if (desc.type == TextureType::Texture3D) {
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
    } else {
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    }

    viewInfo.format = m_VkFormat;

    // Set correct aspect mask based on format
    bool isDepthFormat = (static_cast<uint32>(desc.usage) & static_cast<uint32>(TextureUsage::DepthStencilAttachment)) != 0;
    viewInfo.subresourceRange.aspectMask = isDepthFormat ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = desc.mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = desc.arrayLayers;

    VK_CHECK(vkCreateImageView(m_Context.device, &viewInfo, nullptr, &m_ImageView));

    // Don't transition layout here - let UploadData handle it for sampled textures
    // Depth attachments are transitioned by the render pass

    if (desc.type == TextureType::TextureCube) {
        METAGFX_INFO << "Created cubemap texture: " << desc.width << "x" << desc.height
                     << " with " << desc.mipLevels << " mip levels";
    } else {
        METAGFX_INFO << "Created texture: " << desc.width << "x" << desc.height
                     << " with " << desc.mipLevels << " mip levels";
    }
}

VulkanTexture::~VulkanTexture() {
    if (m_OwnsImage) {
        if (m_ImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(m_Context.device, m_ImageView, nullptr);
        }

        if (m_Image != VK_NULL_HANDLE) {
            vkDestroyImage(m_Context.device, m_Image, nullptr);
        }

        if (m_Memory != VK_NULL_HANDLE) {
            vkFreeMemory(m_Context.device, m_Memory, nullptr);
        }
    }
}

void VulkanTexture::UploadData(const void* data, uint64 size) {
    // Create staging buffer
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer stagingBuffer;
    VK_CHECK(vkCreateBuffer(m_Context.device, &bufferInfo, nullptr, &stagingBuffer));

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_Context.device, stagingBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkDeviceMemory stagingMemory;
    VK_CHECK(vkAllocateMemory(m_Context.device, &allocInfo, nullptr, &stagingMemory));
    VK_CHECK(vkBindBufferMemory(m_Context.device, stagingBuffer, stagingMemory, 0));

    // Copy data to staging buffer
    void* mappedData;
    VK_CHECK(vkMapMemory(m_Context.device, stagingMemory, 0, size, 0, &mappedData));
    memcpy(mappedData, data, size);

    // DEBUG: Print first few bytes of mip 1 data for cubemaps
    if (m_ArrayLayers == 6 && m_MipLevels > 1) {
        const uint8* byteData = static_cast<const uint8*>(data);
        uint64 mip0Size = static_cast<uint64>(m_Width) * m_Height * GetFormatSize(m_Format) * 6;
        if (size > mip0Size + 16) {
            METAGFX_INFO << "First 16 bytes of Mip 1 data in buffer:";
            for (int i = 0; i < 16; i++) {
                METAGFX_INFO << "  Byte " << i << " at offset " << (mip0Size + i) << ": " << static_cast<int>(byteData[mip0Size + i]);
            }
        }
    }

    vkUnmapMemory(m_Context.device, stagingMemory);

    // Create command buffer for copy operation
    VkCommandBufferAllocateInfo cmdAllocInfo{};
    cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandPool = m_Context.commandPool;
    cmdAllocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(m_Context.device, &cmdAllocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    // Transition to transfer destination layout
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_Image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = m_MipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = m_ArrayLayers;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(commandBuffer,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Copy buffer to image (all mip levels and layers)
    // NOTE: Upload each face separately for better MoltenVK compatibility
    std::vector<VkBufferImageCopy> regions;
    uint64 bufferOffset = 0;

    METAGFX_INFO << "Setting up texture upload regions:";
    METAGFX_INFO << "  Total buffer size: " << size << " bytes";
    METAGFX_INFO << "  Texture dimensions: " << m_Width << "x" << m_Height;
    METAGFX_INFO << "  Mip levels: " << m_MipLevels;
    METAGFX_INFO << "  Array layers: " << m_ArrayLayers;
    METAGFX_INFO << "  Format size: " << GetFormatSize(m_Format) << " bytes/pixel";

    for (uint32 mip = 0; mip < m_MipLevels; ++mip) {
        uint32 mipWidth = std::max(1u, m_Width >> mip);
        uint32 mipHeight = std::max(1u, m_Height >> mip);
        uint32 bytesPerPixel = GetFormatSize(m_Format);
        uint64 faceSize = static_cast<uint64>(mipWidth) * mipHeight * bytesPerPixel;

        // Upload each array layer (cubemap face) separately
        for (uint32 layer = 0; layer < m_ArrayLayers; ++layer) {
            VkBufferImageCopy region{};
            region.bufferOffset = bufferOffset;
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = mip;
            region.imageSubresource.baseArrayLayer = layer;
            region.imageSubresource.layerCount = 1;  // One face at a time
            region.imageOffset = {0, 0, 0};
            region.imageExtent = {mipWidth, mipHeight, 1};

            regions.push_back(region);
            bufferOffset += faceSize;
        }

        METAGFX_INFO << "  Mip " << mip << ": " << m_ArrayLayers << " faces, "
                     << faceSize << " bytes/face, "
                     << "dimensions=" << mipWidth << "x" << mipHeight;
    }

    METAGFX_INFO << "  Total regions: " << regions.size();
    METAGFX_INFO << "  Total calculated size: " << bufferOffset << " bytes";

    vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, m_Image,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          static_cast<uint32>(regions.size()), regions.data());

    // Transition to shader read layout
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(commandBuffer);

    // Submit and wait
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(m_Context.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_Context.graphicsQueue);

    // Cleanup
    vkFreeCommandBuffers(m_Context.device, m_Context.commandPool, 1, &commandBuffer);
    vkDestroyBuffer(m_Context.device, stagingBuffer, nullptr);
    vkFreeMemory(m_Context.device, stagingMemory, nullptr);

    METAGFX_INFO << "Uploaded " << size << " bytes to texture";
}

void VulkanTexture::TransitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_Context.commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(m_Context.device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_Image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = m_MipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = m_ArrayLayers;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        METAGFX_ERROR << "Unsupported layout transition";
        return;
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage,
                        0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(m_Context.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_Context.graphicsQueue);

    vkFreeCommandBuffers(m_Context.device, m_Context.commandPool, 1, &commandBuffer);
}

uint32 VulkanTexture::FindMemoryType(uint32 typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_Context.physicalDevice, &memProperties);

    for (uint32 i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    METAGFX_ERROR << "Failed to find suitable memory type";
    return 0;
}

} // namespace rhi
} // namespace metagfx