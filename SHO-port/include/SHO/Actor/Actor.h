#pragma once

#include "SHO/Core/Behaviour.h"

namespace SHO {
namespace Actor {

class Actor : public Core::Behaviour {
public:
    Actor();
    virtual ~Actor() = default;

    float GetHealth() const { return m_health; }
    void SetHealth(float hp) { m_health = hp; }

    float GetMaxHealth() const { return m_maxHealth; }
    void SetMaxHealth(float maxHp) { m_maxHealth = maxHp; }

    bool IsAlive() const { return m_health > 0.0f; }

    virtual void TakeDamage(float amount, const Core::Vec3& hitDir);
    virtual void Heal(float amount);

    float GetYaw() const { return m_yaw; }
    void SetYaw(float yawDeg);

protected:
    float m_health = 100.0f;
    float m_maxHealth = 100.0f;
    float m_yaw = 0.0f;
};

} // namespace Actor
} // namespace SHO
