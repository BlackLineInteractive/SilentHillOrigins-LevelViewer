#pragma once

#include "SHO/Actor/Actor.h"
#include "SHO/Actor/CharacterController.h"

namespace SHO {
namespace Actor {

enum class EnemyState {
    Patrol,
    Alert,
    Chase,
    Attack,
    Hurt,
    Dead
};

// Translated from CEnemyBehaviour (base enemy AI logic)
class EnemyBehaviour : public Actor {
public:
    EnemyBehaviour();
    virtual ~EnemyBehaviour() = default;

    void OnUpdate(float dt) override;

    void SetTarget(Actor* target) { m_target = target; }
    void SetDetectionRadius(float r) { m_detectionRadius = r; }
    void SetAttackRadius(float r) { m_attackRadius = r; }

    EnemyState GetEnemyState() const { return m_state; }

private:
    EnemyState m_state = EnemyState::Patrol;
    Actor* m_target = nullptr;
    CharacterController m_controller;

    float m_detectionRadius = 8.0f;
    float m_attackRadius = 1.2f;
    float m_moveSpeed = 1.2f;
    float m_attackCooldown = 0.0f;
};

} // namespace Actor
} // namespace SHO
