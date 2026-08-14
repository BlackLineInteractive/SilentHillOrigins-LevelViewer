#pragma once

#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "SHO/Core/MessageRelay.h"

namespace SHO {
namespace Triggers {

enum class TriggerType {
    Plane,
    AreaBox,
    AreaPoly,
    Zone,
    Button,
    MessageBox,
    Cutscene,
    Music
};

struct BaseTrigger {
    std::string name;
    TriggerType type;
    glm::vec3   position = glm::vec3(0.0f);
    glm::vec3   size = glm::vec3(1.0f);
    bool        active = true;
    bool        oneShot = false;
    bool        triggered = false;
};

struct ZoneLinkTrigger : public BaseTrigger {
    std::string fromZone;
    std::string toZone;
    std::string doorName;
    std::string requiredKey;
    bool        locked = false;
    float       interactionRadius = 1.5f;
    glm::vec3   spawnPosition = glm::vec3(0.0f);
    glm::vec3   spawnFacing = glm::vec3(0.0f, 0.0f, 1.0f);
};

struct CameraPlaneTrigger : public BaseTrigger {
    glm::vec3 normal = glm::vec3(0, 0, 1);
    float     d = 0.0f;
    int       camFront = -1;
    int       camBack = -1;
    float     lastDot = 0.0f;
    bool      initialized = false;
};

struct InteractiveTrigger : public BaseTrigger {
    std::string promptText;
    std::string examinationText;
    std::string itemId;
    std::string eventId;
    float       radius = 1.6f;
    bool        isExamineOnly = false;
};

class TriggerManager {
public:
    static TriggerManager& GetInstance() {
        static TriggerManager instance;
        return instance;
    }

    void Clear() {
        m_zoneTriggers.clear();
        m_planeTriggers.clear();
        m_interactiveTriggers.clear();
    }

    void AddZoneTrigger(const ZoneLinkTrigger& trigger) {
        m_zoneTriggers.push_back(trigger);
    }

    void AddCameraPlane(const CameraPlaneTrigger& trigger) {
        m_planeTriggers.push_back(trigger);
    }

    void AddInteractive(const InteractiveTrigger& trigger) {
        m_interactiveTriggers.push_back(trigger);
    }

    // Evaluate camera switches along path
    int EvaluateCameraPlanes(const glm::vec3& playerPos, int currentCam);

    // Find closest door / zone prompt near player
    ZoneLinkTrigger* CheckZonePrompt(const glm::vec3& playerPos);

    // Find interactive item/examination prompt near player
    InteractiveTrigger* CheckInteractivePrompt(const glm::vec3& playerPos);

    const std::vector<ZoneLinkTrigger>& GetZoneTriggers() const { return m_zoneTriggers; }

private:
    TriggerManager() = default;
    std::vector<ZoneLinkTrigger>     m_zoneTriggers;
    std::vector<CameraPlaneTrigger>  m_planeTriggers;
    std::vector<InteractiveTrigger>  m_interactiveTriggers;
};

} // namespace Triggers
} // namespace SHO
