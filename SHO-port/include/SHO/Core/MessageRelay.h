#pragma once

#include <string>
#include <vector>
#include <functional>
#include <map>
#include <memory>
#include <glm/glm.hpp>

namespace SHO {
namespace Core {

enum class MessageType {
    // Triggers & Level Flow
    TriggerEnter,
    TriggerExit,
    TriggerActivate,
    ZoneTransition,
    DoorInteraction,
    DoorLocked,
    DoorUnlocked,

    // Cameras
    CameraCut,
    CameraBlend,

    // Player Actions
    PlayerWalk,
    PlayerRun,
    PlayerFootstep,
    PlayerFlashlightToggle,
    PlayerAttack,
    PlayerDamaged,
    PlayerDied,

    // Items & Puzzles
    ItemCollected,
    ItemUsed,
    PuzzleActivated,
    PuzzleSolved,
    PuzzleFailed,

    // Audio & Atmosphere
    PlayAudioCue,
    PlayStreamMusic,
    StopStreamMusic,
    SetFogDensity
};

struct GameEvent {
    MessageType type;
    std::string sender;
    std::string target;
    std::string stringParam;
    int         intParam = 0;
    float       floatParam = 0.0f;
    glm::vec3   vectorParam = glm::vec3(0.0f);
    void*       userData = nullptr;
};

using GameEventCallback = std::function<void(const GameEvent&)>;

class MessageRelay {
public:
    static MessageRelay& GetInstance() {
        static MessageRelay instance;
        return instance;
    }

    // Subscribe to a specific message type
    void Subscribe(MessageType type, GameEventCallback callback) {
        m_listeners[type].push_back(callback);
    }

    // Dispatch an event immediately to all subscribers
    void Send(const GameEvent& event) {
        auto it = m_listeners.find(event.type);
        if (it != m_listeners.end()) {
            for (auto& cb : it->second) {
                if (cb) cb(event);
            }
        }
    }


    // Convenience broadcast helpers
    void BroadcastTriggerEnter(const std::string& triggerName, const std::string& entityName) {
        GameEvent e;
        e.type = MessageType::TriggerEnter;
        e.sender = triggerName;
        e.target = entityName;
        Send(e);
    }

    void BroadcastZoneTransition(const std::string& fromZone, const std::string& toZone) {
        GameEvent e;
        e.type = MessageType::ZoneTransition;
        e.sender = fromZone;
        e.target = toZone;
        Send(e);
    }

    void BroadcastCameraCut(int cameraIndex, float fovDeg) {
        GameEvent e;
        e.type = MessageType::CameraCut;
        e.intParam = cameraIndex;
        e.floatParam = fovDeg;
        Send(e);
    }

    void BroadcastFootstep(const std::string& surfaceName, const glm::vec3& position) {
        GameEvent e;
        e.type = MessageType::PlayerFootstep;
        e.stringParam = surfaceName;
        e.vectorParam = position;
        Send(e);
    }

    void Clear() {
        m_listeners.clear();
    }

private:
    MessageRelay() = default;
    std::map<MessageType, std::vector<GameEventCallback>> m_listeners;

};

} // namespace Core
} // namespace SHO
