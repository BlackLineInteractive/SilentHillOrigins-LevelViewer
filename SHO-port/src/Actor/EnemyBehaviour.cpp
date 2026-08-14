#include "SHO/Actor/EnemyBehaviour.h"
#include <algorithm>
#include <cmath>

namespace SHO {
namespace Actor {

EnemyBehaviour::EnemyBehaviour() {
    m_className = "CEnemyBehaviour";
    m_health = 60.0f;
    m_maxHealth = 60.0f;
}

void EnemyBehaviour::OnUpdate(float dt) {
    if (!IsAlive()) {
        m_state = EnemyState::Dead;
        return;
    }

    if (m_attackCooldown > 0.0f) {
        m_attackCooldown -= dt;
    }

    if (!m_target || !m_target->IsAlive()) {
        m_state = EnemyState::Patrol;
        return;
    }

    Core::Vec3 myPos = GetPosition();
    Core::Vec3 targetPos = m_target->GetPosition();
    Core::Vec3 toTarget = targetPos - myPos;
    float dist = glm::length(toTarget);

    if (dist <= m_attackRadius) {
        m_state = EnemyState::Attack;
        if (m_attackCooldown <= 0.0f) {
            m_target->TakeDamage(15.0f, glm::normalize(toTarget));
            m_attackCooldown = 1.5f;
        }
    } else if (dist <= m_detectionRadius) {
        m_state = EnemyState::Chase;
        Core::Vec3 dir = glm::normalize(toTarget);

        float targetYaw = glm::degrees(std::atan2(dir.x, dir.z));
        SetYaw(targetYaw);

        Core::Vec3 displacement = dir * (m_moveSpeed * dt);
        Core::Vec3 newPos = m_controller.Move(myPos, displacement, dt);
        SetPosition(newPos);
    } else {
        m_state = EnemyState::Patrol;
    }
}

} // namespace Actor
} // namespace SHO
