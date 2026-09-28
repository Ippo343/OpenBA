#pragma once
// ============================================================================
// code/Game/Game/GameUpdateLoop.h — ENDGAME B.3.8 block LOOP (B38_RULES D8)
//
// The exe's in-game simulation loop, a real `Engine::UpdateLoop` subclass.
// Unreflected. It owns every game subsystem: graphics scene, audio, renderer,
// UI, entities, citizens, pathfinder, weather, tutorial, save/load, camera
// and the scenario/achievement instance. g_pGameWorldSingleton
// (DAT_1401c3dd8) points at the live instance; the tree's many
// `gameWorld + 0x…` reads are fields of this class.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md sec. 6, B37_RULES sec. 3.1):
//   table 0x14015e2f0, 12 slots, sizeof 0x160 (MemAlloc(0x160, 8))
//   [0]  0x140019dd0  scalar deleting dtor: restamp, g_pGameWorldSingleton = 0,
//                     ~String(+0x28), ~UpdateLoop, MemFree on &1
//   [1]  0x140097c00  Startup     [2]  0x1400977c0  Shutdown
//   [3]  0x140096380  Update      [4]  0x140096010  Draw
//   [5]  0x140096000  DrawFailed: a bare `RET 0`
//   [6]  0x140096b00  Load (the world build; LOADER THREAD)
//   [7]  0x1400959c0  SaveRestore(save, SavedState*, int* out) — 4 arguments
//   [8]  0x140095970  OnVideoError     [9]  0x140095930  OnVideoRestart
//   [10] 0x140019d40  GetResourceFilename: "Game/Game"
//   [11] IMPORT ?AllowsModChanges@UpdateLoop@Engine@@ (inherited: false)
//   ctor 0x140098060(this, savedState, &ibp, scenarioIdx, name, p6, p7, p8,
//   p9, p10, p11) — see the ctor below.
//
// Fields keep their exe-side `m_` names (the bodies in gameupdateloop*.cpp
// read them); the UpdateLoop base's are Engine::UpdateLoop's.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "engine/UpdateLoop.h"   // -Iruntime/types: Engine::UpdateLoop
#include "system/String.h"       // -Iruntime/types: System::String (by value at +0x28)

class GameUpdateLoop final : public Engine::UpdateLoop {
public:
    // 0x140098060. `scenarioIbp` is the InternalBasePtr's 8 bytes (retail copies
    // `*param_3`); `disasters` is retail's char p8: +0xD0 = p8 ? 0 : 0xF0.
    GameUpdateLoop(void* savedState, uint64_t scenarioIbp, int32_t scenarioIndex,
                   const System::String& saveName, int32_t seed,
                   int32_t mapSize, bool disasters, void* mapConfig,
                   void* terrainConfig, void* climateConfig);

    // [0] 0x140019dd0's body: g_pGameWorldSingleton = 0; then ~String(+0x28) and
    // ~UpdateLoop are the compiler's member/base chain, in retail's order.
    ~GameUpdateLoop() override;                                  // [0] 0x140019dd0
    void Startup() override;                                     // [1] 0x140097c00
    void Shutdown() override;                                    // [2] 0x1400977c0
    bool Update(const System::Time& time) override;              // [3] 0x140096380
    void Draw(float interpolation) override;                     // [4] 0x140096010
    void DrawFailed() override {}                                // [5] 0x140096000 `RET 0`
    void Load() override;                                        // [6] 0x140096b00
    // [7] 0x1400959c0 takes a 4th argument (int* out) that only the LOAD path
    // passes, by a DIRECT call (GameUpdateLoop::Load). Every virtual caller
    // (Save::SaveLoad::UpdateSave) passes a dead R9, which the save path never
    // reads; the override hands SaveRestoreOut nullptr for it.
    void SaveRestore(bool saving, Save::SavedState* state) override;
    // 0x1400959c0's body with retail's 4th argument (a distinct name, not an
    // overload of the virtual, B5-17): Load and the roundtrip check call it.
    void SaveRestoreOut(bool saving, Save::SavedState* state, int* out);
    void OnVideoError() override;                                // [8] 0x140095970
    void OnVideoRestart() override;                              // [9] 0x140095930
    const char* GetResourceFilename() override;                  // [10] 0x140019d40 "Game/Game"
    // [11] AllowsModChanges: inherited (Engine::UpdateLoop's, false).

    void OnScenarioComplete();                                   // 0x140096830

    static const void* Vtable();

    // The exe cell 0x1401c3dd8 (B3R STORAGE): the live loop. Load stores `this`
    // (0x140096b35); both destructors clear it (0x140098030, 0x140019df7).
    // Null outside a game. Defined in gameupdateloop.cpp.
    static GameUpdateLoop* _singleton;

public:
    // Construction parameters (stored by the ctor, used by Load)
    System::String m_saveName;      // +0x28  save file name
    int32_t  m_seed;                // +0x30  map seed (seeds the Random at +0xD8; newgamedialog writes it)
    int32_t  m_mapSize;             // +0x34  map size index (the terrain size-table row)
    int64_t  m_gameTime;            // +0x38  accumulated game time (Time)
    void*    m_mapConfig;           // +0x40  map generation config resource
    void*    m_terrainConfig;       // +0x48  terrain generation config resource
    void*    m_climateConfig;       // +0x50  climate generation config resource
    void*    m_savedState;          // +0x58  SavedState* (non-null = loading from save)

    // Rendering subsystems (created in Load, destroyed in Shutdown)
    void*    m_graphicsScene;       // +0x60  Graphics::Scene*
    void*    m_animationManager;    // +0x68  Graphics::AnimationManager*
    void*    m_pickingScene;        // +0x70  Picking::Scene*
    void*    m_audioScene;          // +0x78  Audio::Scene*
    // +0x80: MusicPlayer* — MemAlloc(0x18,4) {+0x00 int32 currentTrackId = -1,
    //   +0x08 MusicTrackList* = Find("Game/MusicTracks.rsc"), +0x10 int64 0};
    //   ticked by 0x1400a40e0 (MusicPlayer::Update).
    void*    m_musicPlayer;         // +0x80
    // +0x88: loose Morton (Z-order) quadtree over characters, MemAlloc(0x60,8),
    //   ctor 0x1400182b0 (8 levels, 0x5555 cells). NOT a MusicPlayer and NOT a
    //   Transform (the +0x80/+0x88 names were swapped until 2026-08-30).
    void*    m_characterGrid;       // +0x88
    void*    m_renderer;            // +0x90  Graphics::Renderer*
    void*    m_userInterface;       // +0x98  UI::UserInterface*

    // Resource references (looked up in Load from the ExternalList)
    void*    m_helpTopics;          // +0xA0  "Game/Help/HelpTopics.rsc"
    void*    m_diseaseList;         // +0xA8  "Game/Disease/Diseases.rsc"
    void*    m_professionList;      // +0xB0  "Game/Profession/Profession.rsc"
    void*    m_uiMaterial;          // +0xB8  "Material/UI/UIMaterialNoBlend.rsc"

    // Input state
    uint8_t  m_quickSavePressed;    // +0xC0  KeyMap::Pressed(0x21)
    uint8_t  m_quickLoadPressed;    // +0xC1  KeyMap::Pressed(0x22)
    uint8_t  _padC2[6];             // +0xC2

    void*    m_nextLoop;            // +0xC8  UpdateLoop* — pending loop transition

    uint32_t m_gameFlags;           // +0xD0  0xF0 or 0x00 (ctor's p8)
    uint32_t _padD4;                // +0xD4

    // Game systems (created in Load, destroyed in Shutdown)
    void*    m_random;              // +0xD8  System::Random* (0x9C4 bytes)
    void*    m_playerController;    // +0xE0  PlayerController* (0x10C bytes, camera+frustum)
    void*    m_entityList;          // +0xE8  EntityList* (0xE8 bytes)
    void*    m_citizenList;         // +0xF0  CitizenBehavior* (0x1C0 bytes)
    void*    m_game;                // +0xF8  Game* (0xF8 bytes, timed events at +0x78)

    // Scene geometry objects (added to Graphics::Scene as globals)
    void*    m_sunLight;            // +0x100 Graphics::SunLight* (0x108 bytes)
    void*    m_skyMesh;             // +0x108 Graphics::MeshInstance* (sky, 0x120 bytes)
    void*    m_worldGeometry;       // +0x110 Graphics::Geometry* (terrain, 0x230 bytes)

    // Game managers
    void*    m_tutorial;            // +0x118 Tutorial* (0x138 bytes)
    void*    m_entityUI;            // +0x120 EntityUI/PlayerToolManager* (0xB0 bytes)
    void*    m_dayCounter;          // +0x128 DayCounter* (0x18 bytes: {day, accumulated, dayLength})
    void*    m_weather;             // +0x130 Weather* (0x78 bytes)
    void*    m_eventNotifier;       // +0x138 EventNotifier* (0x28 bytes)
    void*    m_saveLoad;            // +0x140 Save::SaveLoad* (0x90 bytes)
    void*    m_imageCapture;        // +0x148 ImageCapture* (0x38 bytes)

    // Scenario/achievement
    void*    m_scenarioInstance;    // +0x150 InternalBasePtr to AchievementInstance
    uint8_t  m_scenarioCompleted;   // +0x158 set to 1 when scenario met/failed
    uint8_t  _pad159[3];            // +0x159
    int32_t  m_scenarioIndex;       // +0x15C index into scenario list (-1 = none)
};

static_assert(sizeof(GameUpdateLoop) == 0x160, "GameUpdateLoop is 0x160 (MemAlloc(0x160, 8))");
static_assert(alignof(GameUpdateLoop) == 8, "qword-aligned (the vptr)");
static_assert(offsetof(GameUpdateLoop, m_saveName) == 0x28, "m_saveName at +0x28");
static_assert(offsetof(GameUpdateLoop, m_seed) == 0x30, "m_seed at +0x30");
static_assert(offsetof(GameUpdateLoop, m_mapSize) == 0x34, "m_mapSize at +0x34");
static_assert(offsetof(GameUpdateLoop, m_gameTime) == 0x38, "m_gameTime at +0x38");
static_assert(offsetof(GameUpdateLoop, m_mapConfig) == 0x40, "m_mapConfig at +0x40");
static_assert(offsetof(GameUpdateLoop, m_climateConfig) == 0x50, "m_climateConfig at +0x50");
static_assert(offsetof(GameUpdateLoop, m_savedState) == 0x58, "m_savedState at +0x58");
static_assert(offsetof(GameUpdateLoop, m_graphicsScene) == 0x60, "m_graphicsScene at +0x60");
static_assert(offsetof(GameUpdateLoop, m_animationManager) == 0x68, "m_animationManager at +0x68");
static_assert(offsetof(GameUpdateLoop, m_pickingScene) == 0x70, "m_pickingScene at +0x70");
static_assert(offsetof(GameUpdateLoop, m_audioScene) == 0x78, "m_audioScene at +0x78");
static_assert(offsetof(GameUpdateLoop, m_musicPlayer) == 0x80, "m_musicPlayer at +0x80");
static_assert(offsetof(GameUpdateLoop, m_characterGrid) == 0x88, "m_characterGrid at +0x88");
static_assert(offsetof(GameUpdateLoop, m_renderer) == 0x90, "m_renderer at +0x90");
static_assert(offsetof(GameUpdateLoop, m_userInterface) == 0x98, "m_userInterface at +0x98");
static_assert(offsetof(GameUpdateLoop, m_helpTopics) == 0xA0, "m_helpTopics at +0xA0");
static_assert(offsetof(GameUpdateLoop, m_uiMaterial) == 0xB8, "m_uiMaterial at +0xB8");
static_assert(offsetof(GameUpdateLoop, m_quickSavePressed) == 0xC0, "m_quickSavePressed at +0xC0");
static_assert(offsetof(GameUpdateLoop, m_quickLoadPressed) == 0xC1, "m_quickLoadPressed at +0xC1");
static_assert(offsetof(GameUpdateLoop, m_nextLoop) == 0xC8, "m_nextLoop at +0xC8");
static_assert(offsetof(GameUpdateLoop, m_gameFlags) == 0xD0, "m_gameFlags at +0xD0");
static_assert(offsetof(GameUpdateLoop, m_random) == 0xD8, "m_random at +0xD8");
static_assert(offsetof(GameUpdateLoop, m_playerController) == 0xE0, "m_playerController at +0xE0");
static_assert(offsetof(GameUpdateLoop, m_entityList) == 0xE8, "m_entityList at +0xE8");
static_assert(offsetof(GameUpdateLoop, m_citizenList) == 0xF0, "m_citizenList at +0xF0");
static_assert(offsetof(GameUpdateLoop, m_game) == 0xF8, "m_game at +0xF8");
static_assert(offsetof(GameUpdateLoop, m_sunLight) == 0x100, "m_sunLight at +0x100");
static_assert(offsetof(GameUpdateLoop, m_skyMesh) == 0x108, "m_skyMesh at +0x108");
static_assert(offsetof(GameUpdateLoop, m_worldGeometry) == 0x110, "m_worldGeometry at +0x110");
static_assert(offsetof(GameUpdateLoop, m_tutorial) == 0x118, "m_tutorial at +0x118");
static_assert(offsetof(GameUpdateLoop, m_entityUI) == 0x120, "m_entityUI at +0x120");
static_assert(offsetof(GameUpdateLoop, m_dayCounter) == 0x128, "m_dayCounter at +0x128");
static_assert(offsetof(GameUpdateLoop, m_weather) == 0x130, "m_weather at +0x130");
static_assert(offsetof(GameUpdateLoop, m_eventNotifier) == 0x138, "m_eventNotifier at +0x138");
static_assert(offsetof(GameUpdateLoop, m_saveLoad) == 0x140, "m_saveLoad at +0x140");
static_assert(offsetof(GameUpdateLoop, m_imageCapture) == 0x148, "m_imageCapture at +0x148");
static_assert(offsetof(GameUpdateLoop, m_scenarioInstance) == 0x150, "m_scenarioInstance at +0x150");
static_assert(offsetof(GameUpdateLoop, m_scenarioCompleted) == 0x158, "m_scenarioCompleted at +0x158");
static_assert(offsetof(GameUpdateLoop, m_scenarioIndex) == 0x15C, "m_scenarioIndex at +0x15C");
