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

WORK-c, 2026-09-29: the removal path, read against retail. Retail removes a citizen from the
post's tree only through the citizen's action stack: `CharacterComponent::OnRemove` 0x140109980
clears the stack through 0x1400c5400, which calls slot 8 of every chained action; the
`CompleteTask` that `TradeWork::Execute` queued (through 0x1400bf790, its +0x20 = the citizen,
+0x30 = the work id `Work+0x20`) answers with `CheckEntity` 0x1400c5ac0 -> 0x1400f8800, which
finds the work in the scheduler's id map (`WorkScheduler::trees[0]`, +0x30) and calls its slot 7,
`TradeWork::OnCancel` 0x1400f5d10 = 0x14002c690 find-and-remove keyed on the citizen. Slot 6
(0x1400f5d20, the CompleteTask's own completion through 0x1400f8760) removes it the same way.
The tree's port of each link was read and matches retail: `TradeWork::Validate` (0x1400f5ae0),
`Work::ValidateTask` (0x1400f7100), `CharacterComponent::OnRemove`, the stack clear (it re-reads
the array and its size every iteration, as retail), `ActCt_TaskReassign_8800`, `FindTaskNode`
(the same +0x30 tree the scheduler posts into), `TradeWork::OnCancel` / `OnComplete`, and the
chain `Execute` builds (twoWay = 1: the CompleteTask follows the pickup). The ONE difference on
the path: the tree's `CompleteTask::CheckEntity` (`completetask.cpp`) returns early when
`GameUpdateLoop::_singleton` or its +0x118 scheduler is null; retail 0x1400c5ac0 has no such test
(14d). It cannot drop a removal while a town is running. NOT FOUND: where the stale entry comes
from. Two open questions for the fix, neither read: whether the tree's death path (the
`[Burial]` handler in `citizen_lifeevent_handler.cpp` and what destroys the citizen entity after
it) reaches `CharacterComponent::OnRemove` at all, and whether a citizen can hold a post
assignment with no CompleteTask left on its stack (a builder that pushes without clearing first,
e.g. 0x1400c0ad0 with its cancel flag 0, runs on top of the old chain). SUSPECTED, not CONFIRMED.
No `behaviour:` commit; no check added.

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
| 34 | `Engine::PlatformConfigure`'s launcher (`engine_platform_win.cpp`) | reads the two launcher check-box bytes and calls the dialog proc 0x1800a1ae0 at module-relative addresses of a LOADED `Runtime-x64.dll` (`GetModuleHandleW`); the standalone never loads it, so the launcher dialog is never shown and both bytes read 0 | retail runs its own dialog proc 0x1800a1ae0 over resource 0x66 | CONFIRMED from the code, not from retail (R-ENG); a door into the retail binary, LEFT |
| 35 | the worker-count callers in `tradework.cpp`, `plantarea.cpp`, `plantorchard.cpp`, `tendarea.cpp` | call `WorkComponent_CalcMaxWorkers` (`tutorial_base.cpp`) | one body, 0x14012e5c0; the tree has a second port of it, `WorkComponent::CalcMaxWorkers` (COMP-f), and the two were not compared, so which one differs from retail is unknown | SUSPECTED (WORK-c) |
| 36 | `StatusBarUI` slots 4 and 8; `StorageUI`, `StorageCapacityUI`, `StatusIconUI` slot 8 | overridden: slot 4 runs OnOpened; slot 8 rebuilds the panel and refreshes it (StatusBarUI every 30th frame, counted in the +0x104 gap) | inherits DialogController's (StatusBarUI table 0x14015d810); the refresh is slot 7 (StatusBarUI 0x1400798d0) | CONFIRMED (DLG-d) |
| 37 | `StatusBarUI::OnOpened` element finds | stores every FindElement result unchecked, the hearts / stars groups and labelTitleParam included | 0x14007a3d0 narrows with IsDerivedFrom; the tree's comment says the group check's id was read as ButtonElement and rejected the element | SUSPECTED (DLG-d; not re-read) |
| 38 | `CitizenUI_BuildIconArray_Impl` (`citizen_icon_helpers.h`; CitizenUI and StatusBarUI call it) | a second body of 0x14006abf0: pushes the "whole" / "half" find results without narrowing and returns on a null group | 0x14006abf0 narrows each to ImageElement (null when not one) and reads the group unguarded; `TownHall_GroupRatingInit` (`townhall_helpers.cpp`) is the faithful copy | CONFIRMED (DLG-d) |
| 39 | `TownHall_InventorySortAsc` / `Desc` (`townhall_helpers.cpp`) | a stable insertion sort | 0x140015200 / 0x140015500 are an introsort: rows with equal keys may come out in another order | SUSPECTED (DLG-d; the tree's note, not re-read) |
| 40 | `TitleDescToolTipUI::OnCreated` (`titledesctooltipui.cpp`) | takes "textTitle" / "textDesc" and the data source as found | 0x14007b730 narrows both texts with IsDerivedFrom (the id Ghidra labels LabelElement's, read through a mislabelled import) and the data source to UI::Element | CONFIRMED (DLG-d) |
| 41 | `SpriteCaptureUI::OnElementEvent` (`ui_dialog_stub_factories.cpp`) | a no-op (PORT GAP): the capture button starts nothing, so OnUpdate's save arm is never reached | 0x140079620 (not ported) | CONFIRMED (DLG-d; the gap, not the body) |
| 43 | the Work bodies' worker cap (Forester, GatherRawMaterials, HarvestArea, HarvestOrchard, Herbalist, Hunt, MarketWork, Pasture Execute) | call `WorkComponent_CalcMaxWorkers` (`tutorial_base.cpp`), a second port of 0x14012e5c0 that differs from the member `WorkComponent::CalcMaxWorkers` (which footprint component it reads, FloorComponent vs MapComponent, and its guards) | one body, 0x14012e5c0 | SUSPECTED (WORK-b; the two ports compared, retail not re-read; row 35 is the same pair seen from WORK-c's callers) |
| 44 | `PastureWork::OnCancel` (`pasturework.cpp`) | tests the entity's description IsDerivedFrom AnimalDescription and returns when it is not; calls `ActHunt_NotifyEntityChanged` when bit 20 changed. Net: the tree never clears bit 20 of the AnimalComponent's flags | 0x1400f22c0 checks against ComponentEntity (cell 0x1401c5d70) and passes null on failure, finds the AnimalComponent with no null test, always clears bit 20 (`BTR`), and tests `(old ^ new) & 0xFFFF`, always 0 for bit 20, so it never notifies | CONFIRMED (WORK-b, disassembly read) |
| 45 | `ShowCitizenTool` toolId < 0 (`action_base_complete.cpp`) | reads the qword at ModelDescription +0x28 (`_meshGroupCount` / `_meshGroupCap`) as a pointer and dereferences it once | 0x1401173b0 reads `*(*(desc+0x20)+8)` and dereferences that: mesh 0 of mesh group 0 (one more dereference) | CONFIRMED (ACT-a) |
| 46 | 0x1400c3470 has two tree bodies | `ComponentEntity::RefreshEntityFlags` (`componententity.cpp`, WORK-a) skips null component slots and appends through `Collection::Array::SetSize`; `NotifyEntityChanged` (`action_base_hunt.cpp`, the `ActHunt_NotifyEntityChanged_c3470` spelling's body) skips nothing and appends through the 0x140003110 port | one body, no null skip | SUSPECTED (ACT-a; retail not re-read for the skip) |
| 47 | `PathfinderThread::ApplyCommand` case 5 (`action_pf_astar.cpp`) | holds the lock through the ready store | 0x1400a6430 unlocks after the result-array resize (0x1400a658b) | CONFIRMED (ACT-a) |
| 48 | `PathfinderThread::ApplyCommand`'s 64 KB path buffer | `static` (one worker thread uses it) | on the stack (0x1400a6436) | CONFIRMED (ACT-a) |
| 49 | `RegionPairCache::Store` (`action_pf_region.cpp`) | writes 0 to `RegionPairEntry::field_0x0C` | 0x1400a5b30 stores a stale stack dword there (never read) | CONFIRMED (ACT-a) |
| 51 | `PerformWork` ctor, tool-less time scaling (`performwork.cpp`) | reads the low dword of `requiredTime` (+0x30) and of `Time(desc->_f114)` as float bits and multiplies them | 0x1400c9720 calls `Time::AsSeconds` (IAT 0x140151318) on both (0x1400c9808 / 0x1400c9814), multiplies, and rebuilds with `Time(float)` (0x140152b00) | CONFIRMED (ACT-b; the two puns are LEFT at the site) |
| 52 | `RemoveAnimal::Persist` (`removeanimal.cpp`) | validates the loaded +0x20 against `FireDescription::TypeId()` (cell 0x1401c4218), so a loaded RemoveAnimal always loses its entity | 0x1400ca870 tests ComponentEntity (`MOV EDX,[0x1401c5d70]` at 0x1400ca8e1) | CONFIRMED (ACT-b) |
| 60 | the main menu's camera frustum (`MenuScene_Helper_8660`, `menudialogloopt.cpp`) | a second port of 0x140098660 next to `PlayerController::UpdateCamera`: takes the width / height from `Video::Configuration` +0x04 / +0x08, which are `interfaceIndex` / `adapterIndex` (so the aspect is 1 / 0 -> the 16:9 fallback, or interface / adapter), and has no ultrawide clamp | 0x140098660 reads `Video::Device`'s back-buffer width / height (+0x04 / +0x08) and clamps fovX past 16:9 (the member's body, `gameupdateloop_load.cpp`) | CONFIRMED from the two bodies and the header (GAME-b); retail not re-read for the menu's call |
| 61 | `WorldGeometry::Serialize` step 3, the +0xD8 height words (`geometry_serialize.cpp`) | writes / reads them through `Serializer::Serialize(n, unsigned short*)` (the ushort array slot, +0x50) | 0x1400b04d0 calls import slot 0x1401523f8, `Serialize(n, short*)` (+0x58) | CONFIRMED from the slot comment kept in the body (GAME-b); the binary reader / writer write two bytes per element for both, so the save bytes are the same; a text encoding would differ |
| 62 | `MenuScene::Serialize` (`menuscene.cpp`) | calls `WorldGeometry::SerializeLegacyBatches` (0x1400af1a0) unconditionally after `WorldGeometry::Serialize` | 0x1400a36da..0x1400a36f0: `GetVersion` of the outer serializer, `CMP EAX,1; JL` past the call — only version >= 1 streams carry the legacy batches | CONFIRMED from the disassembly (GAME-b) |
| 63 | `Reimpl_Array_Serialize_Stride20` (the menu scene's 0x20 mesh records, `serialize_arrays.cpp`) | writes / reads each record as eight `Serialize(uint32&)` words | 0x14001bbf0 serializes seven floats and two shorts | CONFIRMED from the body's own comment (GAME-b); the binary encoding writes the same bytes; a text encoding would differ |
| 70 | `PlacementTool::CreateAt` (`tool_base_tile.cpp`) | passes 250.0f as the DAT_140172248 argument of the footprint test 0x14009d580 (the old comment said 0x447a0000 == 250) | the cell holds 0x447a0000 = 1000.0f (read 2026-10-02); `placementtool.cpp` already uses 1000.0f for the same cell | CONFIRMED (PLAYER) |
| 71 | the tool registry node (`ToolRegistryNode`, `Dialog/toolbardialog.cpp`, DLG-d's) | 0x30 bytes, no balance field | the node allocator 0x14002df10 allocates 0x38 bytes with a balance byte at +0x30; `tool_activation_standalone.cpp` reads it through its own 0x38 `ToolModeNode` | CONFIRMED (PLAYER); header of a finished block, left for the coordinator |
| 72 | `TrackedEntityNode::key` (`PlayerToolManager.h`, GAME-b's) | typed `ComponentEntity*` | 0x1401001f0 stores a dialog description there | CONFIRMED (PLAYER); naming / typing only, GAME-b's header |
| 73 | `Tool::dialogPtr` (+0x10, `Tool.h`) | the name says dialog | holds the owning `PlayerToolManager` (SelectTool 0x140102b60 stores it at 0x140102dc2); B4 typed it `PlayerToolManager*` and kept the name (14f) | CONFIRMED (PLAYER); naming only |
| 74 | guards the PLAYER bodies keep that retail lacks | `FencedTool::Startup`'s missing-description bail, `BridgeTool::Draw`'s two cell-range checks, `RemovalCalc_helper_e4d0` / `_dfa0`'s missing-StateMachine bail, EditTool's citizen-position fallback (`TryProjectEntityModelPos`) | retail dereferences / has no fallback | CONFIRMED from the tree's own comments; kept as they were, no new guard added (PLAYER) |
| 75 | the tool overlay arrays of `fencedtool.cpp` (FencedTool::Draw) and `pathtool.cpp` (PathTool::Draw) | FIXED in B4 (code change inside `c757fe4b`, recorded by its `behaviour:` commit): the tree's hand-written arrays grew to n*3 even from empty (first record: capacity 3); they are now `Collection::Array` (first record: capacity 1) | retail's inlined grow is capacity n, trebled only on a one-record append to a non-empty array: 0x1400dc542-0x1400dc54d / 0x1400dc942-0x1400dc94e (FencedTool), 0x1400dd47c-0x1400dd487 (PathTool tiles; segments the same from 0x1400dd5d0). Allocation sizes only; every record is the same | CONFIRMED (PLAYER), retail read 2026-10-02 |
| 76 | `FencedTool::CalcCost` (`fencedtool.cpp`) calling the anchor calc 0x1400dbf90 | passes `&anchorMinCol` as the 7th argument (the yaw out), the same slot as the 2nd; the anchor calc stores the yaw's bits there BEFORE its rotation-2 snap reads `*param_2`, so the snap and the two terrain queries after it run on the float bits as a column (only the anchor position is affected, which CalcCost never reads) | passes a separate stack slot, `[RBP+0x58]` (`LEA RAX,[RBP+0x58]; MOV [RSP+0x30],RAX` at 0x1400db7eb), so the corners are never overwritten | CONFIRMED (PLAYER); LEFT, not fixed in B4 |
| 90 | `WeatherParticleGeometry::Configure` (`weather_particles.cpp`) | draws the particles' positions and speeds from `GameUpdateLoop::m_random` (null-tested) | 0x1400d3f80 constructs a LOCAL `System::Random` on its stack (`CALL [0x140152b50]` on `RBP-0x80` at 0x1400d3fde) and draws `RandFloatTol` (`[0x140151400]`) from it: the world Random is not advanced | CONFIRMED (GAME-c) |
| 92 | `WorldGeometry::BuildTerrain`'s material resolve (`wg_ResolveMaterial_fd10`, `worldgen.cpp`) | resolves each of TerrainGenerator's three triples with a bare `ExternalFactory::Find` of the name: no IsDerivedFrom test, no generation cache, the triple itself never updated | 0x1400b0230 copies each triple (String, generation, cached) and runs 0x14001fd10 on the copy (generation-cached Find, IsDerivedFrom MaterialInstance), storing the copy's +0x10 into +0xF8 / +0x150 / +0x158 | CONFIRMED (GAME-c) |

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
| 42 | `NoiseUI` dtor (`NoiseUI.h`, DLG-b's) | `= default`: the ImageBuffer, MaterialInstance and SpriteSheet OnCreated (0x1400714c0, wired by DLG-d) makes are never freed | 0x140071bc0 deletes +0x40, +0x38, +0x30 through slot 0 (ModDialog's dtor has the same three `delete`s) | CONFIRMED (DLG-d); a header line of a finished block, left for the coordinator |
| 50 | `PathfinderThread` (`PathfinderThread.h`, ACT-a) | the +0x160 result array is a trivially-constructible `RawArray<PathResult>` (H5) and the +0x30 lock a trivially-constructible `System::CriticalSection`, so declaring them runs no ctor / dtor; the tree's ctor / dtor bodies run exactly what they ran before | the class's ctor 0x1400a67f0 / dtor construct and destroy both members | SUSPECTED (ACT-a; the H5 rule of the coordinator's condition 1) |
| 91 | `WeatherParticleGeometry::Configure` (`weather_particles.cpp`) | allocates the +0xD0 index block with a raw MemAlloc and stores data and count; the capacity (+0xDC) is never written | 0x1400d3f80 sizes +0xD0 through 0x14001c890 (`Array<uint16>::SetSize`, data / size / capacity) | CONFIRMED (GAME-c) |
| 104 | `Picking::Scene` grid temporaries (`scene_grid.cpp` EntryAlloc / GridFreeEntry) | B.4 made them real `Entry` objects: their `~Array<Transform>` (= `rtcoll::ArrayDtorImpl`) is retail's closing CALL 0x18001c100, but it hands `MemFree` the pointer unconditionally, so an installed allocator instrument (`BANISHED_REDZONE`) now sees a `MemFree(nullptr)` where the old `ArrayDtorTransform` tested first | 0x18001c100 tests `_data` before the Enter/Free/Leave triple; every other `~Array` in the tree already behaves like the new form | CONFIRMED from the two bodies (R-CORE); no game-visible effect |

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
- KEPT: `Input::Device::PlatformStartup` tests `Application::_singleton` for null before reading
  `_windowHandle` (twice); `KeyMap::Load`'s settings lookup tests the Persistence pointer before the
  root; `Input::Device::PlatformShutdown` skips the three callback erases when `Application::_singleton`
  is null (its comment says why). Retail loads each unguarded. SUSPECTED for the first two (not
  re-read), CONFIRMED by the file's own banner for the third (R-ENG).
- DROPPED: `ComponentEntity::FindComponent<T>()` does not skip a null component slot and does not
  test the type id for 0 / -1 (retail's per-type instances do neither: 26 read by block T). Safe
  while `ComponentEntity`'s ctor never stores a null slot (it skips a component with no factory) and
  every `T` is registered. PastureUI's second copy of ComputeCapacity went with its two guards
  (`behaviour:` 0x140128570).
- DROPPED WITHOUT READING RETAIL: the MemAlloc-failure path of `TickIdle` (COMP-f). SUSPECTED.
- KEPT (WORK-c): `Work::InitFromEntity`'s null MapComponent / map tests; `Work::TryAutoPickup`'s
  null AutoPickupComponent and null-singleton returns (0x1400e3db0); the action-chain builders'
  `derivedTarget` arms (0x1400c1eb0: PerformWork / MoveTo / ShowTool are built empty when the
  target has no NaturalResourceDescription) and `derived` arms (0x1400c1530), the `targetIBP` and
  singleton tests of 0x1400c0ad0, the `condPtr8` / `msgIdPtr9` null arms of 0x1400be6e0 and the
  `param_9` null arm of 0x1400bef70; `CompleteTask::CheckEntity`'s null singleton / scheduler
  return (retail 0x1400c5ac0 has none; see entry 13). All SUSPECTED except the last (CONFIRMED).
  0x1400bef70's two reads through the null receiver ([0+0x50], [0+0x28]) stay unguarded, as retail.
- KEPT (DLG-d): StatusBarUI's Bound / Ready / TownReady / Built gates on slots 5, 7 and 9, its
  test of textSeason's string table and of a null `g_gameSettings` (0x1400798d0 has none); the
  status-icon handler's null game / town / citizen / StatusIconComponent tests and its
  `count <= 0` return (0x14007a120 has none); ToolUI's dialog-address windows
  (`ToolUI_DialogHasEntityUI`, OnUpdate); `PlayerToolManager::SelectTool`'s empty-registry return
  (retail goes straight to Factory::CreateObject) and the game-world / text-element tests in
  `toolbardialog.cpp`; SpriteCaptureUI's Input::Device, capture-box, FileSystem and edit-element
  tests; StatusIconUI's EntityUI-pages test. CONFIRMED where the site quotes retail.
- KEPT (DLG-d): `TitleDescToolTipUI::OnCreated`'s unbound-controller return and its null tests of
  the two texts (0x14007b730 has none).
- KEPT (WORK-b): `HarvestArea::Execute` skips a cell whose occupant has no NaturalResourceComponent,
  where retail 0x1400eccd0 reads its +0x20 unguarded (0x1400ed082), CONFIRMED; `HarvestArea::Cleanup`
  breaks when a cancel does not drop the count, where retail 0x1400ed270 loops on `+0x50 > 0`
  (0x1400ed2cb), CONFIRMED; the null-slot and null-entry skips of `PG_FindComp` /
  `PG_FindRawMaterialDesc` and `CitizenBehavior::HasStorageFor` (`pickup_gate.cpp`, 0x14008d3c0 /
  0x14012d990); PastureWork's guarded finder 0x1400012d0; `Pickup` slot 5's freed-self vptr test;
  the GatherRawMaterials site-ctor guards; the HarvestOrchard ctor's orchard / array / slots guards;
  `CitizenComp_StorageComponent_QueryCount`'s null test. SUSPECTED for the last five (retail not
  re-read in B.4).

- KEPT (ACT-a): `InteractNodePosition`'s SA-GUARD zero vector and ModelComponent fallback
  (0x140122e20 reads both unguarded); `EntityAnchorCell`'s null-entity diagnostic return
  (0x140087fb0); `InteractNodeCount`'s null-description 0 (0x140123070); the null component-slot
  skips of `StorageCanAdd` / `PickBestDropTarget` (`FindComponentSkippingNull` /
  `FindDescriptionSkippingNull`, `action_pf_roots_route.cpp`) and `PickBestDropTarget`'s
  StorageLocationDescription and source-mask tests (0x14008d590); `StorageCanAdd`'s missing
  RawMaterialDescription weight 0 (0x14012cd80); `FindCharacterComponent`'s type-id test
  (0x1400012d0); `HerdStartMigration`'s null-member skip (0x140122340); `PlayModelAnimation`'s
  null-model and type-miss returns (0x1401250f0); `HuntIsReady`'s null description (0x140122a80);
  `ResourceCategory`'s missing-description -1 (0x140088f90); `PositionRawMaterial`'s MapComponent /
  ModelComponent tests (0x140129760); `HeadingToward`'s null world / map (width 1, 0x1400c29a0);
  `TaskCleanup`'s missing-bucket test (0x1400f8e30, see the WAVE 9 note at the site); the
  `DropRawMaterialOnGround` Random test. SUSPECTED unless the site quotes retail.
- DROPPED (ACT-a): 0x140107260, 0x1400c7620 and 0x1400bbea0 find the CharacterComponent through
  `ComponentEntity::FindComponent<CharacterComponent>` instead of the guarded 0x1400012d0 copy (the
  type-id test is dead once the type is registered); `StorageCanAdd` sums the stored weight through
  `StorageComponent::QueryCount` instead of `CitizenComp_StorageComponent_QueryCount`, whose null
  test is unreachable there (0x14012c560 has already dereferenced the storage).
- KEPT (ACT-b): `AnimateNaturalResource::Execute`'s null-ModelComponent return (0x1400c55b0 reads
  +0x28 unguarded); `CompleteTask::AbstractImpl`'s null singleton / `m_tutorial` return
  (0x1400c5b40); `CreateRawMaterial::AbstractImpl`'s zero-batch return (0x1400c6c22 divides by it);
  the `MoveTo` ctor's `charDescTid &&` and `random ?` tests (0x1400c9170 has neither);
  the `PerformWork` ctor's null-CitizenDescription skip of the scaling (0x1400c97e2..0x1400c97ee:
  CMOVNZ picks null on an IsDerivedFrom miss and +0x114 is read unguarded, CONFIRMED);
  `PickupStoredRawMaterial`'s null-RawMaterialDescription guards in the ctor and AbstractImpl
  (0x1400ca520 / 0x1400c9d40 read +0x48 unguarded); `PlayAnimation::AbstractImpl`'s null-target
  return, readable-memory stale-target probe and null-CharacterComponent return (0x140042420);
  `RemoveNaturalResource` AbstractImpl / CheckEntity null-NaturalResourceComponent returns
  (0x1400cab10 / 0x1400caaa0); `SendComponentMessage::AbstractImpl`'s canonical / dying /
  readability probes and sentinel-slot skip (0x1400caef0); `ShowTool`'s null-citizen returns in
  AbstractImpl / CheckEntity / GetTarget (0x140042710 / 0x1400426d0 / 0x140042680) and
  `ShowTool_ShowCarried`, a second guarded port of 0x1401173b0's toolId < 0 branch beside
  `ShowCitizenTool` (row 45 is the other copy's difference); `Pf_RegionPairCacheTick_5d10`'s null
  tests. SUSPECTED unless an address range is quoted.
- DROPPED (ACT-b, seen in B11): ConsumeRawMaterial AbstractImpl / CheckEntity, DropOffRawMaterial
  AbstractImpl (two finds), PickupRawMaterial AbstractImpl and PickupStoredRawMaterial AbstractImpl
  / CheckEntity find the StorageComponent through `ComponentEntity::FindComponent<StorageComponent>`
  instead of `fn_FindComponent_Storage` (`entity_findcomponent.cpp`), whose null-entity return and
  null-slot skip retail's 0x140006270 lacks (the tree's own 0x140006270 forwarder,
  `WorkBase_FindComponent_6270`, already delegates to the template). A null entity there now
  faults where the old body returned no storage.
- KEPT (GAME-b): `ProfessionList::Startup` returns on a null `ExternalFactory::_singleton` and a 0
  Profession type id, and skips an external that is not a Profession; 0x1400a90a0 calls FindNext
  on the singleton unguarded and reads +0x4e of the null it substitutes for a non-Profession.
  CONFIRMED. DROPPED (GAME-b): the old body's null test of the list itself (its one caller,
  `GameUpdateLoop::Startup`, tests `m_professionList` first; retail has none).

TUT (rows 80-84, 88-89):

| # | where | the tree | retail | status |
|---|---|---|---|---|
| 80 | `WorkComponent_CalcMaxWorkers` (`tutorial_base.cpp`, the B4 FORWARD for 21 outside callers) | a second port of 0x14012e5c0 that adds two guards before the member's body: `WorkDescription::_i30 == 0` and a missing MapComponent each return the default worker count `_i24` | 0x14012e5c0 has neither test; `WorkComponent::CalcMaxWorkers` (the member) matches it and the forwarder now calls it after the guards | CONFIRMED (TUT); kept so no caller's result moves |
| 81 | `WorkPlaceComponent::GetMaxWorkers` (`tutorial_base.cpp`) | returns 0 when the entity has no WorkComponent | 0x14012fbb0 reads the found component without a null test | CONFIRMED (TUT) |
| 82 | `FindOnEntity<T>` in `tutorialgoalcount.cpp` (TutorialGoalCount::IsComplete 0x1400df290) | returns null for a null entity before `FindComponent` | not read | SUSPECTED (TUT) |
| 83 | `TutRestrict_ResolveRefOrLoad` (`tutorialsetuprestrictplayer.cpp`) | after the find-only resolve misses, normalises the name and calls `ExternalFactory::FindOrLoad` (the site's STANDALONE DEVIATION note) | resolves both refs with 0x14001b1d0, find-only | SUSPECTED (TUT; the site comment quotes the address, not re-read) |
| 84 | `NamedExternalRef::Resolve` (`goal_exe_helpers.cpp`, 0x1400265b0) | the same load-on-miss (`FindOrLoad` of the normalised name) and a re-read of the factory generation after it | find-only, one generation read | SUSPECTED (TUT; not re-read) |
| 88 | `TimeGoalInstance::CheckGoalState` (`timegoalinstance.cpp`) | tests `goal` for null before IsDerivedFrom and the narrowed goal for null before reading `requiredYears` (the miss returns 0) | 0x1400dacb0: `CMOVNZ RBX,RDI` then `MOV ECX,[RBX+0x30]` -- no test; a non-TimeGoal reads through null | CONFIRMED (TUT) |
| 89 | `TutorialHighlightDialog::PlaceOnElement` / `PlaceOnEntity` (`tutorialhighlightdialog.cpp`) | null tests on `glow`, `glowGroup`, `arrow`, the target element, the Video::Device singleton and the UI | not read | SUSPECTED (TUT) |
| 77 | `PathTool::ClearHighlight` / `SetHighlight` / `BuildRoutes` (`pathtool_setselected.cpp`, 0x1400dd250 / 0x1400dd0f0 / 0x1400dd770) | B4 dropped the file-local finder's null-component-slot and zero-type-id skips (the finder is now `ComponentEntity::FindComponent<HighlightComponent>()`, no null test); the route builder's A* wait still gives up after 20,000,000 spins (KEPT, the tree's standalone safety) | 0x140007080 has neither skip: it calls the slot's `Type()` through `[RCX]` unguarded (0x1400070c0-0x1400070cc) and compares with DAT_1401c4410 without testing it for zero, so the drop matches retail; retail's wait has no bound (0x1400ddc40-0x1400ddc76 loops on `Application::YieldToThreads` until the slot is ready) | CONFIRMED (PLAYER-3, retail read 2026-10-02): finder drop = retail; the spin bound is a tree guard retail lacks, LEFT |
| 78 | `AddDecal` (`tool_base_highlight.cpp`, the decal slot manager 0x1400b0d80) writing GeometryBatch +0x44 / +0x5D | stores the caller's raw byte through a `uint8_t*` cast (LEFT): `GeometryBatch::field_0x44` / `alternateKey` are `bool` (GAME-b's WorldGeometryElements.h) and a bool store would normalise a byte that is not 0 / 1 | retail copies the byte (the tree's port of 0x1400b0d80); HighlightAdd 0x140055430 passes 0 / 1, `decalcomponent.cpp`'s caller passes its own bytes | SUSPECTED (PLAYER-3; retail's store width not re-read); field type is GAME-b's, LEFT |

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
- (ACT-b) Now `bool`: `AnimateNaturalResource` +0x2C (0x1400c5660 serializes it as bool),
  `ConsumeRawMaterial` +0x38, `CreateRawMaterial` +0x50 / +0x51, `DropOffRawMaterial` +0x20,
  `HuntAnimal` +0x38, `PerformWork::useTool` +0x40, `PickupStoredRawMaterial` +0x40 and
  `RemoveNaturalResource` +0x30. The typed ctors copy the byte (B11: `movzbl` / `mov %dl`, as the
  old ctors did); the NORMALISATION (`setne`) is at the seven `uint8_t` -> `bool` conversions of
  the C entry points, where retail's ctor copies the byte raw:
  `animatenaturalresource.cpp:145` (0x1400c57d0), `removenaturalresource.cpp:218` (0x1400cadf0),
  `dropoffrawmaterial.cpp:276` (0x1400c7cd0), `consumerawmaterial.cpp:392` (0x1400c63b0, raw copy
  read at 0x1400c640f), `pickupstoredrawmaterial.cpp:421` (0x1400ca520),
  `createrawmaterial.cpp:379` (0x1400c71a0) and `performwork.cpp:252` (the `void*` ctor reached
  through `Pf_PerformWorkCtor7_9720`, 0x1400c9720, raw copy read at 0x1400c9796). Retail not
  re-read for the other five. H8 checked in B11: every caller of the seven passes a literal 0 / 1
  (directly, or through `CitizenAI_PerformWorkCtor4_c1070`'s 1 / 0, `WorkAct_GatherRawMatCtor_bf790`'s
  `useAnimal` / `param_14` 0 / 1, `WorkAct_PerformWorkCtor_c1eb0`'s 0 / 1), so no stored value
  differs from the old tree's. The array appends of ConsumeRawMaterial /
  PickupStoredRawMaterial keep retail's `&array[i] != nullptr` test, which an -O2 compiler may fold.

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
- `Action::_entity` (+0x10, ACT) is the NEXT action of a chain wherever the action stack is walked
  (0x1400c5400, the inlined appends of 0x1400c1530 / 0x1400bef70, 0x1400c4f10). (WORK-c)
- The Work action-chain builders are not constructors: `WorkAct_*Ctor_*` (0x1400c1eb0, 0x1400c0ad0,
  0x1400c2490, 0x1400bb0a0, 0x1400be6e0, 0x1400ba9f0, 0x1400bffd0, 0x1400bb7b0, 0x1400beae0,
  0x1400bef70) each clear the citizen's stack and push a chain; Ghidra's `ShowTool_Action_Ctor4`
  (0x1400c1530) and `Pathfinding_FindPath` (0x1400bf790) are two more.
  `WorkBase_TaskListCleanup_c5400` is CharacterComponent's action-stack clear, `WorkBase_FindComponent_6270`
  is FindComponent<StorageComponent>, and `Entity_FindConsumeProduce_b6f0`'s old label
  "Entity_FindStateMachine" is wrong. `Work::field_20` is the work id the scheduler keys on,
  `field_28` the target cell. (WORK-c)
- INVENTORY_A's `ToolUI_ChildDialog` is `UI::DialogInstance` (the exe's copy of its table,
  0x14015f5a0); `fn_FindComponent_MeshInstance` finds the StatusIconComponent (cell 0x1401c7e08);
  Ghidra's `Entity_impl_a120` is StatusBarUI's status-icon handler 0x14007a120 and
  `ToolUI_helper_e100` is `UIDescription::AddControllers` 0x14012e100 (DLG-d).
- Work (WORK-b): `Pickup::candidateList` is an `ExternalPtr<Profession>`, not a list; the
  `citizenEntity` locals of `Pickup` slot 5 and the pickup gate hold a Profession; the finder
  forwarders `WorkBase_FindComponent_2f90` ("MeshInstance") and `WorkBase_FindComponent_2ed0`
  ("Position") find the StatusIconComponent and the InteractComponent; `WorkAct_HarvestAreaCtor_c1530`
  is named for its caller. HarvestArea / HarvestOrchard / Herbalist `Cleanup` walks the LEFT child
  (the tree's comments said "rightmost"; corrected in the converted files).

- `Action::_entity` is now `Action::_next` (ACT-a renamed it; the WORK-c row above predates it).
  `TrackerComponent::displayed` (+0x28) and `produced` (+0x38): 0x140058fd0 upserts into +0x28 and
  copies it into +0x38 when the year changes, the opposite of the header's comments. The
  `Pf_EntityResolvePos_2e20` / `Pf_RecursiveChildSum_123070` pair work on the InteractComponent
  node tree (`InteractNodePosition` / `InteractNodeCount`), and `ActCNR_NR_*` on the ModelComponent
  mesh selection (`ModelMeshGroupCount`, `ModelMeshCount`, `ModelSelectMesh`), not on a
  NaturalResourceComponent. (ACT-a)
- (ACT-b) `CreateRawMaterial` +0x28 is the working CitizenComponent, and the yield picks
  `_lowCreateCount` while its `education` < `CitizenDescription::maxEducation` (the tree said
  building age / maturity); `RemoveNaturalResource`'s "growth value / threshold" are the same pair
  and +0x248 is the workplace. `HuntAnimal` +0x20 is the HuntComponent and +0x28 the hunting
  citizen (the tree: "animal", "herd"); 0x140005500 finds the AnimalComponent and 0x1400227e0 the
  WildAnimalComponent (the tree: a model component, "HerdComponent"); 0x1400052c0 is
  NaturalResourceComponent and 0x140003f60 ModelComponent (createnaturalresource.cpp's old banner
  swapped them). `MoveTo`: +0x468 plays when a path starts and +0x46C when it ends (the tree's
  idle / walk were swapped); +0x470 ("bridge flag") is SetMovementCosts' profile; +0x474 ("road
  flag") is set when the nearest other character within 0.5 faces this one; +0x30 ("path data") is
  the CharacterDescription; +0x20 / +0x24 are a PathTicket. `PickupStoredRawMaterial` +0x20 is the
  carrying citizen and +0x28 the storage entity. `ShowTool_ShowCarried`'s "+0xb8 +0x120 sceneId"
  store lands on `CitizenComponent::toolId` (+0x1D8), its "StateMachineComponent" is
  `CitizenComponent::character`, and its scale Vector4 is `MeshInstance::_color`. The function the
  tree called `Game::UpdateTimedEvents` (0x1400a5d10) is `RegionPairCache::Expire`.

TUT (rows 85-87):

| # | where | the tree | retail | status |
|---|---|---|---|---|
| 85 | `FinishTutorialInstance::Startup` (`finishtutorialinstance.cpp`) | read `Video::Device` +0x04 as the screen "height" | +0x04 is `backBufferWidth` (the R-VIDEO header); the body now reads it by that name, value unchanged | CONFIRMED by the header (TUT) |
| 86 | `PlayerToolManager` +0xA4 / +0xA8 / +0xAC (`field_0xA4..AC`, PLAYER's header) | `TutorialSetupCamera::Begin` calls them the camera position flag / x / y; `TutorialSetupRestrictPlayer::Begin` called the same three fields "camera restriction bounds" | both bodies store the same values there (0x1400e20d0, 0x1400e2f20); the fields are unnamed | SUSPECTED (TUT); the name belongs to PLAYER |
| 87 | `Cemetery_CountBurials_6dd0` (`statvaluedispatch.cpp`) | a file-local copy of the cemetery burial count | `cemeteryui.cpp`'s static `CemeteryGetBurialCount_Impl` (DLG-a) ports the same count; two copies | SUSPECTED (TUT); not merged (no behaviour change) |
| 100 | `RtObject::TypeNode` +0x48 (`TypeInfo.h`, R-IO's header) | one `int32_t balance` (commented GUESS) and `_pad4c` | two int16: balance at +0x48 and height at +0x4a (`MOVZX ..,word [child+0x4a]`, word `SUB` / `CMOVG` in 0x180030540 / 0x1800307b0 / 0x1800308e0, as `avl_trees.cpp` transcribes them) | SUSPECTED (R-CORE; the tree's transcription, retail not re-read); `avl_trees.cpp` reads them through its two offset accessors, LEFT |
| 101 | `kSpec_Rec14` (`array.cpp`) | its banner calls the 0x14 element `{Array<T> header, int32}`, a ParticleSystem pool record's inner array; `Particle.h` names the same stride `ParticleExtra` (velocity / rotation / size rates) and points at kSpec_Rec14 | one of the two readings is wrong; R-CORE typed the init arm over `ParticleExtra` (a qword and a dword of zero either way) | SUSPECTED (R-CORE) |
| 102 | `GraphicsMesh::TriangleList` (`GraphicsMesh.h`, R-GFX) | a 0x18-byte struct of six int32 | the element 0x18005d840 / 0x180013c50 / 0x1800fefc0 build is 0xb0 bytes: String, 10000.0f, four Video buffers, Array, int (`array_graphics_mesh.cpp`); its casts stay LEFT until it has a type | SUSPECTED (R-CORE; from the tree's transcription) |
| 103 | `RtIO::PackageFileEntry::name` (`FileSystem.h`), `RtObject::ExternalArrayElement::owned` (`External.h`) | `RtSystem::String` / `char*` | both are copy-constructed with `??0Filename@System@@QEAA@AEBV01@@Z` (0x1800067d0, at 0x1800299bd / 0x18002c773), i.e. retail's field is a `System::Filename` | CONFIRMED from the call the tree ports (R-CORE); the two Filename views in `array.cpp` stay LEFT |
| 120 | `MenuDialogLoopT::_dlcShip` +0xBC8 (`MenuDialogLoopT.h`, GAME-b) | commented `TradeWork* (DLC)`, kept `void*`; `Shutdown` (menudialogloopt_stubs.cpp) removes it from the scene and deletes it as a `Graphics::Geometry` | MenuDLT::Load builds it with MemAlloc(0xF0, 8) + the exe ctor 0x1400D3F10 (0x14009AC83..0x14009ACA0), configures it through 0x1400D3F80 and adds it as a scene global object (0x14009ACF0): a 0xF0-byte scene object, not a TradeWork; its class is not settled | CONFIRMED (STANDALONE) the construction; the class name is SUSPECTED |
| 121 | CitizenComponent +0x48 (`standalone/citizen_vitals.cpp`) | the harness calls it `thirst` and prints it as such | the header (W0-H1) names it `coldness` (`+= dt * desc->coldRate[clothingLevel]`); retail not read by this block | SUSPECTED (STANDALONE); the harness text was kept (log lines are grepped) |
| 122 | `Graphics::Geometry` +0x20 (`standalone/scene_watch.cpp`, SceneWatch_Census) | the harness calls it "the counter SetTransform (0x1800155a0) tests before dispatching the box refresh" and counts `noRefresh(+0x20==0)` | the header (`Geometry.h`, R-GFX) names +0x20 the size word of `_attached` (`Array<Geometry*>` at +0x18, grown by Attach 0x1800a3fa0); retail not read by this block | SUSPECTED (STANDALONE-2); read by the header's name, the harness text kept |

### 14g. Commit messages that state something nobody verified

`9a0c288e` (COMP-d: "the next update re-grounds it either way"), `a61be2e2` (COMP-f: the load-time
recompute), DLG-b's GoalDialog commit ("46 -> 0": the base was 70). The code is as described in
the hand-ins; the sentences are not evidence.
