#include "SHO/Puzzle/IronLungPuzzle.h"
#include <algorithm>

namespace SHO {
namespace Puzzle {

IronLungPuzzle::IronLungPuzzle()
    : PuzzleBase("IronLung", "ironlungpuzzle.xml") {
    m_solveEvent = "Event_IronLungKeyEjected";
    m_rewardItemId = "Key_IronLung";
    m_backdropTex = "sho_il_bd_**.jpg";
}

void IronLungPuzzle::OnInit() {
    m_status = PuzzleStatus::InProgress;
    m_pressure = 0;
    m_selectedValve = 0;
    m_targetPressure = 25;
}

void IronLungPuzzle::TurnValve(int valveIdx, bool clockwise) {
    if (m_status == PuzzleStatus::Solved) return;

    int delta = 0;
    if (valveIdx == 0) delta = clockwise ? 5 : -3;
    else if (valveIdx == 1) delta = clockwise ? 7 : -4;
    else if (valveIdx == 2) delta = clockwise ? 10 : -6;

    m_pressure = std::clamp(m_pressure + delta, 0, 50);

    if (CheckSolution()) {
        OnSolve();
    }
}

bool IronLungPuzzle::CheckSolution() const {
    return (m_pressure == m_targetPressure);
}

bool IronLungPuzzle::HandleInput(Core::PadButton button) {
    if (m_status == PuzzleStatus::Solved) return false;

    if (button == Core::PadButton::DpadLeft) {
        m_selectedValve = (m_selectedValve + 2) % 3;
        return true;
    } else if (button == Core::PadButton::DpadRight) {
        m_selectedValve = (m_selectedValve + 1) % 3;
        return true;
    } else if (button == Core::PadButton::DpadUp || button == Core::PadButton::Cross) {
        TurnValve(m_selectedValve, true);
        return true;
    } else if (button == Core::PadButton::DpadDown || button == Core::PadButton::Square) {
        TurnValve(m_selectedValve, false);
        return true;
    } else if (button == Core::PadButton::Circle) {
        OnClose();
        return true;
    }
    return false;
}

} // namespace Puzzle
} // namespace SHO
