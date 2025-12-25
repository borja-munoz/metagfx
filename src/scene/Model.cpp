// ============================================================================
// src/scene/Model.cpp
// ============================================================================
#include "metagfx/scene/Model.h"
#include "metagfx/scene/Material.h"
#include "metagfx/rhi/GraphicsDevice.h"
#include "metagfx/rhi/Types.h"
#include "metagfx/core/Logger.h"
#include "metagfx/utils/TextureUtils.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <glm/gtc/constants.hpp>
#include <filesystem>

namespace metagfx {

Model::Model() = default;

Model::~Model() {
    Cleanup();
}

Model::Model(Model&& other) noexcept
    : m_Meshes(std::move(other.m_Meshes))
    , m_FilePath(std::move(other.m_FilePath))
{
}

Model& Model::operator=(Model&& other) noexcept {
    if (this != &other) {
        Cleanup();
        m_Meshes = std::move(other.m_Meshes);
        m_FilePath = std::move(other.m_FilePath);
    }
    return *this;
}

// Helper function to load texture (handles both embedded and external textures)
static Ref<rhi::Texture> LoadTextureFromAssimp(rhi::GraphicsDevice* device,
                                                const aiScene* scene,
                                                const std::string& texPath,
                                                const std::string& modelDir,
                                                bool useSRGB = false) {
    // Determine format: SRGB for albedo/diffuse, UNORM for data textures (normal, metallic, roughness, AO)
    rhi::Format format = useSRGB ? rhi::Format::R8G8B8A8_SRGB : rhi::Format::R8G8B8A8_UNORM;

    // Check if this is an embedded texture (path starts with '*')
    if (texPath[0] == '*') {
        // Embedded texture: extract index
        int textureIndex = std::atoi(texPath.c_str() + 1);

        if (textureIndex >= 0 && static_cast<unsigned int>(textureIndex) < scene->mNumTextures) {
            const aiTexture* embeddedTex = scene->mTextures[textureIndex];

            // Check if texture is compressed (mHeight == 0) or uncompressed
            if (embeddedTex->mHeight == 0) {
                // Compressed texture (PNG, JPG, etc.)
                utils::ImageData imageData = utils::LoadImageFromMemory(
                    reinterpret_cast<const uint8_t*>(embeddedTex->pcData),
                    embeddedTex->mWidth  // mWidth stores the data size for compressed textures
                );

                return utils::CreateTextureFromImage(device, imageData, format);
            } else {
                // Uncompressed texture (raw RGBA data)
                utils::ImageData imageData{
                    reinterpret_cast<uint8_t*>(embeddedTex->pcData),
                    embeddedTex->mWidth,
                    embeddedTex->mHeight,
                    4  // RGBA
                };

                return utils::CreateTextureFromImage(device, imageData, format);
            }
        } else {
            METAGFX_WARN << "Invalid embedded texture index: " << textureIndex;
            return nullptr;
        }
    } else {
        // External texture file
        std::filesystem::path fullPath = std::filesystem::path(modelDir) / texPath;

        // For external files, we need to create texture with correct format
        utils::ImageData imageData = utils::LoadImage(fullPath.string(), 4);
        if (!imageData.pixels) {
            METAGFX_ERROR << "Failed to load texture from: " << fullPath;
            return nullptr;
        }

        auto texture = utils::CreateTextureFromImage(device, imageData, format);
        utils::FreeImage(imageData);

        return texture;
    }
}

// Helper function to extract material from Assimp
static std::unique_ptr<Material> ProcessMaterial(rhi::GraphicsDevice* device,
                                                  const aiScene* scene,
                                                  const aiMaterial* aiMat,
                                                  const std::string& modelDir) {
    if (!aiMat) {
        return std::make_unique<Material>();  // Return default material
    }

    // Extract diffuse color → albedo
    aiColor3D diffuse(0.8f, 0.8f, 0.8f);
    aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);

    // Extract shininess → roughness (inverse relationship)
    float shininess = 32.0f;
    aiMat->Get(AI_MATKEY_SHININESS, shininess);

    // Convert shininess to roughness (inverse mapping)
    // High shininess (256) = low roughness (0), Low shininess (0) = high roughness (1)
    float roughness = 1.0f - glm::clamp(shininess / 256.0f, 0.0f, 1.0f);

    // Metallic defaults to 0.0 (no metallic data in basic formats like OBJ)
    float metallic = 0.0f;

    // Create material with scalar properties
    auto material = std::make_unique<Material>(
        glm::vec3(diffuse.r, diffuse.g, diffuse.b),
        roughness,
        metallic
    );

    METAGFX_INFO << "Material properties: albedo=(" << diffuse.r << ", " << diffuse.g << ", " << diffuse.b
                 << "), roughness=" << roughness << ", metallic=" << metallic;

    // Extract albedo/diffuse texture
    if (aiMat->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
        aiString texPath;
        if (aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS) {
            try {
                auto texture = LoadTextureFromAssimp(device, scene, texPath.C_Str(), modelDir, true);  // Use SRGB for albedo
                if (texture) {
                    material->SetAlbedoMap(texture);
                    METAGFX_INFO << "Loaded albedo texture: " << texPath.C_Str();
                }
            } catch (const std::exception& e) {
                METAGFX_WARN << "Failed to load albedo texture: " << texPath.C_Str() << " - " << e.what();
            }
        }
    }

    // Extract normal map
    if (aiMat->GetTextureCount(aiTextureType_NORMALS) > 0) {
        aiString texPath;
        if (aiMat->GetTexture(aiTextureType_NORMALS, 0, &texPath) == AI_SUCCESS) {
            try {
                auto texture = LoadTextureFromAssimp(device, scene, texPath.C_Str(), modelDir);
                if (texture) {
                    material->SetNormalMap(texture);
                    METAGFX_INFO << "Loaded normal map: " << texPath.C_Str();
                }
            } catch (const std::exception& e) {
                METAGFX_WARN << "Failed to load normal map: " << texPath.C_Str() << " - " << e.what();
            }
        }
    }

    // Extract metallic-roughness map (glTF workflow)
    // For glTF models, Assimp provides the combined metallic-roughness texture via aiTextureType_METALNESS
    // glTF format: G channel = roughness, B channel = metallic
    bool hasMetallicRoughness = false;
    if (aiMat->GetTextureCount(aiTextureType_METALNESS) > 0) {
        aiString texPath;
        if (aiMat->GetTexture(aiTextureType_METALNESS, 0, &texPath) == AI_SUCCESS) {
            try {
                auto texture = LoadTextureFromAssimp(device, scene, texPath.C_Str(), modelDir);
                if (texture) {
                    // Treat as combined metallic-roughness texture for glTF
                    material->SetMetallicRoughnessMap(texture);
                    hasMetallicRoughness = true;
                    METAGFX_INFO << "Loaded metallic-roughness map (glTF): " << texPath.C_Str();
                }
            } catch (const std::exception& e) {
                METAGFX_WARN << "Failed to load metallic-roughness map: " << texPath.C_Str() << " - " << e.what();
            }
        }
    }

    // Extract separate roughness map (non-glTF workflows only if we don't have combined texture)
    if (!hasMetallicRoughness && aiMat->GetTextureCount(aiTextureType_DIFFUSE_ROUGHNESS) > 0) {
        aiString texPath;
        if (aiMat->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &texPath) == AI_SUCCESS) {
            try {
                auto texture = LoadTextureFromAssimp(device, scene, texPath.C_Str(), modelDir);
                if (texture) {
                    material->SetRoughnessMap(texture);
                    METAGFX_INFO << "Loaded roughness map: " << texPath.C_Str();
                }
            } catch (const std::exception& e) {
                METAGFX_WARN << "Failed to load roughness map: " << texPath.C_Str() << " - " << e.what();
            }
        }
    }

    // Extract ambient occlusion map
    if (aiMat->GetTextureCount(aiTextureType_AMBIENT_OCCLUSION) > 0) {
        aiString texPath;
        if (aiMat->GetTexture(aiTextureType_AMBIENT_OCCLUSION, 0, &texPath) == AI_SUCCESS) {
            try {
                auto texture = LoadTextureFromAssimp(device, scene, texPath.C_Str(), modelDir);
                if (texture) {
                    material->SetAOMap(texture);
                    METAGFX_INFO << "Loaded AO map: " << texPath.C_Str();
                }
            } catch (const std::exception& e) {
                METAGFX_WARN << "Failed to load AO map: " << texPath.C_Str() << " - " << e.what();
            }
        }
    }

    // Extract combined metallic-roughness map (glTF standard)
    // In glTF: R channel is unused/occlusion, G is roughness, B is metallic
    if (aiMat->GetTextureCount(aiTextureType_UNKNOWN) > 0) {
        aiString texPath;
        if (aiMat->GetTexture(aiTextureType_UNKNOWN, 0, &texPath) == AI_SUCCESS) {
            std::string pathStr = texPath.C_Str();
            // Check if this might be a combined metallic-roughness texture
            if (pathStr.find("metallicRoughness") != std::string::npos ||
                pathStr.find("MetallicRoughness") != std::string::npos ||
                pathStr.find("metallic_roughness") != std::string::npos ||
                pathStr[0] == '*') {  // Embedded textures in glTF are often metallic-roughness

                try {
                    auto texture = LoadTextureFromAssimp(device, scene, pathStr, modelDir);
                    if (texture) {
                        material->SetMetallicRoughnessMap(texture);
                        METAGFX_INFO << "Loaded combined metallic-roughness map: " << pathStr;
                    }
                } catch (const std::exception& e) {
                    METAGFX_WARN << "Failed to load metallic-roughness map: " << pathStr << " - " << e.what();
                }
            }
        }
    }

    return material;
}

// Helper function to process an Assimp mesh
static std::unique_ptr<Mesh> ProcessMesh(rhi::GraphicsDevice* device, aiMesh* aiMesh, const aiScene* scene,
                                         const std::string& modelDir) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // Process vertices
    vertices.reserve(aiMesh->mNumVertices);
    for (uint32_t i = 0; i < aiMesh->mNumVertices; ++i) {
        Vertex vertex;

        // Position
        vertex.position = glm::vec3(
            aiMesh->mVertices[i].x,
            aiMesh->mVertices[i].y,
            aiMesh->mVertices[i].z
        );

        // Normal
        if (aiMesh->HasNormals()) {
            vertex.normal = glm::vec3(
                aiMesh->mNormals[i].x,
                aiMesh->mNormals[i].y,
                aiMesh->mNormals[i].z
            );
        } else {
            vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }

        // Texture coordinates (use first UV channel if available)
        if (aiMesh->HasTextureCoords(0)) {
            vertex.texCoord = glm::vec2(
                aiMesh->mTextureCoords[0][i].x,
                aiMesh->mTextureCoords[0][i].y
            );
        } else {
            vertex.texCoord = glm::vec2(0.0f, 0.0f);
        }

        vertices.push_back(vertex);
    }

    // Process indices
    for (uint32_t i = 0; i < aiMesh->mNumFaces; ++i) {
        aiFace face = aiMesh->mFaces[i];
        for (uint32_t j = 0; j < face.mNumIndices; ++j) {
            indices.push_back(face.mIndices[j]);
        }
    }

    // Create and initialize mesh
    auto mesh = std::make_unique<Mesh>();
    if (!mesh->Initialize(device, vertices, indices)) {
        METAGFX_ERROR << "Failed to initialize mesh";
        return nullptr;
    }

    // Extract and attach material
    if (scene && aiMesh->mMaterialIndex >= 0 && aiMesh->mMaterialIndex < scene->mNumMaterials) {
        aiMaterial* aiMat = scene->mMaterials[aiMesh->mMaterialIndex];
        auto material = ProcessMaterial(device, scene, aiMat, modelDir);
        mesh->SetMaterial(std::move(material));
    } else {
        // Fallback to default material
        mesh->SetMaterial(std::make_unique<Material>());
    }

    return mesh;
}

// Helper function to process an Assimp node recursively
static void ProcessNode(rhi::GraphicsDevice* device, aiNode* node, const aiScene* scene,
                       std::vector<std::unique_ptr<Mesh>>& meshes, const std::string& modelDir) {
    // Process all meshes in this node
    for (uint32_t i = 0; i < node->mNumMeshes; ++i) {
        aiMesh* aiMesh = scene->mMeshes[node->mMeshes[i]];
        auto mesh = ProcessMesh(device, aiMesh, scene, modelDir);
        if (mesh) {
            meshes.push_back(std::move(mesh));
        }
    }

    // Process children nodes recursively
    for (uint32_t i = 0; i < node->mNumChildren; ++i) {
        ProcessNode(device, node->mChildren[i], scene, meshes, modelDir);
    }
}

bool Model::LoadFromFile(rhi::GraphicsDevice* device, const std::string& filepath) {
    if (!device) {
        METAGFX_ERROR << "Model::LoadFromFile - Invalid device";
        return false;
    }

    METAGFX_INFO << "Loading model: " << filepath;

    // Create Assimp importer
    Assimp::Importer importer;

    // Load the model with post-processing
    // aiProcess_Triangulate: Convert all primitives to triangles
    // aiProcess_FlipUVs: Flip texture coordinates on Y axis (OpenGL convention)
    // aiProcess_GenNormals: Generate normals if not present
    // aiProcess_CalcTangentSpace: Calculate tangents/bitangents (for normal mapping later)
    // aiProcess_JoinIdenticalVertices: Optimize by merging identical vertices
    const aiScene* scene = importer.ReadFile(filepath,
        aiProcess_Triangulate |
        aiProcess_FlipUVs |
        aiProcess_GenNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_JoinIdenticalVertices
    );

    // Check for errors
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        METAGFX_ERROR << "Assimp error: " << importer.GetErrorString();
        return false;
    }

    // Clear existing meshes
    Cleanup();

    // Extract model directory for texture path resolution
    std::filesystem::path modelPath(filepath);
    std::string modelDir = modelPath.parent_path().string();
    if (modelDir.empty()) {
        modelDir = ".";  // Current directory if no path specified
    }

    // Process the scene
    ProcessNode(device, scene->mRootNode, scene, m_Meshes, modelDir);

    if (m_Meshes.empty()) {
        METAGFX_ERROR << "No meshes loaded from: " << filepath;
        return false;
    }

    m_FilePath = filepath;
    METAGFX_INFO << "Model loaded successfully: " << m_Meshes.size() << " meshes";
    return true;
}

bool Model::CreateCube(rhi::GraphicsDevice* device, float size) {
    if (!device) {
        METAGFX_ERROR << "Model::CreateCube - Invalid device";
        return false;
    }

    Cleanup();

    float s = size * 0.5f;
    
    std::vector<Vertex> vertices = {
        // Front face
        {{-s, -s,  s}, {0, 0, 1}, {0, 0}},
        {{ s, -s,  s}, {0, 0, 1}, {1, 0}},
        {{ s,  s,  s}, {0, 0, 1}, {1, 1}},
        {{-s,  s,  s}, {0, 0, 1}, {0, 1}},
        
        // Back face
        {{ s, -s, -s}, {0, 0, -1}, {0, 0}},
        {{-s, -s, -s}, {0, 0, -1}, {1, 0}},
        {{-s,  s, -s}, {0, 0, -1}, {1, 1}},
        {{ s,  s, -s}, {0, 0, -1}, {0, 1}},
        
        // Left face
        {{-s, -s, -s}, {-1, 0, 0}, {0, 0}},
        {{-s, -s,  s}, {-1, 0, 0}, {1, 0}},
        {{-s,  s,  s}, {-1, 0, 0}, {1, 1}},
        {{-s,  s, -s}, {-1, 0, 0}, {0, 1}},
        
        // Right face
        {{ s, -s,  s}, {1, 0, 0}, {0, 0}},
        {{ s, -s, -s}, {1, 0, 0}, {1, 0}},
        {{ s,  s, -s}, {1, 0, 0}, {1, 1}},
        {{ s,  s,  s}, {1, 0, 0}, {0, 1}},
        
        // Top face
        {{-s,  s,  s}, {0, 1, 0}, {0, 0}},
        {{ s,  s,  s}, {0, 1, 0}, {1, 0}},
        {{ s,  s, -s}, {0, 1, 0}, {1, 1}},
        {{-s,  s, -s}, {0, 1, 0}, {0, 1}},
        
        // Bottom face
        {{-s, -s, -s}, {0, -1, 0}, {0, 0}},
        {{ s, -s, -s}, {0, -1, 0}, {1, 0}},
        {{ s, -s,  s}, {0, -1, 0}, {1, 1}},
        {{-s, -s,  s}, {0, -1, 0}, {0, 1}}
    };

    std::vector<uint32_t> indices = {
        0, 1, 2,  2, 3, 0,    // Front
        4, 5, 6,  6, 7, 4,    // Back
        8, 9, 10, 10, 11, 8,  // Left
        12, 13, 14, 14, 15, 12, // Right
        16, 17, 18, 18, 19, 16, // Top
        20, 21, 22, 22, 23, 20  // Bottom
    };

    auto mesh = std::make_unique<Mesh>();
    if (!mesh->Initialize(device, vertices, indices)) {
        METAGFX_ERROR << "Failed to create cube mesh";
        return false;
    }

    // Set default material for cube (white, slightly shiny)
    mesh->SetMaterial(std::make_unique<Material>(
        glm::vec3(1.0f, 1.0f, 1.0f),  // albedo: white
        0.3f,                          // roughness: smooth
        0.0f                           // metallic: non-metallic
    ));

    m_Meshes.push_back(std::move(mesh));
    m_FilePath = "procedural_cube";
    return true;
}

bool Model::CreateSphere(rhi::GraphicsDevice* device, float radius, uint32_t segments) {
    if (!device) {
        METAGFX_ERROR << "Model::CreateSphere - Invalid device";
        return false;
    }

    Cleanup();

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // Generate sphere vertices
    for (uint32_t y = 0; y <= segments; ++y) {
        float phi = glm::pi<float>() * static_cast<float>(y) / static_cast<float>(segments);
        
        for (uint32_t x = 0; x <= segments; ++x) {
            float theta = 2.0f * glm::pi<float>() * static_cast<float>(x) / static_cast<float>(segments);
            
            Vertex vertex;
            vertex.position.x = radius * std::sin(phi) * std::cos(theta);
            vertex.position.y = radius * std::cos(phi);
            vertex.position.z = radius * std::sin(phi) * std::sin(theta);
            
            vertex.normal = glm::normalize(vertex.position);
            
            vertex.texCoord.x = static_cast<float>(x) / static_cast<float>(segments);
            vertex.texCoord.y = static_cast<float>(y) / static_cast<float>(segments);
            
            vertices.push_back(vertex);
        }
    }

    // Generate sphere indices
    for (uint32_t y = 0; y < segments; ++y) {
        for (uint32_t x = 0; x < segments; ++x) {
            uint32_t current = y * (segments + 1) + x;
            uint32_t next = current + segments + 1;
            
            indices.push_back(current);
            indices.push_back(next);
            indices.push_back(current + 1);
            
            indices.push_back(current + 1);
            indices.push_back(next);
            indices.push_back(next + 1);
        }
    }

    auto mesh = std::make_unique<Mesh>();
    if (!mesh->Initialize(device, vertices, indices)) {
        METAGFX_ERROR << "Failed to create sphere mesh";
        return false;
    }

    // Set default material for sphere (light gray, medium roughness)
    mesh->SetMaterial(std::make_unique<Material>(
        glm::vec3(0.8f, 0.8f, 0.8f),  // albedo: light gray
        0.5f,                          // roughness: medium
        0.0f                           // metallic: non-metallic
    ));

    m_Meshes.push_back(std::move(mesh));
    m_FilePath = "procedural_sphere";
    return true;
}

void Model::Cleanup() {
    m_Meshes.clear();
    m_FilePath.clear();
}

} // namespace metagfx