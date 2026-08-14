#pragma once

#include "SHO/Core/Types.h"
#include <functional>
#include <string>
#include <vector>

namespace SHO {
namespace World {

enum class SceneCmdType {
    FadeOut,
    UnloadScene,
    LoadContainer,
    SpawnEntities,
    LoadAudioBanks,
    FadeIn,
    CustomCallback
};

// Scene queue command node (decompiled 0x1C-byte node from FUN_00179FB8)
struct SceneCommand {
    SceneCmdType type = SceneCmdType::FadeOut;
    std::string  containerName;
    std::string  spawnPointName;
    float        fadeDuration = 0.5f;
    std::function<bool()> callback;
};

// Scene Queue Manager translated from FUN_00179FB8 and FUN_00179D60
class SceneQueue {
public:
    static SceneQueue& GetInstance();

    // Queue standard level transition sequence
    void QueueLevelTransition(const std::string& containerName, const std::string& spawnName = "", float fadeDuration = 0.5f);

    // Queue generic command
    void PushCommand(const SceneCommand& cmd);

    // Process queue frame by frame (returns true when transition is in progress)
    bool Update(float dt);

    bool IsBusy() const { return !m_queue.empty() || m_isFading; }
    float GetFadeAlpha() const { return m_fadeAlpha; }

    void Clear();

private:
    SceneQueue() = default;
    ~SceneQueue() = default;

    std::vector<SceneCommand> m_queue;
    size_t m_currentIdx = 0;
    float  m_fadeAlpha = 0.0f;
    float  m_fadeTimer = 0.0f;
    float  m_fadeTarget = 0.0f;
    float  m_fadeDuration = 0.5f;
    bool   m_isFading = false;
};

} // namespace World
} // namespace SHO
