// ============================================================================
// include/metagfx/scene/pbrt/PbrtParser.h
// ============================================================================
#pragma once

#include "metagfx/scene/pbrt/PbrtLexer.h"
#include "metagfx/scene/Mesh.h"
#include "metagfx/scene/Material.h"
#include "metagfx/scene/Light.h"
#include "metagfx/core/Types.h"

#include <glm/glm.hpp>
#include <map>
#include <string>
#include <vector>
#include <memory>
#include <variant>

namespace metagfx {

namespace rhi {
    class GraphicsDevice;
    class Texture;
}

// ── Output types ─────────────────────────────────────────────────────────────

/**
 * @brief Camera parameters extracted from LookAt + Camera directives.
 */
struct PbrtCameraParams {
    bool      defined = false;
    glm::vec3 eye  { 0.0f, 0.0f, 5.0f };
    glm::vec3 look { 0.0f, 0.0f, 0.0f };
    glm::vec3 up   { 0.0f, 1.0f, 0.0f };
    float     fov  = 45.0f;
};

/**
 * @brief Intermediate mesh produced by the parser (CTM baked into vertices).
 */
struct PbrtMesh {
    std::vector<Vertex>   vertices;
    std::vector<uint32_t> indices;
    Material              material;
};

/**
 * @brief Full result returned by PbrtParser::Parse().
 */
struct PbrtParseResult {
    std::vector<PbrtMesh>               meshes;
    PbrtCameraParams                    camera;
    std::vector<std::unique_ptr<Light>> lights;
    std::string                         envMapPath;   ///< Path to env map from LightSource "infinite" (empty if none)
    float                               envMapScale = 1.0f;
};

// ── Parser ────────────────────────────────────────────────────────────────────

/**
 * @brief Recursive-descent parser for PBRT v4 scene files.
 *
 * Supported features (Milestone 5.2):
 *  - LookAt / Camera "perspective"
 *  - WorldBegin / WorldEnd
 *  - AttributeBegin/End + brace {} blocks
 *  - Transform / Translate / Rotate / Scale / ConcatTransform / Identity
 *  - Material / MakeNamedMaterial / NamedMaterial
 *  - Texture "name" "spectrum"|"float" "imagemap" (albedo, normal, roughness, metallic)
 *  - Texture "name" "float"|"spectrum" "scale" (pass-through for color; skip for float/bump)
 *  - Shape "trianglemesh" + Shape "plymesh" + Shape "sphere" + Shape "disk"
 *  - LightSource "distant" + LightSource "point" + LightSource "spot" + LightSource "infinite"
 *  - AreaLightSource "diffuse" (emissive proxy point lights)
 *  - ObjectBegin / ObjectEnd / ObjectInstance (geometry instancing)
 *  - Include (recursive file inclusion)
 *
 * Material types (Milestone 5.2):
 *  - diffuse, coateddiffuse, conductor, coatedconductor, dielectric, mirror
 *  - mix (proper lerp of two named materials)
 *  - measured (approximated as white coateddiffuse)
 *
 * Unsupported (silently skipped):
 *  - Film, Sampler, Integrator, Accelerator
 *  - LightSource "infinite" mapname loaded (path stored in PbrtParseResult.envMapPath)
 *  - AreaLightSource "goniometric", "laser"
 */
class PbrtParser {
public:
    explicit PbrtParser(rhi::GraphicsDevice* device);

    /**
     * @brief Parse a PBRT v4 file and return all extracted scene data.
     * @param filepath  Absolute or relative path to the .pbrt scene file.
     */
    PbrtParseResult Parse(const std::string& filepath);

private:
    // ── Parameter-list types ─────────────────────────────────────────────────

    using ParamValue = std::variant<
        float,
        int,
        std::string,
        std::vector<float>,
        std::vector<int>
    >;
    using ParamMap = std::map<std::string, ParamValue>;

    // ── Graphics state (pushed/popped per attribute block) ───────────────────

    struct GraphicsState {
        glm::mat4 ctm { 1.0f };
        Material  material;
        bool      hasMaterial        = false;
        bool      reverseOrientation = false;
        glm::vec3 areaLightEmission { 0.0f };
        bool      hasAreaLight       = false;
    };

    // ── Members ──────────────────────────────────────────────────────────────

    rhi::GraphicsDevice*   m_Device  = nullptr;
    std::string            m_BaseDir;       // directory of the root .pbrt file

    // Named materials/textures are global (not scoped to attribute blocks)
    std::map<std::string, Material>           m_NamedMaterials;
    std::map<std::string, Ref<rhi::Texture>>  m_NamedTextures;  // all GPU textures (color + float)

    // Object instancing state
    std::map<std::string, std::vector<PbrtMesh>> m_NamedObjects;
    bool                   m_InsideObject = false;
    std::string            m_CurrentObjectName;
    std::vector<PbrtMesh>  m_ObjectBuffer;

    GraphicsState              m_State;
    std::vector<GraphicsState> m_StateStack;
    PbrtParseResult            m_Result;

    // ── Parse helpers ─────────────────────────────────────────────────────────

    void ParseGlobalSection(PbrtLexer& lex);
    void ParseWorldSection(PbrtLexer& lex);

    void ApplyTransformDirective(PbrtLexer& lex, const std::string& directive);

    void HandleMaterial(const std::string& typeStr, const ParamMap& params);
    void HandleMakeNamedMaterial(const std::string& name, const ParamMap& params);
    void HandleTexture(PbrtLexer& lex);
    void HandleShape(PbrtLexer& lex, const std::string& typeStr, const ParamMap& params);
    void HandleLightSource(const std::string& typeStr, const ParamMap& params);

    void BuildTriangleMesh(const ParamMap& params);
    void BuildPlyMesh(const ParamMap& params);
    void BuildSphere(const ParamMap& params);
    void BuildDisk(const ParamMap& params);

    /// Routes a completed mesh to m_Result.meshes or m_ObjectBuffer depending on instancing state.
    void EmitMesh(PbrtMesh mesh);

    Material BuildMaterial(const std::string& typeStr, const ParamMap& params);

    /// Derive conductor albedo (F0) from "spectrum eta"/"spectrum k" params.
    /// Falls back to glm::vec3(0.9f) (generic bright metal) if unknown.
    static glm::vec3 ConductorAlbedoFromParams(const ParamMap& params);

    // ── Parameter parsing ────────────────────────────────────────────────────

    ParamMap ParseParams(PbrtLexer& lex);
    float    ReadFloat(PbrtLexer& lex);

    // ── Param accessors ───────────────────────────────────────────────────────

    static float       GetFloat (const ParamMap& p, const std::string& name, float def = 0.0f);
    static glm::vec3   GetVec3  (const ParamMap& p, const std::string& name,
                                  glm::vec3 def = glm::vec3(0.0f));
    static std::string GetString(const ParamMap& p, const std::string& name,
                                  std::string def = "");

    // ── Transform helpers ─────────────────────────────────────────────────────

    static glm::mat4 PbrtTransformToMat4(const std::vector<float>& f);
};

} // namespace metagfx
