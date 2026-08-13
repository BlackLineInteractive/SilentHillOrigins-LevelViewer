#include "ClimaxEngine/Viewer/ViewerApp.h"
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <fstream>

#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "ImGuizmo.h"
#include "im_anim.h"
#include "ClimaxEngine/Game/CameraLinks.h"
#include "ClimaxEngine/Game/CharacterController.h"
#include "ClimaxEngine/Game/ZoneLinks.h"
#include "ClimaxEngine/Game/ButtonTriggers.h"
#include "ClimaxEngine/Render/PlayerModel.h"
#include "ClimaxEngine/Viewer/ViewerDraw.h"

void InstallGLTextureSink();

#include "ClimaxEngine/Core/RWS/FileSystem/CArchiveManager.h"
#include "ClimaxEngine/Core/Common.h"
#include "ClimaxEngine/Render/GPUMesh.h"
#include "ClimaxEngine/SG/SceneObject.h"
#include "ClimaxEngine/Loader/Export.h"
#include "ClimaxEngine/Loader/Loader.h"
#include "ClimaxEngine/Rendering/CPURasterizer.h"
#include "ClimaxEngine/UI/UI.h"
#include "ClimaxEngine/Platform/PS2/AudioParser.h"

// ---------------------------------------------------------------------------
// Audio playback
//
// One SDL device at a time, reopened whenever the next clip has a different
// rate or channel count -- the level banks alone span 6 kHz mono to 32 kHz,
// and the cutscene streams are 48 kHz stereo.
// ---------------------------------------------------------------------------
#include "ClimaxEngine/Platform/PS2/RwsAudio.h"
#include "ClimaxEngine/Viewer/ViewerAudio.h"

// ---------------------------------------------------------------------------
// Prefs: persist the last opened .arc path so the next launch auto-mounts it.
// File: <basePath>/ClimaxGameEngineToolkit.prefs  (one line = arc path)
// ---------------------------------------------------------------------------
static std::string g_PrefsPath;

static void InitPrefsPath() {
    // SDL_GetBasePath() returns the directory containing the executable.
    char* base = SDL_GetBasePath();
    if (base) {
        g_PrefsPath = std::string(base) + "ClimaxGameEngineToolkit.prefs";
        SDL_free(base);
    } else {
        g_PrefsPath = "ClimaxGameEngineToolkit.prefs";
    }
}

void SaveArcPref(const std::string& arcPath) {
    if (g_PrefsPath.empty() || arcPath.empty()) return;
    std::ofstream f(g_PrefsPath, std::ios::trunc);
    if (f) f << arcPath << "\n";
}

// Not static: the file browser uses it to work out where to open.
std::string LoadArcPref() {
    if (g_PrefsPath.empty()) return {};
    std::ifstream f(g_PrefsPath);
    std::string line;
    if (f && std::getline(f, line) && !line.empty()) return line;
    return {};
}

// ── Doorway state, shared between the step and the overlay ───────────────────
// Which ZoneLink the player is standing in, and the container it leads to. The
// overlay reads them to draw the prompt; the step writes them.
static int         s_zoneLinkHere = -1;
static std::string s_zonePrompt;

// ── MessageBox and SavePoint state ───────────────────────────────────────────
static bool        s_showMessageBox = false;
static std::string s_messageBoxText;
static float       s_messageBoxTimer = 0.0f;
static bool        s_showSaveMenu = false;

// Set by the key handler, consumed and cleared by the step, so a single press
// travels once instead of once per frame it is held.
static bool        s_useDoorPressed = false;

// Where to stand Travis and which way to turn him. Written by the walk step,
// read by the draw pass.
glm::vec3 s_playerFeet = glm::vec3(0.0f);
float     s_playerYaw  = 0.0f;
std::string s_playerClipName;
bool        s_showPlayerPieces = false;

// Build a view matrix from orbit parameters and return camera world position
static glm::mat4 BuildView(glm::vec3& outEye) {
    float yRad = glm::radians(state.camYaw);
    float pRad = glm::radians(glm::clamp(state.camPitch, -89.0f, 89.0f));

    if (state.useWASD) {
        outEye = glm::vec3(state.camPosX, state.camPosY, state.camPosZ);
        glm::vec3 fwd(
             -cosf(pRad) * sinf(yRad),
             -sinf(pRad),
             -cosf(pRad) * cosf(yRad)
        );
        return glm::lookAt(outEye, outEye + fwd, glm::vec3(0, 1, 0));
    }

    float dist = std::max(state.camDist, 0.1f);

    glm::vec3 target(state.camTargetX, state.camTargetY, state.camTargetZ);
    glm::vec3 offset(
         dist * cosf(pRad) * sinf(yRad),
         dist * sinf(pRad),
         dist * cosf(pRad) * cosf(yRad)
    );
    outEye = target + offset;
    return glm::lookAt(outEye, target, glm::vec3(0, 1, 0));
}

// The five helpers that used to sit here are in src/Viewer/ViewerDraw.cpp now;
// they were the only free functions above main that read nothing but their
// arguments. Pulled in by name so the call sites below are untouched.
using ClimaxEngine::Viewer::AppendMarker;
using ClimaxEngine::Viewer::DrawOrbitSphere;
using ClimaxEngine::Viewer::DrawWorldLabel;
using ClimaxEngine::Viewer::EvalUVAnim;
using ClimaxEngine::Viewer::MakeProgram;

using namespace ClimaxEngine::Viewer;

int ViewerApp::Run(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    // The shaders are `#version 330 core`, so ask for a matching context instead
    // of taking whatever the driver defaults to (a compatibility/2.1 context on
    // macOS, where every shader would then fail to compile).
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window* win = SDL_CreateWindow("Climax Silent Hill Engine Toolkit 0.6  -  game pre-alpha 0.0.1.2",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!win) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    if (!ctx) {
        std::cerr << "SDL_GL_CreateContext failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(win); SDL_Quit();
        return 1;
    }
    SDL_GL_SetSwapInterval(1);

    glewExperimental = GL_TRUE;   // required for a core profile context
    GLenum glewErr = glewInit();
    if (glewErr != GLEW_OK) {
        std::cerr << "glewInit failed: " << glewGetErrorString(glewErr) << std::endl;
        return 1;
    }
    glGetError();   // swallow the spurious INVALID_ENUM glewExperimental produces

    // The decoder hands finished pixels to a sink; this is the one that puts
    // them on the GPU. It must be installed before any container is read.
    InstallGLTextureSink();

    // Initialise prefs now that SDL_GetBasePath() is available
    InitPrefsPath();

    // Load only once a GL context exists — LoadLevel() uploads buffers and
    // textures, and it used to run before SDL was even initialised.
    //
    //   ClimaxGameEngineToolkit SH.ARC [LevelName]        — mount the archive, load by name
    //   ClimaxGameEngineToolkit <container> [txd ...]     — loose files; TXDs are optional
    if (argc >= 2) {
        const std::string first = argv[1];
        const bool looksLikeArc =
            first.size() > 4 && sho_stricmp(first.c_str() + first.size() - 4, ".arc") == 0;

        if (looksLikeArc && ClimaxEngine::RWS::FileSystem::CArchiveManager::GetInstance().Mount(first)) {
            ScanAudioLibrary();
            SaveArcPref(first);                         // ← remember for next launch
            std::cerr << "[arc] mounted " << first << " ("
                      << ClimaxEngine::RWS::FileSystem::CArchiveManager::GetInstance().GetFirstArchive()->Entries().size() << " files)\n";
            if (argc >= 3 && std::string(argv[2]) != "--export") {
                const int idx = ClimaxEngine::RWS::FileSystem::CArchiveManager::GetInstance().GetFirstArchive()->Find(argv[2]);
                if (idx >= 0) LoadLevelFromArc(idx);
                else std::cerr << "[arc] no entry named '" << argv[2] << "'\n";
            }
        } else {
            if (looksLikeArc)
                std::cerr << "[arc] " << ClimaxEngine::RWS::FileSystem::CArchiveManager::GetInstance().GetFirstArchive()->Error() << " — treating as a container\n";
            std::vector<std::string> txds;
            for (int i = 2; i < argc; i++) txds.push_back(argv[i]);
            LoadLevel(first, txds);
        }
    } else {
        // No CLI argument: try to auto-mount the last opened archive
        const std::string saved = LoadArcPref();
        if (!saved.empty()) {
            if (ClimaxEngine::RWS::FileSystem::CArchiveManager::GetInstance().Mount(saved)) {
                ScanAudioLibrary();
                std::cerr << "[arc] auto-mounted last arc: " << saved
                          << " (" << ClimaxEngine::RWS::FileSystem::CArchiveManager::GetInstance().GetFirstArchive()->Entries().size() << " files)\n";
                state.showArc = true;   // open the archive browser automatically
            } else {
                std::cerr << "[arc] prefs arc no longer accessible: " << saved << "\n";
            }
        }
    }

    IMGUI_CHECKVERSION(); ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // don't write imgui.ini

    // ---- Theme ----
    //
    // Everything used to be the same grey, so nothing told you what was on,
    // what was interactive and what was just a label. One warm accent now
    // carries state -- checkmarks, slider grabs, the active tab, selection --
    // against a cool neutral chrome, which is enough hierarchy without turning
    // the tool into a paintbox.
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding    = 7.0f;
    style.ChildRounding     = 5.0f;
    style.FrameRounding     = 5.0f;
    style.PopupRounding     = 5.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding      = 5.0f;
    style.TabRounding       = 5.0f;
    style.WindowPadding     = ImVec2(12, 11);
    style.FramePadding      = ImVec2(8, 5);
    style.ItemSpacing       = ImVec2(8, 7);
    style.ItemInnerSpacing  = ImVec2(7, 5);
    style.IndentSpacing     = 18.0f;
    style.ScrollbarSize     = 12.0f;
    style.GrabMinSize       = 11.0f;
    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 0.0f;
    style.SeparatorTextBorderSize = 2.0f;
    style.SeparatorTextPadding    = ImVec2(18, 4);

    // C++ blue.
    const ImVec4 accent      = ImVec4(0.00f, 0.41f, 0.71f, 1.00f);
    const ImVec4 accentHi    = ImVec4(0.16f, 0.58f, 0.89f, 1.00f);
    const ImVec4 accentDim   = ImVec4(0.04f, 0.22f, 0.39f, 1.00f);

    ImVec4* c = style.Colors;
    c[ImGuiCol_WindowBg]          = ImVec4(0.075f, 0.075f, 0.085f, 0.98f);
    c[ImGuiCol_ChildBg]           = ImVec4(0.055f, 0.055f, 0.065f, 0.92f);
    c[ImGuiCol_PopupBg]           = ImVec4(0.085f, 0.085f, 0.098f, 0.99f);
    c[ImGuiCol_Border]            = ImVec4(0.24f, 0.24f, 0.28f, 0.60f);
    c[ImGuiCol_FrameBg]           = ImVec4(0.135f, 0.135f, 0.155f, 1.00f);
    c[ImGuiCol_FrameBgHovered]    = ImVec4(0.195f, 0.195f, 0.225f, 1.00f);
    c[ImGuiCol_FrameBgActive]     = ImVec4(0.245f, 0.245f, 0.285f, 1.00f);
    c[ImGuiCol_TitleBg]           = ImVec4(0.065f, 0.065f, 0.075f, 1.00f);
    c[ImGuiCol_TitleBgActive]     = ImVec4(0.105f, 0.105f, 0.125f, 1.00f);
    c[ImGuiCol_MenuBarBg]         = ImVec4(0.095f, 0.095f, 0.110f, 1.00f);
    c[ImGuiCol_Header]            = ImVec4(accentDim.x, accentDim.y, accentDim.z, 0.55f);
    c[ImGuiCol_HeaderHovered]     = ImVec4(accent.x, accent.y, accent.z, 0.45f);
    c[ImGuiCol_HeaderActive]      = ImVec4(accent.x, accent.y, accent.z, 0.62f);
    c[ImGuiCol_Button]            = ImVec4(0.165f, 0.165f, 0.195f, 1.00f);
    c[ImGuiCol_ButtonHovered]     = ImVec4(0.245f, 0.245f, 0.285f, 1.00f);
    c[ImGuiCol_ButtonActive]      = ImVec4(accent.x, accent.y, accent.z, 0.75f);
    c[ImGuiCol_SliderGrab]        = accent;
    c[ImGuiCol_SliderGrabActive]  = accentHi;
    c[ImGuiCol_CheckMark]         = accentHi;
    c[ImGuiCol_Tab]               = ImVec4(0.105f, 0.105f, 0.125f, 1.00f);
    c[ImGuiCol_TabHovered]        = ImVec4(accent.x, accent.y, accent.z, 0.50f);
    c[ImGuiCol_TabActive]         = ImVec4(0.135f, 0.165f, 0.205f, 1.00f);
    c[ImGuiCol_Separator]         = ImVec4(0.24f, 0.24f, 0.28f, 0.70f);
    c[ImGuiCol_SeparatorHovered]  = accent;
    c[ImGuiCol_ScrollbarBg]       = ImVec4(0.050f, 0.050f, 0.058f, 0.85f);
    c[ImGuiCol_ScrollbarGrab]     = ImVec4(0.215f, 0.215f, 0.250f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.290f, 0.290f, 0.330f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive]  = accent;
    c[ImGuiCol_ResizeGrip]        = ImVec4(0.24f, 0.24f, 0.28f, 0.50f);
    c[ImGuiCol_ResizeGripHovered] = ImVec4(accent.x, accent.y, accent.z, 0.60f);
    c[ImGuiCol_ResizeGripActive]  = accent;
    c[ImGuiCol_Text]              = ImVec4(0.88f, 0.88f, 0.90f, 1.00f);
    c[ImGuiCol_TextDisabled]      = ImVec4(0.48f, 0.48f, 0.53f, 1.00f);
    c[ImGuiCol_TextSelectedBg]    = ImVec4(accent.x, accent.y, accent.z, 0.35f);
    c[ImGuiCol_NavHighlight]      = accent;
    c[ImGuiCol_TableHeaderBg]     = ImVec4(0.125f, 0.125f, 0.145f, 1.00f);
    c[ImGuiCol_TableBorderStrong] = ImVec4(0.24f, 0.24f, 0.28f, 0.80f);
    c[ImGuiCol_TableBorderLight]  = ImVec4(0.18f, 0.18f, 0.21f, 0.60f);
    c[ImGuiCol_TableRowBgAlt]     = ImVec4(1.00f, 1.00f, 1.00f, 0.020f);

    ImGui_ImplSDL2_InitForOpenGL(win, ctx);
    ImGui_ImplOpenGL3_Init("#version 330");

    if (!m_Graphics.Init()) return 1;


    // Batch export: `--export out.glb` writes the loaded scene and exits.
    for (int i = 1; i + 1 < argc; i++) {
        if (std::string(argv[i]) != "--export") continue;
        std::string err;
        GlbExportOptions eo;
        const bool ok = ExportGLB(argv[i + 1], eo, err);
        std::cerr << "[export] " << (ok ? "wrote " + std::string(argv[i + 1])
                                        : "failed: " + err) << "\n";
        ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        SDL_GL_DeleteContext(ctx); SDL_DestroyWindow(win); SDL_Quit();
        return ok ? 0 : 1;
    }

    // Mouse orbit state
    bool  mouseRight = false;
    int   prevMouseX = 0, prevMouseY = 0;

    // Who owns the mouse. Computed at the end of each frame and consumed by the
    // next frame's event loop, because SDL events are polled before NewFrame().
    //
    // io.WantCaptureMouse is deliberately NOT used here: ImGuizmo raises it via
    // SetNextFrameWantCaptureMouse() as soon as the cursor merely *hovers* a gizmo
    // handle, which used to kill wheel-zoom and right-drag orbit across the whole
    // middle of the viewport. Hovering the gizmo must not block the camera —
    // only an active left-button manipulation does.
    bool viewportOwnsMouse = true;

    // Gizmo / orbit-sphere interaction state (persist across frames)
    bool sphereDragging = false;
    bool gizmoUsing     = false;

    bool mouse_captured = false;
    Uint32 last_tick = SDL_GetTicks();


    bool run = true;
    while (run) {
        Uint32 now = SDL_GetTicks();
        float dt = (now - last_tick) / 1000.0f;
        if (dt > 0.1f) dt = 0.1f;
        last_tick = now;

        if (state.animSpeed > 0.0f) {
            for (auto& obj : ClimaxEngine::SG::CSceneObjectRegistrar::GetInstance().GetObjects()) {
                if (auto clump = std::dynamic_pointer_cast<ClimaxEngine::SG::CClumpObject>(obj)) {
                    if (clump->animClip.duration > 0.0f) {
                        clump->animTime += dt * state.animSpeed;
                        if (clump->animTime > clump->animClip.duration) {
                            clump->animTime = fmod(clump->animTime, clump->animClip.duration);
                        }
                    }
                }
            }
        }
        if (state.animRestPose) {
            for (auto& obj : ClimaxEngine::SG::CSceneObjectRegistrar::GetInstance().GetObjects()) {
                if (auto clump = std::dynamic_pointer_cast<ClimaxEngine::SG::CClumpObject>(obj)) {
                    clump->animTime = 0.0f; // Force rest pose evaluation (time 0 usually maps to bind pose? Or wait, if we want rest pose, we can just clear the clip. Actually, animClip.duration = 0 in SceneObject.cpp forces rest pose).
                    // We'll let UI.cpp or SceneObject handle rest pose explicitly.
                }
            }
        }

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL2_ProcessEvent(&e);

            if (e.type == SDL_QUIT) run = false;
            
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_m && state.useWASD) {
                    mouse_captured = !mouse_captured;
                    SDL_SetRelativeMouseMode(mouse_captured ? SDL_TRUE : SDL_FALSE);
                }
                // The action button. The trigger asks for MSG_PAD_GRAB, which
                // is the pad's grab/use input; E is its keyboard stand-in.
                if (e.key.keysym.sym == SDLK_e && state.playMode)
                    s_useDoorPressed = true;
                // The container names its animations Clip_1 upwards, so the
                // idle cannot be found by name -- these step through the ones
                // that fit the body until it looks right.
                if (state.playMode && g_Player.loaded &&
                    (e.key.keysym.sym == SDLK_LEFTBRACKET ||
                     e.key.keysym.sym == SDLK_RIGHTBRACKET)) {
                    const int d = e.key.keysym.sym == SDLK_RIGHTBRACKET ? 1 : -1;
                    s_playerClipName = g_Player.CycleClip(d);
                    std::cerr << "[player] clip " << (g_Player.currentClip + 1)
                              << "/" << g_Player.usableClips.size() << "  "
                              << s_playerClipName << "\n";
                }
            }

            // Mouse wheel zoom — proportional so zooming stays usable at any scale
            if (e.type == SDL_MOUSEWHEEL && viewportOwnsMouse) {
                state.camDist = glm::clamp(
                    state.camDist * powf(0.9f, (float)e.wheel.y), 0.5f, 2000.0f);
            }

            // Right mouse button drag → orbit (yaw / pitch)
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_RIGHT && viewportOwnsMouse && !mouse_captured) {
                mouseRight = true;
                prevMouseX = e.button.x;
                prevMouseY = e.button.y;
            }
            if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_RIGHT) {
                mouseRight = false;
            }
            if (e.type == SDL_DROPFILE) {
                std::string path = e.drop.file;
                SDL_free(e.drop.file);

                // Update the file browser path to remember the directory
                g_FileBrowser.currentPath = fs::path(path).parent_path().string();

                // Anything the audio parser recognises is played; the sniff is
                // on the contents, so the extension only decides whether it is
                // worth reading the file at all.
                std::string ext;
                const size_t dot = path.find_last_of('.');
                if (dot != std::string::npos) {
                    ext = path.substr(dot);
                    for (auto& c : ext) c = (char)tolower((unsigned char)c);
                }
                static const char* kAudioExt[] = {
                    ".igc", ".igcstream", ".abc", ".ads", ".rws", ".vag", ".wav",
                };
                bool isAudio = false;
                for (const char* e2 : kAudioExt)
                    if (ext == e2) { isAudio = true; break; }

                if (isAudio) {
                    AudioClip clip;
                    if (::Audio::LoadFile(path, clip)) {
                        PlayAudioClip(clip);
                    } else {
                        std::cerr << "[audio] cannot decode " << path << "\n";
                    }
                }
            }
            // Once a drag has started it keeps running even if the cursor leaves the
            // viewport, otherwise the orbit stutters whenever it crosses a panel.
            if (e.type == SDL_MOUSEMOTION && mouseRight && !mouse_captured) {
                float dx = (float)(e.motion.x - prevMouseX);
                float dy = (float)(e.motion.y - prevMouseY);
                state.camYaw   -= dx * 0.4f;
                state.camPitch  = glm::clamp(state.camPitch + dy * 0.4f, -89.0f, 89.0f);
                prevMouseX = e.motion.x;
                prevMouseY = e.motion.y;
            }
            if (e.type == SDL_MOUSEMOTION && mouse_captured && state.useWASD) {
                state.camYaw   -= e.motion.xrel * state.wasdSensitivity;
                state.camPitch  = glm::clamp(state.camPitch + e.motion.yrel * state.wasdSensitivity, -89.0f, 89.0f);
            }
        }

        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        if (state.useWASD) {
            float yRad = glm::radians(state.camYaw);
            float pRad = glm::radians(state.camPitch);
            glm::vec3 fwd(
                 -cosf(pRad) * sinf(yRad),
                 -sinf(pRad),
                 -cosf(pRad) * cosf(yRad)
            );
            glm::vec3 right = glm::normalize(glm::cross(fwd, glm::vec3(0, 1, 0)));
            glm::vec3 flat_fwd = glm::normalize(glm::cross(glm::vec3(0, 1, 0), right));

            float speed = state.wasdSpeed * dt;
            if (keys[SDL_SCANCODE_LSHIFT]) speed *= 3.0f;

            if (state.playMode) {
                // Walk mode: the same keys drive a body through the collision
                // mesh instead of teleporting the camera. Speed is a rate here,
                // not a per-frame step, because the controller integrates.
                // Travis is fetched once, the first time a level is walked.
                // It costs a level reload, so it must not sit in the frame path.
                static bool triedPlayer = false;
                if (!triedPlayer && !g_CurrentMeshContainer.empty()) {
                    triedPlayer = true;
                    LoadPlayerModel("CPlayerBehaviour.Travis");
                }

                static ClimaxEngine::Game::CharacterController body;
                // Keyed by the container's name, not by &g_Collision: that is a
                // global whose address never changes, so the old test never
                // fired and neither the spawn nor the camera links were ever
                // rebuilt when a level was swapped.
                static std::string placedFor;
                static std::string arrivedFrom;   // zone the player came out of
                static std::vector<ClimaxEngine::Game::ZoneLink> zoneLinks;
                static std::vector<ClimaxEngine::Game::ButtonTrigger> buttonTriggers;

                if (placedFor != g_CurrentMeshContainer) {
                    zoneLinks = ClimaxEngine::Game::BuildZoneLinks(g_GameObjects);
                    buttonTriggers = ClimaxEngine::Game::BuildButtonTriggers(g_GameObjects);
                    std::cerr << "[zone] " << zoneLinks.size()
                              << " doorway(s) in " << g_CurrentMeshContainer << "\n";
                    for (const auto &z : zoneLinks)
                        std::cerr << "[zone]   -> " << z.toZone << "  at ("
                                  << z.position.x << ", " << z.position.y << ", "
                                  << z.position.z << ")\n";
                    glm::vec3 spawn, facing;
                    if (ClimaxEngine::Game::FindZoneSpawn(g_GameObjects, arrivedFrom,
                                                          spawn, facing)) {
                        body.position = spawn;
                        // Face the way the spawner faces -- walking out of a
                        // door should not drop the player looking at it.
                        state.camYaw = glm::degrees(atan2f(-facing.x, -facing.z));
                    } else {
                        body.position = glm::vec3(state.camPosX, state.camPosY,
                                                  state.camPosZ);
                    }
                    body.velocity = glm::vec3(0.0f);
                    body.SnapToGround(g_Collision);
                    placedFor = g_CurrentMeshContainer;
                }

                glm::vec3 wish(0.0f);
                if (keys[SDL_SCANCODE_W]) wish += flat_fwd;
                if (keys[SDL_SCANCODE_S]) wish -= flat_fwd;
                if (keys[SDL_SCANCODE_A]) wish -= right;
                if (keys[SDL_SCANCODE_D]) wish += right;
                if (glm::length(wish) > 1e-4f) wish = glm::normalize(wish);

                float walk = state.walkSpeed;
                if (keys[SDL_SCANCODE_LSHIFT]) walk *= 2.0f;
                body.Step(g_Collision, wish * walk * dt, dt);

                // SnapToGround puts the sphere's centre one radius above the
                // floor, so the model's feet are that much below it.
                s_playerFeet = glm::vec3(body.position.x,
                                         body.position.y - body.radius,
                                         body.position.z);
                if (glm::length(wish) > 1e-3f) {
                    // Turn towards the way he is walking, by the short way
                    // round, so crossing due south does not spin him.
                    const float want = glm::degrees(atan2f(wish.x, wish.z));
                    float d = want - s_playerYaw;
                    while (d >  180.0f) d -= 360.0f;
                    while (d < -180.0f) d += 360.0f;
                    s_playerYaw += d * glm::min(1.0f, dt * 12.0f);
                }
                g_Player.Advance(dt);

                // Idle, walk or run, chosen by what the body is doing. Bound
                // only when it changes: PlayClipAt restarts the clip, so
                // calling it every frame would freeze him on the first frame
                // of the animation.
                if (g_Player.loaded) {
                    const bool moving = glm::length(wish) > 1e-3f;
                    const bool running = moving && keys[SDL_SCANCODE_LSHIFT];
                    int want = moving ? (running ? g_Player.runClip
                                                 : g_Player.walkClip)
                                      : g_Player.idleClip;
                    if (want < 0) want = g_Player.idleClip;
                    if (want >= 0 && want != g_Player.currentClip)
                        s_playerClipName = g_Player.PlayClipAt(want);
                        
                    // ── Footsteps ───────────────────────────────────────────
                    if (moving) {
                        static float s_footstepTimer = 0.0f;
                        s_footstepTimer += dt;
                        const float stepInterval = running ? 0.35f : 0.5f;
                        if (s_footstepTimer >= stepInterval) {
                            s_footstepTimer = 0.0f;
                            // Play a footstep sound if available
                            if (!g_Sounds.empty()) {
                                // Find a sound that might be a footstep
                                for (const auto& snd : g_Sounds) {
                                    if (snd.name.find("Foot") != std::string::npos || 
                                        snd.name.find("Step") != std::string::npos) {
                                        PlayAudioClip(snd);
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }

                // Camera planes: crossing one hands the view to the camera
                // that side names. Rebuilt when the level changes, which is
                // what the collision-mesh pointer tracks.
                static ClimaxEngine::Game::CameraSwitcher switcher;
                static std::string switchesFor;
                if (switchesFor != g_CurrentMeshContainer) {
                    switcher.Reset(ClimaxEngine::Game::BuildCameraSwitches(
                        g_GameObjects, g_Cameras));
                    switchesFor = g_CurrentMeshContainer;
                    state.activeCamera = -1;
                }

                // ── Doorways ────────────────────────────────────────────────
                // Standing in a ZoneTrigger box and pressing the action key
                // loads the container it names and puts Travis on that level's
                // spawner for the zone he just left. MSG_PAD_GRAB is the button
                // the trigger asks for; E stands in for it here.
                // Reach, not containment: a closed door still blocks the body,
                // so the box on its far side has to be usable from this side.
                s_zoneLinkHere = ClimaxEngine::Game::ZoneLinkAt(
                    zoneLinks, body.position, 0.9f);
                if (s_zoneLinkHere >= 0) {
                    s_zonePrompt = zoneLinks[(size_t)s_zoneLinkHere].toZone;
                    if (s_useDoorPressed) {
                        const auto &link = zoneLinks[(size_t)s_zoneLinkHere];
                        auto *arc = ClimaxEngine::RWS::FileSystem::
                            CArchiveManager::GetInstance().GetFirstArchive();
                        const int idx = arc ? arc->Find(link.toZone) : -1;
                        if (idx >= 0) {
                            arrivedFrom = link.fromZone;
                            std::cout << "[zone] " << link.fromZone << " -> "
                                      << link.toZone << "  (" << link.eventName
                                      << ")\n";
                            // ── Door Sound ────────────────────────────────
                            if (!g_Sounds.empty()) {
                                for (const auto& snd : g_Sounds) {
                                    if (snd.name.find("Door") != std::string::npos || 
                                        snd.name.find("Open") != std::string::npos) {
                                        PlayAudioClip(snd);
                                        break;
                                    }
                                }
                            }
                            LoadLevelFromArc(idx);
                        } else {
                            std::cerr << "[zone] no container named "
                                      << link.toZone << " in the archive\n";
                        }
                    }
                } else {
                    s_zonePrompt.clear();
                }
                
                if (state.autoCameras) {
                    const int cut = switcher.Update(body.position);
                    if (cut >= 0 && cut < (int)g_Cameras.size())
                        state.activeCamera = cut;

                    // Which of a plane's two names belongs to which side was a
                    // guess, and a wrong guess puts the view behind a wall.
                    // Rather than guess again, keep the choice honest: if the
                    // active camera cannot see the player, take the nearest one
                    // that can. Cameras sit inside walls looking in, so the wall
                    // between them and the player is exactly what this finds.
                    const glm::vec3 head(body.position.x,
                                         body.position.y + state.eyeHeight * 0.6f,
                                         body.position.z);
                    // A camera is usable when the player is actually in its
                    // shot: inside the cone its own field of view cuts, and not
                    // behind a wall. Both halves come from the level rather than
                    // from tuning -- the cone is the camera's aim and FOV, the
                    // wall is the collision mesh.
                    auto usable = [&](int idx) {
                        if (idx < 0 || idx >= (int)g_Cameras.size()) return false;
                        const LevelCamera &c = g_Cameras[(size_t)idx];
                        const glm::vec3 v = head - c.position;
                        const float dist = glm::length(v);
                        if (dist < 1e-3f) return false;
                        // Slightly wider than the frustum so the player is not
                        // dropped the instant he touches the frame edge.
                        const float halfFov =
                            glm::radians(glm::min(c.fovDeg * 0.6f, 85.0f));
                        if (glm::dot(v / dist,
                                     ClimaxEngine::Game::CameraAim(
                                         c, c.position, head)) < cosf(halfFov))
                            return false;
                        return ClimaxEngine::Game::HasLineOfSight(g_Collision,
                                                                  c.position, head);
                    };

                    if (!usable(state.activeCamera)) {
                        // Nearest camera that still has the player in shot.
                        // Falling back to any that merely sees him keeps a view
                        // on screen while walking through a gap no camera
                        // covers, instead of freezing on a wall.
                        int best = -1, anySight = -1;
                        float bestDist = 1e9f, anyDist = 1e9f;
                        for (size_t k = 0; k < g_Cameras.size(); ++k) {
                            const glm::vec3 &cp = g_Cameras[k].position;
                            if (!ClimaxEngine::Game::HasLineOfSight(g_Collision, cp, head))
                                continue;
                            const float dd = glm::length(cp - head);
                            if (dd < anyDist) { anyDist = dd; anySight = (int)k; }
                            if (usable((int)k) && dd < bestDist) {
                                bestDist = dd; best = (int)k;
                            }
                        }
                        if (best >= 0) state.activeCamera = best;
                        else if (anySight >= 0) state.activeCamera = anySight;
                    }
                }

                // ── Button Triggers ─────────────────────────────────────────
                static int s_buttonTriggerHere = -1;
                s_buttonTriggerHere = ClimaxEngine::Game::ButtonTriggerAt(
                    buttonTriggers, body.position, 0.9f);
                if (s_buttonTriggerHere >= 0) {
                    // Similar to doors, we could display a prompt or just trigger
                    // an event on action key press. We'll use the objName as prompt.
                    s_zonePrompt = buttonTriggers[(size_t)s_buttonTriggerHere].objName;
                    if (s_useDoorPressed) {
                        const auto& btn = buttonTriggers[(size_t)s_buttonTriggerHere];
                        
                        if (btn.className == "MessageBoxTrigger") {
                            s_showMessageBox = true;
                            // the eventName usually holds the string ID or similar reference
                            s_messageBoxText = btn.eventName.empty() ? btn.objName : btn.eventName;
                            s_messageBoxTimer = 3.0f; // show for 3 seconds
                            std::cout << "[MessageBox] " << s_messageBoxText << "\n";
                        } 
                        else if (btn.className == "SavePoint") {
                            s_showSaveMenu = true;
                            std::cout << "[SavePoint] Opening save menu\n";
                        }
                        else {
                            // ButtonBoxTrigger / teleports / etc.
                            if (!btn.targetMap.empty()) {
                                auto *arc = ClimaxEngine::RWS::FileSystem::
                                    CArchiveManager::GetInstance().GetFirstArchive();
                                const int idx = arc ? arc->Find(btn.targetMap) : -1;
                                if (idx >= 0) {
                                    arrivedFrom = btn.eventName; 
                                    std::cout << "[trigger] Teleporting to " << btn.targetMap << "\n";
                                    LoadLevelFromArc(idx);
                                } else {
                                    std::cerr << "[trigger] map " << btn.targetMap << " not found in ARC\n";
                                }
                            } else {
                                std::cerr << "[trigger] Interact with " << btn.objName << " (event: " << btn.eventName << ")\n";
                            }
                        }
                    }
                }

                if (state.autoCameras && state.activeCamera >= 0 &&
                    state.activeCamera < (int)g_Cameras.size()) {
                    // A fixed camera does not follow the player: it sits where
                    // the designer put it and looks where they aimed it, which
                    // is the whole point of the framing in this game.
                    const LevelCamera& lc = g_Cameras[(size_t)state.activeCamera];
                    state.camFovDeg = lc.fovDeg;

                    // The aim of a static camera is authored, and it is in the
                    // placement matrix after all: row 2 is the look direction,
                    // RenderWare's "at".
                    //
                    // An earlier reading called those matrices orientation-free
                    // because all four cameras in HO_1_Hallway1 share one
                    // rotation. Measured over the whole archive that is a
                    // coincidence of one small level: 989 cameras carry 358
                    // distinct rotations, 175 of 229 levels hold more than one,
                    // and 307 are pitched -- up to 83 degrees down a stairwell.
                    // Against every walkable marker in the level, +row2 aims
                    // nearer the playable space than -row2 on 654 cameras to
                    // 314, mean cosine 0.97, which settles the sign too.
                    //
                    // Which cameras honour that, which track the player, and how
                    // the eye gets out from behind the wall it was placed in is
                    // the game layer's business, not the viewer's.
                    const glm::vec3 subject(body.position.x,
                                            body.position.y + state.eyeHeight * 0.6f,
                                            body.position.z);
                    glm::vec3 eye, look;
                    ClimaxEngine::Game::ResolveCameraView(g_Collision, lc, subject,
                                                          eye, look);
                    state.camPosX = eye.x;
                    state.camPosY = eye.y;
                    state.camPosZ = eye.z;

                    // The subject, kept current so the near-plane clip below
                    // measures against the player and not a stale orbit pivot.
                    state.camTargetX = subject.x;
                    state.camTargetY = subject.y;
                    state.camTargetZ = subject.z;
                    const glm::vec3 back = -look;
                    state.camPitch = glm::degrees(asinf(glm::clamp(back.y, -1.0f, 1.0f)));
                    state.camYaw   = glm::degrees(atan2f(back.x, back.z));
                } else {
                    state.camPosX = body.position.x;
                    state.camPosY = body.position.y + state.eyeHeight;
                    state.camPosZ = body.position.z;
                }
            } else {
                if (keys[SDL_SCANCODE_W]) { state.camPosX += flat_fwd.x * speed; state.camPosZ += flat_fwd.z * speed; }
                if (keys[SDL_SCANCODE_S]) { state.camPosX -= flat_fwd.x * speed; state.camPosZ -= flat_fwd.z * speed; }
                if (keys[SDL_SCANCODE_A]) { state.camPosX -= right.x * speed; state.camPosZ -= right.z * speed; }
                if (keys[SDL_SCANCODE_D]) { state.camPosX += right.x * speed; state.camPosZ += right.z * speed; }
                if (keys[SDL_SCANCODE_Q]) { state.camPosY -= speed; }
                if (keys[SDL_SCANCODE_E]) { state.camPosY += speed; }
            }
        } else if (mouse_captured) {
            mouse_captured = false;
            SDL_SetRelativeMouseMode(SDL_FALSE);
        }

        // Logical window size — this is the coordinate space ImGui and ImGuizmo
        // report mouse positions in, so all UI/gizmo rects must use it.
        int winW, winH;
        SDL_GetWindowSize(win, &winW, &winH);
        // Framebuffer size — differs from the logical size on HiDPI/Retina displays.
        // glViewport must use this one; using winW/winH rendered the 3-D scene into
        // a fraction of the framebuffer while the gizmo overlay covered the whole
        // window, so the gizmo did not line up with the geometry at all.
        int fbW, fbH;
        SDL_GL_GetDrawableSize(win, &fbW, &fbH);
        float aspect = fbH > 0 ? (float)fbW / fbH : 1.0f;

        // Build matrices
        glm::vec3 eye;
        glm::mat4 view = BuildView(eye);
        // Fixed cameras are placed behind the walls of the room they film, so
        // the view used to open on that wall. This was answered by pushing the
        // near plane out in proportion to the distance to the subject, which
        // cleared the wall but also sliced the front off the room and let the
        // frame show the emptiness past the floor.
        //
        // Game::ResolveCameraView now steps the eye through the wall instead,
        // so it renders from inside the room and the near plane can go back to
        // being a near plane. A small proportional clip is kept for the case it
        // declines to move -- when the wall sits so close to the player that
        // stepping through would land the camera on top of him.
        float nearPlane = 0.1f;
        if (state.playMode && state.autoCameras && state.activeCamera >= 0 &&
            state.activeCamera < (int)g_Cameras.size()) {
            const glm::vec3 d = eye - glm::vec3(state.camTargetX, state.camTargetY,
                                                state.camTargetZ);
            nearPlane = glm::clamp(glm::length(d) * 0.05f, 0.1f, 0.6f);
        }
        glm::mat4 proj = glm::perspective(glm::radians(state.camFovDeg), aspect, nearPlane, 2000.0f);
        glm::mat4 mvp  = proj * view;

        // --- Render 3-D scene ---
        // Sync audio state
        state.isAudioPlaying = ClimaxEngine::Audio::CAudioRelay::GetInstance().IsAudioPlaying();
        state.audioProgress = ClimaxEngine::Audio::CAudioRelay::GetInstance().GetAudioProgress();
        ClimaxEngine::Audio::CAudioRelay::GetInstance().SetVolume(state.audioVolume);
        ClimaxEngine::Audio::CAudioRelay::GetInstance().SetLoop(state.audioLoop);

        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplSDL2_NewFrame(); ImGui::NewFrame();
        ImGuizmo::BeginFrame();
        iam_update_begin_frame();
        iam_clip_update(io.DeltaTime);

        // Key 1 → reset camera, F1 → hide/show the whole interface
        if (!io.WantCaptureKeyboard && ImGui::IsKeyPressed(ImGuiKey_1, false)) {
            state.camTargetX = 0; state.camTargetY = 2; state.camTargetZ = 0;
            state.camYaw = 0; state.camPitch = 20; state.camDist = 15; state.camFovDeg = 60.0f;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F1, false)) state.showUI = !state.showUI;
        if (ImGui::IsKeyPressed(ImGuiKey_F2, false)) state.showManual = !state.showManual;
        if (ImGui::IsKeyPressed(ImGuiKey_G,  false) && !io.WantCaptureKeyboard)
            state.showPivotGizmo = !state.showPivotGizmo;

        size_t totalMeshes = 0;
        for (auto& obj : ClimaxEngine::SG::CSceneObjectRegistrar::GetInstance().GetObjects()) {
            totalMeshes += obj->GetMeshes().size();
        }
        const bool haveModel = totalMeshes > 0;


        glm::vec3 viewDir(-view[0][2], -view[1][2], -view[2][2]);
        
        // The level's placed lights. CColorLight is parsed by the loader and
        // was thrown away: nothing read `go.isLight`. Its placement matrix sits
        // in the object's fourth component, not its first, so `go.position`
        // stays at the origin -- which is why every light in HO_1_ExamRoom sat
        // outside the room.
        m_Graphics.lightPos.clear();
        m_Graphics.lightCol.clear();
        m_Graphics.lightRange.clear();
        m_Graphics.lightType.clear();
        for (const auto& go : g_GameObjects) {
            if (!go.isLight) continue;
            if (m_Graphics.lightPos.size() >= 16) break;
            m_Graphics.lightPos.push_back(go.haveLightPos ? go.lightPos : go.position);
            m_Graphics.lightCol.push_back(go.lightColor);
            m_Graphics.lightRange.push_back(go.lightRange);
            m_Graphics.lightType.push_back(go.lightType);
        }

        // Native Fog Extraction
        if (state.useNativeFog) {
            for (const auto& go : g_GameObjects) {
                if (go.isFogConfig) {
                    state.enableFog = true;
                    state.fogStart = go.fogStart;
                    state.fogEnd = go.fogEnd;
                    state.fogDensity = go.fogDensity;
                    state.fogColor[0] = go.fogColor.r;
                    state.fogColor[1] = go.fogColor.g;
                    state.fogColor[2] = go.fogColor.b;
                    break;
                }
            }
        }

        m_Graphics.RenderFrame(fbW, fbH, winW, winH, mvp, eye, viewDir, totalMeshes);


        // -- Orbit sphere geometry (hit region only; drawn further down) -----
        // The circle is computed before the gizmo runs so the two widgets can
        // arbitrate over the same left button instead of both grabbing it.
        const float SR = 54.0f;
        const ImVec2 sphereCtr((float)winW - 10.0f - 122.0f * 0.5f, 10.0f + SR + 8.0f);
        const float sdx = io.MousePos.x - sphereCtr.x;
        const float sdy = io.MousePos.y - sphereCtr.y;
        const bool  overSphere = (sdx*sdx + sdy*sdy <= (SR + 3.0f)*(SR + 3.0f));

        // -- ImGuizmo translate pivot ----------------------------------------
        // Runs before every IsOver()/IsUsing() query below: ImGuizmo only refreshes
        // its hover/use state inside Manipulate(), so querying it earlier in the
        // frame returned data from the previous frame.
        gizmoUsing = false;
        // No level, no gizmo: the arrows used to float in an empty viewport with
        // nothing to aim at.
        if (state.showPivotGizmo && state.showUI && haveModel) {
            // Enable() only suppresses interaction — Manipulate() still draws the
            // gizmo — so hiding it has to skip the call entirely.
            // Hand the left button to the orbit sphere when the cursor is on it,
            // but never cancel a manipulation that is already in progress.
            ImGuizmo::Enable(!sphereDragging && (ImGuizmo::IsUsing() || !overSphere));

            ImGuizmo::SetOrthographic(false);
            ImGuizmo::AllowAxisFlip(false);          // no confusing flips
            ImGuizmo::SetGizmoSizeClipSpace(0.12f);
            // No SetDrawlist(): ImGuizmo::BeginFrame() already installed its own
            // full-screen NoInputs window. Pointing it at the foreground draw list
            // made the gizmo paint over every panel and broke the hover test that
            // keeps it from reacting to clicks landing on the UI.
            ImGuizmo::SetRect(0.0f, 0.0f, (float)winW, (float)winH);

            float viewArr[16], projArr[16], matArr[16];
            memcpy(viewArr, glm::value_ptr(view), sizeof(viewArr));
            memcpy(projArr, glm::value_ptr(proj), sizeof(projArr));
            glm::mat4 pivotMat = glm::translate(glm::mat4(1.0f),
                glm::vec3(state.camTargetX, state.camTargetY, state.camTargetZ));
            memcpy(matArr, glm::value_ptr(pivotMat), sizeof(matArr));

            const bool snapping = state.pivotSnapOn || io.KeyCtrl;
            const float snapStep = std::max(state.pivotSnap, 0.001f);
            const float snapVec[3] = { snapStep, snapStep, snapStep };

            // Take the manipulated matrix verbatim. The previous code applied only
            // 35 % of (result - current) per frame as a "sensitivity" tweak, but
            // ImGuizmo returns the *absolute* position the cursor projects to, not
            // an incremental delta — so scaling it turned the drag into a per-frame
            // exponential lag: the pivot never reached the cursor, kept creeping for
            // a while after the mouse stopped, and moved at a different speed
            // depending on the frame rate.
            if (ImGuizmo::Manipulate(viewArr, projArr,
                    ImGuizmo::TRANSLATE, ImGuizmo::WORLD, matArr,
                    nullptr, snapping ? snapVec : nullptr)) {
                state.camTargetX = matArr[12];
                state.camTargetY = matArr[13];
                state.camTargetZ = matArr[14];
            }

            gizmoUsing = ImGuizmo::IsUsing();
            // No SDL_CaptureMouse() here: imgui_impl_sdl2 already calls it every
            // frame from the button mask, so the manual calls were dead at best and
            // released a capture the orbit sphere still needed at worst.
        }

        // -- Orbit sphere overlay (top-right, direct circular hit-test) ------
        if (state.showUI) {
            DrawOrbitSphere(ImGui::GetForegroundDrawList(), sphereCtr, SR, view);

            if (!sphereDragging && overSphere
                    && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
                    && !gizmoUsing
                    && !ImGuizmo::IsOver()
                    && !io.WantCaptureMouse) {
                sphereDragging = true;
            }
            if (sphereDragging) {
                if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                    // Dragging the ball turns the ball, so the camera has to move
                    // the opposite way from a free right-drag orbit. Both axes used
                    // to be inherited from the orbit code and came out reversed.
                    state.camYaw   -= io.MouseDelta.x * 0.32f;
                    state.camPitch  = glm::clamp(
                        state.camPitch + io.MouseDelta.y * 0.32f, -89.0f, 89.0f);
                } else {
                    sphereDragging = false;
                }
            }
        }

        // -- Main control panel (pinned top-left, fixed 256 px, scrollable) --
        if (state.showUI) {
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(256, (float)winH - 20.0f), ImGuiCond_Always);
        // The tool covers both games, so the panel is not "SHO" anything.
        ImGui::Begin("Climax Toolkit", nullptr,
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

        if (ImGui::Button("Open SH.ARC", ImVec2(-1, 0))) g_FileBrowser.Open(FileBrowserMode::Arc);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Load the game archive and browse levels by their real names");
        if (ClimaxEngine::RWS::FileSystem::CArchiveManager::GetInstance().GetFirstArchive()) {
            ImGui::TextDisabled("%s", fs::path(ClimaxEngine::RWS::FileSystem::CArchiveManager::GetInstance().GetFirstArchive()->Path()).filename().string().c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("(%zu files)", ClimaxEngine::RWS::FileSystem::CArchiveManager::GetInstance().GetFirstArchive()->Entries().size());
        }
        if (ImGui::Button("Open Loose File", ImVec2(-1, 0))) g_FileBrowser.Open(FileBrowserMode::Mesh);

        // What is loaded, stated once and clearly. This used to be two dim
        // lines that read like a caption; it is the single most useful thing
        // on the panel, so it gets a framed block of its own.
        if (!g_CurrentMeshContainer.empty()) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.085f, 0.115f, 0.150f, 0.85f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
            ImGui::BeginChild("##loaded", ImVec2(-1, ImGui::GetTextLineHeightWithSpacing() * 2.5f),
                              true, ImGuiWindowFlags_NoScrollbar);
            ImGui::TextColored(ImVec4(0.35f, 0.68f, 0.95f, 1.0f), "%s",
                fs::path(g_CurrentMeshContainer).filename().string().c_str());
            ImGui::TextDisabled("%zu meshes   %zu textures", totalMeshes,
                                g_TextureMap.size() / 2);
            if (!g_AnimClips.empty()) {
                ImGui::SameLine();
                ImGui::TextDisabled("  %zu clips", g_AnimClips.size());
            }
            if (!g_UVAnims.empty()) {
                ImGui::SameLine();
                ImGui::TextDisabled("  %zu uv", g_UVAnims.size());
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();

        // ---- Camera --------------------------------------------------
        ImGui::SeparatorText("Camera");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##dist", &state.camDist, 1.0f, 200.0f, "Dist %.1f");

        if (ImGui::Checkbox("Free Flight (WASD, M to toggle mouse)", &state.useWASD)) {
            if (state.useWASD) {
                state.showPivotGizmo = false;
                // Sync position
                float yRad = glm::radians(state.camYaw);
                float pRad = glm::radians(glm::clamp(state.camPitch, -89.0f, 89.0f));
                float dist = std::max(state.camDist, 0.1f);
                state.camPosX = state.camTargetX + cosf(pRad) * sinf(yRad) * dist;
                state.camPosY = state.camTargetY + sinf(pRad) * dist;
                state.camPosZ = state.camTargetZ + cosf(pRad) * cosf(yRad) * dist;
            } else {
                // Sync orbit target
                float yRad = glm::radians(state.camYaw);
                float pRad = glm::radians(glm::clamp(state.camPitch, -89.0f, 89.0f));
                state.camTargetX = state.camPosX - cosf(pRad) * sinf(yRad) * state.camDist;
                state.camTargetY = state.camPosY - sinf(pRad) * state.camDist;
                state.camTargetZ = state.camPosZ - cosf(pRad) * cosf(yRad) * state.camDist;
                
                if (mouse_captured) {
                    mouse_captured = false;
                    SDL_SetRelativeMouseMode(SDL_FALSE);
                }
            }
        }
        if (state.useWASD) {
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat("##wasd_speed", &state.wasdSpeed, 1.0f, 100.0f, "Speed %.1f");
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat("##wasd_sens", &state.wasdSensitivity, 0.01f, 1.0f, "Sens %.2f");
        }

        {
            // Walk mode only means anything once the level has collision to
            // stand on, so it is disabled rather than silently doing nothing.
            const bool canWalk = !g_Collision.indices.empty();
            ImGui::BeginDisabled(!canWalk);
            if (ImGui::Checkbox("Walk (collision)", &state.playMode) && state.playMode)
                state.useWASD = true;   // walk mode drives the same camera
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered() && state.uiTooltips)
                ImGui::SetTooltip(canWalk
                    ? "Move as a body through the level's collision mesh.\n"
                      "Starts at the level's CPlayerSpawner when it has one."
                    : "This level has no collision mesh loaded.");
            if (state.playMode) {
                ImGui::SetNextItemWidth(-70.0f);
                ImGui::SliderFloat("##walkspeed", &state.walkSpeed, 1.0f, 8.0f, "%.1f");
                ImGui::SameLine(); ImGui::TextDisabled("Speed");
                ImGui::SetNextItemWidth(-70.0f);
                ImGui::SliderFloat("##eye", &state.eyeHeight, 0.8f, 2.2f, "%.2f");
                ImGui::SameLine(); ImGui::TextDisabled("Eye");
                // What the player container actually gave us. In its own
                // window: the side panel is 250 px wide and a five-column
                // table does not fit in it legibly.
                static bool showPieces = false;
                if (g_Player.loaded)
                    ImGui::Checkbox("Player pieces", &showPieces);
                s_showPlayerPieces = showPieces && g_Player.loaded;
                ImGui::Checkbox("Fixed cameras", &state.autoCameras);
                if (ImGui::IsItemHovered() && state.uiTooltips)
                    ImGui::SetTooltip("Hand the view to the level's own cameras when\n"
                                      "a PlaneTrigger is crossed, as the game does.\n"
                                      "Off keeps a first-person view on the body.");
                if (state.autoCameras && state.activeCamera >= 0 &&
                    state.activeCamera < (int)g_Cameras.size())
                    ImGui::TextDisabled("camera: %s",
                                        g_Cameras[(size_t)state.activeCamera].name.c_str());
            }
        }

        if (ImGui::Button("Reset Camera", ImVec2(-1, 0))) {
            state.camTargetX = 0; state.camTargetY = 2; state.camTargetZ = 0;
            state.camPosX = 0; state.camPosY = 2; state.camPosZ = 15;
            state.camYaw = 0; state.camPitch = 20; state.camDist = 15; state.camFovDeg = 60.0f;
        }

        // ---- Level cameras ------------------------------------------
        if (!g_Cameras.empty()) {
            ImGui::Spacing();
            ImGui::TextDisabled("Level cameras (%zu)", g_Cameras.size());
            {
                // Level logic, read straight out of the object graph: which
                // plane hands over to which camera.
                static std::vector<ClimaxEngine::Game::CameraSwitch> switches;
                static size_t builtFor = (size_t)-1;
                if (builtFor != g_GameObjects.size()) {
                    switches = ClimaxEngine::Game::BuildCameraSwitches(g_GameObjects, g_Cameras);
                    builtFor = g_GameObjects.size();
                }
                if (!switches.empty()) {
                    int resolved = 0;
                    for (const auto& sw : switches)
                        if (sw.cameraA >= 0 && sw.cameraB >= 0) resolved++;
                    ImGui::TextDisabled("%zu camera switches, %d resolved",
                                        switches.size(), resolved);
                    if (ImGui::IsItemHovered() && state.uiTooltips) {
                        ImGui::BeginTooltip();
                        for (size_t k = 0; k < switches.size() && k < 12; k++)
                            ImGui::Text("%s  ->  %s", switches[k].nameA.c_str(),
                                        switches[k].nameB.c_str());
                        ImGui::EndTooltip();
                    }
                }
            }
            if (state.camFovDeg != 60.0f) {
                ImGui::SameLine();
                ImGui::TextDisabled("FOV %.0f°", state.camFovDeg);
            }
            ImGui::SetNextItemWidth(-1);
            if (ImGui::BeginCombo("##camsel", "Jump to camera...")) {
                for (size_t i = 0; i < g_Cameras.size(); i++) {
                    const LevelCamera& c = g_Cameras[i];
                    ImGui::PushID((int)i);
                    if (ImGui::Selectable(c.name.c_str())) {
                        // Orbit around a point in front of the camera so the
                        // existing orbit controls keep working from there.
                        const float d = 3.0f;
                        glm::vec3 target = c.position + c.forward * d;
                        state.camTargetX = target.x;
                        state.camTargetY = target.y;
                        state.camTargetZ = target.z;
                        state.camDist    = d;
                        glm::vec3 back   = -c.forward;
                        state.camPitch   = glm::degrees(asinf(glm::clamp(back.y, -1.0f, 1.0f)));
                        state.camYaw     = glm::degrees(atan2f(back.x, back.z));
                        state.camFovDeg  = c.fovDeg;
                    }
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("(%.2f, %.2f, %.2f)   FOV %.0f°",
                                          c.position.x, c.position.y, c.position.z,
                                          c.fovDeg);
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }
            ImGui::Spacing();
        }

        if (ImGui::Checkbox("Pivot gizmo", &state.showPivotGizmo)) {
            if (state.showPivotGizmo) {
                state.useWASD = false;
            }
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Drag the arrows/planes to move the orbit pivot.\n"
                              "Hold Ctrl while dragging to snap.");
        if (state.showPivotGizmo) {
            ImGui::SameLine(128);
            ImGui::Checkbox("Snap", &state.pivotSnapOn);
            ImGui::SetNextItemWidth(-44.0f);
            ImGui::DragFloat("##snapstep", &state.pivotSnap, 0.05f, 0.01f, 100.0f, "%.2f");
            ImGui::SameLine(); ImGui::TextDisabled("Step");
            ImGui::TextDisabled("Pivot %.2f  %.2f  %.2f",
                state.camTargetX, state.camTargetY, state.camTargetZ);
        }
        ImGui::TextDisabled("G gizmo  F1 hide UI  F2 manual");
        // ---- Render Device (GPU / CPU) -----------------------------------
        ImGui::TextDisabled("Render Device / Engine");
        if (ImGui::RadioButton("GPU (Hardware)", state.renderDevice == RenderDevice::GPU)) {
            state.renderDevice = RenderDevice::GPU;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Hardware Acceleration (OpenGL 3.3 Core / Apple Metal shaders)");
        ImGui::SameLine();
        if (ImGui::RadioButton("CPU (Software)", state.renderDevice == RenderDevice::CPU)) {
            state.renderDevice = RenderDevice::CPU;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Software Rasterizer: renders geometry & shading on CPU thread");

        ImGui::Spacing();

        // ---- Render mode ------------------------------------------------
        ImGui::SeparatorText("Render Mode");
        ImGui::Spacing();

        struct ModeBtn { const char* label; RenderMode mode; const char* tip; };
        const ModeBtn MODES[] = {
            {"Textured",    RenderMode::Textured,    "Texture + vertex colors"},
            {"Vert.Color",  RenderMode::VertexColor, "Vertex colors only"},
            {"Flat",        RenderMode::FlatShaded,  "Per-face shading, no texture"},
            {"Normals",     RenderMode::Normals,     "Face normals as RGB"},
            {"Depth",       RenderMode::Depth,       "Linear depth grey-scale"},
            {"Checker",     RenderMode::Checker,     "UV checkerboard"},
            {"Unlit",       RenderMode::Unlit,       "Texture, no lighting"},
        };
        const float BTN_W = (248.0f - 20.0f - 2.0f * 6.0f) / 4.0f;
        int mi = 0;
        // The selected mode used to pop on and off with a hard colour swap,
        // which in a grid of eight buttons makes it hard to see which one moved.
        // Tweening the fill through ImAnim means the eye follows the change.
        {
            const float dt = ImGui::GetIO().DeltaTime;
            const ImVec4 idle = ImGui::GetStyleColorVec4(ImGuiCol_Button);
            const ImVec4 on   = ImVec4(0.00f, 0.41f, 0.71f, 1.00f);
            for (auto& mb : MODES) {
                const bool active = (state.renderMode == mb.mode);
                const ImGuiID bid = ImGui::GetID(mb.label);
                const ImVec4 fill = iam_tween_color(
                    bid, ImGui::GetID("fill"), active ? on : idle, 0.18f,
                    iam_ease_preset(iam_ease_out_cubic), iam_policy_crossfade,
                    iam_col_oklab, dt);
                ImGui::PushStyleColor(ImGuiCol_Button, fill);
                if (ImGui::Button(mb.label, ImVec2(BTN_W, 22.0f)))
                    state.renderMode = mb.mode;
                if (ImGui::IsItemHovered() && state.uiTooltips)
                    ImGui::SetTooltip("%s", mb.tip);
                ImGui::PopStyleColor();
                if (++mi % 4 != 0) ImGui::SameLine(0, 3.0f);
            }
        }

        ImGui::Spacing();

        // ---- Display options ----------------------------------------
        ImGui::SeparatorText("Display");
        ImGui::Checkbox("Wireframe",   &state.showWireframe); ImGui::SameLine(128);
        ImGui::Checkbox("Linear",      &state.linearFilter);
        ImGui::Checkbox("Vert.Colors", &state.useVertexColors);
        ImGui::SetNextItemWidth(-44.0f);
        ImGui::SliderFloat("##bright", &state.brightness, 0.5f, 3.0f);
        ImGui::SameLine(); ImGui::TextDisabled("Bright");

        // ---- Model sections nothing placed --------------------------
        {
            size_t orphan = 0, placedSecs = 0;
            for (const auto& s : g_ShoSections) {
                if (s.isWorldSpace) continue;
                if (s.instances.empty()) orphan++; else placedSecs++;
            }
            if (orphan || placedSecs) {
                ImGui::Checkbox("Unplaced models", &state.showUnplacedModels);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip(
                        "%zu model sections are placed by game objects.\n"
                        "%zu are referenced by nothing; showing them piles\n"
                        "their geometry on the origin.", placedSecs, orphan);
            }
        }

        // ---- Overlay objects (shown when loaded) -------------------
        if (GpuPeek(g_Collision) || !g_Clumps.empty()) {
            ImGui::Spacing();
            ImGui::SeparatorText("Overlay");
            if (GpuPeek(g_Collision)) {
                ImGui::Checkbox("Collision Wire", &state.showCollision);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%zu verts  %zu tris",
                        g_Collision.verts.size(), g_Collision.indices.size() / 3);
                if (state.showCollision) {
                    ImGui::SameLine(128);
                    ImGui::Checkbox("Solid##cs", &state.showCollisionSolid);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Semi-transparent fill");
                }
            }
            if (!g_Clumps.empty()) {
                ImGui::Checkbox("Clumps", &state.showClumps);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%zu clump objects", g_Clumps.size());
            }
            if (!g_GameObjects.empty()) {
                size_t placed = 0;
                for (const auto& go : g_GameObjects) if (!go.atOrigin) placed++;
                ImGui::Checkbox("Objects", &state.showGameObjects);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%zu game objects, %zu with a world transform",
                                      g_GameObjects.size(), placed);
                if (state.showGameObjects) {
                    ImGui::SameLine(128);
                    ImGui::Checkbox("Labels", &state.showObjectLabels);
                    ImGui::Checkbox("At origin", &state.showOriginObjects);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Also show the %zu logical objects that carry\n"
                                          "an identity transform (CZone, GameMessage, ...)",
                                          g_GameObjects.size() - placed);
                }
            }
        }

        ImGui::Spacing();

        // ---- Panels & extras ---------------------------------------
        ImGui::SeparatorText("Panels");
        ImGui::Checkbox("Structure", &state.showStructure); ImGui::SameLine(128);
        ImGui::Checkbox("Textures",  &state.showTextures);
        ImGui::Checkbox("Archive",   &state.showArc); ImGui::SameLine(128);
        ImGui::Checkbox("Manual",    &state.showManual);
        ImGui::Checkbox("Playback",  &state.showAudioPlayer);

        // Wii-only options; hidden when the loaded container has neither.
        {
            bool anyIce = false, anyAlt = false;
            for (auto& obj : ClimaxEngine::SG::CSceneObjectRegistrar::GetInstance().GetObjects()) {
                for (auto* cPtr : obj->GetMeshes()) {
                    const auto& c = *cPtr;
                    anyIce |= c.iceEffect;
                    anyAlt |= !c.altTexName.empty() && c.altTexName != c.texName;
                }
            }
            if (anyIce || anyAlt) {
                ImGui::Spacing();
                ImGui::TextDisabled("Wii");
                if (anyIce) {
                    ImGui::Checkbox("Ice shading", &state.iceShading);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Fresnel and specular over the colour map.\n"
                                          "An approximation: the real look comes from\n"
                                          "the GX TEV stages, which the container does\n"
                                          "not store.");
                }
                if (anyAlt) {
                    ImGui::Checkbox("Frozen variant", &state.frozenVariant);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Draw the alternate texture the material\n"
                                          "names in its 0x0129 extension - the same\n"
                                          "surface in its Otherworld state.");
                }
            }
        }

        // ---- Export -------------------------------------------------
        if (haveModel) {
            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
            if (ImGui::CollapsingHeader("Export glTF")) {
                static GlbExportOptions opt;
                static std::string exportMsg;
                static bool exportOk = false;
                static bool saveToDesktop = true;
                static char customFilename[128] = "";

                ImGui::Checkbox("Embed textures (PNG)",     &opt.embedTextures);
                ImGui::Checkbox("Accurate Alpha (PBR)",     &opt.accurateAlpha);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Accurately assigns OPAQUE, MASK (0.02), and BLEND modes\n"
                                      "matching the on-screen viewer (character faces, blood, etc.)");
                ImGui::Checkbox("Unlit Materials",          &opt.unlitMaterials);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Emit KHR_materials_unlit for additive fire/effects and unlit geometry");
                ImGui::Checkbox("UV Animation tracks",      &opt.exportUvAnimations);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Export UV animation tracks via KHR_texture_transform\n"
                                      "and KHR_animation_pointer for animatable scrolling UVs");
                ImGui::Checkbox("Bake Active UV Frame",     &opt.bakeCurrentUvAnim);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Bake the active UV animation frame directly into vertex UVs\n"
                                      "(100%% compatibility with any DCC / game engine)");
                ImGui::Checkbox("Vertex colors",            &opt.includeVertexColors);
                ImGui::Checkbox("Lights",                   &opt.includeLights);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Export CColorLight objects as\nKHR_lights_punctual lights");
                ImGui::Checkbox("Save to Desktop",          &saveToDesktop);

                // Default filename derived from container
                std::string defaultBase = g_CurrentMeshContainer.empty()
                    ? std::string("scene")
                    : fs::path(g_CurrentMeshContainer).filename().string();
                if (defaultBase.size() > 4 && (sho_stricmp(defaultBase.c_str() + defaultBase.size() - 4, ".arc") == 0))
                    defaultBase = defaultBase.substr(0, defaultBase.size() - 4);
                for (char& c : defaultBase)
                    if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
                        c = '_';

                if (customFilename[0] == '\0') {
                    snprintf(customFilename, sizeof(customFilename), "%s", defaultBase.c_str());
                }

                ImGui::SetNextItemWidth(-1);
                ImGui::InputTextWithHint("##outname", "Output filename (.glb)", customFilename, sizeof(customFilename));

                if (ImGui::Button("Export .glb", ImVec2(-1, 0))) {
                    std::string fname = customFilename[0] ? customFilename : defaultBase;
                    if (fname.size() < 4 || sho_stricmp(fname.c_str() + fname.size() - 4, ".glb") != 0)
                        fname += ".glb";

                    std::string outPath = fname;
                    if (saveToDesktop) {
#if defined(_WIN32)
                        const char* home = std::getenv("USERPROFILE");
                        std::string desk = home ? std::string(home) + "\\Desktop\\" : "";
#else
                        const char* home = std::getenv("HOME");
                        std::string desk = home ? std::string(home) + "/Desktop/" : "";
#endif
                        if (!desk.empty()) outPath = desk + fname;
                    }

                    opt.uvAnimTime = state.uvAnimTime;
                    std::string err;
                    exportOk = ExportGLB(outPath, opt, err);
                    exportMsg = exportOk ? ("Saved " + outPath) : ("Failed: " + err);
                    std::cerr << "[export] " << exportMsg << "\n";
                }
                if (!exportMsg.empty())
                    ImGui::TextColored(exportOk ? ImVec4(0.52f, 0.86f, 0.52f, 1.0f)
                                                : ImVec4(1.0f, 0.45f, 0.45f, 1.0f),
                                       "%s", exportMsg.c_str());
            }
        }

        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        if (ImGui::CollapsingHeader("Settings")) {
            ImGui::SeparatorText("Interface");

            ImGui::SetNextItemWidth(-70.0f);
            if (ImGui::SliderFloat("##uiscale", &state.uiScale, 0.8f, 1.6f, "%.2f"))
                ImGui::GetIO().FontGlobalScale = state.uiScale;
            ImGui::SameLine(); ImGui::TextDisabled("Scale");
            if (ImGui::IsItemHovered() && state.uiTooltips)
                ImGui::SetTooltip("Size of every panel and label. Useful on a\n"
                                  "high-density display where 256 px of panel\n"
                                  "is a very small 256 px.");

            // ImAnim drives the panel's motion; this is its master rate, so 0
            // turns every transition off rather than leaving them half-done.
            ImGui::SetNextItemWidth(-70.0f);
            if (ImGui::SliderFloat("##uianim", &state.uiAnimSpeed, 0.0f, 2.0f, "%.2fx"))
                iam_set_global_time_scale(state.uiAnimSpeed);
            ImGui::SameLine(); ImGui::TextDisabled("Motion");
            if (ImGui::IsItemHovered() && state.uiTooltips)
                ImGui::SetTooltip("Speed of button and list transitions.\n"
                                  "Set to 0 for no animation at all.");

            ImGui::Checkbox("Tooltips", &state.uiTooltips);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Turn these explanations off once you know\n"
                                  "your way around.");

            if (ImGui::Button("Reset interface", ImVec2(-1, 0))) {
                state.uiScale = 1.0f;
                state.uiAnimSpeed = 1.0f;
                state.uiTooltips = true;
                ImGui::GetIO().FontGlobalScale = 1.0f;
                iam_set_global_time_scale(1.0f);
            }

            ImGui::SeparatorText("Paths");
            {
                const std::string arc = LoadArcPref();
                ImGui::TextDisabled("Archive remembered:");
                ImGui::TextWrapped("%s", arc.empty() ? "(none yet)" : arc.c_str());
                if (ImGui::IsItemHovered() && !arc.empty() && state.uiTooltips)
                    ImGui::SetTooltip("%s", arc.c_str());
                if (!arc.empty() && ImGui::Button("Forget", ImVec2(-1, 0)))
                    SaveArcPref("");
            }
        }

        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        if (ImGui::CollapsingHeader("Background")) {
            ImGui::Checkbox("Gradient sky", &state.skyGradient);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Draw a vertical colour gradient behind the scene");
            ImGui::Spacing();
            ImGui::TextDisabled(state.skyGradient ? "Top colour" : "Clear colour");
            ImGui::SetNextItemWidth(-1);
            ImGui::ColorEdit3("##skyTop", state.skyColorTop,
                ImGuiColorEditFlags_Float | ImGuiColorEditFlags_PickerHueBar);
            if (state.skyGradient) {
                ImGui::TextDisabled("Bottom colour");
                ImGui::SetNextItemWidth(-1);
                ImGui::ColorEdit3("##skyBot", state.skyColorBot,
                    ImGuiColorEditFlags_Float | ImGuiColorEditFlags_PickerHueBar);
            }
            if (ImGui::Button("Reset##bg", ImVec2(-1, 0))) {
                state.skyColorTop[0] = 0.07f; state.skyColorTop[1] = 0.07f; state.skyColorTop[2] = 0.09f;
                state.skyColorBot[0] = 0.11f; state.skyColorBot[1] = 0.11f; state.skyColorBot[2] = 0.14f;
                state.skyGradient = false;
            }
        }
        
        if (ImGui::CollapsingHeader("Environment (Fog)")) {
            ImGui::Checkbox("Level lights (CColorLight)", &state.enableLights);
            if (state.enableLights)
                ImGui::SliderFloat("Light strength", &state.lightIntensity, 0.0f, 2.0f);
            ImGui::Checkbox("Use Native Fog (CFogConfig)", &state.useNativeFog);
            ImGui::Checkbox("Enable Fog", &state.enableFog);
            if (state.enableFog) {
                ImGui::SetNextItemWidth(-1);
                ImGui::ColorEdit3("##fogColor", state.fogColor, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_PickerHueBar);
                ImGui::Combo("Mode", &state.fogMode, "Linear\0Exponential\0Exp Squared\0");
                if (state.fogMode == 0) {
                    ImGui::DragFloat("Start", &state.fogStart, 0.5f, 0.0f, 1000.0f);
                    ImGui::DragFloat("End", &state.fogEnd, 0.5f, 0.0f, 1000.0f);
                } else {
                    ImGui::DragFloat("Density", &state.fogDensity, 0.001f, 0.0f, 1.0f);
                }
            }
        }
        
        RenderPlaybackPanel();

        ImGui::End();

        if (state.showStructure) RenderStructureWindow();
        if (state.showTextures)  RenderTxdWindow();
        if (state.showArc)       RenderArcWindow();
        if (state.showManual)    RenderManualWindow();

        if (s_showPlayerPieces) {
            ImGui::SetNextWindowSize(ImVec2(520, 380), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Player pieces", nullptr)) {
                ImGui::TextDisabled("%zu of %zu clips fit the body",
                                    g_Player.usableClips.size(),
                                    g_Player.clips.size());
                auto clipName = [](int i) {
                    return i >= 0 && i < (int)g_Player.usableClips.size()
                               ? g_Player.clips[(size_t)g_Player.usableClips[(size_t)i]]
                                     .name.c_str()
                               : "-";
                };
                ImGui::TextDisabled("idle %s   walk %s   run %s",
                                    clipName(g_Player.idleClip),
                                    clipName(g_Player.walkClip),
                                    clipName(g_Player.runClip));
                ImGui::Separator();
                if (ImGui::BeginTable("pieces", 6,
                                      ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_Borders |
                                      ImGuiTableFlags_ScrollY |
                                      ImGuiTableFlags_Resizable)) {
                    ImGui::TableSetupColumn("object",  ImGuiTableColumnFlags_WidthFixed, 52.0f);
                    ImGui::TableSetupColumn("texture", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("frame",   ImGuiTableColumnFlags_WidthFixed, 52.0f);
                    ImGui::TableSetupColumn("skinned", ImGuiTableColumnFlags_WidthFixed, 62.0f);
                    ImGui::TableSetupColumn("verts",   ImGuiTableColumnFlags_WidthFixed, 56.0f);
                    ImGui::TableSetupColumn("drawn",   ImGuiTableColumnFlags_WidthFixed, 62.0f);
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableHeadersRow();
                    for (size_t i = 0; i < g_Player.objects.size(); ++i) {
                        const bool isBody = (int)i == g_Player.bodyObject;
                        const int bone = i < g_Player.attachBone.size()
                                             ? g_Player.attachBone[i] : -1;
                        for (auto* m : g_Player.objects[i]->GetMeshes()) {
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            if (isBody) ImGui::TextUnformatted("body");
                            else if (bone >= 0) ImGui::Text("bone %d", bone);
                            else ImGui::TextUnformatted("loose");
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(m->texName.empty() ? "-"
                                                                      : m->texName.c_str());
                            ImGui::TableNextColumn();
                            ImGui::Text("%d", m->frameIndex);
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(m->hasWeights ? "yes" : "no");
                            ImGui::TableNextColumn();
                            ImGui::Text("%zu", m->vertices.size());
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(
                                IsPlayerBodyMesh(*m, g_Player.textures) ? "yes"
                                                                        : "skipped");
                        }
                    }
                    ImGui::EndTable();
                }
            }
            ImGui::End();
        }

        // File browser
        g_FileBrowser.Render();
        } else {
            // F1 hides everything; leave one hint so the UI is recoverable.
            auto* dl = ImGui::GetForegroundDrawList();
            dl->AddText(ImVec2(12.0f, (float)winH - 22.0f),
                        IM_COL32(190, 190, 200, 150), "F1 - show interface");
        }

        // Doorway prompt. Drawn on the foreground list so it survives F1, and
        // centred low like the game's own use-prompt.
        if (state.playMode && g_Player.loaded && !g_Player.usableClips.empty()) {
            char cb[192];
            snprintf(cb, sizeof(cb), "[ ]  clip %d/%d  %s",
                     g_Player.currentClip + 1, (int)g_Player.usableClips.size(),
                     s_playerClipName.c_str());
            ImGui::GetForegroundDrawList()->AddText(
                ImVec2(12.0f, (float)winH - 40.0f),
                IM_COL32(190, 190, 200, 170), cb);
        }
        if (state.playMode && s_zoneLinkHere >= 0 && !s_zonePrompt.empty()) {
            char buf[192];
            snprintf(buf, sizeof(buf), "E  -  %s", s_zonePrompt.c_str());
            auto* dl = ImGui::GetForegroundDrawList();
            const ImVec2 sz = ImGui::CalcTextSize(buf);
            const ImVec2 at((winW - sz.x) * 0.5f, (float)winH * 0.78f);
            dl->AddRectFilled(ImVec2(at.x - 12.0f, at.y - 6.0f),
                              ImVec2(at.x + sz.x + 12.0f, at.y + sz.y + 6.0f),
                              IM_COL32(0, 0, 0, 150), 4.0f);
            dl->AddText(at, IM_COL32(235, 235, 245, 255), buf);
        }

        // ── MessageBox Render ───────────────────────────────────────────────
        if (s_showMessageBox) {
            s_messageBoxTimer -= ImGui::GetIO().DeltaTime;
            if (s_messageBoxTimer <= 0.0f) {
                s_showMessageBox = false;
            } else {
                auto* dl = ImGui::GetForegroundDrawList();
                const ImVec2 sz = ImGui::CalcTextSize(s_messageBoxText.c_str());
                const ImVec2 at((winW - sz.x) * 0.5f, (float)winH * 0.85f);
                dl->AddRectFilled(ImVec2(at.x - 20.0f, at.y - 12.0f),
                                  ImVec2(at.x + sz.x + 20.0f, at.y + sz.y + 12.0f),
                                  IM_COL32(20, 20, 30, 220), 8.0f);
                dl->AddRect(ImVec2(at.x - 20.0f, at.y - 12.0f),
                            ImVec2(at.x + sz.x + 20.0f, at.y + sz.y + 12.0f),
                            IM_COL32(200, 200, 200, 100), 8.0f);
                dl->AddText(at, IM_COL32(255, 255, 255, 255), s_messageBoxText.c_str());
            }
        }

        // ── SavePoint Render ────────────────────────────────────────────────
        if (s_showSaveMenu) {
            ImGui::SetNextWindowPos(ImVec2((float)winW * 0.5f, (float)winH * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            if (ImGui::Begin("Save Point", &s_showSaveMenu, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
                ImGui::Text("Would you like to save your progress?");
                ImGui::Separator();
                if (ImGui::Button("Save Game", ImVec2(120, 30))) {
                    std::cout << "[SavePoint] Game Saved! (Dummy)\n";
                    s_showSaveMenu = false;
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 30))) {
                    s_showSaveMenu = false;
                }
            }
            ImGui::End();
        }

        // Decide who owns the mouse for the *next* frame's event loop. Panels and
        // active widgets win; a hovered (but not dragged) gizmo does not.
        {
            const bool overPanel = ImGui::IsWindowHovered(
                ImGuiHoveredFlags_AnyWindow |
                ImGuiHoveredFlags_AllowWhenBlockedByPopup |
                ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
            viewportOwnsMouse = !overPanel && !ImGui::IsAnyItemActive()
                                && !gizmoUsing && !sphereDragging;
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        s_useDoorPressed = false;
        SDL_GL_SwapWindow(win);
    }

    m_Graphics.Shutdown();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}

