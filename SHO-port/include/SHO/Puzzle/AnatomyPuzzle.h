#pragma once

#include "SHO/Puzzle/PuzzleBase.h"
#include <array>

namespace SHO {
namespace Puzzle {

enum class OrganType {
    None = -1,
    Lungs = 0,
    Heart = 1,
    Liver = 2,
    Stomach = 3,
    Intestines = 4,
    Count = 5
};

class AnatomyPuzzle : public PuzzleBase {
public:
    AnatomyPuzzle();

    void OnInit() override;
    bool HandleInput(Core::PadButton button) override;

    // Place organ into next open anatomical slot
    bool InsertOrgan(OrganType organ);
    // Remove last inserted organ
    void RemoveLastOrgan();

    bool CheckSolution() const;
    const std::vector<OrganType>& GetPlacedOrgans() const { return m_placedOrgans; }

    int GetSelectedOrganIndex() const { return m_selectedOrganIdx; }

private:
    std::vector<OrganType> m_placedOrgans;
    int m_selectedOrganIdx = 0;
};

} // namespace Puzzle
} // namespace SHO
