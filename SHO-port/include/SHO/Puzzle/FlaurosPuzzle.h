#pragma once

#include "SHO/Puzzle/PuzzleBase.h"
#include <array>

namespace SHO {
namespace Puzzle {

struct FlaurosPiece {
    int currentRotX = 0; // 0, 1, 2 (120 deg increments)
    int currentRotY = 0;
    int targetRotX  = 0;
    int targetRotY  = 0;

    bool IsAligned() const {
        return (currentRotX % 3 == targetRotX % 3) &&
               (currentRotY % 3 == targetRotY % 3);
    }
};

class FlaurosPuzzle : public PuzzleBase {
public:
    FlaurosPuzzle();

    void OnInit() override;
    bool HandleInput(Core::PadButton button) override;

    void SelectPiece(int idx);
    void RotateSelectedX();
    void RotateSelectedY();

    bool CheckSolution() const;

    int GetSelectedPiece() const { return m_selectedPiece; }
    const std::array<FlaurosPiece, 4>& GetPieces() const { return m_pieces; }

private:
    std::array<FlaurosPiece, 4> m_pieces;
    int m_selectedPiece = 0;
};

} // namespace Puzzle
} // namespace SHO
