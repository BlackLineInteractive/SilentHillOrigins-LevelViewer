#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// Small drawing helpers lifted out of src/main.cpp.
//
// The only thing these five have in common is that they are *pure*: they read
// their arguments and nothing else. main.cpp keeps a dozen file-scope statics
// (the doorway prompt, the player's feet, the camera `state`) and every other
// free function up there touches one of them, which is why only these five
// moved. `BuildView` in particular stays behind: it reads the global `state`.
//
// Moving them is a readability change and nothing more -- the bodies are
// unchanged, so anything that looked right before still looks right.
// ─────────────────────────────────────────────────────────────────────────────

#include <vector>

#include <GL/glew.h>
#include <glm/glm.hpp>

#include "imgui.h"

#include "ClimaxEngine/Core/Types.h"

namespace ClimaxEngine {
namespace Viewer {

// Draw a 3-ring orientation sphere into the given draw list.
// `view`'s upper-left 3x3 acts as the world-to-camera rotation.
// Rings: XZ (equatorial/green), XY (blue), YZ (red).
void DrawOrbitSphere(ImDrawList *dl, ImVec2 ctr, float R, const glm::mat4 &view);

// Append one octahedron-wireframe marker plus three orientation axes.
void AppendMarker(std::vector<glm::vec3> &out, const glm::vec3 &pos,
                  const glm::mat4 &xform, float R, float axisLen);

// Project a world position to screen space and draw a boxed label.
void DrawWorldLabel(ImDrawList *dl, const glm::mat4 &viewProj,
                    const glm::vec3 &pos, int winW, int winH, const char *text,
                    ImU32 col);

// Compile + link a program, reporting the driver's log instead of silently
// handing back a broken (black-screen) program object.
GLuint MakeProgram(const char *name, const char *vsSrc, const char *fsSrc);

// Samples a UV animation layer, returning (uScale, vScale, uOffset, vOffset).
// Linear between keyframes, which is the blend `ClimaxT1KeyFrameBlend` does.
glm::vec4 EvalUVAnim(const UVAnimClip &clip, size_t layer, float t);

} // namespace Viewer
} // namespace ClimaxEngine
