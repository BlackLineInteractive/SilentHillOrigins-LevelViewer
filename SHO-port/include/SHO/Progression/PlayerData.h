#pragma once

#include "SHO/Core/Types.h"
#include <map>
#include <string>
#include <vector>

namespace SHO {
namespace Progression {

enum class EndingType {
    Good,
    Bad,
    UFO
};

// Player statistics and save game state (translated from PlayerData & AccoladeData)
struct PlayerStats {
    float timePlayedSec = 0.0f;
    float distanceWalked = 0.0f;
    float distanceRan = 0.0f;
    int   enemiesKilledMelee = 0;
    int   enemiesKilledGun = 0;
    int   enemiesKilledStomp = 0;
    int   saveCount = 0;
    int   itemsCollected = 0;
    float flashlightTimeSec = 0.0f;
    float damageTaken = 0.0f;
};

class PlayerData {
public:
    static PlayerData& GetInstance();

    PlayerStats& GetStats() { return m_stats; }
    const PlayerStats& GetStats() const { return m_stats; }

    // Level progression flags (e.g. "DoorExamRoomUnlocked", "MirrorExamRoomSharded")
    void SetFlag(const std::string& flagName, bool value = true);
    bool GetFlag(const std::string& flagName) const;

    // Current zone & spawn
    const std::string& GetCurrentZone() const { return m_currentZone; }
    void SetCurrentZone(const std::string& zone) { m_currentZone = zone; }

    const std::string& GetCurrentSpawn() const { return m_currentSpawn; }
    void SetCurrentSpawn(const std::string& spawn) { m_currentSpawn = spawn; }

    // Reset / New Game
    void Reset();

    // Check earned accolades at game ending
    std::vector<std::string> EvaluateAccolades(EndingType ending) const;

private:
    PlayerData() = default;
    ~PlayerData() = default;

    PlayerStats m_stats;
    std::map<std::string, bool> m_flags;
    std::string m_currentZone = "IntroRoad";
    std::string m_currentSpawn = "SpawnRoad01";
};

} // namespace Progression
} // namespace SHO
