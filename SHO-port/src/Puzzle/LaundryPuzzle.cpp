#include "SHO/Puzzle/LaundryPuzzle.h"

namespace SHO {
namespace Puzzle {

LaundryPuzzle::LaundryPuzzle()
    : PuzzleBase("LaundryMachine", "laundrypuzzle.xml") {
    m_solveEvent = "Event_LaundryFinished";
    m_rewardItemId = "Key_Cleopatra";
    m_backdropTex = "sho_laund_bd_**.jpg";
}

void LaundryPuzzle::OnInit() {
    m_status = PuzzleStatus::InProgress;
    m_activeDial = 0;
    m_washCycle = 0;
    m_temperature = 0;
    m_spinSpeed = 0;
    m_rinseMode = 0;
}

void LaundryPuzzle::NextDial() {
    m_activeDial = (m_activeDial + 1) % 4;
}

void LaundryPuzzle::PrevDial() {
    m_activeDial = (m_activeDial + 3) % 4;
}

void LaundryPuzzle::RotateDial(int delta) {
    if (m_status == PuzzleStatus::Solved) return;

    if (m_activeDial == 0) {
        m_washCycle = (m_washCycle + delta + 4) % 4;
    } else if (m_activeDial == 1) {
        m_temperature = (m_temperature + delta + 3) % 3;
    } else if (m_activeDial == 2) {
        m_spinSpeed = (m_spinSpeed + delta + 3) % 3;
    } else if (m_activeDial == 3) {
        m_rinseMode = (m_rinseMode + delta + 2) % 2;
    }
}

void LaundryPuzzle::PressStart() {
    if (m_status == PuzzleStatus::Solved) return;

    if (CheckSolution()) {
        OnSolve();
    }
}

bool LaundryPuzzle::CheckSolution() const {
    // In Silent Hill Origins: Heavy Cycle (2), Hot Temp (2), Medium Spin (1), Drain (1)
    return (m_washCycle == 2 &&
            m_temperature == 2 &&
            m_spinSpeed == 1 &&
            m_rinseMode == 1);
}

bool LaundryPuzzle::HandleInput(Core::PadButton button) {
    if (m_status == PuzzleStatus::Solved) return false;

    if (button == Core::PadButton::DpadUp) {
        PrevDial();
        return true;
    } else if (button == Core::PadButton::DpadDown) {
        NextDial();
        return true;
    } else if (button == Core::PadButton::DpadLeft) {
        RotateDial(-1);
        return true;
    } else if (button == Core::PadButton::DpadRight) {
        RotateDial(+1);
        return true;
    } else if (button == Core::PadButton::Cross) {
        PressStart();
        return true;
    } else if (button == Core::PadButton::Circle) {
        OnClose();
        return true;
    }
    return false;
}

} // namespace Puzzle
} // namespace SHO
