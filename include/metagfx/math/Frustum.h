// ============================================================================
// include/metagfx/math/Frustum.h
// Header-only frustum culling utilities using the Gribb-Hartmann method.
// ============================================================================
#pragma once

#include <glm/glm.hpp>
#include <array>

namespace metagfx {

struct Plane {
    glm::vec3 normal;
    float     distance;  // D coefficient in Ax+By+Cz+D=0 (signed distance from origin)

    float DistanceTo(const glm::vec3& point) const {
        return glm::dot(normal, point) + distance;
    }
};

class Frustum {
public:
    // Extract frustum planes from a combined view-projection matrix.
    // Uses the Gribb-Hartmann method: row-combine VP matrix columns.
    // Planes are in world space; normals point INWARD.
    static Frustum FromViewProjection(const glm::mat4& vp) {
        // GLM stores matrices in column-major order.
        // The rows of the VP matrix are the columns of the GLM mat4.
        // Row i = { vp[0][i], vp[1][i], vp[2][i], vp[3][i] }
        //
        // Planes (Gribb-Hartmann):
        //   Left:   row3 + row0
        //   Right:  row3 - row0
        //   Bottom: row3 + row1
        //   Top:    row3 - row1
        //   Near:   row3 + row2
        //   Far:    row3 - row2

        Frustum f;

        // Helper: extract a plane from the VP matrix using Gribb-Hartmann
        auto extractPlane = [&](int sign, int row) -> Plane {
            Plane p;
            p.normal.x = vp[0][3] + sign * vp[0][row];
            p.normal.y = vp[1][3] + sign * vp[1][row];
            p.normal.z = vp[2][3] + sign * vp[2][row];
            p.distance = vp[3][3] + sign * vp[3][row];
            // Normalize so DistanceTo returns metric distance
            float len = glm::length(p.normal);
            if (len > 1e-6f) {
                p.normal  /= len;
                p.distance /= len;
            }
            return p;
        };

        f.m_Planes[0] = extractPlane(+1, 0);  // Left
        f.m_Planes[1] = extractPlane(-1, 0);  // Right
        f.m_Planes[2] = extractPlane(+1, 1);  // Bottom
        f.m_Planes[3] = extractPlane(-1, 1);  // Top
        f.m_Planes[4] = extractPlane(+1, 2);  // Near
        f.m_Planes[5] = extractPlane(-1, 2);  // Far
        return f;
    }

    // Returns true if the sphere (center, radius) is at least partially inside the frustum.
    bool IntersectsSphere(const glm::vec3& center, float radius) const {
        for (const auto& plane : m_Planes) {
            if (plane.DistanceTo(center) < -radius) {
                return false;  // Entirely outside this plane
            }
        }
        return true;
    }

    // Returns true if the AABB [min, max] is at least partially inside the frustum.
    bool IntersectsAABB(const glm::vec3& min, const glm::vec3& max) const {
        for (const auto& plane : m_Planes) {
            // Find the positive vertex (farthest along plane normal)
            glm::vec3 pv(
                plane.normal.x >= 0 ? max.x : min.x,
                plane.normal.y >= 0 ? max.y : min.y,
                plane.normal.z >= 0 ? max.z : min.z
            );
            if (plane.DistanceTo(pv) < 0.0f) {
                return false;  // Positive vertex is outside → entire AABB is outside
            }
        }
        return true;
    }

private:
    std::array<Plane, 6> m_Planes;  // Left, Right, Bottom, Top, Near, Far
};

} // namespace metagfx
