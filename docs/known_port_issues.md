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
