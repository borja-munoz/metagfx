// ============================================================================
// include/metagfx/scene/Mesh.h
// ============================================================================
#pragma once

#include "metagfx/core/Types.h"
#include "metagfx/rhi/Buffer.h"
#include "metagfx/rhi/Types.h"
#include <glm/glm.hpp>
#include <vector>
#include <memory>

namespace metagfx {

// Forward declarations
namespace rhi {
    class GraphicsDevice;
    class Buffer;
}

/**
 * @brief Vertex structure containing position, normal, and texture coordinates
 */
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;

    Vertex() = default;
    Vertex(const glm::vec3& pos, const glm::vec3& norm, const glm::vec2& uv)
        : position(pos), normal(norm), texCoord(uv) {}

    bool operator==(const Vertex& other) const {
        return position == other.position && 
               normal == other.normal && 
               texCoord == other.texCoord;
    }
};

/**
 * @brief Mesh class holding geometry data and GPU buffers
 * 
 * A mesh represents a single drawable piece of geometry with vertices and indices.
 * It owns the GPU buffers for vertex and index data.
 */
class Mesh {
public:
    Mesh();
    ~Mesh();

    // Non-copyable but movable
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&&) noexcept;
    Mesh& operator=(Mesh&&) noexcept;

    /**
     * @brief Initialize mesh with vertex and index data
     * @param device Graphics device to create buffers
     * @param vertices Vector of vertex data
     * @param indices Vector of index data
     * @return true if successful, false otherwise
     */
    bool Initialize(rhi::GraphicsDevice* device,
                   const std::vector<Vertex>& vertices,
                   const std::vector<uint32_t>& indices);

    /**
     * @brief Clean up GPU resources
     */
    void Cleanup();

    /**
     * @brief Check if mesh has been initialized
     */
    bool IsValid() const { return m_VertexBuffer != nullptr && m_IndexBuffer != nullptr; }

    // Getters
    Ref<rhi::Buffer> GetVertexBuffer() const { return m_VertexBuffer; }
    Ref<rhi::Buffer> GetIndexBuffer() const { return m_IndexBuffer; }
    uint32_t GetVertexCount() const { return m_VertexCount; }
    uint32_t GetIndexCount() const { return m_IndexCount; }

    const std::vector<Vertex>& GetVertices() const { return m_Vertices; }
    const std::vector<uint32_t>& GetIndices() const { return m_Indices; }

private:
    std::vector<Vertex> m_Vertices;
    std::vector<uint32_t> m_Indices;

    Ref<rhi::Buffer> m_VertexBuffer;
    Ref<rhi::Buffer> m_IndexBuffer;
    
    uint32_t m_VertexCount = 0;
    uint32_t m_IndexCount = 0;
};

} // namespace metagfx