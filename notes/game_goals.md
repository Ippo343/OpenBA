# Game Module — Reimplementation Goals

**Directory:** `code/Game/Game/`
**Source files:** `gameupdateloop.cpp`, `weather.cpp`, `weather_funcs.h`

---

## Overview

Core game systems: main update loop, map, pathfinding, state machine, weather, scenario, world generation, professions, names, music, tornado, workforce.

---

## Classes

| Class | Key Address | Reimpl Status | Priority |
|-------|------------|---------------|----------|
| GameUpdateLoop | Update 0x140096380 | **REIMPL LIVE** | Done (Update), High (others) |
| Weather | Vtable 0x140160290 | **ALL SLOTS REIMPL** (Dtor, Serialize, Cleanup) | Done |
| StateMachine | Tick 0x1400a9660 | **REIMPL LIVE** (`statemachine.cpp`) | Done |
| Game | Various | Not started | High |
| Map | Various | Not started | Medium |
| Pathfinder | FindPath 0x1400bf790 | Not started | High |
| Scenario | Vtable 0x14015f020 | **ALL SLOTS REIMPL** (`scenario.cpp`) | Done |
| MapGenerator | Vtable 0x14015ed80 | **ALL SLOTS REIMPL** (`mapgenerator.cpp`) | Done |
| ClimateGenerator | Vtable 0x14015e140 | **ALL SLOTS REIMPL** (`climategenerator.cpp`) | Done |
| TerrainGenerator | Vtable 0x14015f170 | **ALL SLOTS REIMPL** (`terraingenerator.cpp`) | Done |
| MenuScene | Vtable 0x14015ede0 | **REIMPL** (Dtor/Cleanup/Slot8, Serialize delegates) | Done |
| NameDatabase | - | Not started | Low |
| Profession/ProfessionList | Vtable 0x14015efa0/0x14015ef50 | **ALL SLOTS REIMPL** | Done |
| Toolbar | Vtable 0x140160120 | **ALL SLOTS REIMPL** (Slot8 unhookable but implemented) | Done |

---

### Summary
- **Update**: REIMPL LIVE — main simulation tick working
- **Draw**: Passthrough (crashes in VideoDX11)
- **Startup**: Passthrough (crashes on ExternalList::Find)
- **Shutdown**: Passthrough (waiting on Startup)
- **Load**: Passthrough (759 instructions, large)
- **SaveRestore**: Unhookable (Bug Class 3)

### Open Issues
- [ ] AchievementManager::Update disabled (null vtable in FinishTutorialInstance)
- [ ] EntityList tree node iteration skipped
- [ ] Sun direction not passed to Scene::Clean
- [ ] Quicksave/Quickload keys disabled (KeyMap null deref)

---

## Key Game Functions

| Function | Address | Status | Notes |
|----------|---------|--------|-------|
| StateMachine_Tick | 0x1400a9660 | **REIMPL LIVE** | Full reimpl in `statemachine.cpp`, drives all citizen AI |
| Game::SpawnTornado | named | Not started | |
| Game::InfectRandomCitizen | named | Not started | |
| Game::StartRandomFire | named | Not started | |
| Game::DestroyAllRawMaterials | named | Not started | |

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
