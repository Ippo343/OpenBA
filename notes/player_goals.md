# Player Module — Reimplementation Status

**Directory:** `code/Game/Player/`
**Type headers:** `engine/types/` — `Tool.h`, `BridgeTool.h`, `DragPlacementTool.h`, `EditTool.h`, `FencedTool.h`, `PathTool.h`, `PlacementTool.h`, `RemovalTool.h`, `RoadTool.h`, `RotationSetting.h`

---

## Summary

All 9 tool classes now have clean C++ source files in `code/Game/Player/`. 16 vtable slots reimplemented, 2 unhookable slots written for standalone exe. ~17 slots remain as trampolines (complex rendering/entity-creation engine API deps).

---

## Per-Class Status

### DragTool (abstract base, 0x38, vtable 0x140161c10)
| Slot | Method | Address | Status |
|------|--------|---------|--------|
| 0 | Dtor | 0x14002da20 | **REIMPL** |
| 6 | OnKey | 0x1400fc6b0 | **REIMPL** (full state machine) |

### BridgeTool (0x40, vtable 0x140161b00, extends DragTool)
| Slot | Method | Address | Status |
|------|--------|---------|--------|
| 0 | Dtor | 0x14002d7f0 | **REIMPL** |
| 4 | Startup | 0x1400fb7a0 | **REIMPL** |
| 13 | CalcPath | 0x1400f9cd0 | Trampoline (1066 instr, entity creation) |
| 14 | CalcCost | 0x1400facf0 | **REIMPL** |

### PlacementTool (0x70, vtable 0x140161d20, extends Tool)
| Slot | Method | Address | Status |
|------|--------|---------|--------|
| 0 | Dtor | 0x14002dc90 | **REIMPL** |
| 4 | Startup | 0x1400fe580 | Trampoline (MeshInstance, Graphics::Scene) |
| 5 | Shutdown2 | 0x1400fe4e0 | Trampoline |
| 6 | OnKey | 0x1400fe2a0 | Trampoline (rotation, footprint validation) |
| 7 | Serialize | 0x1400fdee0 | Trampoline |
| 8 | Draw | 0x1400fdcc0 | Trampoline |

### DragPlacementTool (0x48, vtable 0x140161b90, extends DragTool)
| Slot | Method | Address | Status |
|------|--------|---------|--------|
| 0 | Dtor | 0x14002d900 | **REIMPL** |
| 4 | Startup | 0x1400fc3e0 | **REIMPL** |
| 8 | Draw | 0x1400fbe40 | Trampoline (terrain rendering) |
| 13 | CalcPath | 0x1400fb900 | Trampoline (entity creation) |
| 14 | CalcCost | 0x1400fbae0 | **REIMPL** |

### EditTool (0x38, vtable 0x140161ca0, extends Tool)
| Slot | Method | Address | Status |
|------|--------|---------|--------|
| 0 | Dtor | 0x14002db00 | **REIMPL** |
| 6 | OnKey | 0x1400fd030 | Trampoline (picking, camera raycast) |
| 7 | Startup | 0x1400fcda0 | Trampoline |
| 10 | Serialize | 0x1400fd290 | **REIMPL** |

### FencedTool (0x70, vtable 0x1401605d0, extends DragTool)
| Slot | Method | Address | Status |
|------|--------|---------|--------|
| 0 | Dtor | 0x140026930 | **REIMPL** |
| 4 | Startup | 0x1400dcd50 | Trampoline (5 component lookups + mesh) |
| 5 | Shutdown2 | 0x1400dccc0 | **REIMPL** |
| 6 | OnKey | 0x1400dcbe0 | **REIMPL** (wraps DragTool::OnKey + rotation) |
| 7 | Serialize | 0x1400dcad0 | Trampoline |
| 8 | Draw | 0x1400dc440 | Trampoline |
| 9 | Shutdown | 0x1400dcd10 | **REIMPL** |
| 13 | CalcPath | 0x1400dae00 | Trampoline |
| 14 | CalcCost | 0x1400db490 | Trampoline |

### PathTool (0x60, vtable 0x140160680, extends Tool)
| Slot | Method | Address | Status |
|------|--------|---------|--------|
| 0 | Dtor | 0x140026b70 | **REIMPL** |
| 6 | OnKey | 0x1400de6b0 | **REIMPL** (terrain raycast, entity select) |
| 8 | Draw | 0x1400dd3b0 | Trampoline (dynamic array render entries) |
| 10 | Serialize | 0x1400de8d0 | **REIMPL** |

### RemovalTool (0x40, vtable 0x140161de0, extends DragTool)
| Slot | Method | Address | Status |
|------|--------|---------|--------|
| 0 | Dtor | 0x14002e7f0 | **REIMPL** |
| 4 | Startup | 0x140104bd0 | **REIMPL** |
| 8 | Draw | 0x140104050 | Trampoline (entity eligibility checks) |
| 13 | CalcPath | 0x1401035e0 | Trampoline (entity creation, BST traversal) |
| 14 | CalcCost | 0x140103ef0 | **REIMPL** |

### RoadTool (0x48, vtable 0x140161e70, extends DragTool)
| Slot | Method | Address | Status |
|------|--------|---------|--------|
| 0 | Dtor | 0x14002e8c0 | **REIMPL** |
| 4 | Startup | 0x1401056e0 | **REIMPL** |
| 6 | OnKey | 0x140105690 | Clean code (UNHOOKABLE — CALL rel32) |
| 8 | Draw | 0x140105190 | Trampoline |
| 13 | CalcPath | 0x140104d30 | Documented (UNHOOKABLE — needs A* helper) |
| 14 | CalcCost | 0x140104fe0 | Clean code (UNHOOKABLE — partial) |

---

## Remaining Work (trampolines to convert)

The remaining ~17 trampoline slots are blocked on untyped engine APIs:
- **Draw functions** (7 slots) — Graphics::Scene, MeshInstance, Transform, Geometry rendering
- **CalcPath functions** (4 slots) — ComponentEntity_CopyCtor, entity registration, transform math
- **PlacementTool OnKey/Startup/Shutdown/Serialize** (4 slots) — rotation, MeshInstance, footprint
- **EditTool OnKey/Startup** (2 slots) — picking scene intersection, camera raycasting

These will unblock as more Runtime DLL rendering APIs are typed and resolved.

---

## Profile Source Paths

- `Player\PlacementTool.cpp`, `Player\Player.cpp`
- `Player\BridgeTool.cpp`, `Player\DragPlacementTool.cpp`, `Player\EditTool.cpp`
- `Player\FencedTool.cpp`, `Player\RemovalTool.cpp`

## Key Engine Dependencies

| Function | Source | Notes |
|----------|--------|-------|
| Map tile queries | Exe (Game module) | Tile occupancy, terrain type |
| Pathfinder | Exe (Game module) | Road/path validation |
| Input::Device | Runtime | Mouse position, click state |
| Graphics::Scene | Runtime | Preview rendering (blocks Draw slots) |
| MeshInstance | Runtime | Mesh allocation/setup (blocks Startup/Draw) |
| Math::Transform | Runtime | Entity positioning (blocks CalcPath) |
| ComponentEntity_CopyCtor | Exe | Entity template cloning (blocks CalcPath) |
