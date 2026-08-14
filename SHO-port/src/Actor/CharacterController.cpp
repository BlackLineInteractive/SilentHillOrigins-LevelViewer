#include "SHO/Actor/CharacterController.h"
#include <algorithm>
#include <cmath>

namespace SHO {
namespace Actor {

CharacterController::CharacterController()
    : m_radius(0.35f), m_height(1.8f), m_gravity(9.81f), m_maxStepHeight(0.35f),
      m_verticalVelocity(0.0f), m_isGrounded(false), m_groundNormal(0, 1, 0) {}

void CharacterController::SetCollisionMesh(const std::vector<Core::CollisionFace>& faces) {
    m_faces = faces;
}

// Ericson's barycentric closest point on triangle
Core::Vec3 CharacterController::ClosestPointOnTriangle(
    const Core::Vec3& p, const Core::Vec3& a, const Core::Vec3& b, const Core::Vec3& c) const {

    Core::Vec3 ab = b - a;
    Core::Vec3 ac = c - a;
    Core::Vec3 ap = p - a;

    float d1 = glm::dot(ab, ap);
    float d2 = glm::dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return a;

    Core::Vec3 bp = p - b;
    float d3 = glm::dot(ab, bp);
    float d4 = glm::dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) return b;

    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        float v = d1 / (d1 - d3);
        return a + v * ab;
    }

    Core::Vec3 cp = p - c;
    float d5 = glm::dot(ab, cp);
    float d6 = glm::dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) return c;

    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        float w = d2 / (d2 - d6);
        return a + w * ac;
    }

    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return b + w * (c - b);
    }

    float denom = 1.0f / (va + vb + vc);
    float v = vb * denom;
    float w = vc * denom;
    return a + ab * v + ac * w;
}

Core::Vec3 CharacterController::Move(const Core::Vec3& currentPos, const Core::Vec3& displacement, float dt) {
    if (m_faces.empty()) {
        return currentPos + displacement;
    }

    // Apply gravity
    if (!m_isGrounded) {
        m_verticalVelocity -= m_gravity * dt;
    } else {
        m_verticalVelocity = -0.5f; // small downward stick
    }

    Core::Vec3 target = currentPos + displacement + Core::Vec3(0, m_verticalVelocity * dt, 0);

    // Iterative sphere-capsule collision resolution (3 passes)
    for (int iter = 0; iter < 3; iter++) {
        Core::Vec3 feet = target + Core::Vec3(0, m_radius, 0);
        Core::Vec3 waist = target + Core::Vec3(0, m_height * 0.5f, 0);

        for (const auto& face : m_faces) {
            // Check lower sphere (feet)
            Core::Vec3 ptFeet = ClosestPointOnTriangle(feet, face.v0, face.v1, face.v2);
            Core::Vec3 diffFeet = feet - ptFeet;
            float distFeet = glm::length(diffFeet);

            if (distFeet < m_radius && distFeet > 1e-5f) {
                Core::Vec3 norm = diffFeet / distFeet;
                float penetration = m_radius - distFeet;
                target += norm * penetration;
                feet += norm * penetration;
                waist += norm * penetration;
            }

            // Check waist sphere
            Core::Vec3 ptWaist = ClosestPointOnTriangle(waist, face.v0, face.v1, face.v2);
            Core::Vec3 diffWaist = waist - ptWaist;
            float distWaist = glm::length(diffWaist);

            if (distWaist < m_radius && distWaist > 1e-5f) {
                // Ignore steep slope / floor for waist
                if (std::abs(diffWaist.y / distWaist) < 0.7f) {
                    Core::Vec3 norm = diffWaist / distWaist;
                    norm.y = 0.0f; // horizontal push only
                    if (glm::length(norm) > 1e-4f) {
                        norm = glm::normalize(norm);
                        float penetration = m_radius - distWaist;
                        target += norm * penetration;
                    }
                }
            }
        }
    }

    // Ground raycast check
    m_isGrounded = false;
    m_groundNormal = Core::Vec3(0, 1, 0);

    Core::Vec3 rayOrigin = target + Core::Vec3(0, m_maxStepHeight + 0.1f, 0);
    float closestGroundY = -1e30f;

    for (const auto& face : m_faces) {
        // Floor faces have positive Y normal
        if (face.normal.y < 0.4f) continue;

        // Check if ray hits triangle horizontally
        Core::Vec3 p = ClosestPointOnTriangle(target + Core::Vec3(0, 0.2f, 0), face.v0, face.v1, face.v2);
        float xzDist = glm::distance(Core::Vec2(target.x, target.z), Core::Vec2(p.x, p.z));

        if (xzDist <= m_radius) {
            float groundY = p.y;
            if (groundY > closestGroundY && groundY <= rayOrigin.y) {
                closestGroundY = groundY;
                m_groundNormal = face.normal;
            }
        }
    }

    if (closestGroundY > -1e20f) {
        float deltaY = closestGroundY - target.y;
        if (target.y <= closestGroundY || (deltaY >= -0.4f && deltaY <= m_maxStepHeight)) {
            target.y = closestGroundY;
            m_isGrounded = true;
            m_verticalVelocity = 0.0f;
        }
    }

    return target;
}


} // namespace Actor
} // namespace SHO
