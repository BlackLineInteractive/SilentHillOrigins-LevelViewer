#include "SHO/Actor/Monster.h"
#include "SHO/Core/EventManager.h"
#include <algorithm>
#include <cmath>

namespace SHO {
namespace Actor {

Monster::Monster(MonsterType type) : m_type(type) {
    switch (type) {
    case MonsterType::Nurse:
        m_className = "CNurseBehaviour";
        m_health = 45.0f;
        m_maxHealth = 45.0f;
        SetAttackRadius(1.2f);
        break;
    case MonsterType::StraightJacket:
        m_className = "CStraightJacketBehaviour";
        m_health = 55.0f;
        m_maxHealth = 55.0f;
        SetAttackRadius(1.4f);
        break;
    case MonsterType::Butcher:
        m_className = "CButcherBehaviour";
        m_health = 250.0f;
        m_maxHealth = 250.0f;
        SetAttackRadius(2.2f);
        break;
    case MonsterType::Caliban:
        m_className = "CCalibanBehaviour";
        m_health = 400.0f;
        m_maxHealth = 400.0f;
        SetAttackRadius(3.0f);
        break;
    default:
        m_className = "CEnemyBehaviour";
        break;
    }
}

bool Monster::CanSeePlayer(const Core::Vec3& playerPos, bool playerFlashlightOn) const {
    Core::Vec3 eyePos = GetPosition() + Core::Vec3(0, 1.4f, 0);
    Core::Vec3 toPlayer = playerPos - eyePos;
    float dist = glm::length(toPlayer);

    float maxRange = playerFlashlightOn ? m_sightRangeLit : m_sightRangeDark;
    if (dist > maxRange || dist <= 1e-4f) return false;

    Core::Vec3 lookDir = GetForward();
    float cosAngle = glm::dot(lookDir, toPlayer / dist);
    float cosThreshold = std::cos(glm::radians(m_visionAngleDeg));

    return cosAngle >= cosThreshold;
}

bool Monster::CanHearPlayer(const Core::Vec3& playerPos, bool isRunning) const {
    float dist = glm::distance(GetPosition(), playerPos);
    float hearLimit = isRunning ? m_hearingRangeRun : m_hearingRangeWalk;
    return dist <= hearLimit;
}

void Monster::TakeDamage(float amount, const Core::Vec3& hitDir) {
    Actor::TakeDamage(amount, hitDir);

    if (!IsAlive()) {
        m_actionState = MonsterActionState::Dead;
        Core::EventManager::GetInstance().SendMsg("MonsterKilled", this);
        return;
    }

    // Heavy damage triggers knockdown (vulnerable to finish)
    if (amount >= 25.0f && m_actionState != MonsterActionState::KnockedDown) {
        m_actionState = MonsterActionState::KnockedDown;
        m_knockdownTimer = 4.0f; // 4 seconds on the floor
        Core::EventManager::GetInstance().SendMsg("MonsterKnockedDown", this);
    }
}

void Monster::AddQTEStruggle(float amount) {
    m_qteProgress += amount;
    if (m_qteProgress >= 1.0f) {
        BreakGrab();
    }
}

void Monster::BreakGrab() {
    m_actionState = MonsterActionState::Alerted;
    m_qteProgress = 0.0f;
    Core::EventManager::GetInstance().SendMsg("GrabEscaped", this);
}

void Monster::OnUpdate(float dt) {
    if (!IsAlive()) {
        m_actionState = MonsterActionState::Dead;
        return;
    }

    if (m_actionState == MonsterActionState::KnockedDown) {
        m_knockdownTimer -= dt;
        if (m_knockdownTimer <= 0.0f) {
            // Stand back up!
            m_actionState = MonsterActionState::Alerted;
            Core::EventManager::GetInstance().SendMsg("MonsterStoodUp", this);
        }
        return;
    }

    EnemyBehaviour::OnUpdate(dt);
}

} // namespace Actor
} // namespace SHO
