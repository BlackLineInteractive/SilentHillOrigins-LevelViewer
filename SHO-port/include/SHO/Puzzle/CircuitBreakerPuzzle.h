#pragma once

#include "SHO/Puzzle/PuzzleBase.h"
#include <array>

namespace SHO {
namespace Puzzle {

struct BreakerSwitch {
    int amperage = 0;
    bool isOn    = false;
};

class CircuitBreakerPuzzle : public PuzzleBase {
public:
    CircuitBreakerPuzzle();

    void OnInit() override;
    bool HandleInput(Core::PadButton button) override;

    void ToggleSwitch(int idx);
    int GetCurrentTotalAmps() const;
    bool CheckSolution() const;

    int GetSelectedSwitch() const { return m_selectedSwitch; }
    int GetTargetAmperage() const { return m_targetAmps; }
    const std::array<BreakerSwitch, 5>& GetSwitches() const { return m_switches; }

private:
    std::array<BreakerSwitch, 5> m_switches;
    int m_selectedSwitch = 0;
    int m_targetAmps = 65; // Total target load in Amperes
};

} // namespace Puzzle
} // namespace SHO
