#include "SHO/Environment/Light.h"
#include <algorithm>
#include <cmath>

namespace SHO {
namespace Environment {

Light::Light() {
    m_className = "CColorLight";
}

void Light::SetConeAngleDeg(float angleDeg) {
    m_coneAngleDeg = angleDeg;
    if (angleDeg > 0.0f && angleDeg < 180.0f) {
        m_type = LightType::Spot;
    } else {
        m_type = LightType::Point;
    }
}

Core::Vec3 Light::EvaluateLight(const Core::Vec3& worldPos, const Core::Vec3& normal) const {
    if (!m_active || !m_visible || m_range <= 0.0f) {
        return Core::Vec3(0.0f);
    }

    Core::Vec3 lightPos = GetPosition();
    Core::Vec3 toLight = lightPos - worldPos;
    float dist = glm::length(toLight);

    if (dist >= m_range || dist <= 1e-4f) {
        return Core::Vec3(0.0f);
    }

    Core::Vec3 L = toLight / dist;

    // Distance attenuation: smooth quadratic falloff
    float atten = std::clamp(1.0f - (dist / m_range), 0.0f, 1.0f);
    atten = atten * atten;

    // Spot light cone attenuation
    if (m_type == LightType::Spot) {
        Core::Vec3 spotDir = GetForward();
        float cosAngle = glm::dot(-L, spotDir);
        float outerCos = std::cos(glm::radians(m_coneAngleDeg * 0.5f));
        float innerCos = std::cos(glm::radians(m_innerConeAngleDeg * 0.5f));

        if (cosAngle < outerCos) {
            return Core::Vec3(0.0f);
        }

        if (innerCos > outerCos) {
            float spotAtten = std::clamp((cosAngle - outerCos) / (innerCos - outerCos), 0.0f, 1.0f);
            atten *= spotAtten;
        }
    }

    // Lambertian diffuse
    float NdotL = std::max(glm::dot(normal, L), 0.0f);
    return m_color * (m_intensity * atten * NdotL);
}

} // namespace Environment
} // namespace SHO
