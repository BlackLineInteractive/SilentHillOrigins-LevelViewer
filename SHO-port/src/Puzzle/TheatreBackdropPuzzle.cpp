#include "SHO/Puzzle/TheatreBackdropPuzzle.h"

namespace SHO {
namespace Puzzle {

TheatreBackdropPuzzle::TheatreBackdropPuzzle()
    : PuzzleBase("TheatreBackdrop", "bkdropproppuzzle.xml") {
    m_solveEvent = "Event_TheatreMirrorOpened";
    m_rewardItemId = "Key_StageProps";
    m_backdropTex = "sho_bd_theatre.png";
}

void TheatreBackdropPuzzle::OnInit() {
    m_status = PuzzleStatus::InProgress;
    m_backdrop = StageBackdrop::None;
    m_prop = StageProp::None;
    m_light = StageLight::None;
}

void TheatreBackdropPuzzle::SetBackdrop(StageBackdrop bd) {
    if (m_status == PuzzleStatus::Solved) return;
    m_backdrop = bd;
    if (CheckSolution()) OnSolve();
}

void TheatreBackdropPuzzle::SetProp(StageProp prop) {
    if (m_status == PuzzleStatus::Solved) return;
    m_prop = prop;
    if (CheckSolution()) OnSolve();
}

void TheatreBackdropPuzzle::SetLight(StageLight light) {
    if (m_status == PuzzleStatus::Solved) return;
    m_light = light;
    if (CheckSolution()) OnSolve();
}

bool TheatreBackdropPuzzle::CheckSolution() const {
    // In Artaud Theatre: Forest Backdrop (1) + Tree Prop (1) + Moonlight (2)
    return (m_backdrop == StageBackdrop::Forest &&
            m_prop == StageProp::Tree &&
            m_light == StageLight::Moonlight);
}

bool TheatreBackdropPuzzle::HandleInput(Core::PadButton button) {
    if (m_status == PuzzleStatus::Solved) return false;

    if (button == Core::PadButton::Square) {
        int nextBd = (static_cast<int>(m_backdrop) + 1) % 4;
        SetBackdrop(static_cast<StageBackdrop>(nextBd));
        return true;
    } else if (button == Core::PadButton::Triangle) {
        int nextProp = (static_cast<int>(m_prop) + 1) % 4;
        SetProp(static_cast<StageProp>(nextProp));
        return true;
    } else if (button == Core::PadButton::Cross) {
        int nextLight = (static_cast<int>(m_light) + 1) % 4;
        SetLight(static_cast<StageLight>(nextLight));
        return true;
    } else if (button == Core::PadButton::Circle) {
        OnClose();
        return true;
    }
    return false;
}

} // namespace Puzzle
} // namespace SHO
