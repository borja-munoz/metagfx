// ============================================================================
// include/metagfx/scene/pbrt/PbrtLoader.h
// ============================================================================
#pragma once

#include "metagfx/scene/pbrt/PbrtParser.h"
#include "metagfx/scene/Model.h"

#include <string>
#include <memory>

namespace metagfx {

class Scene;
namespace rhi { class GraphicsDevice; }

/**
 * @brief Result returned by PbrtLoader::Load().
 */
struct PbrtLoadResult {
    std::unique_ptr<Model> model;   ///< All PBRT meshes assembled into one Model
    PbrtCameraParams       camera;  ///< Camera params (check camera.defined before using)
};

/**
 * @brief High-level entry point for loading a PBRT v4 scene file.
 *
 * Orchestrates PbrtParser → assembles GPU buffers via Mesh::Initialize() →
 * populates the Scene's light list.
 *
 * Usage:
 * @code
 *   auto result = PbrtLoader::Load(device, "path/to/scene.pbrt", scene);
 *   if (result.model && result.model->IsValid()) { ... }
 *   if (result.camera.defined) { ... }
 * @endcode
 */
class PbrtLoader {
public:
    /**
     * @brief Load a PBRT v4 scene file.
     *
     * @param device   Graphics device used to create GPU buffers.
     * @param filepath Path to the .pbrt file.
     * @param scene    Optional scene to receive the PBRT lights.
     *                 If non-null, existing lights are cleared and PBRT lights
     *                 are added.
     * @return PbrtLoadResult with model (may be nullptr on failure) and camera.
     */
    static PbrtLoadResult Load(rhi::GraphicsDevice* device,
                               const std::string&   filepath,
                               Scene*               scene = nullptr);
};

} // namespace metagfx
