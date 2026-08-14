#include "SHO/Bridge/LevelBridge.h"
#include "SHO/Camera/StaticCamera.h"
#include "SHO/Camera/ConstraintCamera.h"
#include "SHO/Camera/CameraManager.h"
#include "SHO/Triggers/PlaneTrigger.h"
#include "SHO/Triggers/ZoneTrigger.h"
#include "SHO/Triggers/ButtonTrigger.h"
#include "SHO/Triggers/AreaTrigger.h"
#include "SHO/Environment/FogConfig.h"
#include "SHO/Environment/Light.h"
#include "SHO/Actor/PlayerLight.h"
#include "SHO/Actor/MirrorReflector.h"
#include "SHO/Actor/Monster.h"
#include "SHO/Inventory/PickupItem.h"
#include "SHO/Progression/SavePoint.h"
#include "SHO/World/World.h"
#include "ClimaxEngine/Game/CameraLinks.h"
#include "ClimaxEngine/Game/ZoneLinks.h"
#include "ClimaxEngine/Game/ButtonTriggers.h"

#include <cmath>
#include <iostream>

namespace SHO {
namespace Bridge {

void LevelBridge::ResetWorld() {
    World::World::GetInstance().Clear();
}

bool LevelBridge::PopulateWorldFromLevel(
    const std::vector<GameObject>& objects,
    const std::vector<LevelCamera>& cameras,
    const CollisionMesh& collision,
    float fogStart,
    float fogEnd,
    float fogDensity,
    const glm::vec3& fogColor,
    const std::string& preferredSpawn)
{
    auto& world = World::World::GetInstance();
    world.Clear();
    world.Init();

    // ── 1. Collision Mesh (CBSP) ─────────────────────────────────────────────
    if (!collision.indices.empty() && !collision.verts.empty()) {
        std::vector<Core::CollisionFace> faces;
        faces.reserve(collision.indices.size() / 3);

        for (size_t i = 0; i + 2 < collision.indices.size(); i += 3) {
            uint32_t i0 = collision.indices[i];
            uint32_t i1 = collision.indices[i + 1];
            uint32_t i2 = collision.indices[i + 2];

            if (i0 < collision.verts.size() &&
                i1 < collision.verts.size() &&
                i2 < collision.verts.size()) {

                Core::CollisionFace f;
                f.v0 = collision.verts[i0];
                f.v1 = collision.verts[i1];
                f.v2 = collision.verts[i2];
                f.CalculateNormal();
                faces.push_back(f);
            }
        }
        world.SetCollisionFaces(faces);
    }

    // ── 2. Fog Configuration ─────────────────────────────────────────────────
    auto fog = std::make_shared<Environment::FogConfig>();
    fog->SetStartDistance(fogStart);
    fog->SetEndDistance(fogEnd);
    fog->SetDensity(fogDensity);
    fog->SetColor(fogColor);
    fog->SetFogEnabled(true);
    world.SetFogConfig(fog);

    // ── 3. Cameras ───────────────────────────────────────────────────────────
    for (const auto& cam : cameras) {
        auto c = std::make_shared<Camera::StaticCamera>();
        c->SetName(cam.name);
        c->SetEventName(cam.name);
        if (!cam.altName.empty()) {
            c->SetAltEventName(cam.altName);
        }
        c->SetPosition(cam.position);
        c->SetTransform(cam.transform);
        c->SetFOV(cam.fovDeg > 0.0f ? cam.fovDeg : 60.0f);
        world.AddEntity(c);
    }

    // ── 4. Plane Triggers (Camera Switching) ─────────────────────────────────
    auto switches = ClimaxEngine::Game::BuildCameraSwitches(objects, cameras);
    for (const auto& sw : switches) {
        auto trig = std::make_shared<Triggers::PlaneTrigger>();
        trig->SetPosition(sw.position);
        trig->SetTransform(sw.transform);

        // Compute thinnest extent axis as plane normal
        Core::Vec3 axis[3] = {
            Core::Vec3(sw.transform[0]),
            Core::Vec3(sw.transform[1]),
            Core::Vec3(sw.transform[2])
        };
        float ext[3] = { glm::length(axis[0]), glm::length(axis[1]), glm::length(axis[2]) };
        int thin = 0;
        for (int k = 1; k < 3; ++k) {
            if (ext[k] < ext[thin]) thin = k;
        }

        if (ext[thin] > 1e-5f) {
            trig->SetPlaneNormal(axis[thin] / ext[thin]);
        }

        trig->SetForwardCameraName(sw.nameA);
        trig->SetBackwardCameraName(sw.nameB);
        world.AddEntity(trig);
    }

    // ── 5. Zone Links (Doors and Portals) ─────────────────────────────────────
    auto zoneLinks = ClimaxEngine::Game::BuildZoneLinks(objects);
    for (const auto& zl : zoneLinks) {
        auto zt = std::make_shared<Triggers::ZoneTrigger>();
        zt->SetPosition(zl.position);
        zt->SetTransform(zl.transform);
        zt->SetSourceZone(zl.fromZone);
        zt->SetTargetZone(zl.toZone);
        zt->SetTargetSpawnName(zl.fromZone);

        // Oriented bounding box bounds
        Core::AABB box;
        Core::Vec3 pos = zl.position;
        float hx = glm::length(Core::Vec3(zl.transform[0]));
        float hy = glm::length(Core::Vec3(zl.transform[1]));
        float hz = glm::length(Core::Vec3(zl.transform[2]));
        box.min = pos - Core::Vec3(hx, hy, hz);
        box.max = pos + Core::Vec3(hx, hy, hz);
        zt->SetBounds(box);

        world.AddEntity(zt);
    }

    // ── 6. Button & Examination Triggers ─────────────────────────────────────
    auto btnTriggers = ClimaxEngine::Game::BuildButtonTriggers(objects);
    for (const auto& bt : btnTriggers) {
        if (bt.className == "SavePoint") {
            auto sp = std::make_shared<Progression::SavePoint>();
            sp->SetPosition(bt.position);
            sp->SetTransform(bt.transform);
            world.AddEntity(sp);
        } else {
            auto btn = std::make_shared<Triggers::ButtonTrigger>();
            btn->SetPosition(bt.position);
            btn->SetTransform(bt.transform);
            btn->SetActionEventName(bt.eventName);
            btn->SetPromptStringId(bt.targetMap);

            Core::AABB box;
            Core::Vec3 pos = bt.position;
            float hx = glm::length(Core::Vec3(bt.transform[0]));
            float hy = glm::length(Core::Vec3(bt.transform[1]));
            float hz = glm::length(Core::Vec3(bt.transform[2]));
            box.min = pos - Core::Vec3(hx, hy, hz);
            box.max = pos + Core::Vec3(hx, hy, hz);
            btn->SetBoxBounds(box);

            world.AddEntity(btn);
        }
    }

    // ── 7. Lights, Pickups, Monsters, Mirrors, and Spawners ───────────────────
    Core::Vec3 spawnPos(0.0f);
    Core::Vec3 spawnFacing(0, 0, 1);
    bool foundSpawn = false;

    for (const auto& go : objects) {
        if (go.isLight) {
            auto light = std::make_shared<Environment::Light>();
            light->SetPosition(go.haveLightPos ? go.lightPos : go.position);
            light->SetColor(go.lightColor);
            light->SetRange(go.lightRange > 0.0f ? go.lightRange : 10.0f);
            light->SetConeAngleDeg(go.lightAngle);
            world.AddEntity(light);
        } else if (go.className == "CPlayerReflector") {
            auto mirror = std::make_shared<Actor::MirrorReflector>();
            mirror->SetPosition(go.position);
            mirror->SetTransform(go.transform);
            mirror->SetMirrorNormal(Core::Vec3(go.transform[2]));
            if (!go.linkNames.empty()) {
                mirror->SetOtherworldZone(go.linkNames[0]);
            }
            world.AddEntity(mirror);
        } else if (go.className == "CPickupItem" || go.className == "CDynamicHealthItem") {
            auto pickup = std::make_shared<Inventory::PickupItem>();
            pickup->SetPosition(go.position);
            pickup->SetTransform(go.transform);

            Inventory::ItemDef item(go.objName.empty() ? "Item_Generic" : go.objName,
                                   go.objName,
                                   Inventory::ItemCategory::Supply);
            if (go.className == "CDynamicHealthItem") {
                item.SetHealthType(Inventory::HealthItemType::HealthDrink);
            }
            pickup->SetItem(item);
            world.AddEntity(pickup);
        } else if (go.className == "CEnemyBehaviour" ||
                   go.className == "CNurseBehaviour" ||
                   go.className == "CStraightJacketBehaviour" ||
                   go.className == "CButcherBehaviour" ||
                   go.className == "CCalibanBehaviour") {

            Actor::MonsterType mtype = Actor::MonsterType::Nurse;
            if (go.className == "CStraightJacketBehaviour") mtype = Actor::MonsterType::StraightJacket;
            else if (go.className == "CButcherBehaviour")   mtype = Actor::MonsterType::Butcher;
            else if (go.className == "CCalibanBehaviour")   mtype = Actor::MonsterType::Caliban;

            auto monster = std::make_shared<Actor::Monster>(mtype);
            monster->SetPosition(go.position);
            monster->SetTransform(go.transform);
            monster->SetTarget(world.GetPlayer().get());
            world.AddEntity(monster);
        } else if (go.className == "CPlayerSpawner") {
            if (!foundSpawn || (!preferredSpawn.empty() && go.objName == preferredSpawn)) {
                spawnPos = go.position;
                Core::Vec3 fwd = Core::Vec3(go.transform[2]);
                if (glm::length(fwd) > 1e-4f) spawnFacing = glm::normalize(fwd);
                foundSpawn = true;
            }
        }
    }

    // Spawn player at chosen spawner location
    if (auto player = world.GetPlayer()) {
        player->SetPosition(spawnPos);
        float yaw = glm::degrees(std::atan2(spawnFacing.x, spawnFacing.z));
        player->SetYaw(yaw);
    }

    return true;
}

} // namespace Bridge
} // namespace SHO
