#pragma once

#include "SHO/Inventory/ItemDef.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace SHO {
namespace Inventory {

class InventoryManager {
public:
    static InventoryManager& GetInstance();

    // Inventory operations
    void AddItem(const ItemDef& item);
    bool RemoveItem(const std::string& itemId, int count = 1);
    bool HasItem(const std::string& itemId, int count = 1) const;
    ItemDef* FindItem(const std::string& itemId);
    const ItemDef* FindItem(const std::string& itemId) const;

    // Item filtering
    std::vector<ItemDef*> GetItemsByCategory(ItemCategory cat);
    const std::vector<ItemDef>& GetAllItems() const { return m_items; }

    // Weapon equipment
    bool EquipWeapon(const std::string& weaponId);
    ItemDef* GetEquippedWeapon() { return m_equippedWeapon; }
    const ItemDef* GetEquippedWeapon() const { return m_equippedWeapon; }
    void UnequipWeapon() { m_equippedWeapon = nullptr; }

    // Using items (Healing drinks, first aid, ammo reload)
    bool UseItem(const std::string& itemId, void* playerActor);

    // Flashlight & Radio status
    bool HasFlashlight() const { return m_hasFlashlight; }
    void SetHasFlashlight(bool has) { m_hasFlashlight = has; }

    bool HasRadio() const { return m_hasRadio; }
    void SetHasRadio(bool has) { m_hasRadio = has; }

    void Clear();

private:
    InventoryManager();
    ~InventoryManager() = default;

    std::vector<ItemDef> m_items;
    ItemDef* m_equippedWeapon = nullptr;
    bool m_hasFlashlight = true;
    bool m_hasRadio = true;
};

} // namespace Inventory
} // namespace SHO
