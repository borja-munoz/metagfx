# Model Loading System Design

## Overview

The model loading system in MetaGFX provides a unified interface for loading and rendering 3D geometry from various file formats. It consists of two main components: **Mesh** and **Model**, which work together to manage geometry data and GPU resources.

## Architecture

```
┌─────────────────────────────────────────┐
│         Application Layer               │
│  - Creates models                       │
│  - Renders meshes via command buffers   │
├─────────────────────────────────────────┤
│            Model                        │
│  - Container for meshes                 │
│  - Assimp integration                   │
│  - Procedural geometry generation       │
├─────────────────────────────────────────┤
│            Mesh                         │
│  - Geometry data (vertices, indices)    │
│  - GPU buffer management                │
├─────────────────────────────────────────┤
│         RHI (Graphics Device)           │
│  - Buffer creation                      │
│  - Command buffer operations            │
└─────────────────────────────────────────┘
```

## Vertex Structure

All meshes use a unified vertex format that supports basic PBR rendering:

```cpp
struct Vertex {
    glm::vec3 position;   // Vertex position in model space
    glm::vec3 normal;     // Vertex normal for lighting calculations
    glm::vec2 texCoord;   // Texture coordinates for mapping
};
```

**Size**: 32 bytes per vertex (3 floats + 3 floats + 2 floats)

**Shader Layout** (`model.vert`):
```glsl
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
```

## Mesh Class

**Location**: `include/metagfx/scene/Mesh.h`, `src/scene/Mesh.cpp`

### Responsibilities

1. **Geometry Storage**: Stores vertex and index data in CPU memory
2. **GPU Buffer Management**: Creates and manages vertex and index buffers
3. **Resource Lifetime**: Ensures proper cleanup of GPU resources

### Key Methods

```cpp
class Mesh {
public:
    // Initialize mesh with geometry data and create GPU buffers
    bool Initialize(rhi::GraphicsDevice* device,
                   const std::vector<Vertex>& vertices,
                   const std::vector<uint32_t>& indices);

    // Clean up GPU resources
    void Cleanup();

    // Check if mesh has valid GPU buffers
    bool IsValid() const;

    // Getters for rendering
    Ref<rhi::Buffer> GetVertexBuffer() const;
    Ref<rhi::Buffer> GetIndexBuffer() const;
    uint32_t GetVertexCount() const;
    uint32_t GetIndexCount() const;
};
```

### Buffer Creation Strategy

**Memory Usage**: `CPUToGPU`
- Allows CPU writes for initial data upload
- GPU reads for rendering
- Suitable for static geometry that doesn't change

**Buffer Usage Flags**:
- Vertex Buffer: `BufferUsage::Vertex | BufferUsage::TransferDst`
- Index Buffer: `BufferUsage::Index | BufferUsage::TransferDst`

**Creation Process**:
1. Create buffer with `CreateBuffer(desc)`
2. Copy data with `buffer->CopyData(data, size)`
3. Store shared pointer for automatic cleanup

### Move Semantics

The Mesh class is **movable but not copyable**:
- Prevents accidental duplication of large geometry data
- Enables efficient transfer of ownership
- Move constructor/assignment properly transfers GPU buffer ownership

## Model Class

**Location**: `include/metagfx/scene/Model.h`, `src/scene/Model.cpp`

### Responsibilities

1. **Multi-Mesh Container**: Manages multiple meshes that form a complete model
2. **File Loading**: Integrates with Assimp to load various 3D formats
3. **Procedural Generation**: Creates simple geometric primitives
4. **Scene Graph Processing**: Recursively processes Assimp node hierarchies

### Key Methods

```cpp
class Model {
public:
    // Load model from file using Assimp
    bool LoadFromFile(rhi::GraphicsDevice* device, const std::string& filepath);

    // Create procedural geometry
    bool CreateCube(rhi::GraphicsDevice* device, float size = 1.0f);
    bool CreateSphere(rhi::GraphicsDevice* device, float radius = 1.0f, uint32_t segments = 32);

    // Resource management
    void Cleanup();
    bool IsValid() const;

    // Access meshes for rendering
    const std::vector<std::unique_ptr<Mesh>>& GetMeshes() const;
    size_t GetMeshCount() const;
};
```

## Assimp Integration

### Supported Formats

Currently enabled importers (configured in `external/CMakeLists.txt`):
- **OBJ**: Wavefront Object files
- **FBX**: Autodesk Filmbox
- **glTF**: GL Transmission Format (JSON-based)
- **COLLADA**: Collaborative Design Activity

Additional formats can be enabled by setting Assimp CMake options.

### Post-Processing Flags

The loader applies these Assimp post-processing steps:

```cpp
aiProcess_Triangulate              // Convert all primitives to triangles
aiProcess_FlipUVs                  // Flip texture coordinates on Y axis (OpenGL convention)
aiProcess_GenNormals               // Generate normals if not present
aiProcess_CalcTangentSpace         // Calculate tangents/bitangents (for normal mapping)
aiProcess_JoinIdenticalVertices    // Optimize by merging identical vertices
```

### Scene Graph Processing

The model loader recursively processes the Assimp scene graph:

```cpp
static void ProcessNode(rhi::GraphicsDevice* device, aiNode* node, const aiScene* scene,
                       std::vector<std::unique_ptr<Mesh>>& meshes) {
    // Process all meshes in this node
    for (uint32_t i = 0; i < node->mNumMeshes; ++i) {
        aiMesh* aiMesh = scene->mMeshes[node->mMeshes[i]];
        auto mesh = ProcessMesh(device, aiMesh);
        if (mesh) {
            meshes.push_back(std::move(mesh));
        }
    }

    // Recursively process children
    for (uint32_t i = 0; i < node->mNumChildren; ++i) {
        ProcessNode(device, node->mChildren[i], scene, meshes);
    }
}
```

### Mesh Processing

For each Assimp mesh:

1. **Extract Vertices**:
   - Read position from `aiMesh->mVertices`
   - Read normal from `aiMesh->mNormals` (or default to (0,1,0))
   - Read UV from `aiMesh->mTextureCoords[0]` (or default to (0,0))

2. **Extract Indices**:
   - Iterate through faces (`aiMesh->mFaces`)
   - Extract all indices from each face
   - Assumes triangulated geometry (via `aiProcess_Triangulate`)

3. **Create Mesh**:
   - Initialize new Mesh object
   - Create GPU buffers
   - Add to model's mesh list

## Procedural Geometry

### Cube Generation

Creates a unit cube with proper normals and UVs:

**Vertices**: 24 (4 per face for proper normals)
**Indices**: 36 (6 faces × 2 triangles × 3 vertices)

Each face has:
- Outward-facing normal
- Texture coordinates (0,0) to (1,1)
- Counter-clockwise winding order

### Sphere Generation

Creates a UV sphere using parametric equations:

**Parameters**:
- `radius`: Size of the sphere
- `segments`: Longitude/latitude divisions (default: 32)

**Vertices**: `(segments + 1) × (segments + 1)`
**Indices**: `segments × segments × 6`

**Generation**:
```cpp
for (uint32_t y = 0; y <= segments; ++y) {
    float phi = π * y / segments;
    for (uint32_t x = 0; x <= segments; ++x) {
        float theta = 2π * x / segments;

        position.x = radius * sin(phi) * cos(theta);
        position.y = radius * cos(phi);
        position.z = radius * sin(phi) * sin(theta);

        normal = normalize(position);
        texCoord = (x/segments, y/segments);
    }
}
```

## Rendering Pipeline

### Shader Integration

The model rendering pipeline uses dedicated shaders:

**Vertex Shader** (`model.vert`):
- Transforms vertices using MVP matrices
- Computes world-space position
- Calculates normal matrix for non-uniform scaling
- Passes data to fragment shader

**Fragment Shader** (`model.frag`):
- Implements basic Phong lighting
- Directional light from (1, 1, 1)
- Ambient + diffuse lighting
- Gray material color (will be replaced with material system in Phase 2.2)

### Application Integration

**Pipeline Creation**:
```cpp
void Application::CreateModelPipeline() {
    // Load model shaders
    auto vertShader = m_Device->CreateShader(vertShaderDesc);
    auto fragShader = m_Device->CreateShader(fragShaderDesc);

    // Configure vertex input with Mesh vertex layout
    pipelineDesc.vertexInput.stride = sizeof(Vertex);
    pipelineDesc.vertexInput.attributes = {
        { 0, Format::R32G32B32_SFLOAT, 0 },                   // position
        { 1, Format::R32G32B32_SFLOAT, sizeof(float) * 3 },   // normal
        { 2, Format::R32G32_SFLOAT, sizeof(float) * 6 }       // texCoord
    };

    // Create pipeline
    m_ModelPipeline = m_Device->CreateGraphicsPipeline(pipelineDesc);
}
```

**Rendering**:
```cpp
void Application::Render() {
    // Bind model pipeline
    cmd->BindPipeline(m_ModelPipeline);
    cmd->BindDescriptorSet(vkPipeline->GetLayout(), descriptorSet);

    // Draw all meshes in the model
    for (const auto& mesh : m_Model->GetMeshes()) {
        if (mesh && mesh->IsValid()) {
            cmd->BindVertexBuffer(mesh->GetVertexBuffer());
            cmd->BindIndexBuffer(mesh->GetIndexBuffer());
            cmd->DrawIndexed(mesh->GetIndexCount());
        }
    }
}
```

## Error Handling

### Loading Errors

The system handles various error conditions:

1. **Invalid File Path**: Returns false, logs error
2. **Assimp Import Failure**: Logs Assimp error string, returns false
3. **Empty Scene**: Returns false if no meshes loaded
4. **Buffer Creation Failure**: Cleans up partial resources, returns false

### Fallback Strategy

The application implements a fallback pattern:

```cpp
if (!m_Model->LoadFromFile(device, "assets/models/bunny.obj")) {
    METAGFX_WARN << "Failed to load bunny.obj, falling back to procedural cube";
    if (!m_Model->CreateCube(device, 1.0f)) {
        METAGFX_ERROR << "Failed to create fallback cube model";
        return;  // Fatal error
    }
}
```

## Memory Management

### Ownership Model

- **Model** owns **Mesh** objects via `std::unique_ptr<Mesh>`
- **Mesh** owns GPU buffers via `Ref<rhi::Buffer>` (shared_ptr)
- Automatic cleanup when Model is destroyed
- Explicit `Cleanup()` methods for early resource release

### Resource Lifetime

1. **Creation**: Application creates Model
2. **Loading**: Model creates Meshes, Meshes create GPU buffers
3. **Rendering**: Application holds references to buffers during draw calls
4. **Cleanup**: Application destroys Model → Meshes destroyed → Buffers released

### Move Semantics

Both Mesh and Model support move operations:
- Enables efficient return from functions
- Allows storage in containers
- Prevents accidental copies

## Future Enhancements

### Phase 2.2: Material System
- Material properties per mesh
- Texture loading and binding
- PBR material parameters (roughness, metallic)

### Phase 2.3: Textures
- Albedo, normal, roughness maps
- Texture atlas support
- Sampler configuration

### Phase 3: Lighting
- Support for material-specific lighting
- Multiple light types
- Shadow mapping integration

### Phase 5: PBRT Scene Loading
- Extend Model to support PBRT scene format
- Material property extraction
- Light data extraction

## Build Configuration

### CMake Options

Assimp is configured in `external/CMakeLists.txt`:

```cmake
# Disable unnecessary features
set(ASSIMP_BUILD_TESTS OFF)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF)
set(ASSIMP_WARNINGS_AS_ERRORS OFF)

# Enable only needed importers
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF)
set(ASSIMP_BUILD_OBJ_IMPORTER ON)
set(ASSIMP_BUILD_FBX_IMPORTER ON)
set(ASSIMP_BUILD_GLTF_IMPORTER ON)
set(ASSIMP_BUILD_COLLADA_IMPORTER ON)
```

### Scene Module Linking

The scene module (`src/scene/CMakeLists.txt`) links against:
- `metagfx_core` - Base types and logging
- `metagfx_rhi` - Graphics device and buffer interfaces
- `glm` - Math operations
- `assimp` - Model loading

## Usage Examples

### Loading a Model

```cpp
auto model = std::make_unique<Model>();
if (model->LoadFromFile(device, "assets/models/bunny.obj")) {
    // Model loaded successfully
    for (const auto& mesh : model->GetMeshes()) {
        // Render each mesh
    }
}
```

### Creating Procedural Geometry

```cpp
auto cubeModel = std::make_unique<Model>();
cubeModel->CreateCube(device, 2.0f);  // 2-unit cube

auto sphereModel = std::make_unique<Model>();
sphereModel->CreateSphere(device, 1.5f, 64);  // High-res sphere
```

### Integrating with Renderer

```cpp
// In application initialization
m_Model = std::make_unique<Model>();
m_Model->LoadFromFile(m_Device.get(), modelPath);

// In render loop
for (const auto& mesh : m_Model->GetMeshes()) {
    commandBuffer->BindVertexBuffer(mesh->GetVertexBuffer());
    commandBuffer->BindIndexBuffer(mesh->GetIndexBuffer());
    commandBuffer->DrawIndexed(mesh->GetIndexCount());
}
```

## Debugging Tips

### Model Loading Issues

1. **Check file path**: Ensure path is relative to working directory (`build/bin/`)
2. **Verify format support**: Check if importer is enabled in CMake
3. **Examine Assimp errors**: Error messages indicate file format issues
4. **Test with simple models**: Start with basic OBJ files

### Rendering Issues

1. **Verify mesh validity**: Call `mesh->IsValid()` before rendering
2. **Check vertex count**: Ensure meshes have geometry
3. **Inspect normals**: Verify lighting calculations
4. **Test with procedural geometry**: Rule out file loading issues

### Performance Profiling

1. **Mesh count**: Monitor `model->GetMeshCount()`
2. **Vertex/Index count**: Track total geometry complexity
3. **Buffer creation**: Profile GPU buffer allocation
4. **Loading time**: Measure Assimp import duration
