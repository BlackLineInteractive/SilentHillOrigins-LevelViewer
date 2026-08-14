#include "SHO/Triggers/PlaneTrigger.h"
#include "SHO/Camera/CameraManager.h"
#include "SHO/Core/EventManager.h"

namespace SHO {
namespace Triggers {

PlaneTrigger::PlaneTrigger() {
    m_className = "PlaneTrigger";
}

bool PlaneTrigger::CheckTrigger(const Core::Vec3& actorPos, const Core::Vec3& prevActorPos) {
    if (!m_triggerEnabled || !m_active) return false;

    Core::Vec3 origin = GetPosition();
    Core::Vec3 right = GetRight();
    Core::Vec3 up = GetUp();

    // Check lateral bounds
    Core::Vec3 toActor = actorPos - origin;
    float latX = glm::dot(toActor, right);
    float latY = glm::dot(toActor, up);

    if (std::abs(latX) > m_halfWidth || std::abs(latY) > m_halfHeight) {
        return false;
    }

    // Check crossing plane
    float dCurr = glm::dot(actorPos - origin, m_normal);
    float dPrev = glm::dot(prevActorPos - origin, m_normal);

    if (dPrev < 0.0f && dCurr >= 0.0f) {
        // Crossed from negative to positive side -> switch to forward camera
        if (!m_forwardCamName.empty()) {
            Camera::CameraManager::GetInstance().SetActiveCamera(m_forwardCamName);
            Core::EventManager::GetInstance().SendMsg(m_forwardCamName, this);
            return true;
        }
    } else if (dPrev > 0.0f && dCurr <= 0.0f) {
        // Crossed from positive to negative side -> switch to backward camera
        if (!m_backwardCamName.empty()) {
            Camera::CameraManager::GetInstance().SetActiveCamera(m_backwardCamName);
            Core::EventManager::GetInstance().SendMsg(m_backwardCamName, this);
            return true;
        }
    }

    return false;
}

} // namespace Triggers
} // namespace SHO
