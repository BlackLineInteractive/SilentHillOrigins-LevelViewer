#include "SHO/Puzzle/CalendarPuzzle.h"

namespace SHO {
namespace Puzzle {

CalendarPuzzle::CalendarPuzzle()
    : PuzzleBase("Calendar", "calenderpuzzle.xml") {
    m_solveEvent = "Event_CalendarLockboxOpened";
    m_rewardItemId = "Key_PatientDiary";
    m_backdropTex = "sho_cal_bd_**.jpg";
}

void CalendarPuzzle::OnInit() {
    m_status = PuzzleStatus::InProgress;
    m_month = 1;
    m_day = 1;
    m_selectedCol = 0;
}

void CalendarPuzzle::AdjustSelected(int delta) {
    if (m_status == PuzzleStatus::Solved) return;

    if (m_selectedCol == 0) {
        m_month += delta;
        if (m_month < 1) m_month = 12;
        if (m_month > 12) m_month = 1;
    } else {
        m_day += delta;
        if (m_day < 1) m_day = 31;
        if (m_day > 31) m_day = 1;
    }

    if (CheckSolution()) {
        OnSolve();
    }
}

bool CalendarPuzzle::CheckSolution() const {
    return (m_month == m_targetMonth && m_day == m_targetDay);
}

bool CalendarPuzzle::HandleInput(Core::PadButton button) {
    if (m_status == PuzzleStatus::Solved) return false;

    if (button == Core::PadButton::DpadLeft || button == Core::PadButton::DpadRight) {
        m_selectedCol = 1 - m_selectedCol;
        return true;
    } else if (button == Core::PadButton::DpadUp) {
        AdjustSelected(+1);
        return true;
    } else if (button == Core::PadButton::DpadDown) {
        AdjustSelected(-1);
        return true;
    } else if (button == Core::PadButton::Circle) {
        OnClose();
        return true;
    }
    return false;
}

} // namespace Puzzle
} // namespace SHO
