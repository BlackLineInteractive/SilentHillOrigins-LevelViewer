#include "ClimaxEngine/Game/FrontEnd.h"

namespace ClimaxEngine {
namespace Game {

// ── the tables ───────────────────────────────────────────────────────────────

// 0x00338A00. Five records; the fifth is terminal.
static const BootRecord kBoot[] = {
    {BootKind::Language,   nullptr, 0},
    {BootKind::MemoryCard, nullptr, 0},
    {BootKind::Aspect,     nullptr, 0},
    {BootKind::Movie,      "Logo",  0},
    {BootKind::Leave,      nullptr, 0},
};

const BootRecord *BootTable() { return kBoot; }
int BootTableCount() { return (int)(sizeof(kBoot) / sizeof(kBoot[0])); }

const char *BootKindName(BootKind k) {
    switch (k) {
    case BootKind::Language:   return "Language";
    case BootKind::Aspect:     return "Aspect";
    case BootKind::Movie:      return "Movie";
    case BootKind::MemoryCard: return "MemoryCard";
    case BootKind::Leave:      return "Leave";
    }
    return "?";
}

// 0x00338A58, stride 12: {id, name, texture handle filled in at load}.
const char *LanguageFlag(Language l) {
    switch (l) {
    case Language::English: return "sho_flg_GB";
    case Language::French:  return "sho_flg_FR";
    case Language::Italian: return "sho_flg_IT";
    case Language::German:  return "sho_flg_DE";
    case Language::Spanish: return "sho_flg_ES";
    }
    return "sho_flg_GB";
}

// Descriptor word 1 at 0x0033EFB0 + id*20, through the format "Strings.%s".
const char *LanguageStrings(Language l) {
    switch (l) {
    case Language::English: return "Strings.Eng";
    case Language::French:  return "Strings.Fre";
    case Language::Italian: return "Strings.Ita";
    case Language::German:  return "Strings.Ger";
    case Language::Spanish: return "Strings.Spa";
    }
    return "Strings.Eng";
}

// Descriptor word 0, a UTF-16 string in the executable, transcribed to UTF-8.
// This is the heading `FUN_00151918` draws, which is why it is written in each
// language rather than in English.
const char *LanguageOwnName(Language l) {
    switch (l) {
    case Language::English: return "English";
    case Language::French:  return "Fran\xC3\xA7" "ais";
    case Language::Italian: return "Italiano";
    case Language::German:  return "Deutsch";
    case Language::Spanish: return "Espa\xC3\xB1ol";
    }
    return "English";
}

// Descriptor word 2. All five flag-row languages share FontEUR; only the
// Japanese and Korean descriptors name FontJAP.
const char *LanguageFont(Language) { return "FontEUR"; }

int LanguageCount() { return 5; }

FlagRowLayout LanguageRow() { return FlagRowLayout{}; }

TextRect HeadingRect() { return TextRect{0.0f, 128.0f, 512.0f, 32.0f, 16.0f}; }
TextRect AspectRowRect() { return TextRect{0.0f, 192.0f, 512.0f, 32.0f, 14.0f}; }
float AspectRowStep() { return 28.0f; }   // size + size, FUN_00151FB8

// 0x00338A98, stride 8.
const char *AspectStringId(int index) {
    return index == 0 ? "display_4x3" : "display_ws";
}
int AspectMode(int index) { return index == 0 ? 0 : 1; }
int AspectCount() { return 2; }

MenuTint SelectedTint() { return MenuTint{1.0f, 1.0f, 1.0f, 1.0f}; }
MenuTint UnselectedTint() {
    // 0xC8505050
    const float k = 0x50 / 255.0f;
    return MenuTint{k, k, k, 0xC8 / 255.0f};
}

// ── MenuState ────────────────────────────────────────────────────────────────

void MenuState::SetScreens(
    std::vector<std::pair<std::string, const UI::Element *>> screens) {
    m_screens = std::move(screens);
    m_screenId.clear();
    m_screen = nullptr;
    m_activeId.clear();
    m_back.clear();
}

const UI::Element *MenuState::FindScreen(const std::string &id) const {
    for (const auto &[name, el] : m_screens)
        if (name == id)
            return el;
    // The XML names screens both by file ("mainmenu") and by the id on the
    // <SCREEN> element ("main_menu_screen"), and `onaccept` uses the second.
    for (const auto &[name, el] : m_screens)
        if (el && el->Attr("id") == id)
            return el;
    return nullptr;
}

bool MenuState::Open(const std::string &screenId) {
    const UI::Element *s = FindScreen(screenId);
    if (!s)
        return false;
    m_screenId = screenId;
    m_screen = s;
    SelectDefault();
    return true;
}

void MenuState::SelectDefault() {
    m_activeId.clear();
    if (!m_screen)
        return;
    // `default_active="true"` marks the button the screen opens on; failing
    // that, the first child that can take focus.
    for (const UI::Element &c : m_screen->children)
        if (c.Bool("default_active")) {
            m_activeId = c.Attr("id");
            return;
        }
    for (const UI::Element &c : m_screen->children)
        if (c.tag == "BUTTON" || c.tag == "TOGGLEBUTTON" || c.tag == "SLIDERBUTTON") {
            m_activeId = c.Attr("id");
            return;
        }
}

const UI::Element *MenuState::Active() const {
    if (!m_screen || m_activeId.empty())
        return nullptr;
    for (const UI::Element &c : m_screen->children)
        if (c.Attr("id") == m_activeId)
            return &c;
    return nullptr;
}

bool MenuState::Toggle(const std::string &type) const {
    auto it = m_toggles.find(type);
    // The defaults are the strings the screen ships with: newgame.xml opens
    // with `ui_newgame_no` on subtitles and `ui_options_on` on vibration.
    if (it != m_toggles.end())
        return it->second;
    return type == "vibration";
}

void MenuState::SetToggle(const std::string &type, bool on) { m_toggles[type] = on; }

const std::string *MenuState::ToggleTypeForTextbox(const std::string &textboxId) const {
    if (!m_screen || textboxId.empty())
        return nullptr;
    for (const UI::Element &c : m_screen->children)
        if (c.tag == "TOGGLEBUTTON" && c.Attr("toggletextbox") == textboxId) {
            const std::string *t = c.Get("toggletype");
            if (t && !t->empty())
                return t;
        }
    return nullptr;
}

std::string MenuState::Update(const MenuInput &in) {
    const UI::Element *cur = Active();
    if (!cur)
        return {};

    // Movement: the named element has to exist on this screen, or the press
    // does nothing. A missing target is a dead end in the data, not a screen
    // change -- treating it as one would open the main menu from a stray typo.
    auto move = [&](const char *attr) {
        const std::string *to = cur->Get(attr);
        if (!to || to->empty())
            return;
        for (const UI::Element &c : m_screen->children)
            if (c.Attr("id") == *to) {
                m_activeId = *to;
                return;
            }
    };

    // A TOGGLEBUTTON takes left and right for itself: they change the setting
    // rather than moving the cursor, and `ignorecross="true"` says cross must
    // not flip it -- accept is a move, which is why `onaccept` on the subtitles
    // row names the vibration row.
    if (cur->tag == "TOGGLEBUTTON") {
        const std::string type = cur->Attr("toggletype");
        if (!type.empty() && (in.left || in.right)) {
            SetToggle(type, in.right ? true : false);
            return {};
        }
    }

    if (in.up) move("onup");
    else if (in.down) move("ondown");
    else if (in.left) move("onleft");
    else if (in.right) move("onright");

    if (in.cancel) {
        const std::string *to = cur->Get("oncancel");
        if (to && !to->empty() && Open(*to))
            return {};
        if (!m_back.empty()) {
            const std::string prev = m_back.back();
            m_back.pop_back();
            m_screenId = prev;
            m_screen = FindScreen(prev);
            SelectDefault();
        }
        return {};
    }

    if (in.accept) {
        cur = Active();   // movement above may have changed it
        const std::string *to = cur ? cur->Get("onaccept") : nullptr;
        if (!to || to->empty())
            return {};
        // A sibling first, then a screen, then it is a command for the caller.
        for (const UI::Element &c : m_screen->children)
            if (c.Attr("id") == *to) {
                m_activeId = *to;
                return {};
            }
        if (FindScreen(*to)) {
            m_back.push_back(m_screenId);
            Open(*to);
            return {};
        }
        return *to;
    }
    return {};
}

// ── FrontEnd ─────────────────────────────────────────────────────────────────

const BootRecord &FrontEnd::State() const {
    const int n = BootTableCount();
    const int i = m_index < 0 ? 0 : (m_index >= n ? n - 1 : m_index);
    return kBoot[i];
}

bool FrontEnd::Finished() const { return State().kind == BootKind::Leave; }

Language FrontEnd::SelectedLanguage() const {
    const int n = LanguageCount();
    const int raw = State().kind == BootKind::Language ? m_cursor : m_language;
    const int i = raw < 0 ? 0 : (raw >= n ? n - 1 : raw);
    return (Language)i;
}

// The cursor is shared and each entry function resets it, so once the aspect
// state has been left the answer only survives in what was pushed to the host.
// These two keep a copy for the port's own drawing.
bool FrontEnd::Widescreen() const {
    return State().kind == BootKind::Aspect ? AspectMode(m_cursor) != 0
                                            : m_displayMode != 0;
}

// FUN_00152278: the object is allocated with stateIndex, cursor and done all
// zero, and FUN_00151C00 immediately enters record 0.
void FrontEnd::Start(FrontEndHost *host) {
    m_host = host;
    m_index = 0;
    m_cursor = 0;
    m_done = 0;
    m_prevButtons = 0xFFFFFFFFu;
    if (State().kind == BootKind::Leave) {
        if (m_host) m_host->Leave();
        return;
    }
    m_done = Enter() ? 0 : 1;
}

// The four entry functions, FUN_00151E00 / F48 / 21F8 / 2198. The return value
// is "did this state start"; false makes FUN_001515D0 mark it finished at once,
// which is how a state gets skipped.
bool FrontEnd::Enter() {
    if (!m_host)
        return false;
    switch (State().kind) {
    case BootKind::Language: {
        // FUN_00151E00. The background is a looping movie, and *its* success is
        // what the entry function returns.
        const bool started = m_host->PlayMovie("Back", true);
        if (m_host->CurrentLanguage() == kLanguageJapanese)
            return false;   // a build that presets the language never asks
        m_cursor = 0;
        return started;
    }
    case BootKind::Aspect:
        // FUN_00151F48: cursor to the first row, and it never skips -- the
        // aspect question is asked on every boot, there is no "remembered"
        // path here and the previous version of this file inventing one is
        // what made the screen disappear.
        m_cursor = 0;
        return true;
    case BootKind::Movie:
        // FUN_001521F8 tail-calls the movie player, so the player's own
        // "did it start" is the entry result.
        return m_host->PlayMovie(State().arg ? State().arg : "", false);
    case BootKind::MemoryCard:
        // FUN_00152198.
        m_host->MemoryCardCheck();
        return true;
    case BootKind::Leave:
        m_host->Leave();
        return true;
    }
    return true;
}

// The four per-frame functions, FUN_00151E98 / F58 / 2218 / 21C8. `pressed` is
// already the rising edge. Returns whether the state is finished.
bool FrontEnd::Tick(uint32_t pressed) {
    switch (State().kind) {
    case BootKind::Language: {
        // FUN_00151E98. Left and right only -- the row is horizontal, and up
        // and down do nothing at all in the original.
        const int before = m_cursor;
        if (pressed & Pad::Left)  --m_cursor;
        if (pressed & Pad::Right) ++m_cursor;
        const int n = LanguageCount();
        m_cursor = (m_cursor + n) % n;
        // The language changes on the *move*, not on accept: this is what
        // repaints the heading and swaps the font as the cursor travels.
        if (before != m_cursor && m_host)
            m_host->SetLanguage(m_cursor);
        return (pressed & Pad::Cross) != 0;
    }
    case BootKind::Aspect: {
        // FUN_00151F58. Up and down only -- the list is vertical.
        if (pressed & Pad::Up)   --m_cursor;
        if (pressed & Pad::Down) ++m_cursor;
        const int n = AspectCount();
        m_cursor = (m_cursor + n) % n;
        return (pressed & Pad::Cross) != 0;
    }
    case BootKind::Movie:
        // FUN_00152218. Cross or start end the clip early, but only when the
        // record says so, and the Logo record does not -- the logo is watched
        // to the end and finishes on the player's "Ended" message.
        return State().skippable != 0 &&
               (pressed & (Pad::Cross | Pad::Start)) != 0;
    case BootKind::MemoryCard:
        // FUN_001521C8: done when the card task stops reporting busy.
        return m_host && !m_host->MemoryCardBusy();
    case BootKind::Leave:
        return false;
    }
    return false;
}

// The four exit functions, FUN_00151F28 / 2120 / 2258 / 21F0.
void FrontEnd::Exit() {
    if (!m_host)
        return;
    switch (State().kind) {
    case BootKind::Language:
        // FUN_00151F28: build the string table for the accepted language.
        m_language = m_cursor;
        m_host->LoadStrings();
        break;
    case BootKind::Aspect:
        // FUN_00152120, in this order: the display mode first, because the
        // movie path's N/W suffix reads it, then the background movie down,
        // then LocaleUI.
        m_displayMode = AspectMode(m_cursor);
        m_host->SetDisplayMode(m_displayMode);
        m_host->StopMovie();
        m_host->LoadLocaleUI();
        break;
    case BootKind::Movie:
        // FUN_00152258.
        m_host->StopMovie();
        break;
    case BootKind::MemoryCard:
    case BootKind::Leave:
        break;   // FUN_001521F0 is a stub
    }
}

// FUN_001515D0.
void FrontEnd::Update(uint32_t buttons, bool inputBlocked) {
    uint32_t pressed = buttons & ~m_prevButtons;
    m_prevButtons = buttons;
    if (inputBlocked)
        pressed = 0;   // a fade or a load discards the edge, it does not queue it

    if (m_done == 0) {
        m_done = Tick(pressed) ? 1 : 0;
        if (m_done == 0)
            return;
    }

    Exit();
    m_done = 0;
    if (m_index + 1 < BootTableCount())
        ++m_index;

    if (State().kind == BootKind::Leave) {
        // The terminal record calls FUN_001FF3F8 and, notably, does not write
        // the done flag -- the object is on its way out.
        if (m_host)
            m_host->Leave();
        return;
    }
    m_done = Enter() ? 0 : 1;
}

// FUN_00151810's first branch: the "Ended" message with no payload sets the
// done flag, whatever state is current.
void FrontEnd::NotifyMovieEnded() { m_done = 1; }

} // namespace Game
} // namespace ClimaxEngine
