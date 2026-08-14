#include "SHO/Puzzle/PuzzleManager.h"
#include "SHO/Puzzle/AnatomyPuzzle.h"
#include "SHO/Puzzle/FlaurosPuzzle.h"
#include "SHO/Puzzle/LaundryPuzzle.h"
#include "SHO/Puzzle/TillPuzzle.h"
#include "SHO/Puzzle/PillDollPuzzle.h"
#include "SHO/Puzzle/CircuitBreakerPuzzle.h"
#include "SHO/Puzzle/IronLungPuzzle.h"
#include "SHO/Puzzle/OrganBoxPuzzle.h"
#include "SHO/Puzzle/CalendarPuzzle.h"
#include "SHO/Puzzle/TheatreBackdropPuzzle.h"
#include "SHO/Core/EventManager.h"
#include "ClimaxEngine/Core/RWS/FileSystem/CArchive.h"
#include "ClimaxEngine/Core/UI/ScreenDef.h"
#include <iostream>

namespace SHO {
namespace Puzzle {


void PuzzleManager::Init() {
    Clear();

    // Register all game puzzles
    RegisterPuzzle(std::make_shared<AnatomyPuzzle>());
    RegisterPuzzle(std::make_shared<FlaurosPuzzle>());
    RegisterPuzzle(std::make_shared<LaundryPuzzle>());
    RegisterPuzzle(std::make_shared<TillPuzzle>());
    RegisterPuzzle(std::make_shared<PillDollPuzzle>());
    RegisterPuzzle(std::make_shared<CircuitBreakerPuzzle>());
    RegisterPuzzle(std::make_shared<IronLungPuzzle>());
    RegisterPuzzle(std::make_shared<OrganBoxPuzzle>());
    RegisterPuzzle(std::make_shared<CalendarPuzzle>());
    RegisterPuzzle(std::make_shared<TheatreBackdropPuzzle>());

    // Register event listeners to trigger puzzle screens on RWS events
    auto& em = Core::EventManager::GetInstance();
    for (const auto& pair : m_puzzles) {
        std::string id = pair.first;
        std::string openEvent = "OpenPuzzle_" + id;
        std::string xmlEvent = "OpenXml_" + pair.second->GetXmlFile();

        em.LinkMsg(openEvent, [this, id](const Core::Msg& msg) {
            (void)msg;
            this->OpenPuzzle(id);
        });

        em.LinkMsg(xmlEvent, [this, id](const Core::Msg& msg) {
            (void)msg;
            this->OpenPuzzle(id);
        });
    }
}



void PuzzleManager::RegisterPuzzle(std::shared_ptr<PuzzleBase> puzzle) {
    if (puzzle) {
        puzzle->OnInit();
        m_puzzles[puzzle->GetPuzzleId()] = puzzle;
        m_puzzles[puzzle->GetXmlFile()] = puzzle;
    }
}

std::shared_ptr<PuzzleBase> PuzzleManager::GetPuzzle(const std::string& id) const {
    auto it = m_puzzles.find(id);
    if (it != m_puzzles.end()) {
        return it->second;
    }
    return nullptr;
}

bool PuzzleManager::OpenPuzzle(const std::string& id) {
    auto puzzle = GetPuzzle(id);
    if (puzzle) {
        m_activePuzzle = puzzle;
        m_activePuzzle->OnOpen();
        std::cout << "[PUZZLE] Activated: " << id << " (" << puzzle->GetXmlFile() << ")" << std::endl;
        return true;
    }
    return false;
}

void PuzzleManager::CloseActivePuzzle() {
    if (m_activePuzzle) {
        m_activePuzzle->OnClose();
        m_activePuzzle = nullptr;
    }
}

bool PuzzleManager::HandleInput(Core::PadButton button) {
    if (m_activePuzzle && m_activePuzzle->GetStatus() == PuzzleStatus::InProgress) {
        return m_activePuzzle->HandleInput(button);
    }
    return false;
}

void PuzzleManager::Update(float dt) {
    if (m_activePuzzle) {
        m_activePuzzle->OnUpdate(dt);
        if (m_activePuzzle->GetStatus() != PuzzleStatus::InProgress) {
            // Closed or solved
            m_activePuzzle = nullptr;
        }
    }
}

bool PuzzleManager::LoadPuzzlesFromArchive(const std::string& arcPath) {

    ClimaxEngine::RWS::FileSystem::CArchive arc;
    if (!arc.Open(arcPath)) {
        std::cerr << "[PUZZLE] Failed to open archive: " << arcPath << std::endl;
        return false;
    }

    int loadedCount = 0;
    for (auto& pair : m_puzzles) {
        auto puzzle = pair.second;
        if (!puzzle) continue;

        int entryIdx = arc.Find(puzzle->GetXmlFile());
        if (entryIdx >= 0) {
            std::vector<uint8_t> data;
            if (arc.Read(entryIdx, data) && !data.empty()) {
                ClimaxEngine::UI::Element root;
                if (ClimaxEngine::UI::ParseXml((const char*)data.data(), data.size(), root)) {
                    puzzle->LoadFromXml(&root);
                    loadedCount++;
                }
            }
        }
    }
    std::cout << "[PUZZLE] Loaded " << loadedCount << " XML puzzle screen definitions from " << arcPath << std::endl;
    return loadedCount > 0;
}

bool PuzzleManager::LoadPuzzleFromXmlString(const std::string& xmlFilename, const std::string& xmlContent) {
    auto puzzle = GetPuzzle(xmlFilename);
    if (!puzzle) return false;

    ClimaxEngine::UI::Element root;
    if (ClimaxEngine::UI::ParseXml(xmlContent.data(), xmlContent.size(), root)) {
        puzzle->LoadFromXml(&root);
        return true;
    }
    return false;
}

void PuzzleManager::Clear() {
    m_puzzles.clear();
    m_activePuzzle = nullptr;
}

} // namespace Puzzle
} // namespace SHO

