#pragma once

#include "SHO/Puzzle/PuzzleBase.h"

namespace SHO {
namespace Puzzle {

enum class StageBackdrop {
    None = 0,
    Forest = 1,
    Library = 2,
    Cave = 3
};

enum class StageProp {
    None = 0,
    Tree = 1,
    Table = 2,
    MirrorArch = 3
};

enum class StageLight {
    None = 0,
    Sunlight = 1,
    Moonlight = 2,
    BloodMoon = 3
};

class TheatreBackdropPuzzle : public PuzzleBase {
public:
    TheatreBackdropPuzzle();

    void OnInit() override;
    bool HandleInput(Core::PadButton button) override;

    void SetBackdrop(StageBackdrop bd);
    void SetProp(StageProp prop);
    void SetLight(StageLight light);

    bool CheckSolution() const;

    StageBackdrop GetBackdrop() const { return m_backdrop; }
    StageProp GetProp() const { return m_prop; }
    StageLight GetLight() const { return m_light; }

private:
    StageBackdrop m_backdrop = StageBackdrop::None;
    StageProp     m_prop     = StageProp::None;
    StageLight    m_light    = StageLight::None;
};

} // namespace Puzzle
} // namespace SHO
