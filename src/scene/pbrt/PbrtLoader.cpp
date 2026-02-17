// ============================================================================
// src/scene/pbrt/PbrtLoader.cpp
// ============================================================================
#include "metagfx/scene/pbrt/PbrtLoader.h"
#include "metagfx/scene/pbrt/PbrtParser.h"
#include "metagfx/scene/Mesh.h"
#include "metagfx/scene/Scene.h"
#include "metagfx/rhi/GraphicsDevice.h"
#include "metagfx/core/Logger.h"

namespace metagfx {

PbrtLoadResult PbrtLoader::Load(rhi::GraphicsDevice* device,
                                 const std::string&   filepath,
                                 Scene*               scene) {
    PbrtLoadResult out;

    // ── Parse ─────────────────────────────────────────────────────────────────
    PbrtParser parser(device);
    PbrtParseResult parsed = parser.Parse(filepath);

    // ── Lights → Scene ────────────────────────────────────────────────────────
    if (scene && !parsed.lights.empty()) {
        scene->ClearLights();
        for (auto& light : parsed.lights) {
            scene->AddLight(std::move(light));
        }
        METAGFX_INFO << "PbrtLoader: added " << scene->GetLightCount() << " light(s) to scene";
    }

    // ── Camera ────────────────────────────────────────────────────────────────
    out.camera      = parsed.camera;
    out.envMapPath  = parsed.envMapPath;
    out.envMapScale = parsed.envMapScale;

    // ── Assemble Model ───────────────────────────────────────────────────────
    if (parsed.meshes.empty()) {
        METAGFX_WARN << "PbrtLoader: no meshes parsed from " << filepath;
        return out;
    }

    auto model = std::make_unique<Model>();

    for (auto& pbrtMesh : parsed.meshes) {
        if (pbrtMesh.vertices.empty() || pbrtMesh.indices.empty()) continue;

        auto mesh = std::make_unique<Mesh>();
        if (!mesh->Initialize(device, pbrtMesh.vertices, pbrtMesh.indices)) {
            METAGFX_WARN << "PbrtLoader: failed to initialize mesh GPU buffers";
            continue;
        }

        // Copy the material onto the heap and attach
        auto mat = std::make_unique<Material>(pbrtMesh.material);
        mesh->SetMaterial(std::move(mat));

        model->AddMesh(std::move(mesh));
    }

    if (!model->IsValid()) {
        METAGFX_ERROR << "PbrtLoader: model has no valid meshes";
        return out;
    }

    METAGFX_INFO << "PbrtLoader: assembled " << model->GetMeshCount()
                 << " mesh(es) from " << filepath;

    out.model = std::move(model);
    return out;
}

} // namespace metagfx
