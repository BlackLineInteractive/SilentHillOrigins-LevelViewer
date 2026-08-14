#include "SHO/Puzzle/PuzzleBase.h"
#include "SHO/Core/EventManager.h"
#include "SHO/Inventory/InventoryManager.h"
#include "SHO/Inventory/ItemDef.h"
#include "SHO/Progression/PlayerData.h"
#include "ClimaxEngine/Core/UI/ScreenDef.h"
#include <iostream>

namespace SHO {
namespace Puzzle {

void PuzzleBase::OnSolve() {
    m_status = PuzzleStatus::Solved;
    std::cout << "[PUZZLE] Solved: " << m_puzzleId << std::endl;

    // 1. Record in progression flags
    Progression::PlayerData::GetInstance().SetFlag("PuzzleSolved_" + m_puzzleId, true);

    // 2. Grant reward item if configured
    if (!m_rewardItemId.empty()) {
        Inventory::ItemDef reward(m_rewardItemId, m_rewardItemId, Inventory::ItemCategory::Key);
        Inventory::InventoryManager::GetInstance().AddItem(reward);
        std::cout << "[PUZZLE] Awarded item: " << m_rewardItemId << std::endl;
    }

    // 3. Dispatch RWS event for world triggers / doors / lights
    if (!m_solveEvent.empty()) {
        Core::EventManager::GetInstance().SendMsg(m_solveEvent);
    }
}

void PuzzleBase::LoadFromXml(const void* xmlRootElement) {

    if (!xmlRootElement) return;
    const auto* root = static_cast<const ClimaxEngine::UI::Element*>(xmlRootElement);

    // Look for backdrop IMAGE or SCREEN bgtexture
    if (const auto* img = root->Find("IMAGE")) {
        std::string bg = img->Attr("bgtexture");
        if (!bg.empty()) {
            m_backdropTex = bg;
        }
    }
}

} // namespace Puzzle
} // namespace SHO

