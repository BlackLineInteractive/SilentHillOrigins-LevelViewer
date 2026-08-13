#include "ClimaxEngine/Viewer/ViewerGraphics.h"

#include <algorithm>
#include "ClimaxEngine/Viewer/ViewerDraw.h"
#include "ClimaxEngine/Render/ViewerState.h"
#include "ClimaxEngine/Render/PlayerModel.h"
#include "ClimaxEngine/Rendering/CPURasterizer.h"
#include "ClimaxEngine/SG/SceneObject.h"
#include "ClimaxEngine/Render/GPUMesh.h"
#include "ClimaxEngine/Core/RWS/FileSystem/CArchiveManager.h"
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include "imgui.h"

void ReleaseAllGpuMeshes();
using namespace ClimaxEngine::Viewer;
extern std::map<std::string, GLuint> g_TextureMap;
extern CPURasterizer g_CPURasterizer;
void drawBatch(const std::vector<glm::vec3>& verts, float r, float g, float b, bool lineLoop = false);

extern glm::vec3 s_playerFeet;
extern float     s_playerYaw;
extern std::string s_playerClipName;
extern bool        s_showPlayerPieces;

namespace ClimaxEngine {
namespace Viewer {

bool ViewerGraphics::Init() {
const char* vS = R"(
#version 330 core
layout(location=0) in vec3 P;
layout(location=1) in vec2 T;
layout(location=2) in vec4 C;
layout(location=3) in vec4 W;
layout(location=4) in vec4 B;

out vec2  TC;
out vec4  VC;
out vec3  fragWorldPos;
uniform mat4  m;
uniform mat4  model;     // instance placement, identity for world geometry
uniform bool  flipU;
uniform bool  flipV;
uniform vec2  uvOffset;
uniform vec2  uvScale;
uniform bool  useSkinning;
uniform mat4  boneTransforms[128]; // Max 128 bones

void main(){
    vec4 localPos = vec4(P, 1.0);
    if(useSkinning) {
        float wSum = W.x + W.y + W.z + W.w;
        if(wSum > 0.001) {
            mat4 skinMat = boneTransforms[int(B.x)] * W.x +
                           boneTransforms[int(B.y)] * W.y +
                           boneTransforms[int(B.z)] * W.z +
                           boneTransforms[int(B.w)] * W.w;
            localPos = skinMat * localPos;
        }
    }

gl_Position  = m * localPos;
    fragWorldPos = vec3(model * localPos);
    vec2 coord = T;
    if(flipU) coord.x = 1.0 - coord.x;
    if(flipV) coord.y = 1.0 - coord.y;
    TC = (coord * uvScale) + uvOffset;
    VC = C;
}
)";

const char* fS = R"(
#version 330 core
out vec4 FragColor;
in vec2  TC;
in vec4  VC;
in vec3  fragWorldPos;
uniform sampler2D t;
uniform bool  useVertexColors;
uniform bool  untextured;
uniform bool  additive;
uniform bool  unlitGeometry;
uniform bool  alphaOff;
uniform vec4  matColor;
uniform float brightness;
uniform int   renderMode;
// 0=Textured 1=VertexColor 2=FlatShaded 3=Normals 4=Depth 5=Checker 6=Unlit
uniform vec3  eyePos;
uniform vec3  viewDir;
uniform float depthMax;
uniform bool  iceEffect;

// Placed lights, from the level's CColorLight objects. There are no normals in
// this vertex format, so this is radial falloff only -- the light reaches what
// is near it and fades over its own range. That is what CColorLight carries:
// a colour and a range, not a direction we could shade against.
const int MAX_LIGHTS = 16;
uniform int   lightCount;
uniform vec3  lightPos[MAX_LIGHTS];
uniform vec3  lightCol[MAX_LIGHTS];
uniform float lightRange[MAX_LIGHTS];
uniform int   lightType[MAX_LIGHTS];
uniform float lightIntensity;

uniform bool  enableFog;
uniform vec3  fogColor;
uniform float fogStart;
uniform float fogEnd;
uniform float fogDensity;
uniform int   fogMode;


void main(){
    vec3 dx = dFdx(fragWorldPos);
    vec3 dy = dFdy(fragWorldPos);
    vec3 N  = normalize(cross(dx, dy));

    if(renderMode == 1){
        // Vertex Color
        if(VC.a < 0.05) discard;
        FragColor = vec4(VC.rgb * brightness, VC.a);
    } else if(renderMode == 2){
        // Flat Shaded
        vec3 L    = normalize(vec3(0.55, 1.0, 0.45));
        float d   = max(dot(N, L), 0.0) * 0.72 + 0.28;
        FragColor = vec4(vec3(0.70, 0.72, 0.76) * d * brightness, 1.0);
    } else if(renderMode == 3){
        // Normals
        FragColor = vec4(N * 0.5 + 0.5, 1.0);
    } else if(renderMode == 4){
        // Depth
        float dist = abs(dot(fragWorldPos - eyePos, viewDir));
        float v    = clamp(1.0 - dist / depthMax, 0.0, 1.0);
        v = v * v;
        FragColor  = vec4(vec3(v), 1.0);
    } else if(renderMode == 5){
        // Checker
        vec2 ch = floor(TC * 8.0);
        float c = mod(ch.x + ch.y, 2.0) < 1.0 ? 0.82 : 0.18;
        FragColor = vec4(vec3(c), 1.0);
    } else if(renderMode == 6){
        // Unlit
        vec4 tex = texture(t, TC);
        if(tex.a < 0.1) discard;
        FragColor = vec4(tex.rgb * brightness, tex.a);
    } else if(untextured){
        // A material with no texture chunk: flat colour times vertex colour,
        // which is how the game shades it. Sampling the unbound sampler here
        // returned solid black instead.
        vec4 col = matColor;
        if(useVertexColors && !unlitGeometry) col *= VC;
        col.rgb *= brightness;
        if(col.a < 0.05) discard;
        FragColor = col;
    } else if(iceEffect){
        // Ice and water.
        //
        // The real look comes from the Wii's TEV stages, which the container
        // does not carry, so this is an approximation and not a decode: a
        // Fresnel rim plus a sharp specular highlight over the colour map,
        // with the surface normal taken from the screen-space derivatives the
        // flat-shading path already uses.
        vec4 tex = texture(t, TC);
        // A material whose blend op is NONE has no coverage channel: the
        // engine writes it with SRCBLEND ONE / DESTBLEND ZERO and never looks
        // at alpha. Character heads and bodies are all declared that way, and
        // their textures are barely opaque anywhere -- nurse_head has 1% opaque
        // texels and a third of it below this threshold -- so testing alpha on
        // them discarded the face and left the head bare.
        if(!alphaOff && tex.a < 0.02) discard;
        vec3 V = normalize(eyePos - fragWorldPos);
        vec3 L = normalize(vec3(0.45, 1.0, 0.35));
        vec3 Nf = faceforward(N, -V, N);
        float fres = pow(1.0 - clamp(dot(Nf, V), 0.0, 1.0), 3.0);
        float spec = pow(max(dot(reflect(-L, Nf), V), 0.0), 48.0);
        vec4 col = (useVertexColors && !unlitGeometry) ? tex * VC : tex;
        col.rgb  = mix(col.rgb, vec3(0.62, 0.78, 0.92), 0.35 * fres);
        col.rgb += vec3(0.55, 0.68, 0.80) * spec * 0.9;
        col.rgb += vec3(0.10, 0.16, 0.22) * fres;
        col.rgb *= brightness;
        FragColor = vec4(col.rgb, col.a);
    } else {
        // Textured (default, renderMode == 0)
        vec4 tex = texture(t, TC);
        // Discard only what is fully transparent. Cutting at 0.1 threw away the
        // whole soft edge of a gradient and left a hard jagged border where the
        // game fades out smoothly; the rest is handled by alpha blending.
        // A material whose blend op is NONE has no coverage channel: the
        // engine writes it with SRCBLEND ONE / DESTBLEND ZERO and never looks
        // at alpha. Character heads and bodies are all declared that way, and
        // their textures are barely opaque anywhere -- nurse_head has 1% opaque
        // texels and a third of it below this threshold -- so testing alpha on
        // them discarded the face and left the head bare.
        if(!alphaOff && tex.a < 0.02) discard;
        // Additive effect sheets carry their own brightness. Multiplying their
        // RGB by the baked vertex lighting drives them to black in a dark room,
        // which is why they only showed up with vertex colours switched off.
        //
        // Their vertex *alpha* is a different thing entirely: on the flame
        // sheets it runs the full 0..1 across the mesh, and it is the artist's
        // fade — the thing that stops a flame from ending in a hard polygon
        // edge. Dropping the whole vertex colour threw that away too, so the
        // fire came out as flat slabs with visible borders. Take the alpha and
        // leave the RGB alone: with SRC_ALPHA/ONE it scales the additive
        // contribution to nothing at the edges, without darkening the sheet.
        vec4 col;
        if(useVertexColors && !unlitGeometry)
            col = additive ? vec4(tex.rgb, tex.a * VC.a) : tex * VC;
        else
            col = tex;
        col.rgb *= brightness;
        if(alphaOff) col.a = 1.0;
        FragColor = col;
    }

    // Lit before fog, so fog sits on top of the lit colour the way it does on
    // the console. Modulating rather than adding keeps a texture's own colour:
    // an unlit room stays as its baked vertex colour, a lit one is brightened.
    // Only on surfaces with no baked light. A world surface already carries
    // its lighting in the vertex colours, so adding CColorLight on top of it
    // lights the room twice -- that is what turned HO_1_ExamRoom white.
    // `unlitGeometry` marks the pieces whose vertex colours are all zero: those
    // are the ones the engine has to light at run time, and they are what these
    // placed lights are for.
    if (lightCount > 0 && renderMode == 0 && unlitGeometry) {
        vec3 lit = vec3(0.0);
        for (int i = 0; i < lightCount; ++i) {
            // The type is in the data and used to be ignored, so a fill light
            // with a 1000-unit range was treated as a point light and drowned
            // everything near it. Type 2 is the room's ambient term: no
            // position, no falloff. The rest fall off over their own range.
            if (lightType[i] == 2) {
                lit += lightCol[i];
                continue;
            }
            float d = distance(fragWorldPos, lightPos[i]);
            float a = 1.0 - clamp(d / max(lightRange[i], 0.001), 0.0, 1.0);
            lit += lightCol[i] * a * a;
        }
        // Clamped, then scaled. The first version multiplied the surface by
        // the raw sum, so two lights could more than double it and the room
        // came out white. A placed light in this game lifts a dark corner; it
        // does not relight the scene.
        lit = clamp(lit, 0.0, 1.0) * lightIntensity;
        FragColor.rgb += FragColor.rgb * lit;
    }

    if (enableFog && renderMode != 4 && renderMode != 3 && renderMode != 5) {
        float dist = abs(dot(fragWorldPos - eyePos, viewDir));
        float f = 1.0;
        if (fogMode == 0) {
            f = (fogEnd - dist) / (fogEnd - fogStart);
        } else if (fogMode == 1) {
            f = exp(-fogDensity * dist);
        } else if (fogMode == 2) {
            f = exp(-pow(fogDensity * dist, 2.0));
        }
        f = clamp(f, 0.0, 1.0);
        FragColor.rgb = mix(fogColor, FragColor.rgb, f);
    }
}
)";

    p = MakeProgram("scene", vS, fS);

    // ---- Solid-colour shader (collision wireframe + clump markers) ----
const char* colvS = R"(
#version 330 core
layout(location=0) in vec3 P;
uniform mat4 m;
void main(){ gl_Position = m * vec4(P, 1.0); }
)";
const char* colfS = R"(
#version 330 core
out vec4 FragColor;
uniform vec4 solidColor;
void main(){ FragColor = solidColor; }
)";
    collProg = MakeProgram("solid", colvS, colfS);

    // ---- Sky / gradient background shader (fullscreen quad via gl_VertexID) ----
const char* skyVS = R"(
#version 330 core
out vec2 fragY;
void main(){
    // Two-triangle fullscreen quad from vertex id 0-5
    vec2 pos[6] = vec2[6](
        vec2(-1,-1),vec2(1,-1),vec2(1,1),
        vec2(-1,-1),vec2(1,1), vec2(-1,1));
gl_Position = vec4(pos[gl_VertexID], 0.0, 1.0);
    fragY = pos[gl_VertexID] * 0.5 + 0.5;  // 0=bottom 1=top
}
)";
const char* skyFS = R"(
#version 330 core
in  vec2 fragY;
out vec4 FragColor;
uniform vec3 skyTop;
uniform vec3 skyBot;
void main(){
    FragColor = vec4(mix(skyBot, skyTop, fragY.y), 1.0);
}
)";
    skyProg = MakeProgram("sky", skyVS, skyFS);
    // Empty VAO required by core profile for attributeless draws
    glGenVertexArrays(1, &skyVao);

    // Persistent line buffer for clump / game-object markers
    glGenVertexArrays(1, &markerVao);
glGenBuffers(1, &markerVbo);
glBindVertexArray(markerVao);
glBindBuffer(GL_ARRAY_BUFFER, markerVbo);
glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, nullptr);
glEnableVertexAttribArray(0);
glBindVertexArray(0);

    return true;
}

void ViewerGraphics::RenderFrame(int fbW, int fbH, int winW, int winH, const glm::mat4& mvp, const glm::vec3& eye, const glm::vec3& viewDir, size_t totalMeshes) {
    bool haveModel = totalMeshes > 0;
    if (state.renderDevice == RenderDevice::CPU) {
        // --- CPU Software Rasterization Pass ---
        g_CPURasterizer.Init(fbW, fbH);
        g_CPURasterizer.Clear(state.skyColorBot[0], state.skyColorBot[1], state.skyColorBot[2], 1.0f);
        if (haveModel) {
        g_CPURasterizer.RenderScene(ClimaxEngine::SG::CSceneObjectRegistrar::GetInstance().GetObjects(), mvp, eye);
        }
        g_CPURasterizer.PresentOnScreen();
    } else {
        // --- GPU Hardware Acceleration Pass (OpenGL 3.3 / Metal) ---
        glViewport(0, 0, fbW, fbH);
        glClearColor(state.skyColorBot[0], state.skyColorBot[1], state.skyColorBot[2], 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        // Draw gradient sky before any geometry
        if (state.skyGradient) {
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glUseProgram(skyProg);
        glUniform3fv(glGetUniformLocation(skyProg, "skyTop"), 1, state.skyColorTop);
        glUniform3fv(glGetUniformLocation(skyProg, "skyBot"), 1, state.skyColorBot);
        glBindVertexArray(skyVao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        }
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        if (state.showWireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        else             glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        GLint filter = state.linearFilter ? GL_LINEAR : GL_NEAREST;
        for (auto const& [name, id] : g_TextureMap) {
        glBindTexture(GL_TEXTURE_2D, id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
        if (state.forceRepeat) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        }
        }

        glUseProgram(p);
        glUniformMatrix4fv(glGetUniformLocation(p, "m"),    1, GL_FALSE, glm::value_ptr(mvp));
        glUniform1i(glGetUniformLocation(p, "flipU"),    state.flipU);
        glUniform1i(glGetUniformLocation(p, "flipV"),    state.flipV);
        glUniform2f(glGetUniformLocation(p, "uvOffset"),     state.uvOffsetX, state.uvOffsetY);
        glUniform2f(glGetUniformLocation(p, "uvScale"),      state.uvScaleX,  state.uvScaleY);
        glUniform1i(glGetUniformLocation(p, "useVertexColors"), state.useVertexColors);
        glUniform1f(glGetUniformLocation(p, "brightness"),   state.brightness);
        glUniform1i(glGetUniformLocation(p, "renderMode"),   (int)state.renderMode);
        glUniform3f(glGetUniformLocation(p, "eyePos"),       eye.x, eye.y, eye.z);
        glUniform1f(glGetUniformLocation(p, "depthMax"),     state.camDist * 4.5f);
        // The lights the level placed. Gathered by ViewerApp because it is the
        // one that holds the object list.
        {
            const int n = (int)std::min<size_t>(lightPos.size(), 16);
            glUniform1i(glGetUniformLocation(p, "lightCount"), state.enableLights ? n : 0);
            if (n > 0) {
                glUniform3fv(glGetUniformLocation(p, "lightPos"),   n, &lightPos[0].x);
                glUniform3fv(glGetUniformLocation(p, "lightCol"),   n, &lightCol[0].x);
                glUniform1fv(glGetUniformLocation(p, "lightRange"), n, &lightRange[0]);
                glUniform1iv(glGetUniformLocation(p, "lightType"),  n, &lightType[0]);
                glUniform1f(glGetUniformLocation(p, "lightIntensity"), state.lightIntensity);
            }
        }

        const GLint uM     = glGetUniformLocation(p, "m");
        const GLint uModel  = glGetUniformLocation(p, "model");

        // Advance animation time for all objects playing a clip
        static size_t s_lastLoadChunkCount = 0;
        static bool s_debugPrinted = false;
        if (totalMeshes != s_lastLoadChunkCount) {
        s_lastLoadChunkCount = totalMeshes;
        s_debugPrinted = false;
        }
        if (!s_debugPrinted && totalMeshes > 0) {
        std::cerr << "[render] first frame: meshes=" << totalMeshes
              << " sections=" << g_ShoSections.size()
              << " gameObjects=" << g_GameObjects.size() << "\n";
        std::cerr.flush();
        }
        float dt = ImGui::GetIO().DeltaTime;
        if (state.uvAnimRun) state.uvAnimTime += dt * state.uvAnimSpeed;
        // Every clump shares the transport in the Playback panel.
        for (auto& o : ClimaxEngine::SG::CSceneObjectRegistrar::GetInstance().GetObjects())
        if (auto cl = std::dynamic_pointer_cast<ClimaxEngine::SG::CClumpObject>(o))
            cl->animTime += dt * state.animSpeed;
        for (auto& go : g_GameObjects) {
        if (go.currentClipIndex >= 0 && go.currentClipIndex < (int)go.clipSectionIndices.size()) {
            go.animTime += dt;
        } // No auto-start: user picks clip from UI
        }
        if (!s_debugPrinted && totalMeshes > 0) {
        std::cerr << "[render] after anim loop OK\n";
        std::cerr.flush();
        }
    const GLint uUntex  = glGetUniformLocation(p, "untextured");
    const GLint uAdd    = glGetUniformLocation(p, "additive");
    const GLint uUnlit  = glGetUniformLocation(p, "unlitGeometry");
    const GLint uAlphaOff = glGetUniformLocation(p, "alphaOff");
    const GLint uUvOff  = glGetUniformLocation(p, "uvOffset");
    const GLint uUvScl  = glGetUniformLocation(p, "uvScale");
    const GLint uMatCol = glGetUniformLocation(p, "matColor");
    const GLint uIce    = glGetUniformLocation(p, "iceEffect");
        // Two passes: opaque first with depth writes on, blended second
        // with them off. A blended surface that writes depth hides whatever
        // stands behind it, which is why one semi-transparent sheet made the
        // next one disappear.
        
        ClimaxEngine::SG::RenderContext ctx;
        ctx.viewProj = mvp;
        ctx.eye = eye;
        ctx.uM = uM;
        ctx.uModel = uModel;
        ctx.uUntex = uUntex;
        ctx.uAdd = uAdd;
        ctx.uUnlit = uUnlit;
        ctx.uMatCol = uMatCol;
        ctx.uIce = uIce;
        ctx.uUseSkinning = glGetUniformLocation(p, "useSkinning");
        ctx.textureMap = &g_TextureMap;
        
        for (int pass = 0; pass < 2; pass++) {
        ctx.pass = pass;
        
        auto& registrar = ClimaxEngine::SG::CSceneObjectRegistrar::GetInstance();
        for (auto& obj : registrar.GetObjects()) {
            auto meshes = obj->GetMeshes();
            for (auto* chunkPtr : meshes) {
            const auto& chunk = *chunkPtr;
            
            if (chunk.alphaPass) continue;
            
            // Effect sheets must never occlude each other.
            //
            // Which pass a mesh belongs in is decided by the blend
            // mode the material itself declares, not by how its
            // texture happens to be named. An additive or
            // subtractive sheet is authored so that black drops
            // out, so it carries no transparent texels at all --
            // FX_fire_Dahlia is 99% opaque. Both the FX_ prefix and
            // the measured alpha gradient therefore miss it, and it
            // lands in the opaque pass and draws as a solid
            // rectangle. Blood_Pool_SUB and 42 other textures fail
            // the name test the same way.
            //
            // The name and gradient tests stay as a fallback for
            // materials that declare mode 0 but still need sorting.
            auto itG0 = g_TexGradient.find(chunk.texName);
            const bool isFx = chunk.texName.size() > 3 &&
                sho_strnicmp(chunk.texName.c_str(), "FX_", 3) == 0;
            // The engine masks this to 16 bits too -- the world
            // pipe setup does `andi $s3, $v0, 0xffff` right after
            // reading the field.
            const uint32_t declared = chunk.blendMode & 0xFFFF;
            // Mode 3 is the engine's NONE op: SRCBLEND ONE,
            // DESTBLEND ZERO. The surface is written straight out
            // and its alpha is not a coverage channel at all, so it
            // belongs in the opaque pass whatever its texture looks
            // like.
            const bool alphaOff = (declared == 3);
            const bool blended = !alphaOff &&
                (declared == 1 || declared == 2 ||
                 isFx || chunk.additive ||
                 (itG0 != g_TexGradient.end() && itG0->second));
            if ((pass == 0) == blended) continue;
            
            const std::string& tName =
                (state.frozenVariant && !chunk.altTexName.empty())
                ? chunk.altTexName : chunk.texName;
            GLuint tid = 0;
            if (g_TextureMap.count(tName)) tid = g_TextureMap[tName];
            if (!tid) {
                std::string alt = tName;
                for (auto& ch : alt) ch = (char)toupper((unsigned char)ch);
                if (g_TextureMap.count(alt)) tid = g_TextureMap[alt];
            }
            if (!tid) {
                std::string alt = tName;
                for (auto& ch : alt) ch = (char)tolower((unsigned char)ch);
                if (g_TextureMap.count(alt)) tid = g_TextureMap[alt];
            }
            
            if (chunk.untextured) continue;
            
            // Blend mode comes from the material itself -- the
            // 0x0A01 extension field the engine reads through
            // ClimaxT1MaterialGetFrameBlendMode. 0 is standard
            // alpha, 1 additive, 2 subtractive.
            //
            // This replaces a hand-maintained list of FX_ name
            // prefixes. The list could never be right: FX_TV and
            // FX_save_point1 are standard alpha while FX_Flare_01
            // is additive, and nothing in their names says so. The
            // field also carries a third mode the list had no way
            // to express -- subtractive, used by the blood decals,
            // two of which spell SUB in the texture name.
            const uint32_t blend = chunk.blendMode & 0xFFFF;
            const bool addNow = (blend == 1);
            const bool subNow = (blend == 2);

            glUniform1i(uAdd, addNow ? 1 : 0);
            glUniform1i(uUnlit, chunk.unlitGeometry ? 1 : 0);
            glUniform1i(uIce, (chunk.iceEffect && state.iceShading) ? 1 : 0);

            if (addNow) {
                // ClimaxT1AtomicSetAlphaOpADD sets SRCBLEND to
                // rwBLENDSRCALPHA (5) and DESTBLEND to rwBLENDONE
                // (2) -- the source is scaled by its own alpha
                // before being added. GL_ONE, GL_ONE adds the full
                // colour instead, which is what made the headlight
                // beams read as flat white.
                glBlendEquation(GL_FUNC_ADD);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                glDepthMask(GL_FALSE);
            } else if (subNow) {
                // SUB sets the same two blend factors as ADD and
                // changes the equation instead.
                glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                glDepthMask(GL_FALSE);
            } else if (alphaOff) {
                glBlendEquation(GL_FUNC_ADD);
                glBlendFunc(GL_ONE, GL_ZERO);
                glDepthMask(GL_TRUE);
            } else {
                glBlendEquation(GL_FUNC_ADD);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDepthMask(pass == 0 ? GL_TRUE : GL_FALSE);
            }
            glUniform1i(uAlphaOff, alphaOff ? 1 : 0);

            // UV animation. The clip's offsets run to whole texture
            // units over its duration (-28.0 after 28 s), so with
            // WRAP addressing the end of the loop samples exactly
            // what the start does and the cycle is seamless.
            {
                glm::vec4 uv(state.uvScaleX, state.uvScaleY,
                     state.uvOffsetX, state.uvOffsetY);
                if (!chunk.uvAnimName.empty()) {
                auto itA = g_UVAnims.find(chunk.uvAnimName);
                if (itA != g_UVAnims.end())
                    uv = EvalUVAnim(itA->second, 0, state.uvAnimTime);
                }
                glUniform2f(uUvScl, uv.x, uv.y);
                glUniform2f(uUvOff, uv.z, uv.w);
            }

            glBindTexture(GL_TEXTURE_2D, tid);
            glUniform1i(uUntex, (chunk.untextured || tid == 0) ? 1 : 0);
            glUniform4fv(uMatCol, 1, glm::value_ptr(chunk.matColor));
            
            // Delegate drawing to the object which handles transformations
            obj->SetMatrixAndDraw(ctx, chunkPtr);
            }
        }
        } // opaque pass, then blended pass

        // ── Travis ──────────────────────────────────────────────────────
        // Drawn after the level and outside the two passes: he is opaque,
        // and his textures are not in g_TextureMap -- the model owns them
        // so that loading the next room cannot delete them.
        //
        // Drawn through the objects' own SetMatrixAndDraw, so the skeleton,
        // the clip and the skinning all run on the existing path. Placing
        // him is just setting the transform they already multiply by.
        if (state.playMode && state.autoCameras && g_Player.loaded) {
        glm::mat4 model = glm::translate(glm::mat4(1.0f), s_playerFeet);
        model = glm::rotate(model, glm::radians(s_playerYaw),
                    glm::vec3(0, 1, 0));

        glUniform1i(uAlphaOff, 0);
        glUniform1i(uAdd, 0);
        glUniform1i(uIce, 0);
        glUniform2f(uUvScl, 1.0f, 1.0f);
        glUniform2f(uUvOff, 0.0f, 0.0f);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_TRUE);

        // The body first: its pose has to exist before anything can
        // ride one of its bones.
        auto drawObject = [&](size_t i, const glm::mat4& xform) {
            auto& obj = g_Player.objects[i];
            obj->SetTransform(xform);
            for (auto* chunkPtr : obj->GetMeshes()) {
            const MeshChunk& m = *chunkPtr;
            // Effect sheets, particle sheets and the untextured
            // shadow proxy are in the container but are not Travis.
            if (!IsPlayerBodyMesh(m, g_Player.textures)) continue;

            const GLuint tid = PlayerTextureId(g_Player.textures, m.texName);
            glBindTexture(GL_TEXTURE_2D, tid);
            glUniform1i(uUntex, (m.untextured || tid == 0) ? 1 : 0);
            glUniform1i(uUnlit, m.unlitGeometry ? 1 : 0);
            glUniform4fv(uMatCol, 1, glm::value_ptr(m.matColor));
            obj->SetMatrixAndDraw(ctx, chunkPtr);
            }
        };

        if (g_Player.bodyObject >= 0)
            drawObject((size_t)g_Player.bodyObject, model);

        // Then the attachments, each carried by the bone it rides.
        // The delta is the same one the rigid path uses: where the bone
        // is now, against where it rested when the piece was authored.
        auto* body = g_Player.bodyObject >= 0
            ? dynamic_cast<ClimaxEngine::SG::CClumpObject*>(
              g_Player.objects[(size_t)g_Player.bodyObject].get())
            : nullptr;
        for (size_t i = 0; i < g_Player.objects.size(); ++i) {
            if ((int)i == g_Player.bodyObject) continue;
            glm::mat4 xform = model;
            const int b = i < g_Player.attachBone.size()
                      ? g_Player.attachBone[i] : -1;
            if (body && b >= 0 &&
            b < (int)body->currentBoneMats.size() &&
            b < (int)body->restBoneMats.size())
            xform = model * body->currentBoneMats[(size_t)b] *
                glm::inverse(body->restBoneMats[(size_t)b]);
            drawObject(i, xform);
        }
        }

        glUniformMatrix4fv(uM, 1, GL_FALSE, glm::value_ptr(mvp));
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_TRUE);
        glBindVertexArray(0);
        if (!s_debugPrinted) {
        std::cerr << "[render] chunk loop finished OK\n";
        std::cerr.flush();
        s_debugPrinted = true;
        }
    }

    // --- Collision render pass (solid fill + wireframe) ---
    if (state.showCollision && GpuPeek(g_Collision) && !g_Collision.indices.empty()) {
        glUseProgram(collProg);
        glUniformMatrix4fv(glGetUniformLocation(collProg, "m"), 1, GL_FALSE, glm::value_ptr(mvp));
        glDisable(GL_CULL_FACE);
        glBindVertexArray(GpuFor(g_Collision).vao);

        if (state.showCollisionSolid) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glUniform4f(glGetUniformLocation(collProg, "solidColor"), 0.10f, 0.80f, 0.20f, 0.28f);
        glDrawElements(GL_TRIANGLES, (GLsizei)g_Collision.indices.size(), GL_UNSIGNED_INT, nullptr);
        }

        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glLineWidth(1.4f);
        glUniform4f(glGetUniformLocation(collProg, "solidColor"), 0.15f, 0.95f, 0.30f, 0.85f);
        glDrawElements(GL_TRIANGLES, (GLsizei)g_Collision.indices.size(), GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);

        if (!state.showWireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        // Restore, don't enable: culling is off for the whole viewer (the PS2
        // strips have inconsistent winding). Unconditionally enabling it here
        // made half the level vanish from the frame after collision was first
        // switched on, and it never came back.
        glDisable(GL_CULL_FACE);
        glLineWidth(1.0f);
    }

    // --- Object markers: CLUMPs and placed 0x0704 game objects ---
    // Both batches share one persistent VBO; this used to allocate and then
    // destroy a VAO + VBO on every single frame.
    {
        auto* fdl = ImGui::GetForegroundDrawList();

        auto drawBatch = [&](const std::vector<glm::vec3>& verts, float r, float g, float b) {
        if (verts.empty()) return;
        glBindVertexArray(markerVao);
        glBindBuffer(GL_ARRAY_BUFFER, markerVbo);
        glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(verts.size() * sizeof(glm::vec3)),
                 verts.data(), GL_DYNAMIC_DRAW);
        glUseProgram(collProg);
        glUniformMatrix4fv(glGetUniformLocation(collProg, "m"), 1, GL_FALSE, glm::value_ptr(mvp));
        glUniform4f(glGetUniformLocation(collProg, "solidColor"), r, g, b, 1.0f);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glLineWidth(2.0f);
        glDrawArrays(GL_LINES, 0, (GLsizei)verts.size());
        glLineWidth(1.0f);
        glBindVertexArray(0);
        };

        std::vector<glm::vec3> lineVerts;

        if (state.showClumps && !g_Clumps.empty()) {
        lineVerts.clear();
        lineVerts.reserve(g_Clumps.size() * 30);
        for (const auto& cl : g_Clumps)
            AppendMarker(lineVerts, cl.position, cl.transform, 0.35f, 0.5f);
        drawBatch(lineVerts, 1.0f, 0.72f, 0.10f);

        if (state.showObjectLabels)
            for (const auto& cl : g_Clumps)
            DrawWorldLabel(fdl, mvp, cl.position, winW, winH,
                       cl.label.c_str(), IM_COL32(255, 192, 40, 255));
        }

        if (state.showGameObjects && !g_GameObjects.empty()) {
        lineVerts.clear();
        lineVerts.reserve(g_GameObjects.size() * 30);
                for (const auto& go : g_GameObjects) {
            // Non-spatial objects (CZone, GameMessage, …) carry identity;
            // drawing them all stacked on the origin is just noise.
            if (go.atOrigin && !state.showOriginObjects) continue;
            AppendMarker(lineVerts, go.position, go.transform, 0.22f, 0.45f);
        }
        drawBatch(lineVerts, 0.35f, 0.85f, 1.0f);
        

        if (state.showObjectLabels) {
            for (const auto& go : g_GameObjects) {
            if (go.atOrigin && !state.showOriginObjects) continue;
            if (go.className == "SavePoint") continue; // We know what it is
            DrawWorldLabel(fdl, mvp, go.position, winW, winH,
                       go.label.c_str(), IM_COL32(120, 216, 255, 255));
            }
        }
        }

        if (state.showBoneOverlay) {
        lineVerts.clear();
        for (const auto& obj : ClimaxEngine::SG::CSceneObjectRegistrar::GetInstance().GetObjects()) {
            if (auto clump = std::dynamic_pointer_cast<ClimaxEngine::SG::CClumpObject>(obj)) {
            if (clump->skeleton.bones.empty() || clump->currentBoneMats.empty()) continue;
            const glm::mat4& clumpMat = clump->GetTransform();
            for (size_t b = 0; b < clump->skeleton.bones.size(); ++b) {
                glm::mat4 worldMat = clumpMat * clump->currentBoneMats[b];
                glm::vec3 pos = glm::vec3(worldMat[3]);
                AppendMarker(lineVerts, pos, worldMat, 0.02f, 0.05f);
                
                int parent = clump->skeleton.bones[b].parent;
                if (parent >= 0 && parent < (int)b) {
                glm::vec3 parentPos = glm::vec3(clumpMat * clump->currentBoneMats[parent][3]);
                lineVerts.push_back(parentPos);
                lineVerts.push_back(pos);
                }
            }
            }
        }
        if (!lineVerts.empty()) {
            drawBatch(lineVerts, 1.0f, 0.0f, 1.0f); // Magenta for bones
        }
        }
    }

    glUseProgram(p);


}

void ViewerGraphics::Shutdown() {
glDeleteVertexArrays(1, &skyVao);
glDeleteVertexArrays(1, &markerVao);
glDeleteBuffers(1, &markerVbo);
ClimaxEngine::RWS::FileSystem::CArchiveManager::GetInstance().UnmountAll();

for (auto& [name, id] : g_TextureMap) glDeleteTextures(1, &id);
ReleaseAllGpuMeshes();

}

} // namespace Viewer
} // namespace ClimaxEngine
