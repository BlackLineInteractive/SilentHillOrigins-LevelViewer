#include "SHO/Camera/StaticCamera.h"
#include <cmath>

namespace SHO {
namespace Camera {

StaticCamera::StaticCamera() {
    m_className = "CStaticCamera";
}

void StaticCamera::ExtractAuthoredAngles() {
    // In RenderWare PS2 matrix conventions (right/up/at/pos), row 2 is the 'at' (forward) vector
    Core::Vec3 fwd = GetForward();
    if (glm::length(fwd) > 1e-4f) {
        fwd = glm::normalize(fwd);
        m_authoredPitch = glm::degrees(std::asin(std::clamp(-fwd.y, -1.0f, 1.0f)));
        m_authoredYaw = glm::degrees(std::atan2(fwd.x, fwd.z));
        m_hasAuthoredAngles = true;
        m_currentForward = fwd;
    }
}

void StaticCamera::SetAuthoredAngles(float pitchDeg, float yawDeg) {
    m_authoredPitch = pitchDeg;
    m_authoredYaw = yawDeg;
    m_hasAuthoredAngles = true;

    float radP = glm::radians(pitchDeg);
    float radY = glm::radians(yawDeg);

    m_currentForward = glm::normalize(Core::Vec3(
        std::sin(radY) * std::cos(radP),
        -std::sin(radP),
        std::cos(radY) * std::cos(radP)
    ));
}

void StaticCamera::OnUpdate(float dt) {
    if (!m_hasAuthoredAngles) {
        ExtractAuthoredAngles();
    }

    if (!m_isCurrent) return;

    Core::Vec3 eye = GetPosition();
    Core::Vec3 toTarget = m_targetPos - eye;
    float dist = glm::length(toTarget);

    if (m_dynamicFallback && dist > 0.001f) {
        Core::Vec3 desiredFwd = toTarget / dist;
        // Check if target is inside authored FOV cone
        float dot = glm::dot(desiredFwd, m_currentForward);
        float thresholdCos = std::cos(glm::radians(m_fovDeg * 0.45f));

        if (dot < thresholdCos) {
            // Target is leaving frame: smoothly blend towards target look direction
            float blendSpeed = 3.0f * dt;
            m_currentForward = glm::normalize(glm::mix(m_currentForward, desiredFwd, std::clamp(blendSpeed, 0.0f, 1.0f)));
        }
    }
}

Core::Mat4 StaticCamera::GetViewMatrix() const {
    Core::Vec3 eye = GetPosition();
    Core::Vec3 up = Core::Vec3(0, 1, 0);
    return glm::lookAt(eye, eye + m_currentForward, up);
}

} // namespace Camera
} // namespace SHO
