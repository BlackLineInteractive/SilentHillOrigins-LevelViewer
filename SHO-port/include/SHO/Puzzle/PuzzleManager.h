#pragma once

#include "SHO/Puzzle/PuzzleBase.h"
#include <memory>
#include <unordered_map>
#include <string>

namespace SHO {
namespace Puzzle {

class PuzzleManager {
public:
    static PuzzleManager& GetInstance() {
        static PuzzleManager instance;
        return instance;
    }

    void Init();
    void RegisterPuzzle(std::shared_ptr<PuzzleBase> puzzle);
    std::shared_ptr<PuzzleBase> GetPuzzle(const std::string& id) const;

    bool OpenPuzzle(const std::string& id);
    void CloseActivePuzzle();

    // Read and parse all 40 XML puzzle/UI scripts directly from SH.ARC
    bool LoadPuzzlesFromArchive(const std::string& arcPath);
    // Parse single XML data string for a puzzle
    bool LoadPuzzleFromXmlString(const std::string& xmlFilename, const std::string& xmlContent);

    std::shared_ptr<PuzzleBase> GetActivePuzzle() const { return m_activePuzzle; }
    bool IsPuzzleActive() const { return m_activePuzzle != nullptr && m_activePuzzle->GetStatus() == PuzzleStatus::InProgress; }

    bool HandleInput(Core::PadButton button);
    void Update(float dt);

    void Clear();

private:
    PuzzleManager() = default;
    std::unordered_map<std::string, std::shared_ptr<PuzzleBase>> m_puzzles;
    std::shared_ptr<PuzzleBase> m_activePuzzle = nullptr;
};


} // namespace Puzzle
} // namespace SHO
