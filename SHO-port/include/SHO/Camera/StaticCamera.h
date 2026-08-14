#pragma once

#include "SHO/Camera/BaseCamera.h"

namespace SHO {
namespace Camera {

// Translated from CStaticCamera (fixed level cameras with authored orientation)
class StaticCamera : public BaseCamera {
public:
    StaticCamera();
    virtual ~StaticCamera() = default;

    // Sets whether this camera should strictly lock to authored look direction
    // or provide dynamic soft tracking if the target wanders far out of frame
    void SetDynamicTrackingFallback(bool enable) { m_dynamicFallback = enable; }

    Core::Mat4 GetViewMatrix() const override;
    void OnUpdate(float dt) override;

    // Authored pitch and yaw angles in degrees
    float GetAuthoredPitch() const { return m_authoredPitch; }
    float GetAuthoredYaw() const { return m_authoredYaw; }

    void SetAuthoredAngles(float pitchDeg, float yawDeg);

private:
    void ExtractAuthoredAngles();

    float m_authoredPitch = 0.0f;
    float m_authoredYaw = 0.0f;
    bool  m_hasAuthoredAngles = false;
    bool  m_dynamicFallback = true;
    Core::Vec3 m_currentForward = Core::Vec3(0, 0, 1);
};

} // namespace Camera
} // namespace SHO
