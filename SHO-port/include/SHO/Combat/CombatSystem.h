#pragma once

#include "SHO/Core/Types.h"
#include "SHO/Inventory/ItemDef.h"
#include "SHO/Actor/Actor.h"
#include <vector>

namespace SHO {
namespace Combat {

enum class AttackType {
    LightMelee,
    HeavyMelee,
    RangedFire,
    FinishingStomp
};

struct AttackResult {
    bool hit = false;
    Actor::Actor* hitActor = nullptr;
    float damageDealt = 0.0f;
    bool weaponBroke = false;
    bool enemyKilled = false;
};

// Combat System managing attacks, aiming mode, hit registration, durability loss
class CombatSystem {
public:
    static CombatSystem& GetInstance();

    // Perform an attack from attacker against all registered enemies
    AttackResult PerformAttack(
        Actor::Actor* attacker,
        Inventory::ItemDef* weapon,
        AttackType attackType,
        const std::vector<Actor::Actor*>& potentialTargets
    );

    // Attempt to reload current ranged weapon
    bool ReloadWeapon(Inventory::ItemDef* weapon);

    // Check if an enemy is in range and facing direction
    Actor::Actor* FindTargetInCone(
        const Core::Vec3& eyePos,
        const Core::Vec3& lookDir,
        float maxRange,
        float maxAngleDeg,
        const std::vector<Actor::Actor*>& targets
    ) const;

private:
    CombatSystem() = default;
    ~CombatSystem() = default;
};

} // namespace Combat
} // namespace SHO
