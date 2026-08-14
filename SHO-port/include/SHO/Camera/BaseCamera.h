#pragma once

#include "SHO/Core/Behaviour.h"

namespace SHO {
namespace Camera {

// Translated from CBaseCamera (base camera class for static, constraint, cutscene cameras)
class BaseCamera : public Core::Behaviour {
public:
    BaseCamera();
    virtual ~BaseCamera() = default;

    // Field of View in degrees (from CBaseCamera property 2)
    float GetFOV() const { return m_fovDeg; }
    void SetFOV(float fovDeg) { m_fovDeg = fovDeg; }

    // Event identifiers: Primary event name and Secondary 'S' event name
    // e.g. "camHallway" and "camHallwayS"
    const std::string& GetEventName() const { return m_eventName; }
    void SetEventName(const std::string& name);

    const std::string& GetAltEventName() const { return m_altEventName; }
    void SetAltEventName(const std::string& name) { m_altEventName = name; }

    // Target object / tracking position
    const Core::Vec3& GetTargetPosition() const { return m_targetPos; }
    void SetTargetPosition(const Core::Vec3& pos) { m_targetPos = pos; }

    // Matrices
    virtual Core::Mat4 GetViewMatrix() const;
    virtual Core::Mat4 GetProjectionMatrix(float aspect, float nearZ = 0.1f, float farZ = 200.0f) const;

    // Camera update hook
    void OnUpdate(float dt) override;
    void OnEvent(const Core::Msg& msg) override;

    // Whether this camera is currently active
    bool IsCurrent() const { return m_isCurrent; }
    virtual void SetCurrent(bool current);

protected:
    float       m_fovDeg = 60.0f;
    std::string m_eventName;
    std::string m_altEventName;
    Core::Vec3  m_targetPos = Core::Vec3(0.0f);
    bool        m_isCurrent = false;
};

} // namespace Camera
} // namespace SHO
