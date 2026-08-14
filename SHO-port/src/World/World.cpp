#include "SHO/World/World.h"
#include "SHO/World/SceneQueue.h"
#include "SHO/Core/EventManager.h"
#include <algorithm>

namespace SHO {
namespace World {

World& World::GetInstance() {
    static World instance;
    return instance;
}

void World::Init() {
    m_fog = std::make_shared<Environment::FogConfig>();
    m_player = std::make_shared<Actor::PlayerBehaviour>();
    m_player->OnInit();
}

void World::SetCollisionFaces(const std::vector<Core::CollisionFace>& faces) {
    m_collisionFaces = faces;
    if (m_player) {
        m_player->GetController().SetCollisionMesh(faces);
    }
}

void World::AddEntity(std::shared_ptr<Core::Behaviour> entity) {
    if (!entity) return;

    m_entities.push_back(entity);
    entity->OnInit();

    if (auto trig = std::dynamic_pointer_cast<Triggers::TriggerBase>(entity)) {
        m_triggers.push_back(trig);
    }
    if (auto light = std::dynamic_pointer_cast<Environment::Light>(entity)) {
        m_lights.push_back(light);
    }
    if (auto cam = std::dynamic_pointer_cast<Camera::BaseCamera>(entity)) {
        Camera::CameraManager::GetInstance().RegisterCamera(cam.get());
    }
}

void World::RemoveEntity(std::shared_ptr<Core::Behaviour> entity) {
    if (!entity) return;

    entity->OnDestroy();
    m_entities.erase(std::remove(m_entities.begin(), m_entities.end(), entity), m_entities.end());

    if (auto trig = std::dynamic_pointer_cast<Triggers::TriggerBase>(entity)) {
        m_triggers.erase(std::remove(m_triggers.begin(), m_triggers.end(), trig), m_triggers.end());
    }
    if (auto light = std::dynamic_pointer_cast<Environment::Light>(entity)) {
        m_lights.erase(std::remove(m_lights.begin(), m_lights.end(), light), m_lights.end());
    }
    if (auto cam = std::dynamic_pointer_cast<Camera::BaseCamera>(entity)) {
        Camera::CameraManager::GetInstance().UnregisterCamera(cam.get());
    }
}

void World::Update(float dt) {
    // Process scene queue transitions
    SceneQueue::GetInstance().Update(dt);

    // Update player
    if (m_player && m_player->IsActive()) {
        Core::Vec3 currPos = m_player->GetPosition();
        m_player->OnUpdate(dt);

        // Check triggers
        for (auto& trig : m_triggers) {
            if (trig) {
                trig->CheckTrigger(m_player->GetPosition(), currPos);
            }
        }
        m_prevPlayerPos = currPos;
    }

    // Update all general entities
    for (auto& entity : m_entities) {
        if (entity && entity->IsActive()) {
            entity->OnUpdate(dt);
        }
    }

    // Update cameras tracking player
    Core::Vec3 targetPos = m_player ? (m_player->GetPosition() + Core::Vec3(0, 1.2f, 0)) : Core::Vec3(0.0f);
    Camera::CameraManager::GetInstance().Update(dt, targetPos);

    // Flush any pending event messages
    Core::EventManager::GetInstance().FlushQueuedMessages();
}

void World::FixedUpdate(float fixedDt) {
    if (m_player && m_player->IsActive()) {
        m_player->OnFixedUpdate(fixedDt);
    }

    for (auto& entity : m_entities) {
        if (entity && entity->IsActive()) {
            entity->OnFixedUpdate(fixedDt);
        }
    }
}

void World::Clear() {
    for (auto& entity : m_entities) {
        if (entity) entity->OnDestroy();
    }
    m_entities.clear();
    m_triggers.clear();
    m_lights.clear();
    m_collisionFaces.clear();
    Camera::CameraManager::GetInstance().Clear();
    SceneQueue::GetInstance().Clear();
}

} // namespace World
} // namespace SHO
