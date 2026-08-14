#pragma once

#include "SHO/Core/Behaviour.h"

namespace SHO {
namespace Environment {

enum class LightType {
    Point,
    Spot,
    Directional
};

// Translated from CColorLight and CBaseLight
class Light : public Core::Behaviour {
public:
    Light();
    virtual ~Light() = default;

    LightType GetLightType() const { return m_type; }
    void SetLightType(LightType t) { m_type = t; }

    const Core::Vec3& GetColor() const { return m_color; }
    void SetColor(const Core::Vec3& col) { m_color = col; }

    float GetIntensity() const { return m_intensity; }
    void SetIntensity(float intensity) { m_intensity = intensity; }

    float GetRange() const { return m_range; }
    void SetRange(float range) { m_range = range; }

    // Cone angles for Spotlights
    float GetConeAngleDeg() const { return m_coneAngleDeg; }
    void SetConeAngleDeg(float angleDeg);

    float GetInnerConeAngleDeg() const { return m_innerConeAngleDeg; }
    void SetInnerConeAngleDeg(float angleDeg) { m_innerConeAngleDeg = angleDeg; }

    // Evaluates light illumination at target world position
    Core::Vec3 EvaluateLight(const Core::Vec3& worldPos, const Core::Vec3& normal) const;

private:
    LightType  m_type = LightType::Point;
    Core::Vec3 m_color = Core::Vec3(1.0f);
    float      m_intensity = 1.0f;
    float      m_range = 10.0f;
    float      m_coneAngleDeg = 180.0f;
    float      m_innerConeAngleDeg = 0.0f;
};

} // namespace Environment
} // namespace SHO
