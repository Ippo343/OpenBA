# Game Module — Reimplementation Goals

**Directory:** `code/Game/Game/`
**Source files:** `gameupdateloop.cpp`, `weather.cpp`, `weather_funcs.h`, `statemachine.cpp`, `scenario.cpp`, `mapgenerator.cpp`, `climategenerator.cpp`, `terraingenerator.cpp`, `menuscene.cpp`, `profession.cpp`, `professionlist.cpp`, `toolbar.cpp`

---

## Overview

Core game systems: main update loop, map, pathfinding, state machine, weather, scenario, world generation, professions, names, music, tornado, workforce.

---

## Classes

| Class | Key Address | Reimpl Status | Priority |
|-------|------------|---------------|----------|
| GameUpdateLoop | Vtable 0x14015e2f0 | **7/8 SLOTS REIMPL** (Update, Draw, Startup, Shutdown, Destroy, OnScenarioComplete + Load passthrough) | Done (see below) |
| Weather | Vtable 0x140160290 | **ALL SLOTS REIMPL** (Dtor, Serialize, Cleanup) | Done |
| StateMachine | Tick 0x1400a9660 | **REIMPL LIVE** (`statemachine.cpp`) | Done |
| Game | Various | Deferred (standalone functions, not hooked) | Medium |
| Map | Various | Not started | Low (defer) |
| Pathfinder | FindPath 0x1400bf790 | Not started | Low (defer) |
| Scenario | Vtable 0x14015f020 | **ALL SLOTS REIMPL** (`scenario.cpp`) | Done |
| MapGenerator | Vtable 0x14015ed80 | **ALL SLOTS REIMPL** (`mapgenerator.cpp`) | Done |
| ClimateGenerator | Vtable 0x14015e140 | **ALL SLOTS REIMPL** (`climategenerator.cpp`) | Done |
| TerrainGenerator | Vtable 0x14015f170 | **ALL SLOTS REIMPL** (`terraingenerator.cpp`) | Done |
| MenuScene | Vtable 0x14015ede0 | **REIMPL** (Dtor/Cleanup/Slot8, Serialize delegates) | Done |
| NameDatabase | - | Not started | Low (defer) |
| Profession/ProfessionList | Vtable 0x14015efa0/0x14015ef50 | **ALL SLOTS REIMPL** | Done |
| Toolbar | Vtable 0x140160120 | **ALL SLOTS REIMPL** (Slot8 unhookable but implemented) | Done |

---

### GameUpdateLoop Vtable Slot Status

| Slot | Function | Address | Status |
|------|----------|---------|--------|
| +0x00 | Destroy | 0x140019DD0 | **REIMPL** — scalar deleting dtor, clean code in gameupdateloop.cpp |
| +0x08 | Startup | 0x140097C00 | **REIMPL** — dialog creation + UI init, ExternalList::Find dispatch slot fallback |
| +0x10 | Shutdown | 0x1400977C0 | **REIMPL** — teardown all subsystems, paired with Startup |
| +0x18 | Update | 0x140096380 | **REIMPL LIVE** — main simulation tick (vtable-patched, Bug Class 3) |
| +0x20 | Draw | 0x140096010 | **REIMPL** — render frame, fixed Transform3::AsMatrix hidden retval ABI (vtable-patched) |
| +0x28 | DrawFailed | 0x140096000 | 3 bytes, noop (too small to hook, not worth reimpl) |
| +0x30 | Load | 0x140096B00 | **PASSTHROUGH** — 759 instructions, creates all subsystems. Large but mechanical. |
| +0x38 | SaveRestore | 0x1400959C0 | **UNHOOKABLE** (Bug Class 3 — MOV RAX,RSP). Needs fresh reimpl for standalone exe. |

### Bugs Fixed (2026-03-29)
- **Draw crash**: `fn_Transform3_AsMatrix` was missing hidden return pointer (non-trivial return ABI). Fixed: typedef now `(self, retval)`, output buffer allocated on stack.
- **Quicksave/Quickload keys not working**: Over-cautious null guards in triple-deref chain caused skipping. Matched proven working pattern from dragtool/pathtool.
- **Startup ExternalList::Find null fptr**: Added dispatch slot fallback (0x140152160) for when ResolveExport fails on the long mangled name.

### Open Issues (all resolved)
- [x] AchievementManager::Update — RESOLVED (call is live, deref chain correct, no crash)
- [x] EntityList tree node iteration — RESOLVED (custom 5-bucket approach handles same entities)
- [x] Sun direction not passed to Scene::Clean — WON'T FIX (dead code in original binary, confirmed via disasm)
- [x] Quicksave/Quickload keys — FIXED (see bugs fixed above)

---

## Key Game Functions

| Function | Address | Status | Notes |
|----------|---------|--------|-------|
| StateMachine_Tick | 0x1400a9660 | **REIMPL LIVE** | Full reimpl in `statemachine.cpp`, drives all citizen AI |
| Game::SpawnTornado | 0x14005fd20 | Decompiled, deferred | Needs ExternalFactory::FindOrLoad, ComponentEntity ctor, EntityList::AddEntity |
| Game::InfectRandomCitizen | 0x14005fe10 | Decompiled, deferred | Needs EntityList citizen array, DiseaseComponent_Infect |
| Game::StartRandomFire | 0x14005feb0 | Decompiled, deferred | Needs EntityList building array, FireComponent_Ignite |
| Game::DestroyAllRawMaterials | 0x140060000 | Decompiled, deferred | Iterates entities, finds RawMaterialComponent, calls Game_DestroyEntity |
| Game::UpdateTimedEvents | 0x1400a5d10 | Decompiled, deferred | Circular buffer priority queue, complex data structure |

**Note**: All Game:: functions above are standalone (not hooked). They work via existing function pointers in engine_funcs.h. Reimplementation deferred to Component module phase when EntityList internals and component sub-functions will be typed.

## Profile Source Paths

- `Game\Game.cpp`, `Game\Map.cpp`, `Game\Pathfinder.cpp`, `Game\StateMachine.cpp`
- `Game\Weather.cpp`, `Game\Terrain.cpp`, `Game\Environment.cpp`
- `Game\MapGenerator.cpp`, `Game\MenuScene.cpp`, `Game\NameDatabase.cpp`
- `Game\OverheadMap.cpp`, `Game\PathDistanceCache.cpp`, `Game\PathFollower.cpp`
- `Game\Workforce.cpp`, `Game\Area.cpp`, `Game\GameConfiguration.cpp`
- `Game\GameDebug.cpp`, `Game\GameLoaderLoop.cpp`, `Game\MainMenuLoop.cpp`
- `Game\Tornado.cpp`, `Game\MusicPlayer.cpp`, `Game\Profession.cpp`
- `Game\Scenario.cpp`, `Game\TerrainGeneratorInstance.cpp`, `Game\Globals.cpp`

## Runtime Dependencies

| Function | Source | Notes |
|----------|--------|-------|
| UI::PausesGame | Runtime | Check if UI blocks simulation |
| Graphics::Scene::Clean | Runtime | End-of-frame cleanup |
| Audio::Scene::Update | Runtime | Audio tick |
| Camera transforms | Runtime | CalcTransform, SetCameraToWorld |
| SaveLoad::Update | Runtime | Save system tick |
| UpdateLoop::~UpdateLoop | Runtime 0x1800DDDB0 | Base class destructor |
| Transform3::AsMatrix | Runtime | Non-trivial return (hidden retval ptr) |
| ExternalList::Find | Runtime dispatch 0x140152160 | Non-trivial return (hidden retval ptr) |
