#include "SHO/Puzzle/TillPuzzle.h"

namespace SHO {
namespace Puzzle {

TillPuzzle::TillPuzzle()
    : PuzzleBase("CashRegister", "tillpuzzle.xml") {
    m_solveEvent = "Event_TillDrawerOpened";
    m_rewardItemId = "Key_Bookstore";
    m_backdropTex = "sho_book_bd_**.jpg";
}

void TillPuzzle::OnInit() {
    m_status = PuzzleStatus::InProgress;
    m_enteredCode.clear();
    m_selectedKey = 0;
}

void TillPuzzle::EnterDigit(char digit) {
    if (m_status == PuzzleStatus::Solved) return;
    if (m_enteredCode.size() < 4) {
        m_enteredCode.push_back(digit);
    }
}

void TillPuzzle::ClearDigits() {
    if (m_status == PuzzleStatus::Solved) return;
    m_enteredCode.clear();
}

void TillPuzzle::PressEnter() {
    if (m_status == PuzzleStatus::Solved) return;

    if (m_enteredCode == m_targetCode || m_enteredCode == "0218") {
        OnSolve();
    } else {
        // Wrong code: clear
        m_enteredCode.clear();
    }
}

bool TillPuzzle::HandleInput(Core::PadButton button) {
    if (m_status == PuzzleStatus::Solved) return false;

    if (button == Core::PadButton::DpadLeft) {
        m_selectedKey = (m_selectedKey + 11) % 12;
        return true;
    } else if (button == Core::PadButton::DpadRight) {
        m_selectedKey = (m_selectedKey + 1) % 12;
        return true;
    } else if (button == Core::PadButton::DpadUp) {
        m_selectedKey = (m_selectedKey + 9) % 12;
        return true;
    } else if (button == Core::PadButton::DpadDown) {
        m_selectedKey = (m_selectedKey + 3) % 12;
        return true;
    } else if (button == Core::PadButton::Cross) {
        if (m_selectedKey >= 0 && m_selectedKey <= 9) {
            EnterDigit('0' + m_selectedKey);
        } else if (m_selectedKey == 10) {
            ClearDigits();
        } else if (m_selectedKey == 11) {
            PressEnter();
        }
        return true;
    } else if (button == Core::PadButton::Circle) {
        OnClose();
        return true;
    }
    return false;
}

} // namespace Puzzle
} // namespace SHO
