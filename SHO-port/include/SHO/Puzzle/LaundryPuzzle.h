#pragma once

#include "SHO/Puzzle/PuzzleBase.h"

namespace SHO {
namespace Puzzle {

class LaundryPuzzle : public PuzzleBase {
public:
    LaundryPuzzle();

    void OnInit() override;
    bool HandleInput(Core::PadButton button) override;

    void NextDial();
    void PrevDial();
    void RotateDial(int delta);
    void PressStart();

    bool CheckSolution() const;

    int GetCurrentDialIndex() const { return m_activeDial; }
    int GetWashCycle() const { return m_washCycle; }
    int GetSpinSpeed() const { return m_spinSpeed; }
    int GetTemperature() const { return m_temperature; }
    int GetRinseMode() const { return m_rinseMode; }

private:
    int m_activeDial = 0; // 0: Cycle, 1: Temp, 2: Spin, 3: Rinse
    int m_washCycle = 0;   // 0: Delicate, 1: Normal, 2: Heavy, 3: Soak
    int m_temperature = 0; // 0: Cold, 1: Warm, 2: Hot
    int m_spinSpeed = 0;   // 0: Low, 1: Medium, 2: High
    int m_rinseMode = 0;   // 0: Auto, 1: Drain
};

} // namespace Puzzle
} // namespace SHO
