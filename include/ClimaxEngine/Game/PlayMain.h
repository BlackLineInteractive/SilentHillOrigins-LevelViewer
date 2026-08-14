#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// The front end, as the first half of one program.
//
// This used to be its own executable, `climax-play`, and the split was
// artificial: the boot sequence, the menu and the level are one game. What the
// separate target was really enforcing is that the game code takes no ImGui
// dependency, and that is now enforced where it belongs -- in `climax-game`,
// which still links neither SDL nor GL.
//
// RunFrontEnd owns the window from the splash through to the moment the scene
// queue reaches its handover command (`SceneCmd::HandOver`, id 0x0A -- the
// game's own word for control returning to the player). It does not tear the
// window down at that point; it hands it over still alive, so there is one
// window for the whole session and no flicker between the menu and the level.
//
// If the player quits out of the menu instead, `startGame` stays false and the
// window is destroyed here, exactly as before.
// ─────────────────────────────────────────────────────────────────────────────

#include <string>

struct SDL_Window;
typedef void *SDL_GLContext;

namespace ClimaxEngine {
namespace Game {

struct FrontEndExit {
    // False means the player closed the window or asked to quit; there is
    // nothing to hand over and the caller should just exit.
    bool startGame = false;

    // The scene the queue was asked for. FUN_001CF718 resets 22 subsystems and
    // calls FUN_00179FB8(world, "IntroRoad", ...), so on a new game this is
    // "IntroRoad".
    std::string firstScene;

    // What the front end mounted, so the caller does not guess at it again.
    std::string archive;

    // The two answers the boot records collect. The aspect record always asks
    // (FUN_00151F48 returns 1 unconditionally); the language record is skipped
    // outright on a Japanese build.
    bool widescreen = true;
    int  language = 0;

    // Alive and current when startGame is true. Null otherwise.
    SDL_Window   *window = nullptr;
    SDL_GLContext context = nullptr;
};

// Returns a process exit code. Anything non-zero is a failure to start at all
// (no archive, no GL) and `exit` is not meaningful.
int RunFrontEnd(int argc, char **argv, FrontEndExit &exit);

} // namespace Game
} // namespace ClimaxEngine
