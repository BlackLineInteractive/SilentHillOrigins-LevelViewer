#include "SHO/Triggers/ZoneTrigger.h"
#include "SHO/Core/EventManager.h"
#include <iostream>

namespace SHO {
namespace Triggers {

ZoneTrigger::ZoneTrigger() {
    m_className = "ZoneTrigger";
}

bool ZoneTrigger::CheckTrigger(const Core::Vec3& actorPos, const Core::Vec3& /*prevActorPos*/) {
    if (!m_triggerEnabled || !m_active) return false;

    if (m_bounds.Contains(actorPos)) {
        if (!m_isLocked) {
            TriggerTransition();
            return true;
        }
    }
    return false;
}

void ZoneTrigger::TriggerTransition() {
    if (m_targetZone.empty()) return;

    // Send ZoneTransition message with target zone and target spawn name
    struct TransitionPayload {
        const char* targetZone;
        const char* targetSpawn;
    };

    TransitionPayload payload{ m_targetZone.c_str(), m_targetSpawnName.c_str() };
    Core::EventManager::GetInstance().SendMsg("ZoneTransition", this, &payload, sizeof(payload));
}

} // namespace Triggers
} // namespace SHO
