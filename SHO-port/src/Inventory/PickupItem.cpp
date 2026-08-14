#include "SHO/Inventory/PickupItem.h"
#include "SHO/Inventory/InventoryManager.h"
#include "SHO/Core/EventManager.h"
#include <iostream>

namespace SHO {
namespace Inventory {

PickupItem::PickupItem() {
    m_className = "CPickupItem";
}

bool PickupItem::CheckTrigger(const Core::Vec3& actorPos, const Core::Vec3& /*prevActorPos*/) {
    if (m_isPickedUp || !m_active || !m_triggerEnabled) return false;

    float dist = glm::distance(actorPos, GetPosition());
    bool inRange = dist <= m_radius;

    if (inRange && !m_playerInRange) {
        m_playerInRange = true;
        // Prompt pickup
        Core::EventManager::GetInstance().SendMsg("ShowPickupPrompt", this, (void*)m_item.GetName().c_str());
    } else if (!inRange && m_playerInRange) {
        m_playerInRange = false;
        Core::EventManager::GetInstance().SendMsg("HidePickupPrompt", this);
    }

    return inRange;
}

bool PickupItem::TryPickup() {
    if (m_isPickedUp || !m_playerInRange) return false;

    InventoryManager::GetInstance().AddItem(m_item);
    SetPickedUp(true);
    m_playerInRange = false;

    // Send item collected event
    Core::EventManager::GetInstance().SendMsg("ItemCollected", this, (void*)m_item.GetId().c_str());
    Core::EventManager::GetInstance().SendMsg("HidePickupPrompt", this);
    return true;
}

} // namespace Inventory
} // namespace SHO
