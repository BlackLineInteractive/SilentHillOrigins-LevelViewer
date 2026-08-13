#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "ClimaxEngine/Core/Types.h"

namespace ClimaxEngine {
namespace Game {

struct ButtonTrigger {
    std::string className; // "ButtonBoxTrigger", "ButtonTrigger", etc.
    std::string objName;   // Name of this trigger
    std::string eventName; // What it triggers (usually linkNames[1] or similar)
    std::string targetMap; // If it's a teleport, where it goes
    
    glm::vec3 position = glm::vec3(0.0f);
    glm::mat4 transform = glm::mat4(1.0f);
    
    // Additional parameters from decompiled logic
    int typeFlags = 0; // Extracted from property index 3 or similar
    bool flag4 = false; // Extracted from property index 4
};

// Build a list of button triggers from the raw GameObject list.
std::vector<ButtonTrigger> BuildButtonTriggers(const std::vector<GameObject>& objects);

// Find the closest trigger that the player is inside (if any).
int ButtonTriggerAt(const std::vector<ButtonTrigger>& triggers, const glm::vec3& p, float reach = 0.5f);

} // namespace Game
} // namespace ClimaxEngine
