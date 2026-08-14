#pragma once

#include "SHO/Core/Types.h"
#include <string>
#include <vector>

namespace SHO {
namespace Puzzle {

enum class PuzzleStatus {
    Inactive,
    InProgress,
    Solved,
    Cancelled
};

// Base class for all 40 Silent Hill: Origins puzzles and interactive scripts
class PuzzleBase {
public:
    PuzzleBase(const std::string& puzzleId, const std::string& xmlFile)
        : m_puzzleId(puzzleId), m_xmlFile(xmlFile) {}
    virtual ~PuzzleBase() = default;

    const std::string& GetPuzzleId() const { return m_puzzleId; }
    const std::string& GetXmlFile() const { return m_xmlFile; }
    PuzzleStatus GetStatus() const { return m_status; }
    bool IsSolved() const { return m_status == PuzzleStatus::Solved; }

    // Lifecycle
    virtual void OnInit() {}
    virtual void OnOpen() { m_status = PuzzleStatus::InProgress; }
    virtual void OnClose() {
        if (m_status == PuzzleStatus::InProgress) {
            m_status = PuzzleStatus::Cancelled;
        }
    }
    virtual void OnUpdate(float dt) { (void)dt; }

    // Pad input handling during puzzle interaction
    virtual bool HandleInput(Core::PadButton button) = 0;

    // Triggered when puzzle reaches winning state
    virtual void OnSolve();

    // Reward / solved event name
    const std::string& GetSolveEvent() const { return m_solveEvent; }
    void SetSolveEvent(const std::string& evt) { m_solveEvent = evt; }

    const std::string& GetBackdropTexture() const { return m_backdropTex; }
    void SetBackdropTexture(const std::string& tex) { m_backdropTex = tex; }

    // Parse visual layout & backdrop properties directly from XML
    virtual void LoadFromXml(const void* xmlRootElement);

protected:
    std::string  m_puzzleId;
    std::string  m_xmlFile;
    PuzzleStatus m_status = PuzzleStatus::Inactive;
    std::string  m_solveEvent;
    std::string  m_rewardItemId;
    std::string  m_backdropTex;
};


} // namespace Puzzle
} // namespace SHO
