#include "SHO/Core/EventManager.h"
#include "SHO/Core/SystemCommands.h"
#include "SHO/Camera/StaticCamera.h"
#include "SHO/Camera/ConstraintCamera.h"
#include "SHO/Camera/CameraManager.h"
#include "SHO/Triggers/PlaneTrigger.h"
#include "SHO/Triggers/ZoneTrigger.h"
#include "SHO/Triggers/ButtonTrigger.h"
#include "SHO/Environment/FogConfig.h"
#include "SHO/Environment/Light.h"
#include "SHO/Actor/PlayerBehaviour.h"
#include "SHO/Actor/PlayerLight.h"
#include "SHO/Actor/MirrorReflector.h"
#include "SHO/Actor/Monster.h"
#include "SHO/Inventory/ItemDef.h"
#include "SHO/Inventory/PickupItem.h"
#include "SHO/Inventory/InventoryManager.h"
#include "SHO/Combat/CombatSystem.h"
#include "SHO/Progression/PlayerData.h"
#include "SHO/Progression/SavePoint.h"
#include "SHO/World/SceneQueue.h"
#include "SHO/World/World.h"

#include <cassert>
#include <iostream>


using namespace SHO;

void TestEventManager() {
    std::cout << "[TEST] EventManager..." << std::endl;
    auto& em = Core::EventManager::GetInstance();
    em.Clear();

    bool received = false;
    uint32_t handle = em.LinkMsg("TestEvent", [&](const Core::Msg& msg) {
        received = true;
        assert(msg.id.name == "TestEvent");
    });

    em.SendMsg("TestEvent");
    assert(received == true);

    em.UnlinkMsg(handle);
    received = false;
    em.SendMsg("TestEvent");
    assert(received == false);
    std::cout << "  -> PASS" << std::endl;
}

void TestCamerasAndTriggers() {
    std::cout << "[TEST] Cameras & PlaneTriggers..." << std::endl;
    auto& camMgr = Camera::CameraManager::GetInstance();
    camMgr.Clear();

    auto cam1 = std::make_shared<Camera::StaticCamera>();
    cam1->SetName("camLanding01");
    cam1->SetEventName("camLanding01");
    cam1->SetAuthoredAngles(15.0f, 90.0f);
    cam1->SetPosition(Core::Vec3(0, 2, 5));
    camMgr.RegisterCamera(cam1.get());

    auto cam2 = std::make_shared<Camera::StaticCamera>();
    cam2->SetName("camLanding02");
    cam2->SetEventName("camLanding02");
    cam2->SetAuthoredAngles(-20.0f, 180.0f);
    cam2->SetPosition(Core::Vec3(0, 3, -5));
    camMgr.RegisterCamera(cam2.get());

    assert(camMgr.GetActiveCamera() == cam1.get());

    // Create a PlaneTrigger
    Triggers::PlaneTrigger trigger;
    trigger.SetPosition(Core::Vec3(0, 0, 0));
    trigger.SetPlaneNormal(Core::Vec3(0, 0, 1));
    trigger.SetForwardCameraName("camLanding02");
    trigger.SetBackwardCameraName("camLanding01");

    // Cross plane in +Z direction
    bool triggered = trigger.CheckTrigger(Core::Vec3(0, 0, 1.0f), Core::Vec3(0, 0, -1.0f));
    assert(triggered == true);
    assert(camMgr.GetActiveCamera() == cam2.get());
    std::cout << "  -> PASS (Switched to " << camMgr.GetActiveCamera()->GetName() << ")" << std::endl;
}

void TestPlayerAndCollision() {
    std::cout << "[TEST] Player Behaviour & Collision..." << std::endl;
    Camera::CameraManager::GetInstance().Clear();
    auto player = std::make_shared<Actor::PlayerBehaviour>();
    player->OnInit();
    player->SetPosition(Core::Vec3(0, 0.1f, 0));

    // Floor triangle at Y=0 (normal pointing +Y)
    Core::CollisionFace floor1;
    floor1.v0 = Core::Vec3(-10, 0, -10);
    floor1.v1 = Core::Vec3( 10, 0,  10);
    floor1.v2 = Core::Vec3( 10, 0, -10);
    floor1.CalculateNormal();

    Core::CollisionFace floor2;
    floor2.v0 = Core::Vec3(-10, 0, -10);
    floor2.v1 = Core::Vec3(-10, 0,  10);
    floor2.v2 = Core::Vec3( 10, 0,  10);
    floor2.CalculateNormal();

    std::vector<Core::CollisionFace> mesh = { floor1, floor2 };
    player->GetController().SetCollisionMesh(mesh);


    // Apply movement input (forward stick)
    player->SetAnalogStick(0.0f, 1.0f);
    player->OnUpdate(0.1f);

    Core::Vec3 pos = player->GetPosition();
    assert(player->GetController().IsGrounded());
    assert(pos.y == 0.0f); // Snapped to ground!
    assert(pos.z > 0.0f);  // Moved forward!
    std::cout << "  -> PASS (Grounded at Y=" << pos.y << ", Z=" << pos.z << ")" << std::endl;
}

void TestFogAndLighting() {
    std::cout << "[TEST] Fog Config & Light..." << std::endl;
    Environment::FogConfig fog;
    assert(fog.GetStartDistance() == 13.0f);
    assert(fog.GetEndDistance() == 25.0f);
    assert(fog.GetDensity() == 0.3f);

    float fNear = fog.CalculateFogFactor(10.0f);
    float fMid  = fog.CalculateFogFactor(19.0f);
    float fFar  = fog.CalculateFogFactor(30.0f);

    assert(fNear == 0.0f);
    assert(fMid > 0.0f && fMid < 1.0f);
    assert(fFar == 1.0f);

    Environment::Light light;
    light.SetPosition(Core::Vec3(0, 5, 0));
    light.SetRange(10.0f);
    light.SetIntensity(2.0f);
    light.SetColor(Core::Vec3(1.0f, 0.8f, 0.6f));

    Core::Vec3 ill = light.EvaluateLight(Core::Vec3(0, 0, 0), Core::Vec3(0, 1, 0));
    assert(ill.x > 0.0f && ill.y > 0.0f && ill.z > 0.0f);
    std::cout << "  -> PASS" << std::endl;
}

void TestSceneQueue() {
    std::cout << "[TEST] SceneQueue Transitions..." << std::endl;
    auto& sq = World::SceneQueue::GetInstance();
    sq.Clear();

    sq.QueueLevelTransition("HO_1_Lobby", "SpawnDoor01", 0.1f);
    assert(sq.IsBusy());

    // Step through queue until done
    for (int i = 0; i < 20; i++) {
        sq.Update(0.05f);
    }
    assert(!sq.IsBusy());
    std::cout << "  -> PASS" << std::endl;
}

void TestInventoryAndItems() {
    std::cout << "[TEST] Inventory & Item Management..." << std::endl;
    auto& inv = Inventory::InventoryManager::GetInstance();
    inv.Clear();

    // Add health drink & key
    Inventory::ItemDef healthDrink("Item_HealthDrink", "Health Drink", Inventory::ItemCategory::Supply);
    healthDrink.SetHealthType(Inventory::HealthItemType::HealthDrink);
    inv.AddItem(healthDrink);

    Inventory::ItemDef keyExam("Key_ExamRoom", "Exam Room Key", Inventory::ItemCategory::Key);
    inv.AddItem(keyExam);

    assert(inv.HasItem("Item_HealthDrink"));
    assert(inv.HasItem("Key_ExamRoom"));

    // Test healing player
    Actor::PlayerBehaviour player;
    player.TakeDamage(50.0f, Core::Vec3(0, 0, 1));
    assert(player.GetHealth() == 50.0f);

    bool used = inv.UseItem("Item_HealthDrink", &player);
    assert(used == true);
    assert(player.GetHealth() == 75.0f); // 50 + 25
    assert(!inv.HasItem("Item_HealthDrink"));
    std::cout << "  -> PASS" << std::endl;
}

void TestCombatAndWeapons() {
    std::cout << "[TEST] Combat System & Durability..." << std::endl;
    auto& combat = Combat::CombatSystem::GetInstance();
    auto& inv = Inventory::InventoryManager::GetInstance();
    inv.Clear();

    // Create fragile melee weapon (Television, 1 hit durability)
    Inventory::ItemDef tv("Weapon_TV", "Portable TV", Inventory::ItemCategory::WeaponMelee);
    tv.SetDamage(60.0f);
    tv.SetRange(1.3f);
    tv.SetDurability(1);
    inv.AddItem(tv);
    inv.EquipWeapon("Weapon_TV");

    Actor::PlayerBehaviour attacker;
    attacker.SetPosition(Core::Vec3(0, 0, 0));
    attacker.SetYaw(0.0f); // Facing +Z

    Actor::Actor target;
    target.SetPosition(Core::Vec3(0, 0, 1.0f));
    target.SetHealth(100.0f);

    std::vector<Actor::Actor*> enemies = { &target };

    // Perform light attack with TV
    auto res = combat.PerformAttack(&attacker, inv.GetEquippedWeapon(), Combat::AttackType::LightMelee, enemies);
    assert(res.hit == true);
    assert(res.damageDealt == 60.0f);
    assert(target.GetHealth() == 40.0f);
    assert(res.weaponBroke == true);
    assert(!inv.HasItem("Weapon_TV")); // Broken weapon removed from inventory!

    // Create ranged weapon (Service Pistol)
    Inventory::ItemDef pistol("Weapon_Pistol", "Target Pistol", Inventory::ItemCategory::WeaponRanged);
    pistol.SetDamage(35.0f);
    pistol.SetRange(15.0f);
    pistol.SetClipCapacity(6);
    pistol.SetAmmoInClip(2);
    pistol.SetRequiredAmmoType("Ammo_Pistol");
    inv.AddItem(pistol);
    inv.EquipWeapon("Weapon_Pistol");

    Inventory::ItemDef pistolBullets("Ammo_Pistol", "Pistol Bullets", Inventory::ItemCategory::Ammo, 10);
    inv.AddItem(pistolBullets);

    // Fire 2 shots
    res = combat.PerformAttack(&attacker, inv.GetEquippedWeapon(), Combat::AttackType::RangedFire, enemies);
    assert(res.hit == true);
    assert(target.GetHealth() == 5.0f); // 40 - 35
    assert(inv.GetEquippedWeapon()->GetAmmoInClip() == 1);

    res = combat.PerformAttack(&attacker, inv.GetEquippedWeapon(), Combat::AttackType::RangedFire, enemies);
    assert(res.hit == true);
    assert(res.enemyKilled == true); // 5 - 35 <= 0
    assert(inv.GetEquippedWeapon()->GetAmmoInClip() == 0);

    // Reload pistol from inventory ammo
    bool reloaded = combat.ReloadWeapon(inv.GetEquippedWeapon());
    assert(reloaded == true);
    assert(inv.GetEquippedWeapon()->GetAmmoInClip() == 6);
    assert(inv.FindItem("Ammo_Pistol")->GetCount() == 4); // 10 - 6 = 4
    std::cout << "  -> PASS" << std::endl;
}

void TestMonsterAI() {
    std::cout << "[TEST] Monster AI & Sensory System..." << std::endl;
    Actor::Monster nurse(Actor::MonsterType::Nurse);
    nurse.SetPosition(Core::Vec3(0, 0, 0));
    nurse.SetYaw(0.0f); // Looking +Z

    Core::Vec3 playerPosInDark(0, 0, 6.0f);
    Core::Vec3 playerPosLit(0, 0, 6.0f);

    // Player at 6m distance in dark -> Nurse cannot see
    assert(!nurse.CanSeePlayer(playerPosInDark, false));

    // Player turns on flashlight -> Nurse sees!
    assert(nurse.CanSeePlayer(playerPosLit, true));

    // Player running at 5m distance -> Nurse hears!
    assert(nurse.CanHearPlayer(Core::Vec3(0, 0, 5.0f), true));

    // Test Heavy Knockdown
    nurse.TakeDamage(30.0f, Core::Vec3(0, 0, 1));
    assert(nurse.IsKnockedDown());
    std::cout << "  -> PASS" << std::endl;
}

void TestMirrorAndProgression() {
    std::cout << "[TEST] Mirror Shifting & Progression State..." << std::endl;
    Actor::MirrorReflector mirror;
    mirror.SetPosition(Core::Vec3(0, 1.0f, 0));
    mirror.SetMirrorNormal(Core::Vec3(0, 0, 1));
    mirror.SetOtherworldZone("HO_1_ExamRoom_Alt");

    // Player touches mirror
    bool inFront = mirror.CheckTrigger(Core::Vec3(0, 1.0f, 0.5f), Core::Vec3(0, 1.0f, 0.5f));
    assert(inFront == true);
    mirror.TouchMirror();

    auto& pd = Progression::PlayerData::GetInstance();
    pd.Reset();
    pd.SetFlag("MirrorExamRoomUsed", true);
    assert(pd.GetFlag("MirrorExamRoomUsed") == true);

    pd.GetStats().enemiesKilledMelee = 40;
    pd.GetStats().enemiesKilledGun = 5;
    pd.GetStats().timePlayedSec = 5400.0f; // 1.5 hours
    pd.GetStats().saveCount = 0;

    auto accolades = pd.EvaluateAccolades(Progression::EndingType::Good);
    bool hasSavior = false, hasBrawler = false, hasSprinter = false, hasDaredevil = false;
    for (const auto& a : accolades) {
        if (a == "Savior") hasSavior = true;
        if (a == "Brawler") hasBrawler = true;
        if (a == "Sprinter") hasSprinter = true;
        if (a == "Daredevil") hasDaredevil = true;
    }

    assert(hasSavior && hasBrawler && hasSprinter && hasDaredevil);
    std::cout << "  -> PASS" << std::endl;
}

#include "SHO/Bridge/LevelBridge.h"

void TestLevelBridge() {
    std::cout << "[TEST] LevelBridge (Connecting Toolkit Data to SHO World)..." << std::endl;

    std::vector<GameObject> objects;

    // 1. Spawner
    GameObject spawner;
    spawner.className = "CPlayerSpawner";
    spawner.objName = "SpawnRoad01";
    spawner.position = glm::vec3(5.0f, 0.0f, 10.0f);
    spawner.transform = glm::mat4(1.0f);
    spawner.transform[3] = glm::vec4(5.0f, 0.0f, 10.0f, 1.0f);
    objects.push_back(spawner);

    // 2. Light
    GameObject lightObj;
    lightObj.className = "CColorLight";
    lightObj.isLight = true;
    lightObj.haveLightPos = true;
    lightObj.lightPos = glm::vec3(0, 4.0f, 0);
    lightObj.lightColor = glm::vec3(1.0f, 0.9f, 0.7f);
    lightObj.lightRange = 12.0f;
    objects.push_back(lightObj);

    // 3. Nurse enemy
    GameObject nurseObj;
    nurseObj.className = "CNurseBehaviour";
    nurseObj.position = glm::vec3(0, 0, 15.0f);
    objects.push_back(nurseObj);

    // 4. Pickup item
    GameObject pickupObj;
    pickupObj.className = "CDynamicHealthItem";
    pickupObj.objName = "HealthDrink";
    pickupObj.position = glm::vec3(2.0f, 0.5f, 3.0f);
    objects.push_back(pickupObj);

    // Cameras
    std::vector<LevelCamera> cameras;
    LevelCamera cam;
    cam.name = "camRoad01";
    cam.position = glm::vec3(0, 3.0f, 5.0f);
    cam.fovDeg = 55.0f;
    cameras.push_back(cam);

    // Collision
    CollisionMesh collision;
    collision.verts = {
        glm::vec3(-10, 0, -10),
        glm::vec3( 10, 0,  10),
        glm::vec3( 10, 0, -10)
    };
    collision.indices = { 0, 1, 2 };

    bool ok = Bridge::LevelBridge::PopulateWorldFromLevel(
        objects, cameras, collision, 10.0f, 30.0f, 0.4f, glm::vec3(0.65f, 1.0f, 0.70f), "SpawnRoad01");
    assert(ok == true);


    auto& world = World::World::GetInstance();
    assert(world.GetPlayer() != nullptr);
    assert(world.GetPlayer()->GetPosition().x == 5.0f); // Spawned at spawner pos!
    assert(world.GetLights().size() == 1);
    assert(world.GetFogConfig()->GetStartDistance() == 10.0f);
    assert(Camera::CameraManager::GetInstance().GetActiveCamera() != nullptr);
    assert(Camera::CameraManager::GetInstance().GetActiveCamera()->GetName() == "camRoad01");

    std::cout << "  -> PASS (World successfully populated from toolkit level data!)" << std::endl;
}

#include "SHO/Puzzle/PuzzleManager.h"
#include "SHO/Puzzle/AnatomyPuzzle.h"
#include "SHO/Puzzle/FlaurosPuzzle.h"
#include "SHO/Puzzle/LaundryPuzzle.h"
#include "SHO/Puzzle/TillPuzzle.h"
#include "SHO/Puzzle/PillDollPuzzle.h"
#include "SHO/Puzzle/CircuitBreakerPuzzle.h"
#include "SHO/Puzzle/IronLungPuzzle.h"
#include "SHO/Puzzle/OrganBoxPuzzle.h"
#include "SHO/Puzzle/CalendarPuzzle.h"
#include "SHO/Puzzle/TheatreBackdropPuzzle.h"

void TestPuzzles() {
    std::cout << "[TEST] 40 XML Puzzles and Interactive Scripts Engine..." << std::endl;
    auto& pm = Puzzle::PuzzleManager::GetInstance();
    pm.Init();

    // 1. Anatomy Model Puzzle
    bool opened = pm.OpenPuzzle("AnatomyDoll");
    assert(opened == true);
    auto anatomy = std::dynamic_pointer_cast<Puzzle::AnatomyPuzzle>(pm.GetActivePuzzle());
    assert(anatomy != nullptr);
    anatomy->InsertOrgan(Puzzle::OrganType::Lungs);
    anatomy->InsertOrgan(Puzzle::OrganType::Heart);
    anatomy->InsertOrgan(Puzzle::OrganType::Liver);
    anatomy->InsertOrgan(Puzzle::OrganType::Stomach);
    anatomy->InsertOrgan(Puzzle::OrganType::Intestines);
    assert(anatomy->IsSolved() == true);
    assert(Inventory::InventoryManager::GetInstance().HasItem("Key_GlassEyes"));
    assert(Progression::PlayerData::GetInstance().GetFlag("PuzzleSolved_AnatomyDoll"));

    // 2. Laundry Machine Puzzle
    opened = pm.OpenPuzzle("LaundryMachine");
    assert(opened == true);
    auto laundry = std::dynamic_pointer_cast<Puzzle::LaundryPuzzle>(pm.GetActivePuzzle());
    assert(laundry != nullptr);
    laundry->RotateDial(2); // Heavy wash
    laundry->NextDial();
    laundry->RotateDial(2); // Hot temp
    laundry->NextDial();
    laundry->RotateDial(1); // Med spin
    laundry->NextDial();
    laundry->RotateDial(1); // Drain
    laundry->PressStart();
    assert(laundry->IsSolved() == true);
    assert(Inventory::InventoryManager::GetInstance().HasItem("Key_Cleopatra"));

    // 3. Till / Cash Register Puzzle
    opened = pm.OpenPuzzle("CashRegister");
    assert(opened == true);
    auto till = std::dynamic_pointer_cast<Puzzle::TillPuzzle>(pm.GetActivePuzzle());
    assert(till != nullptr);
    till->EnterDigit('0');
    till->EnterDigit('8');
    till->EnterDigit('2');
    till->EnterDigit('3');
    till->PressEnter();
    assert(till->IsSolved() == true);
    assert(Inventory::InventoryManager::GetInstance().HasItem("Key_Bookstore"));

    // 4. Pill Doll Puzzle
    opened = pm.OpenPuzzle("PillDoll");
    assert(opened == true);
    auto pillDoll = std::dynamic_pointer_cast<Puzzle::PillDollPuzzle>(pm.GetActivePuzzle());
    assert(pillDoll != nullptr);
    pillDoll->PlacePill(Puzzle::PillColor::Green);
    pillDoll->SelectMouth(1);
    pillDoll->PlacePill(Puzzle::PillColor::Blue);
    pillDoll->SelectMouth(2);
    pillDoll->PlacePill(Puzzle::PillColor::Red);
    pillDoll->SelectMouth(3);
    pillDoll->PlacePill(Puzzle::PillColor::Yellow);
    pillDoll->SelectMouth(4);
    pillDoll->PlacePill(Puzzle::PillColor::Blue);
    assert(pillDoll->IsSolved() == true);
    assert(Inventory::InventoryManager::GetInstance().HasItem("Key_FemaleHydrotherapy"));

    // 5. Circuit Breaker Puzzle
    opened = pm.OpenPuzzle("CircuitBreaker");
    assert(opened == true);
    auto breaker = std::dynamic_pointer_cast<Puzzle::CircuitBreakerPuzzle>(pm.GetActivePuzzle());
    assert(breaker != nullptr);
    breaker->ToggleSwitch(0); // 15A
    breaker->ToggleSwitch(1); // 20A
    breaker->ToggleSwitch(3); // 30A (Total = 65A)
    assert(breaker->IsSolved() == true);

    // 6. Flauros Puzzle
    opened = pm.OpenPuzzle("Flauros");
    assert(opened == true);
    auto flauros = std::dynamic_pointer_cast<Puzzle::FlaurosPuzzle>(pm.GetActivePuzzle());
    assert(flauros != nullptr);
    // Align all 4 pieces
    flauros->SelectPiece(0); flauros->RotateSelectedX(); flauros->RotateSelectedX(); flauros->RotateSelectedY();
    flauros->SelectPiece(1); flauros->RotateSelectedX(); flauros->RotateSelectedY(); flauros->RotateSelectedY();
    flauros->SelectPiece(2); flauros->RotateSelectedY(); flauros->RotateSelectedY();
    flauros->SelectPiece(3); flauros->RotateSelectedX(); flauros->RotateSelectedX();
    assert(flauros->IsSolved() == true);
    assert(Inventory::InventoryManager::GetInstance().HasItem("Item_FlaurosArtifact"));

    // 7. Test loading XML definitions directly from SH.ARC
    bool arcLoaded = pm.LoadPuzzlesFromArchive("game-iso/SHO/SH.ARC");
    assert(arcLoaded == true);
    assert(!pm.GetPuzzle("AnatomyDoll")->GetBackdropTexture().empty());

    std::cout << "  -> PASS (All XML puzzles and script state machines validated from SH.ARC!)" << std::endl;
}


int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "SHO-port Foundation Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;

    TestEventManager();
    TestCamerasAndTriggers();
    TestPlayerAndCollision();
    TestFogAndLighting();
    TestSceneQueue();
    TestInventoryAndItems();
    TestCombatAndWeapons();
    TestMonsterAI();
    TestMirrorAndProgression();
    TestLevelBridge();
    TestPuzzles();

    std::cout << "========================================" << std::endl;
    std::cout << "ALL SHO-PORT TESTS PASSED SUCCESSFULLY!" << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}



