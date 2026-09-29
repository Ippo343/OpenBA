# Known port issues

Places where OUR code is known or suspected to differ from retail, found while doing something
else and deliberately NOT fixed on the spot. (Behaviours that are faithful to retail and look like
bugs are in `retail_bugs.md`; this file is the opposite list.)

Rules for an entry: what the code does today, what retail is believed to do, the evidence and how
strong it is, and what a fix must do. A fix is its own commit titled `behaviour:`, proven from
retail's disassembly and gated with resave x5. Status is one of `SUSPECTED` (not yet read in
retail), `CONFIRMED` (retail read, addresses given), `FIXED` (commit).

## 1. `CitizenComp_helper_fe40`: the age-difference `abs()` does nothing — SUSPECTED

**Where.** `code/Game/Component/citizencomponent.cpp`, `CitizenComp_helper_fe40` (retail
`0x14010fe40`): finds a partner citizen whose age is inside the description's window
(`+0x48` minimum age, `+0x4c` maximum age difference).

**What the code does.** It takes `abs(candidateAge - ownAge)` by AND-ing the float's bits with a
mask read from the `.rdata` cell `0x1401722b0`. That cell is registered as the 8-byte DOUBLE fabs
mask `0x7fffffffffffffff` (`standalone/reg_component_nature.cpp`, now
`kRdata_1401722b0` in `code/Game/Game/exe_constants.h`). The body reads it as a `uint32_t`, i.e. its
low dword `0xffffffff`, so the AND clears nothing and the sign survives: a candidate YOUNGER than
the citizen has a negative difference, which always passes `adiff <= maxDifference`.

**Effect, if retail takes a real `abs`.** The age window is one-sided: any candidate older than the
minimum age and younger than the citizen is accepted however large the gap.

**Evidence.** Found 2026-09-28 by the registry teardown's G-CompA block when the reader of the cell
became the named constant; the block kept the exact value (`static_cast<uint32_t>`), so behaviour
is unchanged by that conversion. NOT yet read in retail. Two explanations are open and the fix
differs: (a) retail's instruction is a float `ANDPS` against a DIFFERENT cell (a 4-byte
`0x7fffffff` mask) and the port resolves the wrong address; (b) retail's cell at `0x1401722b0`
holds a value whose low dword is `0x7fffffff` and the REGISTERED value is wrong — in which case
every other reader of `kRdata_1401722b0` must be re-checked too.

**What a fix must do.** Read `0x14010fe40` in Ghidra (the AND's operand address and width) and the
image bytes at that address; then either point the body at the right constant or correct the
constant. Check the other readers of `0x1401722b0` in the same commit.

## 2. `OrchardRequestState` passes an ADDRESS where the state machine takes the time by value — SUSPECTED

**Where.** `code/Game/Component/orchardcomponent_states.cpp`, the call of `StateMachine_TickReimpl`
(retail `0x1400a9660`).

**What the code does.** The old door typedef took a pointer and the caller passed `&timeBuf`; the
body takes the time BY VALUE (`int64_t`). The direct call keeps today's bits
(`reinterpret_cast<int64_t>(timeBuf)`-style cast of the address). Retail is reported to pass the
value 0.

**Evidence.** Found 2026-09-28 by G-CompC; retail's call site not yet quoted with an address.
The trade state machine had the same shape (`tradecomponent_states.cpp`) and now passes the value;
on that path the argument is reported unread.

**What a fix must do.** Read retail's call site, pass what retail passes.

## 3. Growth sub-resources never spawn — SUSPECTED (a PORT GAP, listed here because a port exists)

**Where.** `code/Game/Component/growthcomponent.cpp`, `SpawnSubResource`.

**What the code does.** The pointer to retail `0x14009f5d0` (`CharacterComponent_impl_f5d0`) was
never filled, so the "not placed" branch has always run and sub-resources never spawn. The
teardown wrote that branch out as a `PORT GAP` null arm. A port of the body exists
(`PlayerPath_CharComp_f5d0`) but has never been called from here.

**What a fix must do.** Wire the body in as its own `behaviour:` commit, after checking its
signature against retail's call site, and watch the entity census on the new-game and tutorial
routes.

## 4. `BANISHED_ALLOC_STATS` counted no frees — FIXED

Every free bypassed the instrument, so its live-bytes figure only grew. Fixed 2026-09-28 by the
registry teardown's W0-A0 block (`behaviour:` commit `33d73c9a`): the instrument sits on the
allocator's hook and sees `MemFree`. Diagnostic only; the game was never affected.

## 5. Door typedefs that disagree with their body's parameter WIDTH or sign — SUSPECTED (a list)

When the registry teardown turned door calls into direct calls, the compiler exposed doors whose
typedef did not match the body. Each call keeps the bits the door passed (a cast at the call), so
behaviour is unchanged; none has been read against retail's call site yet.

| where | address | mismatch |
|---|---|---|
| `code/Game/Action/action_base_move.cpp` | `0x1400a6a30`, `0x1400a68b0`, `0x1400a6b20`, `0x1400a6c10` | the door passed `int` arguments; the bodies take pointer-width parameters (`0x14009c480` was here too: it takes one parameter now, entry 7) |
| `code/Game/Action/action_pf_road.cpp` | `0x1400aa810` | the door took a `uint64` bit pattern where the body takes a pointer |
| `code/Game/Action/action_pf_region.cpp` | `0x14001dc00` | `param_4` goes through a `void*` cast |
| `code/Game/Action/` | `0x14012c560`, `0x140087fb0` | return type `int` against `uint32_t` |
| `code/Game/Component/` (G-CompD) | `0x14011e710` | the body returns `uint64_t`; the door read AL (kept: cast to `char`) |

**What a fix must do.** Per row: read retail's call site (argument registers and their width), then
correct the BODY's declaration or the call, as a `behaviour:` commit only if a value changes.

## 6. Body names that disagree with how their address is used — naming only

Not bugs: the address and the binding are right, the NAME misleads. Rename when the class gets its
members (B.4).

| address | body name | used as |
|---|---|---|
| `0x140004680` | `FloorComponent_FindResData` | the `MapComponent` finder |
| `0x140026a30` | `BuildComponent_FindStorage26a30` | the `PickingComponent` finder |
| `0x14012ee60` | `WorkPlaceComponent_OnCreateInit` | the worker-release loop |
| `0x140124750` | `MapComponent_DetachOccupancy_4750` (registered) vs `CreateNaturalResource_impl_4750` (a second body) | — |

The 19 misnamed `engine_funcs` globals of `b3r_session/INVENTORY_E_ENGINE_FUNCS.md` §5.8 die with
the globals and are not repeated here.

## 7. `Pf_ZoneRefresh_9c480` declares three parameters retail does not have — FIXED (signature only)

**Where.** The body of retail `0x14009c480`, and its callers in `code/Game/Work` (commits
`6067e8a1`, `4b9d6b19`), `code/Game/Game/gamegrid_serialize.cpp` (`e200bbb6`) and `code/Game/Action`.

**What the code does.** The body is declared with four parameters and hands the fourth to the flood
fill (`0x14009c170`). The direct calls pass `(grid, 0, 0, 0)`.

**Retail** (read 2026-09-28 by the registry teardown's G-Game block). The callers `0x14009f29e`,
`0x1400eb303`, `0x1400e532a`, `0x1400e54ea`, `0x1400e556c` load ONLY RCX. `0x14009c480` WRITES EDX
and R8D before it reads them (`0x14009c4bd`, `0x14009c520` / `c524`) and never touches R9;
`0x14009c170` overwrites R9D at `0x14009c26f` and rewrites its home slot (`c365`, `c3e8`) before any
read. So no incoming value of RDX, R8 or R9 is ever read: the three extra parameters are Ghidra's
(it invents arguments from register use), and what the callers pass cannot matter. The gate showed
no diff on any route or resave.

**What a fix must do.** Declare the body with ONE parameter and make its three former parameters
locals; drop the zeros at the callers. No behaviour change. Same check for `0x14009c170`'s list.

**Fixed** 2026-09-28 by the registry teardown's LEFTOVERS block (commit titled `behaviour:`, "the
zone refresh 0x14009c480 takes one parameter, the flood fill 0x14009c170 three"), after
re-reading both functions. `0x14009c480`: EDX is written at `0x14009c4bd` and per cell at
`0x14009c524` (= EDI, the cell index), R8D at `0x14009c520` (= R14W, the region id), R9 never;
those two locals already existed in the body (`uVar2`, `sVar6`), so the "three former
parameters" needed no new local: two were those locals, the third existed only to be passed on.
`0x14009c170`: the entry spills R9D to its home slot, every read of the slot follows a write
(`0x14009c365`, `0x14009c3e8`) and R9D is overwritten at `0x14009c26f`, so Ghidra's `param_4`
is the local `local_res20` (initial value never read, left uninitialised). Declarations:
`Pf_ZoneRefresh_9c480(int*)`, `Pf_FloodFillRegion_9c170(int*, uint32_t, uint16_t)`. All 15
call sites of `0x14009c480` (Action, Component, Game, Work; most passed non-zero values, e.g.
`(grid, out, flags, destCell)`, `(mapGrid, zr_a, zr_b, 1)`, `(mapInfo, mapInfo, 0, 0)`) and the
two of `0x14009c170` lost their trailing arguments; none had side effects. The caller-less
second body `WorkAcc_ZoneRefresh_9c480` (`work_base_accessor.cpp`) got the same list. No value
that is ever read changed.

## 8. Wrong BINDINGS found in `code/Game/Dialog` — SUSPECTED

| door | bound to | problem |
|---|---|---|
| `fn_Game_DestroyEntity` (debug UI "destroy all raw materials", retail `FUN_140060000`) | `0x140091e60` `RawMaterialComponent_RegisterRawMaterial` | the body REGISTERS where the caller means to destroy. Debug UI only |
| `fn_DiseaseComponent_Infect` | `DiseaseComp_Infect(DiseaseComponent*)` | the caller hands it a citizen pointer |
| `fn_StartNewGame` | `StartNewGame_Reg` | the door passed seed and terrain size as `uint32` values; the body takes pointer-width slots and reads their low dword |
| — | `PastureComponent_ComputeCapacity` | returns `uint64_t`; the door read `int` |
| — | `TradeComponent_SpawnPurchasedLivestock` | takes an `unsigned` quantity; the door passed `int` |

Each call keeps today's bits. The first two need retail's call site read before anything changes.

## 9. Bodies declared `void` whose callers read a return value — FIXED / no reader

| address | body | what the caller reads |
|---|---|---|
| `0x14009ce00` | `RoadTool_sub_ce00` (road A*) | the path length retail leaves in EAX (`roadtool.cpp`, `roadtool_calcpath.cpp`). The readers were LEFT as resolves by the registry teardown because the signature cannot be settled without retail |
| `0x1400fd960` | — | a return value |
| — | `Pf_RoadMeshRebuild_f370` | a return value |

Today the callers read whatever the body's last computation left in EAX. **What a fix must do:**
read each function's epilogue in retail, give the body its return type and value.

**2026-09-28, the registry teardown's LEFTOVERS block:**
- `0x14009ce00` — FIXED (commit titled `behaviour:`, "the road A* 0x14009ce00 returns the path
  length"). Retail ends in a tail `JMP 0x1400a6e30` at `0x14009cf41` (only callee-saved
  registers restored before it); `0x1400a6e30` takes EAX from its `CALL 0x1400a6d50` at
  `0x1400a6edf` and leaves it to the `RET` at `0x1400a6ef8`. So the value IS computed by the
  body: our `RoadTool_helper_6e30` already returned it as `int`. `RoadTool_sub_ce00` now
  returns `int` (`return RoadTool_helper_6e30(...)`), and the five readers in `roadtool.cpp` /
  `roadtool_calcpath.cpp` call it by name. Its 5th parameter is never read (retail overwrites
  its stack slot at `0x14009cf22`). `standalone/reg_keystone_a_zone.cpp` still has a local
  `void` prototype of it (return types are not mangled; W2 deletes the row).
- `0x1400fd960` (`PlacementToolPlace_FlipVariant_d960`) and `0x1400af370`
  (`Pf_RoadMeshRebuild_f370`) — no reader reads a value any more: their callers
  (`placementtool.cpp`, `tool_base_tile.cpp`, `keystone_a_removalcalcpath.cpp`) call them by
  name as `void`, and the only "value" the old doors read was the upper bytes of RAX under a
  `bool` slot whose contract is AL. Nothing to fix unless a caller is found that reads more.

## 10. `0x140172248` is 1000.0f; the code around it says otherwise — SUSPECTED

The image bytes at `0x140172248` are `00 00 7a 44` (1000.0f, read in Ghidra 2026-09-28). A comment
in the placement code calls the cell 0.5, and `code/Game/Player/tool_base_tile*.cpp` hard-codes
`250.0f` where retail reads this cell. **What a fix must do:** read retail's instruction at the
`tool_base_tile` site and use `kRdata_140172248` if that is the operand.

## 11. `String::ToInteger` and a `memmove` were never reachable from the UI edit widgets — FIXED, not exercised

`runtime/src/ui`: `NumberEditElement::FinalizeText`, `SpinnerElement` (3 sites) and `DocumentText`
resolved `0x18003f620` (`System::String::ToInteger`), and `EditElement` resolved `0x18008a290`
(a `memmove`), by ADDRESS. Nothing registered either address, so both resolved to null and the first
call would have jumped to 0. Fixed 2026-09-28 (`behaviour:` commit `638c4b3a`, the registry
teardown's RT-UI block): direct calls of `System::String::ToInteger` and `std::memmove`.
No gate route types into a number field or edits text, so the fix is compiled and unit-built but
has NOT been seen running. A route that types into the New Game seed field would exercise it.

## 12. `AttendanceComponent_GetJobHostComponent` was `void`; `FindAvailableBuilding` read its return — FIXED

**Where.** `code/Game/Component/attendancecomponent.cpp` (retail `0x140002e10`, a by-value
`InternalBasePtr` return: RAX is the return buffer), read by `FindAvailableBuilding`
(`code/Game/Game/town_workplace_registry.cpp`, retail `0x14008d100`).

**What happened.** The body was declared `void`. The reader called it through a function pointer
typed `void* (*)(…)` and dereferenced RAX. Through the registry's pointer RAX happened to hold the
buffer; when the registry teardown made it a direct call, the -O2 build left `1` in RAX and the
stress town crashed (`READ of 0000000000000001`, `town_workplace_registry.cpp:286`, reached from
`CitizenEmploy_SeatWorkers`). No gate route seats workers through this path; only `sa-stress` does.

**Fix.** The body returns `outPtr`; the reader calls it by name. Entry 9 lists the same class of
defect. **The lesson for B.4:** a cast of a function to another signature is never "keeping today's
bits" — what RAX holds after a `void` function is not defined, and changes with the call form.

## 13. `TradeWork::Validate` walks a stale entry of the post's citizen tree — OPEN (found 2026-09-29)

**Symptom.** The stress town with the virtual merchant customer
(`BANISHED_AUTOSTART=1 BANISHED_STRESS=1 BANISHED_TRADE_SELL=600 BANISHED_TRADE_SELL_SEED=50`, debug
exe) dies with exit 3 after 17-19 trades (frame ~10,800-12,000): `Object::Base::IsA`
(`TypeInfo.h:338`) <- `WorkBase_ValidateTask_f7100` (`work_base_reimpl.cpp`) <-
`TradeWork::Validate()` (`tradework.cpp:545`) <- the citizen AI's idle helpers
(`citizenai_housing_idle.cpp`) <- `CitizenAIComponent::State_Idle`. The object whose `Type()` is
read is garbage (`READ of 0x18900000073` at B.4's tip; `EXEC of 0x6000000060` at the base: the
vtable slot itself was garbage there).

**Not a B.4 regression.** Reproduced on `b2a5434c` (the step's base) with the same route: same three
frames. No block of B.4 changed `tradework.cpp` or the validate body.

**What is known.** `TradeWork::Validate` walks the in-order BST of entities assigned to the trading
post and validates each; the last log line before the fault in the B.4 run is a citizen dying
(`[Burial] NO GRAVE … cause=0(starvation)`). A citizen that leaves the world is still an entry of
the post's tree. Retail has no check at the fault site (`IsA` on the entry): do NOT add one. The
question for the fix is which retail path removes a dying / leaving citizen from the work's tree
(the citizen's `OnRemove` 0x140113ca0 and the workplace release 0x140116540 / 0x140115a50 are the
candidates) and whether the tree's port of it reaches TradeWork. The route is not part of any gate.

## 14. Differences from retail found by the B.4 blocks and left — a list (2026-09-29)

Each was found by a B.4 block while it converted the body, read against retail by that block, and
deliberately NOT changed inside the conversion. Status `CONFIRMED` means the block quoted retail's
address; `SUSPECTED` means it did not read retail. Every fix is its own `behaviour:` commit.
The block's hand-in is summarised in `runtime_port/endgame_b/b4_session/GATE_TABLE.md`; the site
in the code carries a comment.

### 14a. Wrong callee, wrong object, wrong overload

| # | where | the tree | retail | status |
|---|---|---|---|---|
| 1 | `CitizenAIComponent::CheckEducation` (`citizenaicomponent.cpp`) | calls the Residence finder 0x140008090, adds a null-guard bail, and writes Residence+0x38 (inside the embedded controller) | 0x14010f7a0 calls the StorageComponent finder 0x140006270 at 0x14010f7e1 / 0x14010f830 | CONFIRMED (COMP-b, COMP-e) |
| 2 | `AnimAudioEvent::Serialize` | calls the StringTable serializer | calls 0x1400019a0 at 0x140087efa | CONFIRMED (COMP-a) |
| 3 | `UpgradeUIConfig::Serialize` | validates the target against `ComponentEntity::TypeId()` | 0x140086fb0 validates against ComponentDescription (cell 0x1401c7318); no runtime effect today: `IsDerivedFromOrTrust` accepts any non-null pointer | CONFIRMED (DLG-e) |
| 4 | `PickingDescription::Serialize` | calls 0x14001bcc0 | 0x140128d00 calls 0x140037000 | CONFIRMED (COMP-e) |
| 5 | `NaturalResourceComponent` slot 8 | NOT installed | 0x140125c40 finds a ParticleComponent through 0x140005200 (a 0x48-byte object) and reads its +0x28 as a MeshInstance, while 0x140126440, which sets the tick bit it clears, animates the ModelComponent: needs a ruling on what the receiver really is | CONFIRMED, unresolved (COMP-d) |
| 6 | `MainMenuDialog` OnOpened / OnClosed | the tree's own bodies | the ports of slots 5 / 4 are 0x1400b3c60 / 0x1400b3cd0; installing them changes what the main menu does on open and close (the gate's UI routes) | CONFIRMED (DLG-b) |
| 7 | `EventNotifyDialog::OnUpdate` | SubString arguments, the final +0xC4 value and how the popup is built differ | 0x140067c30 | CONFIRMED (DLG-b) |
| 8 | `EventLogUI` | sizes its display arrays from ChildCount | reads +0xC0 | CONFIRMED (DLG-b) |
| 9 | `NoiseUI` | stores the five sliders without checking they are EditElements | checks | CONFIRMED (DLG-b) |
| 10 | `WorkPlaceUI` element finds | unchecked | narrows each with IsDerivedFrom | CONFIRMED (DLG-e) |
| 11 | Tombstone slot 4 | the tree's form | checks the description type and always writes the timer | CONFIRMED (COMP-e) |
| 12 | `TerraformComponent_RegionRecalc` | a second copy of `Pf_RegionDirty4_b150` with the tree's zero-chunk guard | one body | CONFIRMED (COMP-e) |
| 33 | `Audio::Stream::Run` (`vtbatch7b_stream_run_win.cpp`), `IXAudio2SourceVoice::GetState` | mingw's `<xaudio2.h>` is the 2.8 interface, so the call passes a second argument (`Flags = 0` in R8) | XAudio2 2.7's `GetState(pVoiceState)` takes one; slot +0xc8 is the same and 2.7 ignores R8 | SUSPECTED harmless (R-ENG) |

### 14b. The save format (each fix re-cuts the resave golden)

| # | where | the tree | retail | status |
|---|---|---|---|---|
| 13 | `CitizenBehavior::Serialize`, `ProfessionResourceEntry` | writes the Time at +0x14 | 0x14008f760 uses +0x18 | CONFIRMED (GAME-a) |
| 14 | same, after the +0x150 array | no resize | resizes to 0x47 with a zero-extend fixup on load | CONFIRMED (GAME-a) |
| 15 | same, version 5 | clears the +0x160 tree | clears +0x140 | CONFIRMED (GAME-a) |
| 16 | `EventLog::Serialize`, entry colour | the int overload | the uint overload | CONFIRMED (GAME-a) |
| 17 | `SpawnComponent::Persist` | tests the stream as if it were a Serializer, so the load-time reconcile never runs | 0x140055ee0 reconciles on every load | CONFIRMED (COMP-e) |
| 18 | `StateMachineComponent::Persist` | the uchar overload, through 4 casts | the signed-char overload, 0x14012b940 | CONFIRMED (COMP-e) |
| 19 | `WorkComponent::workFlags` | `uint32` | serialized through the int door | CONFIRMED (COMP-f) |
| 20 | `LayoutDescription` enum fields, `FruitComponent::field30` (a float stored as `uint32_t`) | as declared | the overload retail calls decides the type | SUSPECTED (R-UI-a, COMP-d) |
| 21 | the settings Save | normalises each flag with `!= 0` | passes the raw byte, 0x1400989d7 | CONFIRMED (W0-H2) |
| 22 | the settings init (`game_main.cpp`) | writes int32 at +0x0C and +0x18 (the pads become 0) | writes one byte (0x140042001, 0x140042013); the pads keep MemAlloc's bytes | CONFIRMED (W0-H2, D) |

### 14c. Construction, destruction, leaks

| # | where | the tree | retail | status |
|---|---|---|---|---|
| 23 | `HelpUI` | its history array is a raw pointer, never freed | the dtor frees it, 0x140011cf0 | CONFIRMED (DLG-b) |
| 24 | `EventLogUI` | its three arrays are raw pointers, never freed | 0x1400114b0 | CONFIRMED (DLG-b) |
| 25 | `StorageComponent` | the dtor does not free the item array | retail's does | CONFIRMED (W0-H2) |
| 26 | `TradeDescription::Serialize` | allocates without setting capacity; its dtor walks the capacity | resizes through 0x14000f370 | CONFIRMED (COMP-f) |
| 27 | `TradeComponent` ctor | writes +0x6c..+0x70 | does not | CONFIRMED (COMP-f) |
| 28 | `MapComponent` ctor | zero-fills the footprint rect | writes 0xFFFF | CONFIRMED (COMP-d; an older PORT GAP) |
| 29 | `IO::PackageFile::GetFileInfo` | returns `_file` as a bitwise copy, no AddRef; harmless only while `_name0` / `_name1` are null | the FileInfo copy ctor 0x18000c110; `ReadFile` then releases the names | CONFIRMED (R-IO); needs `FileInfo` to be a real class |
| 30 | `Graphics::AnimationState` | 0x48 bytes (a unit test pins it) | 0x18, ctor 0x1800fdaf0 | CONFIRMED (R-GFX) |
| 31 | H5 raw-storage members: CitizenComponent `name` +0x28 (String) and `consumedTypes` +0x98; Profession's three Strings; the Build / WorkPlace arrays; Map's picking scene; PlayerController's transform and camera (`Camera`'s default ctor is protected); the `Collection::Array` headers declared as three fields | raw storage, so no ctor / dtor runs | real members | CONFIRMED per class (W0-H1, W0-H2, GAME-a); B4_RULES correction 7 |
| 32 | `BeginDrawGroup`, `Render` (runtime graphics) | stack `RendererDrawGroup` / `RendererSceneEntry` objects whose implicit ctors zero memory that is then overwritten | no such stores | CONFIRMED (R-GFX); cost only |

### 14d. Guards the tree has and retail lacks — and guards B.4 dropped

Retail's unguarded derefs are load-bearing (CLAUDE.md). The first group is kept and listed; the
second group was removed when a typed member replaced a guarded helper, and is safe only under the
stated condition.

- KEPT: null entity / MapComponent / map tests and null component-slot skips in the Component c
  bodies; `ClearArea::IsDone` reads the description without IsDerivedFrom; the missing-StateMachine
  guard in `LeaveDocked` and `SetAccepting`; `EntityList_RegisterEntity_Impl`'s null guards (38
  callers); the guarded find copies (`fn_FindComponent_*`, `Entity_FindComponentByExeTypeId`, the
  disaster finders, `Pasture_FindLivestockComp`, `NaturalResourceComponent_FindComponentByType`,
  `ActMv_FindComponentFamily_12d0`); `(uintptr_t)comp < 0x10000` at `townhallui.cpp`; the standalone
  `dialogRes != nullptr` skips of EventNotifyDialog / AutoSaveDialog.
- KEPT: `UpdateLoop::LoadResources` (`updateloop_loadresources.cpp`) tests `IO::Modification::_singleton`
  and `IO::FileSystem::_singleton` for null; retail loads both and dereferences them unguarded
  (0x1800dd8c8 / 0x1800dd8d2, 0x1800dd988 / 0x1800dd98f). CONFIRMED (R-ENG).
- KEPT: `OptionsDialog::ApplyInputSettings` (`optionsdialog.cpp`) returns early when
  `Input::Configuration::_singleton` is null; retail 0x140072360 loads it (0x14007237f) and stores
  through it with no test. CONFIRMED (R-ENG read it; the file is DLG's).
- DROPPED: `ComponentEntity::FindComponent<T>()` does not skip a null component slot and does not
  test the type id for 0 / -1 (retail's per-type instances do neither: 26 read by block T). Safe
  while `ComponentEntity`'s ctor never stores a null slot (it skips a component with no factory) and
  every `T` is registered. PastureUI's second copy of ComputeCapacity went with its two guards
  (`behaviour:` 0x140128570).
- DROPPED WITHOUT READING RETAIL: the MemAlloc-failure path of `TickIdle` (COMP-f). SUSPECTED.

### 14e. Stores that may normalise where retail copies

- `optionsdialog.cpp:745,748`: `inputCfg->_mouseClipping = checkMouseClip->_checked != 0;` and
  `_mouseDisplay` likewise. UI's `EnableCursor` tests `Input::Configuration::_mouseDisplay` as a
  `bool` where retail compares a byte. SETTLED by R-ENG from retail: the three Input::Configuration
  flags ARE `bool` -- `Input::Configuration::Startup` reads them through
  `?Get@Persistence@IO@@QEBA_NPEBD0AEA_N@Z` (0x1800b8290, `AEA_N` = `bool&`), and `EnableCursor`
  0x180122580 tests `CMP byte ptr [RAX+0xa],0` / `CMOVNZ` (0x180122599), a bool's zero test.
  `ApplyInputSettings` 0x140072360 copies the check box's +0xd8 byte RAW: `MOVZX EAX,byte [R8+0xd8]`
  / `MOV byte [RDX+0x8],AL` (0x140072390 / 0x14007239f) and `MOVZX EAX,byte [RDX+0xd8]` /
  `MOV byte [RCX+0xa],AL` (0x1400723b9 / 0x1400723c2) -- a bool-to-bool copy in the source, so
  `CheckElement::_checked` (+0xd8, `uint8_t` in the tree) is the retail `bool`. The tree's `!= 0` is
  value-identical while `_checked` holds 0 / 1; the faithful spelling is the raw copy
  `inputCfg->_mouseClipping = checkMouseClip->_checked;` (DLG's file, not changed). CONFIRMED. Several flags became `bool` in R-UI-b and R-VIDEO
  (`SpriteSheet::m_field50`, `TextDescription::_b84/_b85`, the ribbon / scroll `_vertical`,
  RenderState's six flags). Identical only if the byte is always 0 or 1. SUSPECTED; the settings
  flags were the same question and retail copies raw (0x14007248f), so those are `uint8_t`.

### 14f. Names that mislead (naming only)

- CitizenComponent +0x238 is the HOME and +0x248 the WORKPLACE; the bodies the tree and Ghidra call
  `AssignWorkplace` (0x1401163d0) and `AssignHousing` (0x140111ee0) are the other way round.
- `GameUpdateLoop::m_tutorial` holds a `WorkScheduler`, `m_imageCapture` the `Minimap`.
- `SelectDescription::_ref20` (townhallui reads it by that name); `Profession::priority` (+0x50) is
  a carry / capacity amount; `ClearAreaComponent` +0x48 `clearState` is used as a capacity;
  `ToolUI` +0x40 `hasBorderTitle` is the pin flag; CitizenDescription's `min/maxPregnancyAge` are
  the spawn age range in debugui; the list DebugUI's disaster trigger picks from is
  `CitizenBehavior::citizens` (the tree called it the building list).
- Ghidra labels: `0x140112ae0 CitizenComponent_ConsumeFuel` is the happiness update;
  `0x140114900 CitizenComponent_OnCreate` serializes CitizenDescription; `g_PauseDialog_Vtable`
  holds MainMenuDialog's slots and `g_ReloadDialog_Vtable` MessageDialog's; `0x140115de0`
  ("GetHappiness") is the carry count.
- Two tree bodies both claim retail 0x1400049c0 (`Reimpl_ExternalPtrArray_Serialize`,
  `Desc_LoadExternalArray`).
- `Video::Device::gap50[0x10]` (+0x50 / +0x58) is not a gap: MainLoop's ctor stores the video-error
  and video-restart callbacks there (the profile build's `Video::Device::SetVideoErrorFuncs`,
  0x1800fd860, inlined); `engine.cpp` still writes them as raw bytes because the header (R-VIDEO)
  has no named fields.

### 14g. Commit messages that state something nobody verified

`9a0c288e` (COMP-d: "the next update re-grounds it either way"), `a61be2e2` (COMP-f: the load-time
recompute), DLG-b's GoalDialog commit ("46 -> 0": the base was 70). The code is as described in
the hand-ins; the sentences are not evidence.
