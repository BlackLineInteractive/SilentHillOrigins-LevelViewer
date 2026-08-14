#pragma once

#include "SHO/Core/Behaviour.h"

namespace SHO {
namespace Environment {

// Translated from CBloomCfg (bloom post-processing configuration)
class BloomConfig : public Core::Behaviour {
public:
    BloomConfig() {
        m_className = "CBloomCfg";
    }
    virtual ~BloomConfig() = default;

    float GetThreshold() const { return m_threshold; }
    void SetThreshold(float t) { m_threshold = t; }

    float GetIntensity() const { return m_intensity; }
    void SetIntensity(float i) { m_intensity = i; }

    float GetRadius() const { return m_radius; }
    void SetRadius(float r) { m_radius = r; }

    bool IsBloomEnabled() const { return m_enabled; }
    void SetBloomEnabled(bool en) { m_enabled = en; }

private:
    float m_threshold = 0.8f;
    float m_intensity = 0.5f;
    float m_radius = 1.0f;
    bool  m_enabled = true;
};

} // namespace Environment
} // namespace SHO
