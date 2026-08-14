#include "SHO/Camera/BaseCamera.h"
#include "SHO/Camera/CameraManager.h"

namespace SHO {
namespace Camera {

BaseCamera::BaseCamera() {
    m_className = "CBaseCamera";
}

void BaseCamera::SetEventName(const std::string& name) {
    m_eventName = name;
    if (m_altEventName.empty() && !name.empty()) {
        m_altEventName = name + "S";
    }

    // Subscribe to both primary and 'S' event triggers
    ListenToEvent(m_eventName);
    ListenToEvent(m_altEventName);
}

void BaseCamera::SetCurrent(bool current) {
    m_isCurrent = current;
}

void BaseCamera::OnUpdate(float /*dt*/) {
    // Base camera update
}

void BaseCamera::OnEvent(const Core::Msg& /*msg*/) {
    // When a trigger fires an event matching this camera's name, activate this camera
    CameraManager::GetInstance().SetActiveCamera(this);
}

Core::Mat4 BaseCamera::GetViewMatrix() const {
    Core::Vec3 eye = GetPosition();
    Core::Vec3 forward = GetForward();
    Core::Vec3 up = GetUp();
    return glm::lookAt(eye, eye + forward, up);
}

Core::Mat4 BaseCamera::GetProjectionMatrix(float aspect, float nearZ, float farZ) const {
    return glm::perspective(glm::radians(m_fovDeg), aspect, nearZ, farZ);
}

} // namespace Camera
} // namespace SHO
