#include "SHO/Puzzle/FlaurosPuzzle.h"

namespace SHO {
namespace Puzzle {

FlaurosPuzzle::FlaurosPuzzle()
    : PuzzleBase("Flauros", "flaurouspuzzle.xml") {
    m_solveEvent = "Event_FlaurosCompleted";
    m_rewardItemId = "Item_FlaurosArtifact";
    m_backdropTex = "sho_fla_sq_pyr.png";
}

void FlaurosPuzzle::OnInit() {
    m_status = PuzzleStatus::InProgress;
    m_selectedPiece = 0;

    // Initialize initial scrambled rotations and target alignment
    m_pieces[0] = { 1, 2, 0, 0 };
    m_pieces[1] = { 2, 1, 0, 0 };
    m_pieces[2] = { 0, 1, 0, 0 };
    m_pieces[3] = { 1, 0, 0, 0 };
}

void FlaurosPuzzle::SelectPiece(int idx) {
    if (idx >= 0 && idx < 4) {
        m_selectedPiece = idx;
    }
}

void FlaurosPuzzle::RotateSelectedX() {
    if (m_status == PuzzleStatus::Solved) return;
    m_pieces[m_selectedPiece].currentRotX = (m_pieces[m_selectedPiece].currentRotX + 1) % 3;
    if (CheckSolution()) {
        OnSolve();
    }
}

void FlaurosPuzzle::RotateSelectedY() {
    if (m_status == PuzzleStatus::Solved) return;
    m_pieces[m_selectedPiece].currentRotY = (m_pieces[m_selectedPiece].currentRotY + 1) % 3;
    if (CheckSolution()) {
        OnSolve();
    }
}

bool FlaurosPuzzle::CheckSolution() const {
    for (const auto& p : m_pieces) {
        if (!p.IsAligned()) return false;
    }
    return true;
}

bool FlaurosPuzzle::HandleInput(Core::PadButton button) {
    if (m_status == PuzzleStatus::Solved) return false;

    if (button == Core::PadButton::L1) {
        m_selectedPiece = (m_selectedPiece + 3) % 4;
        return true;
    } else if (button == Core::PadButton::R1) {
        m_selectedPiece = (m_selectedPiece + 1) % 4;
        return true;
    } else if (button == Core::PadButton::Square || button == Core::PadButton::DpadLeft) {
        RotateSelectedX();
        return true;
    } else if (button == Core::PadButton::Triangle || button == Core::PadButton::DpadUp) {
        RotateSelectedY();
        return true;
    } else if (button == Core::PadButton::Circle) {
        OnClose();
        return true;
    }
    return false;
}

} // namespace Puzzle
} // namespace SHO
