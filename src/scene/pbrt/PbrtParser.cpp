// ============================================================================
// src/scene/pbrt/PbrtParser.cpp
// ============================================================================
#include "metagfx/scene/pbrt/PbrtParser.h"
#include "metagfx/rhi/GraphicsDevice.h"
#include "metagfx/utils/TextureUtils.h"
#include "metagfx/core/Logger.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

#include <filesystem>
#include <cmath>
#include <stdexcept>
#include <cassert>

namespace metagfx {

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

PbrtParser::PbrtParser(rhi::GraphicsDevice* device)
    : m_Device(device) {}

// ─────────────────────────────────────────────────────────────────────────────
// Public entry point
// ─────────────────────────────────────────────────────────────────────────────

PbrtParseResult PbrtParser::Parse(const std::string& filepath) {
    m_Result       = PbrtParseResult{};
    m_State        = GraphicsState{};
    m_StateStack.clear();
    m_NamedMaterials.clear();
    m_NamedTextures.clear();
    m_NamedObjects.clear();
    m_InsideObject = false;
    m_CurrentObjectName.clear();
    m_ObjectBuffer.clear();

    // Determine base directory for resolving relative paths
    std::filesystem::path p(filepath);
    m_BaseDir = p.parent_path().string();
    if (m_BaseDir.empty()) m_BaseDir = ".";

    METAGFX_INFO << "PbrtParser: parsing " << filepath;

    try {
        PbrtLexer lex(filepath);
        ParseGlobalSection(lex);
    } catch (const std::exception& e) {
        METAGFX_ERROR << "PbrtParser: " << e.what();
    }

    // Count textured meshes for diagnostics
    size_t texturedCount = 0;
    for (const auto& m : m_Result.meshes) {
        if (m.material.HasAlbedoMap()) texturedCount++;
    }
    METAGFX_INFO << "PbrtParser: parsed " << m_Result.meshes.size() << " meshes ("
                 << texturedCount << " with albedo textures), "
                 << m_Result.lights.size() << " lights, "
                 << m_NamedTextures.size() << " named textures, "
                 << m_NamedMaterials.size() << " named materials";
    return std::move(m_Result);
}

// ─────────────────────────────────────────────────────────────────────────────
// Blackbody temperature → approximate RGB colour
// ─────────────────────────────────────────────────────────────────────────────

// Attempt to convert a colour temperature (Kelvin) to approximate sRGB.
// Uses Tanner Helland's algorithm (valid ~1000 K – 40 000 K).
static glm::vec3 BlackbodyToRGB(float kelvin) {
    float temp = glm::clamp(kelvin, 1000.0f, 40000.0f) / 100.0f;

    float r, g, b;

    // Red
    if (temp <= 66.0f) {
        r = 255.0f;
    } else {
        r = 329.698727446f * std::pow(temp - 60.0f, -0.1332047592f);
    }

    // Green
    if (temp <= 66.0f) {
        g = 99.4708025861f * std::log(temp) - 161.1195681661f;
    } else {
        g = 288.1221695283f * std::pow(temp - 60.0f, -0.0755148492f);
    }

    // Blue
    if (temp >= 66.0f) {
        b = 255.0f;
    } else if (temp <= 19.0f) {
        b = 0.0f;
    } else {
        b = 138.5177312231f * std::log(temp - 10.0f) - 305.0447927307f;
    }

    return glm::vec3(
        glm::clamp(r / 255.0f, 0.0f, 1.0f),
        glm::clamp(g / 255.0f, 0.0f, 1.0f),
        glm::clamp(b / 255.0f, 0.0f, 1.0f));
}

// ─────────────────────────────────────────────────────────────────────────────
// Float / transform helpers
// ─────────────────────────────────────────────────────────────────────────────

float PbrtParser::ReadFloat(PbrtLexer& lex) {
    PbrtToken t = lex.Next();
    if (t.type != PbrtTokenType::Number) {
        METAGFX_WARN << "PbrtParser: expected number at " << lex.CurrentLocation()
                     << ", got '" << t.value << "'";
        return 0.0f;
    }
    return std::stof(t.value);
}

glm::mat4 PbrtParser::PbrtTransformToMat4(const std::vector<float>& f) {
    // PBRT v4 stores Transform values in column-major order (matching GLM layout).
    // File order: col0(4 floats), col1(4 floats), col2(4 floats), col3(4 floats)
    glm::mat4 M;
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row)
            M[col][row] = f[static_cast<size_t>(col * 4 + row)];
    return M;
}

// ─────────────────────────────────────────────────────────────────────────────
// Param-list parsing
// ─────────────────────────────────────────────────────────────────────────────

PbrtParser::ParamMap PbrtParser::ParseParams(PbrtLexer& lex) {
    ParamMap result;

    while (lex.Peek().type == PbrtTokenType::String) {
        std::string decl = lex.Next().value;  // e.g. "float fov" or "point3 P"

        // Split "type name" into type and name
        auto spacePos = decl.find(' ');
        if (spacePos == std::string::npos) {
            METAGFX_WARN << "PbrtParser: malformed param declaration: \"" << decl << "\"";
            continue;
        }
        std::string ptype = decl.substr(0, spacePos);
        std::string pname = decl.substr(spacePos + 1);

        // Check for optional brackets
        bool inBrackets = false;
        if (lex.Peek().type == PbrtTokenType::LBracket) {
            lex.Next();  // consume [
            inBrackets = true;
        }

        if (ptype == "integer") {
            std::vector<int> ints;
            if (inBrackets) {
                while (lex.Peek().type == PbrtTokenType::Number) {
                    ints.push_back(std::stoi(lex.Next().value));
                }
                if (lex.Peek().type == PbrtTokenType::RBracket) lex.Next();
            } else {
                if (lex.Peek().type == PbrtTokenType::Number) {
                    ints.push_back(std::stoi(lex.Next().value));
                }
            }
            if (ints.size() == 1)     result[pname] = ints[0];
            else if (!ints.empty())   result[pname] = ints;

        } else if (ptype == "string" || ptype == "texture" || ptype == "material") {
            // Value is a quoted string (possibly inside brackets)
            std::string val;
            if (lex.Peek().type == PbrtTokenType::String) {
                val = lex.Next().value;
            } else if (lex.Peek().type == PbrtTokenType::Ident) {
                val = lex.Next().value;
            }
            if (inBrackets && lex.Peek().type == PbrtTokenType::RBracket) lex.Next();
            result[pname] = val;

        } else if (ptype == "bool") {
            std::string val;
            PbrtToken next = lex.Peek();
            if (next.type == PbrtTokenType::String || next.type == PbrtTokenType::Ident) {
                val = lex.Next().value;
            } else if (next.type == PbrtTokenType::Number) {
                val = lex.Next().value;
            }
            if (inBrackets && lex.Peek().type == PbrtTokenType::RBracket) lex.Next();
            result[pname] = (val == "true" || val == "1") ? 1 : 0;

        } else {
            // float, rgb, point2, point3, normal3, vector3, spectrum, blackbody, etc.
            // Spectrum/blackbody may contain a string (named spectrum) or numbers.
            if (inBrackets && lex.Peek().type == PbrtTokenType::String) {
                // Named spectrum: e.g. "spectrum eta" [ "metal-Ag-eta" ]
                std::string val = lex.Next().value;
                result[pname] = val;
                if (lex.Peek().type == PbrtTokenType::RBracket) lex.Next();
            } else {
                std::vector<float> floats;
                if (inBrackets) {
                    while (lex.Peek().type == PbrtTokenType::Number) {
                        floats.push_back(std::stof(lex.Next().value));
                    }
                    if (lex.Peek().type == PbrtTokenType::RBracket) lex.Next();
                } else {
                    if (lex.Peek().type == PbrtTokenType::Number) {
                        floats.push_back(std::stof(lex.Next().value));
                    }
                }
                if (floats.size() == 1)   result[pname] = floats[0];
                else if (!floats.empty()) result[pname] = floats;
            }
        }
    }

    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Param accessors
// ─────────────────────────────────────────────────────────────────────────────

float PbrtParser::GetFloat(const ParamMap& p, const std::string& name, float def) {
    auto it = p.find(name);
    if (it == p.end()) return def;
    if (auto* f = std::get_if<float>(&it->second)) return *f;
    if (auto* v = std::get_if<std::vector<float>>(&it->second))
        return v->empty() ? def : (*v)[0];
    if (auto* i = std::get_if<int>(&it->second)) return static_cast<float>(*i);
    return def;
}

glm::vec3 PbrtParser::GetVec3(const ParamMap& p, const std::string& name, glm::vec3 def) {
    auto it = p.find(name);
    if (it == p.end()) return def;
    if (auto* v = std::get_if<std::vector<float>>(&it->second)) {
        if (v->size() >= 3) return glm::vec3((*v)[0], (*v)[1], (*v)[2]);
        if (v->size() == 1) return glm::vec3((*v)[0]);
    }
    if (auto* f = std::get_if<float>(&it->second)) return glm::vec3(*f);
    return def;
}

std::string PbrtParser::GetString(const ParamMap& p, const std::string& name,
                                   std::string def) {
    auto it = p.find(name);
    if (it == p.end()) return def;
    if (auto* s = std::get_if<std::string>(&it->second)) return *s;
    return def;
}

// ─────────────────────────────────────────────────────────────────────────────
// Transform directives (shared by global + world sections)
// ─────────────────────────────────────────────────────────────────────────────

void PbrtParser::ApplyTransformDirective(PbrtLexer& lex, const std::string& directive) {
    if (directive == "Identity") {
        m_State.ctm = glm::mat4(1.0f);

    } else if (directive == "Translate") {
        float x = ReadFloat(lex), y = ReadFloat(lex), z = ReadFloat(lex);
        m_State.ctm = glm::translate(m_State.ctm, glm::vec3(x, y, z));

    } else if (directive == "Scale") {
        float x = ReadFloat(lex), y = ReadFloat(lex), z = ReadFloat(lex);
        m_State.ctm = glm::scale(m_State.ctm, glm::vec3(x, y, z));

    } else if (directive == "Rotate") {
        float angle = ReadFloat(lex);
        float ax = ReadFloat(lex), ay = ReadFloat(lex), az = ReadFloat(lex);
        m_State.ctm = glm::rotate(m_State.ctm,
                                   glm::radians(angle),
                                   glm::vec3(ax, ay, az));

    } else if (directive == "Transform") {
        // Read 16 floats inside optional brackets
        bool br = false;
        if (lex.Peek().type == PbrtTokenType::LBracket) { lex.Next(); br = true; }
        std::vector<float> f;
        f.reserve(16);
        while (f.size() < 16 && lex.Peek().type == PbrtTokenType::Number) {
            f.push_back(std::stof(lex.Next().value));
        }
        if (br && lex.Peek().type == PbrtTokenType::RBracket) lex.Next();
        if (f.size() == 16) m_State.ctm = PbrtTransformToMat4(f);

    } else if (directive == "ConcatTransform") {
        bool br = false;
        if (lex.Peek().type == PbrtTokenType::LBracket) { lex.Next(); br = true; }
        std::vector<float> f;
        f.reserve(16);
        while (f.size() < 16 && lex.Peek().type == PbrtTokenType::Number) {
            f.push_back(std::stof(lex.Next().value));
        }
        if (br && lex.Peek().type == PbrtTokenType::RBracket) lex.Next();
        if (f.size() == 16) m_State.ctm = m_State.ctm * PbrtTransformToMat4(f);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Global section  (before WorldBegin)
// ─────────────────────────────────────────────────────────────────────────────

void PbrtParser::ParseGlobalSection(PbrtLexer& lex) {
    while (true) {
        PbrtToken tok = lex.Next();
        if (tok.type == PbrtTokenType::Eof) return;
        if (tok.type != PbrtTokenType::Ident) continue;

        const std::string& d = tok.value;

        if (d == "LookAt") {
            // 9 raw floats: eye(3) look(3) up(3)
            float ex = ReadFloat(lex), ey = ReadFloat(lex), ez = ReadFloat(lex);
            float lx = ReadFloat(lex), ly = ReadFloat(lex), lz = ReadFloat(lex);
            float ux = ReadFloat(lex), uy = ReadFloat(lex), uz = ReadFloat(lex);

            // If there are pre-LookAt transforms (e.g. Scale -1 1 1 to flip
            // handedness), apply their inverse to the camera parameters so
            // the world-space positions are correct.
            if (m_State.ctm != glm::mat4(1.0f)) {
                glm::mat4 inv = glm::inverse(m_State.ctm);
                glm::vec3 eye  = glm::vec3(inv * glm::vec4(ex, ey, ez, 1.0f));
                glm::vec3 look = glm::vec3(inv * glm::vec4(lx, ly, lz, 1.0f));
                glm::vec3 up   = glm::vec3(inv * glm::vec4(ux, uy, uz, 0.0f));
                m_Result.camera.eye  = eye;
                m_Result.camera.look = look;
                m_Result.camera.up   = glm::normalize(up);
            } else {
                m_Result.camera.eye  = { ex, ey, ez };
                m_Result.camera.look = { lx, ly, lz };
                m_Result.camera.up   = { ux, uy, uz };
            }
            m_Result.camera.defined = true;

        } else if (d == "Camera") {
            std::string type = lex.Next().value;  // e.g. "perspective"
            ParamMap params = ParseParams(lex);
            if (type == "perspective") {
                m_Result.camera.fov = GetFloat(params, "fov", 45.0f);
            }

            // If no LookAt was used, derive camera from the CTM.
            // The CTM at Camera time is the world-to-camera transform.
            // PBRT camera looks along +Z in camera space.
            if (!m_Result.camera.defined) {
                glm::mat4 camToWorld = glm::inverse(m_State.ctm);
                glm::vec3 eye     = glm::vec3(camToWorld[3]);          // column 3 = position
                glm::vec3 forward = glm::vec3(camToWorld[2]);          // column 2 = +Z axis
                glm::vec3 up      = glm::vec3(camToWorld[1]);          // column 1 = +Y axis
                m_Result.camera.eye  = eye;
                m_Result.camera.look = eye + forward;
                m_Result.camera.up   = up;
                m_Result.camera.defined = true;
            }

        } else if (d == "Film" || d == "Sampler" || d == "Integrator" ||
                   d == "Accelerator" || d == "MediumInterface" ||
                   d == "PixelFilter") {
            // Read and discard type + params
            if (lex.Peek().type == PbrtTokenType::String) lex.Next();
            ParseParams(lex);

        } else if (d == "Transform"  || d == "Translate" || d == "Rotate" ||
                   d == "Scale"      || d == "ConcatTransform" || d == "Identity") {
            ApplyTransformDirective(lex, d);

        } else if (d == "WorldBegin") {
            // Per PBRT spec: CTM is reset to identity at WorldBegin
            m_State.ctm = glm::mat4(1.0f);
            ParseWorldSection(lex);
            return;  // WorldEnd exits

        } else if (d == "Include") {
            std::string incFile = lex.Next().value;
            std::string incPath = (std::filesystem::path(m_BaseDir) / incFile).string();
            lex.PushFile(incPath);
        }
        // Everything else in the global section is silently ignored
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// World section  (WorldBegin … WorldEnd)
// ─────────────────────────────────────────────────────────────────────────────

void PbrtParser::ParseWorldSection(PbrtLexer& lex) {
    while (true) {
        PbrtToken tok = lex.Peek();

        if (tok.type == PbrtTokenType::Eof) return;

        // AttributeEnd / RBrace  — pop state
        if (tok.type == PbrtTokenType::RBrace) {
            lex.Next();
            if (!m_StateStack.empty()) {
                m_State = m_StateStack.back();
                m_StateStack.pop_back();
            }
            continue;
        }

        lex.Next();  // consume token
        if (tok.type != PbrtTokenType::Ident) continue;

        const std::string& d = tok.value;

        // ── Attribute blocks ─────────────────────────────────────────────────
        if (d == "AttributeBegin" || d == "LBrace" || d == "TransformBegin") {
            m_StateStack.push_back(m_State);

        } else if (d == "AttributeEnd" || d == "TransformEnd") {
            if (!m_StateStack.empty()) {
                m_State = m_StateStack.back();
                m_StateStack.pop_back();
            }

        } else if (tok.type == PbrtTokenType::LBrace) {
            // Handled above via RBrace check — shouldn't reach here
            m_StateStack.push_back(m_State);

        // ── Transform directives ─────────────────────────────────────────────
        } else if (d == "Transform"  || d == "Translate" || d == "Rotate" ||
                   d == "Scale"      || d == "ConcatTransform" || d == "Identity") {
            ApplyTransformDirective(lex, d);

        // ── Material directives ──────────────────────────────────────────────
        } else if (d == "Material") {
            std::string typeStr;
            if (lex.Peek().type == PbrtTokenType::String) typeStr = lex.Next().value;
            ParamMap params = ParseParams(lex);
            HandleMaterial(typeStr, params);

        } else if (d == "MakeNamedMaterial") {
            std::string name;
            if (lex.Peek().type == PbrtTokenType::String) name = lex.Next().value;
            ParamMap params = ParseParams(lex);
            HandleMakeNamedMaterial(name, params);

        } else if (d == "NamedMaterial") {
            std::string name;
            if (lex.Peek().type == PbrtTokenType::String) name = lex.Next().value;
            auto it = m_NamedMaterials.find(name);
            if (it != m_NamedMaterials.end()) {
                m_State.material    = it->second;
                m_State.hasMaterial = true;
                if (it->second.HasAlbedoMap()) {
                    METAGFX_DEBUG << "PbrtParser: NamedMaterial '" << name
                                  << "' has albedo map (flags=" << it->second.GetTextureFlags() << ")";
                }
            } else {
                METAGFX_WARN << "PbrtParser: unknown named material: " << name;
            }

        // ── Texture ──────────────────────────────────────────────────────────
        } else if (d == "Texture") {
            HandleTexture(lex);

        // ── Shape ────────────────────────────────────────────────────────────
        } else if (d == "Shape") {
            std::string typeStr;
            if (lex.Peek().type == PbrtTokenType::String) typeStr = lex.Next().value;
            ParamMap params = ParseParams(lex);
            HandleShape(lex, typeStr, params);

        // ── Lights ───────────────────────────────────────────────────────────
        } else if (d == "LightSource") {
            std::string typeStr;
            if (lex.Peek().type == PbrtTokenType::String) typeStr = lex.Next().value;
            ParamMap params = ParseParams(lex);
            HandleLightSource(typeStr, params);

        } else if (d == "AreaLightSource") {
            std::string typeStr;
            if (lex.Peek().type == PbrtTokenType::String) typeStr = lex.Next().value;
            ParamMap params = ParseParams(lex);
            // Store emission in graphics state; shapes in this block become emissive
            float scale = GetFloat(params, "scale", 1.0f);

            // Detect blackbody: a single float L > 100 is a colour temperature (Kelvin),
            // not an RGB colour.  Convert to approximate sRGB and use scale for intensity.
            glm::vec3 color(1.0f);
            float lightIntensity = scale;

            auto litIt = params.find("L");
            bool isBlackbody = false;
            if (litIt != params.end()) {
                if (auto* fv = std::get_if<float>(&litIt->second)) {
                    if (*fv > 100.0f) {
                        // Blackbody temperature
                        color = BlackbodyToRGB(*fv);
                        isBlackbody = true;
                    } else {
                        color = glm::vec3(*fv);
                    }
                } else {
                    color = GetVec3(params, "L", glm::vec3(1.0f));
                }
            }

            // Convert PBRT physical scale to a reasonable rasterizer intensity.
            // PBRT area lights use radiometric units meant for path tracers —
            // a logarithmic mapping keeps things in a usable range.
            if (isBlackbody) {
                lightIntensity = std::log2(scale + 1.0f) * 3.0f;
            } else {
                float maxComp = glm::max(color.r, glm::max(color.g, color.b));
                if (maxComp > 1.0f) {
                    color /= maxComp;
                    lightIntensity = scale * maxComp * 0.1f;
                } else {
                    lightIntensity = scale * 0.1f;
                }
            }

            m_State.areaLightEmission = color * lightIntensity;
            m_State.hasAreaLight = true;

        // ── ReverseOrientation ───────────────────────────────────────────────
        } else if (d == "ReverseOrientation") {
            m_State.reverseOrientation = !m_State.reverseOrientation;

        // ── Include ──────────────────────────────────────────────────────────
        } else if (d == "Include") {
            std::string incFile;
            if (lex.Peek().type == PbrtTokenType::String) incFile = lex.Next().value;
            std::string incPath = (std::filesystem::path(m_BaseDir) / incFile).string();
            lex.PushFile(incPath);

        // ── Object instancing ─────────────────────────────────────────────────
        } else if (d == "ObjectBegin") {
            std::string name;
            if (lex.Peek().type == PbrtTokenType::String) name = lex.Next().value;
            m_StateStack.push_back(m_State);
            m_InsideObject = true;
            m_CurrentObjectName = name;
            m_ObjectBuffer.clear();
            METAGFX_DEBUG << "PbrtParser: ObjectBegin '" << name << "'";

        } else if (d == "ObjectEnd") {
            m_NamedObjects[m_CurrentObjectName] = std::move(m_ObjectBuffer);
            m_ObjectBuffer.clear();
            m_InsideObject = false;
            if (!m_StateStack.empty()) {
                m_State = m_StateStack.back();
                m_StateStack.pop_back();
            }
            METAGFX_DEBUG << "PbrtParser: ObjectEnd '" << m_CurrentObjectName
                          << "' (" << m_NamedObjects[m_CurrentObjectName].size() << " meshes)";

        } else if (d == "ObjectInstance") {
            std::string name;
            if (lex.Peek().type == PbrtTokenType::String) name = lex.Next().value;
            auto it = m_NamedObjects.find(name);
            if (it != m_NamedObjects.end()) {
                const glm::mat4& M   = m_State.ctm;
                const glm::mat3  Mnv = glm::transpose(glm::inverse(glm::mat3(M)));
                for (const PbrtMesh& src : it->second) {
                    PbrtMesh copy;
                    copy.material = src.material;
                    copy.indices  = src.indices;
                    copy.vertices.reserve(src.vertices.size());
                    for (const Vertex& v : src.vertices) {
                        Vertex nv   = v;
                        nv.position = glm::vec3(M * glm::vec4(v.position, 1.0f));
                        nv.normal   = glm::normalize(Mnv * v.normal);
                        copy.vertices.push_back(nv);
                    }
                    m_Result.meshes.push_back(std::move(copy));
                }
                METAGFX_DEBUG << "PbrtParser: ObjectInstance '" << name
                              << "' → " << it->second.size() << " meshes";
            } else {
                METAGFX_WARN << "PbrtParser: unknown ObjectInstance '" << name << "'";
            }

        // ── WorldEnd ─────────────────────────────────────────────────────────
        } else if (d == "WorldEnd") {
            return;

        // ── Skip unrecognised directives ─────────────────────────────────────
        } else {
            // MediumInterface, CoordSysTransform, etc. — read type + params
            if (lex.Peek().type == PbrtTokenType::String) lex.Next();
            ParseParams(lex);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Conductor spectral data → RGB F0 conversion
// ─────────────────────────────────────────────────────────────────────────────

glm::vec3 PbrtParser::ConductorAlbedoFromParams(const ParamMap& params) {
    // Lookup table: PBRT named metal spectra → approximate sRGB normal-incidence
    // reflectance (Fresnel F0).  Computed from published optical constants via
    // F0 = ((n-1)^2 + k^2) / ((n+1)^2 + k^2) at wavelengths 630/532/467 nm.
    struct MetalF0 { const char* name; glm::vec3 f0; };
    static const MetalF0 kMetals[] = {
        { "Ag",  { 0.972f, 0.960f, 0.915f } },  // Silver
        { "Al",  { 0.913f, 0.922f, 0.924f } },  // Aluminum
        { "Au",  { 1.000f, 0.782f, 0.344f } },  // Gold
        { "Cr",  { 0.549f, 0.556f, 0.554f } },  // Chromium
        { "Cu",  { 0.955f, 0.638f, 0.538f } },  // Copper
        { "CuZn",{ 0.680f, 0.640f, 0.380f } },  // Brass (copper-zinc)
        { "Fe",  { 0.531f, 0.512f, 0.496f } },  // Iron
        { "Hg",  { 0.781f, 0.780f, 0.778f } },  // Mercury
        { "Li",  { 0.820f, 0.830f, 0.850f } },  // Lithium
        { "Ni",  { 0.660f, 0.609f, 0.526f } },  // Nickel
        { "Pb",  { 0.632f, 0.626f, 0.641f } },  // Lead
        { "Pt",  { 0.673f, 0.637f, 0.585f } },  // Platinum
        { "Ti",  { 0.542f, 0.497f, 0.449f } },  // Titanium
        { "W",   { 0.504f, 0.500f, 0.478f } },  // Tungsten
        { "Zn",  { 0.664f, 0.824f, 0.850f } },  // Zinc
    };

    // First check for explicit RGB reflectance override
    auto reflIt = params.find("reflectance");
    if (reflIt != params.end()) {
        if (auto* v = std::get_if<std::vector<float>>(&reflIt->second)) {
            if (v->size() >= 3) return { (*v)[0], (*v)[1], (*v)[2] };
        }
    }

    // Try to identify the metal from "spectrum eta" parameter name
    std::string etaStr = GetString(params, "eta", "");
    if (etaStr.empty()) return glm::vec3(0.9f);  // generic bright metal

    // etaStr is typically "metal-Ag-eta" or similar; extract the element symbol
    for (const auto& m : kMetals) {
        std::string pattern = std::string("metal-") + m.name + "-eta";
        if (etaStr.find(pattern) != std::string::npos ||
            etaStr == m.name) {
            METAGFX_DEBUG << "PbrtParser: conductor metal '" << m.name
                          << "' → F0(" << m.f0.r << ", " << m.f0.g << ", " << m.f0.b << ")";
            return m.f0;
        }
    }

    METAGFX_WARN << "PbrtParser: unknown conductor spectrum '" << etaStr
                 << "', using generic bright metal";
    return glm::vec3(0.9f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Material building
// ─────────────────────────────────────────────────────────────────────────────

Material PbrtParser::BuildMaterial(const std::string& typeStr, const ParamMap& params) {
    glm::vec3 albedo    { 0.5f, 0.5f, 0.5f };
    float     roughness = 0.5f;
    float     metallic  = 0.0f;

    if (typeStr == "diffuse" || typeStr == "coateddiffuse") {
        // GetVec3 returns default when reflectance is a texture ref (string) — handled below
        albedo    = GetVec3(params, "reflectance", glm::vec3(0.5f));
        roughness = (typeStr == "coateddiffuse")
                        ? GetFloat(params, "roughness", 0.5f)
                        : 1.0f;
        metallic  = 0.0f;

    } else if (typeStr == "conductor" || typeStr == "coatedconductor") {
        albedo = ConductorAlbedoFromParams(params);
        float isoRough = GetFloat(params, "roughness", -1.0f);
        if (isoRough >= 0.0f) {
            roughness = isoRough;
        } else {
            float ur = GetFloat(params, "uroughness", 0.1f);
            float vr = GetFloat(params, "vroughness", ur);
            roughness = (ur + vr) * 0.5f;
        }
        roughness = std::max(roughness, 0.02f);
        metallic  = 1.0f;

    } else if (typeStr == "dielectric") {
        albedo    = glm::vec3(1.0f);
        float isoRough = GetFloat(params, "roughness", -1.0f);
        if (isoRough >= 0.0f) {
            roughness = isoRough;
        } else {
            float ur = GetFloat(params, "uroughness", 0.1f);
            float vr = GetFloat(params, "vroughness", ur);
            roughness = (ur + vr) * 0.5f;
        }
        metallic  = 0.0f;

    } else if (typeStr == "mirror") {
        albedo    = glm::vec3(1.0f);
        roughness = 0.0f;
        metallic  = 1.0f;

    } else if (typeStr == "mix") {
        // Blend two named materials by 'amount' (PBRT v4: namedmaterial1/namedmaterial2)
        std::string n1 = GetString(params, "namedmaterial1",
                                   GetString(params, "mat1", ""));
        std::string n2 = GetString(params, "namedmaterial2",
                                   GetString(params, "mat2", ""));
        float amount = GetFloat(params, "amount", 0.5f);
        amount = glm::clamp(amount, 0.0f, 1.0f);

        auto it1 = m_NamedMaterials.find(n1);
        auto it2 = m_NamedMaterials.find(n2);

        if (it1 != m_NamedMaterials.end() && it2 != m_NamedMaterials.end()) {
            const Material& m1 = it1->second;
            const Material& m2 = it2->second;
            albedo    = glm::mix(m1.GetAlbedo(),     m2.GetAlbedo(),     amount);
            roughness = glm::mix(m1.GetRoughness(),  m2.GetRoughness(),  amount);
            metallic  = glm::mix(m1.GetMetallic(),   m2.GetMetallic(),   amount);
            Material mat(albedo, roughness, metallic);
            // Prefer the textured material's albedo map
            if (m1.HasAlbedoMap())      mat.SetAlbedoMap(m1.GetAlbedoMap());
            else if (m2.HasAlbedoMap()) mat.SetAlbedoMap(m2.GetAlbedoMap());
            return mat;
        } else if (it1 != m_NamedMaterials.end()) {
            return it1->second;
        } else if (it2 != m_NamedMaterials.end()) {
            return it2->second;
        }
        // fallback: grey diffuse
        METAGFX_DEBUG << "PbrtParser: mix material '" << n1 << "'/'" << n2
                      << "' not found in named materials, using grey diffuse";
        albedo = glm::vec3(0.5f); roughness = 0.5f; metallic = 0.0f;

    } else if (typeStr == "measured") {
        // Proprietary BSDF — approximate as white satin-like diffuse
        albedo    = glm::vec3(0.85f, 0.85f, 0.85f);
        roughness = 0.3f;
        metallic  = 0.0f;
        METAGFX_DEBUG << "PbrtParser: 'measured' material approximated as white coateddiffuse";

    } else {
        METAGFX_WARN << "PbrtParser: unknown material type '" << typeStr
                     << "', using default";
    }

    Material mat(albedo, roughness, metallic);

    // ── Texture map application ───────────────────────────────────────────────
    // Albedo: reflectance may be a texture reference (stored as string in params)
    std::string reflTexName = GetString(params, "reflectance", "");
    if (!reflTexName.empty()) {
        auto it = m_NamedTextures.find(reflTexName);
        if (it != m_NamedTextures.end()) {
            mat.SetAlbedoMap(it->second);
            METAGFX_DEBUG << "PbrtParser: material '" << typeStr
                          << "' albedo texture '" << reflTexName << "'";
        } else {
            METAGFX_WARN << "PbrtParser: texture '" << reflTexName
                         << "' referenced but not found ("
                         << m_NamedTextures.size() << " textures loaded)";
        }
    }

    // Normal map
    std::string normTexName = GetString(params, "normalmap", "");
    if (!normTexName.empty()) {
        auto it = m_NamedTextures.find(normTexName);
        if (it != m_NamedTextures.end())
            mat.SetNormalMap(it->second);
        else
            METAGFX_DEBUG << "PbrtParser: normalmap texture '" << normTexName << "' not found";
    }

    // Roughness map (only activated when roughness param is a texture name, not a float)
    std::string roughTexName = GetString(params, "roughness", "");
    if (!roughTexName.empty()) {
        auto it = m_NamedTextures.find(roughTexName);
        if (it != m_NamedTextures.end())
            mat.SetRoughnessMap(it->second);
    }

    // Metallic map
    std::string metalTexName = GetString(params, "metallic", "");
    if (!metalTexName.empty()) {
        auto it = m_NamedTextures.find(metalTexName);
        if (it != m_NamedTextures.end())
            mat.SetMetallicMap(it->second);
    }

    return mat;
}

void PbrtParser::HandleMaterial(const std::string& typeStr, const ParamMap& params) {
    m_State.material    = BuildMaterial(typeStr, params);
    m_State.hasMaterial = true;
}

void PbrtParser::HandleMakeNamedMaterial(const std::string& name, const ParamMap& params) {
    std::string typeStr = GetString(params, "type", "diffuse");
    Material mat = BuildMaterial(typeStr, params);
    METAGFX_DEBUG << "PbrtParser: MakeNamedMaterial '" << name << "' type='" << typeStr
                  << "' hasAlbedoMap=" << mat.HasAlbedoMap();
    m_NamedMaterials[name] = std::move(mat);
}

// ─────────────────────────────────────────────────────────────────────────────
// Texture
// ─────────────────────────────────────────────────────────────────────────────

void PbrtParser::HandleTexture(PbrtLexer& lex) {
    // Texture "name" "channel" "class" [params...]
    std::string texName, texChannel, texClass;
    if (lex.Peek().type == PbrtTokenType::String) texName    = lex.Next().value;
    if (lex.Peek().type == PbrtTokenType::String) texChannel = lex.Next().value;
    if (lex.Peek().type == PbrtTokenType::String) texClass   = lex.Next().value;

    ParamMap params = ParseParams(lex);

    // ── scale texture operator ────────────────────────────────────────────────
    if (texClass == "scale") {
        if (texChannel == "float") {
            // Float scale textures are bump/displacement maps — not supported in rasterizer
            METAGFX_DEBUG << "PbrtParser: skipping float scale texture '" << texName
                          << "' (bump mapping not supported in rasterizer)";
            return;
        }
        // Spectrum/RGB scale: pass the base texture through under the new name
        // (ignore the numeric scale factor — approximation)
        std::string baseName = GetString(params, "tex",
                               GetString(params, "tex1",
                               GetString(params, "tex2", "")));
        if (!baseName.empty()) {
            auto it = m_NamedTextures.find(baseName);
            if (it != m_NamedTextures.end()) {
                m_NamedTextures[texName] = it->second;
                METAGFX_DEBUG << "PbrtParser: scale texture '" << texName
                              << "' → pass-through from '" << baseName << "'";
            } else {
                METAGFX_DEBUG << "PbrtParser: scale texture '" << texName
                              << "': base '" << baseName << "' not yet loaded, skipping";
            }
        }
        return;
    }

    // ── imagemap ──────────────────────────────────────────────────────────────
    if (texClass != "imagemap") {
        METAGFX_DEBUG << "PbrtParser: skipping texture class '" << texClass
                      << "' (only imagemap and scale supported)";
        return;
    }

    std::string filename = GetString(params, "filename", "");
    if (filename.empty()) return;

    std::string fullPath = (std::filesystem::path(m_BaseDir) / filename).string();

    utils::ImageData img = utils::LoadImage(fullPath, 4);
    if (!img.pixels) {
        METAGFX_WARN << "PbrtParser: failed to load texture: " << fullPath;
        return;
    }

    // "float" channel → UNORM (linear scalar data: roughness, metallic, AO, bump)
    // "spectrum"/"rgb" channel → SRGB (colour data with gamma correction)
    rhi::Format fmt = (texChannel == "float")
                          ? rhi::Format::R8G8B8A8_UNORM
                          : rhi::Format::R8G8B8A8_SRGB;

    auto tex = utils::CreateTextureFromImage(m_Device, img, fmt);
    utils::FreeImage(img);

    if (tex) {
        m_NamedTextures[texName] = tex;
        METAGFX_INFO << "PbrtParser: loaded texture '" << texName << "' from " << filename
                     << " (channel=" << texChannel << ")";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Shape dispatch
// ─────────────────────────────────────────────────────────────────────────────

void PbrtParser::HandleShape(PbrtLexer& /*lex*/, const std::string& typeStr,
                              const ParamMap& params) {
    if (typeStr == "trianglemesh") {
        BuildTriangleMesh(params);
    } else if (typeStr == "plymesh") {
        BuildPlyMesh(params);
    } else if (typeStr == "sphere") {
        BuildSphere(params);
    } else if (typeStr == "disk") {
        BuildDisk(params);
    } else {
        METAGFX_DEBUG << "PbrtParser: skipping unsupported shape type: '" << typeStr << "'";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// TriangleMesh
// ─────────────────────────────────────────────────────────────────────────────

void PbrtParser::BuildTriangleMesh(const ParamMap& params) {
    // ── Positions ────────────────────────────────────────────────────────────
    auto posIt = params.find("P");
    if (posIt == params.end()) {
        METAGFX_WARN << "PbrtParser: TriangleMesh has no P (positions)";
        return;
    }
    const auto* posFloats = std::get_if<std::vector<float>>(&posIt->second);
    if (!posFloats || posFloats->size() < 3) {
        METAGFX_WARN << "PbrtParser: TriangleMesh P array is too small";
        return;
    }

    size_t numVerts = posFloats->size() / 3;

    // ── Normals (optional) ───────────────────────────────────────────────────
    const std::vector<float>* normFloats = nullptr;
    auto normIt = params.find("N");
    if (normIt != params.end()) {
        normFloats = std::get_if<std::vector<float>>(&normIt->second);
    }

    // ── UVs (optional) ───────────────────────────────────────────────────────
    const std::vector<float>* uvFloats = nullptr;
    auto uvIt = params.find("st");
    if (uvIt != params.end()) {
        uvFloats = std::get_if<std::vector<float>>(&uvIt->second);
    }
    // Also check "uv" as an alternate name
    if (!uvFloats) {
        uvIt = params.find("uv");
        if (uvIt != params.end()) {
            uvFloats = std::get_if<std::vector<float>>(&uvIt->second);
        }
    }

    // ── Indices ───────────────────────────────────────────────────────────────
    auto idxIt = params.find("indices");
    if (idxIt == params.end()) {
        METAGFX_WARN << "PbrtParser: TriangleMesh has no indices";
        return;
    }
    std::vector<uint32_t> indices;
    if (auto* vi = std::get_if<std::vector<int>>(&idxIt->second)) {
        indices.reserve(vi->size());
        for (int i : *vi) indices.push_back(static_cast<uint32_t>(i));
    } else if (auto* si = std::get_if<int>(&idxIt->second)) {
        indices.push_back(static_cast<uint32_t>(*si));
    }
    if (indices.empty()) {
        METAGFX_WARN << "PbrtParser: TriangleMesh has no valid indices";
        return;
    }

    // ── Apply CTM ────────────────────────────────────────────────────────────
    const glm::mat4& M   = m_State.ctm;
    const glm::mat3  Mnv = glm::transpose(glm::inverse(glm::mat3(M)));

    std::vector<Vertex> vertices;
    vertices.reserve(numVerts);

    for (size_t i = 0; i < numVerts; ++i) {
        Vertex v;

        glm::vec3 pos(
            (*posFloats)[i * 3 + 0],
            (*posFloats)[i * 3 + 1],
            (*posFloats)[i * 3 + 2]);
        v.position = glm::vec3(M * glm::vec4(pos, 1.0f));

        if (normFloats && normFloats->size() >= (i + 1) * 3) {
            glm::vec3 n(
                (*normFloats)[i * 3 + 0],
                (*normFloats)[i * 3 + 1],
                (*normFloats)[i * 3 + 2]);
            v.normal = glm::normalize(Mnv * n);
        } else {
            v.normal = glm::vec3(0.0f, 1.0f, 0.0f);  // placeholder; flat normals computed below
        }

        if (uvFloats && uvFloats->size() >= (i + 1) * 2) {
            v.texCoord = glm::vec2(
                (*uvFloats)[i * 2 + 0],
                (*uvFloats)[i * 2 + 1]);
        } else {
            v.texCoord = glm::vec2(0.0f);
        }

        vertices.push_back(v);
    }

    // ── Flat normals fallback ─────────────────────────────────────────────────
    if (!normFloats || normFloats->empty()) {
        // Compute per-face normals and assign to each vertex
        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            uint32_t i0 = indices[i + 0];
            uint32_t i1 = indices[i + 1];
            uint32_t i2 = indices[i + 2];
            if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size())
                continue;
            glm::vec3 e1 = vertices[i1].position - vertices[i0].position;
            glm::vec3 e2 = vertices[i2].position - vertices[i0].position;
            glm::vec3 n  = glm::normalize(glm::cross(e1, e2));
            vertices[i0].normal = n;
            vertices[i1].normal = n;
            vertices[i2].normal = n;
        }
    }

    // ── ReverseOrientation ────────────────────────────────────────────────────
    if (m_State.reverseOrientation) {
        for (auto& v : vertices) v.normal = -v.normal;
    }

    // ── Emit mesh ─────────────────────────────────────────────────────────────
    PbrtMesh mesh;
    mesh.vertices = std::move(vertices);
    mesh.indices  = std::move(indices);
    mesh.material = m_State.hasMaterial ? m_State.material : Material{};

    // ── Area light emission ──────────────────────────────────────────────────
    if (m_State.hasAreaLight) {
        mesh.material.SetEmissiveFactor(m_State.areaLightEmission);

        // Create a point light at the centroid of the emissive shape
        glm::vec3 centroid(0.0f);
        for (const auto& v : mesh.vertices) centroid += v.position;
        if (!mesh.vertices.empty())
            centroid /= static_cast<float>(mesh.vertices.size());

        float maxComp = glm::max(m_State.areaLightEmission.r,
                                 glm::max(m_State.areaLightEmission.g,
                                          m_State.areaLightEmission.b));
        float intensity = maxComp * 0.1f;
        glm::vec3 color = m_State.areaLightEmission / glm::max(maxComp, 0.001f);

        auto light = std::make_unique<PointLight>(centroid, 100.0f, color, intensity);
        m_Result.lights.push_back(std::move(light));
    }

    EmitMesh(std::move(mesh));
}

// ─────────────────────────────────────────────────────────────────────────────
// PLYMesh
// ─────────────────────────────────────────────────────────────────────────────

void PbrtParser::BuildPlyMesh(const ParamMap& params) {
    std::string filename = GetString(params, "filename", "");
    if (filename.empty()) {
        METAGFX_WARN << "PbrtParser: PLYMesh has no filename";
        return;
    }

    std::string fullPath = (std::filesystem::path(m_BaseDir) / filename).string();
    METAGFX_TRACE << "PbrtParser: loading PLY mesh: " << fullPath;

    Assimp::Importer importer;
    const aiScene* aiScene = importer.ReadFile(fullPath,
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_JoinIdenticalVertices);

    if (!aiScene || !aiScene->mRootNode || aiScene->mNumMeshes == 0) {
        METAGFX_ERROR << "PbrtParser: Assimp failed to load: " << fullPath
                      << " — " << importer.GetErrorString();
        return;
    }

    // Extract the first mesh from the Assimp scene
    const aiMesh* aim = aiScene->mMeshes[0];

    const glm::mat4& M   = m_State.ctm;
    const glm::mat3  Mnv = glm::transpose(glm::inverse(glm::mat3(M)));

    std::vector<Vertex> vertices;
    vertices.reserve(aim->mNumVertices);

    for (uint32_t i = 0; i < aim->mNumVertices; ++i) {
        Vertex v;

        glm::vec3 pos(aim->mVertices[i].x, aim->mVertices[i].y, aim->mVertices[i].z);
        v.position = glm::vec3(M * glm::vec4(pos, 1.0f));

        if (aim->HasNormals()) {
            glm::vec3 n(aim->mNormals[i].x, aim->mNormals[i].y, aim->mNormals[i].z);
            v.normal = glm::normalize(Mnv * n);
        } else {
            v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }

        if (aim->HasTextureCoords(0)) {
            v.texCoord = glm::vec2(aim->mTextureCoords[0][i].x,
                                   aim->mTextureCoords[0][i].y);
        } else {
            v.texCoord = glm::vec2(0.0f);
        }

        vertices.push_back(v);
    }

    std::vector<uint32_t> indices;
    indices.reserve(aim->mNumFaces * 3);
    for (uint32_t i = 0; i < aim->mNumFaces; ++i) {
        const aiFace& face = aim->mFaces[i];
        for (uint32_t j = 0; j < face.mNumIndices; ++j) {
            indices.push_back(face.mIndices[j]);
        }
    }

    if (m_State.reverseOrientation) {
        for (auto& v : vertices) v.normal = -v.normal;
    }

    PbrtMesh mesh;
    mesh.vertices = std::move(vertices);
    mesh.indices  = std::move(indices);
    mesh.material = m_State.hasMaterial ? m_State.material : Material{};

    // ── Area light emission (same logic as BuildTriangleMesh) ────────────────
    if (m_State.hasAreaLight) {
        mesh.material.SetEmissiveFactor(m_State.areaLightEmission);

        glm::vec3 centroid(0.0f);
        for (const auto& v : mesh.vertices) centroid += v.position;
        if (!mesh.vertices.empty())
            centroid /= static_cast<float>(mesh.vertices.size());

        float maxComp = glm::max(m_State.areaLightEmission.r,
                                 glm::max(m_State.areaLightEmission.g,
                                          m_State.areaLightEmission.b));
        float intensity = maxComp * 0.1f;
        glm::vec3 color = m_State.areaLightEmission / glm::max(maxComp, 0.001f);

        auto light = std::make_unique<PointLight>(centroid, 100.0f, color, intensity);
        m_Result.lights.push_back(std::move(light));
    }

    EmitMesh(std::move(mesh));
}

// ─────────────────────────────────────────────────────────────────────────────
// EmitMesh helper
// ─────────────────────────────────────────────────────────────────────────────

void PbrtParser::EmitMesh(PbrtMesh mesh) {
    if (m_InsideObject) {
        m_ObjectBuffer.push_back(std::move(mesh));
    } else {
        m_Result.meshes.push_back(std::move(mesh));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Sphere shape
// ─────────────────────────────────────────────────────────────────────────────

void PbrtParser::BuildSphere(const ParamMap& params) {
    float radius = GetFloat(params, "radius", 1.0f);

    const uint32_t rings    = 16;
    const uint32_t segments = 32;

    const glm::mat4& M   = m_State.ctm;
    const glm::mat3  Mnv = glm::transpose(glm::inverse(glm::mat3(M)));

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    for (uint32_t y = 0; y <= rings; ++y) {
        float phi = glm::pi<float>() * static_cast<float>(y) / static_cast<float>(rings);
        for (uint32_t x = 0; x <= segments; ++x) {
            float theta = 2.0f * glm::pi<float>() * static_cast<float>(x) / static_cast<float>(segments);
            Vertex v;
            glm::vec3 local(radius * std::sin(phi) * std::cos(theta),
                             radius * std::cos(phi),
                             radius * std::sin(phi) * std::sin(theta));
            v.position = glm::vec3(M * glm::vec4(local, 1.0f));
            v.normal   = glm::normalize(Mnv * glm::normalize(local));
            v.texCoord = glm::vec2(static_cast<float>(x) / segments,
                                   static_cast<float>(y) / rings);
            vertices.push_back(v);
        }
    }

    for (uint32_t y = 0; y < rings; ++y) {
        for (uint32_t x = 0; x < segments; ++x) {
            uint32_t c = y * (segments + 1) + x;
            uint32_t n = c + segments + 1;
            indices.push_back(c);     indices.push_back(n);     indices.push_back(c + 1);
            indices.push_back(c + 1); indices.push_back(n);     indices.push_back(n + 1);
        }
    }

    PbrtMesh mesh;
    mesh.vertices = std::move(vertices);
    mesh.indices  = std::move(indices);
    mesh.material = m_State.hasMaterial ? m_State.material : Material{};
    EmitMesh(std::move(mesh));
}

// ─────────────────────────────────────────────────────────────────────────────
// Disk shape
// ─────────────────────────────────────────────────────────────────────────────

void PbrtParser::BuildDisk(const ParamMap& params) {
    float radius = GetFloat(params, "radius", 1.0f);
    float height = GetFloat(params, "height", 0.0f);

    const uint32_t segments = 32;

    const glm::mat4& M   = m_State.ctm;
    const glm::mat3  Mnv = glm::transpose(glm::inverse(glm::mat3(M)));

    // PBRT disk faces +Z; transform the local normal
    glm::vec3 localNormal(0.0f, 0.0f, 1.0f);
    glm::vec3 worldNormal = glm::normalize(Mnv * localNormal);

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // Center vertex
    {
        Vertex c;
        c.position = glm::vec3(M * glm::vec4(0.0f, 0.0f, height, 1.0f));
        c.normal   = worldNormal;
        c.texCoord = glm::vec2(0.5f, 0.5f);
        vertices.push_back(c);
    }

    for (uint32_t i = 0; i <= segments; ++i) {
        float angle = 2.0f * glm::pi<float>() * static_cast<float>(i) / segments;
        float cx = radius * std::cos(angle);
        float cy = radius * std::sin(angle);
        Vertex v;
        v.position = glm::vec3(M * glm::vec4(cx, cy, height, 1.0f));
        v.normal   = worldNormal;
        v.texCoord = glm::vec2(cx / (2.0f * radius) + 0.5f,
                               cy / (2.0f * radius) + 0.5f);
        vertices.push_back(v);
    }

    for (uint32_t i = 0; i < segments; ++i) {
        indices.push_back(0);
        indices.push_back(i + 1);
        indices.push_back(i + 2);
    }

    PbrtMesh mesh;
    mesh.vertices = std::move(vertices);
    mesh.indices  = std::move(indices);
    mesh.material = m_State.hasMaterial ? m_State.material : Material{};
    EmitMesh(std::move(mesh));
}

// ─────────────────────────────────────────────────────────────────────────────
// Light building
// ─────────────────────────────────────────────────────────────────────────────

void PbrtParser::HandleLightSource(const std::string& typeStr, const ParamMap& params) {
    if (typeStr == "distant") {
        glm::vec3 from = GetVec3(params, "from", glm::vec3(0.0f, 0.0f, 0.0f));
        glm::vec3 to   = GetVec3(params, "to",   glm::vec3(0.0f, 0.0f, 1.0f));
        glm::vec3 color    = GetVec3(params, "L", glm::vec3(1.0f));
        float     scale    = GetFloat(params, "scale", 1.0f);
        float     intensity = scale;

        glm::vec3 dir = glm::normalize(to - from);
        if (glm::length(dir) < 1e-6f) dir = glm::vec3(0.0f, -1.0f, 0.0f);

        auto light = std::make_unique<DirectionalLight>(dir, color, intensity);
        m_Result.lights.push_back(std::move(light));

    } else if (typeStr == "point") {
        glm::vec3 from  = GetVec3(params, "from",  glm::vec3(0.0f));
        glm::vec3 color = GetVec3(params, "I",     glm::vec3(1.0f));
        float     scale = GetFloat(params, "scale", 1.0f);

        // Scale the intensity by scale; derive a reasonable range
        float intensity = scale * 0.001f;  // large scale values → normalise
        float range     = 2000.0f;

        auto light = std::make_unique<PointLight>(from, range, color, intensity);
        m_Result.lights.push_back(std::move(light));

    } else if (typeStr == "spot") {
        glm::vec3 from      = GetVec3(params, "from",  glm::vec3(0.0f));
        glm::vec3 to        = GetVec3(params, "to",    glm::vec3(0.0f, 0.0f, 1.0f));
        glm::vec3 color     = GetVec3(params, "I",     glm::vec3(1.0f));
        float     scale     = GetFloat(params, "scale", 1.0f);
        float     coneAngle = GetFloat(params, "coneangle",      30.0f);   // degrees
        float     coneDelta = GetFloat(params, "conedeltaangle",  5.0f);   // degrees

        float intensity  = scale * 0.001f;
        float outerAngle = coneAngle;
        float innerAngle = coneAngle - coneDelta;

        glm::vec3 dir = to - from;
        dir = (glm::length(dir) > 1e-6f) ? glm::normalize(dir) : glm::vec3(0.0f, -1.0f, 0.0f);

        auto light = std::make_unique<SpotLight>(from, dir, innerAngle, outerAngle,
                                                  500.0f, color, intensity);
        m_Result.lights.push_back(std::move(light));

    } else if (typeStr == "infinite") {
        // Environment / IBL light — store the map path for the caller to use
        std::string mapFile = GetString(params, "mapname",
                              GetString(params, "filename", ""));
        float scale = GetFloat(params, "scale", 1.0f);
        if (!mapFile.empty()) {
            m_Result.envMapPath  = (std::filesystem::path(m_BaseDir) / mapFile).string();
            m_Result.envMapScale = scale;
            METAGFX_INFO << "PbrtParser: infinite light → env map: " << m_Result.envMapPath;
        } else {
            METAGFX_DEBUG << "PbrtParser: infinite light has no mapname, skipping";
        }

    } else {
        METAGFX_DEBUG << "PbrtParser: skipping unsupported light type: '" << typeStr << "'";
    }
}

} // namespace metagfx
