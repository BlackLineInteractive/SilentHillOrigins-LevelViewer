#pragma once

#include "SHO/Core/Types.h"
#include <string>

namespace SHO {
namespace Inventory {

enum class ItemCategory {
    Key,
    Supply,     // Health drink, First Aid Kit, Ampoule
    WeaponMelee,// Fists, Pipe, Knife, Hammer, TV, etc.
    WeaponRanged,// Pistol, Shotgun, Rifle
    Ammo,       // Pistol bullets, Shotgun shells, Rifle rounds
    Document,   // Notes, diaries, maps
    Special     // Flashlight, Radio, Flauros piece
};

enum class HealthItemType {
    None,
    HealthDrink,    // Restores ~25% HP, small stamina boost
    FirstAidKit,    // Restores ~75% HP
    Ampoule         // Restores 100% HP + infinite stamina for a duration
};

// Translated from CInventoryItemDef (size 236 bytes in SLES)
class ItemDef {
public:
    ItemDef() = default;
    ItemDef(const std::string& id, const std::string& name, ItemCategory cat, int count = 1)
        : m_id(id), m_name(name), m_category(cat), m_count(count) {}

    const std::string& GetId() const { return m_id; }
    void SetId(const std::string& id) { m_id = id; }

    const std::string& GetName() const { return m_name; }
    void SetName(const std::string& name) { m_name = name; }

    const std::string& GetDescription() const { return m_description; }
    void SetDescription(const std::string& desc) { m_description = desc; }

    ItemCategory GetCategory() const { return m_category; }
    void SetCategory(ItemCategory cat) { m_category = cat; }

    int GetCount() const { return m_count; }
    void SetCount(int count) { m_count = count; }
    void AddCount(int amount) { m_count += amount; }

    // Weapon specific properties
    float GetDamage() const { return m_damage; }
    void SetDamage(float dmg) { m_damage = dmg; }

    float GetRange() const { return m_range; }
    void SetRange(float rng) { m_range = rng; }

    // Durability for Melee Weapons (e.g. TV breaks in 1 hit, pipe lasts many)
    int GetDurability() const { return m_durability; }
    int GetMaxDurability() const { return m_maxDurability; }
    void SetDurability(int maxDur) { m_durability = maxDur; m_maxDurability = maxDur; }
    bool DegradeDurability(int hits = 1);
    bool IsBroken() const { return m_maxDurability > 0 && m_durability <= 0; }

    // Ammo / Ranged properties
    int GetAmmoInClip() const { return m_ammoInClip; }
    int GetClipCapacity() const { return m_clipCapacity; }
    void SetClipCapacity(int cap) { m_clipCapacity = cap; m_ammoInClip = cap; }
    void SetAmmoInClip(int ammo) { m_ammoInClip = ammo; }
    const std::string& GetRequiredAmmoType() const { return m_requiredAmmoType; }
    void SetRequiredAmmoType(const std::string& ammoId) { m_requiredAmmoType = ammoId; }

    // Model & UI assets
    const std::string& GetModelName() const { return m_modelName; }
    void SetModelName(const std::string& model) { m_modelName = model; }

    const std::string& GetIconName() const { return m_iconName; }
    void SetIconName(const std::string& icon) { m_iconName = icon; }

    HealthItemType GetHealthType() const { return m_healthType; }
    void SetHealthType(HealthItemType ht) { m_healthType = ht; }

private:
    std::string  m_id;
    std::string  m_name;
    std::string  m_description;
    ItemCategory m_category = ItemCategory::Supply;
    int          m_count = 1;

    // Combat attributes
    float m_damage = 10.0f;
    float m_range = 1.2f;
    int   m_durability = -1;     // -1 = indestructible (e.g. fists)
    int   m_maxDurability = -1;

    int   m_ammoInClip = 0;
    int   m_clipCapacity = 0;
    std::string m_requiredAmmoType;

    HealthItemType m_healthType = HealthItemType::None;
    std::string m_modelName;
    std::string m_iconName;
};

} // namespace Inventory
} // namespace SHO
