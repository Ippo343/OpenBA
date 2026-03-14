# Player Module — Reimplementation Goals

**Directory:** `code/Game/Player/`
**Type headers:** `BridgeTool.h`, `DragPlacementTool.h`, `EditTool.h`, `FencedTool.h`, `PathTool.h`, `PlacementTool.h` (base), `RemovalTool.h`, `RoadTool.h`, `RotationSetting.h`, `Tool.h`
**Source files:** None yet (empty directory)

---

## Overview

Placement tools handle building/road/zone placement by the player. Each tool subclasses PlacementTool which handles mouse interaction, preview rendering, and entity creation on confirm. Tool classes are relatively self-contained — they depend on Map, Pathfinder, and Component queries but don't have complex cross-dependencies.

---

## Classes

| Class | Vtable | Type Header | Reimpl Status | Priority |
|-------|--------|-------------|---------------|----------|
| Tool (base) | 0x140161f10 / 0x140161c10 | Tool.h | Not started | - |
| PlacementTool | - | PlacementTool.h | Not started | Medium |
| BridgeTool | 0x140161b00 | BridgeTool.h | Not started | Low |
| DragPlacementTool | 0x140161b90 | DragPlacementTool.h | Not started | Medium |
| EditTool | 0x140161ca0 | EditTool.h | Not started | Medium |
| FencedTool | 0x1401605d0 | FencedTool.h | Not started | Low |
| PathTool | 0x140160680 | PathTool.h | Not started | Low |
| RemovalTool | 0x140161de0 | RemovalTool.h | Not started | Medium |
| RoadTool | 0x140161e70 | RoadTool.h | Not started | Low |
| RotationSetting | 0x140160710 | RotationSetting.h | Not started | Low |

---

## Profile Source Paths

- `Player\PlacementTool.cpp`, `Player\Player.cpp`
- `Player\BridgeTool.cpp`, `Player\DragPlacementTool.cpp`, `Player\EditTool.cpp`
- `Player\FencedTool.cpp`, `Player\RemovalTool.cpp`

## Runtime Dependencies

| Function | Source | Notes |
|----------|--------|-------|
| Map tile queries | Exe (Game module) | Tile occupancy, terrain type |
| Pathfinder | Exe (Game module) | Road/path validation |
| Input::Device | Runtime | Mouse position, click state |
| Graphics::Scene | Runtime | Preview rendering |
