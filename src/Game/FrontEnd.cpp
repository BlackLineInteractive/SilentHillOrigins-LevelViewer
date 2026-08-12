#include "ClimaxEngine/Game/FrontEnd.h"

namespace ClimaxEngine {
namespace Game {

// The flag row, in the order `FUN_00151918` walks the table at 0x00338A5C.
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

int LanguageCount() { return 5; }

FlagRowLayout LanguageRow() { return FlagRowLayout{}; }

const char *BootStageName(BootStage s) {
    switch (s) {
    case BootStage::Loading: return "Loading";
    case BootStage::LanguageSelect: return "LanguageSelect";
    case BootStage::AspectSelect: return "AspectSelect";
    case BootStage::Logo: return "Logo";
    case BootStage::MainMenu: return "MainMenu";
    case BootStage::InGame: return "InGame";
    }
    return "?";
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

void FrontEnd::Reset() {
    m_stage = BootStage::Loading;
    m_elapsed = 0.0f;
}

void FrontEnd::Enter(BootStage s) {
    m_stage = s;
    m_elapsed = 0.0f;
    if (s == BootStage::MainMenu)
        m_menu.Open("mainmenu");
}

// Where the next stage comes from: the game keeps its front end as a table of
// 12-byte state records at 0x00338A00 and dispatches on the record's first
// word, `FUN_00151d60`. Nothing in that table encodes the *order* -- the state
// index is written from outside -- so the sequence below is the one the retail
// game runs on screen: copyright plate, language, aspect, logo, menu.
//
// Both language and aspect are skippable once answered; the game asks on first
// boot and remembers, which is why a returning player sees only the logo.
BootStage FrontEnd::NextStage(BootStage from) const {
    switch (from) {
    case BootStage::Loading:
        if (!languageChosen) return BootStage::LanguageSelect;
        // fallthrough
    case BootStage::LanguageSelect:
        if (!aspectChosen) return BootStage::AspectSelect;
        // fallthrough
    case BootStage::AspectSelect:
        return BootStage::Logo;
    case BootStage::Logo:
        return BootStage::MainMenu;
    default:
        return from;
    }
}

std::string FrontEnd::Update(float dt, const MenuInput &in) {
    m_elapsed += dt;

    switch (m_stage) {
    case BootStage::Loading:
        // Held for however long the first load takes; here that is a timer,
        // skippable the way the original is.
        if (m_elapsed >= loadingSeconds || in.anyKey || in.accept)
            Enter(NextStage(m_stage));
        return {};

    case BootStage::LanguageSelect: {
        // `FUN_00151918` draws the five flags as one horizontal row, so the
        // cursor moves on the horizontal axis. Up/down are accepted too --
        // harmless, and it saves a player hunting for the right key.
        const int n = LanguageCount();
        if (in.left  || in.up)   languageIndex = (languageIndex + n - 1) % n;
        if (in.right || in.down) languageIndex = (languageIndex + 1) % n;
        language = (Language)languageIndex;
        if (in.accept) {
            languageChosen = true;
            Enter(NextStage(m_stage));
        }
        return {};
    }

    case BootStage::AspectSelect:
        // `FUN_00151fb8` steps its two entries in y, one under the other, and
        // they are strings -- `display_4x3` and `display_ws` are string-table
        // ids, not textures. So this list is vertical where the flags are not.
        if (in.up)   widescreen = false;   // 4:3 is drawn first
        if (in.down) widescreen = true;
        if (in.left) widescreen = false;
        if (in.right) widescreen = true;
        if (in.accept) {
            aspectChosen = true;
            Enter(NextStage(m_stage));
        }
        return {};

    case BootStage::Logo:
        // Advances when the LOGOW/LOGON clip actually ends, or -- once it has
        // had a moment to be seen -- on a keypress, the same as the original.
        // `logoSeconds` only fires if no video is playing at all, so the boot
        // sequence can never hang on a missing asset.
        if (in.mediaEnded || m_elapsed >= logoSeconds ||
            ((in.anyKey || in.accept) && m_elapsed > 1.0f))
            Enter(NextStage(m_stage));
        return {};

    case BootStage::MainMenu:
        return m_menu.Update(in);

    case BootStage::InGame:
        return {};
    }
    return {};
}

} // namespace Game
} // namespace ClimaxEngine
