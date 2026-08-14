#pragma once

#include "SHO/Environment/Light.h"

namespace SHO {
namespace Actor {

// Translated from CPlayerLight (Travis's pocket flashlight)
class PlayerLight : public Environment::Light {
public:
    PlayerLight();
    virtual ~PlayerLight() = default;

    void SetFlashlightOn(bool on) {
        m_isOn = on;
        SetActive(on);
        SetVisible(on);
    }
    bool IsFlashlightOn() const { return m_isOn; }
    void Toggle() { SetFlashlightOn(!m_isOn); }

    // Update light position & direction to follow Travis's chest
    void UpdateFollow(const Core::Vec3& playerPos, const Core::Vec3& playerFwd, float dt);

private:
    bool  m_isOn = true;
    float m_flickerTimer = 0.0f;
};

} // namespace Actor
} // namespace SHO
