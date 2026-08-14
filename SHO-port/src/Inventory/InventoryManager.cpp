#include "SHO/Inventory/InventoryManager.h"
#include "SHO/Actor/PlayerBehaviour.h"
#include "SHO/Core/EventManager.h"
#include <algorithm>

namespace SHO {
namespace Inventory {

InventoryManager& InventoryManager::GetInstance() {
    static InventoryManager instance;
    return instance;
}

InventoryManager::InventoryManager() {
    // Default Travis fists (unarmed melee weapon)
    ItemDef fists("Weapon_Fists", "Fists", ItemCategory::WeaponMelee);
    fists.SetDamage(8.0f);
    fists.SetRange(1.0f);
    fists.SetDurability(-1); // Indestructible
    AddItem(fists);
    EquipWeapon("Weapon_Fists");
}

void InventoryManager::AddItem(const ItemDef& item) {
    auto* existing = FindItem(item.GetId());
    if (existing) {
        existing->AddCount(item.GetCount());
    } else {
        m_items.push_back(item);
    }
}

bool InventoryManager::RemoveItem(const std::string& itemId, int count) {
    for (auto it = m_items.begin(); it != m_items.end(); ++it) {
        if (it->GetId() == itemId) {
            if (it->GetCount() > count) {
                it->SetCount(it->GetCount() - count);
                return true;
            } else {
                if (m_equippedWeapon && m_equippedWeapon->GetId() == itemId) {
                    m_equippedWeapon = nullptr;
                }
                m_items.erase(it);
                return true;
            }
        }
    }
    return false;
}

bool InventoryManager::HasItem(const std::string& itemId, int count) const {
    const auto* item = FindItem(itemId);
    return item && item->GetCount() >= count;
}

ItemDef* InventoryManager::FindItem(const std::string& itemId) {
    for (auto& item : m_items) {
        if (item.GetId() == itemId) return &item;
    }
    return nullptr;
}

const ItemDef* InventoryManager::FindItem(const std::string& itemId) const {
    for (const auto& item : m_items) {
        if (item.GetId() == itemId) return &item;
    }
    return nullptr;
}

std::vector<ItemDef*> InventoryManager::GetItemsByCategory(ItemCategory cat) {
    std::vector<ItemDef*> result;
    for (auto& item : m_items) {
        if (item.GetCategory() == cat) {
            result.push_back(&item);
        }
    }
    return result;
}

bool InventoryManager::EquipWeapon(const std::string& weaponId) {
    auto* item = FindItem(weaponId);
    if (!item) return false;

    if (item->GetCategory() == ItemCategory::WeaponMelee ||
        item->GetCategory() == ItemCategory::WeaponRanged) {
        m_equippedWeapon = item;
        Core::EventManager::GetInstance().SendMsg("WeaponEquipped", this, (void*)weaponId.c_str());
        return true;
    }
    return false;
}

bool InventoryManager::UseItem(const std::string& itemId, void* playerActor) {
    auto* item = FindItem(itemId);
    if (!item) return false;

    auto* player = static_cast<Actor::PlayerBehaviour*>(playerActor);

    switch (item->GetHealthType()) {
    case HealthItemType::HealthDrink:
        if (player) player->Heal(player->GetMaxHealth() * 0.25f);
        RemoveItem(itemId, 1);
        Core::EventManager::GetInstance().SendMsg("ItemUsed", this, (void*)itemId.c_str());
        return true;

    case HealthItemType::FirstAidKit:
        if (player) player->Heal(player->GetMaxHealth() * 0.75f);
        RemoveItem(itemId, 1);
        Core::EventManager::GetInstance().SendMsg("ItemUsed", this, (void*)itemId.c_str());
        return true;

    case HealthItemType::Ampoule:
        if (player) player->Heal(player->GetMaxHealth());
        RemoveItem(itemId, 1);
        Core::EventManager::GetInstance().SendMsg("ItemUsed", this, (void*)itemId.c_str());
        return true;

    default:
        break;
    }

    // Equip if weapon
    if (item->GetCategory() == ItemCategory::WeaponMelee ||
        item->GetCategory() == ItemCategory::WeaponRanged) {
        return EquipWeapon(itemId);
    }

    return false;
}

void InventoryManager::Clear() {
    m_items.clear();
    m_equippedWeapon = nullptr;
}

} // namespace Inventory
} // namespace SHO
