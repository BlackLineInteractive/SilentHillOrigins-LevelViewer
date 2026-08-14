#pragma once

#include "SHO/Triggers/TriggerBase.h"

namespace SHO {
namespace Actor {

// Translated from CPlayerReflector (interactive mirrors for Otherworld shifting)
class MirrorReflector : public Triggers::TriggerBase {
public:
    MirrorReflector();
    virtual ~MirrorReflector() = default;

    // Normal of mirror surface
    const Core::Vec3& GetMirrorNormal() const { return m_mirrorNormal; }
    void SetMirrorNormal(const Core::Vec3& n) { m_mirrorNormal = glm::normalize(n); }

    // Target alternate container (e.g. "HO_1_ExamRoom_Alt")
    const std::string& GetOtherworldZone() const { return m_otherworldZone; }
    void SetOtherworldZone(const std::string& zone) { m_otherworldZone = zone; }

    bool IsOtherworld() const { return m_isOtherworld; }
    void SetOtherworld(bool alt) { m_isOtherworld = alt; }

    bool CheckTrigger(const Core::Vec3& actorPos, const Core::Vec3& prevActorPos) override;
    void TouchMirror();

private:
    Core::Vec3  m_mirrorNormal = Core::Vec3(0, 0, 1);
    float       m_interactionDist = 1.5f;
    std::string m_otherworldZone;
    bool        m_isOtherworld = false;
    bool        m_playerCanTouch = false;
};

} // namespace Actor
} // namespace SHO
