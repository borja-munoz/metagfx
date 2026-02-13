#include "metagfx/scene/Scene.h"
#include "metagfx/rhi/GraphicsDevice.h"
#include "metagfx/rhi/Buffer.h"
#include "metagfx/core/Logger.h"
#include <algorithm>

namespace metagfx {

Scene::Scene() = default;

Scene::~Scene() {
    ClearLights();
}

Light* Scene::AddLight(std::unique_ptr<Light> light) {
    if (m_Lights.size() >= MAX_LIGHTS) {
        METAGFX_WARN << "Cannot add light: maximum of " << MAX_LIGHTS << " lights reached";
        return nullptr;
    }

    Light* ptr = light.get();
    m_Lights.push_back(std::move(light));
    METAGFX_INFO << "Added light, total count: " << m_Lights.size();
    return ptr;
}

void Scene::RemoveLight(Light* light) {
    auto it = std::remove_if(m_Lights.begin(), m_Lights.end(),
        [light](const std::unique_ptr<Light>& l) { return l.get() == light; });

    if (it != m_Lights.end()) {
        m_Lights.erase(it, m_Lights.end());
        METAGFX_INFO << "Removed light, remaining count: " << m_Lights.size();
    }
}

void Scene::ClearLights() {
    m_Lights.clear();
}

void Scene::InitializeLightBuffer(rhi::GraphicsDevice* device, rhi::GraphicsAPI api) {
    m_Device = device;

    // Create light buffer (1040 bytes: count + padding + 16 lights)
    rhi::BufferDesc bufferDesc{};
    bufferDesc.size = sizeof(LightBuffer);

    // Use uniform buffer for all backends - the shader declares lightBuffer as uniform (std140)
    // and the size (1040 bytes) fits within WebGPU's 65536-byte uniform buffer limit
    bufferDesc.usage = rhi::BufferUsage::Uniform;
    METAGFX_INFO << "Light buffer: Using UNIFORM buffer";

    bufferDesc.memoryUsage = rhi::MemoryUsage::CPUToGPU;

    m_LightBuffer = m_Device->CreateBuffer(bufferDesc);
    METAGFX_INFO << "Light buffer created: " << bufferDesc.size << " bytes";
}

void Scene::UpdateLightBuffer() {
    if (!m_LightBuffer) {
        METAGFX_WARN << "Light buffer not initialized";
        return;
    }

    // Prepare buffer data
    LightBuffer bufferData{};
    bufferData.lightCount = static_cast<uint32>(m_Lights.size());

    // Copy light data
    for (size_t i = 0; i < m_Lights.size() && i < MAX_LIGHTS; i++) {
        bufferData.lights[i] = m_Lights[i]->ToGPUData();
    }

    // Upload to GPU
    m_LightBuffer->CopyData(&bufferData, sizeof(bufferData));
}

} // namespace metagfx
