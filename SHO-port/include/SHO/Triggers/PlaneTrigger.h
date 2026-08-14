#pragma once

#include "SHO/Triggers/TriggerBase.h"

namespace SHO {
namespace Triggers {

// Translated from PlaneTrigger (switches cameras when player crosses a trigger plane)
class PlaneTrigger : public TriggerBase {
public:
    PlaneTrigger();
    virtual ~PlaneTrigger() = default;

    // Plane definition: normal and distance along normal
    const Core::Vec3& GetPlaneNormal() const { return m_normal; }
    void SetPlaneNormal(const Core::Vec3& n) { m_normal = glm::normalize(n); }

    float GetPlaneDistance() const { return m_distance; }
    void SetPlaneDistance(float d) { m_distance = d; }

    // Width and height limits around trigger origin
    float GetHalfWidth() const { return m_halfWidth; }
    void SetHalfWidth(float hw) { m_halfWidth = hw; }

    float GetHalfHeight() const { return m_halfHeight; }
    void SetHalfHeight(float hh) { m_halfHeight = hh; }

    // Camera target event names (e.g. "camLanding01" and "camLanding02")
    const std::string& GetForwardCameraName() const { return m_forwardCamName; }
    void SetForwardCameraName(const std::string& name) { m_forwardCamName = name; }

    const std::string& GetBackwardCameraName() const { return m_backwardCamName; }
    void SetBackwardCameraName(const std::string& name) { m_backwardCamName = name; }

    bool CheckTrigger(const Core::Vec3& actorPos, const Core::Vec3& prevActorPos) override;

private:
    Core::Vec3  m_normal = Core::Vec3(0, 0, 1);
    float       m_distance = 0.0f;
    float       m_halfWidth = 5.0f;
    float       m_halfHeight = 3.0f;
    std::string m_forwardCamName;
    std::string m_backwardCamName;
};

} // namespace Triggers
} // namespace SHO
