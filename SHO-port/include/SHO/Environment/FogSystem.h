#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace SHO {
namespace Environment {

struct FogZoneSettings {
    std::string zoneName;
    float       fogStart = 2.0f;
    float       fogEnd = 24.0f;
    float       fogDensity = 0.05f;
    glm::vec3   fogColor = glm::vec3(0.44f, 0.47f, 0.52f);
    int         fogMode = 0; // 0 = linear, 1 = exp, 2 = exp2
    std::string fogTextureName = "FX_fog_ALPHA";
};

class FogSystem {
public:
    static FogSystem& GetInstance() {
        static FogSystem instance;
        return instance;
    }

    void Init();
    void SetCurrentZone(const std::string& zoneName);
    void Update(float dt);

    const FogZoneSettings& GetCurrentSettings() const { return m_current; }

private:
    FogSystem() = default;
    FogZoneSettings m_current;
    FogZoneSettings m_target;
    float           m_transitionProgress = 1.0f;
};

} // namespace Environment
} // namespace SHO
