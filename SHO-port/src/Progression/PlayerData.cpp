#include "SHO/Progression/PlayerData.h"

namespace SHO {
namespace Progression {

PlayerData& PlayerData::GetInstance() {
    static PlayerData instance;
    return instance;
}

void PlayerData::SetFlag(const std::string& flagName, bool value) {
    m_flags[flagName] = value;
}

bool PlayerData::GetFlag(const std::string& flagName) const {
    auto it = m_flags.find(flagName);
    if (it != m_flags.end()) {
        return it->second;
    }
    return false;
}

void PlayerData::Reset() {
    m_stats = PlayerStats{};
    m_flags.clear();
    m_currentZone = "IntroRoad";
    m_currentSpawn = "SpawnRoad01";
}

std::vector<std::string> PlayerData::EvaluateAccolades(EndingType ending) const {
    std::vector<std::string> accolades;

    // 1. Savior (Good ending)
    if (ending == EndingType::Good) {
        accolades.push_back("Savior");
    }

    // 2. Butcher (Bad ending / high kill count)
    if (ending == EndingType::Bad || (m_stats.enemiesKilledMelee + m_stats.enemiesKilledGun >= 75)) {
        accolades.push_back("Butcher");
    }

    // 3. UFO (UFO ending)
    if (ending == EndingType::UFO) {
        accolades.push_back("Ambassador");
    }

    // 4. Sprinter (Beat game under 2 hours)
    if (m_stats.timePlayedSec < 7200.0f) {
        accolades.push_back("Sprinter");
    }

    // 5. Sharpshooter (75%+ kills with firearm)
    int totalKills = m_stats.enemiesKilledMelee + m_stats.enemiesKilledGun + m_stats.enemiesKilledStomp;
    if (totalKills >= 30 && (float)m_stats.enemiesKilledGun / (float)totalKills >= 0.75f) {
        accolades.push_back("Sharpshooter");
    }

    // 6. Brawler (75%+ kills with melee/fists)
    if (totalKills >= 30 && (float)m_stats.enemiesKilledMelee / (float)totalKills >= 0.75f) {
        accolades.push_back("Brawler");
    }

    // 7. Cartographer (checked maps frequently)
    if (m_stats.itemsCollected >= 100) {
        accolades.push_back("Collector");
    }

    // 8. Daredevil (Beat game with 0 saves)
    if (m_stats.saveCount == 0) {
        accolades.push_back("Daredevil");
    }

    return accolades;
}

} // namespace Progression
} // namespace SHO
