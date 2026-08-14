#include "SHO/Triggers/TriggerSystem.h"
#include <cmath>
#include <iostream>

namespace SHO {
namespace Triggers {

int TriggerManager::EvaluateCameraPlanes(const glm::vec3& playerPos, int currentCam) {
    int activeCam = currentCam;
    for (auto& pt : m_planeTriggers) {
        if (!pt.active) continue;
        float curDot = glm::dot(pt.normal, playerPos) - pt.d;
        if (!pt.initialized) {
            pt.lastDot = curDot;
            pt.initialized = true;
            continue;
        }

        // Check if player crossed the plane this frame
        if (pt.lastDot < 0.0f && curDot >= 0.0f) {
            if (pt.camFront >= 0) activeCam = pt.camFront;
        } else if (pt.lastDot > 0.0f && curDot <= 0.0f) {
            if (pt.camBack >= 0) activeCam = pt.camBack;
        }
        pt.lastDot = curDot;
    }
    return activeCam;
}

ZoneLinkTrigger* TriggerManager::CheckZonePrompt(const glm::vec3& playerPos) {
    ZoneLinkTrigger* best = nullptr;
    float bestDist = 1e9f;

    for (auto& zt : m_zoneTriggers) {
        if (!zt.active) continue;
        float d = glm::distance(playerPos, zt.position);
        if (d <= zt.interactionRadius && d < bestDist) {
            bestDist = d;
            best = &zt;
        }
    }
    return best;
}

InteractiveTrigger* TriggerManager::CheckInteractivePrompt(const glm::vec3& playerPos) {
    InteractiveTrigger* best = nullptr;
    float bestDist = 1e9f;

    for (auto& it : m_interactiveTriggers) {
        if (!it.active) continue;
        float d = glm::distance(playerPos, it.position);
        if (d <= it.radius && d < bestDist) {
            bestDist = d;
            best = &it;
        }
    }
    return best;
}

} // namespace Triggers
} // namespace SHO
