#pragma once

#include "SHO/Triggers/TriggerBase.h"

namespace SHO {
namespace Triggers {

// Translated from AreaTriggerBox & AreaTrigger2DPoly
class AreaTrigger : public TriggerBase {
public:
    AreaTrigger();
    virtual ~AreaTrigger() = default;

    void SetBoxBounds(const Core::AABB& box) { m_box = box; m_isPoly = false; }
    void SetPolyPoints(const std::vector<Core::Vec2>& pts, float minY, float maxY);

    const std::string& GetEnterEvent() const { return m_enterEvent; }
    void SetEnterEvent(const std::string& evt) { m_enterEvent = evt; }

    const std::string& GetExitEvent() const { return m_exitEvent; }
    void SetExitEvent(const std::string& evt) { m_exitEvent = evt; }

    bool CheckTrigger(const Core::Vec3& actorPos, const Core::Vec3& prevActorPos) override;

private:
    bool PointInPoly(const Core::Vec2& pt) const;

    bool m_isPoly = false;
    Core::AABB m_box;
    std::vector<Core::Vec2> m_polyPoints;
    float m_minY = -1000.0f;
    float m_maxY =  1000.0f;

    std::string m_enterEvent;
    std::string m_exitEvent;
    bool m_isInside = false;
};

} // namespace Triggers
} // namespace SHO
