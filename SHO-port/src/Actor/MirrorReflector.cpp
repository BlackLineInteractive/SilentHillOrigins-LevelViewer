#include "SHO/Actor/MirrorReflector.h"
#include "SHO/World/SceneQueue.h"
#include "SHO/Core/EventManager.h"
#include <iostream>

namespace SHO {
namespace Actor {

MirrorReflector::MirrorReflector() {
    m_className = "CPlayerReflector";
}

bool MirrorReflector::CheckTrigger(const Core::Vec3& actorPos, const Core::Vec3& /*prevActorPos*/) {
    if (!m_triggerEnabled || !m_active) return false;

    Core::Vec3 mirrorPos = GetPosition();
    Core::Vec3 toActor = actorPos - mirrorPos;
    float dist = glm::length(toActor);

    // Player must be in front of the mirror within interaction range
    bool inRange = dist <= m_interactionDist && glm::dot(toActor, m_mirrorNormal) > 0.0f;

    if (inRange && !m_playerCanTouch) {
        m_playerCanTouch = true;
        Core::EventManager::GetInstance().SendMsg("ShowTouchMirrorPrompt", this);
    } else if (!inRange && m_playerCanTouch) {
        m_playerCanTouch = false;
        Core::EventManager::GetInstance().SendMsg("HideTouchMirrorPrompt", this);
    }

    return inRange;
}

void MirrorReflector::TouchMirror() {
    if (!m_playerCanTouch) return;

    // Trigger Otherworld transition distortion and scene load
    Core::EventManager::GetInstance().SendMsg("StartMirrorTransition", this);

    if (!m_otherworldZone.empty()) {
        World::SceneQueue::GetInstance().QueueLevelTransition(m_otherworldZone, "", 0.8f);
    }
}

} // namespace Actor
} // namespace SHO
