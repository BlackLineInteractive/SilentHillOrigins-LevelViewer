#include "ClimaxEngine/Game/ButtonTriggers.h"
#include <cmath>
#include <iostream>

namespace ClimaxEngine {
namespace Game {

namespace {
bool InsideVolume(const glm::mat4 &m, const glm::vec3 &p, float reach) {
    const glm::vec3 rel = p - glm::vec3(m[3]);
    for (int k = 0; k < 3; ++k) {
        const glm::vec3 axis(m[k]);
        const float len = glm::length(axis);
        if (len < 1e-5f)
            continue; 
        if (std::abs(glm::dot(rel, axis / len)) > len + reach)
            return false;
    }
    return true;
}
} // namespace

std::vector<ButtonTrigger> BuildButtonTriggers(const std::vector<GameObject>& objects) {
    std::vector<ButtonTrigger> out;
    for (const GameObject& go : objects) {
        if (go.className != "ButtonBoxTrigger" && 
            go.className != "ButtonTrigger" && 
            go.className != "CDistortionTrigger") {
            continue;
        }
        
        ButtonTrigger btn;
        btn.className = go.className;
        btn.objName = go.objName;
        
        // Decompiled logic: property 0 and 1 are usually events or string IDs
        if (go.linkNames.size() > 0) {
            btn.eventName = go.linkNames[0];
        }
        if (go.linkNames.size() > 1) {
            btn.targetMap = go.linkNames[1];
        }

        btn.position = go.position;
        btn.transform = go.transform;
        
        // In the decompiled code, ButtonBoxTrigger has specific attributes:
        // switch (iVar2) case 4: *(uint*)(param_1 + 0xf0) = apiStack_80[0][2] != 0;
        // Since Loader.cpp generic parser doesn't capture integers, we rely on the
        // transform and the string links which ARE captured genericly.
        
        out.push_back(std::move(btn));
    }
    return out;
}

int ButtonTriggerAt(const std::vector<ButtonTrigger>& triggers, const glm::vec3& p, float reach) {
    int best = -1;
    float bestD = 1e9f;
    for (size_t i = 0; i < triggers.size(); ++i) {
        // Point intersection test
        if (!InsideVolume(triggers[i].transform, p, reach)) {
            continue;
        }
        const float d = glm::length(p - glm::vec3(triggers[i].transform[3]));
        if (d < bestD) {
            bestD = d;
            best = (int)i;
        }
    }
    return best;
}

} // namespace Game
} // namespace ClimaxEngine
