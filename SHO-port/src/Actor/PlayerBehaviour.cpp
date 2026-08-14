#include "SHO/Actor/PlayerBehaviour.h"
#include "SHO/Camera/CameraManager.h"
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

void PlayerBehaviour::HandleMovement(float dt) {
    if (!IsAlive()) {
        m_state = PlayerState::Dead;
        return;
    }

    float stickMag = std::sqrt(m_stickX * m_stickX + m_stickY * m_stickY);
    if (stickMag < 0.15f) {
        // Stick released: clear direction latch
        m_isDirectionLatched = false;
        m_latchedMoveDir = Core::Vec3(0.0f);
        m_state = PlayerState::Idle;
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

    // Determine running vs walking based on Square button or analog stick push
    bool isRunning = Core::HasFlag(m_buttons, Core::PadButton::Square) || stickMag > 0.85f;
    if (isRunning && m_stamina > 5.0f) {
        m_state = PlayerState::Run;
        m_stamina = std::max(0.0f, m_stamina - 15.0f * dt);
    } else {
        m_state = PlayerState::Walk;
        m_stamina = std::min(m_maxStamina, m_stamina + 20.0f * dt);
    }

    float speed = (m_state == PlayerState::Run) ? m_runSpeed : m_walkSpeed;
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
