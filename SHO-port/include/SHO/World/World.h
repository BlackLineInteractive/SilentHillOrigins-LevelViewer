#pragma once

#include "SHO/Core/Behaviour.h"
#include "SHO/Triggers/TriggerBase.h"
#include "SHO/Actor/PlayerBehaviour.h"
#include "SHO/Environment/FogConfig.h"
#include "SHO/Environment/Light.h"
#include "SHO/Camera/CameraManager.h"
#include <memory>
#include <string>
#include <vector>

namespace SHO {
namespace World {

// Central Game World Management
class World {
public:
    static World& GetInstance();

    void Init();
    void Update(float dt);
    void FixedUpdate(float fixedDt);
    void Clear();

    // Entity registration
    void AddEntity(std::shared_ptr<Core::Behaviour> entity);
    void RemoveEntity(std::shared_ptr<Core::Behaviour> entity);

    // Player access
    std::shared_ptr<Actor::PlayerBehaviour> GetPlayer() const { return m_player; }
    void SetPlayer(std::shared_ptr<Actor::PlayerBehaviour> player) { m_player = player; }

    // Environment access
    std::shared_ptr<Environment::FogConfig> GetFogConfig() const { return m_fog; }
    void SetFogConfig(std::shared_ptr<Environment::FogConfig> fog) { m_fog = fog; }

    const std::vector<std::shared_ptr<Environment::Light>>& GetLights() const { return m_lights; }
    const std::vector<std::shared_ptr<Triggers::TriggerBase>>& GetTriggers() const { return m_triggers; }

    // Collision faces feed
    void SetCollisionFaces(const std::vector<Core::CollisionFace>& faces);

private:
    World() = default;
    ~World() = default;

    std::vector<std::shared_ptr<Core::Behaviour>> m_entities;
    std::vector<std::shared_ptr<Triggers::TriggerBase>> m_triggers;
    std::vector<std::shared_ptr<Environment::Light>> m_lights;
    std::shared_ptr<Environment::FogConfig> m_fog;
    std::shared_ptr<Actor::PlayerBehaviour> m_player;

    std::vector<Core::CollisionFace> m_collisionFaces;
    Core::Vec3 m_prevPlayerPos = Core::Vec3(0.0f);
};

} // namespace World
} // namespace SHO
