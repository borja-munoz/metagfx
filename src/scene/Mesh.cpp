// ============================================================================
// src/scene/Mesh.cpp
// ============================================================================
#include "metagfx/scene/Mesh.h"
#include "metagfx/rhi/GraphicsDevice.h"
#include "metagfx/rhi/Types.h"
#include "metagfx/core/Logger.h"

namespace metagfx {

Mesh::Mesh() = default;

Mesh::~Mesh() {
    Cleanup();
}

Mesh::Mesh(Mesh&& other) noexcept
    : m_Vertices(std::move(other.m_Vertices))
    , m_Indices(std::move(other.m_Indices))
    , m_VertexBuffer(std::move(other.m_VertexBuffer))
    , m_IndexBuffer(std::move(other.m_IndexBuffer))
    , m_VertexCount(other.m_VertexCount)
    , m_IndexCount(other.m_IndexCount)
{
    other.m_VertexCount = 0;
    other.m_IndexCount = 0;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        Cleanup();
        
        m_Vertices = std::move(other.m_Vertices);
        m_Indices = std::move(other.m_Indices);
        m_VertexBuffer = std::move(other.m_VertexBuffer);
        m_IndexBuffer = std::move(other.m_IndexBuffer);
        m_VertexCount = other.m_VertexCount;
        m_IndexCount = other.m_IndexCount;
        
        other.m_VertexCount = 0;
        other.m_IndexCount = 0;
    }
    return *this;
}

bool Mesh::Initialize(rhi::GraphicsDevice* device,
                     const std::vector<Vertex>& vertices,
                     const std::vector<uint32_t>& indices) {
    if (!device || vertices.empty() || indices.empty()) {
        METAGFX_ERROR << "Mesh::Initialize - Invalid parameters";
        return false;
    }

    // Store vertices and indices
    m_Vertices = vertices;
    m_Indices = indices;
    m_VertexCount = static_cast<uint32_t>(vertices.size());
    m_IndexCount = static_cast<uint32_t>(indices.size());

    // Create vertex buffer
    rhi::BufferDesc vbDesc = {};
    vbDesc.size = static_cast<uint32_t>(vertices.size() * sizeof(Vertex));
    vbDesc.usage = rhi::BufferUsage::Vertex | rhi::BufferUsage::TransferDst;
    vbDesc.memoryUsage = rhi::MemoryUsage::CPUToGPU;

    m_VertexBuffer = device->CreateBuffer(vbDesc);
    if (!m_VertexBuffer) {
        METAGFX_ERROR << "Mesh::Initialize - Failed to create vertex buffer";
        return false;
    }
    m_VertexBuffer->CopyData(vertices.data(), vbDesc.size);

    // Create index buffer
    rhi::BufferDesc ibDesc = {};
    ibDesc.size = static_cast<uint32_t>(indices.size() * sizeof(uint32_t));
    ibDesc.usage = rhi::BufferUsage::Index | rhi::BufferUsage::TransferDst;
    ibDesc.memoryUsage = rhi::MemoryUsage::CPUToGPU;

    m_IndexBuffer = device->CreateBuffer(ibDesc);
    if (!m_IndexBuffer) {
        METAGFX_ERROR << "Mesh::Initialize - Failed to create index buffer";
        m_VertexBuffer.reset();
        return false;
    }
    m_IndexBuffer->CopyData(indices.data(), ibDesc.size);

    METAGFX_INFO << "Mesh initialized: " <<  m_VertexCount << " vertices, " << m_IndexCount << " indices";
    return true;
}

void Mesh::Cleanup() {
    m_VertexBuffer.reset();
    m_IndexBuffer.reset();
    m_Vertices.clear();
    m_Indices.clear();
    m_VertexCount = 0;
    m_IndexCount = 0;
}

} // namespace metagfx