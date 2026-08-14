#include "SHO/Puzzle/OrganBoxPuzzle.h"

namespace SHO {
namespace Puzzle {

OrganBoxPuzzle::OrganBoxPuzzle()
    : PuzzleBase("MusicOrgan", "organboxpuzzle.xml") {
    m_solveEvent = "Event_OrganBoxOpened";
    m_rewardItemId = "Key_MusicRoom";
    m_backdropTex = "sho_org_bd_**.jpg";
}

void OrganBoxPuzzle::OnInit() {
    m_status = PuzzleStatus::InProgress;
    m_enteredNotes.clear();
    m_selectedKey = 0;
}

void OrganBoxPuzzle::PressKey(int keyIdx) {
    if (m_status == PuzzleStatus::Solved) return;

    m_enteredNotes.push_back(keyIdx);

    // Check if entered prefix matches target
    for (size_t i = 0; i < m_enteredNotes.size(); ++i) {
        if (i >= m_targetMelody.size() || m_enteredNotes[i] != m_targetMelody[i]) {
            // Wrong note, reset
            m_enteredNotes.clear();
            return;
        }
    }

    if (m_enteredNotes.size() == m_targetMelody.size()) {
        OnSolve();
    }
}

void OrganBoxPuzzle::ResetSequence() {
    m_enteredNotes.clear();
}

bool OrganBoxPuzzle::CheckSolution() const {
    return (m_enteredNotes == m_targetMelody);
}

bool OrganBoxPuzzle::HandleInput(Core::PadButton button) {
    if (m_status == PuzzleStatus::Solved) return false;

    if (button == Core::PadButton::DpadLeft) {
        m_selectedKey = (m_selectedKey + 4) % 5;
        return true;
    } else if (button == Core::PadButton::DpadRight) {
        m_selectedKey = (m_selectedKey + 1) % 5;
        return true;
    } else if (button == Core::PadButton::Cross) {
        PressKey(m_selectedKey);
        return true;
    } else if (button == Core::PadButton::Triangle) {
        ResetSequence();
        return true;
    } else if (button == Core::PadButton::Circle) {
        OnClose();
        return true;
    }
    return false;
}

} // namespace Puzzle
} // namespace SHO
