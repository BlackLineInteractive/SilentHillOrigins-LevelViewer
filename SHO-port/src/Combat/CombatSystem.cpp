#include "SHO/Combat/CombatSystem.h"
#include "SHO/Inventory/InventoryManager.h"
#include "SHO/Core/EventManager.h"
#include <algorithm>
#include <cmath>

namespace SHO {
namespace Combat {

CombatSystem& CombatSystem::GetInstance() {
    static CombatSystem instance;
    return instance;
}

Actor::Actor* CombatSystem::FindTargetInCone(
    const Core::Vec3& eyePos,
    const Core::Vec3& lookDir,
    float maxRange,
    float maxAngleDeg,
    const std::vector<Actor::Actor*>& targets) const {

    Actor::Actor* closestTarget = nullptr;
    float closestDist = maxRange;
    float cosThreshold = std::cos(glm::radians(maxAngleDeg));

    for (auto* target : targets) {
        if (!target || !target->IsAlive()) continue;

        Core::Vec3 targetCenter = target->GetPosition() + Core::Vec3(0, 1.2f, 0);
        Core::Vec3 toTarget = targetCenter - eyePos;
        float dist = glm::length(toTarget);

        if (dist > 0.001f && dist <= closestDist) {
            Core::Vec3 dir = toTarget / dist;
            float cosAngle = glm::dot(lookDir, dir);

            if (cosAngle >= cosThreshold) {
                closestDist = dist;
                closestTarget = target;
            }
        }

    }
    return closestTarget;
}

AttackResult CombatSystem::PerformAttack(
    Actor::Actor* attacker,
    Inventory::ItemDef* weapon,
    AttackType attackType,
    const std::vector<Actor::Actor*>& potentialTargets) {

    AttackResult result;
    if (!attacker || !attacker->IsAlive() || !weapon) return result;

    Core::Vec3 attackerPos = attacker->GetPosition() + Core::Vec3(0, 1.2f, 0);
    Core::Vec3 attackerFwd = attacker->GetForward();

    if (weapon->GetCategory() == Inventory::ItemCategory::WeaponRanged) {
        // Ranged Attack (Gunshot)
        if (weapon->GetAmmoInClip() <= 0) {
            // Empty click sound event
            Core::EventManager::GetInstance().SendMsg("WeaponDryFire", attacker);
            return result;
        }

        // Consume bullet
        weapon->SetAmmoInClip(weapon->GetAmmoInClip() - 1);
        Core::EventManager::GetInstance().SendMsg("WeaponFired", attacker, (void*)weapon->GetId().c_str());

        // Find target in narrow cone (15 deg)
        Actor::Actor* hitActor = FindTargetInCone(attackerPos, attackerFwd, weapon->GetRange(), 15.0f, potentialTargets);
        if (hitActor) {
            float dmg = weapon->GetDamage();
            hitActor->TakeDamage(dmg, attackerFwd);
            result.hit = true;
            result.hitActor = hitActor;
            result.damageDealt = dmg;
            result.enemyKilled = !hitActor->IsAlive();

            Core::EventManager::GetInstance().SendMsg("TargetHit", hitActor, &dmg, sizeof(dmg));
        }
        return result;
    }

    // Melee Attack (Pipe, Knife, TV, Fists)
    float attackRange = weapon->GetRange();
    float coneAngle = 45.0f;
    float dmgMultiplier = 1.0f;

    if (attackType == AttackType::HeavyMelee) {
        dmgMultiplier = 1.6f;
        attackRange *= 1.15f;
    } else if (attackType == AttackType::FinishingStomp) {
        dmgMultiplier = 3.0f;
        attackRange = 1.0f;
    }

    Core::EventManager::GetInstance().SendMsg("WeaponSwing", attacker, (void*)weapon->GetId().c_str());

    Actor::Actor* hitActor = FindTargetInCone(attackerPos, attackerFwd, attackRange, coneAngle, potentialTargets);
    if (hitActor) {
        float dmg = weapon->GetDamage() * dmgMultiplier;
        hitActor->TakeDamage(dmg, attackerFwd);
        result.hit = true;
        result.hitActor = hitActor;
        result.damageDealt = dmg;
        result.enemyKilled = !hitActor->IsAlive();

        // Degrade weapon durability on impact
        bool broke = weapon->DegradeDurability(1);
        if (broke) {
            result.weaponBroke = true;
            Core::EventManager::GetInstance().SendMsg("WeaponBroken", attacker, (void*)weapon->GetId().c_str());
            Inventory::InventoryManager::GetInstance().RemoveItem(weapon->GetId(), 1);
        }

        Core::EventManager::GetInstance().SendMsg("TargetHit", hitActor, &dmg, sizeof(dmg));
    }

    return result;
}

bool CombatSystem::ReloadWeapon(Inventory::ItemDef* weapon) {
    if (!weapon || weapon->GetCategory() != Inventory::ItemCategory::WeaponRanged) {
        return false;
    }

    int needed = weapon->GetClipCapacity() - weapon->GetAmmoInClip();
    if (needed <= 0) return false;

    auto& inv = Inventory::InventoryManager::GetInstance();
    auto* ammoItem = inv.FindItem(weapon->GetRequiredAmmoType());
    if (!ammoItem || ammoItem->GetCount() <= 0) {
        return false;
    }

    int toLoad = std::min(needed, ammoItem->GetCount());
    weapon->SetAmmoInClip(weapon->GetAmmoInClip() + toLoad);
    ammoItem->SetCount(ammoItem->GetCount() - toLoad);

    Core::EventManager::GetInstance().SendMsg("WeaponReloaded", weapon);
    return true;
}

} // namespace Combat
} // namespace SHO
