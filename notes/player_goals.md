# Player Module — Reimplementation Status

**Directory:** `code/Game/Player/`
**Type headers:** `engine/types/` — `Tool.h`, `BridgeTool.h`, `DragPlacementTool.h`, `EditTool.h`, `FencedTool.h`, `PathTool.h`, `PlacementTool.h`, `RemovalTool.h`, `RoadTool.h`, `RotationSetting.h`

---

## Summary

All 9 tool classes have clean C++ source files in `code/Game/Player/`. 32 vtable slots reimplemented as clean C++ (including 3 CalcPath functions delegating to exe function pointers). 3 unhookable slots written for standalone exe. Only 3 CalcPath slots remain as exe-pointer delegations (documented, awaiting sub-function resolution).

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
| 13 | CalcPath | 0x1400f9cd0 | **EXE DELEGATE** (400+ lines, linked list segments, multi-entity bridge) |
| 14 | CalcCost | 0x1400facf0 | **REIMPL** |

### PlacementTool (0x70, vtable 0x140161d20, extends Tool)
| Slot | Method | Address | Status |
|------|--------|---------|--------|
| 0 | Dtor | 0x14002dc90 | **REIMPL** |
| 4 | Startup | 0x1400fe580 | **REIMPL** (4 component lookups + MeshInstance + random variant) |
| 5 | Shutdown2 | 0x1400fe4e0 | **REIMPL** (scene/mesh/dialog cleanup) |
| 6 | OnKey | 0x1400fe2a0 | **REIMPL** (rotation keys + placement input) |
| 7 | Serialize | 0x1400fdee0 | **REIMPL** (placement preview + cost UI) |
| 8 | Draw | 0x1400fdcc0 | **REIMPL** (delegates to sub_1380 helper) |

### DragPlacementTool (0x48, vtable 0x140161b90, extends DragTool)
| Slot | Method | Address | Status |
|------|--------|---------|--------|
| 0 | Dtor | 0x14002d900 | **REIMPL** |
| 4 | Startup | 0x1400fc3e0 | **REIMPL** |
| 8 | Draw | 0x1400fbe40 | **REIMPL** (grid iteration + terrain validation) |
| 13 | CalcPath | 0x1400fb900 | **REIMPL** (entity creation + world registration) |
| 14 | CalcCost | 0x1400fbae0 | **REIMPL** |

### EditTool (0x38, vtable 0x140161ca0, extends Tool)
| Slot | Method | Address | Status |
|------|--------|---------|--------|
| 0 | Dtor | 0x14002db00 | **REIMPL** |
| 6 | OnKey | 0x1400fd030 | **REIMPL** (camera ray picking + entity selection) |
| 7 | Startup | 0x1400fcda0 | **REIMPL** (dialog positioning near entity) |
| 10 | Serialize | 0x1400fd290 | **REIMPL** |

### FencedTool (0x70, vtable 0x1401605d0, extends DragTool)
| Slot | Method | Address | Status |
|------|--------|---------|--------|
| 0 | Dtor | 0x140026930 | **REIMPL** |
| 4 | Startup | 0x1400dcd50 | **REIMPL** (5 component lookups + MeshInstance) |
| 5 | Shutdown2 | 0x1400dccc0 | **REIMPL** |
| 6 | OnKey | 0x1400dcbe0 | **REIMPL** (wraps DragTool::OnKey + rotation) |
| 7 | Serialize | 0x1400dcad0 | **REIMPL** (fence preview mesh management) |
| 8 | Draw | 0x1400dc440 | **REIMPL** (two-phase: edit mode + preview tiles) |
| 9 | Shutdown | 0x1400dcd10 | **REIMPL** |
| 13 | CalcPath | 0x1400dae00 | **EXE DELEGATE** (two entities + corner/side fence pieces) |
| 14 | CalcCost | 0x1400db490 | **REIMPL** (rectangular region validation) |

### PathTool (0x60, vtable 0x140160680, extends Tool)
| Slot | Method | Address | Status |
|------|--------|---------|--------|
| 0 | Dtor | 0x140026b70 | **REIMPL** |
| 6 | OnKey | 0x1400de6b0 | **REIMPL** (terrain raycast, entity select) |
| 8 | Draw | 0x1400dd3b0 | **REIMPL** (tile markers + line segments) |
| 10 | Serialize | 0x1400de8d0 | **REIMPL** |

### RemovalTool (0x40, vtable 0x140161de0, extends DragTool)
| Slot | Method | Address | Status |
|------|--------|---------|--------|
| 0 | Dtor | 0x14002e7f0 | **REIMPL** |
| 4 | Startup | 0x140104bd0 | **REIMPL** |
| 8 | Draw | 0x140104050 | **REIMPL** (per-tile removability + color coding) |
| 13 | CalcPath | 0x1401035e0 | **EXE DELEGATE** (300 lines, 3 removal types, BST traversal) |
| 14 | CalcCost | 0x140103ef0 | **REIMPL** |

### RoadTool (0x48, vtable 0x140161e70, extends DragTool)
| Slot | Method | Address | Status |
|------|--------|---------|--------|
| 0 | Dtor | 0x14002e8c0 | **REIMPL** |
| 4 | Startup | 0x1401056e0 | **REIMPL** |
| 6 | OnKey | 0x140105690 | Clean code (UNHOOKABLE — CALL rel32) |
| 8 | Draw | 0x140105190 | **REIMPL** (A* pathfinding + per-tile validity) |
| 13 | CalcPath | 0x140104d30 | Documented (UNHOOKABLE — needs A* helper) |
| 14 | CalcCost | 0x140104fe0 | Clean code (UNHOOKABLE — partial) |

---

## Remaining Work (exe-pointer delegations to convert)

3 CalcPath functions delegate to original exe code via resolved function pointers. They are documented with full decompilation analysis but blocked on 15-20 untyped sub-functions each:

- **BridgeTool::CalcPath** (0x1400f9cd0) — 400+ lines, linked list bridge segments, 5 entity templates, multi-entity creation + transform + picking setup
- **FencedTool::CalcPath** (0x1400dae00) — ~150 lines, dual entity creation, corner post + side fence placement via 8 sub-calls
- **RemovalTool::CalcPath** (0x1401035e0) — ~300 lines, per-tile entity lookup, building/road/crop removal with BST traversal, StateMachine + CitizenAI helpers

These will unblock as more entity creation and world management APIs are typed and resolved.

---

## Shared Engine Function Pointers (resolved in engine_funcs)

Added in Phase 0 to support all Player module reimplementations:

| Function | IAT Slot / Address | Used By |
|----------|-------------------|---------|
| `fn_SceneAddObject` | 0x140151370 | Serialize, Startup |
| `fn_SceneRemoveObject` | 0x140151360 | Shutdown, Serialize |
| `fn_Transform_FromYawPitchOffset` | 0x140151288 | Serialize, CalcPath |
| `fn_Geometry_SetTransform` | 0x1401512e0 | Serialize, CalcPath |
| `fn_MeshInstance_UpdateHierarchy` | 0x140152888 | Startup, Serialize |
| `fn_TileRender` | 0x1401008c0 (exe) | All Draw functions |
| `fn_TileValidityCheck` | 0x14009f5d0 (exe) | Draw, CalcCost |
| `fn_FencedPlacementParams` | 0x1400dbf90 (exe) | FencedTool Serialize/CalcCost |
| `fn_HighlightRemove` | 0x1400b0d00 (exe) | PlacementTool Shutdown2/Serialize |

---

## Profile Source Paths

- `Player\PlacementTool.cpp`, `Player\Player.cpp`
- `Player\BridgeTool.cpp`, `Player\DragPlacementTool.cpp`, `Player\EditTool.cpp`
- `Player\FencedTool.cpp`, `Player\RemovalTool.cpp`

## Key Engine Dependencies

| Function | Source | Status |
|----------|--------|--------|
| Map tile queries | Exe (Game module) | Resolved |
| Pathfinder | Exe (Game module) | Resolved (RoadTool) |
| Input::Device | Runtime | Resolved |
| Graphics::Scene | Runtime (IAT slots) | Resolved |
| MeshInstance | Runtime (IAT slots) | Resolved |
| Math::Transform | Runtime (IAT slots) | Resolved |
| ComponentEntity_CopyCtor | Exe | Resolved (DragPlacementTool) |
| CharacterComponent_impl_08c0 | Exe | Resolved (all Draw) |
