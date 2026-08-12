#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// The boot sequence and the menus.
//
// Game logic: no GL, no SDL, no ImGui. It decides *what* is on screen and what
// a keypress does; drawing it is the renderer's job.
//
// The screens themselves are data -- 40 XML files in SH.ARC, parsed by
// Core/UI/ScreenDef.h -- so this holds only what the data does not: the order
// of the boot stages, and how a button press moves between elements.
//
// Where the order came from: the front end is code, not data -- Origins' archive
// holds 88 object classes and not one of them is front-end -- so it was read out
// of the executable. `decomp/sles_r5900/` holds every function in SLES_551.47
// decompiled with the r5900 processor; `FUN_00151d60` is the dispatcher,
// `FUN_00151918` draws the flags and `FUN_00151fb8` the aspect list, and the
// tables they walk live at 0x00338A00 and 0x00338A5C. See docs/TODO.md §6c.
// ─────────────────────────────────────────────────────────────────────────────

#include <string>
#include <vector>

#include "ClimaxEngine/Core/UI/ScreenDef.h"

namespace ClimaxEngine {
namespace Game {

// The boot sequence, in the order the retail game actually runs it.
//
// Not invented: `FUN_00151d60` in `decomp/sles_r5900/` dispatches on a 12-byte
// state record at `0x00338A00`, and the handlers it reaches are
// `FUN_00151918` (the flag list), `FUN_00151fb8` (the aspect list) and
// `FUN_00152250` ("Logo"). Confirmed against the retail game on screen:
// copyright/loading, then language, then aspect, then the logo movie, then the
// menu.
//
// An earlier version of this enum had Logo first and language last. That was
// reconstructed from the UI XML, which describes screens but says nothing about
// order, and it was wrong in both directions.
enum class BootStage {
    Loading,         // the copyright plate, held while the first load runs
    LanguageSelect,  // five flags
    AspectSelect,    // 4:3 or widescreen; art is sho_aspect_**
    Logo,            // LOGOW.PSS / LOGON.PSS -- idents and content notice, one clip
    MainMenu,
    InGame,
};

// The five the game offers, in the order the flag row shows them.
//
// From the table at `0x00338A5C`, stride 12, name pointer first:
// sho_flg_GB, sho_flg_FR, sho_flg_IT, sho_flg_DE, sho_flg_ES. There is no US
// flag -- an earlier guess added one and reordered the rest -- and no Japanese
// flag either, even though Strings.Jap ships, because that build selects its
// language another way.
enum class Language { English, French, Italian, German, Spanish };

// The texture base name in the Startup container. Append "_h" for the
// highlighted variant.
const char *LanguageFlag(Language l);
// Which Strings.* file it wants.
const char *LanguageStrings(Language l);
int LanguageCount();

// Where the flag row goes, straight out of `FUN_00151918`: five 80x60 quads on
// one line, the first at (32, 192), each 92 px further along -- 12 px of gap
// plus the 80 px quad. Coordinates are in the 512x448 space the UI is authored
// in. Highlighted is drawn white, the rest at 0xC8505050.
struct FlagRowLayout {
    float x = 32.0f, y = 192.0f;
    float w = 80.0f, h = 60.0f;
    float step = 92.0f;
};
FlagRowLayout LanguageRow();

const char *BootStageName(BootStage s);

// What the player is doing this frame. Deliberately not SDL keycodes.
struct MenuInput {
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
    bool accept = false;
    bool cancel = false;
    bool anyKey = false;   // dismisses a timed screen early
    // Set by the caller when a video-driven stage's clip has played to the
    // end. FrontEnd owns no video state -- no GL, no decoder -- so it cannot
    // know this on its own.
    bool mediaEnded = false;
};

// Which screen is up and which element is highlighted.
//
// Navigation comes from the XML: a BUTTON names `onup`, `ondown`, `onaccept`
// and `oncancel`, each holding either another element's id on the same screen
// or the id of a screen to open. Which of the two it is cannot be told from the
// attribute, so the resolution order is: an element on this screen first, a
// screen second. That is the only reading under which `mainmenu.xml` works --
// `ondown="main_menu_load_game"` is a sibling, `onaccept="new_game_menu_screen"`
// is not.
class MenuState {
public:
    // `screens` maps a screen id to its parsed <SCREEN> element. The pointers
    // must outlive this object.
    void SetScreens(std::vector<std::pair<std::string, const UI::Element *>> screens);

    // Opens a screen by id. False when there is no such screen.
    bool Open(const std::string &screenId);

    const std::string &ScreenId() const { return m_screenId; }
    const UI::Element *Screen() const { return m_screen; }
    const std::string &ActiveId() const { return m_activeId; }
    const UI::Element *Active() const;

    // Applies one frame of input. Returns the id of whatever was accepted and
    // could not be resolved -- a command for the caller to act on, such as
    // starting a game -- or an empty string.
    std::string Update(const MenuInput &in);

private:
    const UI::Element *FindScreen(const std::string &id) const;
    void SelectDefault();

    std::vector<std::pair<std::string, const UI::Element *>> m_screens;
    std::string m_screenId;
    const UI::Element *m_screen = nullptr;
    std::string m_activeId;
    std::vector<std::string> m_back;   // screen stack, for cancel
};

// Drives the stages before the menu, and the menu after them.
class FrontEnd {
public:
    // Fallback timeout for the Logo stage, used only if no video loaded (the
    // real advance is `MenuInput::mediaEnded`, driven by the LOGOW/LOGON clip
    // actually finishing -- 16.76 s in the retail files). Without a video this
    // stops the boot sequence from hanging forever with nothing on screen.
    float logoSeconds = 17.0f;
    // How long the copyright plate holds. In the retail game this is however
    // long the first load takes, not a timer; with nothing to load yet it needs
    // a duration, and a keypress skips it as it does there.
    float loadingSeconds = 3.0f;

    // Set false to make the language stage appear; the game shows it once, on
    // first boot, and remembers the answer.
    bool languageChosen = false;
    bool aspectChosen = false;

    // What the player picked on those two screens.
    Language language = Language::English;
    bool widescreen = true;
    int languageIndex = 0;   // cursor on the flag row

    BootStage Stage() const { return m_stage; }
    MenuState &Menu() { return m_menu; }

    // Restart from the first stage.
    void Reset();

    // One frame. Returns a command the caller must act on -- "start a new
    // game", "load" -- or an empty string. Stage changes are not commands;
    // read Stage() for those.
    std::string Update(float dt, const MenuInput &in);

private:
    BootStage m_stage = BootStage::Loading;
    float m_elapsed = 0.0f;
    MenuState m_menu;

    // Which stage follows `from`, honouring the two "already answered" skips.
    BootStage NextStage(BootStage from) const;
    void Enter(BootStage s);
};

} // namespace Game
} // namespace ClimaxEngine
