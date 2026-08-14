#pragma once

#include "SHO/Puzzle/PuzzleBase.h"
#include <array>

namespace SHO {
namespace Puzzle {

class IronLungPuzzle : public PuzzleBase {
public:
    IronLungPuzzle();

    void OnInit() override;
    bool HandleInput(Core::PadButton button) override;

    void TurnValve(int valveIdx, bool clockwise);
    int GetCurrentPressure() const { return m_pressure; }
    int GetTargetPressure() const { return m_targetPressure; }
    int GetSelectedValve() const { return m_selectedValve; }

    bool CheckSolution() const;

private:
    int m_pressure = 0;
    int m_targetPressure = 25; // Target green zone PSI
    int m_selectedValve = 0;   // 0, 1, 2
};

} // namespace Puzzle
} // namespace SHO
