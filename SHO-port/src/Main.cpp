#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <cmath>
#include <glm/gtc/type_ptr.hpp>


#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"

#include "SHO/Core/Types.h"
#include "SHO/Core/EventManager.h"
#include "SHO/World/World.h"
#include "SHO/World/SceneQueue.h"
#include "SHO/Bridge/LevelBridge.h"
#include "SHO/Camera/CameraManager.h"
#include "SHO/Actor/PlayerBehaviour.h"
#include "SHO/Inventory/InventoryManager.h"
#include "SHO/Combat/CombatSystem.h"
#include "SHO/Puzzle/PuzzleManager.h"
#include "SHO/Progression/PlayerData.h"
#include "SHO/Audio/AudioEngine.h"
#include "SHO/Environment/FogSystem.h"
#include "SHO/Triggers/TriggerSystem.h"
#include "SHO/Core/MessageRelay.h"


#include "ClimaxEngine/Core/RWS/FileSystem/CArchiveManager.h"
#include "ClimaxEngine/Loader/Loader.h"
#include "ClimaxEngine/Render/ViewerState.h"
#include "ClimaxEngine/Render/PlayerModel.h"
#include "ClimaxEngine/Game/CharacterController.h"
#include "ClimaxEngine/Game/CameraLinks.h"
#include "ClimaxEngine/Game/ZoneLinks.h"
#include "ClimaxEngine/Game/ButtonTriggers.h"
#include "ClimaxEngine/Viewer/ViewerGraphics.h"
#include "ClimaxEngine/Viewer/ViewerAudio.h"

void InstallGLTextureSink();




// Forward declarations
extern std::map<std::string, GLuint> g_TextureMap;
extern CollisionMesh g_Collision;
extern std::vector<GameObject> g_GameObjects;
extern std::vector<LevelCamera> g_Cameras;
extern std::vector<AudioClip> g_Sounds;
extern PlayerModel g_Player;

glm::vec3   s_playerFeet(0.0f);
float       s_playerYaw = 0.0f;
std::string s_playerClipName;
bool        s_showPlayerPieces = false;

static std::string g_PrefsPath = "SHO-port.prefs";

void SaveArcPref(const std::string& arcPath) {
    if (arcPath.empty()) return;
    std::ofstream f(g_PrefsPath, std::ios::trunc);
    if (f) f << arcPath << "\n";
}

std::string LoadArcPref() {
    std::ifstream f(g_PrefsPath);
    std::string line;
    if (f && std::getline(f, line) && !line.empty()) return line;
    return {};
}

namespace {

GLuint g_MainSkyProg = 0;
GLuint g_MainSkyVao  = 0;

void InitSkyShader() {
    const char* vSky = R"(
    #version 330 core
    const vec2 pos[6] = vec2[](
        vec2(-1,-1), vec2(1,-1), vec2(-1,1),
        vec2(-1,1),  vec2(1,-1), vec2(1,1)
    );
    out vec2 uv;
    void main() {
        uv = pos[gl_VertexID] * 0.5 + 0.5;
        gl_Position = vec4(pos[gl_VertexID], 0.9999, 1.0);
    }
    )";
    const char* fSky = R"(
    #version 330 core
    in vec2 uv;
    out vec4 frag;
    uniform vec3 skyTop;
    uniform vec3 skyBot;
    void main() {
        frag = vec4(mix(skyBot, skyTop, clamp(uv.y, 0.0, 1.0)), 1.0);
    }
    )";

    auto compile = [](GLenum type, const char* src) -> GLuint {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        return s;
    };

    GLuint vs = compile(GL_VERTEX_SHADER, vSky);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fSky);
    g_MainSkyProg = glCreateProgram();
    glAttachShader(g_MainSkyProg, vs);
    glAttachShader(g_MainSkyProg, fs);
    glLinkProgram(g_MainSkyProg);
    glDeleteShader(vs);
    glDeleteShader(fs);

    glGenVertexArrays(1, &g_MainSkyVao);
}

struct FogParticle {
    glm::vec3 pos;
    float size;
    float rotation;
    float rotSpeed;
    float alpha;
};

class VolumetricFogRenderer {
public:
    GLuint vao = 0, vbo = 0, prog = 0, fogFallbackTex = 0;
    std::vector<FogParticle> particles;

    void Init() {
        const char* vs = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in vec2 aUv;
        out vec2 uv;
        out float vAlpha;
        uniform mat4 mvp;
        uniform vec3 center;
        uniform vec3 camRight;
        uniform vec3 camUp;
        uniform float size;
        uniform float rot;
        uniform float alpha;

        void main() {
            uv = aUv;
            vAlpha = alpha;
            float c = cos(rot), s = sin(rot);
            vec2 local = aPos.xy * size;
            vec2 rotated = vec2(local.x * c - local.y * s, local.x * s + local.y * c);
            vec3 worldPos = center + camRight * rotated.x + camUp * rotated.y;
            gl_Position = mvp * vec4(worldPos, 1.0);
        }
        )";

        const char* fs = R"(
        #version 330 core
        in vec2 uv;
        in float vAlpha;
        out vec4 fragColor;
        uniform sampler2D fogTex;
        uniform vec3 fogCol;

        void main() {
            vec4 t = texture(fogTex, uv);
            float d = length(uv - vec2(0.5)) * 2.0;
            float falloff = clamp(1.0 - d * d, 0.0, 1.0);
            float a = t.a * vAlpha * falloff * 0.45;
            if (a < 0.005) discard;
            fragColor = vec4(fogCol * 1.12, a);
        }
        )";

        auto compile = [](GLenum type, const char* src) -> GLuint {
            GLuint s = glCreateShader(type);
            glShaderSource(s, 1, &src, nullptr);
            glCompileShader(s);
            return s;
        };

        GLuint v = compile(GL_VERTEX_SHADER, vs);
        GLuint f = compile(GL_FRAGMENT_SHADER, fs);
        prog = glCreateProgram();
        glAttachShader(prog, v);
        glAttachShader(prog, f);
        glLinkProgram(prog);
        glDeleteShader(v);
        glDeleteShader(f);

        float quad[] = {
            -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,
             0.5f, -0.5f, 0.0f,  1.0f, 0.0f,
            -0.5f,  0.5f, 0.0f,  0.0f, 1.0f,
            -0.5f,  0.5f, 0.0f,  0.0f, 1.0f,
             0.5f, -0.5f, 0.0f,  1.0f, 0.0f,
             0.5f,  0.5f, 0.0f,  1.0f, 1.0f,
        };

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);

        // Fallback procedural smooth cloud texture
        glGenTextures(1, &fogFallbackTex);
        glBindTexture(GL_TEXTURE_2D, fogFallbackTex);
        const int S = 64;
        std::vector<uint8_t> px(S * S * 4);
        for (int y = 0; y < S; ++y) {
            for (int x = 0; x < S; ++x) {
                float fx = (float)x / (float)(S - 1) - 0.5f;
                float fy = (float)y / (float)(S - 1) - 0.5f;
                float d = sqrtf(fx * fx + fy * fy) * 2.0f;
                float a = std::max(0.0f, 1.0f - d * d);
                a = a * a;
                int idx = (y * S + x) * 4;
                px[idx + 0] = 255;
                px[idx + 1] = 255;
                px[idx + 2] = 255;
                px[idx + 3] = (uint8_t)(a * 255.0f);
            }
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, S, S, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Spawn 40 smooth 3D fog cloud particles around player
        particles.resize(40);
        for (size_t i = 0; i < particles.size(); ++i) {
            float ang = (float)i / (float)particles.size() * 6.28318f;
            float r = 3.0f + (float)(rand() % 180) / 10.0f;
            particles[i].pos = glm::vec3(cosf(ang) * r, -0.4f + (float)(rand() % 40) / 10.0f, sinf(ang) * r);
            particles[i].size = 6.0f + (float)(rand() % 50) / 10.0f;
            particles[i].rotation = (float)(rand() % 628) / 100.0f;
            particles[i].rotSpeed = ((float)(rand() % 160 - 80) / 1000.0f);
            particles[i].alpha = 0.20f + (float)(rand() % 15) / 100.0f;
        }
    }

    void UpdateAndRender(float dt, const glm::vec3& playerPos, const glm::mat4& mvp, const glm::mat4& viewMat, GLuint tex) {
        if (!prog || !vao) return;

        glm::vec3 camRight = glm::vec3(viewMat[0][0], viewMat[1][0], viewMat[2][0]);
        glm::vec3 camUp    = glm::vec3(viewMat[0][1], viewMat[1][1], viewMat[2][1]);

        glUseProgram(prog);
        glUniformMatrix4fv(glGetUniformLocation(prog, "mvp"), 1, GL_FALSE, glm::value_ptr(mvp));
        glUniform3fv(glGetUniformLocation(prog, "camRight"), 1, glm::value_ptr(camRight));
        glUniform3fv(glGetUniformLocation(prog, "camUp"), 1, glm::value_ptr(camUp));
        glUniform3fv(glGetUniformLocation(prog, "fogCol"), 1, state.fogColor);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex ? tex : fogFallbackTex);
        glUniform1i(glGetUniformLocation(prog, "fogTex"), 0);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glBindVertexArray(vao);

        const GLint uCenter = glGetUniformLocation(prog, "center");
        const GLint uSize   = glGetUniformLocation(prog, "size");
        const GLint uRot    = glGetUniformLocation(prog, "rot");
        const GLint uAlpha  = glGetUniformLocation(prog, "alpha");

        for (auto& p : particles) {
            p.pos.x += 0.22f * dt;
            p.pos.z += 0.11f * dt;
            p.rotation += p.rotSpeed * dt;

            glm::vec3 d = p.pos - playerPos;
            if (d.x > 22.0f) p.pos.x -= 44.0f;
            if (d.x < -22.0f) p.pos.x += 44.0f;
            if (d.z > 22.0f) p.pos.z -= 44.0f;
            if (d.z < -22.0f) p.pos.z += 44.0f;

            glUniform3fv(uCenter, 1, glm::value_ptr(p.pos));
            glUniform1f(uSize, p.size);
            glUniform1f(uRot, p.rotation);
            glUniform1f(uAlpha, p.alpha);

            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        glBindVertexArray(0);
        glDepthMask(GL_TRUE);
    }
};

} // namespace






int main(int argc, char** argv) {
    std::cout << "========================================" << std::endl;
    std::cout << "Silent Hill: Origins - Native Port Runner" << std::endl;
    std::cout << "========================================" << std::endl;

    std::string arcPath = "game-iso/SHO/SH.ARC";
    std::string currentLevelName = "IntroRoad";

    if (argc >= 2) {
        std::string arg1 = argv[1];
        if (arg1.size() > 4 && arg1.substr(arg1.size() - 4) == ".arc") {
            arcPath = arg1;
            if (argc >= 3) currentLevelName = argv[2];
        } else {
            currentLevelName = arg1;
        }
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        std::cerr << "[ERROR] SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window* window = SDL_CreateWindow(
        "Silent Hill: Origins (SHO-port Native)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );

    if (!window) {
        std::cerr << "[ERROR] SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    SDL_GL_SetSwapInterval(1); // VSync

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "[ERROR] GLEW initialization failed" << std::endl;
        return 1;
    }

    // Initialize ImGui Context and Renderer Backend
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForOpenGL(window, glContext);
    ImGui_ImplOpenGL3_Init("#version 330");

    InstallGLTextureSink();
    InitSkyShader();

    // Mount Archive



    if (!ClimaxEngine::RWS::FileSystem::CArchiveManager::GetInstance().Mount(arcPath)) {
        std::cerr << "[ERROR] Failed to mount game archive: " << arcPath << std::endl;
        return 1;
    }
    std::cout << "[ARC] Mounted " << arcPath << " successfully." << std::endl;
    SHO::Audio::AudioEngine::GetInstance().Init();
    SHO::Environment::FogSystem::GetInstance().Init();

    // Initialize SHO-port Subsystems


    auto& world = SHO::World::World::GetInstance();
    world.Init();

    auto& puzzleMgr = SHO::Puzzle::PuzzleManager::GetInstance();
    puzzleMgr.Init();
    puzzleMgr.LoadPuzzlesFromArchive(arcPath);

    // Character Physics Body
    ClimaxEngine::Game::CharacterController body;
    std::string arrivedFromZone = "";
    std::vector<ClimaxEngine::Game::ZoneLink> zoneLinks;
    ClimaxEngine::Game::CameraSwitcher switcher;

    auto LoadLevelHelper = [&](const std::string& levelName) {
        auto* arc = ClimaxEngine::RWS::FileSystem::CArchiveManager::GetInstance().GetFirstArchive();
        if (!arc) return false;
        int levelIdx = arc->Find(levelName);
        if (levelIdx < 0) {
            std::cerr << "[WARN] Level '" << levelName << "' not found." << std::endl;
            return false;
        }

        std::cout << "[LEVEL] Loading level: " << levelName << " (Index " << levelIdx << ")" << std::endl;
        LoadLevelFromArc(levelIdx);
        currentLevelName = levelName;

        // Populate SHO::World via LevelBridge
        SHO::Bridge::LevelBridge::PopulateWorldFromLevel(
            g_GameObjects,
            g_Cameras,
            g_Collision,
            state.fogStart,
            state.fogEnd,
            state.fogDensity,
            glm::vec3(state.fogColor[0], state.fogColor[1], state.fogColor[2])
        );

        // Build Doorway Links & Camera Switches
        zoneLinks = ClimaxEngine::Game::BuildZoneLinks(g_GameObjects);
        switcher.Reset(ClimaxEngine::Game::BuildCameraSwitches(g_GameObjects, g_Cameras));

        // Spawn placement: Check zone spawn, player spawner, or camera fallbacks
        glm::vec3 spawn(0.0f), facing(0.0f, 0.0f, 1.0f);
        if (!arrivedFromZone.empty() && ClimaxEngine::Game::FindZoneSpawn(g_GameObjects, arrivedFromZone, spawn, facing)) {
            body.position = spawn;
            s_playerYaw = glm::degrees(atan2f(-facing.x, -facing.z));
        } else if (ClimaxEngine::Game::FindPlayerSpawn(g_GameObjects, spawn)) {
            body.position = spawn;
        } else if (!g_Cameras.empty()) {
            body.position = g_Cameras[0].position - glm::vec3(0, 1.0f, 2.0f);
        }
        body.velocity = glm::vec3(0.0f);
        body.SnapToGround(g_Collision);
        s_playerFeet = glm::vec3(body.position.x, body.position.y - body.radius, body.position.z);

        // Initial camera cut for player position
        int initCam = switcher.Update(body.position);
        if (initCam >= 0 && initCam < (int)g_Cameras.size()) {
            state.activeCamera = initCam;
        } else if (!g_Cameras.empty()) {
            state.activeCamera = 0;
        }

        // Authentic PS2 Silent Hill misty atmosphere
        state.enableFog = true;
        state.fogMode = 0;
        state.fogStart = 2.0f;
        state.fogEnd   = 24.0f;
        state.fogDensity = 0.05f;
        state.fogColor[0] = 0.44f;
        state.fogColor[1] = 0.47f;
        state.fogColor[2] = 0.52f;

        return true;
    };


    // Configure Pure Game Presentation (No debug markers/labels, pure cinematic rendering)
    state.playMode = true;
    state.autoCameras = true;
    state.showUI = false;
    state.showClumps = false;
    state.showGameObjects = false;
    state.showObjectLabels = false;
    state.showPivotGizmo = false;
    state.showCollision = false;
    state.showBoneOverlay = false;
    state.showWireframe = false;
    state.enableLights = true;
    state.enableFog = true;
    state.skyGradient = true;

    ClimaxEngine::Viewer::ScanAudioLibrary();
    for (size_t i = 0; i < ClimaxEngine::Viewer::g_AudioLibrary.size(); ++i) {
        if (ClimaxEngine::Viewer::g_AudioLibrary[i].name == "SCN01" ||
            ClimaxEngine::Viewer::g_AudioLibrary[i].name == "DARKTOWN" ||
            ClimaxEngine::Viewer::g_AudioLibrary[i].name == "TOWNSAN") {
            ClimaxEngine::Viewer::PlayLibraryEntry((int)i);
            break;
        }
    }

    LoadLevelHelper(currentLevelName);


    // Load Travis Player Model
    LoadPlayerModel("CPlayerBehaviour.Travis");


    // Graphics Pipeline
    ClimaxEngine::Viewer::ViewerGraphics graphics;
    graphics.Init();

    bool running = true;
    uint32_t lastTicks = SDL_GetTicks();
    bool actionKeyPressed = false;
    std::string zonePrompt = "";

    while (running) {
        uint32_t currentTicks = SDL_GetTicks();
        float dt = (currentTicks - lastTicks) / 1000.0f;
        if (dt > 0.05f) dt = 0.05f; // Clamp delta
        lastTicks = currentTicks;

        actionKeyPressed = false;

        // Input Handling
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                } else if (event.key.keysym.sym == SDLK_f) {
                    state.enableFlashlight = !state.enableFlashlight;
                    state.enableLights = state.enableFlashlight;
                    SHO::Audio::AudioEngine::GetInstance().PlayFlashlightClick();
                    std::cout << "[FLASHLIGHT] " << (state.enableFlashlight ? "ON" : "OFF") << std::endl;
                } else if (event.key.keysym.sym == SDLK_e || event.key.keysym.sym == SDLK_RETURN) {
                    actionKeyPressed = true;
                }
            }
        }



        // Start ImGui Frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        const uint8_t* keys = SDL_GetKeyboardState(nullptr);

        // Movement vectors in camera-relative space
        glm::vec3 flat_fwd(0, 0, 1);
        glm::vec3 right(1, 0, 0);

        int activeCamIdx = state.activeCamera;
        if (activeCamIdx >= 0 && activeCamIdx < (int)g_Cameras.size()) {
            glm::vec3 cFwd = g_Cameras[activeCamIdx].forward;
            cFwd.y = 0.0f;
            if (glm::length(cFwd) > 1e-3f) flat_fwd = glm::normalize(cFwd);
            right = glm::normalize(glm::cross(flat_fwd, glm::vec3(0, 1, 0)));
        }

        glm::vec3 wish(0.0f);
        if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP])    wish += flat_fwd;
        if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN])  wish -= flat_fwd;
        if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT])  wish -= right;
        if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) wish += right;

        if (glm::length(wish) > 1e-4f) wish = glm::normalize(wish);

        bool isRunning = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
        float moveSpeed = isRunning ? 3.8f : 1.9f;

        body.Step(g_Collision, wish * moveSpeed * dt, dt);

        s_playerFeet = glm::vec3(body.position.x, body.position.y - body.radius, body.position.z);

        if (glm::length(wish) > 1e-3f) {
            float wantYaw = glm::degrees(atan2f(wish.x, wish.z));
            float d = wantYaw - s_playerYaw;
            while (d >  180.0f) d -= 360.0f;
            while (d < -180.0f) d += 360.0f;
            s_playerYaw += d * glm::min(1.0f, dt * 12.0f);
        }

        g_Player.Advance(dt);

        if (g_Player.loaded) {
            bool moving = glm::length(wish) > 1e-3f;
            int wantClip = moving ? (isRunning ? g_Player.runClip : g_Player.walkClip) : g_Player.idleClip;
            if (wantClip < 0) wantClip = g_Player.idleClip;
            if (wantClip >= 0 && wantClip != g_Player.currentClip) {
                s_playerClipName = g_Player.PlayClipAt(wantClip);
            }

            if (moving) {
                static float s_footstepTimer = 0.0f;
                s_footstepTimer += dt;
                float stepInterval = isRunning ? 0.32f : 0.48f;
                if (s_footstepTimer >= stepInterval) {
                    s_footstepTimer = 0.0f;
                    SHO::Audio::AudioEngine::GetInstance().PlayFootstep(
                        (currentLevelName.find("HO_") != std::string::npos || currentLevelName.find("Room") != std::string::npos)
                        ? SHO::Audio::SurfaceMaterial::Tile
                        : SHO::Audio::SurfaceMaterial::Road
                    );
                }
            }

        }

        // Update Travis Flashlight (SH2/SH3 Style Cinematic Dynamic Spotlight from chest pocket)
        glm::vec3 facing(sinf(glm::radians(s_playerYaw)), 0.0f, cosf(glm::radians(s_playerYaw)));
        state.flashlightPos = glm::vec3(body.position.x, body.position.y + 1.05f, body.position.z) + facing * 0.15f;
        state.flashlightDir = glm::normalize(facing + glm::vec3(0.0f, -0.06f, 0.0f));

        state.flashlightColor = glm::vec3(1.0f, 0.96f, 0.88f);
        state.flashlightRange = 14.0f;
        state.flashlightInnerAngle = cosf(glm::radians(16.0f));
        state.flashlightOuterAngle = cosf(glm::radians(38.0f));


        // Boundary constraint behind the truck on IntroRoad (Travis cannot walk away from Silent Hill)
        if (currentLevelName == "IntroRoad") {

            if (body.position.z > 5.5f) {
                body.position.z = 5.5f;
                body.velocity.z = 0.0f;
            }
        }

        // Camera Switching

        int cutCam = switcher.Update(body.position);
        if (cutCam >= 0 && cutCam < (int)g_Cameras.size()) {
            state.activeCamera = cutCam;
        } else if (!g_Cameras.empty() && state.activeCamera >= 0 && state.activeCamera < (int)g_Cameras.size()) {
            // Adaptive proximity camera selection if player moves far from current camera
            float distToCurCam = glm::distance(g_Cameras[state.activeCamera].position, body.position);
            if (distToCurCam > 18.0f) {
                float bestDist = 1e9f;
                int bestIdx = state.activeCamera;
                for (size_t c = 0; c < g_Cameras.size(); ++c) {
                    float d = glm::distance(g_Cameras[c].position, body.position);
                    if (d < bestDist && d > 2.5f) {
                        bestDist = d;
                        bestIdx = (int)c;
                    }
                }
                if (bestIdx >= 0) state.activeCamera = bestIdx;
            }
        }

        // Doorway Check
        int linkIdx = ClimaxEngine::Game::ZoneLinkAt(zoneLinks, body.position, 1.0f);
        if (linkIdx >= 0 && linkIdx < (int)zoneLinks.size()) {
            zonePrompt = zoneLinks[linkIdx].toZone;
            if (actionKeyPressed) {
                arrivedFromZone = zoneLinks[linkIdx].fromZone;
                std::cout << "[ZONE] Door transition: " << arrivedFromZone << " -> " << zonePrompt << std::endl;
                if (!g_Sounds.empty()) {
                    for (const auto& snd : g_Sounds) {
                        if (snd.name.find("Door") != std::string::npos || snd.name.find("Open") != std::string::npos) {
                            ClimaxEngine::Viewer::PlayAudioClip(snd);
                            break;
                        }
                    }
                }
                LoadLevelHelper(zonePrompt);
            }
        } else {
            zonePrompt.clear();
        }


        // Update SHO World & Puzzles
        world.Update(dt);
        puzzleMgr.Update(dt);

        // Render 3D Scene
        int w, h;
        SDL_GL_GetDrawableSize(window, &w, &h);
        glViewport(0, 0, w, h);

        const glm::vec3 subject(body.position.x, body.position.y + 1.2f, body.position.z);
        glm::vec3 eye(0.0f), look(0, 0, 1);
        float fov = 50.0f;

        if (state.activeCamera >= 0 && state.activeCamera < (int)g_Cameras.size()) {
            const auto& cam = g_Cameras[state.activeCamera];
            fov = cam.fovDeg > 5.0f ? cam.fovDeg : 50.0f;
            ClimaxEngine::Game::ResolveCameraView(g_Collision, cam, subject, eye, look);
        } else {
            eye = body.position + glm::vec3(0, 2.5f, -4.0f);
            look = glm::normalize(subject - eye);
        }

        glm::mat4 viewMatrix = glm::lookAt(eye, eye + look, glm::vec3(0, 1, 0));
        glm::mat4 projMatrix = glm::perspective(glm::radians(fov), (float)w / (float)h, 0.1f, 250.0f);
        glm::mat4 mvp = projMatrix * viewMatrix;

        size_t totalMeshes = ClimaxEngine::SG::CSceneObjectRegistrar::GetInstance().GetObjects().size();
        graphics.RenderFrame(w, h, w, h, mvp, eye, look, totalMeshes);

        // In-game HUD (Clean & cinematic)



        static bool showDebugInfo = false;
        if (keys[SDL_SCANCODE_TAB]) showDebugInfo = true;

        if (showDebugInfo) {
            ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(0.6f);
            ImGui::Begin("StatusHUD", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
            ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "SILENT HILL ORIGINS");
            ImGui::Text("Location: %s", currentLevelName.c_str());
            ImGui::Text("Camera: %d / %zu", state.activeCamera, g_Cameras.size());
            ImGui::TextColored(state.enableLights ? ImVec4(1.0f, 0.9f, 0.3f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                               "Flashlight [F]: %s", state.enableLights ? "ON" : "OFF");
            ImGui::End();
        }

        if (!zonePrompt.empty()) {
            ImGui::SetNextWindowPos(ImVec2((float)w * 0.5f - 120, (float)h - 70), ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(0.8f);
            ImGui::Begin("DoorPrompt", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), " [E] %s ", zonePrompt.c_str());
            ImGui::End();
        }


        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();

    std::cout << "[EXIT] Clean shutdown complete." << std::endl;
    return 0;
}
