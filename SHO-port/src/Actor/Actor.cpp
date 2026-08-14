#include "SHO/Actor/Actor.h"
#include <algorithm>

namespace SHO {
namespace Actor {

Actor::Actor() {
    m_className = "CActor";
}

void Actor::TakeDamage(float amount, const Core::Vec3& /*hitDir*/) {
    if (!IsAlive()) return;
    m_health = std::max(0.0f, m_health - amount);
}

void Actor::Heal(float amount) {
    if (!IsAlive()) return;
    m_health = std::min(m_maxHealth, m_health + amount);
}

void Actor::SetYaw(float yawDeg) {
    m_yaw = yawDeg;
    Core::Vec3 pos = GetPosition();
    Core::Mat4 rot = glm::rotate(Core::Mat4(1.0f), glm::radians(yawDeg), Core::Vec3(0, 1, 0));
    rot[3] = Core::Vec4(pos, 1.0f);
    SetTransform(rot);
}

} // namespace Actor
} // namespace SHO
