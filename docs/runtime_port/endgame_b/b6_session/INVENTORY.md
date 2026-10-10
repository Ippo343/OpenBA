# B6 Block 0 — the retail-difference inventory

Written 2026-10-09 by Block 0 (read-only). Code read at main `338120596` (the B6 kit commit `9384298d5`
changed docs only). Retail read in Ghidra (`Application-x64.exe`, `Runtime-x64.dll`); every address
quoted under "read by B0" was disassembled in this session. Line numbers are today's: `leftfree_c.py`
regenerated at `338120596` gives the same 63 lines as `LEFT_FREE.md` §C except `userinterface.cpp`
1225 / 1250, which are now **1224 / 1249**.

Bucket = the NATURE of the fix (14a wrong callee / object / overload, 14b serializer bytes or load
path, 14c construction / destruction, 14d a guard or arm retail lacks, 14e normalising store), not the
section the row happens to be filed in (643 sits in the 14c table, 280 / 391 / 470 in the 14d table,
810 / 820 / 850 / 860 / 861 in the 14f table).

Verdicts: **ONE-LINE** (local fix, under ~10 lines), **RE-READ** (retail must be read further, says
what), **SAVE-FORMAT** (changes what a Serialize / Persist writes or how a load reconciles: coordinator,
serial, D3), **BLOCKED** (says by what), **DEAD-CODE** (section B only: the arm is unreachable today;
D5 still fixes it).

---

## A. The 63 retail-difference casts, grouped by fix unit

### A.1 14a — wrong callee / object / overload (41 casts)

| file:line | kpi row | bucket | retail | the tree (read at the line) | retail does | save? | verdict | fix touches |
|---|---|---|---|---|---|---|---|---|
| `src/game/Component/citizenaicomponent.cpp:436` (x2), `:439` | 1 | 14a | 0x14010f7a0 (finds at 0x14010f7e1 / 0x14010f830) | `CheckEducation`: `res8` is the Residence finder's (0x140008090) result; +0x38 read / written raw inside ResidenceComponent's embedded controller; :439 hands a ResidenceComponent to `CitizenComp_StorageComponent_QueryCount` as a StorageComponent; also bails on `profList == nullptr` or `res8 == nullptr` | calls the StorageComponent finder 0x140006270 and reads / writes StorageComponent +0x38 (capacity; the object is 0x40 bytes, factory 0x140038b50) | no | ONE-LINE (finder swap, `storage->capacity` by name, QueryCount on the storage; the two null bails are 14d and go with it per D4) | `citizenaicomponent.cpp` (also hosts row 551's duplicate QueryCount) |
| `src/game/Action/action_base_complete.cpp:333` | 45 (tag says "entry 14", no row number) | 14a | 0x1401173b0 | `ShowCitizenTool` toolId < 0: reads the qword at ModelDescription +0x28 (`_meshGroupCount`/`_meshGroupCap`) as a pointer and dereferences it once | `*(*(desc+0x20)+8)` and one more dereference: mesh 0 of mesh group 0 | no | ONE-LINE (`_meshGroups[0].meshes[0]` by name) | `action_base_complete.cpp` |
| `src/game/Action/performwork.cpp:271`, `:272` | 51 | 14a | 0x1400c9720 (0x1400c9808 / 0x1400c9814 `Time::AsSeconds`, IAT 0x140151318; `Time(float)` 0x140152b00) | tool-less time scaling reads the low dword of `requiredTime` and of `scaleTime` as float bits and multiplies | `AsSeconds()` on both, multiply, `Time(float)` | no | ONE-LINE. OBSERVABLE: tool-less work durations change (today a product of two tick-count bit patterns) | `performwork.cpp` |
| `src/game/Work/cleararea.cpp:700`, `:817` | 220 | 14a | 0x1400e5b10 (0x1400e5c25-0x1400e5c34) | `ClearArea::Validate` collects each invalid node's CITIZEN pointer, then compares those with the set's ENTITY keys | collects the node's KEY (`MOV RAX,[RAX]; MOV [R8],RAX`), phase 3 removes each invalid entity (0x14001ca60) | no | ONE-LINE. OBSERVABLE: invalid entries are actually removed | `cleararea.cpp` (also row 135's guard) |
| `src/game/Work/consumeproduce.cpp:478`, `:494`, `:495`, `:496`, `:497`, `:522` | 260 | 14a | 0x1400e7840 (the `local_58 == 0` arm, jump target 0x1400e837c) | no-StorageLocationComponent arm: the 8-byte `uintptr_t foundEntity` is the float[3] out of InteractNodePosition (12 bytes into 8), copied to `terrainPos`, then the loop goes to the NEXT input; :522 hands the same slot as the entity | out is a 16-byte region (`local_d8`/`local_d0`), copied to `local_a0`/`local_98`, then JUMPS to the work-action setup the StorageLocation arm reaches | no | RE-READ: the evidence is a decompile only; read the disassembly of the arm and of 0x1400e837c (what the setup consumes from `local_a0`/`local_98` and which entity it uses) before the control-flow change. OBSERVABLE: a building without a StorageLocationComponent gets a work action where today it gets none | `consumeproduce.cpp` |
| `src/game/Component/growthcomponent.cpp:243` (x2), `:266`, `:268` | 400 + 550 (one defect, D1) | 14a | 0x14011fba0 (finds 0x14011fd1b, 0x14011fde3, 0x14011fe09 = 0x140003f60 FindComponent<ModelComponent>, cell 0x1401c7a50) | `SpawnSubResource` finds the parent's and child's component with the ParticleComponent finder 0x140005200 and reads +0x164 / hands it to `ModelMeshCount` / `ActCNR_NR_SetVariant_1253d0` as a ModelComponent | the ModelComponent finder for all three | no | ONE-LINE (`FindComponent<ModelComponent>()`, `variantSeed` by name). DEAD today: `SpawnSubResource` is never reached (known_port_issues entry 3) | `growthcomponent.cpp` |
| `src/game/Component/naturalresourcecomponent.cpp:51`, `:52` | 552 (+ row 5 for the slot) | 14a | 0x140125c40 (0x140125c60 calls 0x140003f60) | `NaturalResourceComponent_OnRemove` finds a ParticleComponent (0x48 bytes) and reads its +0x28 as a MeshInstance | ModelComponent finder, ModelComponent +0x28 MeshInstance | no | ONE-LINE (finder swap, `model->meshInstance`). DEAD today: no caller; slot 8 is a PORT GAP. Installing slot 8 (row 5) is a SEPARATE live change (every removed natural resource then runs it): user decision | `naturalresourcecomponent.cpp` |
| `src/game/Component/tornadocomponent.cpp:910` (x2) | 430 | 14a | 0x140057cd0 (0x140057f30-0x140057f3f) | reads the float at `emitters._data + 0x7c` (bytes of the emitter ARRAY) | `emitters[0].instance->_opacity` (Geometry +0x7c) | no | ONE-LINE. DEAD today: feeds only the PORT GAP 0x140002280 audio branch | `tornadocomponent.cpp` |
| `src/game/Dialog/residenceui.cpp:465`, `:468`, `:469`, `:474`, `:475` (x2), `:479`; `src/game/Entity/entity_findcomponent.cpp:100` | 452 | 14a | 0x1400b56a0 (from `ResidenceUI_Slot9` 0x1400b5843: element event 0x12, id 300 + n); finder 0x140006270 | `GoToCitizen` finds the entity's component through `fn_FindComponent_Storage` and walks `+0x58` count / `+0x50` family array raw; the integer overload of the finder exists only for this | **CONFIRMED by B0 — the tree is what retail does.** 0x140006270 compares `Type()` with `DWORD_1401c7e78` (0x1400062b3) and narrows with IsDerivedFrom (0x14000630c). The cell's only writer is `StorageComponent_RegisterType` 0x14014fb10: `LEA RCX,[0x1401c7e78]` (0x14014fb35), name string 0x140164708 = "StorageComponent", parent 0x140158a00 = "Component", factory 0x140038b50 = `MemAlloc(0x40, 8)` + StorageComponent ctor. ResidenceComponent is a different class: cell 0x1401c7cf0 (`ResidenceComponent_RegisterType` 0x14014f7d0), parent "ControllerComponent", factory 0x140037440 = `MemAlloc(0x60, 8)`. So retail 0x1400b56a0 reads `[storage+0x58]` / `[storage+0x50]` past the end of a 0x40-byte StorageComponent: a RETAIL over-read (OnUpdateLayout 0x1400b51f0 walks the same rows correctly through ResidenceComponent) | no | BLOCKED: user ruling. D7's premise does not hold (the cell is StorageComponent's, not a misnamed ResidenceComponent cell). Either (a) close as NOT A DIFFERENCE (D6), record the over-read in `docs/retail_bugs.md`, and keep the raw read with a ruled tag; or (b) deliberately deviate and read `FindComponent<ResidenceComponent>()` as OnUpdateLayout does. Nothing "toward retail" exists to fix | `residenceui.cpp`, `entity_findcomponent.cpp` (the `uintptr_t*` overload dies with (b)) |
| `src/game/Dialog/toolbardialog.cpp:1658`, `:1659`, `:1660` | 690 (settles 664) + 452 | 14a | 0x1400649d0 (the 0x227 arm of 0x1400650f0; 0x140064a61-0x140064a6d) | `Toolbar_CenterOnEntity_Impl(void*, void*, void*)` forwards three untyped pointers to `PlayerToolManager::CenterOnEntity`; citizenui's at-school arm hands `school->_entity` as the DIALOG and the AttendanceComponent as the ENTITY; residenceui hands the entity word of its raw read | RDX = &(this+0x18) (the dialog), R8 = &(school+0x10) (the school's entity), as the workplace arm | no | BLOCKED for the forwarder by 452 (residenceui is its last raw caller); the citizenui half is ONE-LINE now (pass `&_dialog`, `&school->_entity`). The forwarder and its 3 casts go when 452 is ruled and residenceui calls `CenterOnEntity` typed | `citizenui.cpp`, `toolbardialog.cpp`, `residenceui.cpp`, the other forwarder callers (entityui, townhallui, workplaceui) retyped to the member |
| `src/game/Component/statemachinecomponent.cpp:250` | 510 + 392 (one defect, D1) | 14a | `StateMachine::Tick` 0x1400a9660; callers read by B0: ForceSMState 0x14011e7b0 (0x14011e83f-0x14011e85c settle tick: `Time(0.0)` at [RSP+0x20] via 0x140152b00, `LEA RDX,[RSP+0x20]`; the INLINED tail 0x14011e883-0x14011e9c7 hands node slot +0x30 R8 = &Time(0) at [RSP+0x50]); trade sites 0x14005a7c7 (`LEA RDX,[RSP+0x40]`) and 0x14005aa13 (`LEA RDX,[RSP+0x38]`), both `R8D = R9D = 0` | `StateMachine_TickReimpl(sm, int64_t time, …)` reinterprets the tick VALUE of a `Time(0)` (or a literal 0) as the `const Time*`: the nodes get NULL | every caller passes the ADDRESS of a stack `Time(0.0f)` | no | ONE-LINE per caller (`sm->Tick(&t0, …)`); delete the forwarder. RE-READ one item: the tree's `TradeCtrl_Event0A_Dismissed` ticks with param4 = 1 after `RequestStateTransition(sm, 2)`; neither trade site read (0x14005a7c7, 0x14005aa13) has a tail tick, and the Dismissed handler's own retail address was not identified | `statemachinecomponent.cpp`, `controller_state_table.h`, `firecomponent_smstate.cpp`, `tradecomponent_states.cpp` |
| `src/game/Component/huntcomponent.cpp:63` | 511 | 14a | 0x140122740 (0x140122753 keeps RDX in RSI; 0x1401227c8 `MOV RCX,RSI; CALL [0x140151318]`) | reads the BITS of the `const Time*` as the elapsed tick count | `time->AsSeconds()` | no | ONE-LINE. OBSERVABLE: hunting progress per tick changes (today a pointer value times `_secondsPerTick`) | `huntcomponent.cpp` |
| `src/game/Action/action_pf_road.cpp:955` | 643 | 14a (filed in 14c) | 0x1400e6c50 (0x1400e6c8b `CALL 0x140002ed0` FindComponent<InteractComponent>; 0x1400e7103 / 0x1400e7118 hands it to 0x140122e20) | `Pf_EntityResolvePos_2e20(void*, …)` casts its receiver to `InteractComponent*`. Callers: `citizenai_findhelpers.cpp:251` / `:442` and `citizenai_leisure.cpp:253` pass the RIGHT object (the InteractComponent found by a type-id scan, held as `Component*`: typing only); `clearstorage.cpp:202` passes the BuildComponent from `fn_FindComponent_Position` (0x14000d6c0): the difference | ClearStorage::Execute uses the InteractComponent | no | ONE-LINE: clearstorage finds `FindComponent<InteractComponent>()`; the three scan callers `static_cast` (or call `InteractNodePosition` directly); the forwarder is deleted. Note clearstorage's `if (positionComp == nullptr) return 1` (14d) | `clearstorage.cpp`, `action_pf_road.cpp`, `citizenai_findhelpers.cpp`, `citizenai_leisure.cpp`, `Action/exe_bodies.h` |
| `src/game/Component/citizenai_leisure.cpp:461` | 704 | 14a | 0x14010a560: header {root [RBP-0x79], count [RBP-0x71]} zeroed at 0x14010a846 / 0x14010a84a; `LEA RCX,[RBP-0x79]; CALL 0x140021a30` at 0x14010a935 / 0x14010a939, next instruction 0x14010a93e `INC EBX` (no count store); 0x140021a30 does `INC dword [RSI+0x8]` (0x140021b20, RSI = RCX) | **upgraded SUSPECTED -> CONFIRMED by B0.** Two separate locals `LeisureHomeNode* local_d8; uint32_t local_d0`; `&local_d8` goes to the 8-byte-key insert, whose `set->_count += 1` (`work_base_bst.cpp:193`) writes 4 bytes at `&local_d8 + 8` (whatever local the compiler put there), then the tree adds 1 to `local_d0` by hand — also on a DUPLICATE key (the insert returns early without counting) | one {root, count} record; the count is the insert's own increment, so it equals the number of distinct homes. The 13x13 cell scan meets the same building on several cells, so the tree over-counts and `Rand(local_d0)` draws past the node count | no | ONE-LINE (one `AvlTreeHead`/Set local, the hand increment at :462 removed; the inline-insert arms at :477-:504 keep their own `count + 1`, retail's 0x14010aa07-0x14010aa0c). OBSERVABLE: leisure home choice | `citizenai_leisure.cpp` |
| `src/game/Component/citizencomponent.cpp:2412` | 705 | 14a (with a 14d guard) | 0x140114050 (0x1401140a5-0x1401140c1 IsDerivedFrom [0x1401c7190] + CMOVNZ; 0x1401140c8 / 0x1401140ce unguarded reads) | `Startup` takes `_description` as the CitizenDescription without IsDerivedFrom and reads health / happiness only `if (description)`; also returns early when `_entity` is null | `IsDerivedFrom ? +0x18 : 0` into +0x288, then reads `[RCX+0x6c]` / `[RCX+0xa4]` unguarded; never tests +0x10 | no | ONE-LINE, after a D4(a) ruling: the comment's reason is "IsDerivedFrom unreliable in standalone's partial factory tree" (STANDALONE-SPECIFIC as written; probably stale since B.3/B.4 registered every type — check that `CitizenDescription::TypeId()` resolves before dropping) | `citizencomponent.cpp` |
| `src/game/Work/gatherrawmaterials.cpp:363` | 861 | 14a | 0x1400ebe40 (R8 spilled to [RBP+0x77] at entry; the find's out slot is [RBP+0x7f], LEA 0x1400ebee4; the call 0x1400ec361 takes arg 5 from [RBP+0x77], 0x1400ec355 / 0x1400ec35c) | hands the WorkPlaceComponent find's out slot (`param3`) to `StepProgress` as the `const Time* now` | hands Execute's own `context` | no | ONE-LINE. OBSERVABLE: the gather time budget (today the WorkPlaceComponent's vptr read as ticks) | `gatherrawmaterials.cpp` |
| `src/game/Game/starttown.cpp:1814` (x2) | 95 | 14a | State1Enter 0x140108290: `MOV dword [RCX+0x4c],1` (0x1401082bd); finder 0x140004740 (cell 0x1401c7ac0, written only by `MultiObjectComponent_RegisterType` 0x14014f475) at 0x1401082d6 / 0x1401082f3; `MOV ECX,[RAX+0x58]; MOV [R14+0x4c],ECX` (0x140108307 / 0x14010830a) | **upgraded SUSPECTED -> CONFIRMED by B0.** The `!StartTown_InstantBuildPath()` arm stamps `costMultiplier` from the low dword of `WorkPlaceComponent` +0x58 (the roster's data pointer) when positive | multiplier = 1, overwritten UNCONDITIONALLY (no > 0 test) by `MultiObjectComponent` +0x58 (`multiCount`, `MultiObjectComponent.h:72`) when the entity has one | no | ONE-LINE. DEAD today (`StartTown_InstantBuildPath()` returns true, `starttown.cpp:971`); the whole arm is a retired stand-in for State1Enter, so deleting it is the other honest fix (coordinator's pick) | `starttown.cpp` |

### A.2 14b — serializer bytes / load path (8 casts; coordinator, serial, D3)

| file:line | kpi row | bucket | retail | the tree | retail does | save? | verdict | fix touches |
|---|---|---|---|---|---|---|---|---|
| `src/game/Game/citizenlist_serialize.cpp:604` | 13 | 14b | 0x14008f760 (0x14008ff9d `Time::Serialize(piVar7 + 4)`, piVar7 = entry + 8) | `ProfessionResourceEntry`'s Time is serialized from +0x14 (`_field14`, an `int64_t`) | serializes the Time at +0x18 | YES (layout of the entry; resave expected to DIFF unless the two words agree) | SAVE-FORMAT | `citizenlist_serialize.cpp`, `Entity/CitizenBehavior.h` (entry layout) |
| `src/game/Game/gameupdateloop_serialize.cpp:178` | 16 | 14b | the uint overload, 0x1400952c0 (the site's quote) | `EventLog` entry colour through `Serialize(int&)` | `Serialize(unsigned int&)` | yes (overload; binary bytes identical, D9: PASS expected) | SAVE-FORMAT | `gameupdateloop_serialize.cpp`, `Game/EventLog.h` (`color` type) |
| `src/game/Component/spawncomponent.cpp:366` | 17 | 14b | 0x140055ee0 | reads the STREAM as a Serializer and calls `GetDirection()` through its vtable word, so the load-time reconcile never runs | tests the stream's direction and reconciles on every load | yes (the bytes written are unchanged, but a loaded SpawnComponent's state changes, so a resave may DIFF) | SAVE-FORMAT | `spawncomponent.cpp` |
| `src/game/Game/citizenlist_serialize.cpp:230` | 15 + 654 (one defect) | 14b (load-only) | 0x14008f760: after the +0x160 serialize (0x1400901ec-0x1400901f8 / the < 2 arm) control goes straight to the +0xe0 wells array (0x1400901fd): NO version test there. The version-5 clamp is 0x1400903cc-0x1400903e0: `GetVersion; CMP EAX,5; JNZ; LEA RCX,[RDI+0x140]; XOR EDX,EDX; CALL 0x140017640` | **upgraded SUSPECTED -> CONFIRMED by B0, with a correction.** The tree has TWO version-5 clamps: `:553-557` `ResizeStride0x10(&field_0x160._root, 0)` (the cast: the +0x160 Set header resized as an `Array<Array<int32>>`, which would free bucket nodes as inner arrays) and `:577-579` `field_0x140.SetSize(0)` (retail's, already right) | only the +0x140 clamp. Row 654's "retail sizes +0x150 to 0x47 here (0x14008fe50)" is a different site (the +0x150 resize, 0x14009010b `CMP R15D,0x47`, row 14) | no (load of a version-5 file only; the tree writes version 6) | ONE-LINE: delete `:553-557` and `ResizeStride0x10` (its only caller). DEAD at version 6 | `citizenlist_serialize.cpp` |
| `src/game/Game/geometry_serialize.cpp:176` | 61 | 14b | 0x1400b04d0 (import slot 0x1401523f8 `Serialize(n, short*)`) | the +0xD8 height words through `Serialize(n, unsigned short*)` (+0x50 slot) | the `short*` slot | yes (overload; binary identical, D9) | SAVE-FORMAT | `geometry_serialize.cpp`, `Game/WorldGeometry.h` (`_heights` element type) |
| `src/game/Game/serialize_arrays.cpp:337` | 63 | 14b | 0x14001bbf0 | the 0x20 menu-scene record as eight `Serialize(uint32&)` words | seven floats and two shorts | yes (overloads; binary identical, D9) | SAVE-FORMAT | `serialize_arrays.cpp` |
| `src/game/Game/citizenlist_serialize.cpp:431` | 810 | 14b (load-only) | 0x14008f760: `CMP EAX,2; JGE` (0x140090194); < 2 arm zeroes a STACK {root [RSP+0x30], count [RSP+0x38]} (0x140090199 / 0x14009019e), reads into it (0x14000ebe0, 0x1400901ac), frees it (0x14000e320 x2, MemFree: 0x1400901c5-0x1400901db), re-zeroes it (0x1400901e1 / 0x1400901e6) — read again by B0, matches the row | `SerializeLegacyKeyIntTree(&field_0x160._root, &field_0x160._count, ser)` reads the legacy 0x30-byte tree INTO the +0x160 bucket Set | reads and DISCARDS through a stack header; +0x160 untouched | no | ONE-LINE (typed local header, free walk). DEAD at version 6 | `citizenlist_serialize.cpp` |
| `src/game/Entity/componententity.cpp:511` | 820 | 14b | 0x1400c3212 / 0x1400c321e (`LEA RDX,[R15+0x40]; CALL [0x140152a50]` = `Serialize(unsigned int&)`) | `Serialize(*reinterpret_cast<int*>(&_entityFlags))` (SerializeInt) | the uint overload (SerializeUInt) | yes (overload; binary identical, D9) | SAVE-FORMAT | `componententity.cpp` |

### A.3 14d — guards and arms retail lacks (14 casts)

| file:line | kpi row | bucket | retail | the tree | retail does | save? | verdict | standalone-specific reason in the comment? | fix touches |
|---|---|---|---|---|---|---|---|---|---|
| `src/game/Action/playanimation.cpp:83` | 14d ACT-b bullet: "`PlayAnimation::AbstractImpl`'s null-target return, readable-memory stale-target probe and null-CharacterComponent return (0x140042420)" | 14d | 0x140042420, read by B0: CheckPtr(+0x20) 0x140042431, `CALL 0x1400012d0` 0x140042440, CheckPtr 0x14004244a, `CALL 0x14010a1f0` 0x140042463, `MOV EAX,1` 0x14004246d | `PlayAnim_TargetLooksLive` probes the target's memory and vptr with `ForceWork::Readable` and returns 0; plus null-target and null-CharacterComponent returns | **upgraded SUSPECTED -> CONFIRMED by B0**: no test at all, always returns 1 | no | RE-READ (ruling): drop all three per D4 unless the reason still holds | YES: "an object built WITHOUT the real ctor is never table-registered, so CheckPtr cannot invalidate a ref to it" (observed 2026-07-24). Check that every ComponentEntity now runs `Object::Internal`'s ctor (AddObject) before dropping | `playanimation.cpp` |
| `src/game/Component/citizenai_growup.cpp:184` | 280 | 14d | 0x14010c890 State_GrowUp, read by B0: 0x14010caf0-0x14010cb01 `MOV RAX,[R15+0x28]; MOV RCX,[RAX+R14*8]; MOV RAX,[RCX]; CALL [RAX+0x10]` — no test of the array, the element or the vptr; a miss reads `[0+0x5c]` (0x14010cb1a, RCX = R12 = 0) | `ScanArray`: `base == nullptr` returns null; null elements and elements whose vtable word reads 0 are skipped | **upgraded SUSPECTED -> CONFIRMED by B0** for State_GrowUp; LongTermPlan 0x140115e60 not re-read (same helper) | no | ONE-LINE (the three tests go) | no | `citizenai_growup.cpp` (also row 850) |
| `src/game/Component/destroycomponent_states.cpp:210` | 391 (+ 390, same function) | 14d | 0x14012ea20: 0x14012ea29 `IsWorkPosted` 0x14012ead0, tree walk at scheduler +0x30 (0x14012ea50-0x14012ea67), CheckPtr, `JMP [RAX+0x58]` | `!yard`, null-singleton, null-scheduler, null-node and null-vptr returns; lookup through `FindWorkAtCell(yard->workId)` (the cell tree, 0x1400f8d50) | none of the tests; the id tree at +0x30 after IsWorkPosted | no | ONE-LINE (rows 390 + 391 in one commit, ~10 lines) | no | `destroycomponent_states.cpp` |
| `src/game/Component/statusiconcomponent.cpp:63` | none (comment: "[STANDALONE GUARD D4]", NEXT_SESSION §0.2) | 14d | not re-read | `SI_Initialised` tests the billboard geometry's vptr word | no such test (per the comment) | no | ONE-LINE: delete the test (the comment itself says the real ctor makes it always true) | the reason given is RETIRED by its own comment | `statusiconcomponent.cpp` |
| `src/game/Dialog/buildrequirementui.cpp:83` | none (no 14d bullet names `FindCompInDesc` / `SaneHeapPtr`) | 14d | 0x14005e7c0, read by B0: the inline find is `if (0 < [desc+0x30]) loop: (**elem)->slot+0x10` with no null, alignment, count-cap or vptr test | `FindCompInDesc` rejects a non-"sane" desc / array / element / vtable (null, misaligned, > 0x7FFFFFFFFFFF, count > 4096) | the unguarded inline find | no | RE-READ: also an UNLISTED difference in the same body — the tree's fallback "IsDerivedFromOrTrust ... PresentsComponentArray" (`:199-210`) trusts `_dataSource` as the description; retail resolves +0x30 only through IsDerivedFrom ComponentDescription (cell 0x1401c7318), ComponentEntity +0x28, or a UI element's user data. Read the whole resolve chain before fixing; record the fallback as a new row | YES as written ("our standalone desc divergence can hand a description whose array is null while the count is non-zero"); not shown to still occur | `buildrequirementui.cpp` |
| `src/game/Dialog/townhallui.cpp:1087` | 14d first KEPT bullet: "`(uintptr_t)comp < 0x10000` at `townhallui.cpp`" | 14d | UpdateProduction 0x14007c7f0, read by B0: the inline find calls `(**elem)->slot+0x10` per element unguarded | `TownHall_FindComponentByTypeId`: null entity / array, count outside 0..200, element pointer range and null-vtable `continue`s | the unguarded inline find | no | ONE-LINE (the file-local finder becomes `ComponentDescription`'s find; also its other two callers :1177, :1322) | no ("STANDALONE; retail has none", no reason) | `townhallui.cpp` |
| `src/game/Dialog/loaderdialog.cpp:86`, `:87` | 572 (b) | 14d | SplashDLT CreateDialog 0x140099070, read by B0: `MemAlloc(0xc8, 8)` unconditionally (0x140099096); on a null result builds a null InternalPtr in the 8-byte return slot (0x140099109 / 0x140099113 `CALL [0x140152a90]`) | `LoaderDialog_FromSplashDLT` tests `MemoryAllocator::_singleton` and, when null, zeroes 16 bytes over the caller's 8-byte `InternalPtr` slot | no allocator test | no | **upgraded SUSPECTED -> CONFIRMED by B0.** ONE-LINE (delete the arm). Note: the tree then constructs into `mem` without retail's null test of the MemAlloc result (0x1400990aa) | no | `loaderdialog.cpp` |
| `runtime/src/ui/userinterface.cpp:1224`, `:1249` | 371 | 14d | 0x1801204b0, read by B0: 0x1801205f6 `MOV RCX,[RBP-0x11]` (the encoding), 0x1801205fa `MOV RAX,[RCX]`, CALL slot +0x20 / +0x18 at 0x180120604 / 0x1801205ff; second reader-writer 0x180120681-0x18012069a; `rw` used unguarded at 0x180120622 and 0x180120661; `IO::Serializer` ctor 0x1800b9960 has the same shape (`MOV RCX,[RDX+0x10]; MOV RAX,[RCX]` 0x1800b9978 / 0x1800b997c) | hand-built `UiSerializer {stream, rw}`; `if (enc)` before each CreateWriter / CreateReader, `if (rw)` before SerializeInt / SerializeUInt | encoding and reader-writer dereferenced unguarded (only the final `delete`s test null, 0x1801206b8 / 0x1801206da) | no | **upgraded SUSPECTED -> CONFIRMED by B0.** ONE-LINE-sized (a real `IO::Serializer` local per site, the `if (enc)` / `if (rw)` tests dropped) | no (the banner says "if the allocation fails retail carries on with a NULL encoding — reproduced": retail then faults) | `userinterface.cpp` |
| `src/game/Game/mapgenerator.cpp:287` | 470 | 14d | dtor 0x14001aea0: 0x14001af47 `MOV RCX,[RBX+0x60]; CALL [0x140152b68]` unconditional, then both qwords zeroed | `MapGenerator::families` stays a `DynArrayHeader` whose dtor keeps `if (data)` before MemFree; the resize views it as `MGFamilyArray` | unconditional MemFree | no | ONE-LINE (the member becomes the `Collection::Array` row; the view cast goes) | no | `mapgenerator.cpp`, `Game/MapGenerator.h` |
| `src/game/Game/menudialogloopt_stubs.cpp:500` | none | 14d | not read (the clamp is the tree's own code) | `MenuDialogLoopT::Update` `const_cast`s MainLoop's Time and clamps the first tick to 1/60 s, restoring it before return | no clamp (per the comment: "our divergence, not retail's") | no | ONE-LINE after a ruling (D4(a)). OBSERVABLE: the menu's first frame (camera / particles get the full boot-to-first-frame dt) | YES as written ("Startup ran synchronously ... the first Update gets all that accumulated time") — but that cause is not standalone-specific: retail's Startup is synchronous too | `menudialogloopt_stubs.cpp` |
| `src/game/Game/splash_loop_stubs.cpp:265` | none | 14d | not read | `GameLoadDialogLoopT::Update`: the same first-tick clamp (fade alpha advances by dt) | no clamp | no | ONE-LINE after the same ruling. OBSERVABLE: the splash fade's first step | same as above | `splash_loop_stubs.cpp` |
| `src/game/Work/pickup.cpp:514` | 14d WORK-b bullet: "`Pickup` slot 5's freed-self vptr test" | 14d | `Pickup::Slot5` 0x1400f38f0, read by B0: stack clear `CALL 0x1400c5400` (0x1400f3976), then `MOV RAX,[RSI]` (0x1400f3988) and `CALL [RAX+0x20]` (0x1400f3991): no test between | returns 0 when `this`'s vptr word is 0 ("self freed during CancelAllWork") | no test | no | **upgraded SUSPECTED -> CONFIRMED by B0.** ONE-LINE | no (a freed-self theory, not a standalone condition; if self really is freed there, that is the upstream finding) | `pickup.cpp` |

---

## B. Rows of §13-14 with NO cast (the same task, not counted by the metric)

Excluded as closed: FIXED / SETTLED rows (75, 440, 691, 700-702 except 701's pad, 706-708, 720-722, 734,
736, 737, 740, 742, 743, 752, 753, 760, 770-775, 780, 790, 791, 795's main fix, 800, 805, 806, 732),
rows settled into another row (29 -> 708, 23 / 24 -> 440, 25 -> 722, 441 -> 691, 444 -> 742, 553 -> 641 /
642, 573 -> 740, 575 -> 660, 580-582 -> 706, 650 -> 720, 664 -> 690, 709 / 750 / 751 -> 752 / 753, 715 ->
734, 723 -> 736, 730 / 731 -> 732 / 742, 796 -> 800, 662 -> 703), 660 (done in the tree), 125 / 126 / 77's
finder drop (dropped tests that match retail), 890 (device-lost, DEFERRED by the user), and the
naming-only rows of 14a (72, 73) and of the 14f tables (85-87, 100-103, 120-124, 155, 156, 172, 173, 201,
270, 271, 332, 333, 351, 360, 361, 380, 410, 420, 445, 446, 574, 661). Rows that carry a cast are in A.

### B.1 §13

| row | bucket | status | what differs | fix files | verdict |
|---|---|---|---|---|---|
| 13 (§13) | other (debugging unit, D10) | OPEN / SUSPECTED | a dying citizen stays an entry of the trading post's tree; `TradeWork::Validate` faults on it. The only path difference found: `CompleteTask::CheckEntity`'s null singleton / scheduler return (14d) | `tradework.cpp`, `completetask.cpp`, the citizen death path (`citizen_lifeevent_handler.cpp`, `citizen_onremove.cpp`) | RE-READ (run-driven, TRADE block) |

### B.2 14a without a cast (43 lines = 44 rows: 35 / 43 share a line; 733 comes from the 14f table)

| row | bucket | status | what differs | fix files | verdict |
|---|---|---|---|---|---|
| 2 | 14a | CONFIRMED | `AnimAudioEvent::Serialize` calls the StringTable serializer; retail 0x1400019a0 (0x140087efa) | the AnimAudioEvent body | SAVE-FORMAT (serializer callee; check the bytes) |
| 3 | 14a | CONFIRMED | `UpgradeUIConfig::Serialize` validates against ComponentEntity; retail ComponentDescription (cell 0x1401c7318), 0x140086fb0 | upgradeuiconfig | ONE-LINE (no runtime effect today) |
| 4 | 14a | CONFIRMED | `PickingDescription::Serialize` calls 0x14001bcc0; retail 0x140037000 (0x140128d00) | pickingdescription | SAVE-FORMAT (serializer callee) |
| 5 | 14a | CONFIRMED | NaturalResourceComponent slot 8 not installed (retail 0x140125c40) | `naturalresourcecomponent.cpp` + its vtable | BLOCKED: user decision (a live change on every natural-resource removal); depends on 552 |
| 6 | 14a | CONFIRMED | MainMenuDialog OnOpened / OnClosed are the tree's; retail 0x1400b3c60 / 0x1400b3cd0 | `mainmenudialog.cpp` | RE-READ (UI routes change; port both slots) |
| 7 | 14a | CONFIRMED | `EventNotifyDialog::OnUpdate` SubString args, final +0xC4, popup build (0x140067c30) | `eventnotifydialog.cpp` | RE-READ (several differences in one body) |
| 8 | 14a | CONFIRMED | `EventLogUI` sizes display arrays from ChildCount; retail reads +0xC0 | `eventlogui.cpp` | ONE-LINE |
| 9 | 14a | CONFIRMED | `NoiseUI` stores the five sliders without the EditElement check | `noiseui.cpp` | ONE-LINE |
| 10 | 14a | CONFIRMED | `WorkPlaceUI` element finds unchecked; retail narrows with IsDerivedFrom | `workplaceui.cpp` | ONE-LINE |
| 11 | 14a | CONFIRMED | Tombstone slot 4: retail checks the description type and always writes the timer | tombstone component | ONE-LINE |
| 12 | 14a | CONFIRMED | `TerraformComponent_RegionRecalc` is a second copy of `Pf_RegionDirty4_b150` with a zero-chunk guard | terraform + action_pf files | ONE-LINE (delete the copy, call the one body) |
| 33 | 14a | SUSPECTED harmless | XAudio2 `GetState` passes a 2.8 second argument; 2.7 ignores R8 | `vtbatch7b_stream_run_win.cpp` | RE-READ (likely NOT A DIFFERENCE in effect) |
| 34 | 14a | CONFIRMED from the code | the launcher dialog reads a LOADED `Runtime-x64.dll` that the standalone never loads (a door) | `engine_platform_win.cpp` | BLOCKED: needs the dialog proc 0x1800a1ae0 ported (scope: user) |
| 35 / 43 / 80 | 14a | 80 CONFIRMED, 35 / 43 SUSPECTED | `WorkComponent_CalcMaxWorkers` (`tutorial_base.cpp`) is a second port of 0x14012e5c0 with two guards; ~21 callers | `tutorial_base.cpp` + callers (tradework, plantarea, plantorchard, tendarea, forester, gather, harvest x2, herbalist, hunt, market, pasture) | ONE-LINE per caller (D12, one commit) |
| 36 | 14a | CONFIRMED | StatusBarUI slots 4 / 8 and StorageUI / StorageCapacityUI / StatusIconUI slot 8 overridden; retail inherits (refresh is slot 7) | statusbarui, storageui, storagecapacityui, statusiconui | RE-READ (UI behaviour; the 30-frame refresh) |
| 37 | 14a | SUSPECTED | `StatusBarUI::OnOpened` stores finds unchecked (0x14007a3d0 narrows) | `statusbarui.cpp` | RE-READ (read 0x14007a3d0; the group id question) |
| 38 | 14a | CONFIRMED | `CitizenUI_BuildIconArray_Impl` is a second body of 0x14006abf0 (no narrowing, null-group return) | `citizen_icon_helpers.h` | ONE-LINE (use `TownHall_GroupRatingInit`'s faithful copy) |
| 39 | 14a | SUSPECTED | TownHall inventory sorts are stable insertion sorts; retail 0x140015200 / 0x140015500 introsort | `townhall_helpers.cpp` | RE-READ |
| 40 | 14a | CONFIRMED | `TitleDescToolTipUI::OnCreated` takes finds unnarrowed (0x14007b730) | `titledesctooltipui.cpp` | ONE-LINE |
| 41 | 14a | CONFIRMED (gap) | `SpriteCaptureUI::OnElementEvent` is a PORT GAP (0x140079620) | `ui_dialog_stub_factories.cpp` | BLOCKED: port the body first |
| 44 | 14a | CONFIRMED | `PastureWork::OnCancel` never clears bit 20; retail always clears it (0x1400f22c0) | `pasturework.cpp` | ONE-LINE |
| 46 | 14a | SUSPECTED | two bodies of 0x1400c3470 (null-slot skip in one) | `componententity.cpp`, `action_base_hunt.cpp` | RE-READ (the null skip) |
| 47 | 14a | CONFIRMED | PathfinderThread case 5 holds the lock through the ready store (retail unlocks at 0x1400a658b) | `action_pf_astar.cpp` | ONE-LINE |
| 48 | 14a | CONFIRMED | the 64 KB path buffer is `static` (retail stack, 0x1400a6436) | `action_pf_astar.cpp` | ONE-LINE |
| 49 | 14a | CONFIRMED | `RegionPairCache::Store` writes 0 where retail leaves a stale dword (never read) | `action_pf_region.cpp` | ONE-LINE (unobservable) |
| 52 | 14a | CONFIRMED | `RemoveAnimal::Persist` validates against FireDescription; retail ComponentEntity (0x1400ca8e1) | `removeanimal.cpp` | ONE-LINE (OBSERVABLE: a loaded RemoveAnimal keeps its entity) |
| 60 | 14a | CONFIRMED from the bodies | the menu camera frustum takes interface / adapter index as width / height | `menudialogloopt.cpp` | ONE-LINE |
| 62 | 14a | CONFIRMED | `MenuScene::Serialize` reads legacy batches unconditionally; retail only for version >= 1 (0x1400a36da-0x1400a36f0) | `menuscene.cpp` | ONE-LINE (load path; dead for version >= 1 files) |
| 70 | 14a | CONFIRMED | `PlacementTool::CreateAt` passes 250.0f; the cell holds 1000.0f | `tool_base_tile.cpp` | ONE-LINE (OBSERVABLE: placement test) |
| 71 | 14a | CONFIRMED | `ToolRegistryNode` is 0x30 bytes without the +0x30 balance byte (retail 0x38) | `toolbardialog.cpp` | ONE-LINE (layout) |
| 74 | 14d (filed 14a) | CONFIRMED from comments | PLAYER guards: FencedTool::Startup bail, BridgeTool::Draw cell-range checks, RemovalCalc e4d0 / dfa0 StateMachine bail, EditTool position fallback | fencedtool, bridgetool, removal calc, edittool | ONE-LINE each (D4) |
| 76 | 14a | CONFIRMED | `FencedTool::CalcCost` passes `&anchorMinCol` as the yaw out (retail `[RBP+0x58]`, 0x1400db7eb) | `fencedtool.cpp` | ONE-LINE |
| 90 | 14a | CONFIRMED | `WeatherParticleGeometry::Configure` advances the world Random; retail a local Random (0x1400d3fde) | `weather_particles.cpp` | ONE-LINE (D11: changes later world draws) |
| 92 | 14a | CONFIRMED | `wg_ResolveMaterial_fd10` bare Find, no cache, triple not updated (0x1400b0230) | `worldgen.cpp` | ONE-LINE-ish (D11: may change loader counts) |
| 221 | 14a | CONFIRMED | `BuildStructure::Validate` uses a local `CheckCitizenHasTask` (wrong type ids) and removes by node-as-key; retail `Work::ValidateTask` (0x1400e4c67) and 0x14001a6e0 | `buildstructure.cpp` | ONE-LINE (OBSERVABLE: builder assignments are no longer all dropped) |
| 290 | 14a | SUSPECTED | `CitizenAI_SortByPriority` stable insertion sort; retail introsort (0x14002fe10) | `citizenai_candidatelist.cpp` | RE-READ |
| 390 | 14a | CONFIRMED | (with cast row 391) | `destroycomponent_states.cpp` | in unit 391 |
| 392 | 14a | CONFIRMED by B0 (0x14011e7b0) | (with cast row 510) | | in unit 510 |
| 442 | 14a | SUSPECTED | ProductionUI rebuild passes dst = produced, src = displayed (0x140075a50's order not read) | `productionui.cpp` | RE-READ |
| 571 | 14a | SUSPECTED | DebugUI spawn button passes the ProfessionList as a Profession* | `debugui.cpp` | RE-READ (debug path) |
| 663 | 14a | CONFIRMED | GameUpdateLoop ctor takes the scenario IBP by value; retail by reference (0x140098085) | `gameupdateloop.cpp` + 5 callers | ONE-LINE-ish (signature; callers' null arms) |
| 665 | 14a | from the code | `FindAvailableBuilding` / `TownRegistry_ShuffleProfessionList` take the town as int64 | `town_workplace_registry.cpp` | ONE-LINE (typing; no value change expected) |
| 733 | 14a (14f table) | CONFIRMED | FencedTool CalcCost / Draw never read the terrain normal's z (retail 0x1400db71e / 0x1400dc8ad reject steep cells) | `fencedtool.cpp` | ONE-LINE (OBSERVABLE: placement on steep cells) |

### B.3 14b without a cast (6)

| row | bucket | status | what differs | fix files | verdict |
|---|---|---|---|---|---|
| 14 | 14b | CONFIRMED | no resize to 0x47 after +0x150 (retail resizes, zero-extend on load; 0x14009010b-0x140090119) | `citizenlist_serialize.cpp` | SAVE-FORMAT |
| 18 | 14b | CONFIRMED | `StateMachineComponent::Persist` uchar overload through 4 casts; retail signed char (0x14012b940) | `statemachinecomponent.cpp` | SAVE-FORMAT (D9) |
| 19 | 14b | CONFIRMED | `WorkComponent::workFlags` uint32; retail the int door | `WorkComponent.h` / body | SAVE-FORMAT (D9) |
| 20 | 14b | SUSPECTED | LayoutDescription enum fields, `FruitComponent::field30` (a float as uint32) | layoutdescription, fruitcomponent | RE-READ then SAVE-FORMAT |
| 21 | 14b | CONFIRMED | settings Save normalises each flag with `!= 0`; retail raw byte (0x1400989d7) | settings save | SAVE-FORMAT |
| 22 | 14b | CONFIRMED | settings init writes int32 at +0x0C / +0x18; retail one byte (0x140042001, 0x140042013) | `game_main.cpp` | SAVE-FORMAT |

### B.4 14c without a cast (15 lines = 17 rows: 640 / 651 / 652 share a line)

| row | bucket | status | what differs | fix files | verdict |
|---|---|---|---|---|---|
| 26 | 14c | CONFIRMED | `TradeDescription::Serialize` allocates without setting capacity (retail 0x14000f370) | tradedescription | ONE-LINE |
| 27 | 14c | CONFIRMED | `TradeComponent` ctor writes +0x6c..+0x70 | tradecomponent | ONE-LINE |
| 28 | 14c | CONFIRMED | `MapComponent` ctor zero-fills the footprint rect; retail 0xFFFF | mapcomponent | ONE-LINE (may change placement / gate counts: check) |
| 30 | 14c | CONFIRMED | `Graphics::AnimationState` 0x48 bytes (retail 0x18, ctor 0x1800fdaf0) | runtime graphics + the pinning unit test | RE-READ (size change across readers) |
| 31 | 14c | CONFIRMED per class | H5 raw-storage members (CitizenComponent name, Profession Strings, Build / WorkPlace arrays, Map picking scene, PlayerController transform / camera, three-field Array headers) | per class | RE-READ: several are FIXED since (720 etc.); list what is left |
| 32 | 14c | CONFIRMED | stack RendererDrawGroup / RendererSceneEntry zeroing ctors | runtime renderer | ONE-LINE (cost only) |
| 42 | 14c | CONFIRMED | `NoiseUI` dtor `= default` leaks ImageBuffer / MaterialInstance / SpriteSheet (retail 0x140071bc0) | `NoiseUI.h` | ONE-LINE |
| 50 | 14c | SUSPECTED | PathfinderThread +0x160 / +0x30 members trivially constructible (retail ctor 0x1400a67f0) | `PathfinderThread.h` | RE-READ |
| 91 | 14c | CONFIRMED | weather +0xD0 index block: raw MemAlloc, capacity never written (retail 0x14001c890) | `weather_particles.cpp` | ONE-LINE |
| 104 | 14c | CONFIRMED | grid-temporary `~Array` hands MemFree a null (retail 0x18001c100 tests) | `scene_grid.cpp` | ONE-LINE (no game effect) |
| 370 | 14c | SUSPECTED | `UI::Font::~Font` releases +0x20 before the array frees (0x180117590) | `font.cpp` | RE-READ |
| 640 / 651 / 652 | 14c | CONFIRMED | `Collection::Set()` writes its +0x0C pad (retail: root qword + count dword only) | `runtime/types/collection/Collection.h` | ONE-LINE (D8, gated alone) |
| 701 (residual) | 14c | CONFIRMED | AmbientEmitterComponent ctor writes the +0x34 pad dword (retail 0x1400430f1 writes +0x30 only) | ambientemittercomponent | ONE-LINE |
| 735 | 14c | DEFERRED here | `coastline_DeepCopyPath_24ac0` zeroes only +0x08 and tests data before MemFree | `worldgen_f390_coastline.cpp` | ONE-LINE (store order only) |
| 741 | 14a (filed 14c) | CONFIRMED | `EntityUI::OnElementEvent` unpin removes from +0x50 (descriptions); retail SyncPin 0x1400666b0 removes from +0x60 | `entityui.cpp` | ONE-LINE (OBSERVABLE: unpinning works) |

### B.5 14d tables without a cast (37)

| row | bucket | status | what differs | fix files | standalone-specific reason? | verdict |
|---|---|---|---|---|---|---|
| 80 | 14d | CONFIRMED | (unit 35 / 43 / 80) | `tutorial_base.cpp` | no | ONE-LINE (D12) |
| 81 | 14d | CONFIRMED | `WorkPlaceComponent::GetMaxWorkers` returns 0 without a WorkComponent (0x14012fbb0) | `tutorial_base.cpp` | no | ONE-LINE |
| 82 | 14d | SUSPECTED | `FindOnEntity<T>` null-entity return (0x1400df290) | `tutorialgoalcount.cpp` | no | RE-READ |
| 83 | 14d | SUSPECTED | `TutRestrict_ResolveRefOrLoad` load-on-miss (retail 0x14001b1d0 find-only) | `tutorialsetuprestrictplayer.cpp` | YES ("STANDALONE DEVIATION" note) | RE-READ |
| 84 | 14d | SUSPECTED | `NamedExternalRef::Resolve` load-on-miss + generation re-read (0x1400265b0) | `goal_exe_helpers.cpp` | no | RE-READ |
| 88 | 14d | CONFIRMED | `TimeGoalInstance::CheckGoalState` null tests (0x1400dacb0) | `timegoalinstance.cpp` | no | ONE-LINE |
| 89 | 14d | SUSPECTED | `TutorialHighlightDialog` null tests | `tutorialhighlightdialog.cpp` | no | RE-READ |
| 77 (residual) | 14d | CONFIRMED | PathTool route builder's 20,000,000-spin bound (retail loops, 0x1400ddc40-0x1400ddc76) | `pathtool_setselected.cpp` | YES ("the tree's standalone safety") | ONE-LINE after a ruling |
| 78 | 14e (filed 14d) | SUSPECTED | `AddDecal` byte store through a `uint8_t*` cast over a `bool` | `tool_base_highlight.cpp` | no | RE-READ (store width) |
| 97 | 14d | CONFIRMED | two ports of 0x140024ac0 with null tests (735 is the remaining one) | worldgen ranges / coastline | no | ONE-LINE |
| 98 | 14d | SUSPECTED | `town_storage_seed.cpp` null skips / IsDerivedFromNullSafe / missing-description bails | `town_storage_seed.cpp` | YES ("standalone-inert deviations", file header) | RE-READ |
| 99 | 14d | SUSPECTED | FindAvailableBuilding null-slot skips + three "defensive" continues (0x14008d100) | `town_workplace_registry.cpp` | no ("defensive") | RE-READ |
| 93 | 14d | SUSPECTED | `TownMgr_AddAvailableAgri` second port of 0x140091a20 with bails | `starttown.cpp` | no | RE-READ |
| 94 | 14d | SUSPECTED | `StartTown_WriteCellType` second port of 0x14009f2f0 with null tests | `starttown.cpp` | no | RE-READ |
| 135 | 14d | CONFIRMED | `ClearArea::OnComplete` `treeEntry ?` (0x1400e5e3a unguarded) | `cleararea.cpp` | no | ONE-LINE |
| 170 | 14d | SUSPECTED | `ShouldSwitchWorkplace` profession / lookup-miss returns (0x1401109e0) | `citizencomponent.cpp` | no | RE-READ |
| 171 | 14d | SUSPECTED | FindHome / CanFindHousing / Update / ef40 guards | `citizencomponent.cpp` | YES ("STANDALONE GUARD" notes) | RE-READ |
| 210 | 14d | CONFIRMED | `Helper_e360` `ValidNode` link fences (retail tests null only) | `citizenai_housing_idle.cpp` | no | ONE-LINE |
| 211 | 14d | CONFIRMED | `Helper_sub_79b0` profession null test (0x1400f7a89) | `citizenai_housing_idle.cpp` | YES ("null in the standalone for a citizen not yet assigned") | ONE-LINE after a ruling |
| 200 | 14d | CONFIRMED (bucket guard SUSPECTED) | WorkScheduler Post / Rekey null returns and the miss's `workFlags = 0` (retail derefs null, 0x14012ed2b) | `citizenai_work_scheduler.cpp` | no | ONE-LINE |
| 250 | 14d | SUSPECTED | find-helper null tests (`ec70`, `e370`, `2ac30`, `dd30`, `78c0`) | `citizenai_findhelpers.cpp`, `citizenai_findtarget.cpp` | YES ("unfilled standalone type ids") | RE-READ |
| 251 | 14d | SUSPECTED | gather helpers' guards | `citizenai_gather.cpp` | no ("every deref null-guarded") | RE-READ |
| 252 | 14d | SUSPECTED | MoveTick tail / pick registration guards | `citizenai_executor.cpp` | no | RE-READ |
| 240 | 14d | CONFIRMED (State_Leisure inline), helpers SUSPECTED | `VGetTypeId` 0xffffffff, scan null returns, happiness test (retail 0x14010ac53 / 0x14010ad9b) | `citizenai_leisure.cpp` | no | ONE-LINE (inline copies) / RE-READ (helpers) |
| 291 | 14d | SUSPECTED | HasSufficientResources / CarryCount `[STANDALONE GUARD]` returns | `citizenai_candidatelist.cpp` | YES (tag only) | RE-READ |
| 330 | 14d | SUSPECTED | reassign-workers null returns | `citizen_reassign_workers.cpp` | no ("guard for safety") | RE-READ |
| 331 | 14d | radius query CONFIRMED, rest SUSPECTED | `Map_RadiusQuery_92990` null tests (0x140092a02 / 0x1400929fa / 0x140092a39); disease bodies | `citizen_updatehealth.cpp` | no | ONE-LINE (query) / RE-READ (rest) |
| 310 | 14d | SUSPECTED | life-event / burial / assign / needs / OnRemove / SetHome guards | `citizen_lifeevent_handler.cpp`, `citizen_assign_citizens.cpp`, `citizen_needsupdate.cpp`, `citizen_onremove.cpp`, `citizen_assignworkplace.cpp` | YES ("[STANDALONE GUARD]" / "DEVIATION") | RE-READ (also feeds §13's open question) |
| 350 | 14d | SUSPECTED | `SetDestination` kept `!state` / `!owner` returns | `boatcomponent_route.cpp` | no | RE-READ |
| 352 | 14d | SUSPECTED | `PostEventToDestination` kept null returns | `boatcomponent_states.cpp` | no | RE-READ |
| 450 | 14d | CONFIRMED | `CalcWorkerSlots_Impl` third copy of 0x14012e5c0 with guards | `professionui.cpp` | no | ONE-LINE (call the member; joins D12) |
| 451 | 14d | SUSPECTED | ProfessionUI worker-slot walk null skips (0x1400765a0) | `professionui.cpp` | no | RE-READ |
| 443 | 14d | SUSPECTED | two ports of 0x140119470, one guards a null Random | `Dialog/exe_bodies.h`, `action_child_ctors2.cpp` | no | RE-READ |
| 551 | 14d | SUSPECTED | `CitizenComp_StorageComponent_QueryCount` `!self` (member 0x14012cf10 unguarded); ~25 callers | `citizenaicomponent.cpp` + callers | no | ONE-LINE (callers to the member) after reading 0x14012cf10 |
| 570 | 14d | CONFIRMED for 0x140001770 | ambient resizes skip MemAlloc on a null allocator | `ambientemittercomponent.cpp`, `ambientenvironmentdescription.cpp` | no | ONE-LINE (to the erased core) |
| 653 | 14d | CONFIRMED | +0x140 / +0x150 rows keep the serializer's guarded element copy (0x14000f9e0 has none) | `citizenbehavior.cpp` | no | ONE-LINE (unreachable today) |
| 692 | 14d | SUSPECTED | EditTool close path dialog-pointer range test (0x1400fcc90) | `edittool.cpp` | no | RE-READ |

### B.6 14d KEPT bullets, one line per guard site (75 lines)

"SA reason" = the site's comment gives a STANDALONE-SPECIFIC reason (D4(a)). Most of those reasons
describe the pre-B "inert / partial standalone world" (unregistered type ids, components the partial
factory tree did not build); B.3/B.4 made the factory complete, so each needs its reason re-checked, not
assumed (marked "stale?").

| # | bullet | guard site | file | status | SA reason | verdict |
|---|---|---|---|---|---|---|
| G1 | first KEPT | null entity / MapComponent / map tests and null component-slot skips "in the Component c bodies" | `src/game/Component/*` (unenumerated) | SUSPECTED | unknown | RE-READ: enumerate the sites first |
| G2 | first KEPT | `ClearArea::IsDone` reads the description without IsDerivedFrom | not found by that name today (`cleararea.cpp` / `work_leaf_clear.cpp`) | SUSPECTED | — | RE-READ (locate) |
| G3 | first KEPT | `LeaveDocked` missing-StateMachine guard | `tradecomponent_dock.cpp:627` | SUSPECTED | YES ("standalone-only and does NOT skip the teardown") | RE-READ |
| G4 | first KEPT | `SetAccepting` missing-StateMachine guard | `workplacecomponent.cpp:341` | SUSPECTED | YES ("a standalone workplace can lack the state machine"; stale?) | RE-READ |
| G5 | first KEPT | `EntityList_RegisterEntity_Impl` null guards (38 callers) | `componententity.cpp:717` | SUSPECTED | no ("the null guards are the tree's") | ONE-LINE after reading retail |
| G6 | first KEPT | the guarded `fn_FindComponent_*` copies | `entity_findcomponent.cpp` | CONFIRMED shape (0x140006270 has no null tests, read by B0) | no | ONE-LINE per finder (callers to `FindComponent<T>()`) |
| G7 | first KEPT | `Entity_FindComponentByExeTypeId` | `entity_findcomponent.cpp:57` | same | no | ONE-LINE |
| G8 | first KEPT | the disaster finders | not located by name | SUSPECTED | — | RE-READ (locate) |
| G9 | first KEPT | `Pasture_FindLivestockComp` | not found in the tree today | — | — | close if gone |
| G10 | first KEPT | `NaturalResourceComponent_FindComponentByType` | `naturalresourcecomponent.cpp:76` | SUSPECTED | no | dies with 400 / 552 |
| G11 | first KEPT | `ActMv_FindComponentFamily_12d0` (0x1400012d0): today `FindCharacterComponent`'s type-id test | `action_base_move.cpp:62` | SUSPECTED | no | ONE-LINE |
| G12 | first KEPT | `(uintptr_t)comp < 0x10000` | `townhallui.cpp:1083` | CONFIRMED by B0 (0x14007c7f0) | no | in A.3 (townhallui:1087) |
| G13 | first KEPT | EventNotifyDialog `dialogRes != nullptr` skip | not found by that spelling today | — | — | RE-READ (locate) |
| G14 | first KEPT | AutoSaveDialog `dialogRes != nullptr` skip; today the file has `g_gameSettings == nullptr` (:57), no-gameWorld (:142), `g_gameContainer == nullptr` (:158) bails | `autosavedialog.cpp` | SUSPECTED | YES (:44, "standalone" note) | RE-READ |
| G15 | R-ENG | `IO::Modification::_singleton` null test (0x1800dd8c8 / 0x1800dd8d2) | `updateloop_loadresources.cpp:169` | CONFIRMED | YES (:175, the stub's default null) | ONE-LINE after a ruling |
| G16 | R-ENG | `IO::FileSystem::_singleton` null test (0x1800dd988 / 0x1800dd98f) | `updateloop_loadresources.cpp:169` | CONFIRMED | same | same |
| G17 | R-ENG | `OptionsDialog::ApplyInputSettings` null `Input::Configuration` return (0x14007237f) | `optionsdialog.cpp:738` | CONFIRMED | no | ONE-LINE |
| G18 | R-ENG | `Input::Device::PlatformStartup` null `Application::_singleton` (x2) | `runtime/src/input/input_platform_startup_win.cpp` | SUSPECTED | no | RE-READ |
| G19 | R-ENG | `KeyMap::Load` Persistence null test | `runtime/src/input/keymap_load.cpp:238` | SUSPECTED | no | RE-READ |
| G20 | R-ENG | `Input::Device::PlatformShutdown` skips three erases on a null Application | `input_platform_shutdown_win.cpp:226` | CONFIRMED (banner) | YES ("our standalone can reach shutdown on a ...") | keep if the reason holds; ruling |
| G21 | WORK-c | `Work::InitFromEntity` null MapComponent / map | `work_base_reimpl.cpp:64` | SUSPECTED | no | RE-READ |
| G22 | WORK-c | `Work::TryAutoPickup` null AutoPickupComponent / singleton (0x1400e3db0) | `work_base_grid.cpp:121` | SUSPECTED | no | RE-READ |
| G23 | WORK-c | 0x1400c1eb0 `derivedTarget` arms (PerformWork / MoveTo / ShowTool built empty) | `work_base_actionctor.cpp` | SUSPECTED | no | RE-READ |
| G24 | WORK-c | 0x1400c1530 `derived` arms | `work_base_actionctor.cpp` / `work_act_clear.cpp` | SUSPECTED | no | RE-READ |
| G25 | WORK-c | 0x1400c0ad0 `targetIBP` and singleton tests | `work_base_actionctor.cpp` | SUSPECTED | no | RE-READ |
| G26 | WORK-c | 0x1400be6e0 `condPtr8` / `msgIdPtr9` null arms | `work_base_actionctor.cpp` | SUSPECTED | no | RE-READ |
| G27 | WORK-c | 0x1400bef70 `param_9` null arm | `work_act_consume.cpp` / `work_base_actionctor.cpp` | SUSPECTED | no | RE-READ |
| G28 | WORK-c | `CompleteTask::CheckEntity` null singleton / scheduler return (0x1400c5ac0) | `completetask.cpp:117` | CONFIRMED | no | ONE-LINE (also §13) |
| G29 | DLG-d | StatusBarUI Bound / Ready / TownReady / Built gates on slots 5, 7, 9 | `statusbarui.cpp` | CONFIRMED where quoted | YES for the slot 4 / 8 wiring (:608, :615 "STANDALONE DEVIATION"); no for the gates | RE-READ (with row 36) |
| G30 | DLG-d | StatusBarUI textSeason string-table test | `statusbarui.cpp:384` | CONFIRMED (0x1400798d0) | YES ("GetIndexFromName in standalone") | ruling |
| G31 | DLG-d | StatusBarUI null `g_gameSettings` test | `statusbarui.cpp:397` | CONFIRMED (0x1400798d0) | YES ("can be unresolved in standalone") | ruling |
| G32 | DLG-d | status-icon handler null game / town / citizen / StatusIconComponent and `count <= 0` (0x14007a120) | `statusbarui.cpp` | CONFIRMED | no | ONE-LINE |
| G33 | DLG-d | ToolUI dialog-address windows (`ToolUI_DialogHasEntityUI`, OnUpdate) | `toolui.cpp:324`, `:561` | CONFIRMED where quoted | YES (:561 "standalone dialogs / controllers are only partially initialized"; stale?) | RE-READ |
| G34 | DLG-d | `PlayerToolManager::SelectTool` empty-registry return | `toolbardialog.cpp:291` (comment :396-400) | CONFIRMED | YES ("the tool-mode tree can legitimately be empty or partial in a standalone run") | ruling |
| G35 | DLG-d | toolbardialog game-world / text-element tests | `toolbardialog.cpp` | CONFIRMED where quoted | no | ONE-LINE |
| G36 | DLG-d | SpriteCaptureUI Input::Device / capture-box / FileSystem / edit-element tests | `spritecaptureui.cpp` | CONFIRMED where quoted | no | ONE-LINE (row 41's gap first) |
| G37 | DLG-d | StatusIconUI EntityUI-pages test | `statusiconui.cpp:157` | CONFIRMED where quoted | YES ("a partial-world entity, or a building without one in standalone") | ruling |
| G38 | DLG-d | `TitleDescToolTipUI::OnCreated` unbound-controller return + two null text tests (0x14007b730) | `titledesctooltipui.cpp:30` | CONFIRMED | no | ONE-LINE (with row 40) |
| G39 | WORK-b | `HarvestArea::Execute` skips an occupant without NaturalResourceComponent (0x1400ed082) | `harvestarea.cpp:125` | CONFIRMED | no | ONE-LINE |
| G40 | WORK-b | `HarvestArea::Cleanup` break when a cancel does not drop the count (0x1400ed2cb) | `harvestarea.cpp:463` | CONFIRMED | no (:482 "deliberate deviation", a hang argument, not standalone) | ONE-LINE (a hang is then the finding) |
| G41 | WORK-b | `PG_FindComp` null-slot / null-entry skips (0x14008d3c0) | `pickup_gate.cpp:55` | CONFIRMED | no | ONE-LINE |
| G42 | WORK-b | `PG_FindRawMaterialDesc` skips | `pickup_gate.cpp:71` | CONFIRMED | no | ONE-LINE |
| G43 | WORK-b | `CitizenBehavior::HasStorageFor` skips (0x14012d990) | `pickup_gate.cpp:147` | CONFIRMED | no (:143 is a measurement) | ONE-LINE |
| G44 | WORK-b | PastureWork's guarded finder 0x1400012d0 | `pasturework.cpp` | SUSPECTED | no | RE-READ |
| G45 | WORK-b | `Pickup` slot 5 freed-self vptr test | `pickup.cpp:514` | CONFIRMED by B0 | no | in A.3 |
| G46 | WORK-b | GatherRawMaterials site-ctor guards | `gatherrawmaterials.cpp` | SUSPECTED | no | RE-READ |
| G47 | WORK-b | HarvestOrchard ctor orchard / array / slots guards | `harvestorchard.cpp` | SUSPECTED | no | RE-READ |
| G48 | WORK-b | `CitizenComp_StorageComponent_QueryCount` null test | `citizenaicomponent.cpp` | SUSPECTED | no | = row 551 |
| G49 | ACT-a | `InteractNodePosition` SA-GUARD zero vector and ModelComponent fallback (0x140122e20) | `action_pf_road.cpp:482` (comment :489-490) | SUSPECTED | YES ("standalone component wiring can miss it"; stale?) | RE-READ |
| G50 | ACT-a | `EntityAnchorCell` null-entity diagnostic return (0x140087fb0) | `action_pf_road.cpp:635` | SUSPECTED | no | RE-READ |
| G51 | ACT-a | `InteractNodeCount` null-description 0 (0x140123070) | `action_pf_ctors.cpp:143` | SUSPECTED | YES ("type-id global unfilled / unregistered in standalone"; stale?) | RE-READ |
| G52 | ACT-a | `FindComponentSkippingNull` / `FindDescriptionSkippingNull` null-slot skips | `action_pf_roots_route.cpp:91` | SUSPECTED | YES (:86 "in standalone a ..."; stale?) | RE-READ |
| G53 | ACT-a | `PickBestDropTarget` StorageLocationDescription and source-mask tests (0x14008d590) | `action_pf_roots_route.cpp:329` (:370, :382) | SUSPECTED | YES ("standalone can leave it null"; stale?) | RE-READ |
| G54 | ACT-a | `StorageCanAdd` missing RawMaterialDescription weight 0 (0x14012cd80) | `action_pf_roots_route.cpp:164` | SUSPECTED | no | RE-READ |
| G55 | ACT-a | `FindCharacterComponent` type-id test (0x1400012d0) | `action_base_move.cpp:62` | SUSPECTED | no | = G11 |
| G56 | ACT-a | `HerdStartMigration` null-member skip (0x140122340) | `action_pf_roots_misc.cpp:392` (:437) | SUSPECTED | partly ("DEVIATION (guard)") | RE-READ |
| G57 | ACT-a | `PlayModelAnimation` null-model and type-miss returns (0x1401250f0) | `action_base_movedeps.cpp:93` | SUSPECTED | YES ("standalone-inert guard"; stale?) | RE-READ |
| G58 | ACT-a | `HuntIsReady` null description (0x140122a80) | `action_base_hunt.cpp:37` | SUSPECTED | no | RE-READ |
| G59 | ACT-a | `ResourceCategory` missing-description -1 (0x140088f90) | `action_base_rawmat.cpp:234` | SUSPECTED | no | RE-READ |
| G60 | ACT-a | `PositionRawMaterial` MapComponent / ModelComponent tests (0x140129760) | `action_base_rawmat.cpp:389` | SUSPECTED | YES ("[standalone] inert / absent"; stale?) | RE-READ |
| G61 | ACT-a | `HeadingToward` null world / map (0x1400c29a0) | `action_child_ctors.cpp:44` | SUSPECTED | no | RE-READ |
| G62 | ACT-a | `TaskCleanup` missing-bucket test (0x1400f8e30) | `action_base_complete.cpp:148` | SUSPECTED | no (a WAVE 9 note) | RE-READ |
| G63 | ACT-a | `DropRawMaterialOnGround` Random test | `action_pf_roots_drop.cpp:100` | SUSPECTED | no | RE-READ |
| G64 | ACT-b | `AnimateNaturalResource::Execute` null-ModelComponent return (0x1400c55b0) | `animatenaturalresource.cpp:74` | SUSPECTED | YES ("standalone-inert"; stale?) | RE-READ |
| G65 | ACT-b | `CompleteTask::AbstractImpl` null singleton / `m_tutorial` return (0x1400c5b40) | `completetask.cpp:78` | SUSPECTED | YES ("task machinery is inert in standalone": stale, the scheduler is live) | ONE-LINE after reading 0x1400c5b40 |
| G66 | ACT-b | `CreateRawMaterial::AbstractImpl` zero-batch return (0x1400c6c22 divides) | `createrawmaterial.cpp:149` (:198) | SUSPECTED | YES ("STANDALONE-ONLY TRIPWIRE") | ruling (a zero batch faults in retail) |
| G67 | ACT-b | `MoveTo` ctor `charDescTid &&` and `random ?` tests (0x1400c9170) | `moveto.cpp` | SUSPECTED | no | RE-READ |
| G68 | ACT-b | `PerformWork` ctor null-CitizenDescription skip (0x1400c97e2-0x1400c97ee) | `performwork.cpp:256` (:260) | CONFIRMED | YES ("IsDerivedFrom returns false on the partial factory tree"; stale?) | ONE-LINE (with unit 51; same reason as 705) |
| G69 | ACT-b | `PickupStoredRawMaterial` null-RawMaterialDescription guards in ctor and AbstractImpl (0x1400ca520 / 0x1400c9d40) | `pickupstoredrawmaterial.cpp` | SUSPECTED | no | RE-READ |
| G70 | ACT-b | `PlayAnimation::AbstractImpl` null-target and null-CharacterComponent returns (no cast) | `playanimation.cpp` | CONFIRMED by B0 (0x140042420) | YES (same as the probe) | with A.3 playanimation |
| G71 | ACT-b | `RemoveNaturalResource` AbstractImpl / CheckEntity null returns (0x1400cab10 / 0x1400caaa0) | `removenaturalresource.cpp:101`, `:153` | SUSPECTED | YES ("standalone-safety: the find can resolve null"; stale?) | RE-READ |
| G72 | ACT-b | `SendComponentMessage::AbstractImpl` canonical / dying / readability probes, sentinel skip (0x1400caef0) | `sendcomponentmessage.cpp:74` (:83) | SUSPECTED | YES (the weak-ref invalidation reason, as playanimation) | RE-READ (same ruling as playanimation) |
| G73 | ACT-b | `ShowTool` null-citizen returns in AbstractImpl / CheckEntity / GetTarget (0x140042710 / 0x1400426d0 / 0x140042680) | `showtool.cpp` | SUSPECTED | YES (:56-58, :225 "null in standalone"; stale?) | RE-READ |
| G74 | ACT-b | `ShowTool_ShowCarried`, a second guarded port of 0x1401173b0's toolId < 0 branch | `showtool.cpp:116` (:112) | SUSPECTED | YES ("partially-materialized standalone material") | ONE-LINE-ish (delete; call the one body, with row 45) |
| G75 | GAME-b | `ProfessionList::Startup` null ExternalFactory / 0 type id / non-Profession skip (0x1400a90a0) | `professionlist.cpp:138` | CONFIRMED | no | ONE-LINE |

### B.7 14e (6 items)

| item | status | what differs | fix files | verdict |
|---|---|---|---|---|
| `optionsdialog.cpp:745,748` `!= 0` | CONFIRMED value-identical (0x140072390 / 0x14007239f, 0x1400723b9 / 0x1400723c2) | normalising store where retail copies the bool byte | `optionsdialog.cpp` | ONE-LINE (spelling) |
| R-UI-b / R-VIDEO flags now `bool` (SpriteSheet m_field50, TextDescription _b84 / _b85, ribbon / scroll `_vertical`, RenderState flags) | SUSPECTED | identical only if the byte is 0 / 1 | runtime ui / video headers | RE-READ |
| ACT-b seven `uint8_t` -> `bool` conversions (animatenaturalresource:145, removenaturalresource:218, dropoffrawmaterial:276, consumerawmaterial:392, pickupstoredrawmaterial:421, createrawmaterial:379, performwork:252) | 2 CONFIRMED raw copies, 5 SUSPECTED; H8: every caller passes 0 / 1 | `setne` where retail copies | the seven files | ONE-LINE each (value-identical) |
| row 241 `HappinessComponent::registered` | CONFIRMED (0x14010adaa / 0x14010adb0 store a literal 1) | GCC copies the byte | `citizenai_leisure.cpp` | ONE-LINE (value-identical) |
| row 311 `Cemetery_SpawnTombstone_6a90` byte OR into Map +0xA2 | SUSPECTED | byte OR through a cast over a `bool` | `citizen_lifeevent_handler.cpp` | RE-READ |
| row 447 `EventNotifyDialog::OnUpdate` copies the filter byte | SUSPECTED | copy instead of a literal 1 | `eventnotifydialog.cpp` | RE-READ |

### B.8 Behaviour rows in the 14f tables (6, plus 733 already in B.2)

| row | bucket | status | what differs | fix files | verdict |
|---|---|---|---|---|---|
| 754 | 14c | OPEN | WeatherSimulation ctor zero stores precede the PerlinNoise ctor (retail after, 0x140093f60) | `weather_sim.cpp` | ONE-LINE (store order only) |
| 792 | 14d | OPEN | EntityList drain net `AuditDyingEntity` re-fires ParticleComponent::OnRemove (the tree drains a frame late) | `entitylist.cpp` | BLOCKED: the one-frame-late drain is the cause; fix that first |
| 795 (residual) | 14d | listed | GeometryBatch_Resize negative-count / no-allocator guards, the destroy row's `!first` test | `WorldGeometryElements` rows | ONE-LINE |
| 850 | 14d | OPEN | `CitizenAI_Entity_impl_f230` tests the InteractComponent before InteractNodePosition (retail 0x14008f3db-0x14008f40f calls unconditionally) | `citizenai_growup.cpp` | ONE-LINE (D4: the guard goes, the local becomes a `Vector3Slot`) |
| 860 | 14b | OPEN | `ModelDescription::Serialize` group +0x00 typed through the GraphicsMesh test; retail 0x140035b40 (type cell 0x140152980, AnimationGroup) | `modeldescription.cpp` | RE-READ (confirm the import slot's class) then SAVE-FORMAT |
| 880 | other | OPEN, unobservable | Audio::Scene::Update hands a bare zeroed pair; retail a stack EffectSlot (comment wrong, behaviour not) | `audio_scene_update.cpp` | ONE-LINE (comment) |
| 733 | 14a | CONFIRMED | (listed in B.2) | | |

### B.9 Count of section B

| group | rows / lines |
|---|---:|
| §13 | 1 |
| 14a (B.2, including 733 and 74) | 44 rows on 43 lines |
| 14b (B.3) | 6 |
| 14c (B.4) | 17 rows on 15 lines |
| 14d table rows (B.5) | 37 |
| 14d KEPT bullet guard sites (B.6) | 75 lines (G12, G45 carry the A.3 casts; G48 = row 551; G55 = G11; G70 joins A.3) |
| 14e (B.7) | 6 |
| 14f-table behaviour rows (B.8, 733 counted in B.2) | 6 |
| **rows (all but B.6)** | **117 rows on 114 lines** |

---

## C. Dependencies, shared files, dead code, user decisions

### C.1 Rows that depend on another

- **452 before 690's forwarder.** The cell 0x1401c7e78 is StorageComponent's (read above: the writer
  `StorageComponent_RegisterType` 0x14014fb10 / 0x14014fb35, string "StorageComponent" at
  0x140164708, factory 0x140038b50 `MemAlloc(0x40, 8)`). So 452 is a retail over-read, not a tree
  defect: the user rules (a) faithful + retail_bugs.md, or (b) a deliberate read through
  ResidenceComponent. `toolbardialog.cpp:1658-1660` and `entity_findcomponent.cpp:100` go only after it.
- **400 / 550 / 552 / row 5 / entry 3.** The finder swap is dead-code work; wiring `SpawnSubResource`
  (entry 3) and installing slot 8 (row 5) are separate LIVE behaviour changes the user decides.
- **392 / 510** one commit (D1); the Dismissed handler's retail address is the open item.
- **35 / 43 / 80 / 450** one family (D12): `professionui.cpp`'s `CalcWorkerSlots_Impl` is a third port
  of 0x14012e5c0 and should join it.
- **1 and 551.** Row 1's `:439` calls row 551's guarded duplicate; fix 551 (callers to the member)
  before or with row 1.
- **705 and G68 (PerformWork ctor).** Both skip IsDerivedFrom for "the partial factory tree": one ruling
  covers both, and unit 51 (performwork.cpp) shares the file with G68.
- **playanimation (A.3), G70, G72.** One ruling (the weak-ref / unregistered-object reason).
- **Row 13 (§13)** needs G28 (`CompleteTask::CheckEntity`) and row 310's death path read; the TRADE block
  owns `completetask.cpp`.
- **15 / 654.** The correct version-5 clamp is already in the tree (`citizenlist_serialize.cpp:577-579`);
  the fix is deleting the stray one, so 15 and 654 close together.

### C.2 Files shared by several rows (merge conflicts)

| file | rows |
|---|---|
| `src/game/Game/citizenlist_serialize.cpp` | 13, 14, 15 / 654, 810 (all 14b: coordinator, serial) |
| `src/game/Dialog/toolbardialog.cpp` | 690, 452, 71, G34, G35 |
| `src/game/Dialog/residenceui.cpp`, `Entity/entity_findcomponent.cpp` | 452; entity_findcomponent also G6, G7 |
| `src/game/Component/citizenai_leisure.cpp` | 704, 643 (caller :253), 240, row 241 (14e) |
| `src/game/Component/citizenai_findhelpers.cpp` | 643 (callers :251, :442), 250 |
| `src/game/Component/citizenai_growup.cpp` | 280, 850 |
| `src/game/Component/citizenaicomponent.cpp` | 1, 551 / G48 |
| `src/game/Component/citizencomponent.cpp` | 705, 170, 171 |
| `src/game/Action/performwork.cpp` | 51, G68, the 14e `setne` at :252 |
| `src/game/Action/playanimation.cpp` | A.3 probe, G70 |
| `src/game/Action/action_pf_road.cpp` | 643 (forwarder), G49, G50 |
| `src/game/Work/cleararea.cpp` | 220, 135 |
| `src/game/Work/consumeproduce.cpp` | 260, G27 (0x1400bef70 callers) |
| `src/game/Work/gatherrawmaterials.cpp` | 861, G46 |
| `src/game/Component/destroycomponent_states.cpp` | 390, 391 |
| `statemachinecomponent.cpp`, `controller_state_table.h`, `firecomponent_smstate.cpp`, `tradecomponent_states.cpp` | 392 / 510; tradecomponent_* also G3 and the TRADE block's area |
| `src/game/Game/starttown.cpp` | 95, 93, 94 |
| `src/game/Entity/componententity.cpp` | 820, 46, G5 |
| `src/game/Dialog/loaderdialog.cpp` + `Game/splash_loop_stubs.cpp` | 572 (b), the splash first-tick clamp (one boot flow) |
| `src/game/Dialog/statusbarui.cpp` | 36, 37, G29-G32 |
| `src/game/Player/fencedtool.cpp` | 74, 76, 733 |
| `src/game/Game/weather_particles.cpp` | 90, 91 |
| `src/game/Dialog/townhallui.cpp` | A.3 townhallui; `townhall_helpers.cpp` row 39 |
| `runtime/types/collection/Collection.h` | 640 / 651 / 652 (alone, D8) |

### C.3 Dead code today (D5: fixed to retail's shape anyway)

- **95**: `starttown.cpp:1814`, the `!StartTown_InstantBuildPath()` arm (`starttown.cpp:971` returns true).
- **400 / 550**: `SpawnSubResource` is never reached (entry 3). **552**: `NaturalResourceComponent_OnRemove`
  has no caller (slot 8 a PORT GAP, row 5).
- **810**: the version < 2 arm (`citizenlist_serialize.cpp:548`), unreachable at save version 6.
- **15 / 654**: the stray version-5 clamp (`:553-557`), unreachable at version 6 (the B6_RULES "230" is
  this line, see the report).
- **430**: the tornado volume feeds only the PORT GAP 0x140002280 audio branch.
- **62**: MenuScene's legacy batches only matter for version-0 streams.
- **653**: the guarded element copy is unreachable while the allocator is up.

### C.4 What the user must decide

- Every save-format row: 13, 14, 16, 17, 18, 19, 20, 21, 22, 61, 63, 820, 860 (and 2 / 4, whose
  serializer callee changes). 15 / 654 and 810 change no written byte (load of version-5 / version < 2
  files only).
- Rows that change the generated world or the gate's loader counts: 90 (world Random draws), 92
  (generation-cached Finds), 28 (MapComponent footprint 0xFFFF), possibly 733 / 70 (placement tests).
- Live gameplay changes that a run will show: 51, 511, 861 (work / hunt / gather timing), 704 (leisure
  target), 220 / 221 (work validation), 260 (consume-produce actions), 44, 52, 741, 5.
- 452: faithful over-read or deliberate deviation.
- The D4(a) rulings: 705 + G68 (partial factory tree), playanimation + G70 + G72 (unregistered weak
  refs), the two first-tick clamps, buildrequirementui's "desc divergence", and every "stale?" reason in
  B.6.
- Row 34 (the launcher dialog) needs a port of 0x1800a1ae0, outside this step unless the user widens it.

---

## D. Counts

### D.1 Casts by bucket (63)

| bucket | casts | units |
|---|---:|---:|
| 14a | 41 | 17 |
| 14b | 8 | 8 |
| 14c | 0 | 0 |
| 14d | 14 | 11 |
| 14e | 0 | 0 |
| **total** | **63** | **36** |

### D.2 Casts and units by verdict

| verdict | casts | units |
|---|---:|---:|
| ONE-LINE | 38 | 25 (of which 6 are dead code today: 95, 400 / 550, 552, 430, 15 / 654, 810; and 2 wait for a D4(a) ruling: 705, the first-tick clamps) |
| RE-READ | 8 | 3 (260, playanimation, buildrequirementui) |
| SAVE-FORMAT | 6 | 6 (13, 16, 17, 61, 63, 820) |
| BLOCKED | 11 | 2 (452 by the user's ruling; 690's forwarder by 452) |

### D.3 Section B rows by bucket and verdict

| bucket | rows | ONE-LINE | RE-READ | SAVE-FORMAT | BLOCKED | other |
|---|---:|---:|---:|---:|---:|---|
| §13 | 1 | 0 | 1 | 0 | 0 | |
| 14a | 44 | 27 | 10 | 2 | 3 | 2 joined to cast units (390, 392) |
| 14b | 6 | 0 | 1 | 5 | 0 | |
| 14c | 17 (15 lines) | 11 lines | 4 | 0 | 0 | 31 is RE-READ to list what is left |
| 14d tables | 37 | 11 | 22 | 0 | 0 | 2 ONE-LINE after a ruling (77, 211); 2 mixed ONE-LINE / RE-READ (240, 331) |
| 14e | 6 | 3 | 3 | 0 | 0 | |
| 14f-table behaviour | 6 | 4 | 1 | 0 | 1 | 860 RE-READ then SAVE-FORMAT |
| 14d KEPT guard lines | 75 | 19 | 41 | 0 | 0 | 8 rulings (G15, G16, G20, G30, G31, G34, G37, G66); 7 joined / gone (G9, G10, G12, G45, G48, G55, G70) |

Upgraded from SUSPECTED by B0 (read in this session): 95, 280, 371, 572 (b), 704, 15 / 654 (with a
correction), the playanimation probe, the pickup slot-5 probe, 452 (settled as a retail over-read); and
392's ForceSMState tail plus two trade Tick sites for 510.
