#include "ClimaxEngine/Game/SceneObjects.h"

#include <cstring>

namespace ClimaxEngine {
namespace Game {

namespace {

uint32_t Rd32(const uint8_t *p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}

// Names are NUL-terminated and then padded with 0xBF up to the record size.
std::string ReadName(const uint8_t *p, size_t len) {
    size_t end = 0;
    while (end < len && p[end] != 0x00 && p[end] != 0xBF)
        ++end;
    return std::string((const char *)p, end);
}

} // namespace

std::vector<SceneObject> ParseSceneObjects(const uint8_t *data, size_t size) {
    std::vector<SceneObject> out;
    if (!data || size < 12)
        return out;

    size_t o = 0;
    while (o + 12 <= size) {
        if (Rd32(data + o) != 0x0704) {
            o += 4;
            continue;
        }
        const uint32_t chunk = Rd32(data + o + 4);
        if (chunk < 8 || o + 12 + (size_t)chunk > size) {
            o += 4;
            continue;
        }

        const size_t body = o + 12;
        const size_t end = body + chunk;

        SceneObject go;
        go.offset = (uint32_t)o;

        // The body opens with a four-byte field that is not a record.
        size_t p = body + 4;
        std::string component;
        long lastIdx = -1;
        while (p + 8 <= end) {
            const uint32_t rs = Rd32(data + p);
            const uint32_t rid = Rd32(data + p + 4);
            if (rs < 8 || p + rs > end)
                break;

            const uint8_t *pay = data + p + 8;
            const size_t payLen = rs - 8;
            const uint32_t kind = rid >> 24;
            const uint32_t idx = rid & 0x00FFFFFFu;

            if (kind == 0x20) {
                component = ReadName(pay, payLen);
                if (go.className.empty())
                    go.className = component;
                lastIdx = -1;
            } else if (kind == 0x80) {
                if (go.className.empty())
                    go.className = ReadName(pay, payLen);
            } else if (kind == 0x00) {
                if ((long)idx <= lastIdx)
                    lastIdx = -1;   // a new component started
                lastIdx = (long)idx;
                // Take the placement wherever it turns up. The first one wins,
                // which is the object's own; a class that keeps a second 64-byte
                // property uses it for its own volume, not for where it stands.
                if (payLen == 64 && idx == 1 && !go.placed) {
                    std::memcpy(&go.transform, pay, 64);
                    // SH_FORMAT.md §3.5.1: what is stored is a 3x4 affine in a
                    // 4x4 slot -- the homogeneous row was never written, so
                    // m[3][3] is zero along with the rest of row 3. Used as
                    // stored, every transformed vertex gets w = 0 and the
                    // perspective divide throws the object to infinity. True of
                    // all 11801 matrices in the retail archive.
                    go.transform[0][3] = 0.0f;
                    go.transform[1][3] = 0.0f;
                    go.transform[2][3] = 0.0f;
                    go.transform[3][3] = 1.0f;
                    go.position = glm::vec3(go.transform[3]);
                    go.placed = true;
                }
            }
            p += rs;
        }

        // A four-byte value that happens to read 0x0704 is common in a 4 MB
        // container, and taking its "size" at face value skipped most of
        // IntroRoad. A chunk that yielded no class name was not a chunk.
        if (go.className.empty()) {
            o += 4;
            continue;
        }
        out.push_back(go);
        o = end;
    }
    return out;
}

} // namespace Game
} // namespace ClimaxEngine
