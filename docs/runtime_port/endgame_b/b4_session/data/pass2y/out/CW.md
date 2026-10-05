# Wave Y inventory — region CW (Component / Entity / Work / Action)

174 casts in 65 files, 70 units. Per line: `CW.tsv`; per unit: `CW_units.tsv`.

| kind | casts |
|---|---|
| RULING | 46 |
| ERASED | 34 |
| SIG | 33 |
| CLASS | 26 |
| LOCAL | 25 |
| POD | 10 |

## Doable classes, each one commit (disassembly read this pass unless noted)
- **CitizenComponent (6), doable-with consumedTypes.** Retail ctor 0x140114770 builds the +0xB8 MeshInstance
  (0x140114808). Retail dtor 0x1401146f0 runs ~MeshInstance(+0xB8), then MemFree(+0x98), then ~String(+0x28).
  Reverse declaration order gives exactly that once +0x98 is a Collection::Array. The four null-vptr probes
  and the two lazy `::new` arms in action_base_complete.cpp and showtool.cpp go. Row 650 (CONFIRMED) covers it.
  Nothing memsets or memcpys the class.
- **Map (3).** pickingScene is doable alone. Ctor 0x14009ee70 builds the Scene first. Dtor 0x14009ed60 frees
  the +0x68 / +0x70 / +0x78 objects, the +0xE8 tree, the +0xA8 queue and the +0x80 cells, then tail-calls ~Scene.
  pickingScene is the first member, so it is destroyed last. The Map ctor's memset over +0x68..+0xF8 does not
  touch it. Drop RtPicking_Scene_Ctor and the hand ~Scene in the same commit. +0xE8 / +0xF0 can become one
  AvlTreeHead, which is trivial so the memset stays.
- **ToolbarDescription (1).** Body 0x14012db70 frees the +0x50 String array, then +0x48, +0x40, +0x38 and +0x28.
  That is reverse declaration order with `_entries` as a Collection::Array<String>. The resize then takes
  that Array type. TutorialSetupDialog's two callers already hold Collection::Array<String>.
- **WorldGeometry (2), PathGraph (4), WorkScheduler (1).** Each holds a {root, count} pair as two fields.
  Merging them into AvlTreeHead<Node> is a FIELD change of a trivial POD, so the lifecycle does not change.
  PathGraph also needs PathCostNode as AvlNode30, or the insert templated. WorkScheduler needs an AvlTreeHead
  wrapper of WorkBST_BSTRemove_a6e0. Its trivial static_assert and memset in gameupdateloop_load.cpp:468 still hold.
- **ComponentEntity (2).** `_handle` becomes a union so the category byte store needs no cast.
- **DecalDescription (1), FruitDescription (2).** Two pointer retypes: `_materials` becomes MaterialInstance**
  and `_ref30` becomes ComponentDescription*. Their serializers take void* or are templates.
- **StorageComponent storageSlots.** Dtor 0x14012c870 has one array member, so it can be typed alone.
  The erased resize cast (1) falls only after the trade-manifest local is typed too.

## Blocked
- **HighlightComponent (1).** Row 573, ledger wave X round 1. The null-vptr "never outlined" test carries state:
  sceneId reads 0 on an inert component. Ctor 0x14004c930, dtor 0x14004c8c0.
- **bare T* for InternalPtr<T> (POD, 7).** This is a tree-wide convention. Retyping it is its own sweep
  (ledger wave X round 0, about 100 files).
- **Graphics::Geometry (1), Video::Context (2), ::Vector3 (POD, 3).** These are RT headers, outside this region.

## ERASED cores (34; their receiver views are FLOOR?)
WorkCellBST 10, WorkBST 8-byte keyset 10 (ledger round 1, row 553: one agent owns every Work leaf),
coll_bridge 7, CemeteryListErase 0x140002cc0 5, Desc_LoadExternalArray 0x140032090 1, StorageComponent_DynArrayResize 1.

## SIG (33)
DriveWhenIdle 6, CitizenComp_DeferredInit 3, PlayerTrade_BSTFindRemove 3 (a pure forwarder to Avl38FindRemove),
ActHunt_DynArraySetCount 2, ef40 2, EntityList_RegisterEntity 2, Work(void*) / InitFromEntity 2. Thirteen
more have one cast each. Callers are listed per unit in CW_units.tsv.

## LOCAL (25; stale or in-file)
- **Scratch slots (10).** Locals reused across types in buildroad, consumeproduce, fishingwork and work_leaf_clear.
  Split them into typed locals; the B11 comparison decides.
- **Trace probes (4).** componententity.cpp has a port-only stream-cursor trace. Delete it or mark it CAST-OK crash_walk.
- **Stale tags.** `Map::field_0xA2` is already uint8_t (2). `g_pForceWorkBuilding` is already ComponentEntity* (1).
- **In-file fixes.** Tornado's file-static dead argument (2). The raw UI::String locals (2), but read the retail body
  first. The boat Vector2 loop, the leisure locals, the force-work harness pun and the clear-area zero position (1 each).

## RULING totals (46)
- **h8 (11).** Byte overloads in BuildStructure, ClearArea and ConsumeProduce (6). StateMachine int8 fields through
  the uchar overload (4). ComponentEntity `_entityFlags` through the int& overload (1).
- **retail_diff (27).** By known_port_issues row: 260 (6), 400 (4), 1 (3), 51 (2), 552 (2), 430 (2), 220 (2).
  Rows 45, 280, 511, 17 and 510 have one each.
  One more has no row: Startup skips retail's IsDerivedFrom 0x1401c7190 check (citizencomponent.cpp:2414).
- **const_find (2).** FindOrLoad returns const.
- **One each:** vptr_probe (pickup.cpp:514), callback_ctx (Component::OnMessage data), and
  alloc_shape:harness-stack-buffer (clearareacomponent.cpp:193).
- **Outside the listed classes.** null_receiver (2) is work_act_consume's faithful fault address. slot_pun (1) is
  gatherrawmaterials:362, where retail passes the reused param3 slot as the Time.

## Not verified
- I did not open WorldGeometry dtor 0x1400aed50 or the PathGraph free 0x1400171b0. The merges are trivial-POD
  FIELD changes.
- The Map pickingScene readers are files that name `pickingScene`; GameUpdateLoop.h has its own member of that name.
- The RT units take their owners from the line tags.
