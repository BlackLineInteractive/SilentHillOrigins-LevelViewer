// Bodies moved verbatim out of src/main.cpp. See the header for why these
// five and not the rest.
#include "ClimaxEngine/Viewer/ViewerDraw.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace ClimaxEngine {
namespace Viewer {

// Draw a 3-ring orientation sphere into the given draw list.
// 'view' upper-left 3x3 acts as the world-to-camera rotation.
// Rings: XZ (equatorial/green), XY (blue), YZ (red).
void DrawOrbitSphere(ImDrawList* dl, ImVec2 ctr, float R, const glm::mat4& view) {
    dl->AddCircleFilled(ctr, R + 3.0f, IM_COL32(14, 14, 18, 220));
    dl->AddCircle(ctr, R + 3.0f, IM_COL32(50, 50, 65, 200), 64, 1.0f);

    glm::mat3 rot = glm::mat3(view); // world -> camera rotation

    struct Ring { glm::vec3 u, v; ImU32 front, back; };
    Ring rings[3] = {
        { {1,0,0}, {0,0,1}, IM_COL32(65,188,75,225),  IM_COL32(28,76,32,70) },  // XZ
        { {1,0,0}, {0,1,0}, IM_COL32(65,125,228,225), IM_COL32(28,52,98,70) },  // XY
        { {0,1,0}, {0,0,1}, IM_COL32(208,72,62,225),  IM_COL32(86,28,26,70) },  // YZ
    };
    const int N = 80;
    for (auto& ring : rings) {
        std::vector<ImVec2> fpts, bpts;
        fpts.reserve(N + 1); bpts.reserve(N + 1);
        for (int i = 0; i <= N; i++) {
            float a = (float)i * 6.28318f / N;
            glm::vec3 w = cosf(a) * ring.u + sinf(a) * ring.v;
            glm::vec3 c = rot * w;
            ImVec2 s(ctr.x + c.x * R, ctr.y - c.y * R);
            (c.z <= 0.0f ? fpts : bpts).push_back(s);
        }
        if (!bpts.empty()) dl->AddPolyline(bpts.data(), (int)bpts.size(), ring.back,  0, 1.2f);
        if (!fpts.empty()) dl->AddPolyline(fpts.data(), (int)fpts.size(), ring.front, 0, 1.8f);
    }
    // Axis dot labels (front-facing only)
    struct Ax { glm::vec3 d; ImU32 col; const char* lbl; };
    Ax axes[3] = {
        { {1,0,0}, IM_COL32(218,72,52,255),  "X" },
        { {0,1,0}, IM_COL32(55,192,55,255),  "Y" },
        { {0,0,1}, IM_COL32(62,122,222,255), "Z" },
    };
    for (auto& ax : axes) {
        glm::vec3 c = rot * ax.d;
        if (c.z > 0.0f) continue;
        ImVec2 s(ctr.x + c.x * R * 0.86f, ctr.y - c.y * R * 0.86f);
        dl->AddCircleFilled(s, 3.5f, ax.col);
        dl->AddText(ImVec2(s.x + 5.0f, s.y - 7.0f), ax.col, ax.lbl);
    }
}

// Append one octahedron-wireframe marker plus three orientation axes.
void AppendMarker(std::vector<glm::vec3>& out, const glm::vec3& pos,
                         const glm::mat4& xform, float R, float axisLen) {
    const glm::vec3 O[6] = {
        { R, 0, 0}, {-R, 0, 0},
        {0,  R, 0}, {0, -R, 0},
        {0, 0,  R}, {0, 0, -R},
    };
    static const int EDGES[12][2] = {
        {2,0},{2,4},{2,1},{2,5}, // top to equatorial
        {3,0},{3,4},{3,1},{3,5}, // bottom to equatorial
        {0,4},{4,1},{1,5},{5,0}, // equatorial ring
    };
    for (auto& ed : EDGES) {
        out.push_back(pos + O[ed[0]]);
        out.push_back(pos + O[ed[1]]);
    }
    if (axisLen > 0.0f) {
        for (int a = 0; a < 3; a++) {
            out.push_back(pos);
            out.push_back(pos + glm::vec3(xform[a]) * axisLen);
        }
    }
}

// Project a world position to screen space and draw a boxed label.
void DrawWorldLabel(ImDrawList* dl, const glm::mat4& viewProj,
                           const glm::vec3& pos, int winW, int winH,
                           const char* text, ImU32 col) {
    glm::vec4 clip = viewProj * glm::vec4(pos, 1.0f);
    if (clip.w <= 0.0f) return;
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    if (std::abs(ndc.x) > 1.1f || std::abs(ndc.y) > 1.1f) return;
    float sx = (ndc.x * 0.5f + 0.5f) * (float)winW;
    float sy = (1.0f - (ndc.y * 0.5f + 0.5f)) * (float)winH;
    ImVec2 ts = ImGui::CalcTextSize(text);
    dl->AddRectFilled(
        ImVec2(sx - ts.x * 0.5f - 3.0f, sy - ts.y - 8.0f),
        ImVec2(sx + ts.x * 0.5f + 3.0f, sy - 4.0f),
        IM_COL32(20, 20, 28, 180), 3.0f);
    dl->AddText(ImVec2(sx - ts.x * 0.5f, sy - ts.y - 7.0f), col, text);
}

// Compile + link a program, reporting the driver's log instead of silently
// handing back a broken (black-screen) program object.
GLuint MakeProgram(const char* name, const char* vsSrc, const char* fsSrc) {
    auto compile = [&](GLenum stage, const char* src, const char* stageName) -> GLuint {
        GLuint sh = glCreateShader(stage);
        glShaderSource(sh, 1, &src, nullptr);
        glCompileShader(sh);
        GLint ok = GL_FALSE;
        glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[2048];
            glGetShaderInfoLog(sh, sizeof(log), nullptr, log);
            std::cerr << "[shader] " << name << " / " << stageName
                      << " failed to compile:\n" << log << std::endl;
        }
        return sh;
    };

    GLuint vs = compile(GL_VERTEX_SHADER,   vsSrc, "vertex");
    GLuint fs = compile(GL_FRAGMENT_SHADER, fsSrc, "fragment");
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint ok = GL_FALSE;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        std::cerr << "[shader] " << name << " failed to link:\n" << log << std::endl;
    }
    glDetachShader(prog, vs); glDeleteShader(vs);
    glDetachShader(prog, fs); glDeleteShader(fs);
    return prog;
}

// Samples a UV animation layer, returning (uScale, vScale, uOffset, vOffset).
// Linear between keyframes, which is the blend `ClimaxT1KeyFrameBlend` does.
glm::vec4 EvalUVAnim(const UVAnimClip& clip, size_t layer, float t) {
    if (layer >= clip.layers.size()) return glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);
    const auto& k = clip.layers[layer];
    if (k.empty()) return glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);
    if (k.size() == 1) return glm::vec4(k[0].uScale, k[0].vScale, k[0].uOff, k[0].vOff);

    const float dur = clip.duration > 0.0f ? clip.duration : k.back().time;
    const float tt = dur > 0.0f ? std::fmod(t, dur) : 0.0f;
    size_t i = 0;
    while (i + 1 < k.size() && k[i + 1].time <= tt) i++;
    const size_t j = std::min(i + 1, k.size() - 1);
    const float span = k[j].time - k[i].time;
    const float a = span > 1e-6f ? (tt - k[i].time) / span : 0.0f;
    return glm::vec4(glm::mix(k[i].uScale, k[j].uScale, a),
                     glm::mix(k[i].vScale, k[j].vScale, a),
                     glm::mix(k[i].uOff,   k[j].uOff,   a),
                     glm::mix(k[i].vOff,   k[j].vOff,   a));
}

} // namespace Viewer
} // namespace ClimaxEngine
