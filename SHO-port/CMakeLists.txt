# ── SHO-port (Silent Hill: Origins Gameplay & Port Engine) ────────────────────

set(SHO_PORT_SOURCES
    src/Core/EventManager.cpp
    src/Core/SystemCommands.cpp
    src/Core/Behaviour.cpp
    src/Camera/BaseCamera.cpp
    src/Camera/StaticCamera.cpp
    src/Camera/ConstraintCamera.cpp
    src/Camera/CameraManager.cpp
    src/Triggers/PlaneTrigger.cpp
    src/Triggers/ZoneTrigger.cpp
    src/Triggers/ButtonTrigger.cpp
    src/Triggers/AreaTrigger.cpp
    src/Environment/FogConfig.cpp
    src/Environment/Light.cpp
    src/Actor/Actor.cpp
    src/Actor/CharacterController.cpp
    src/Actor/PlayerBehaviour.cpp
    src/Actor/PlayerLight.cpp
    src/Actor/MirrorReflector.cpp
    src/Actor/EnemyBehaviour.cpp
    src/Actor/Monster.cpp
    src/Inventory/ItemDef.cpp
    src/Inventory/PickupItem.cpp
    src/Inventory/InventoryManager.cpp
    src/Combat/CombatSystem.cpp
    src/Progression/PlayerData.cpp
    src/Progression/SavePoint.cpp
    src/World/SceneQueue.cpp
    src/World/World.cpp
    src/Bridge/LevelBridge.cpp
    src/Audio/AudioEntities.cpp
    src/Triggers/TriggerSystem.cpp
    src/Audio/AudioEngine.cpp
    src/Environment/FogSystem.cpp
    src/Puzzle/PuzzleBase.cpp


    src/Puzzle/AnatomyPuzzle.cpp
    src/Puzzle/FlaurosPuzzle.cpp
    src/Puzzle/LaundryPuzzle.cpp
    src/Puzzle/TillPuzzle.cpp
    src/Puzzle/PillDollPuzzle.cpp
    src/Puzzle/CircuitBreakerPuzzle.cpp
    src/Puzzle/IronLungPuzzle.cpp
    src/Puzzle/OrganBoxPuzzle.cpp
    src/Puzzle/CalendarPuzzle.cpp
    src/Puzzle/TheatreBackdropPuzzle.cpp
    src/Puzzle/PuzzleManager.cpp
)

add_library(sho-port STATIC ${SHO_PORT_SOURCES})

target_include_directories(sho-port PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(sho-port PUBLIC climax-game glm::glm)

add_executable(test_sho_port tests/test_sho_port.cpp)
target_link_libraries(test_sho_port PRIVATE sho-port)

# ── sho-game: the whole game, in one executable ──────────────────────────────
#
# Two halves that used to be two targets. `play_main.cpp` is the front end --
# splash, language, aspect, the logo movie, the main menu, the intro clip and
# the scene queue, all transcribed from SLES_551.47 -- and `Main.cpp` takes
# over at the queue's handover command and runs the level. One window, one
# process. See include/ClimaxEngine/Game/PlayMain.h.
set(SHO_GAME_SOURCES
    src/Main.cpp
    ${CMAKE_SOURCE_DIR}/src/play_main.cpp
    ${CMAKE_SOURCE_DIR}/src/Viewer/ViewerGraphics.cpp
    ${CMAKE_SOURCE_DIR}/src/Viewer/ViewerDraw.cpp
    ${CMAKE_SOURCE_DIR}/src/Viewer/ViewerAudio.cpp
    ${CMAKE_SOURCE_DIR}/src/UI/UI.cpp
    ${CMAKE_SOURCE_DIR}/src/Loader/Loader.cpp
    ${CMAKE_SOURCE_DIR}/src/Loader/GeometryDecoder.cpp
    ${CMAKE_SOURCE_DIR}/src/Loader/ResourceLoader.cpp
    ${CMAKE_SOURCE_DIR}/src/Loader/Export.cpp
    ${CMAKE_SOURCE_DIR}/src/SG/SceneObject.cpp
    ${CMAKE_SOURCE_DIR}/src/Platform/Wii/WiiTexture.cpp
    ${CMAKE_SOURCE_DIR}/src/Platform/PS2/RwsAudio.cpp
    ${CMAKE_SOURCE_DIR}/src/Core/Common.cpp
    ${CMAKE_SOURCE_DIR}/src/Rendering/RenderBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/Rendering/GLSLBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/Rendering/CPURasterizer.cpp
    ${CMAKE_SOURCE_DIR}/src/Rendering/GPUMesh.cpp
    ${CMAKE_SOURCE_DIR}/src/Rendering/GLTextureSink.cpp
    ${CMAKE_SOURCE_DIR}/src/Rendering/PlayerModel.cpp
    ${IMGUI_SOURCES}
    ${IMGUIZMO_SOURCES}
    ${IMANIM_SOURCES}
)
if(APPLE)
    list(APPEND SHO_GAME_SOURCES ${CMAKE_SOURCE_DIR}/src/Rendering/MetalBackend.mm)
endif()
# The boot movies and the intro. Optional at configure time: without it the
# front end still runs, it just says so instead of drawing an invented logo.
if(SHO_HAVE_FFMPEG)
    list(APPEND SHO_GAME_SOURCES ${CMAKE_SOURCE_DIR}/src/Rendering/VideoPlayer.cpp)
endif()

add_executable(sho-game ${SHO_GAME_SOURCES})

target_include_directories(sho-game PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${IMGUIZMO_DIR}
    ${CMAKE_SOURCE_DIR}/src/UI/ImAnim
)

target_link_libraries(sho-game PRIVATE
    sho-port
    climax-game
    climax-core
    OpenGL::GL
    GLEW::GLEW
    ${SHO_SDL2_TARGETS}
    ${SHO_SDL2_IMAGE}
    ${EXTRA_LIBS}
)

if(SDL2_INCLUDE_DIRS)
    target_include_directories(sho-game PRIVATE ${SDL2_INCLUDE_DIRS})
endif()

if(SHO_HAVE_FFMPEG)
    target_include_directories(sho-game PRIVATE ${FFMPEG_INCLUDE_DIRS})
    target_link_directories(sho-game PRIVATE ${FFMPEG_LIBRARY_DIRS})
    target_link_libraries(sho-game PRIVATE ${FFMPEG_LIBRARIES})
    target_compile_definitions(sho-game PRIVATE CLIMAX_HAVE_FFMPEG=1)
else()
    message(STATUS "libavformat/libavcodec/libswscale not found: sho-game will build without movie playback")
endif()

