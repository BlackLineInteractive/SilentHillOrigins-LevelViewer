#include "SHO/Triggers/AreaTrigger.h"
#include "SHO/Core/EventManager.h"

namespace SHO {
namespace Triggers {

AreaTrigger::AreaTrigger() {
    m_className = "AreaTriggerBox";
}

void AreaTrigger::SetPolyPoints(const std::vector<Core::Vec2>& pts, float minY, float maxY) {
    m_polyPoints = pts;
    m_minY = minY;
    m_maxY = maxY;
    m_isPoly = true;
    m_className = "AreaTrigger2DPoly";
}

bool AreaTrigger::PointInPoly(const Core::Vec2& pt) const {
    if (m_polyPoints.size() < 3) return false;

    bool inside = false;
    size_t n = m_polyPoints.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        float xi = m_polyPoints[i].x, yi = m_polyPoints[i].y;
        float xj = m_polyPoints[j].x, yj = m_polyPoints[j].y;

        bool intersect = ((yi > pt.y) != (yj > pt.y)) &&
                         (pt.x < (xj - xi) * (pt.y - yi) / (yj - yi + 1e-6f) + xi);
        if (intersect) inside = !inside;
    }
    return inside;
}

bool AreaTrigger::CheckTrigger(const Core::Vec3& actorPos, const Core::Vec3& /*prevActorPos*/) {
    if (!m_triggerEnabled || !m_active) return false;

    bool insideNow = false;
    if (m_isPoly) {
        if (actorPos.y >= m_minY && actorPos.y <= m_maxY) {
            insideNow = PointInPoly(Core::Vec2(actorPos.x, actorPos.z));
        }
    } else {
        insideNow = m_box.Contains(actorPos);
    }

    if (insideNow && !m_isInside) {
        m_isInside = true;
        if (!m_enterEvent.empty()) {
            Core::EventManager::GetInstance().SendMsg(m_enterEvent, this);
        }
    } else if (!insideNow && m_isInside) {
        m_isInside = false;
        if (!m_exitEvent.empty()) {
            Core::EventManager::GetInstance().SendMsg(m_exitEvent, this);
        }
    }

    return insideNow;
}

} // namespace Triggers
} // namespace SHO
