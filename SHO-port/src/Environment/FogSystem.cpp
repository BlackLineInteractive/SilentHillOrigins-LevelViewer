#include "SHO/Environment/FogSystem.h"
#include <algorithm>

namespace SHO {
namespace Environment {

void FogSystem::Init() {
    m_current.zoneName = "IntroRoad";
    m_current.fogStart = 2.0f;
    m_current.fogEnd = 24.0f;
    m_current.fogDensity = 0.05f;
    m_current.fogColor = glm::vec3(0.44f, 0.47f, 0.52f);
    m_current.fogMode = 0;
    m_current.fogTextureName = "FX_fog_ALPHA";
    m_target = m_current;
}

void FogSystem::SetCurrentZone(const std::string& zoneName) {
    m_target.zoneName = zoneName;
    if (zoneName.find("HO_") != std::string::npos || zoneName.find("Room") != std::string::npos || zoneName.find("Lobby") != std::string::npos) {
        // Dark hospital interior
        m_target.fogStart = 0.5f;
        m_target.fogEnd = 16.0f;
        m_target.fogDensity = 0.08f;
        m_target.fogColor = glm::vec3(0.08f, 0.09f, 0.10f); // dark murky interior
    } else {
        // Outdoor misty town
        m_target.fogStart = 2.0f;
        m_target.fogEnd = 24.0f;
        m_target.fogDensity = 0.05f;
        m_target.fogColor = glm::vec3(0.44f, 0.47f, 0.52f); // silvery mist
    }
    m_transitionProgress = 0.0f;
}

void FogSystem::Update(float dt) {
    if (m_transitionProgress < 1.0f) {
        m_transitionProgress += dt * 0.8f;
        if (m_transitionProgress > 1.0f) m_transitionProgress = 1.0f;

        float t = m_transitionProgress;
        m_current.fogStart = glm::mix(m_current.fogStart, m_target.fogStart, t);
        m_current.fogEnd = glm::mix(m_current.fogEnd, m_target.fogEnd, t);
        m_current.fogDensity = glm::mix(m_current.fogDensity, m_target.fogDensity, t);
        m_current.fogColor = glm::mix(m_current.fogColor, m_target.fogColor, t);
    }
}

} // namespace Environment
} // namespace SHO
