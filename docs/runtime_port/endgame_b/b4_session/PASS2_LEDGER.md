# Second-pass ledger — what one block left for another (coordinator's; appended per hand-in)

One line per ask: **owner block** <- asking block: what, where. A block's prompt quotes its lines.
`DONE` is written in front when the owner's merge served it. Wave X = the signature / forwarder wave.

## Asks by owner

### CIT-1 / CIT-2 (citizen_* files)
- <- CIT-4: `CitizenComponent` +0x60, +0x70, +0x78 and `clothingTimer` are still `int64_t` ticks because
  `citizen_updatehealth.cpp` (CIT-2) and `citizen_needsupdate.cpp` (CIT-1) do integer arithmetic on them.
  Retail type is `System::Time` (as `foodTimer`, `happinessTimer`, `schoolTime`, typed by CIT-4). The block
  that owns those two files may ask the coordinator for the header typing once its arithmetic reads
  `.ticks` (or whatever `System::Time` names it); four serializer casts in citizencomponent_serialize.cpp
  then fall (coordinator / wave X).

### COMP-8 (workplacecomponent, workcomponent, zonecomponent, tradecomponent)
- <- CIT-4: `WorkplaceComponent_Detach` / `_Attach` (workplacecomponent.cpp, declared in
  Component/exe_bodies.h) have NO caller left: delete them and their declarations.
- <- ENT: zonecomponent.cpp and workplacecomponent.cpp hold LOCAL copies of the bucket-node struct that
  is now `CitizenBucketNode<Key>` / `ProfessionBucket` / `TemplateBucket` in `Entity/CitizenBehavior.h`:
  delete the local copies, use the header's.
- <- GAME-2: `WorkScheduler::field_0x20` is `void*` (the Tutorial teardown in gameupdateloop_teardown.cpp
  casts it). COMP-8 owns `Game/WorkScheduler.h`.

### COMP-5 (immigrationcomponent_states ...)
- <- ENT: immigrationcomponent_states.cpp has a local copy of the bucket-node struct (see COMP-8).

### DLG-1 / DLG-2 / DLG-3
- <- ENT: toolbardialog.cpp and townhall_helpers.cpp hold local copies of the bucket-node struct.
- <- GAME-2: `ToolRegistryNode` is a struct local to toolbardialog.cpp; the teardown bodies 0x14002df90 /
  0x14002de90 (gameupdateloop_teardown.cpp) walk the same nodes through offset helpers. Move the struct to
  a header (`Game/PlayerToolManager.h` is PLAYER's; a new `Dialog/ToolRegistryNode.h` is fine) so wave X can
  finish teardown.

### TUT
- <- ENT: goal_exe_helpers.cpp has a local copy of the bucket-node struct.

### GAME-1
- <- CIT-4: `Disease::_stringTable` is `void*` (Game/Disease.h).
- <- GAME-2: `EventLog::entries` is a raw triple (serialize SetSize in gameupdateloop_serialize.cpp).

### GAME-3 / GAME-4
- <- ENT: town_workplace_registry.cpp has a local copy of the bucket-node struct.
- <- GAME-2: TerrainGenerator +0x80 (gameupdateloop_load.cpp:1364), Toolbar +0xC8 / +0xD8 / +0x100
  (gameupdateloop.cpp:1770).
- <- T1: `StartTown_GetFoundingHouseEntity`, `StartTown_ResolveTemplateByPath` return `void*`;
  `g_pConstructionDemoEntity` is `void*` (12 casts in tool_harness.cpp).
- <- GAME-2 (its own, not done): `PlayerController::camera` (H5, 14 casts; `Graphics::Camera`'s default ctor
  is protected, Camera.h is R-GFX's), `WorldGeometry::_arrays` (four files of other blocks read it as
  `Header`).

### PLAYER
- <- GAME-2: `PlayerToolManager::cameraViews` is a raw triple; `currentTool`'s `DeletePtr` needs a cast.
- <- T1: the `RemovalTool` / `PlacementTool` factories return `void*` (3), two `void*` bridge globals,
  `EditTool::entityRef` / `lastEntityRef` are `uint64_t` (11 casts in the harnesses, 1 in entitylist.cpp).

### SA (standalone)
- <- T1: `panel_hold_harness.cpp`, `panel_staffing_driver.cpp`, `orchard_force_start.cpp`,
  `trade_buy_harness.cpp` declare the `EditTool_Harness*` functions locally with `void*`; the definitions
  are typed now (`PlayerToolManager*` in, `EditTool*` / `ComponentEntity*` out). Declare them once in
  `Player/edittool_funcs.h` (coordinator will allow that one append) and delete the local externs.
  `tool_harness.h`'s `ToolHarness_Tick(void*)`.
- <- ENT: food_panel_harness.cpp, death_harness.cpp hold local copies of the bucket-node struct.

### ENT (merged) — second visit, or wave X
- <- GAME-2: EntityList's and CitizenBehavior's arrays are raw triples, not `Collection::Array` (H5):
  teardown `PtrArr` x5, `Arr` x9, `Array8_Clear_1740`, the SetSize casts in gameupdateloop_load.cpp.
- `EntityList_RemoveEntity` (entitylist.cpp) has no caller left after GAME-2: delete (wave X).
- action_base_hunt.cpp, gameupdateloop_serialize.cpp:600 still cast `_entityList` (typed now).

### R-IO / R-UI (runtime headers)
- <- ENT: the `IO::Serializer` internal walk in `CitizenBehavior` Persist (10 casts) needs Serializer.h
  fields; `DialogInstancePtr` is `void*` (UserInterface.h).
- <- S1: nine file-local `Stream` / `rtdesc::Stream` copies of `IO::Stream` (collisionmesh, surface,
  pointlist, pickingmesh, external_list, dialog_vtable, elementdescription_serialize, page and radio
  description); three `RtIo::` MemoryStream / CompressedFileStream mirror casts (internal_class.cpp,
  external_load.cpp). The `IO::Serializer`-typed family (`String::Serialize`, `Math::Vector*::Serialize`,
  `Transform::Serialize`, `Audio::Scene::Serialize`, `CompanionData` [6], `ShaderProgram`) still takes
  `void*`: its own tree-wide sweep in wave X.

## Wave X additions (cross-file signatures found by blocks)
- S2: Action slots [4] AbstractImpl, [5] Execute, [6] Enter, [7] Exit stay `void*` until
  `CitizenAI_ActionExecutor_c4c50(int64_t*, void*)` is typed, `CharacterComponent` +0x48 is an
  `ActionStack`, and `Component::Update` / `Tick` pass `System::Time*`.
- S3: `Controller::Tick` / `StateMachine::Tick` pass `time` as `void*` (two handler casts:
  destroycomponent_states.cpp:981, residencecomponent.cpp:577). `FindOrLoad` / `FindNext` const: about 50
  `const_cast` sites; retail drops the const in inlined template code: decide at the close whether this is
  a floor category.
- CIT-4: `WorkComponent_CalcMaxWorkers`, `ExternalPtrSerializeGeneric`, `CitizenComp_Entity_impl_ef40`,
  `WorkBase_ValidateCell_a00f0`, `fn_CitizenComponent_HasVacancy`, `WorkBase_CalcDistance_8830`,
  `Reimpl_ExternalPtrArray_Serialize`, `Pf_ZoneRefresh_9c480`, `CitizenComp_SortByProfession_02a0`,
  `ActRm_StorageSubtract_d150`.
- ENT: `PathBST_Insert_25a10`, `PathPairs_Clear_25b30`, `WorkBST_*`, the `fn_FindComponent_*` family,
  `FindComponentByTypeId`, `EntityList_RegisterEntity_Impl` (38 callers), `ComponentEntity_CtorFromDescription`,
  `Entity_FindConsumeProduce_b6f0`, `EntityList::Update(void* time)`.
- GAME-2: `TradeWork_helper_14b00`, the shared set insert / retrace (`int64_t*`), goals_serialize x8.

## Notes for the close
- tier_i rises of +1 (an alloc-result `static_cast<T*>(MemAlloc(...))` replacing a counted cast) were
  accepted: tier I is not in the total.
- Floor markers added by blocks: GAME-2 `crash_walk` +2, `string_header` +1 (ceilings are compared at the
  close: FLOOR.md).
- GAME-2: 0x1400173d0 / 0x1400174e0 became two template instantiations each over `CitizenBucketNode<Key>`
  where retail has one body (ICF).

### R-OBJ / R-GFX / R-IO (runtime headers) <- R-COLL
- R-OBJ: `TypeInfo.h` `TypeNode` +0x48 is one int32; retail uses two int16 (balance +0x48, height +0x4a:
  `known_port_issues` row 100): `Balance50` / `Height50` in avl_trees.cpp:296 / :299 wait on it. The
  element records `array.cpp` destroys that live in `External.h`.
- R-GFX: the 0xb0 `TriangleList` element (`GraphicsMesh.h`; array_graphics_mesh.cpp, 33 casts; row 102),
  `AnimationGroup.h`, `Camera.h`, `Material.h`, `VertexProgram.h` / `PixelProgram.h` element records
  (`array.cpp`, array_destroy.cpp's VtxStream).
- R-IO: `Modification.h` element records, `FileSystem.h` (introsort.cpp:104); `PortedRotateLeft8` /
  `Right8` in modification_refresh.cpp have NO caller left: delete. `PortedRetrace8` is called from
  Collection.h with an erased signature.
- Wave X (outside callers hold `void*`): `StringMapInsert`, `FilenameSetInsert`, `SubtreeFree`,
  `rtpathid::Insert` / `SubtreeFree`, `rtsetclear::Clear`, `rtsetavl24::AllocNode`, `PtrArrayDtor`, every
  `RtColl_ArrayDtorElem` / `ArrayAssign` / `ArrayCopyCtor` entry, the `A_ENTRY` / `B_ENTRY` / `J2_ENTRY`
  macros, `AsArray` (array_internal.h).
- Close: the six `this` -> `ArrayHdr` casts in `Array<T>`'s members (Collection.h) need a decision (a
  marker whose operand is `this` is a BAD MARKER by rule M2); `elem_stride` is 41 against a ceiling of 18.
- R-COLL renamed two allocators (`AllocTraits::Alloc` -> `Allocate`, `ListNodeAlloc` ->
  `AllocateListNode`) so their results match the tier-I allocation shape: they ARE allocation results;
  recorded so the audit can judge it.

## Round 2 hand-ins (CAI-6, CAI-4, WORK-1, R-OBJ)
### COMP-8 (owns `Game/WorkScheduler.h`)  <- CAI-6
- Names for `WorkScheduler` fields, from the code: `field_0x00` the work-id counter (wraps at 0x7ffffffe),
  `field_0x04` the queue-order counter, `field_0x10` / `field_0x18` the queue-order tree and its count,
  `field_0x120` / `field_0x128` the per-profession bucket tree root and count. `WorkComponent::workFlags`
  (+0x2c) and `Work::field_24` hold the posted work's queue-order key (row 201).
### DLG-1 (owns `Game/Map.h` per the map) <- WORK-1
- `Map::regionPairCache` is `int64_t*` (2 casts in Work files).
### WORK-3 <- WORK-1
- `CharacterComponent::actions` / `field_0x58` (2 casts in Work-1 files).
### R-COLL second visit / wave X <- R-OBJ
- `avl_trees.cpp:295-299` `Balance50` / `Height50` can now read `n->balance` / `n->height` (TypeInfo.h typed).
- `array.cpp:854` copies `ExternalArrayElement::opaque` as one qword (really two int32s); `owned` is
  retail's Filename (H5).
### Wave X additions
- CAI-6: `CitizenAI_WorkScheduler_Insert` / `_Post` / `_Rekey`, `Pf_AvlRebalanceA_a8c0`,
  `Pf_AvlRebalanceB_13480`, `WorkCellBST_Insert_9a40`, `WorkCellBST_Remove_ca60`.
- CAI-4: `Pf_AvlRotateB_e450` / `_e580` (int64_t), `CitizenAI_Reg_helper_e360` (retail 0x14000e360 serves
  several node types: callers in citizenlist_serialize.cpp, action_pf_avl.cpp, tradeui_leaves.cpp).
- WORK-1: `WorkCellBST_*`, `WorkBST_*`, `WorkGrid_*` / `ValidateCell` (map as `int*`),
  `WorkComponent_CalcMaxWorkers`, `WorkGrid_TryAssignWorkplace_f8620`, `WorkAct_PerformWorkCtor_*`,
  `GatherRawMatCtor`, `WorkAcc_FindComponent_6330`, `WorkLeaf_FreeStorageCapacity_116540`,
  `StorageComponent_QueryAvailable`, the two ctors' `uint64_t` description arguments, the `Pf_*CopyCtor`
  signatures. No caller left anywhere: `Pf_RegionPathLength_8ba0`, `Pf_RegionPairCacheInsert_5b30`
  (action_pf_astar.cpp), `Pf_RegionSpiral_9d0a0` (action_pf_geom.cpp): delete.
- R-OBJ: `RtObject_Factory_Dtor`, `InternalFactory_Dtor`, `ExternalFactory_Dtor` (caller
  engine_lifecycle.cpp), `IsDerivedFromNullSafe(const void*)`, `ExternalPtr` `typeInfo`,
  `RtObject_Type_Serialize(void*, void*)` (five callers in code/Game), `BaseTypeInfo::factoryFn` /
  `inPlaceFactoryFn` (printed by standalone/main.cpp, compared in tests/unit/b39_ti_*), the
  `ExternalPtr` serialize thunks and `InternalPtr<T>` bridges (about 100 callers).
### Close
- `elem_stride` is now 53 (R-COLL 31 + R-OBJ 12 + 10 standing) against a ceiling of 18.
- R-OBJ: `Factory::Startup`'s unregistered-parent path still faults reading 0x28 as retail does, but GCC
  now emits it as an explicit load + `ud2`.
- Allocation results not matching the tier-I name shape: `TypeChildAlloc`, `InternalMemAlloc`.

## Round 3 hand-ins (CAI-5, CAI-2, WORK-2, R-GFX, CAI-3)
### Coordinator (after CAI-1, CIT-1, CIT-2 are merged): the `System::Time` members held as `int64_t`
- `CitizenAIComponent` `timers[16]`, `leisureTimer`, `dataC8`: integer arithmetic in citizenai_candidatelist.cpp
  (`timers[id] < 1`, `int64_t acc = dataC8`), citizenai_gather.cpp (`timers[6] = stamp`), citizenai_leisure.cpp
  (`leisureTimer = 0`); seven Time / InternalPtr serializer casts wait on it.
- `CitizenComponent` +0x60, +0x70, +0x78, `clothingTimer` (see CIT-1 / CIT-2 above).
### R-COLL second visit <- R-GFX
- `array_graphics_mesh.cpp` (29 casts) can use `GraphicsMesh::DetailLevel*` (the "TriangleList" name there is
  the misnomer); `array.cpp`'s `_tags` / `MaterialConstant` readers and `array_destroy.cpp`'s VtxStream can use
  `AnimationGroup::Tag`, `MaterialConstant::name`, `VertexProgram::Element` / `ElementArray` / `Stream`.
### S2 follow-up (Action.h)
- <- CAI-2: `ActionStack::field_0x10` is `Action*` where retail holds an `InternalPtr<Action>`.
### GAME-4 <- CAI-3
- `g_pForceWorkBuilding` is `void*` (two casts).
### Wave X additions
- CAI-5: `WorkBST_RegisterWork_1a30`, `Pf_AvlRebalanceA_a8c0` (the template is only instantiated in
  action_pf_avl.cpp), `WorkBase_FindComponent_2ed0`, `CitizenAI_PlayAnimationCtor4_ba6b0`.
- CAI-2: `CitizenAI_ActionExecutor_c4c50`, the move-tick `1094c0`, `ResidenceCheck_2ac30`, `ec70` / `e370` /
  `dd30`, `Storage_GetResourceCount2`, `ActChild_TransferToCarrier_ca230`,
  `CitizenComp_StorageComponent_QueryCount`, `WorkAct_GatherRawMatCtor_bf790`, the SA-owned
  `PerfX::TerrainGet` / `PerfX::PickCullSkip` callbacks. No caller left: `CharMgr_sub_8660` (charmanager.cpp),
  `Pf_PickReachableCell_edb0` (action_pf_road.cpp), `Pf_RemoveAnimalCtor_bbea0` (action_pf_roots_drop.cpp).
- WORK-2: `WorkImpl_WalkablePos*`, `WorkImpl_BuildStructureStep_6ef0` (its "time" argument is a reused slot
  holding a WorkPlaceComponent), `PlayerTrade_Retrace_25920`, `TradeWork_helper_14b00` / `_impl_4b40`;
  MarketWork's claim Set is two raw fields (a six-file header change).
- CAI-3: the `CitizenComp_*` int64 entry points, `CitizenAI_PerformWorkCtor4/6/8/11`, `NavigateTo`,
  `WalkTowards`, `ActMv_FindComponentFamily_12d0`, `Component::Update(void*)`.
- R-GFX: the `ExternalPtrHandle::p` alias (one alias serves five types, 12 reads), the collection element ops
  taking `void*`, the CullStack and EdgeMesh vertex doors (tool_base_render.cpp), PlayAnimation's time
  parameters, the `rtpickgrid` / `rtgfxgrid` helpers. dx11 seam: Context `_renderBuffer` / `_imageBuffers`,
  `SetVertexBuffer` / `SetImageBuffer` / `SetRenderBuffer`.
### Separate task (retail differences), found this round
- Row 260: consumeproduce's first input pass writes 12 bytes into an 8-byte local (retail uses 16 and jumps to
  the action setup). Rows 220-221: `ClearArea::Validate` / `BuildStructure::Validate`.

## Round 4 hand-ins (CAI-1, WORK-3, CIT-2, CIT-1, R-IO)
### Coordinator: `System::Time` members — exact expressions (do after ACT / COMP blocks, one commit, gated)
- `CitizenComponent` +0x60 / +0x70 / +0x78 / `clothingTimer`: citizen_updatehealth.cpp :785 (cast disappears),
  :780 / :786 / :727 `= 0` -> `._ticks = 0`, :479 / :833 / :763 / :829 compares on `._ticks`, :762 / :827 / :828
  `-= *dt` -> `._ticks -= dt->_ticks`, :522 / :524 / :728 / :831 `= TimeFromSeconds(...)`, log lines :482 / :536 /
  :735; citizen_needsupdate.cpp :114 `field_0x70._ticks -= t._ticks`, :203 `clothingTimer = t`.
- `CitizenAIComponent` `timers[16]`, `leisureTimer`, `dataC8`: candidatelist.cpp :454 / :464 / :471 and the sites
  listed in round 3.
### Facts for every later block (from R-IO, merged)
- `IO::Encoding::CreateReader(Serializer&, int)` (+0x18) / `CreateWriter` (+0x20) are typed virtuals: a raw
  `MakeReaderWriter` slot read becomes `IO::Serializer ser(stream, v)` or `s._encoding->CreateReader(ser, v)`.
- `IO::Stream` exposes `_direction`, `_encoding`, `GetDirection()`: the nine file-local `Stream` copies
  (collisionmesh, surface, pointlist, pickingmesh, external_list, dialog_vtable, elementdescription_serialize,
  page / radio description) can use the real class; the `RtIo::` mirror casts in `runtime/src/object/` can use
  `IO::CompressedMemoryStream` / `MemoryStream`.
- Serializer internals by name: `*(void**)&ser` -> `ser._stream`; `+0x8C` ->
  `CompressedMemoryStream::_memoryStream._cursor`; `+0x24` -> `MemoryStream::_cursor`; `+8` ->
  `ser._stream->_direction` (ENT's Persist walk, COMP-7).
- `IO::PackageFile::_priority` (+0x70) for introsort.cpp:104.
### R-COLL second visit, additions
- Stale declarations `PortedRotateLeft8` / `Right8` at Collection.h:1198-1199 (bodies deleted by R-IO).
### R-SYS
- `RtSystem_ConfigMap_Find(void*, RtSystem::String*)` (Configuration.h; 8 casts in R-IO files);
  `Configuration` +0x20 is declared `void*` but retail reads a dword (filesystem_setupdirs_win.cpp:397); the
  `RtSystem::String` / `Filename` / `WideString` PODs (about 25 lines in io).
### Other owners
- ACT (Action.h): `ActionStack::field_0x10` (asked of ACT-1). `CharacterComponent` +0x48 is still separate
  fields, not an `ActionStack` (about 45 reader files): wave X.
- GAME-1: `Disease::_stringTable`, `_name` (citizen_updatehealth.cpp:507). DLG-1 (Map.h): `Map::field_0xA2`
  is `bool` but is ORed as a byte (row 311). COMP-2: `CemeteryDescription::_ref20` is `void*`.
- WORK-3 edited `charactercomponent_serialize.cpp` (COMP-2's file): one value-identical overload of
  `SerializeValidatedIBP`.
### Wave X additions
- `SIG_WORK.tsv` (50 rows): the typed signature each cross-file Work body should get.
- CAI-1: the nine action builders' parameters (0x1400c1070, bd070, bd420, bd790, bccb0, bc790, bda50, bb490,
  ba6b0), `BuildCandidateStateList(int64_t*)`, the `int64_t` storage helpers.
- No caller left (delete): `ActChild_RemoveNRCopyCtor_cadf0`, `_AnimateNRCopyCtor_c57d0`,
  `_CreateNRCopyCtor_c6a20`, `_HuntAnimalCopyCtor_c8400`, `_ConsumeRawCtor3_c6440`,
  `_CompleteRoadCopyCtor_c59d0`, `Pf_CreateRawMaterialCopyCtor_71a0`, `Entity_ConsumeListAppend_64b0`,
  `WeightedRandomIndex_119470`, `WorkLeaf_FindComp_63d0` (a real body, no caller), `Pf_QuadtreeInit_18dd0` /
  `Subdivide_18880` (action_pf_geom.cpp:526-540).
- CIT-1: `CitizenEmploy_Strat_*`, `c3d0`, `SeatWorkers`, `SeatWorkersAt` (callers: professionui, townhallui,
  workplaceui, starttown, buildcomponent_state8, harnesses), `NomadComp_FUN_140103480`,
  `Component::OnMessage`'s `void*` data.
- R-IO: the `void*` Array cores in modification.cpp, the `RtSerializer_*` forwarders, the four
  `RtIo_Persistence_*` forwarders (callers in `input/` declare them locally), `ExistsInPackfile` / `CreatePath`.
### Close
- `byte_io` markers: R-IO added 10 (ceiling 17); `cast_ok` total rises with every runtime block: compare all
  categories against FLOOR.md at the close and re-cut the ceilings in writing.
- CAI-1 row 290: `SortByPriority` is a stable insertion sort where the banner says retail is an introsort.
