#pragma once

#include "SHO/Triggers/TriggerBase.h"
#include "SHO/Inventory/ItemDef.h"

namespace SHO {
namespace Inventory {

// Translated from CPickupItem & CDynamicHealthItem
class PickupItem : public Triggers::TriggerBase {
public:
    PickupItem();
    virtual ~PickupItem() = default;

    void SetItem(const ItemDef& item) { m_item = item; }
    const ItemDef& GetItem() const { return m_item; }
    ItemDef& GetItem() { return m_item; }

    float GetPickupRadius() const { return m_radius; }
    void SetPickupRadius(float r) { m_radius = r; }

    bool IsPickedUp() const { return m_isPickedUp; }
    void SetPickedUp(bool picked) { m_isPickedUp = picked; SetVisible(!picked); }

    bool CheckTrigger(const Core::Vec3& actorPos, const Core::Vec3& prevActorPos) override;
    bool TryPickup();

private:
    ItemDef m_item;
    float   m_radius = 1.4f;
    bool    m_isPickedUp = false;
    bool    m_playerInRange = false;
};

} // namespace Inventory
} // namespace SHO
