#pragma once

#include "SHO/Actor/Actor.h"
#include "SHO/Actor/CharacterController.h"
#include <vector>

namespace SHO {
namespace Actor {

enum class PlayerState {
    Idle,
    Walk,
    Run,
    Exhausted,
    TurnInPlace,
    AimMelee,
    AimRanged,
    Attack,
    Grappled,
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
    bool  IsExhausted() const { return m_isExhausted; }

    // Threat / Pocket Radio Static (cThreatController)
    float GetRadioStatic() const { return m_radioStatic; }
    void UpdateThreat(const std::vector<Actor*>& enemies);

    // QTE Grapple Struggle (CFMAController)
    bool IsGrappled() const { return m_state == PlayerState::Grappled; }
    void StartGrapple(Actor* enemy);
    bool UpdateGrappleQTE(float dt, bool buttonPressed);
    void BreakFreeFromGrapple();
    float GetGrappleMeter() const { return m_grappleProgress; }

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
    bool  m_isExhausted = false;
    bool  m_flashlightOn = true;

    // Threat & radio
    float m_radioStatic = 0.0f;

    // Grapple QTE
    Actor* m_grappler = nullptr;
    float  m_grappleProgress = 0.0f;
    float  m_grappleTimer = 0.0f;

    // Direction latch: holds world move vector across camera cuts until stick is released
    bool       m_isDirectionLatched = false;
    Core::Vec3 m_latchedMoveDir = Core::Vec3(0.0f);
};

} // namespace Actor
} // namespace SHO
