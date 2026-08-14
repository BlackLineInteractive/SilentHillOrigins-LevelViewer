#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace SHO {
namespace Core {

using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;
using Mat4 = glm::mat4;
using Quat = glm::quat;

// 128-bit GUID matching the RWS / RenderWare Studio object identifier
struct Guid {
    uint32_t data[4] = { 0, 0, 0, 0 };

    bool IsValid() const {
        return (data[0] | data[1] | data[2] | data[3]) != 0;
    }

    bool operator==(const Guid& o) const {
        return data[0] == o.data[0] && data[1] == o.data[1] &&
               data[2] == o.data[2] && data[3] == o.data[3];
    }

    bool operator!=(const Guid& o) const { return !(*this == o); }

    bool operator<(const Guid& o) const {
        for (int i = 0; i < 4; i++) {
            if (data[i] < o.data[i]) return true;
            if (data[i] > o.data[i]) return false;
        }
        return false;
    }

    std::string ToString() const {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%08X-%08X-%08X-%08X",
                      data[0], data[1], data[2], data[3]);
        return std::string(buf);
    }
};

// Axis-Aligned Bounding Box
struct AABB {
    Vec3 min = Vec3(1e30f);
    Vec3 max = Vec3(-1e30f);

    bool Contains(const Vec3& pt) const {
        return pt.x >= min.x && pt.x <= max.x &&
               pt.y >= min.y && pt.y <= max.y &&
               pt.z >= min.z && pt.z <= max.z;
    }

    void Expand(const Vec3& pt) {
        min = glm::min(min, pt);
        max = glm::max(max, pt);
    }
};

// Triangle for collision tests
struct CollisionFace {
    Vec3 v0, v1, v2;
    Vec3 normal;
    uint16_t surfaceFlags = 0;

    void CalculateNormal() {
        normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));
    }
};

// PS2 Pad Input Bitmask (from SLES decompilation FUN_00193F90)
enum class PadButton : uint32_t {
    None      = 0,
    Up        = 0x001,
    Down      = 0x002,
    Left      = 0x004,
    Right     = 0x008,
    DpadUp    = 0x001,
    DpadDown  = 0x002,
    DpadLeft  = 0x004,
    DpadRight = 0x008,
    Triangle  = 0x010,
    Cross     = 0x020,
    Square    = 0x040,
    Circle    = 0x080,
    Start     = 0x100,
    Select    = 0x200,
    L1        = 0x400,
    R1        = 0x800,
    L2        = 0x1000,
    R2        = 0x2000,
    L3        = 0x4000,
    R3        = 0x8000,
};


inline PadButton operator|(PadButton a, PadButton b) {
    return static_cast<PadButton>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline bool HasFlag(PadButton mask, PadButton btn) {
    return (static_cast<uint32_t>(mask) & static_cast<uint32_t>(btn)) != 0;
}

} // namespace Core
} // namespace SHO
