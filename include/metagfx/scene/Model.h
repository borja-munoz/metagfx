// ============================================================================
// include/metagfx/scene/Model.h
// ============================================================================
#pragma once

#include "metagfx/scene/Mesh.h"
#include <string>
#include <vector>
#include <memory>

namespace metagfx {

// Forward declarations
namespace rhi {
    class GraphicsDevice;
}

/**
 * @brief Model class representing a 3D model with one or more meshes
 * 
 * A model can contain multiple meshes (e.g., different parts of a character).
 * It handles loading from various file formats using Assimp.
 */
class Model {
public:
    Model();
    ~Model();

    // Non-copyable but movable
    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;
    Model(Model&&) noexcept;
    Model& operator=(Model&&) noexcept;

    /**
     * @brief Load a model from file
     * @param device Graphics device for creating GPU buffers
     * @param filepath Path to the model file (OBJ, FBX, glTF, etc.)
     * @return true if loaded successfully, false otherwise
     */
    bool LoadFromFile(rhi::GraphicsDevice* device, const std::string& filepath);

    /**
     * @brief Create a simple procedural cube
     * @param device Graphics device for creating GPU buffers
     * @param size Size of the cube (default: 1.0)
     * @return true if created successfully
     */
    bool CreateCube(rhi::GraphicsDevice* device, float size = 1.0f);

    /**
     * @brief Create a simple procedural sphere
     * @param device Graphics device for creating GPU buffers
     * @param radius Radius of the sphere (default: 1.0)
     * @param segments Number of segments (default: 32)
     * @return true if created successfully
     */
    bool CreateSphere(rhi::GraphicsDevice* device, float radius = 1.0f, uint32_t segments = 32);

    /**
     * @brief Clean up all resources
     */
    void Cleanup();

    /**
     * @brief Check if model has been loaded
     */
    bool IsValid() const { return !m_Meshes.empty(); }

    // Getters
    const std::vector<std::unique_ptr<Mesh>>& GetMeshes() const { return m_Meshes; }
    size_t GetMeshCount() const { return m_Meshes.size(); }
    const std::string& GetFilePath() const { return m_FilePath; }

    /**
     * @brief Calculate the bounding box of the entire model
     * @param outMin Output parameter for minimum corner
     * @param outMax Output parameter for maximum corner
     * @return true if bounding box was calculated, false if model is empty
     */
    bool GetBoundingBox(glm::vec3& outMin, glm::vec3& outMax) const;

    /**
     * @brief Get the center point of the model's bounding box
     */
    glm::vec3 GetCenter() const;

    /**
     * @brief Get the size (extent) of the model's bounding box
     */
    glm::vec3 GetSize() const;

    /**
     * @brief Get the radius of the bounding sphere (distance from center to farthest corner)
     */
    float GetBoundingSphereRadius() const;

    /**
     * @brief Add a mesh to the model (for procedural geometry)
     */
    void AddMesh(std::unique_ptr<Mesh> mesh);

private:
    std::vector<std::unique_ptr<Mesh>> m_Meshes;
    std::string m_FilePath;
};

} // namespace metagfx