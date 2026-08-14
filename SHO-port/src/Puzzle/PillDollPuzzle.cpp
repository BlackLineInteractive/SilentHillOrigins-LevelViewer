#include "SHO/Puzzle/PillDollPuzzle.h"

namespace SHO {
namespace Puzzle {

PillDollPuzzle::PillDollPuzzle()
    : PuzzleBase("PillDoll", "pilldollpuzzle.xml") {
    m_solveEvent = "Event_PillDollKeyReleased";
    m_rewardItemId = "Key_FemaleHydrotherapy";
    m_backdropTex = "sho_pd_bd_**.jpg";
}

void PillDollPuzzle::OnInit() {
    m_status = PuzzleStatus::InProgress;
    m_mouthPills.fill(PillColor::None);
    m_selectedMouth = 0;
}

void PillDollPuzzle::SelectMouth(int idx) {
    if (idx >= 0 && idx < 5) {
        m_selectedMouth = idx;
    }
}

void PillDollPuzzle::PlacePill(PillColor color) {
    if (m_status == PuzzleStatus::Solved) return;

    m_mouthPills[m_selectedMouth] = color;
    if (CheckSolution()) {
        OnSolve();
    }
}

void PillDollPuzzle::RemovePill() {
    if (m_status == PuzzleStatus::Solved) return;
    m_mouthPills[m_selectedMouth] = PillColor::None;
}

bool PillDollPuzzle::CheckSolution() const {
    // Clue mapping from medical notes:
    // Mouth 0 (Psycho) -> Green
    // Mouth 1 (Pyro) -> Blue
    // Mouth 2 (Self-Harm) -> Red
    // Mouth 3 (Transgender) -> Yellow
    // Mouth 4 (Anorexic) -> Blue
    return (m_mouthPills[0] == PillColor::Green &&
            m_mouthPills[1] == PillColor::Blue &&
            m_mouthPills[2] == PillColor::Red &&
            m_mouthPills[3] == PillColor::Yellow &&
            m_mouthPills[4] == PillColor::Blue);
}

bool PillDollPuzzle::HandleInput(Core::PadButton button) {
    if (m_status == PuzzleStatus::Solved) return false;

    if (button == Core::PadButton::DpadLeft) {
        m_selectedMouth = (m_selectedMouth + 4) % 5;
        return true;
    } else if (button == Core::PadButton::DpadRight) {
        m_selectedMouth = (m_selectedMouth + 1) % 5;
        return true;
    } else if (button == Core::PadButton::Square) {
        PlacePill(PillColor::Green);
        return true;
    } else if (button == Core::PadButton::Cross) {
        PlacePill(PillColor::Blue);
        return true;
    } else if (button == Core::PadButton::Circle) {
        PlacePill(PillColor::Red);
        return true;
    } else if (button == Core::PadButton::Triangle) {
        PlacePill(PillColor::Yellow);
        return true;
    } else if (button == Core::PadButton::Select) {
        RemovePill();
        return true;
    }
    return false;
}

} // namespace Puzzle
} // namespace SHO
