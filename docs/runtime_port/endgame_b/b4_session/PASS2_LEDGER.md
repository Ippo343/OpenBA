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
