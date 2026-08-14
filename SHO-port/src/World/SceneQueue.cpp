#include "SHO/World/SceneQueue.h"
#include "SHO/Core/EventManager.h"
#include <algorithm>
#include <iostream>

namespace SHO {
namespace World {

SceneQueue& SceneQueue::GetInstance() {
    static SceneQueue instance;
    return instance;
}

void SceneQueue::QueueLevelTransition(const std::string& containerName, const std::string& spawnName, float fadeDuration) {
    // Standard retail sequence from FUN_00179FB8:
    // 1. Fade out screen to black
    // 2. Unload current level
    // 3. Load target container
    // 4. Instantiate game objects and spawners
    // 5. Load level audio banks
    // 6. Fade in screen

    SceneCommand cmdFadeOut;
    cmdFadeOut.type = SceneCmdType::FadeOut;
    cmdFadeOut.fadeDuration = fadeDuration;

    SceneCommand cmdUnload;
    cmdUnload.type = SceneCmdType::UnloadScene;

    SceneCommand cmdLoad;
    cmdLoad.type = SceneCmdType::LoadContainer;
    cmdLoad.containerName = containerName;
    cmdLoad.spawnPointName = spawnName;

    SceneCommand cmdSpawn;
    cmdSpawn.type = SceneCmdType::SpawnEntities;
    cmdSpawn.spawnPointName = spawnName;

    SceneCommand cmdAudio;
    cmdAudio.type = SceneCmdType::LoadAudioBanks;

    SceneCommand cmdFadeIn;
    cmdFadeIn.type = SceneCmdType::FadeIn;
    cmdFadeIn.fadeDuration = fadeDuration;

    PushCommand(cmdFadeOut);
    PushCommand(cmdUnload);
    PushCommand(cmdLoad);
    PushCommand(cmdSpawn);
    PushCommand(cmdAudio);
    PushCommand(cmdFadeIn);
}

void SceneQueue::PushCommand(const SceneCommand& cmd) {
    m_queue.push_back(cmd);
}

bool SceneQueue::Update(float dt) {
    // Process screen fades
    if (m_isFading) {
        m_fadeTimer += dt;
        float progress = std::clamp(m_fadeTimer / std::max(0.001f, m_fadeDuration), 0.0f, 1.0f);
        if (m_fadeTarget > 0.5f) {
            m_fadeAlpha = progress; // fading to black
        } else {
            m_fadeAlpha = 1.0f - progress; // fading to transparent
        }

        if (progress >= 1.0f) {
            m_fadeAlpha = m_fadeTarget;
            m_isFading = false;
        }
        return true;
    }

    if (m_currentIdx >= m_queue.size()) {
        m_queue.clear();
        m_currentIdx = 0;
        return false;
    }

    const auto& cmd = m_queue[m_currentIdx];
    bool cmdDone = true;

    switch (cmd.type) {
    case SceneCmdType::FadeOut:
        m_isFading = true;
        m_fadeTimer = 0.0f;
        m_fadeTarget = 1.0f;
        m_fadeDuration = cmd.fadeDuration;
        cmdDone = true;
        break;

    case SceneCmdType::FadeIn:
        m_isFading = true;
        m_fadeTimer = 0.0f;
        m_fadeTarget = 0.0f;
        m_fadeDuration = cmd.fadeDuration;
        cmdDone = true;
        break;


    case SceneCmdType::UnloadScene:
        Core::EventManager::GetInstance().SendMsg("UnloadScene");
        break;

    case SceneCmdType::LoadContainer:
        Core::EventManager::GetInstance().SendMsg("LoadContainer", nullptr, (void*)cmd.containerName.c_str());
        break;

    case SceneCmdType::SpawnEntities:
        Core::EventManager::GetInstance().SendMsg("SpawnEntities", nullptr, (void*)cmd.spawnPointName.c_str());
        break;

    case SceneCmdType::LoadAudioBanks:
        Core::EventManager::GetInstance().SendMsg("LoadAudioBanks");
        break;

    case SceneCmdType::CustomCallback:
        if (cmd.callback) {
            cmdDone = cmd.callback();
        }
        break;
    }

    if (cmdDone) {
        m_currentIdx++;
    }

    return true;
}

void SceneQueue::Clear() {
    m_queue.clear();
    m_currentIdx = 0;
    m_fadeAlpha = 0.0f;
    m_isFading = false;
}

} // namespace World
} // namespace SHO
