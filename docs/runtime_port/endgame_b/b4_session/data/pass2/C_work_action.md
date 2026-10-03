# C_work_action blockers (code/Game/Work + code/Game/Action): 1,301 casts, 86 files

Method: every cast classified by snippet + source read at the site + class header check; splits inside a file
are estimates (sum = file count). Totals by family: SIG 606, FWD 248, FIELD 158, LOCAL 153, NOCLASS 81, ERASED 23, H5 11, OTHER 12, POD 8, CONST 2, FLOOR 1.

## Blockers by casts (key, casts, files, header that changes, the change)
1. SIG:Work-set-bodies 108 / 20 files; Work/exe_bodies.h (WorkBST_*, WorkCellBST_*, WorkLeaf_Bst*): root `uintptr_t*`, node `void*` -> typed `Set<..>*` / `Node*` (Node types already in Work.h); then retype each leaf's `_field48/field_58/bstRoot` root member.
2. FIELD:ClearArea::field_48+field_58 82 / 1 (cleararea.cpp); Work/ClearArea.h: `void*` roots -> `WorkEntitySetNode*` / `WorkCellSetNode*`. Node types exist, so this is one header + one file.
3. SIG:Work-slots 63 / 22; Work/Work.h: Execute/OnComplete/OnCancel/GetTarget/Slot5 `void*` citizen/retbuf -> `CitizenComponent**`, `Profession*`. One header edit, 22 leaves follow.
4. SIG:Work-exe_bodies-C-calls 61 / 21; Work/exe_bodies.h WorkAct_*/WorkImpl_*/WorkLeaf_* integer/void* params.
5. SIG:Action-exe_bodies-ibp-uint64 56 / 10; Action/exe_bodies.h: `uint64_t*` InternalBasePtr args of the SendMessage/ActStackPush C bodies (work_act_*.cpp, work_base_actionctor.cpp).
6. SIG:Action-exe_bodies-C-calls 53 / 24, SIG:Pf_PickBestDropTarget_8d590 32 / 7 (0x14008d590: town mgr `int64_t**`, entity returned as integer); Action/exe_bodies.h.
7. SIG:Serialize-stream 45 / 39; Work/Work.h + Action/Action.h `Persist(void*)` (the standard Persist stream ask).
8. NOCLASS:Pf-element-0x14 41 and NOCLASS:HighlightTool-tile-entry-0x60 18 (both action_pf_pools.cpp); element structs of three Array resize bodies (Player tessellation / DecalComponent 0x14 bytes; highlight tool tile entry 0x60 bytes) have no header.
9. SIG:Component-C-int(QueryCount,CalcMaxWorkers,StorageCanAccept) 36 / 21; Component/exe_bodies.h and *_funcs.h: storage/work/position components passed as int64_t.
10. SIG:Work-grid-int-map 31 / 26; Work/exe_bodies.h grid callees (FindCell, CalcDistance, ValidateCell, SortTiles, GetTilesInArea) take `int*` Map -> `Map*`.
11. FWD (248 total, 40 files): the file's own `// B4 FORWARD` C spellings; largest action_pf_avl 25, action_pf_road 14, action_base_rawmat 17 (+7 ActRm-MaterialCountArray), action_base_move 12, astar 12, pf_ctors 12, base_cnr 11. They go when their outside callers are converted, not before.
12. LOCAL:* 153 (see below); SIG:Work-FindComponent-uintptr 27 / 11; SIG:Work-act-ctors 22 / 8; SIG:Action-slots 20 / 16 (Action.h Execute(time)/GetTarget(param)/AbstractImpl: the `*(void**)param` and `*(int64_t*)time` reads).
13. FIELD:InternalPtr-members-declared-as-T* 19 / 12; FIELD:CharacterComponent::field_0x58 16 / 9 (declared `Action*`, needs `InternalPtr<Action>`); SIG:MarketWork-bst-C 19 / 5; NOCLASS:ActMv-path-state-object 21 (action_base_move.cpp: self of c8d20/c8c10).
14. Singles: FIELD:Map::regionPairCache 8 (Game/Map.h int64_t* -> RegionPairCache*), FIELD:WorkSchedulerTree::root 4 (+WorkScheduler::field_0x10/0x120 3) in Game/WorkScheduler.h, SIG:Work-TryAssign-mgr 7, SIG:ValidateTask-out 9, ERASED:hand-rolled-dynarray 13 (buildstructure.cpp; use Collection::Array), H5:CitizenComponent::meshInstance 6, POD:RtObject::Internal 8.

## 10 largest LOCAL files (headers already declare the fields unless noted)
- consumeproduce.cpp 55: ComponentDescription 4, ConsumeProduceDescription 13 (product sub-fields may need names), StorageComponent 10, Map 6, MapCell 8, ModelComponent 7, RawMaterialDescription 4, CitizenBehavior 2
- buildstructure.cpp 26: Map 10, MapCell 6, ModelComponent 4, CharacterComponent 3 (+0x48/+0x60 raw), CompleteTask 2
- fishingwork.cpp 18: Map 6, MapCell 4, CitizenBehavior 4, ModelComponent 3
- clearstorage.cpp 17: ComponentDescription 7, StorageComponent 4, ModelComponent 4, RawMaterialDescription 2
- buildroad.cpp 10; cleararea.cpp 10 (WorkComponent::workState +0x30 ALREADY declared and Work::field_18 is already `WorkComponent*`: the LEFT is stale); firework.cpp 8; action_base_move.cpp 4 (GameUpdateLoop::m_game already `Map*`); action_base_complete.cpp 2 (Work::field_24 is named; LEFT stale); work_leaf_hunt.cpp 2.
- By class: ModelComponent 30 (Math::Transform member exists at +0x148; +0x158/0x15c/0x160 reads need Transform position accessors, otherwise a field list), Map 24, MapCell 20, ComponentDescription 16 (`_subDescs`/`_subDescCount`), StorageComponent 14, ConsumeProduceDescription 13, RawMaterialDescription 9, WorkComponent 7.

## Classes with NO header yet (NOCLASS)
- the 0x14-byte element of Pf_VectorResize14 (Player tessellation / DecalComponent), 0x60-byte highlight tool tile entry
- the self object of ActMv_UpdateFacing_c8d20 / ActMv_PathSetup_c8c10 (+0x20 PathTicket, +0x28 citizen, +0x38/0x3c pos, +0x5c, +0x60/0x64/0x68 path, +0x470 profile); layout is in comments only
- the surface entity read at +0x24 (action_base_cnr.cpp:383)

## Where the brief / first-pass LEFTs are wrong or stale
- `LEFT: Work.h keeps field_18 void*` (cleararea.cpp) and `field_0x24 not named` (action_base_complete.cpp): both are typed now.
- The detail counts double every `(T*)(base + off)` read (the `(uintptr_t)base + off` operand is counted too), so 1 BST child read = 2 casts; ClearArea's 82 are ~41 reads.
- Several `reinterpret_cast<uintptr_t>(x->typedField)` casts exist only because the member is already typed (e.g. `_recipes`, `storageSlots`): the cast is simply removable (LOCAL).
- `(CitizenBehavior*)m_citizenList` (firework.cpp:109) and `reinterpret_cast<Map*>(m_game)` (firework_search.cpp:155) are redundant casts.
- `CitizenBehavior::professionResources` (+0x170) is typed; consumeproduce/fishing read it through `worldEntityList[0x2e]` / `+ 0x170`.
- Keys `FLOOR:ptr_int` (action_pf_road.cpp:700 %llx print), `OTHER:null-receiver-read-spelled-as-address` (work_act_consume 262/296) and the pickup.cpp:531 null-this probe are faithful crash reads: mark CAST-OK, do not "fix".
