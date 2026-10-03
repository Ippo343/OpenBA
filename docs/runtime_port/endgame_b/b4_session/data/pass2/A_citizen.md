# A_citizen (code/Game/Component/citizen*) - blockers, 1,956 casts / 29 files

Method: function-by-function ranges + line patterns (estimates; counts sum to the file totals). TSV = per (file, blocker).

## Group totals
LOCAL 640 | SIG on file-local helpers 539 | SIG declared in Component/exe_bodies.h 269 | NOCLASS 195 | SIG extern "C" with no header 60 |
POD:RtObject::Internal 61 | OTHER 86 | FIELD 41 | FWD 22 | SIG Serialize/slots 21 | ERASED 11 | FLOOR 9 | H5 2

## Blockers by casts (key, casts, files, header that changes, change)
- `LOCAL:WorkScheduler` 220 (3f) -> code/Game/Game/WorkScheduler.h + WorkSchedulerNodes.h: node structs (Id 0x30, Count 0x28, Bucket 0x38) already declared; type the walkers
- `LOCAL:GameUpdateLoop` 122 (20f) -> code/Game/Game/GameUpdateLoop.h: m_game/m_citizenList/m_entityList/m_random etc already typed; replace +0xF0/+0xF8 offset r
- `NOCLASS:WorkCandidateTree node (0x30)` 95 (1f) -> code/Game/Game/WorkSchedulerNodes.h (new struct): node {Work* +0, int cost +8, parent +0x10, left +0x18, right +0x20, s16 bal +0x28/ht +0x2A
- `LOCAL:ModelComponent` 86 (12f) -> code/Game/Component/ModelComponent.h: transform(+0x148)/yaw(+0x168) declared; translation +0x158/15C/160 is inside Math::Transfo
- `LOCAL:CitizenBehavior` 75 (2f) -> code/Game/Entity/CitizenBehavior.h: citizens(+0xA0)/count(+0xA8), field_0x20 residences, attendance already declared; type the
- `NOCLASS:LeisureCandidateSet node (0x28)` 63 (1f) -> code/Game/Component/citizenai_leisure.cpp (new struct): 0x28 AVL-ish node via NodeAlloc, 3 pointers + int; layout to confirm vs WorkSchedulerCount
- `POD:RtObject::Internal` 61 (10f) -> runtime/types (Object::InternalBasePtr ctor): InternalBasePtr(reinterpret_cast<RtObject::Internal*>(obj)) bridge; needs a typed ctor fro
- `LOCAL:StorageComponent` 57 (3f) -> code/Game/Component/StorageComponent.h: storageSlots(+0x28)/slotCount(+0x30) declared; row stride 0x10 {data,count,cap} needs an I
- `OTHER:IBP-held-as-int64` 54 (2f) -> code/Game/Component/citizenai_actionctors.cpp: InternalBasePtr locals kept as int64_t/uint64_t and passed as uint64_t* to Action ctors
- `SIG:CitizenAI_sub_78c0` 52 (1f) -> code/Game/Component/exe_bodies.h: int64_t/void* param(s) declared here; retype to the class and update callers
- `SIG:CharacterComponent_MoveTick_1094c0` 41 (1f) -> (extern "C" with outside callers; no header): void*/int64_t param; callers in other modules declare local externs
- `SIG:CitizenAIComponent::BuildCandidateStateList` 38 (1f) -> code/Game/Component/CitizenAIComponent.h: BuildCandidateStateList(int64_t* outList,...): the list is a raw Array
- `NOCLASS:QuadtreeWalker records (0x24)` 37 (1f) -> code/Game/Component/exe_bodies.h / Map_RadiusQuery_92990: 0x2430 walker, 0xFF records of 0x24 {pos,ext,nodeId}; Map_RadiusQuery_92990 takes float* m
- `SIG:PathGraph_ReassignWorkers` 33 (1f) -> code/Game/Component/exe_bodies.h: int64_t/void* param(s) declared here; retype to the class and update callers
- `SIG:Entity_impl_ec70` 33 (1f) -> code/Game/Component/exe_bodies.h: int64_t/void* param(s) declared here; retype to the class and update callers
- `SIG:Entity_PlaceAtCell_7b20` 31 (1f) -> (file-local static / extern "C" in the .cpp): int64_t/void* param of an in-file helper; retype params
- `OTHER:Time-local-held-as-int64` 30 (9f) -> code/Game/Component/*.cpp (locals): System::Time locals declared as int64_t/uint64_t, then cast for the Time ctor / arithmetic
- `SIG:CitizenAI_ActionExecutor_c4c50` 29 (1f) -> code/Game/Component/exe_bodies.h: int64_t/void* param(s) declared here; retype to the class and update callers
- `SIG:State-callee-ints` 29 (2f) -> (file-local static / extern "C" in the .cpp): int64_t/void* param of an in-file helper; retype params
- `SIG:CitizenAI_State_Leisure_sub_fd50` 29 (1f) -> (file-local static / extern "C" in the .cpp): int64_t/void* param of an in-file helper; retype params
- `LOCAL:Map` 28 (2f) -> code/Game/Game/Map.h: Map::cells + MapCell(0x30) declared; +0x26/+0x29 flags fields exist in MapCell
- (blockers below 14 casts each: 78 keys, 379 casts; see TSV)

## Cross-module / shared blockers worth one agent each
- `LOCAL:WorkScheduler` 220 (208 in citizenai_work_scheduler.cpp): the three node structs and typed WorkScheduler fields already exist; the file is the
  scheduler insert/erase AVL port, nothing outside blocks it. Only `CitizenAI_WorkScheduler_Insert(int* sched, void* pWorkIBP)` (declared Component/exe_bodies.h,
  caller Player/roadtool_calcpath.cpp) is an outside SIG (11 casts) and Work::GetTarget buffers (2).
- `NOCLASS:WorkCandidateTree node (0x30)` 95 (housing_idle Helper_e360/8000/7c90/7b60): node is {Work* +0, int cost +8, parent +0x10, left +0x18, right +0x20,
  s16 bal +0x28, ht +0x2A}; NOT WorkSchedulerIdNode (key at +0). Needs one new struct; callers in action_pf_avl.cpp (Pf_AvlRotate*) share it.
- `NOCLASS:LeisureCandidateSet node (0x28)` 63 (State_Leisure inline tree insert/erase): same family as above or WorkSchedulerCountNode; decide once.
- `NOCLASS:QuadtreeWalker records (0x24)` 37 (Map_RadiusQuery_92990, declared Component/exe_bodies.h, 2nd caller tornadocomponent.cpp).
- `FIELD:CitizenBehavior::field_0x100` 25 (14 + the citizencomponent/assign_citizens lookups): void* root of the profession tree; needs a 0x38 node struct.
- `POD:RtObject::Internal` 61: `Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(obj))`; one typed ctor/helper removes all (10 files).
- `OTHER:IBP-held-as-int64` 54 + `OTHER:Time-local-held-as-int64` 30: locals declared int64_t/uint64_t; retype locals to InternalBasePtr / System::Time.
- Exe-declared SIGs (Component/exe_bodies.h): CitizenAI_sub_78c0 52, Entity_impl_ec70 33, PathGraph_ReassignWorkers 33, CitizenAssignCitizens 27,
  CitizenAI_ActionExecutor_c4c50 29, CitizenComp_Entity_impl_ef40 20, Storage_GetResourceCount* 36, CitizenAI_PerformWorkCtor* 15: retype int64_t/void* params; callers outside the block need updating.
- extern "C" with outside callers and no header: Entity_PlaceAtCell_7b20 31, CharacterComponent_MoveTick_1094c0 41, Cemetery_* 14, TownRegistry_RegisterResidence 3,
  CitizenEmploy_SeatWorkersAt 2 (callers in buildcomponent_state8, residencecomponent, starttown declare local externs).

## 10 largest LOCAL files (LOCAL casts)
- 208 citizenai_work_scheduler.cpp: WorkScheduler 207, GameUpdateLoop 1
- 167 citizencomponent.cpp: CitizenBehavior 73, GameUpdateLoop 24, CitizenComponent 22, ModelComponent 19
- 46 citizenai_growup.cpp: StorageComponent 39, ModelComponent 4, GameUpdateLoop 3
- 27 citizen_lifeevent_handler.cpp: GameUpdateLoop 14, ModelComponent 7, CemeteryComponent 6
- 24 citizenai_executor.cpp: GameUpdateLoop 13, ModelComponent 11
- 23 citizenai_housing_idle.cpp: GameUpdateLoop 9, WorkScheduler 8, ProfessionList 4, ModelComponent 2
- 23 citizenai_leisure.cpp: Map 16, GameUpdateLoop 4, ModelComponent 3
- 19 citizen_updatehealth.cpp: ModelComponent 7, CharacterGrid 6, GameUpdateLoop 4, Disease 2
- 18 citizenai_handlers2.cpp: ModelComponent 10, GameUpdateLoop 8
- 14 citizenai_handlers3.cpp: ModelComponent 9, GameUpdateLoop 5

## Classes with NO header yet
- WorkCandidateTree node (0x30), LeisureCandidateSet node (0x28), ProfessionJobNode (0x38, CitizenBehavior +0x100 tree), QuadtreeWalker record (0x24),
  CandidateStateList (Array {int id,int prio} behind BuildCandidateStateList int64_t* outList). Everything else the region reads has a header.

## Where the brief is wrong / stale
- The `LEFT: GameUpdateLoop::m_citizenList / m_game is void*` comments (~50 in other regions) are STALE: GameUpdateLoop.h now types m_citizenList (CitizenBehavior*),
  m_game (Map*), m_entityList, m_random, m_professionList, m_tutorial (WorkScheduler*). Those casts are LOCAL, not FIELD.
- No `A_citizen.left.txt` exists (slice has no LEFT comments); classification came from reading the code.
- Most of this region is "SIG on file-local statics" (539): int64_t/void* params of in-file helpers, not outside blockers. They behave like LOCAL.
- Heuristic split: counts inside one function are +-10%; function default applies where no line pattern matched.
