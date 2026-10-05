# Wave Y inventory — region GAMEB (Dialog/, Player/, standalone/)

128 cast lines in 41 files, 37 units. Per line: `GAMEB.tsv`; per unit: `GAMEB_units.tsv`.
Work (CLASS / SIG / POD / LOCAL that a commit can remove): 77. Rulings only (incl. the ERASED row): 51.
Retail dtors read in Ghidra this pass (2026-10-04): HighlightComponent 0x14004c8c0, TownHallUI 0x1400810d0,
PlayerToolManager 0x1401024d0, Video GraphicsInterface 0x18000cdc0 / Adapter 0x18000cc10,
Entity_impl_c4a0 0x14004c4a0.

## Doable classes and their one-commit scope (by casts)
- **CLASS:HighlightComponent (17)**, `behaviour:`. Type +0x28 as `Graphics::EdgeMeshInstance`. The ctor then
  constructs it, which is retail's ctor order. Give the dtor a body that deletes `fieldF8` through its slot 0
  and nulls it. Member destruction then runs `~EdgeMeshInstance`, which is retail's order. Delete the tree's
  null-vptr tests (highlightcomponent.cpp:62, tool_base_render.cpp:473, :875) and the lazy `::new`.
  Retail `Entity_impl_c4a0` has no such test: it tests +0x120 >= 0. The component is built by its real
  ctor, so the "inert factory" comment at tool_base_render.cpp:467 is stale. Files: HighlightComponent.h,
  highlightcomponent.cpp, tool_base_render.cpp, vis_abi_test.cpp. Header is CW's.
- **CLASS:TownHallUI (4)**, `behaviour:`, doable-with all eleven owned containers. Retail frees them in
  strictly descending offset order: +0x1d8 tree, +0x1c8, +0x138 tree, +0x118, +0xf0, +0xe0/+0xd0, +0xc0/+0xb0,
  +0x50 tree, +0x40 rows. So reverse-declaration order of real members equals retail only if the three
  trees become typed members too. Typing only the arrays would free the trees first. `ProfessionRow` owns
  an inner Array, so its element needs a real dtor. Its push is byte-copy plus per-element destroy, which
  needs a hand-derived ArraySpec (townhall_helpers.cpp:161-169). This is the largest commit in the region.
- **CLASS:PlayerToolManager (2)**: FIELD. It has no C++ dtor, and `AvlTreeHead` is a POD. Type +0x50 and
  +0x60 as set headers. The +0x50 nodes carry a `ComponentDescription*` key, so they get their own node
  type. Readers are about ten files, listed in the units file.
- **CLASS:Map (1)**, **CLASS:TradeComponent (1)**, **CLASS:MultiObjectComponent (1)**,
  **CLASS:Object::BaseTypeInfo (1)**, **CLASS:SaveLoad::SaveInfo (1)**: single-field typings. They do not
  move any destruction. Map's ctor memset over +0x68..+0xf8 is harmless for a POD header. SaveInfo's qsort
  comparator reads sortKey as an integer and needs DateTime's ticks.
- **CLASS:Video::Device::GraphicsInterface (2)**, RT. Both dtors run in reverse-declaration order. The
  commit is doable-with Adapter's three members and GraphicsInterface's two. Today they are doors, and the
  `Array<Adapter>` helpers open-code the teardown.
- **CLASS:GeometryBatch (3)**: the narrow fix is FreeHeader for :271/:272 and a RawArray<uint16_t> grow for
  :95. Full H5 is blocked: the record lives in RawArray pools with hand-written copy and resize
  (0x14001f5e0, 0x14001f250, 0x14001fa80).
- **SIG, doable now**:
  - fn_TrackerComponent_Rebuild (2): stale since round 7.
  - CitizenComp_DeferredInit_117080 (1).
  - BuildRoad::BuildRoad (1).
  - RoadTool_DecalStore_acd90 (1): stale tag.
  - Dialog_AutoSaveHandler (1): the saveload harness's stack fake goes too.
- **SIG, typed wrappers per node type over one erased core**:
  - ToolbarUtility_BstInsert_4890 (5)
  - PlayerTrade_BSTRemove_25740 (3)
  - PlayerTrade_BSTFindRemove_2c690 (2)
  - WorkBST_BSTRemove_a6e0 (2): with Map +0xE8 and WorkSchedulerCountNode.
- **POD:AvlNode28 (3)**: give the path-segment tree its own node with a PathSegment element.
- **LOCAL (12)**:
  - tess vertex copy (8): per-member memcpy of 8/8/4, then check the -O2 store widths.
  - CalcCost out (2): an int local.
  - perf_experimental (1): a `::Frustum` local, as geometry_draw.cpp does.
  - keystone `timeOutSlot` (1): the slot really is both a Time and an int, so a union is still a pun.
    The coordinator may rule it instead.

## Blocked
- **CLASS:RtIO::Mod (6)**: ledger wave X round 1 says Mod is an element of erased containers with
  hand-written lifecycles. The `void*` Array cores in modification.cpp come first.
- **POD:RtSystem::Filename (2)**, from `ModSetNode::key`: X-R3 says the mirror is not deletable yet. It
  goes with the "Filename is-a String" `behaviour:` commit (ledger round 8).
- **SIG:Toolbar_CenterOnEntity_Impl (3)**: its two callers carry retail differences, rows 690
  (citizenui.cpp) and 452 (residenceui.cpp).

## Rulings (count per class; one ruling each)
| ruling | casts | where |
|---|---:|---|
| const_find | 14 | FindOrLoad x13, FindNext x1 (debugui, townhallui x4, moddialog x2, mapui, noiseui, saveload, mainmenu, dialog_exe_helpers) |
| vptr_probe | 8 | tool_base_render x5 (664 is a raw dump: crash_walk), cropfield_force_start x2, stress_harness |
| retail_diff:452 | 7 | residenceui GoToCitizen |
| callback_ctx | 5 | Picking::Scene::GetUserData x3, MainLoopCallback::_context, Element user data |
| dx11_seam | 5 | main.cpp swap chain x4, boot_window |
| h8 | 3 | EventLog::field_0x10, GeometryBatch +0x44 / alternateKey |
| retail_diff:155 | 3 | combo handed as InternalPtr<Element>* (harnesses) |
| erased_self | 3 | scene_watch member-to-owner |
| retail_diff:572 | 2 | loaderdialog null-allocator arm |
| ERASED 0x140059670 | 1 | TradeUI_GetBuyEntries_59670, two out-array types |
| LOCAL:time_slot | 1 | above (local or ruling) |
