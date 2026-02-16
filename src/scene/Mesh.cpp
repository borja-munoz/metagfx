// ============================================================================
// src/scene/Mesh.cpp
// ============================================================================
#include "metagfx/scene/Mesh.h"
#include "metagfx/scene/Material.h"
#include "metagfx/rhi/GraphicsDevice.h"
#include "metagfx/rhi/Types.h"
#include "metagfx/core/Logger.h"
#include <meshoptimizer.h>

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
    , m_LODLevels(std::move(other.m_LODLevels))
    , m_Material(std::move(other.m_Material))
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
        m_LODLevels = std::move(other.m_LODLevels);
        m_Material = std::move(other.m_Material);

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

    // Generate LOD levels (LOD1 = 50%, LOD2 = 20%) using meshoptimizer
    const float lodRatios[2] = { 0.5f, 0.2f };
    for (int lod = 0; lod < 2; ++lod) {
        size_t targetCount = static_cast<size_t>((indices.size() / 3) * lodRatios[lod]) * 3;
        if (targetCount < 3) targetCount = 3;  // At least one triangle

        std::vector<uint32_t> lodIndices(indices.size());
        float targetError = 0.01f * (lod + 1);  // More error allowed for lower LODs
        size_t lodCount = meshopt_simplify(
            lodIndices.data(),
            indices.data(), indices.size(),
            &vertices[0].position.x, vertices.size(), sizeof(Vertex),
            targetCount, targetError);
        lodIndices.resize(lodCount);

        if (lodCount >= 3) {
            // Optimize simplified index buffer for GPU vertex cache
            meshopt_optimizeVertexCache(lodIndices.data(), lodIndices.data(), lodCount, vertices.size());

            rhi::BufferDesc lodDesc{};
            lodDesc.size = static_cast<uint32_t>(lodCount * sizeof(uint32_t));
            lodDesc.usage = rhi::BufferUsage::Index | rhi::BufferUsage::TransferDst;
            lodDesc.memoryUsage = rhi::MemoryUsage::CPUToGPU;
            m_LODLevels[lod].indexBuffer = device->CreateBuffer(lodDesc);
            if (m_LODLevels[lod].indexBuffer) {
                m_LODLevels[lod].indexBuffer->CopyData(lodIndices.data(), lodDesc.size);
                m_LODLevels[lod].indexCount = static_cast<uint32_t>(lodCount);
                METAGFX_TRACE << "  LOD" << (lod + 1) << ": " << lodCount << " indices ("
                             << static_cast<int>(100 * lodRatios[lod]) << "% target, actual "
                             << static_cast<int>(100.0 * lodCount / indices.size()) << "%)";
            }
        }
    }

    // Create default material if none is set
    if (!m_Material) {
        m_Material = std::make_unique<Material>();
    }

    METAGFX_TRACE << "Mesh initialized: " <<  m_VertexCount << " vertices, " << m_IndexCount << " indices";
    return true;
}

void Mesh::Cleanup() {
    m_VertexBuffer.reset();
    m_IndexBuffer.reset();
    for (auto& lod : m_LODLevels) {
        lod.indexBuffer.reset();
        lod.indexCount = 0;
    }
    m_Material.reset();
    m_Vertices.clear();
    m_Indices.clear();
    m_VertexCount = 0;
    m_IndexCount = 0;
}

Ref<rhi::Buffer> Mesh::GetIndexBuffer(int lod) const {
    if (lod <= 0) return m_IndexBuffer;
    int idx = lod - 1;
    if (idx < static_cast<int>(m_LODLevels.size()) && m_LODLevels[idx].indexBuffer)
        return m_LODLevels[idx].indexBuffer;
    return m_IndexBuffer;  // Fallback to LOD0
}

uint32_t Mesh::GetIndexCount(int lod) const {
    if (lod <= 0) return m_IndexCount;
    int idx = lod - 1;
    if (idx < static_cast<int>(m_LODLevels.size()) && m_LODLevels[idx].indexBuffer)
        return m_LODLevels[idx].indexCount;
    return m_IndexCount;  // Fallback to LOD0
}

int Mesh::GetLODCount() const {
    int count = 1;  // Always at least LOD0
    for (const auto& lod : m_LODLevels) {
        if (lod.indexBuffer) ++count;
        else break;
    }
    return count;
}

void Mesh::SetMaterial(std::unique_ptr<Material> material) {
    m_Material = std::move(material);
}

} // namespace metagfx