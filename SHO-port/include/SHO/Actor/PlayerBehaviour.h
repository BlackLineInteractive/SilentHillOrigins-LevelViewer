#pragma once

#include "SHO/Actor/Actor.h"
#include "SHO/Actor/CharacterController.h"

namespace SHO {
namespace Actor {

enum class PlayerState {
    Idle,
    Walk,
    Run,
    TurnInPlace,
    Aim,
    Attack,
    Interact,
    Hurt,
    Dead
};

// Translated from CPlayerBehaviour (Travis Grady entity and player controller)
class PlayerBehaviour : public Actor {
public:
    PlayerBehaviour();
    virtual ~PlayerBehaviour() = default;

    void OnInit() override;
    void OnUpdate(float dt) override;
    void OnEvent(const Core::Msg& msg) override;

    // Pad inputs (bitmask of Core::PadButton)
    void SetInputMask(Core::PadButton buttons) { m_buttons = buttons; }
    void SetAnalogStick(float stickX, float stickY) { m_stickX = stickX; m_stickY = stickY; }

    // Movement parameters
    float GetWalkSpeed() const { return m_walkSpeed; }
    float GetRunSpeed() const { return m_runSpeed; }
    PlayerState GetPlayerState() const { return m_state; }

    // Flashlight & items
    bool IsFlashlightOn() const { return m_flashlightOn; }
    void ToggleFlashlight() { m_flashlightOn = !m_flashlightOn; }

    float GetStamina() const { return m_stamina; }
    float GetMaxStamina() const { return m_maxStamina; }

    CharacterController& GetController() { return m_controller; }

private:
    void HandleMovement(float dt);
    void UpdateState();

    PlayerState m_state = PlayerState::Idle;
    CharacterController m_controller;

    Core::PadButton m_buttons = Core::PadButton::None;
    float m_stickX = 0.0f;
    float m_stickY = 0.0f;

    float m_walkSpeed = 1.6f;
    float m_runSpeed = 4.2f;
    float m_turnSpeed = 180.0f;

    float m_stamina = 100.0f;
    float m_maxStamina = 100.0f;
    bool  m_flashlightOn = true;

    // Direction latch: holds world move vector across camera cuts until stick is released
    bool       m_isDirectionLatched = false;
    Core::Vec3 m_latchedMoveDir = Core::Vec3(0.0f);
};

} // namespace Actor
} // namespace SHO
