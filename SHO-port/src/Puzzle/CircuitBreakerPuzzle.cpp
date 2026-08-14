#include "SHO/Puzzle/CircuitBreakerPuzzle.h"

namespace SHO {
namespace Puzzle {

CircuitBreakerPuzzle::CircuitBreakerPuzzle()
    : PuzzleBase("CircuitBreaker", "circuitbrkpuzzle.xml") {
    m_solveEvent = "Event_PowerRestored";
    m_rewardItemId = "";
    m_backdropTex = "sho_cb_bd_**.jpg";
}

void CircuitBreakerPuzzle::OnInit() {
    m_status = PuzzleStatus::InProgress;
    m_selectedSwitch = 0;
    m_targetAmps = 65;

    // Fuse ratings: 15A, 20A, 25A, 30A, 40A
    m_switches[0] = { 15, false };
    m_switches[1] = { 20, false };
    m_switches[2] = { 25, false };
    m_switches[3] = { 30, false };
    m_switches[4] = { 40, false };
}

void CircuitBreakerPuzzle::ToggleSwitch(int idx) {
    if (m_status == PuzzleStatus::Solved) return;
    if (idx >= 0 && idx < 5) {
        m_switches[idx].isOn = !m_switches[idx].isOn;
        if (CheckSolution()) {
            OnSolve();
        }
    }
}

int CircuitBreakerPuzzle::GetCurrentTotalAmps() const {
    int total = 0;
    for (const auto& s : m_switches) {
        if (s.isOn) total += s.amperage;
    }
    return total;
}

bool CircuitBreakerPuzzle::CheckSolution() const {
    // Exactly 65 Amps (e.g. 15A + 20A + 30A = 65A, or 25A + 40A = 65A)
    return (GetCurrentTotalAmps() == m_targetAmps);
}

bool CircuitBreakerPuzzle::HandleInput(Core::PadButton button) {
    if (m_status == PuzzleStatus::Solved) return false;

    if (button == Core::PadButton::DpadLeft) {
        m_selectedSwitch = (m_selectedSwitch + 4) % 5;
        return true;
    } else if (button == Core::PadButton::DpadRight) {
        m_selectedSwitch = (m_selectedSwitch + 1) % 5;
        return true;
    } else if (button == Core::PadButton::Cross) {
        ToggleSwitch(m_selectedSwitch);
        return true;
    } else if (button == Core::PadButton::Circle) {
        OnClose();
        return true;
    }
    return false;
}

} // namespace Puzzle
} // namespace SHO
