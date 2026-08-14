#pragma once

#include "SHO/Core/Types.h"
#include <vector>

namespace SHO {
namespace Actor {

// Character Controller with barycentric collision against CBSP faces
class CharacterController {
public:
    CharacterController();
    ~CharacterController() = default;

    void SetRadius(float r) { m_radius = r; }
    float GetRadius() const { return m_radius; }

    void SetHeight(float h) { m_height = h; }
    float GetHeight() const { return m_height; }

    void SetGravity(float g) { m_gravity = g; }
    void SetMaxStepHeight(float sh) { m_maxStepHeight = sh; }

    // Collision mesh feed
    void SetCollisionMesh(const std::vector<Core::CollisionFace>& faces);

    // Movement resolution: returns final adjusted world position
    Core::Vec3 Move(const Core::Vec3& currentPos, const Core::Vec3& displacement, float dt);

    bool IsGrounded() const { return m_isGrounded; }
    const Core::Vec3& GetGroundNormal() const { return m_groundNormal; }

private:
    Core::Vec3 ClosestPointOnTriangle(const Core::Vec3& p, const Core::Vec3& a, const Core::Vec3& b, const Core::Vec3& c) const;

    float m_radius = 0.35f;
    float m_height = 1.8f;
    float m_gravity = 9.81f;
    float m_maxStepHeight = 0.35f;
    float m_verticalVelocity = 0.0f;
    bool  m_isGrounded = false;
    Core::Vec3 m_groundNormal = Core::Vec3(0, 1, 0);

    std::vector<Core::CollisionFace> m_faces;
};

} // namespace Actor
} // namespace SHO
