#pragma once

#include "SHO/Actor/EnemyBehaviour.h"

namespace SHO {
namespace Actor {

enum class MonsterType {
    Nurse,
    StraightJacket,
    Butcher,
    Caliban,
    Ariel,
    SadDaddy
};

enum class MonsterActionState {
    Patrol,
    Alerted,    // Heard footstep or saw flashlight
    Chase,
    AttackMelee,
    GrabPlayer, // QTE Struggle state
    KnockedDown,// On floor, vulnerable to finishing stomp
    Dead
};

// Specialized Monster AI with sensory detection and combat states
class Monster : public EnemyBehaviour {
public:
    Monster(MonsterType type = MonsterType::Nurse);
    virtual ~Monster() = default;

    MonsterType GetMonsterType() const { return m_type; }
    MonsterActionState GetActionState() const { return m_actionState; }

    void OnUpdate(float dt) override;
    void TakeDamage(float amount, const Core::Vec3& hitDir) override;

    // Senses
    bool CanSeePlayer(const Core::Vec3& playerPos, bool playerFlashlightOn) const;
    bool CanHearPlayer(const Core::Vec3& playerPos, bool isRunning) const;

    // Grab & QTE mechanics
    bool IsGrabbing() const { return m_actionState == MonsterActionState::GrabPlayer; }
    void BreakGrab();
    float GetGrabStruggleProgress() const { return m_qteProgress; }
    void AddQTEStruggle(float amount);

    bool IsKnockedDown() const { return m_actionState == MonsterActionState::KnockedDown; }

private:
    MonsterType        m_type = MonsterType::Nurse;
    MonsterActionState m_actionState = MonsterActionState::Patrol;

    float m_visionAngleDeg = 65.0f;
    float m_sightRangeDark = 3.5f;
    float m_sightRangeLit = 12.0f;
    float m_hearingRangeRun = 8.0f;
    float m_hearingRangeWalk = 2.0f;

    float m_knockdownTimer = 0.0f;
    float m_qteProgress = 0.0f;
    float m_patrolTimer = 0.0f;
};

} // namespace Actor
} // namespace SHO
