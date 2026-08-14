#pragma once

#include "ClimaxEngine/Core/Types.h"
#include "SHO/World/World.h"
#include <vector>
#include <string>

namespace SHO {
namespace Bridge {

// Bridge that converts parsed level data from ClimaxEngine into SHO-port runtime entities
class LevelBridge {
public:
    // Builds and populates SHO::World::World with cameras, triggers, lights, fog, collision, and actors
    static bool PopulateWorldFromLevel(
        const std::vector<GameObject>& objects,
        const std::vector<LevelCamera>& cameras,
        const CollisionMesh& collision,
        float fogStart = 13.0f,
        float fogEnd = 25.0f,
        float fogDensity = 0.3f,
        const glm::vec3& fogColor = glm::vec3(0.65f, 1.0f, 0.70f),
        const std::string& preferredSpawn = ""
    );

    // Clears and resets the active SHO world
    static void ResetWorld();
};

} // namespace Bridge
} // namespace SHO
