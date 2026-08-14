#include "SHO/Inventory/ItemDef.h"
#include <algorithm>

namespace SHO {
namespace Inventory {

bool ItemDef::DegradeDurability(int hits) {
    if (m_maxDurability <= 0) {
        // Indestructible weapon (fists, etc.)
        return false;
    }

    m_durability = std::max(0, m_durability - hits);
    return m_durability <= 0;
}

} // namespace Inventory
} // namespace SHO
