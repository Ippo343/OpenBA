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
| `code/Game/Action/action_base_move.cpp` | `0x1400a6a30`, `0x1400a68b0`, `0x14009c480`, `0x1400a6b20`, `0x1400a6c10` | the door passed `int` arguments; the bodies take pointer-width parameters |
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

## 7. The zone refresh `0x14009c480` is called with a seed WE chose — OPEN

**Where.** `code/Game/Work/foresterwork*.cpp`, `buildstructure*.cpp` (commits `6067e8a1`,
`4b9d6b19`), `code/Game/Game/gamegrid_serialize.cpp` (`fn_Map_Invalidate`), and the Action callers.

**What the code does.** The body `Pf_ZoneRefresh_9c480` takes four parameters and hands the fourth
to the flood fill (`0x14009c170`) as its seed. Retail's callers `0x1400eb303`, `0x1400e532a`,
`0x1400e54ea`, `0x1400e556c` load ONLY RCX before the call. The old door was typedef'd with one
parameter, so our callers left RDX / R8 / R9 as whatever the compiler had there; the direct calls
pass `(grid, 0, 0, 0)`.

**Why it is open.** Either (a) the body's parameter list is wrong — Ghidra drops and invents
arguments on forwarders, and retail's function may take ONE parameter and compute the seed itself —
or (b) retail really reads a stale register, in which case its value at each call site is whatever
the preceding code left there and can be read from the disassembly. Zero is neither.

**What a fix must do.** Read `0x14009c480`'s prologue (which argument registers it reads before
writing them) and `0x14009c170`'s use of the seed. If (a): correct the body's signature. If (b):
pass what the caller's preceding instructions leave in R9.

## 8. Wrong BINDINGS found in `code/Game/Dialog` — SUSPECTED

| door | bound to | problem |
|---|---|---|
| `fn_Game_DestroyEntity` (debug UI "destroy all raw materials", retail `FUN_140060000`) | `0x140091e60` `RawMaterialComponent_RegisterRawMaterial` | the body REGISTERS where the caller means to destroy. Debug UI only |
| `fn_DiseaseComponent_Infect` | `DiseaseComp_Infect(DiseaseComponent*)` | the caller hands it a citizen pointer |
| `fn_StartNewGame` | `StartNewGame_Reg` | the door passed seed and terrain size as `uint32` values; the body takes pointer-width slots and reads their low dword |
| — | `PastureComponent_ComputeCapacity` | returns `uint64_t`; the door read `int` |
| — | `TradeComponent_SpawnPurchasedLivestock` | takes an `unsigned` quantity; the door passed `int` |

Each call keeps today's bits. The first two need retail's call site read before anything changes.

## 9. Bodies declared `void` whose callers read a return value — SUSPECTED

| address | body | what the caller reads |
|---|---|---|
| `0x14009ce00` | `RoadTool_sub_ce00` (road A*) | the path length retail leaves in EAX (`roadtool.cpp`, `roadtool_calcpath.cpp`). The readers were LEFT as resolves by the registry teardown because the signature cannot be settled without retail |
| `0x1400fd960` | — | a return value |
| — | `Pf_RoadMeshRebuild_f370` | a return value |

Today the callers read whatever the body's last computation left in EAX. **What a fix must do:**
read each function's epilogue in retail, give the body its return type and value.

## 10. `0x140172248` is 1000.0f; the code around it says otherwise — SUSPECTED

The image bytes at `0x140172248` are `00 00 7a 44` (1000.0f, read in Ghidra 2026-09-28). A comment
in the placement code calls the cell 0.5, and `code/Game/Player/tool_base_tile*.cpp` hard-codes
`250.0f` where retail reads this cell. **What a fix must do:** read retail's instruction at the
`tool_base_tile` site and use `kRdata_140172248` if that is the operand.
