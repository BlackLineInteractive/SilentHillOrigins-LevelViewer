#pragma once

#include "SHO/Core/Behaviour.h"

namespace SHO {
namespace Environment {

// Translated from CFogConfig (native PS2 constructor FUN_0015F148)
class FogConfig : public Core::Behaviour {
public:
    FogConfig();
    virtual ~FogConfig() = default;

    // Fog distances and parameters from property table & constructor
    float GetStartDistance() const { return m_start; }
    void SetStartDistance(float s) { m_start = s; }

    float GetEndDistance() const { return m_end; }
    void SetEndDistance(float e) { m_end = e; }

    float GetDensity() const { return m_density; }
    void SetDensity(float d) { m_density = d; }

    const Core::Vec3& GetColor() const { return m_color; }
    void SetColor(const Core::Vec3& col) { m_color = col; }

    int GetMode() const { return m_mode; }
    void SetMode(int m) { m_mode = m; }

    bool IsFogEnabled() const { return m_enabled; }
    void SetFogEnabled(bool en) { m_enabled = en; }

    // Evaluates fog factor at a given view distance
    float CalculateFogFactor(float distance) const;

private:
    float      m_start = 13.0f;          // prop 2 -> +0x58 (FUN_0015F148 default)
    float      m_end = 25.0f;            // prop 3 -> +0x60 (FUN_0015F148 default)
    float      m_density = 0.3f;         // prop 5 -> +0x64 (FUN_0015F148 default)
    Core::Vec3 m_color = Core::Vec3(0.65f, 1.0f, 0.70f); // props 10,11,8 -> +0x9C, +0xA0, +0xA4
    int        m_mode = 1500;            // prop 1 -> +0x50
    bool       m_enabled = true;
};

} // namespace Environment
} // namespace SHO
