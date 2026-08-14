#include "SHO/Actor/PlayerLight.h"

namespace SHO {
namespace Actor {

PlayerLight::PlayerLight() {
    m_className = "CPlayerLight";
    SetLightType(Environment::LightType::Spot);
    SetColor(Core::Vec3(1.0f, 0.95f, 0.85f)); // Warm tungsten flashlight beam
    SetIntensity(2.5f);
    SetRange(14.0f);
    SetConeAngleDeg(55.0f);
    SetInnerConeAngleDeg(25.0f);
}

void PlayerLight::UpdateFollow(const Core::Vec3& playerPos, const Core::Vec3& playerFwd, float /*dt*/) {
    if (!m_isOn) return;

    // Position light slightly in front of chest (Y=1.2m)
    Core::Vec3 chestPos = playerPos + Core::Vec3(0, 1.2f, 0) + playerFwd * 0.15f;
    SetPosition(chestPos);

    // Orient forward towards player look direction with slight downward tilt
    Core::Vec3 lightDir = glm::normalize(playerFwd + Core::Vec3(0, -0.05f, 0));
    Core::Vec3 right = glm::normalize(glm::cross(Core::Vec3(0, 1, 0), lightDir));
    Core::Vec3 up = glm::normalize(glm::cross(lightDir, right));

    Core::Mat4 rot(1.0f);
    rot[0] = Core::Vec4(right, 0.0f);
    rot[1] = Core::Vec4(up, 0.0f);
    rot[2] = Core::Vec4(lightDir, 0.0f);
    rot[3] = Core::Vec4(chestPos, 1.0f);
    SetTransform(rot);
}

} // namespace Actor
} // namespace SHO
