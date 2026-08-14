#include "SHO/Camera/CameraManager.h"
#include "SHO/Core/EventManager.h"
#include <algorithm>
#include <iostream>

namespace SHO {
namespace Camera {

CameraManager& CameraManager::GetInstance() {
    static CameraManager instance;
    return instance;
}

void CameraManager::RegisterCamera(BaseCamera* cam) {
    if (!cam) return;

    if (std::find(m_cameras.begin(), m_cameras.end(), cam) == m_cameras.end()) {
        m_cameras.push_back(cam);
    }

    if (!cam->GetName().empty()) m_nameLookup[cam->GetName()] = cam;
    if (!cam->GetEventName().empty()) m_nameLookup[cam->GetEventName()] = cam;
    if (!cam->GetAltEventName().empty()) m_nameLookup[cam->GetAltEventName()] = cam;

    if (!m_activeCamera) {
        SetActiveCamera(cam);
    }
}

void CameraManager::UnregisterCamera(BaseCamera* cam) {
    if (!cam) return;

    m_cameras.erase(std::remove(m_cameras.begin(), m_cameras.end(), cam), m_cameras.end());

    for (auto it = m_nameLookup.begin(); it != m_nameLookup.end(); ) {
        if (it->second == cam) it = m_nameLookup.erase(it);
        else ++it;
    }

    if (m_activeCamera == cam) {
        m_activeCamera = m_cameras.empty() ? nullptr : m_cameras.front();
        if (m_activeCamera) m_activeCamera->SetCurrent(true);
    }
}

BaseCamera* CameraManager::FindCamera(const std::string& name) const {
    auto it = m_nameLookup.find(name);
    if (it != m_nameLookup.end()) return it->second;
    return nullptr;
}

void CameraManager::SetActiveCamera(BaseCamera* cam) {
    if (!cam || cam == m_activeCamera) return;

    if (m_activeCamera) {
        m_activeCamera->SetCurrent(false);
    }

    m_activeCamera = cam;
    m_activeCamera->SetCurrent(true);
    m_didCut = true;

    // Send camera cut message through EventManager
    Core::EventManager::GetInstance().SendMsg("CameraCut", this, m_activeCamera, sizeof(BaseCamera*));
}

void CameraManager::SetActiveCamera(const std::string& name) {
    BaseCamera* cam = FindCamera(name);
    if (cam) {
        SetActiveCamera(cam);
    }
}

void CameraManager::Update(float dt, const Core::Vec3& playerTargetPos) {
    m_didCut = false;

    for (auto* cam : m_cameras) {
        if (cam) {
            cam->SetTargetPosition(playerTargetPos);
            cam->OnUpdate(dt);
        }
    }
}

Core::Mat4 CameraManager::GetViewMatrix() const {
    if (m_activeCamera) {
        return m_activeCamera->GetViewMatrix();
    }
    return glm::lookAt(Core::Vec3(0, 2, 5), Core::Vec3(0, 1, 0), Core::Vec3(0, 1, 0));
}

Core::Mat4 CameraManager::GetProjectionMatrix(float aspect) const {
    if (m_activeCamera) {
        return m_activeCamera->GetProjectionMatrix(aspect);
    }
    return glm::perspective(glm::radians(60.0f), aspect, 0.1f, 200.0f);
}

void CameraManager::Clear() {
    m_cameras.clear();
    m_nameLookup.clear();
    m_activeCamera = nullptr;
    m_didCut = false;
}

} // namespace Camera
} // namespace SHO
