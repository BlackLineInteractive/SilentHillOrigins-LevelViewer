#include "SHO/Audio/AudioEngine.h"
#include <iostream>
#include <cstdlib>

namespace SHO {
namespace Audio {

void AudioEngine::Init() {
    if (m_initialized) return;

    auto* arc = ClimaxEngine::RWS::FileSystem::CArchiveManager::GetInstance().GetFirstArchive();
    if (!arc) return;

    // 1. Load Audiotest for authentic multi-surface footstep sound bank
    int testIdx = arc->Find("Audiotest");
    if (testIdx >= 0) {
        std::vector<uint8_t> data;
        if (arc->Read(testIdx, data)) {
            for (size_t i = 0; i + 16 <= data.size(); ++i) {
                if (data[i] == 0x09 && data[i+1] == 0x08 && data[i+2] == 0x00 && data[i+3] == 0x00) {
                    std::vector<AudioClip> allSteps;
                    ::Audio::ParseWaveDictionary(&data[i], data.size() - i, allSteps);
                    for (const auto& s : allSteps) {
                        if (s.name.find("road") != std::string::npos)       m_roadSteps.push_back(s);
                        else if (s.name.find("dirt") != std::string::npos)  m_dirtSteps.push_back(s);
                        else if (s.name.find("tile") != std::string::npos)  m_tileSteps.push_back(s);
                        else if (s.name.find("wood") != std::string::npos)  m_woodSteps.push_back(s);
                        else if (s.name.find("metal") != std::string::npos) m_metalSteps.push_back(s);
                    }
                    std::cout << "[AUDIO-ENGINE] Initialized authentic surface audio banks: " 
                              << m_roadSteps.size() << " road, " 
                              << m_tileSteps.size() << " tile, " 
                              << m_woodSteps.size() << " wood samples." << std::endl;
                    break;
                }
            }
        }
    }

    // 2. Load AudioPlayer for player sfx (flashlight click, radio, impact)
    int playerIdx = arc->Find("AudioPlayer");
    if (playerIdx >= 0) {
        std::vector<uint8_t> data;
        if (arc->Read(playerIdx, data)) {
            for (size_t i = 0; i + 16 <= data.size(); ++i) {
                if (data[i] == 0x09 && data[i+1] == 0x08 && data[i+2] == 0x00 && data[i+3] == 0x00) {
                    ::Audio::ParseWaveDictionary(&data[i], data.size() - i, m_playerSfx);

                    std::cout << "[AUDIO-ENGINE] Loaded " << m_playerSfx.size() << " player sfx samples." << std::endl;
                    break;
                }
            }
        }
    }

    m_initialized = true;
}

void AudioEngine::PlayFootstep(SurfaceMaterial mat) {
    const std::vector<AudioClip>* pool = &m_roadSteps;
    switch (mat) {
        case SurfaceMaterial::Road:   pool = &m_roadSteps; break;
        case SurfaceMaterial::Dirt:   pool = &m_dirtSteps; break;
        case SurfaceMaterial::Tile:   pool = &m_tileSteps; break;
        case SurfaceMaterial::Wood:   pool = &m_woodSteps; break;
        case SurfaceMaterial::Metal:  pool = &m_metalSteps; break;
        default:                      pool = &m_roadSteps; break;
    }

    if (!pool->empty()) {
        int pick = rand() % pool->size();
        ClimaxEngine::Viewer::PlayAudioClip((*pool)[pick]);
    }
}

void AudioEngine::PlayFlashlightClick() {
    for (const auto& s : m_playerSfx) {
        if (s.name.find("flashlight") != std::string::npos || s.name.find("click") != std::string::npos) {
            ClimaxEngine::Viewer::PlayAudioClip(s);
            return;
        }
    }
}

void AudioEngine::PlayDoorSound(bool open, bool locked) {
    for (const auto& s : m_levelSounds) {
        if (locked && s.name.find("Lock") != std::string::npos) {
            ClimaxEngine::Viewer::PlayAudioClip(s);
            return;
        }
        if (open && (s.name.find("Door") != std::string::npos || s.name.find("Open") != std::string::npos)) {
            ClimaxEngine::Viewer::PlayAudioClip(s);
            return;
        }
    }
}

void AudioEngine::PlayPositional3D(const std::string& sampleName, const glm::vec3& soundPos, const glm::vec3& listenerPos, float maxDist) {
    float dist = glm::distance(soundPos, listenerPos);
    if (dist > maxDist) return;

    for (const auto& s : m_levelSounds) {
        if (s.name == sampleName) {
            ClimaxEngine::Viewer::PlayAudioClip(s);
            return;
        }
    }
}

void AudioEngine::Update(float dt) {
    (void)dt;
}

} // namespace Audio
} // namespace SHO
