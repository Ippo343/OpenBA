# XA_component inventory (code/Game/Component, 632 casts, 586 lines, 117 files)

Classes (casts): SIG 419 (174 families), ERASED 86 (7), SLOT 28 (5), FIELD 39, H5 32, OTHER 11,
LOCAL 5, H8 4, FLOOR 4, POD 3, CONST 1. Tables: XA_component.tsv (per cast line),
XA_component_families.tsv (SIG/SLOT/ERASED/POD, all_caller_files = whole tree, regex-built on
comment/string-stripped sources, includes declarations and local externs).

## Families by casts (SIG/SLOT/ERASED first; the owner of each is the file that DEFINES it)
- 20 ERASED AmbientEmitterResize_Reimpl; 20 ERASED AmbientArrayResize_Reimpl; 20 ERASED
  StorageComponent_DynArrayResize (4 element types); 14 ERASED ToolbarDescription_StringArrayResize;
  5 ERASED CemeteryListErase_Reimpl (19 caller files: the widest of the five).
  All hand-expanded Array<T>::SetSize / erase bodies whose C signature is void*: ONE owner per
  function, callers are 2 / 2 / 5 / 3 / 19 files.
- 20 SIG CitizenAI_WorkScheduler_Post(uintptr_t workComp): 14 casts-files, 15 files in all (every
  controller state that posts work). Typed form is `WorkComponent*` (body-derived).
- 13 SLOT Component::Update(void* time) (+2 Tick, 1 OnMessage): ~100 files override or call it
  (Component.h:60-67). One header edit + every override; keep as its own owner.
- 10 WorkBase_ValidateCell_a00f0 (typed in SIG_WORK as Map::CanPathTo), 9 CitizenComp_StorageComponent_QueryCount,
  9 CitizenComp_Entity_impl_ef40, 9 CitizenAI_WorkScheduler_Rekey, 8 CitizenList_AssignWorker_c3d0,
  8 CitizenAI_FindBestTarget_dd30, 8 fn_CitizenComponent_HasVacancy, 7 CitizenAI_Pathfinding_NavigateTo_bda50,
  7 CitizenComp_Storage_GetResourceCount, 7 WorkComponent_CalcMaxWorkers, 7 ResidenceComponent_impl_7230.
- 6 SLOT Controller::Tick(void* time) and 5 SLOT Controller::StateHandler(void* sm): the table in
  controller_state_table.h; ~55 files include it.
- Non-SIG: 7 H5 WorldGeometry::_arrays (decal + boat), 6 FIELD CitizenAIComponent::timers (int64_t Time),
  5 FIELD StartCondition (no class, starttown.cpp), H5 CitizenComponent::{home,spouse,...} IBP members.
- 79 SIG families (135 casts) are pure/guarded B4 FORWARDERS whose receiver cast is the only cast left.

## Forwarders with ZERO callers left in the tree (deletable now, with the exe_bodies.h declaration)
CitizenComp_sub_1fa0, CitizenComp_helper_1b90, WorkPlaceComponent_EnterBuilderProfession,
PastureComponent_ComputeCapacity, CitizenComp_FUN_14012c3f0, Component_SerializeReimpl
(Component.h:102, one-line Persist forwarder). Not forwarders but also uncalled: ConsumeProduce_RecipeLabelAt,
ModelComponent_RandomizeVariant, CitizenEmploy_SeatWorkersAt (only a dead local extern in
buildcomponent_state8.cpp:77). Retail uses of the 0x-addresses outside the tree are not checkable here.

## LEFT comments the code contradicts
- "B4 FORWARD: 1 outside caller (standalone/death_harness.cpp)" on CitizenComp_sub_1fa0: that file does not
  name it. Same for the claims on WorkPlaceComponent_EnterBuilderProfession (workplacecomponent.cpp,
  tool_harness.cpp), PastureComponent_ComputeCapacity (Dialog/debugui.cpp), CitizenComp_FUN_14012c3f0
  ("outside callers in 16 files" in exe_bodies.h:211; destroycomponent_states.cpp), ModelComponent_RandomizeVariant
  (starttown.cpp), ConsumeProduce_RecipeLabelAt (tavern_stock_probe.cpp does not exist).
- tornadocomponent.cpp:500/536 "dead first argument" is a `static` function used in one file: LOCAL, not a
  signature family (Tornado_StrikeEntityCounted).
- statusiconcomponent.cpp:63 / citizenai_growup.cpp:184 / destroycomponent_states.cpp:210 read a vptr: tree-only
  guards (known_port_issues rows), classed FLOOR:vslot; the statusicon one is an always-true guard.
- keystone_a_map.cpp:393 and mapcomponent.cpp:90 "nothing calls this body": TRUE (CreateNaturalResource_impl_4750
  has no caller), so CreateNaturalResource_impl_dee0 is reachable only from that dead body.
- ClearAreaComponent_HarnessFind: `defined in clearareacomponent.cpp:157`, declared in standalone/tool_harness.cpp:131.

## Notes for owners
- Typed signatures come from SIG_WORK/SIG_ACT.tsv where the name matches; otherwise "body-derived" (from the
  cast of each parameter) or hand-written in the table; the MeshInstance / DecalMaterial / CandidateState
  names are inferred, confirm in the header before use.
- GameColl::AsArray / ArrayAt (coll_bridge.h, 111 / 4 files) are bridges that vanish when the raw-triple H5
  members become Array<T>; they are not a unit of work by themselves.
- ExternalPtrSerializeGeneric / Reimpl_ExternalPtrArray_Serialize are defined in Game/serialize_externals*.cpp
  (outside this region); 9 and 8 caller files.
