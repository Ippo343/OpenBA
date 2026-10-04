# Wave X inventory: XB_work_action_entity (Work/, Action/, Entity/)

607 casts on 564 lines, all classified (tsv has one row per line). Family totals:
SIG 490 (135 functions), OTHER 36, FIELD 25, H5 23, LOCAL 13, FLOOR 8, H8 7, SLOT 4, CONST 1.
Typed signatures: SIG_WORK / SIG_ACT where a row exists, otherwise written from the body
(marked "from body"); `all_caller_files` was computed by a comment- and string-stripped scan of
code runtime standalone dx11 engine tests (local externs and definitions count).

## Largest families (casts, files that must move together)
1. WorkAct_GatherRawMatCtor_bf790  23  (8 files; srcLoc / dstLoc / resource are integer slots)
2. WorkCellBST_Remove_ca60  21 (13 files) and WorkBST_BSTFindRemove_25520 21 (11)
3. WorkBase_ValidateCell_a00f0 20 (30 files): every `int* mapGrid = (int*)map` also feeds
   WorkGrid_FindCell_9ccf0 / WorkBase_CalcDistance_8830 (note column lists the callees):
   type the three together (Map*), one owner.
4. WorkBST_BSTRemove_a6e0 16 (15), WorkBST_RegisterWork_1a30 16 (13), WorkBST_BST_Serialize_296b0 15 (12):
   the same `WorkBST_*` set of six (+ WorkCellBST_*, WorkLeaf_Bst*, PlayerTrade_*, TradeWork_*, Pf_Avl*)
   all take the tree head as uintptr_t*/int64_t*; the owners differ per tree node type, so
   one owner for the whole BST group is cheaper than one per function.
5. CitizenComp_StorageComponent_QueryCount 14 (17 files), WorkComponent_CalcMaxWorkers 13 (20 files):
   one-parameter integer receivers, defined in Component/ and Tutorial/ (other regions' files).
6. OTHER reused scratch slot (known_port_issues 260/row 220) 13+2: not typable, keep as LEFT.
7. Pf_PickBestDropTarget_8d590 11 (4 files), OTHER IO::Serializer stream internals 10
   (componententity.cpp Persist: offsets into the stream, R-IO accessor needed),
   PathBST_Insert_25a10 10 (3 files), WorkGrid_TryAssignWorkplace_f8620 10 (7 files).

## Pure forwarders with ZERO callers left (deletable now; only the declaration, definition and
## include-line comments remain; each carries 1-2 casts)
ActCNR_FindComponent_3f60, ActMvDep_AnimHelper_50f0, ActRm_DynArrayPush4_44810,
ActRm_ArrayUpsert_58fd0, ActRm_PositionEntity_9760, Pf_EnqueueCmd1_a6a30, Pf_EnqueueCmd4_a68b0,
Pf_QuadtreeSubdivide_18880, Pf_QuadtreeInit_18dd0, Pf_RegionDirty4_b150,
Entity_FindConsumeProduce_b6f0, EntityList_RemoveEntity (entitylist_funcs.h; the LEFT says it
serves gameupdateloop_teardown.cpp, which does not call it), WorkLeaf_FindComp_63d0 (a body,
not a forwarder, also uncalled). Delete the declaration in exe_bodies.h with each.
Few callers (1-3 files): ActCNR_CheckTerrain_cb00, ActCNR_SetGrowthProgress_120db0,
ActCNR_NR_*, ActCNR_SetAnimIndex_126440, ActCt_HideTool_1176e0, ActMvDep_TimeCheck_a9290.

## LEFT comments the code contradicts (stale; these casts are LOCAL, no outside change needed)
- "Game/WorkScheduler.h root / field_0x10 / field_0x120 void* (COMP-8)": the header now types
  them (WorkSchedulerIdNode*, WorkSchedulerCountNode*, WorkSchedulerBucketNode*). 7 casts in
  action_base_complete.cpp:103/212/235, action_base_move.cpp:351, work_base_grid.cpp:82/145,
  pickup.cpp:221 only convert to a file-local node struct: use the header's node types.
- "Map.h regionPairCache is int64_t*": it is `RegionPairCache*` (Map.h:115). buildstructure.cpp:428,460.
- "EditTool.h lastEntityRef is uint64_t": it is ComponentEntity* (EditTool.h:64), and the +0x40 read in
  entitylist.cpp:749 is ComponentEntity::_entityFlags. 
- "CharacterComponent::field_0x58 declared Action*" (build_actions.cpp:120) and
  "ActionStack +0x10 declared Action*" (work_base_reimpl.cpp:208): both are InternalPtr<Action> now.
- "UserInterface.h DialogInstancePtr is void*": `typedef DialogInstance* DialogInstancePtr`
  (entitylist.cpp:697).
- action_pf_roots_drop.cpp:197 `PickBestDropTarget(reinterpret_cast<const RawArray<...>*>(&...field_0x00)`:
  CitizenBehavior.h still declares field_0x00 as ComponentEntity** (H5, correct), but the 16 lists
  are one family with the other callers (H5:CitizenBehavior::lists, 7 casts).
Other notes. Genuine floors found: work_act_consume.cpp:267/301 null-receiver reads (crash_walk, retail's
  unguarded deref, keep), performwork.cpp:284/285 float_bits, work_base_walkpos.cpp:164 ptr_int,
  _handle byte stores (byte_io), pickup.cpp:523 freed-self vptr probe (vslot).
- Four H8 groups (BuildStructure::field_48, ClearArea::field_70-72, ConsumeProduce::field_48,
  ExternalPtrSerialize present flag) must stay; 1 CONST:FindOrLoad (action_base_move.cpp:213).
- Name collision: `FindComponentByTypeId` is a free inline (Entity/exe_bodies.h:117) AND a static
  in Component/citizen_lifeevent_handler.cpp; callers column counts only the free function's spelling.
- SLOT:Action::* callers column is the union of the AbstractImpl / Execute / Enter / Exit
  overrider and call files (slots [4]-[7] must be retyped together; ~40 files).
