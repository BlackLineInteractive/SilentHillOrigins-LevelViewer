#include "SHO/Triggers/ButtonTrigger.h"
#include "SHO/Core/EventManager.h"

namespace SHO {
namespace Triggers {

ButtonTrigger::ButtonTrigger() {
    m_className = "ButtonTrigger";
}

bool ButtonTrigger::CheckTrigger(const Core::Vec3& actorPos, const Core::Vec3& /*prevActorPos*/) {
    if (!m_triggerEnabled || !m_active) return false;

    bool inRange = false;
    if (m_useBox) {
        inRange = m_boxBounds.Contains(actorPos);
    } else {
        float dist = glm::distance(actorPos, GetPosition());
        inRange = dist <= m_radius;
    }

    if (inRange && !m_playerInRange) {
        m_playerInRange = true;
        // Prompt available: notify UI
        Core::EventManager::GetInstance().SendMsg("ShowInteractPrompt", this, (void*)m_promptStringId.c_str());
    } else if (!inRange && m_playerInRange) {
        m_playerInRange = false;
        Core::EventManager::GetInstance().SendMsg("HideInteractPrompt", this);
    }

    return inRange;
}

void ButtonTrigger::Interact() {
    if (!m_playerInRange || !m_active) return;

    if (!m_actionEvent.empty()) {
        Core::EventManager::GetInstance().SendMsg(m_actionEvent, this);
    }
}

} // namespace Triggers
} // namespace SHO
