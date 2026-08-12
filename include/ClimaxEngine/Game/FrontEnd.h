#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// The boot sequence and the menus.
//
// Game logic: no GL, no SDL, no ImGui. It decides *what* is on screen and what
// a button press does; drawing it is the renderer's job.
//
// The boot sequence is not invented and not derived from the UI XML. It is the
// state machine in `SLES_551.47`, read out of `decomp/sles_r5900/` and out of
// `.data`. The C++ below is a transcription of those functions, one to one:
//
//   FUN_00152278  the front end object: 0x14 bytes, {refcount, vtable,
//                 stateIndex, cursor, done}, all three fields zeroed, then
//                 FUN_00151c00 enters state 0.
//   FUN_001515d0  the per-frame update -- read pad, rising edge, run the
//                 current state, and on completion exit / advance / enter.
//   FUN_00151810  the message handler; "Ended" (0x006BFF68) sets done = 1.
//   FUN_00151d60  the per-frame draw dispatch.
//   0x00338A00    the state table: five 12-byte records {kind, arg, skippable}.
//   0x00338A58    the language table: five 12-byte records {id, name, texture}.
//   0x00338A98    the aspect table: two 8-byte records {stringId, mode}.
//   FUN_00193f90  the pad-bit remap that produces the mask these states read.
//
// See docs/TODO.md §6c for the full reading.
// ─────────────────────────────────────────────────────────────────────────────

#include <cstdint>
#include <string>
#include <vector>

#include "ClimaxEngine/Core/UI/ScreenDef.h"

namespace ClimaxEngine {
namespace Game {

// ── the menu button mask ─────────────────────────────────────────────────────
//
// `MsgGetPadButtonMask` (0x00699DC8) does not hand out raw pad bits; it hands
// out the remapped mask that `FUN_00193f90` builds at pad+0x13C. The remap is
// listed here in full because every menu in the game reads these bits and the
// numbers look arbitrary until you have the mapping:
//
//     mask   raw pad   button        mask    raw pad   button
//     0x001  0x1000    up            0x040   0x0080    square
//     0x002  0x4000    down          0x080   0x0020    circle
//     0x004  0x8000    left          0x100   0x0800    start
//     0x008  0x2000    right         0x200   0x0100    select
//     0x010  0x0010    triangle      0x400   0x0004    L1
//     0x020  0x0040    cross         0x800   0x0008    R1
namespace Pad {
enum : uint32_t {
    Up       = 0x001,
    Down     = 0x002,
    Left     = 0x004,
    Right    = 0x008,
    Triangle = 0x010,
    Cross    = 0x020,
    Square   = 0x040,
    Circle   = 0x080,
    Start    = 0x100,
    Select   = 0x200,
    L1       = 0x400,
    R1       = 0x800,
};
}

// ── the state table at 0x00338A00 ────────────────────────────────────────────

// The first word of each record. `FUN_00151d60` and `FUN_001515d0` switch on
// it; it is *not* a position in the sequence, which is why an enum of stages in
// running order -- what this file used to hold -- could not represent it.
enum class BootKind {
    Language   = 0,   // the flag row
    Aspect     = 1,   // 4:3 / widescreen
    Movie      = 2,   // play record.arg, advance on the movie's "Ended" message
    MemoryCard = 3,   // MEMCARD_MSG_CHECK, advance when the card task goes idle
    Leave      = 4,   // terminal: FUN_001ff3f8, the front end is over
};

struct BootRecord {
    BootKind kind;
    const char *arg;    // record+4; only Movie uses it
    int skippable;      // record+8; only Movie reads it (cross/start skips)
};

// The five records, verbatim from the executable. Byte for byte:
//
//   [0] 0x00338A00  kind 0  arg 0           skippable 0
//   [1] 0x00338A0C  kind 3  arg 0           skippable 0
//   [2] 0x00338A18  kind 1  arg 0           skippable 0
//   [3] 0x00338A24  kind 2  arg "Logo"      skippable 0
//   [4] 0x00338A30  kind 4  arg 0           skippable 0
//
// So the retail order is: language, memory card, aspect, logo movie, leave.
// The logo is fourth, not first, and there is no copyright/loading plate in
// this object at all -- an earlier version of this file opened with one.
const BootRecord *BootTable();
int BootTableCount();
const char *BootKindName(BootKind k);

// ── languages ────────────────────────────────────────────────────────────────

// The flag row shows five, from the table at 0x00338A58 walked with stride 12.
enum class Language { English = 0, French, Italian, German, Spanish };

// The descriptor table at 0x0033EFB0 has seven entries of 20 bytes; the two
// past the flag row are Japanese (5) and Korean (6), reached only by a build
// that presets the language rather than by this screen. `FUN_00151e00` uses
// that: it skips the whole language state when the current language is 5.
enum { kLanguageJapanese = 5, kLanguageKorean = 6 };

// Texture base name in the Startup container, from record+4.
const char *LanguageFlag(Language l);
// Which Strings.* file it wants -- descriptor word 1 through "Strings.%s".
const char *LanguageStrings(Language l);
// Descriptor word 0: the language's name written in that language, as UTF-8.
// `FUN_00151918` draws this as the heading, and `FUN_00151e98` re-points the
// current language on every cursor move, so the heading changes as you scroll.
const char *LanguageOwnName(Language l);
// Descriptor word 2: "FontEUR" for 0..4, "FontJAP" for Japanese and Korean.
const char *LanguageFont(Language l);
int LanguageCount();

// ── layout, all of it out of .data ───────────────────────────────────────────
//
// Coordinates are in the 512-wide authored space the UI quads map onto.

// `FUN_00151918`: five 80x60 quads on one line, the first at (32,192), each
// 80+12 further along in x. Selected is drawn 0xFFFFFFFF, the rest 0xC8505050 --
// the flag itself is tinted, there is no separate frame or "_sel" art.
struct FlagRowLayout {
    float x = 32.0f, y = 192.0f;
    float w = 80.0f, h = 60.0f;
    float step = 92.0f;   // 0x00338A?? is not a table; the 12.0 gap is literal
};
FlagRowLayout LanguageRow();

// A text rectangle: `FUN_002174d8(size, ctx, text, rect, colour, 1)`.
struct TextRect {
    float x, y, w, h, size;
};
// 0x00338A48 = (0,128,512,32) at size 0x00338A3C = 16. Shared by both screens.
TextRect HeadingRect();
// 0x00338AA8 = (0,192,512,32) at size 0x00338A40 = 14, and `FUN_00151fb8`
// advances y by that size twice per row, so rows are 28 apart.
TextRect AspectRowRect();
float AspectRowStep();

// The two entries of the aspect list, from the 8-byte records at 0x00338A98:
// {"display_4x3", 0} and {"display_ws", 1}. They are string-table ids, not
// textures. The second word is what `FUN_00152120` writes to the display-mode
// global on exit.
const char *AspectStringId(int index);
int AspectMode(int index);
int AspectCount();

// The two colours both lists use, as 0xAABBGGRR-agnostic components.
struct MenuTint { float r, g, b, a; };
MenuTint SelectedTint();     // 0xFFFFFFFF
MenuTint UnselectedTint();   // 0xC8505050

// ── what the front end needs from outside ────────────────────────────────────
//
// Each of these is one function in the executable. Keeping them abstract is
// what keeps this file free of GL and SDL: the port supplies a subclass, and
// the state machine above stays a transcription rather than a rewrite.
class FrontEndHost {
public:
    virtual ~FrontEndHost() = default;

    // FUN_00180DB8 -> FUN_00180798. Resolves "<movies>/<L>/<name><N|W>.pss"
    // -- first letter of the name is the subdirectory, and the N/W suffix is
    // picked by the display-mode global this same front end sets. Returns
    // whether a clip actually started, and that return matters: `FUN_00151E00`
    // passes it straight back as "did this state start", so a background movie
    // that will not open makes the game skip the language screen. A port with
    // no decoder should return true if it will put *something* on screen for
    // that state, or it will silently lose the screen.
    virtual bool PlayMovie(const char *name, bool loop) = 0;
    // FUN_00180EF8.
    virtual void StopMovie() = 0;

    // FUN_001F3600 / FUN_001F3620: the current language id. SetLanguage is
    // called on every cursor move, not on accept -- that is what makes the
    // heading and the font follow the flag under the cursor.
    virtual void SetLanguage(int id) = 0;
    virtual int CurrentLanguage() const = 0;
    // FUN_001F31D8: build the string table from Strings.<code> for the
    // language that was just accepted.
    virtual void LoadStrings() = 0;

    // FUN_00152120: write the aspect entry's mode word to the display-mode
    // global (base+0x198, the same one the movie path's N/W suffix reads).
    virtual void SetDisplayMode(int mode) = 0;
    // FUN_001F3660: load LocaleUI once, after the aspect is known.
    virtual void LoadLocaleUI() = 0;

    // FUN_001D3AD0: queue MEMCARD_MSG_CHECK and the PlayerData reads.
    virtual void MemoryCardCheck() = 0;
    // FUN_001D2D08: whether that task is still running.
    virtual bool MemoryCardBusy() const = 0;

    // FUN_001FF3F8: the terminal record. The front end is finished; whatever
    // owns it should move on to the main menu.
    virtual void Leave() = 0;
};

// ── the XML menus, after the front end ───────────────────────────────────────

// What the player is doing this frame, for MenuState. The front end proper
// takes the raw button mask instead; this is the XML-driven menu, which is a
// separate machine.
struct MenuInput {
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
    bool accept = false;
    bool cancel = false;
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

// ── the front end ────────────────────────────────────────────────────────────

// The object at FUN_00152278: three ints and a table index. Everything it does
// per frame is FUN_001515d0.
class FrontEnd {
public:
    // `host` must outlive this object. Start() runs FUN_00151C00 -- enter the
    // first record -- and must be called before the first Update.
    void Start(FrontEndHost *host);

    // One frame of FUN_001515d0.
    //
    // `buttons` is the mask from MsgGetPadButtonMask, held state rather than
    // edges: the rising edge is taken here, against a previous mask that the
    // game initialises to 0xFFFFFFFF (DAT_00338AB8), so nothing held down at
    // boot registers on the first frame.
    //
    // `inputBlocked` is FUN_001AE558() != 0 -- a fade or a load in progress
    // discards the edge entirely rather than queueing it.
    void Update(uint32_t buttons, bool inputBlocked = false);

    // The "Ended" message from the movie player, FUN_00151810's first branch.
    // Marks the current record finished whatever it is.
    void NotifyMovieEnded();

    int StateIndex() const { return m_index; }
    const BootRecord &State() const;
    BootKind Kind() const { return State().kind; }
    // True once the table has reached the terminal record.
    bool Finished() const;

    // The cursor, obj+0xC: the flag under the pointer on the language screen,
    // the row on the aspect screen. Shared between the two states, as in the
    // original -- each entry function resets it.
    int Cursor() const { return m_cursor; }
    Language SelectedLanguage() const;
    bool Widescreen() const;

    MenuState &Menu() { return m_menu; }

private:
    bool Enter();    // FUN_00151E00 / F48 / 21F8 / 2198 / FF3F8
    bool Tick(uint32_t pressed);   // FUN_00151E98 / F58 / 2218 / 21C8
    void Exit();     // FUN_00151F28 / 2120 / 2258 / 21F0

    FrontEndHost *m_host = nullptr;
    int m_index = 0;                    // obj+0x8
    int m_cursor = 0;                   // obj+0xC
    int m_done = 0;                     // obj+0x10
    uint32_t m_prevButtons = 0xFFFFFFFFu;   // DAT_00338AB8
    // Not in the original object: there the answers live in globals the host
    // owns (the language id at 0x0033F044, the display mode at base+0x198).
    // Kept here so the port can draw without reaching back into the host.
    int m_language = 0;
    int m_displayMode = 0;
    MenuState m_menu;
};

} // namespace Game
} // namespace ClimaxEngine
