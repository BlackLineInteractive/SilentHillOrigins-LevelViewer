#pragma once

#include "SHO/Triggers/TriggerBase.h"

namespace SHO {
namespace Triggers {

// Translated from ZoneTrigger & CZone (room transitions and doorway portals)
class ZoneTrigger : public TriggerBase {
public:
    ZoneTrigger();
    virtual ~ZoneTrigger() = default;

    // Target container / room name (e.g. "DH_1_Hallway", "HO_1_Lobby")
    const std::string& GetTargetZone() const { return m_targetZone; }
    void SetTargetZone(const std::string& zone) { m_targetZone = zone; }

    const std::string& GetSourceZone() const { return m_sourceZone; }
    void SetSourceZone(const std::string& zone) { m_sourceZone = zone; }

    // Spawn point identifier in the target zone
    const std::string& GetTargetSpawnName() const { return m_targetSpawnName; }
    void SetTargetSpawnName(const std::string& spawn) { m_targetSpawnName = spawn; }

    // Doorway interaction bounds
    const Core::AABB& GetBounds() const { return m_bounds; }
    void SetBounds(const Core::AABB& aabb) { m_bounds = aabb; }

    // Whether this door is locked or requires interaction
    bool IsLocked() const { return m_isLocked; }
    void SetLocked(bool locked) { m_isLocked = locked; }

    const std::string& GetRequiredKeyItem() const { return m_requiredKey; }
    void SetRequiredKeyItem(const std::string& key) { m_requiredKey = key; }

    bool CheckTrigger(const Core::Vec3& actorPos, const Core::Vec3& prevActorPos) override;
    void TriggerTransition();

private:
    std::string m_sourceZone;
    std::string m_targetZone;
    std::string m_targetSpawnName;
    Core::AABB  m_bounds;
    bool        m_isLocked = false;
    std::string m_requiredKey;
};

} // namespace Triggers
} // namespace SHO
