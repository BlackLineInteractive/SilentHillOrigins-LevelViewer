#include "SHO/Puzzle/AnatomyPuzzle.h"
#include <algorithm>

namespace SHO {
namespace Puzzle {

AnatomyPuzzle::AnatomyPuzzle()
    : PuzzleBase("AnatomyDoll", "anatomypuzzle.xml") {
    m_solveEvent = "Event_DollEyesOpened";
    m_rewardItemId = "Key_GlassEyes";
    m_backdropTex = "sho_doll_bd_**.jpg";
}

void AnatomyPuzzle::OnInit() {
    m_placedOrgans.clear();
    m_selectedOrganIdx = 0;
    m_status = PuzzleStatus::InProgress;
}

bool AnatomyPuzzle::InsertOrgan(OrganType organ) {
    if (m_status == PuzzleStatus::Solved) return false;
    if (m_placedOrgans.size() >= 5) return false;

    // Check if organ is already placed
    auto it = std::find(m_placedOrgans.begin(), m_placedOrgans.end(), organ);
    if (it != m_placedOrgans.end()) return false;

    m_placedOrgans.push_back(organ);

    if (m_placedOrgans.size() == 5) {
        if (CheckSolution()) {
            OnSolve();
            return true;
        }
    }
    return false;
}

void AnatomyPuzzle::RemoveLastOrgan() {
    if (!m_placedOrgans.empty() && m_status != PuzzleStatus::Solved) {
        m_placedOrgans.pop_back();
    }
}

bool AnatomyPuzzle::CheckSolution() const {
    // In Silent Hill Origins, correct anatomical top-to-bottom placement:
    // 0: Lungs, 1: Heart, 2: Liver, 3: Stomach, 4: Intestines
    if (m_placedOrgans.size() != 5) return false;

    return (m_placedOrgans[0] == OrganType::Lungs &&
            m_placedOrgans[1] == OrganType::Heart &&
            m_placedOrgans[2] == OrganType::Liver &&
            m_placedOrgans[3] == OrganType::Stomach &&
            m_placedOrgans[4] == OrganType::Intestines);
}

bool AnatomyPuzzle::HandleInput(Core::PadButton button) {
    if (m_status == PuzzleStatus::Solved) return false;

    if (button == Core::PadButton::DpadUp || button == Core::PadButton::DpadLeft) {
        m_selectedOrganIdx = (m_selectedOrganIdx + 4) % 5;
        return true;
    } else if (button == Core::PadButton::DpadDown || button == Core::PadButton::DpadRight) {
        m_selectedOrganIdx = (m_selectedOrganIdx + 1) % 5;
        return true;
    } else if (button == Core::PadButton::Cross) {
        // Place selected organ
        InsertOrgan(static_cast<OrganType>(m_selectedOrganIdx));
        return true;
    } else if (button == Core::PadButton::Triangle) {
        // Remove organ
        RemoveLastOrgan();
        return true;
    } else if (button == Core::PadButton::Circle) {
        OnClose();
        return true;
    }
    return false;
}

} // namespace Puzzle
} // namespace SHO
