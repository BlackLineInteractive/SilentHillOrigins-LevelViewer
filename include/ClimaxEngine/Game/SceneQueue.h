#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// Loading a scene, as the game does it.
//
// `FUN_00179FB8` does not load anything. It pushes commands: 0x1C-byte nodes
// onto a linked list at `world + 0x18`. `FUN_00179D60` drains that list once
// per frame and switches on the node's id (`node[2]`). A handler returning 0
// stops the drain for this frame -- the node stays at the head and runs again
// next frame -- and returning non-zero removes it and lets the next one run.
//
// That is the whole shape, and it is why a level change is not a stall: the
// fade, the unload, the file read and the bring-up are separate steps spread
// across frames, each free to take as long as it needs.
//
// The ids are the cases of `FUN_00179D60`'s switch, verbatim:
//
//     0x00  FUN_0017AA58      0x09  FUN_0017B5B0
//     0x01  FUN_0017B398      0x0A  FUN_0017B780
//     0x02  FUN_0017B448      0x0B  FUN_0017B680
//     0x03  FUN_0017B488      0x0C  FUN_0017A6B8
//     0x04  FUN_0017B510      0x0D  FUN_0017A7A8
//     0x05  FUN_0017B2E0      0x0E  FUN_0017B6C0
//     0x06  FUN_0017A900      0x0F  FUN_0017B7A0
//     0x08  FUN_0017B540      0x10  FUN_0017B848
//                             0x11  FUN_0017B890
//
// Note there is no case 7: the switch skips it. And 0x0C and 0x0D are two
// different handlers, not one -- an earlier reading of this queue had them as a
// single "fade" step, which is wrong.
//
// Game logic: no GL, no SDL. Everything that touches a file, the screen or the
// audio device goes through SceneQueueHost.
// ─────────────────────────────────────────────────────────────────────────────

#include <deque>
#include <string>

namespace ClimaxEngine {
namespace Game {

enum class SceneCmd {
    Begin        = 0x00,   // FUN_0017AA58
    LoadTextures = 0x01,   // FUN_0017B398 -- "%s.txd" and "%s-%s.txd"
    ReadHeader   = 0x02,   // FUN_0017B448
    Prepare      = 0x03,   // FUN_0017B488
    Step4        = 0x04,   // FUN_0017B510
    ClearWorld   = 0x05,   // FUN_0017B2E0
    ReleaseRes   = 0x06,   // FUN_0017A900
    Instantiate  = 0x08,   // FUN_0017B540 -> FUN_001777E8
    FinishTeardown = 0x09, // FUN_0017B5B0
    HandOver     = 0x0A,   // FUN_0017B780 -- control returns to the player
    ResetSystems = 0x0B,   // FUN_0017B680
    FadeOut      = 0x0C,   // FUN_0017A6B8
    FadeIn       = 0x0D,   // FUN_0017A7A8
    StepE        = 0x0E,   // FUN_0017B6C0
    StepF        = 0x0F,   // FUN_0017B7A0
    FreeTemp     = 0x10,   // FUN_0017B848
    LevelAudio   = 0x11,   // FUN_0017B890 -> FUN_001D9A38
};

const char *SceneCmdName(SceneCmd c);

// One queued node. The original carries a name pointer and two parameter
// blocks; the only one any handler reads by name is the scene.
struct SceneNode {
    SceneCmd cmd = SceneCmd::Begin;
    std::string arg;   // scene name, for the commands that take one
};

// What the queue needs from outside. Each call is one handler; returning false
// is the handler returning 0 -- "not finished, run me again next frame".
class SceneQueueHost {
public:
    virtual ~SceneQueueHost() = default;

    // 0x0C / 0x0D. The originals take a descriptor -- {flag, 0, 1.0f, 0…} at
    // 0x339F68 and 0x339F50 -- so a second is the duration both use.
    virtual bool FadeOut(float seconds) = 0;
    virtual bool FadeIn(float seconds) = 0;

    // 0x05 / 0x06 / 0x09: tear the old scene down.
    virtual bool ClearWorld() = 0;
    virtual bool ReleaseResources() = 0;
    virtual bool FinishTeardown() = 0;

    // 0x01. Not the level: FUN_0017B398 formats "%s.txd" and, when it has a
    // second name, "%s-%s.txd" -- the room's texture dictionary and the one
    // shared with the room it is being entered from. Those are the
    // `HO_1_ExamRoom.txd` / `HO_1_ExamRoom-HO_1_Lobby.txd` entries in SH.ARC.
    // (An earlier comment here said it opened "%s.ARC". That was wrong.)
    virtual bool LoadTextures(const std::string &scene,
                              const std::string &from) = 0;
    virtual bool ReadHeader() = 0;
    virtual bool Prepare() = 0;

    // 0x08: build the world out of what was read.
    virtual bool Instantiate(const std::string &scene) = 0;

    // 0x10 / 0x0B / 0x11 / 0x0A.
    virtual bool FreeTemporaries() = 0;
    virtual bool ResetSystems() = 0;
    virtual bool StartLevelAudio() = 0;
    virtual bool HandOver() = 0;

    // Every handler opens with the same gate: while the streamer is working
    // (`FUN_001EF8F8`) it returns 0 and the queue stalls on that node. One
    // check, checked once, rather than repeated in each step.
    virtual bool StreamBusy() { return false; }

    // 0x00 FUN_0017AA58: walks the world's resource slots against the node's
    // and matches them by name. Returns 0 until they line up.
    virtual bool MatchResources() { return true; }
    // 0x04 FUN_0017B510: resolves the node's resource handle, if it has one.
    virtual bool ResolveResource() { return true; }
    // 0x0E FUN_0017B6C0: puts the subsystems back to a known state -- audio,
    // the character manager, the renderer's lists, the loading indicator.
    virtual bool ResetSubsystems() { return true; }
    // 0x0F FUN_0017B7A0: plays a clip as part of the change. It starts the
    // movie on the first visit and then returns 0 until the player reports it
    // finished, which is how a cutscene sits inside a scene transition.
    virtual bool PlaySceneMovie(const std::string &name) { (void)name; return true; }
};

class SceneQueue {
public:
    // `FUN_00179FB8`: push the sequence that swaps one scene for another.
    void LoadScene(const std::string &name, const std::string &from = std::string());

    // `FUN_00179D60`: drain until a handler says "not yet". Call once a frame.
    void Update(SceneQueueHost &host, float dt);

    bool Busy() const { return !m_nodes.empty(); }
    // What the queue is doing right now, for a log line.
    SceneCmd Current() const;
    const std::string &Scene() const { return m_scene; }

    // The 1.0f both transition descriptors carry.
    float fadeSeconds = 1.0f;

private:
    std::deque<SceneNode> m_nodes;
    std::string m_scene;
    std::string m_from;   // the room being left, for the shared .txd
};

} // namespace Game
} // namespace ClimaxEngine
