#pragma once

#include "SHO/Core/Types.h"
#include <string>

namespace SHO {
namespace Core {

// Translated from CSystemCommands (base placed object properties)
class SystemCommands {
public:
    SystemCommands();
    virtual ~SystemCommands() = default;

    // Instance identification
    const std::string& GetName() const { return m_name; }
    void SetName(const std::string& name) { m_name = name; }

    const std::string& GetClassName() const { return m_className; }
    void SetClassName(const std::string& cls) { m_className = cls; }

    const Guid& GetGuid() const { return m_guid; }
    void SetGuid(const Guid& guid) { m_guid = guid; }

    // World Transformation (from placed 0x0704 records)
    const Mat4& GetTransform() const { return m_transform; }
    void SetTransform(const Mat4& mat);

    Vec3 GetPosition() const;
    void SetPosition(const Vec3& pos);

    Vec3 GetForward() const;
    Vec3 GetRight() const;
    Vec3 GetUp() const;

    // Activation / Enabled state
    bool IsActive() const { return m_active; }
    void SetActive(bool active) { m_active = active; }

    bool IsVisible() const { return m_visible; }
    void SetVisible(bool vis) { m_visible = vis; }

protected:
    std::string m_name;
    std::string m_className;
    Guid        m_guid;
    Mat4        m_transform = Mat4(1.0f);
    bool        m_active = true;
    bool        m_visible = true;
};

} // namespace Core
} // namespace SHO
