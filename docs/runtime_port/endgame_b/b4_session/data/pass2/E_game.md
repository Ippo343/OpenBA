# E_game blockers (code/Game/Game + code/Game/Object): 931 casts, 71 files

Counts are estimates split by line range (sum = 931). Rows: `E_game.tsv`.

## Blockers by casts (key, casts, files, header that changes, change)
- ERASED:ArraySpec-elem, 155, 11 (starttown-free; mapgenerator, game_leaf_helpers, gameupdateloop_load,
  citizenlist/gamegrid/gameupdateloop_serialize, eventlog, serialize_arrays, worldgen_f390_*) — runtime
  collection ArraySpec: the assign/init/destroy hooks take `uint8_t*`; plus `SetSize` through local
  `XArray*` aliases on a `void* header`. One typed-hook door in the runtime Array core removes ~all.
- SIG:Teardown-externC-void*, 65, 1 (gameupdateloop_teardown.cpp) — EntityList_Shutdown/Dtor, CitizenList_Dtor,
  EntityUI_Dtor, Tutorial_Dtor take `void* self`; the headers (EntityList.h, CitizenBehavior.h, EntityUI.h,
  Tutorial.h) exist: type the parameter, member-ise, delete the P/I32/Arr accessors. Callers: Shutdown.
- NOCLASS:work-tree-node, 49, 2 (teardown 40, game_leaf_helpers 9) — {root,count} trees whose nodes (kids
  +0x18/+0x20 or +0x20/+0x28, IBP at +0x08/+0x10) have no class; layouts need settling (shared Work tree).
- LOCAL:CreatePlacedDescription 34 (starttown) — cpd+0x40/0x44/0x48 are `_i40/_i44/_i48` already declared.
- CONST:FindOrLoad 23, 11 files — `const_cast` of FindOrLoad/Find/FindNext (runtime Object header).
- LOCAL:Map 22 (starttown 18, gamegrid_serialize 4) — Map.h declares cells/size; MapCell stride 0x30.
- H5:PlayerController::camera 21, 5 files (gameupdateloop, menudialogloopt, load, menuscene, minimap_paint) —
  Camera held as raw bytes at +0x34 (protected default ctor). Fix: ctor-in-place or friend door.
- LOCAL:CharacterGrid 20 (starttown 3413-3441) — CharacterGrid.h exists; gridPtr is a void*.
- LOCAL:NameDatabase 18 (citizen_names) — header already declares `_array1.._array4`.
- LOCAL:CitizenBehavior 14 (starttown) — +0xA0/0xA8/0x110/0x118 declared.
- SIG:ExternalFactory-void* 14 (gameupdateloop_load, game_leaf_helpers) — factory held as void*.
- ERASED:AVL-retrace 14 (citizenlist_serialize, gameupdateloop_serialize, goals_serialize) — shared AVL/set
  helpers take `int64_t*` (Player|Component|Goal/exe_bodies.h).
- SIG:EventLog-wrapper-void* 14 (eventlog.cpp, eventlog_achievement.cpp) — C wrappers take void*.
- LOCAL:CitizenComponent 13 (starttown) — +0x238/0x248/0x250 declared; check +0x3c/+0x1f0.
- H5:ExternalRef::strPtr 13 (mapgenerator 10, starttown 3) — String held as raw storage (MapGenerator.h).
- POD:RtGraphics::Renderer 13 (4 files) — Scene/Renderer/AnimMgr ctor/dtor are doors on POD mirrors.
- SIG:MenuLoop-void* 13 (menudialogloopt, _stubs, splash_loop_stubs, game_main) — STANDALONE passes void*.
- OTHER:std::malloc-scratch 13 (water_contour_mesh) — typed malloc scratch; use std::vector. Not a header job.
- LOCAL:BuildComponent 12, SIG:TownRegistry-int64-callees 11, SIG:Map-as-int* 11, POD:RtObject::Internal 11,
  SIG:Serialize-stream 11 (10 files), SIG:FindComponentOnEntity-void* 10, LOCAL:GameUpdateLoop 10 (starttown
  local `void* gw`), OTHER:InternalBasePtr-raw-slot 9, H5:WorldGeometry::_arrays 9, H5:MenuDialogLoopT::arrays 8,
  FIELD:Video::Context::_pixelConstantStaging 8 (+_unknown20f0 3), POD:RtMath::Frustum 8, SIG:AllocSub-void* 8.

## 10 largest LOCAL files
starttown.cpp 134 (CreatePlacedDescription 34, Map 18, CharacterGrid 20, CitizenBehavior 14, CitizenComponent 13,
 GameUpdateLoop 10, ResidenceComponent 7, InteractDescription 7, StorageComponent 6, Radius/RawMaterial 12);
citizen_names 18 (NameDatabase); construction_force_harness 12 (BuildComponent; matComp+0x30 class unchecked);
forester_force_harness 14 (ForesterComponent 8, ComponentEntity 6); gamegrid_serialize 4 (Map);
gameupdateloop 6 (Toolbar: +0xC8/+0xD8/+0x100 need adding to Toolbar.h);
gameupdateloop_load 4+4 (ProfessionList +0x30/0x38 declared; TerrainGenerator +0x80 H5).
Headers needing fields: Toolbar.h, WorldGeometry.h (+0x1E8 count), InteractDescription (0x18-stride entry),
RadiusDescription/FishingDescription/HerbalistDescription (check +0x28/+0x2c/+0x20), CitizenComponent (+0x3c,+0x1f0).

## Classes with NO header (NOCLASS)
Work tree node (two layouts), starttown path-array entry {float* pts; int n} (0x10), weather effect record
(geo+0xC4/+0xC0), ForceWork record (s_fw), NameDatabase none, ProfessionTreeNode (declared locally).

## Brief corrections / notes
- Teardown's 111 "casts" are the offset-accessor helpers P/I32/Arr plus ~100 calls through them, not `reinterpret_cast`
  at call sites; one retype of the extern "C" parameters does not remove them until the accessors are replaced
  by member access, so it is a rewrite, not a cast sweep.
- FLOOR candidates here are few: byte_io 6 (the accessors), win32 3, ptr_int 3, vslot 5 (force_work probe,
  exe_type.h macro), string_header 4, alloc_header 1.
- `LOCAL` for starttown is mostly local `void*` finders (FindComponentOnEntity/FindDescriptionOnTemplate return
  void*): typing the returns (ComponentEntity-component lookup by TypeId) turns ~60 LOCAL casts into field access.
- Line 4675 (PerfX::TerrainNoteWrite takes bytes) is in standalone/perf_terrain.h, not this region.
