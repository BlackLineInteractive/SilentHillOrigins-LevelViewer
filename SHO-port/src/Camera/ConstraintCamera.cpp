#include "SHO/Camera/ConstraintCamera.h"
#include <algorithm>
#include <cmath>

namespace SHO {
namespace Camera {

ConstraintCamera::ConstraintCamera() {
    m_className = "CConstraintCamera";
    m_fovDeg = 60.0f;
}

void ConstraintCamera::SetPitchLimits(float minPitchDeg, float maxPitchDeg) {
    m_minPitch = minPitchDeg;
    m_maxPitch = maxPitchDeg;
}

void ConstraintCamera::SetYawLimits(float minYawDeg, float maxYawDeg) {
    m_minYaw = minYawDeg;
    m_maxYaw = maxYawDeg;
}

void ConstraintCamera::OnUpdate(float dt) {
    Core::Vec3 anchor = GetPosition();
    if (glm::length(m_currentEye) < 1e-4f) {
        m_currentEye = anchor;
    }

    if (!m_isCurrent) {
        m_currentEye = anchor;
        return;
    }

    // Compute direction to target
    Core::Vec3 toTarget = m_targetPos - m_currentEye;
    float dist = glm::length(toTarget);

    if (dist > 1e-4f) {
        Core::Vec3 desiredFwd = toTarget / dist;
        float pitch = glm::degrees(std::asin(std::clamp(-desiredFwd.y, -1.0f, 1.0f)));
        float yaw = glm::degrees(std::atan2(desiredFwd.x, desiredFwd.z));

        // Clamp to constrained angles
        pitch = std::clamp(pitch, m_minPitch, m_maxPitch);
        yaw = std::clamp(yaw, m_minYaw, m_maxYaw);

        float radP = glm::radians(pitch);
        float radY = glm::radians(yaw);

        Core::Vec3 constrainedFwd = glm::normalize(Core::Vec3(
            std::sin(radY) * std::cos(radP),
            -std::sin(radP),
            std::cos(radY) * std::cos(radP)
        ));

        float blendRate = std::clamp(m_trackingSpeed * dt, 0.0f, 1.0f);
        m_currentForward = glm::normalize(glm::mix(m_currentForward, constrainedFwd, blendRate));

        // Constrain eye position inside volume if specified
        if (m_hasVolume) {
            Core::Vec3 desiredEye = m_targetPos - m_currentForward * m_trackDistance;
            desiredEye.x = std::clamp(desiredEye.x, m_volume.min.x, m_volume.max.x);
            desiredEye.y = std::clamp(desiredEye.y, m_volume.min.y, m_volume.max.y);
            desiredEye.z = std::clamp(desiredEye.z, m_volume.min.z, m_volume.max.z);
            m_currentEye = glm::mix(m_currentEye, desiredEye, blendRate);
        } else {
            m_currentEye = anchor;
        }
    }
}

Core::Mat4 ConstraintCamera::GetViewMatrix() const {
    Core::Vec3 up = Core::Vec3(0, 1, 0);
    return glm::lookAt(m_currentEye, m_currentEye + m_currentForward, up);
}

} // namespace Camera
} // namespace SHO
