#include "SHO/Core/SystemCommands.h"

namespace SHO {
namespace Core {

SystemCommands::SystemCommands()
    : m_transform(1.0f), m_active(true), m_visible(true) {}

void SystemCommands::SetTransform(const Mat4& mat) {
    m_transform = mat;
}

Vec3 SystemCommands::GetPosition() const {
    return Vec3(m_transform[3]);
}

void SystemCommands::SetPosition(const Vec3& pos) {
    m_transform[3] = Vec4(pos, 1.0f);
}

Vec3 SystemCommands::GetRight() const {
    return glm::normalize(Vec3(m_transform[0]));
}

Vec3 SystemCommands::GetUp() const {
    return glm::normalize(Vec3(m_transform[1]));
}

Vec3 SystemCommands::GetForward() const {
    // In RenderWare PS2 matrix conventions (right/up/at/pos), row 2 is the 'at' (forward) vector
    return glm::normalize(Vec3(m_transform[2]));
}

} // namespace Core
} // namespace SHO
