#include "SHO/Actor/PlayerBehaviour.h"
#include "SHO/Camera/CameraManager.h"
#include "SHO/Core/EventManager.h"
#include <algorithm>
#include <cmath>

namespace SHO {
namespace Actor {

PlayerBehaviour::PlayerBehaviour() {
    m_className = "CPlayerBehaviour";
    m_name = "Travis";
    m_health = 100.0f;
    m_maxHealth = 100.0f;
}

void PlayerBehaviour::OnInit() {
    ListenToEvent("CameraCut");
}

void PlayerBehaviour::OnEvent(const Core::Msg& msg) {
    if (msg.id.name == "CameraCut") {
        // When camera cut occurs during motion, latch current world move vector
        // so Travis continues in the same physical direction without spinning around!
        if (std::abs(m_stickX) > 0.15f || std::abs(m_stickY) > 0.15f) {
            m_isDirectionLatched = true;
        }
    }
}

void PlayerBehaviour::StartGrapple(Actor* enemy) {
    m_state = PlayerState::Grappled;
    m_grappler = enemy;
    m_grappleProgress = 0.0f;
    m_grappleTimer = 3.5f; // 3.5 seconds to escape
    Core::EventManager::GetInstance().SendMsg("QTEGrappleStarted", this);
}

bool PlayerBehaviour::UpdateGrappleQTE(float dt, bool buttonPressed) {
    if (m_state != PlayerState::Grappled) return false;

    if (buttonPressed) {
        m_grappleProgress += 0.20f; // Each mash adds 20%
    }

    // Natural decay over time
    m_grappleProgress = std::max(0.0f, m_grappleProgress - 0.15f * dt);
    m_grappleTimer -= dt;

    if (m_grappleProgress >= 1.0f) {
        BreakFreeFromGrapple();
        return true;
    }

    if (m_grappleTimer <= 0.0f) {
        // Failed struggle: take grapple bite damage
        TakeDamage(25.0f, GetForward() * -1.0f);
        m_state = PlayerState::Idle;
        m_grappler = nullptr;
        Core::EventManager::GetInstance().SendMsg("QTEGrappleFailed", this);
        return false;
    }

    return false;
}

void PlayerBehaviour::BreakFreeFromGrapple() {
    m_state = PlayerState::Idle;
    if (m_grappler) {
        // Push grappler back and stagger it
        m_grappler->TakeDamage(10.0f, GetForward());
        m_grappler = nullptr;
    }
    Core::EventManager::GetInstance().SendMsg("QTEGrappleEscaped", this);
}

void PlayerBehaviour::UpdateThreat(const std::vector<Actor*>& enemies) {
    float maxStatic = 0.0f;
    const float radioMaxRadius = 12.0f;
    Core::Vec3 myPos = GetPosition();

    for (auto* enemy : enemies) {
        if (!enemy || !enemy->IsAlive()) continue;

        float dist = glm::distance(myPos, enemy->GetPosition());
        if (dist < radioMaxRadius) {
            float s = 1.0f - (dist / radioMaxRadius);
            if (s > maxStatic) {
                maxStatic = s;
            }
        }
    }

    m_radioStatic = maxStatic;
}

void PlayerBehaviour::HandleMovement(float dt) {
    if (!IsAlive()) {
        m_state = PlayerState::Dead;
        return;
    }

    if (m_state == PlayerState::Grappled) {
        return;
    }

    // Stamina recovery when not running
    float stickMag = std::sqrt(m_stickX * m_stickX + m_stickY * m_stickY);
    if (stickMag < 0.15f) {
        m_isDirectionLatched = false;
        m_latchedMoveDir = Core::Vec3(0.0f);

        // Regenerate stamina faster while standing still
        m_stamina = std::min(m_maxStamina, m_stamina + 35.0f * dt);
        if (m_stamina > 25.0f) {
            m_isExhausted = false;
        }

        if (m_isExhausted) {
            m_state = PlayerState::Exhausted;
        } else {
            m_state = PlayerState::Idle;
        }
        return;
    }

    Core::Vec3 moveDir(0.0f);

    if (m_isDirectionLatched && glm::length(m_latchedMoveDir) > 0.1f) {
        moveDir = m_latchedMoveDir;
    } else {
        // Camera-relative input calculation
        auto& camMgr = Camera::CameraManager::GetInstance();
        auto* activeCam = camMgr.GetActiveCamera();

        Core::Vec3 camFwd(0, 0, 1);
        Core::Vec3 camRight(1, 0, 0);

        if (activeCam) {
            camFwd = activeCam->GetForward();
            camFwd.y = 0.0f;
            if (glm::length(camFwd) > 1e-4f) camFwd = glm::normalize(camFwd);

            camRight = activeCam->GetRight();
            camRight.y = 0.0f;
            if (glm::length(camRight) > 1e-4f) camRight = glm::normalize(camRight);
        }

        moveDir = camRight * m_stickX + camFwd * m_stickY;
        if (glm::length(moveDir) > 1e-4f) {
            moveDir = glm::normalize(moveDir);
        }
        m_latchedMoveDir = moveDir;
    }

    // Determine running vs walking based on Square button and stamina state
    bool runRequested = Core::HasFlag(m_buttons, Core::PadButton::Square) || stickMag > 0.85f;
    if (runRequested && !m_isExhausted && m_stamina > 5.0f) {
        m_state = PlayerState::Run;
        m_stamina = std::max(0.0f, m_stamina - 15.0f * dt);
        if (m_stamina <= 0.0f) {
            m_isExhausted = true;
        }
    } else {
        if (m_isExhausted) {
            m_state = PlayerState::Exhausted;
        } else {
            m_state = PlayerState::Walk;
        }
        m_stamina = std::min(m_maxStamina, m_stamina + 20.0f * dt);
        if (m_stamina > 25.0f) {
            m_isExhausted = false;
        }
    }

    float speed = m_walkSpeed;
    if (m_state == PlayerState::Run) {
        speed = m_runSpeed;
    } else if (m_state == PlayerState::Exhausted) {
        speed = m_walkSpeed * 0.6f; // Slow down during exhaustion
    }

    Core::Vec3 displacement = moveDir * (speed * dt);

    // Update facing orientation
    float targetYaw = glm::degrees(std::atan2(moveDir.x, moveDir.z));
    float yawDiff = targetYaw - m_yaw;
    while (yawDiff > 180.0f)  yawDiff -= 360.0f;
    while (yawDiff < -180.0f) yawDiff += 360.0f;

    float maxTurn = m_turnSpeed * dt;
    yawDiff = std::clamp(yawDiff, -maxTurn, maxTurn);
    SetYaw(m_yaw + yawDiff);

    // Apply movement with character controller collision
    Core::Vec3 currentPos = GetPosition();
    Core::Vec3 newPos = m_controller.Move(currentPos, displacement, dt);
    SetPosition(newPos);
}

void PlayerBehaviour::OnUpdate(float dt) {
    HandleMovement(dt);
}

} // namespace Actor
} // namespace SHO
