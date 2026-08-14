#pragma once

#include "SHO/Puzzle/PuzzleBase.h"
#include <vector>

namespace SHO {
namespace Puzzle {

class OrganBoxPuzzle : public PuzzleBase {
public:
    OrganBoxPuzzle();

    void OnInit() override;
    bool HandleInput(Core::PadButton button) override;

    void PressKey(int keyIdx); // 0: C, 1: D, 2: E, 3: F, 4: G
    void ResetSequence();

    bool CheckSolution() const;
    const std::vector<int>& GetEnteredNotes() const { return m_enteredNotes; }

private:
    std::vector<int> m_enteredNotes;
    std::vector<int> m_targetMelody = { 2, 1, 0, 1, 2 }; // E - D - C - D - E
    int m_selectedKey = 0;
};

} // namespace Puzzle
} // namespace SHO
