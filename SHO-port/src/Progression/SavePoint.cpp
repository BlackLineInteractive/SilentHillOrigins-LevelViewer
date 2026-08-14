#include "SHO/Progression/SavePoint.h"
#include "SHO/Progression/PlayerData.h"
#include "SHO/Core/EventManager.h"
#include <iostream>

namespace SHO {
namespace Progression {

SavePoint::SavePoint() {
    m_className = "SavePoint";
}

bool SavePoint::CheckTrigger(const Core::Vec3& actorPos, const Core::Vec3& /*prevActorPos*/) {
    if (!m_triggerEnabled || !m_active) return false;

    float dist = glm::distance(actorPos, GetPosition());
    bool inRange = dist <= m_radius;

    if (inRange && !m_playerInRange) {
        m_playerInRange = true;
        Core::EventManager::GetInstance().SendMsg("ShowSavePrompt", this);
    } else if (!inRange && m_playerInRange) {
        m_playerInRange = false;
        Core::EventManager::GetInstance().SendMsg("HideSavePrompt", this);
    }

    return inRange;
}

void SavePoint::TriggerSave() {
    if (!m_playerInRange) return;

    PlayerData::GetInstance().GetStats().saveCount++;
    Core::EventManager::GetInstance().SendMsg("GameSaved", this);
}

} // namespace Progression
} // namespace SHO
