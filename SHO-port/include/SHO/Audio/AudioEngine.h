#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <glm/glm.hpp>
#include "ClimaxEngine/Platform/PS2/AudioParser.h"
#include "ClimaxEngine/Core/RWS/FileSystem/CArchiveManager.h"
#include "ClimaxEngine/Viewer/ViewerAudio.h"

namespace SHO {
namespace Audio {

enum class SurfaceMaterial {
    Road,
    Dirt,
    Tile,
    Wood,
    Metal,
    Water,
    Carpet
};

class AudioEngine {
public:
    static AudioEngine& GetInstance() {
        static AudioEngine instance;
        return instance;
    }

    void Init();
    void LoadLevelSoundBank(const std::string& levelName);
    void PlayFootstep(SurfaceMaterial mat);
    void PlayFlashlightClick();
    void PlayDoorSound(bool open, bool locked);
    void PlayRadioStatic(float intensity);
    void PlayPositional3D(const std::string& sampleName, const glm::vec3& soundPos, const glm::vec3& listenerPos, float maxDist = 15.0f);
    void Update(float dt);

private:
    AudioEngine() = default;
    
    std::vector<AudioClip> m_roadSteps;
    std::vector<AudioClip> m_dirtSteps;
    std::vector<AudioClip> m_tileSteps;
    std::vector<AudioClip> m_woodSteps;
    std::vector<AudioClip> m_metalSteps;
    std::vector<AudioClip> m_playerSfx;
    std::vector<AudioClip> m_levelSounds;
    bool                   m_initialized = false;
};

} // namespace Audio
} // namespace SHO
