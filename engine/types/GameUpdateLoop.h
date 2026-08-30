#pragma once
// GameUpdateLoop — main gameplay loop (extends UpdateLoop).
// Application-x64.exe, constructor at 0x140098060.
// Struct size: 0x160 bytes. Vtable at 0x14015e2f0 (12 entries).
//
// This is the active loop during gameplay. It owns all game subsystems:
// graphics scene, audio, renderer, UI, entities, citizens, pathfinder,
// weather, tutorial, save/load, camera, and the scenario/achievement instance.
//
// IMPORTANT: g_pGameWorldSingleton (DAT_1401c3dd8) points to the active
// GameUpdateLoop instance. All gameWorld+offset accesses in the codebase
// are fields of this struct.
//
// Vtable slots (all in exe):
//   +0x00: 0x140019DD0 — Destroy (scalar deleting dtor)
//   +0x08: 0x140097C00 — Startup (init UI dialogs, cursor, game systems)
//   +0x10: 0x1400977C0 — Shutdown (teardown all subsystems)
//   +0x18: 0x140096380 — Update (main simulation tick)
//   +0x20: 0x140096010 — Draw (render interpolated frame)
//   +0x28: 0x140096000 — DrawFailed (3 bytes, noop)
//   +0x30: 0x140096B00 — Load (create all subsystems)
//   +0x38: 0x1400959C0 — SaveRestore (serialize/deserialize game state)
//   +0x40: 0x140095970 — OnVideoError
//   +0x48: 0x140095930 — OnVideoRestart
//   +0x50: 0x140019D40 — GetResourceFilename
//   +0x58: 0x14003A8F4 — AllowsModChanges

#include "UpdateLoop.h"
#include <cstdint>
#include <cstddef>

// ============================================================
// GameUpdateLoop struct — 0x160 bytes
// ============================================================
// Fields verified from ctor (0x140098060), Load (0x140096B00),
// Update (0x140096380), Draw (0x140096010), Startup (0x140097C00),
// Shutdown (0x1400977C0), and Serialize (0x140095a90).

struct GameUpdateLoop {
    // --- Base class (UpdateLoop, 0x28 bytes) ---
    UpdateLoopVtable* vtable;       // +0x00: MSVC vptr (-> 0x14015e2f0)
    void* m_resourceList;           // +0x08: ExternalList* — loaded resources
    void* m_internalListA;          // +0x10: managed by Runtime
    void* m_internalListB;          // +0x18: cleared in ctor/dtor
    int32_t m_loadBit;              // +0x20: ExternalFactory bitmask index
    int32_t m_loadProgress;         // +0x24: load progress (0-100)

    // --- GameUpdateLoop fields (+0x28 to +0x15F) ---

    // Construction parameters (stored from ctor, used by Load)
    uint8_t  m_saveName[8];         // +0x28: System::String — save file name
    int32_t  m_mapSizeIndex;        // +0x30: map size parameter (also Random seed)
    int32_t  m_mapTypeIndex;        // +0x34: map type parameter
    int64_t  m_gameTime;            // +0x38: accumulated game time (Time)
    void*    m_mapConfig;           // +0x40: map generation config resource
    void*    m_terrainConfig;       // +0x48: terrain generation config resource
    void*    m_climateConfig;       // +0x50: climate generation config resource
    void*    m_savedState;          // +0x58: SavedState* (non-null = loading from save)

    // Rendering subsystems (created in Load, destroyed in Shutdown)
    void*    m_graphicsScene;       // +0x60: Graphics::Scene*
    void*    m_animationManager;    // +0x68: Graphics::AnimationManager*
    void*    m_pickingScene;        // +0x70: Picking::Scene*
    void*    m_audioScene;          // +0x78: Audio::Scene*
    // +0x80 / +0x88 were SWAPPED-AND-MISNAMED until 2026-08-30. Verified against
    // retail GameUpdateLoop::Load (0x140096B00) instruction-by-instruction; the
    // plate comment on that function is ALSO wrong about +0x88 ("SkyMesh Transform").
    void*    m_musicPlayer;         // +0x80: MusicPlayer* — MemAlloc(0x18,4), seeded
                                    //   {+0x00 int32 currentTrackId = -1,
                                    //    +0x08 MusicTrackList* = Find("Game/MusicTracks.rsc"),
                                    //    +0x10 int64 0}.
                                    //   Ticked by 0x1400a40e0 (MusicPlayer::Update — NOT an
                                    //   "ambient sound" update): early-outs on
                                    //   Audio::Scene::IsEffectPlaying(+0x78, currentTrackId),
                                    //   then picks a track whose [monthLo..monthHi] and
                                    //   [moodLo..moodHi] ranges contain the current month
                                    //   (from m_dayCounter +0x128) and the town mood averaged
                                    //   over m_citizenList (+0xF0) +0xA0/+0xA8.
    void*    m_characterGrid;       // +0x88: loose Morton (Z-order) quadtree over characters.
                                    //   MemAlloc(0x60,8); ctor 0x1400182b0(self, DAT_140172258)
                                    //   (0x140092880 is the default-arg wrapper). 8 levels;
                                    //   level base offsets {0,1,5,0x15,0x55,0x155,0x555,0x1555}
                                    //   in the 8-int array at +0x40; 0x5555 (21845) cells in the
                                    //   int32 array at +0x30 (seeded -1 = empty cell head) and a
                                    //   parallel count array at +0x50 (seeded 0). Entity records
                                    //   live at +0x10, stride 0x18, with the owning cell index at
                                    //   record +0x10. 0x140018660 re-buckets one entity by its
                                    //   bounds (level from size, Morton code from position).
                                    //   NOT a MusicPlayer and NOT a Transform. Raw-offset users
                                    //   in the tree call it charMgr / gridPtr / bridgeMgr:
                                    //   charactercomponent.cpp is the accurate one.
    void*    m_renderer;            // +0x90: Graphics::Renderer*
    void*    m_userInterface;       // +0x98: UI::UserInterface*

    // Resource references (looked up in Load from ExternalList)
    void*    m_helpTopics;          // +0xA0: "Game/Help/HelpTopics.rsc"
    void*    m_diseaseList;         // +0xA8: "Game/Disease/Diseases.rsc"
    void*    m_professionList;      // +0xB0: "Game/Profession/Profession.rsc"
    void*    m_uiMaterial;          // +0xB8: "Material/UI/UIMaterialNoBlend.rsc"

    // Input state
    uint8_t  m_quickSavePressed;    // +0xC0: set on KeyMap::Pressed(0x21)
    uint8_t  m_quickLoadPressed;    // +0xC1: set on KeyMap::Pressed(0x22)
    uint8_t  _padC2[6];            // +0xC2: alignment padding

    // Loop transition
    void*    m_nextLoop;            // +0xC8: UpdateLoop* — pending loop transition

    // Game flags
    uint32_t m_gameFlags;           // +0xD0: 0xF0 (loaded) or 0x00 (new game)
    uint32_t _padD4;               // +0xD4: alignment padding

    // Game systems (created in Load, destroyed in Shutdown)
    void*    m_random;              // +0xD8: System::Random* (0x9C4 bytes)
    void*    m_playerController;    // +0xE0: PlayerController* (0x10C bytes, camera+frustum)
    void*    m_entityList;          // +0xE8: EntityList* (0xE8 bytes, all ComponentEntity instances)
    void*    m_citizenList;         // +0xF0: CitizenBehavior* (0x1C0 bytes, population manager)
    void*    m_game;                // +0xF8: Game* (0xF8 bytes, timed events at +0x78)

    // Scene geometry objects (added to Graphics::Scene as globals)
    void*    m_sunLight;            // +0x100: Graphics::SunLight* (0x108 bytes)
    void*    m_skyMesh;             // +0x108: Graphics::MeshInstance* (sky, 0x120 bytes)
    void*    m_worldGeometry;       // +0x110: Graphics::Geometry* (terrain, 0x230 bytes)

    // Game managers
    void*    m_tutorial;            // +0x118: Tutorial* (0x138 bytes, frame count at +0x08, time at +0x130)
    void*    m_entityUI;            // +0x120: EntityUI/PlayerToolManager* (0xB0 bytes, +0x30=paused, +0x34=speed)
    void*    m_dayCounter;          // +0x128: DayCounter* (0x18 bytes: {day, accumulated, dayLength})
    void*    m_weather;             // +0x130: Weather* (0x78 bytes)
    void*    m_eventNotifier;       // +0x138: EventNotifier* (0x28 bytes, string table ptrs)
    void*    m_saveLoad;            // +0x140: Save::SaveLoad* (0x90 bytes)
    void*    m_imageCapture;        // +0x148: ImageCapture* (0x38 bytes, screenshot/thumbnail)

    // Scenario/achievement
    void*    m_scenarioInstance;    // +0x150: InternalBasePtr to AchievementInstance
    uint8_t  m_scenarioCompleted;   // +0x158: set to 1 when scenario met/failed
    uint8_t  _pad159[3];           // +0x159: alignment padding
    int32_t  m_scenarioIndex;       // +0x15C: index into scenario list (-1 = none)
};

static_assert(sizeof(GameUpdateLoop) == 0x160, "GameUpdateLoop must be 0x160 bytes");
static_assert(offsetof(GameUpdateLoop, m_saveName) == 0x28, "m_saveName at +0x28");
static_assert(offsetof(GameUpdateLoop, m_mapSizeIndex) == 0x30, "m_mapSizeIndex at +0x30");
static_assert(offsetof(GameUpdateLoop, m_gameTime) == 0x38, "m_gameTime at +0x38");
static_assert(offsetof(GameUpdateLoop, m_savedState) == 0x58, "m_savedState at +0x58");
static_assert(offsetof(GameUpdateLoop, m_graphicsScene) == 0x60, "m_graphicsScene at +0x60");
static_assert(offsetof(GameUpdateLoop, m_audioScene) == 0x78, "m_audioScene at +0x78");
static_assert(offsetof(GameUpdateLoop, m_musicPlayer) == 0x80, "m_musicPlayer at +0x80");
static_assert(offsetof(GameUpdateLoop, m_characterGrid) == 0x88, "m_characterGrid at +0x88");
static_assert(offsetof(GameUpdateLoop, m_renderer) == 0x90, "m_renderer at +0x90");
static_assert(offsetof(GameUpdateLoop, m_userInterface) == 0x98, "m_userInterface at +0x98");
static_assert(offsetof(GameUpdateLoop, m_quickSavePressed) == 0xC0, "m_quickSavePressed at +0xC0");
static_assert(offsetof(GameUpdateLoop, m_nextLoop) == 0xC8, "m_nextLoop at +0xC8");
static_assert(offsetof(GameUpdateLoop, m_gameFlags) == 0xD0, "m_gameFlags at +0xD0");
static_assert(offsetof(GameUpdateLoop, m_random) == 0xD8, "m_random at +0xD8");
static_assert(offsetof(GameUpdateLoop, m_entityList) == 0xE8, "m_entityList at +0xE8");
static_assert(offsetof(GameUpdateLoop, m_game) == 0xF8, "m_game at +0xF8");
static_assert(offsetof(GameUpdateLoop, m_sunLight) == 0x100, "m_sunLight at +0x100");
static_assert(offsetof(GameUpdateLoop, m_worldGeometry) == 0x110, "m_worldGeometry at +0x110");
static_assert(offsetof(GameUpdateLoop, m_tutorial) == 0x118, "m_tutorial at +0x118");
static_assert(offsetof(GameUpdateLoop, m_entityUI) == 0x120, "m_entityUI at +0x120");
static_assert(offsetof(GameUpdateLoop, m_weather) == 0x130, "m_weather at +0x130");
static_assert(offsetof(GameUpdateLoop, m_saveLoad) == 0x140, "m_saveLoad at +0x140");
static_assert(offsetof(GameUpdateLoop, m_scenarioInstance) == 0x150, "m_scenarioInstance at +0x150");
static_assert(offsetof(GameUpdateLoop, m_scenarioCompleted) == 0x158, "m_scenarioCompleted at +0x158");
static_assert(offsetof(GameUpdateLoop, m_scenarioIndex) == 0x15C, "m_scenarioIndex at +0x15C");
