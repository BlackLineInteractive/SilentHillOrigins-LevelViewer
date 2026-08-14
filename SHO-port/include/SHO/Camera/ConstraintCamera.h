#pragma once

#include "SHO/Camera/BaseCamera.h"

namespace SHO {
namespace Camera {

// Translated from CConstraintCamera (semi-static follow camera tracking the player)
class ConstraintCamera : public BaseCamera {
public:
    ConstraintCamera();
    virtual ~ConstraintCamera() = default;

    // Tracking constraints (21 properties from PS2 executable analysis)
    void SetPitchLimits(float minPitchDeg, float maxPitchDeg);
    void SetYawLimits(float minYawDeg, float maxYawDeg);
    void SetTrackingDistance(float dist) { m_trackDistance = dist; }
    void SetTrackingSpeed(float speed) { m_trackingSpeed = speed; }
    void SetConstraintVolume(const Core::AABB& volume) { m_volume = volume; m_hasVolume = true; }

    Core::Mat4 GetViewMatrix() const override;
    void OnUpdate(float dt) override;

private:
    float m_minPitch = -60.0f;
    float m_maxPitch =  60.0f;
    float m_minYaw   = -180.0f;
    float m_maxYaw   =  180.0f;
    float m_trackDistance = 5.0f;
    float m_trackingSpeed = 4.0f;
    bool  m_hasVolume = false;
    Core::AABB m_volume;

    Core::Vec3 m_currentEye = Core::Vec3(0.0f);
    Core::Vec3 m_currentForward = Core::Vec3(0, 0, 1);
};

} // namespace Camera
} // namespace SHO
