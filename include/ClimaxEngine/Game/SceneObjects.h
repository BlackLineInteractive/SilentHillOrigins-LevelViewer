#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// The placed objects of a scene, read straight from the container bytes.
//
// The toolkit already parses these, but that code sits in a translation unit
// that calls GL, so the game cannot use it. This is the same walk with nothing
// but memory in it: give it the bytes `SceneCmd::OpenArchive` read and it hands
// back what the level places -- the player's spawn, the cameras, the triggers,
// the lights.
//
// A 0x0704 chunk is a flat list of tagged records:
//
//     [u32 recordSize][u32 recordId][payload]
//
// The top byte of the id selects the kind and the low 24 bits are the property
// index inside the current component:
//
//     0x20  component class name     0x80  instance / base-class name
//     0x40  16-byte GUID             0x00  indexed property
//
// Property indices restart at 0 for every component, so "property 3" means
// nothing without knowing which component it belongs to; a component boundary
// is simply the index no longer increasing. A 64-byte property is a
// column-major 4x4 placement, and it is not always in the first component --
// CColorLight keeps its in the fourth.
// ─────────────────────────────────────────────────────────────────────────────

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace ClimaxEngine {
namespace Game {

struct SceneObject {
    std::string className;   // "CPlayerSpawner", "CStaticCamera", …
    glm::mat4 transform = glm::mat4(1.0f);
    glm::vec3 position = glm::vec3(0.0f);
    bool placed = false;     // a 64-byte placement was found
    uint32_t offset = 0;     // where in the container, for reporting
};

// Walks every 0x0704 chunk in `data`. Never throws, never allocates beyond the
// result, and does not care what the rest of the container holds.
std::vector<SceneObject> ParseSceneObjects(const uint8_t *data, size_t size);

} // namespace Game
} // namespace ClimaxEngine
