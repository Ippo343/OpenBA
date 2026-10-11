# G_standalone blockers (standalone/): 602 casts, 24 files

Counts are estimates (tool_harness.cpp was classified by snippet pattern: entity/map/list variable names,
then offsets; expect +-10%). Rows: `G_standalone.tsv`. Sum = 602.

LOCAL 386 (64%), SIG 103, FIELD 50, other (NOCLASS/OTHER/POD/H5) 42, FLOOR 21.

## Blockers by casts (key, casts, files, header that changes, change)
- LOCAL:ComponentEntity 65 (tool_harness) — entity held as void*/uint8_t*; `_components`(+0x30) and
  `_componentCount`(+0x38) are declared in Entity/ComponentEntity.h. Retype the harness locals; the 0x28/0x2a
  int16 reads of a MapComponent (comp+0x28/+0x2a) need MapComponent.h checked.
- LOCAL:EntityList 63 (tool_harness) — `gw+0xE8` list, +0x08/+0x10 array, +0xD0/+0xD8 second array, local+0x40
  flags. EntityList.h exists; check +0xD0/+0xD8 are declared.
- LOCAL:Map 54 (tool_harness) — size, cells +0x80, MapCell stride 0x30, +0x29/+0x40 flags: Game/Map.h declares
  them.
- SIG:EditTool_HarnessAcquire-void* 54 (tool_harness 49, orchard/food_panel) — the acquire returns void*; tool
  offsets +0x10/+0x20/+0x28/+0x38/+0x68. Retype the return in Player/edittool_funcs.h and use EditTool members.
- LOCAL:GameUpdateLoop 48 (tool_harness) — `gw` is void*/uintptr_t; +0xE8/+0xF0/+0xF8/+0x118/+0x120/+0x148 are
  declared members of GameUpdateLoop.h.
- LOCAL:BuildComponent 36 (tool_harness) — footprint rect +0x28..+0x2e (int16), +0x48/+0x4c, +0x18; confirm the
  +0x28..0x2e rect is declared in Component/BuildComponent.h (+0x48/+0x4c/+0x58 are).
- LOCAL:DragTool 23 (tool_harness) — +0x28 flag, +0x2c startCell, +0x30 endCell: Player/dragtool.h declares them.
- LOCAL:EntityUI 19 (tool_harness 15, food_panel 4) — entityUI+0x28 tool IBP slot; Dialog/EntityUI.h exists.
- LOCAL:PastureComponent 16, LOCAL:OrchardComponent 8, LOCAL:UpgradeUI 14, LOCAL:DialogInstance 14,
  LOCAL:CreateBridgeDescription 6, LOCAL:TradeUI 3, LOCAL:ModelComponent 10 (perf_experimental, scene_watch),
  LOCAL:Geometry 3 — one-offs; each header exists (check the fields).
- FIELD:EditTool::entityRef 16 (5 files) — `entityRef` is uint64_t; harnesses read the DialogInstance** at tool+0x28.
- FLOOR:vslot 16 (cropfield, pausemenu, food_placement, orchard_diag, main) — vtable word probes; mark CAST-OK vslot.
- NOCLASS:ui-buffer-component 13 (tool_harness) — component +0x28 buffer / +0x30 count / +0x34 cap (DlgSettings
  house buffer); no class.
- SIG:EntityUI_SetEditMode-uint64 11, FIELD:DeviceData::swapChain 11 (main.cpp; the swap chain lives at +0x18 of
  the DX11 DeviceData subclass), NOCLASS:AnimationState-layers 11 (perf_experimental), SIG:CitizenEmploy-int64 8
  (Component/exe_bodies.h), SIG:UI-element-void* 6, FIELD:UI::Element::_name 5 (mouse_inject), FIELD:MeshInstance::_mesh 5,
  NOCLASS:mesh-level-record 5 (scene_watch), H5:PlayerController::camera 4 (scene_watch), SIG:Dialog-void* 6.

## 10 largest LOCAL files
tool_harness.cpp 372, perf_experimental.cpp 7 (+11 NOCLASS), food_panel_harness.cpp 4, scene_watch.cpp 3. No other
file has more than 4 LOCAL casts: tool_harness alone is 62% of the region.

## Classes with NO header yet
UI component raw buffer (+0x28/+0x30/+0x34), Animation/AnimationState layer records (perf_experimental),
mesh level record 0xb0 stride (scene_watch), ProfessionTreeNode in CitizenBehavior::field_0x100 (0x38 node).

## Brief corrections
- Almost nothing in tool_harness.cpp is a real blocker: the harness reads offsets of objects whose headers
  already declare the fields, because it keeps them as void*/uintptr_t. One agent retyping the harness locals
  (gw, entity, entityList, map, tool) removes ~330 of 455 casts.
- The `static_cast<uint8_t*>(tool) + 0x28` pattern in 6 other harnesses is the same FIELD:EditTool::entityRef
  / SIG:EditTool_HarnessAcquire pair; fix once in Player/edittool_funcs.h.
- redzone_alloc.cpp (2) is already FLOOR:alloc_header (its own LEFT says the ceiling is full).
