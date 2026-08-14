#pragma once

#include "SHO/Puzzle/PuzzleBase.h"
#include <array>

namespace SHO {
namespace Puzzle {

enum class PillColor {
    None = 0,
    Green,
    Blue,
    Red,
    Yellow
};

class PillDollPuzzle : public PuzzleBase {
public:
    PillDollPuzzle();

    void OnInit() override;
    bool HandleInput(Core::PadButton button) override;

    void SelectMouth(int idx);
    void PlacePill(PillColor color);
    void RemovePill();

    bool CheckSolution() const;

    int GetSelectedMouth() const { return m_selectedMouth; }
    const std::array<PillColor, 5>& GetMouthPills() const { return m_mouthPills; }

private:
    std::array<PillColor, 5> m_mouthPills = { PillColor::None, PillColor::None, PillColor::None, PillColor::None, PillColor::None };
    int m_selectedMouth = 0;
};

} // namespace Puzzle
} // namespace SHO
