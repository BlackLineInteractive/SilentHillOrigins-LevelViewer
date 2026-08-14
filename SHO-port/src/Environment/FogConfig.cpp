#include "SHO/Environment/FogConfig.h"
#include <algorithm>
#include <cmath>

namespace SHO {
namespace Environment {

FogConfig::FogConfig() {
    m_className = "CFogConfig";
    // Native constructor values from FUN_0015F148
    m_start   = 13.0f;
    m_end     = 25.0f;
    m_density = 0.3f;
    m_color   = Core::Vec3(0.65f, 1.0f, 0.70f);
    m_mode    = 1500;
    m_enabled = true;
}

float FogConfig::CalculateFogFactor(float distance) const {
    if (!m_enabled || distance <= m_start) return 0.0f;
    if (distance >= m_end) return 1.0f;

    float span = m_end - m_start;
    if (span <= 1e-4f) return 1.0f;

    // Linear fog ramp modulated by density
    float f = (distance - m_start) / span;
    return std::clamp(f * m_density, 0.0f, 1.0f);
}

} // namespace Environment
} // namespace SHO
