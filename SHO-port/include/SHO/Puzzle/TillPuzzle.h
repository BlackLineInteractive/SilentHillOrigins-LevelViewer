#pragma once

#include "SHO/Puzzle/PuzzleBase.h"
#include <string>

namespace SHO {
namespace Puzzle {

class TillPuzzle : public PuzzleBase {
public:
    TillPuzzle();

    void OnInit() override;
    bool HandleInput(Core::PadButton button) override;

    void EnterDigit(char digit);
    void ClearDigits();
    void PressEnter();

    const std::string& GetCurrentInput() const { return m_enteredCode; }
    int GetSelectedKeyIndex() const { return m_selectedKey; }

private:
    std::string m_enteredCode;
    std::string m_targetCode = "0823"; // Clue date from note
    int m_selectedKey = 0; // 0-9 for digits, 10 for Clear, 11 for Enter
};

} // namespace Puzzle
} // namespace SHO
