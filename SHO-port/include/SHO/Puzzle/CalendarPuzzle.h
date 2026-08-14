#pragma once

#include "SHO/Puzzle/PuzzleBase.h"

namespace SHO {
namespace Puzzle {

class CalendarPuzzle : public PuzzleBase {
public:
    CalendarPuzzle();

    void OnInit() override;
    bool HandleInput(Core::PadButton button) override;

    void AdjustSelected(int delta);
    bool CheckSolution() const;

    int GetMonth() const { return m_month; }
    int GetDay() const { return m_day; }
    int GetSelectedColumn() const { return m_selectedCol; }

private:
    int m_month = 1;       // 1 - 12
    int m_day   = 1;       // 1 - 31
    int m_targetMonth = 11; // November
    int m_targetDay   = 12; // 12th
    int m_selectedCol = 0;  // 0: Month, 1: Day
};

} // namespace Puzzle
} // namespace SHO
