# Exe close-out — lane D: residual gap triage

Branch `worktree-agent-ae8b11d891120c498` (worktree), based on `main` @ `4eeed89`.
Scope: `docs/runtime_port/PLAN.md` §1.2 items 7–16 plus the fourteen unregistered
`.text` addresses in §1.1.

Every verdict below is PORTED, CLOSED (no port needed, with the evidence) or OPEN.

**The measurement run** all runtime evidence comes from (one run, all questions batched):

```
BANISHED_AUTOSTART=1 BANISHED_UNREG_CENSUS=1 BANISHED_AMBIENT_TRACE=1 \
BANISHED_LIVE_MUSIC=1 BANISHED_PAUSEMENU=1800 BANISHED_PAUSEMENU_QUIT=120 \
BANISHED_PAUSEMENU_BUTTON=6 ./reimpl-x64.exe
```

Autostart new game, ~30 s of simulation, then the retail Quit path so the shutdown
census dumps. Exe deployed 11:05, `standalone_crash.log` empty, clean self-quit.
Log excerpts quoted below are from that run's `standalone_error.log`.

An earlier attempt at the same run (11:01) was lost: the file `.agent_lock` protocol
has no atomic claim and lane C overwrote the lock and truncated the logs mid-run. The
run above was taken under the replacement `mkdir .agent_lock.d` protocol.

---

## #7 — `ClearStorage` slot-3 Serialize is a no-op → **PORTED**

`code/Game/Work/clearstorage.cpp:704` held `Work_SerializeNop` in vtable slot 3.

Retail is `0x1400e7350`, read off the vtable bytes at `0x1401611a0` slot [3]
(`python tools/dump_vtable.py 0x1401611a0`). Its disassembly is 22 instructions with no
branches:

```
1400e736a  CALL 0x1400f7400          ; Work::Serialize(this, stream)
1400e737a  CALL [0x140152ad8]        ; IO::Serializer ser(stream, 0)
1400e738a  CALL 0x1400296b0          ; BST serialize, LEA RCX,[RDI+0x58]
1400e7395  CALL [0x140152ad0]        ; ~IO::Serializer
```

So it consumes bytes. A no-op there reads **zero** bytes on load while the writer wrote
the task BST, which desyncs the entity stream on the record *after* ClearStorage — the
silent shape `docs/waves/.../reference_null_guard_on_a_serializer_call_is_a_desync.md`
describes, invisible to our own round trip because both legs skip the same bytes.

Ported as `ClearStorage_Serialize_Impl` and wired into slot 3. It is the FireWork
serializer (`0x1400e9a50`, `firework.cpp:661`) minus the trailing capacity int; the BST
offset is `+0x58`, confirmed independently by `ClearStorage_ScalarDtor_Impl`
(`clearstorage.cpp:554`) which frees the same `+0x58` tree. Both sub-calls
(`0x1400f7400`, `0x1400296b0`) were already ported and registered, so no new closure.

Not exercised in the measurement run: a ClearStorage work object only exists once the
player issues a clear order on a storage building, which the autostart town never does.
The correctness argument is the byte-for-byte match with the retail body, not a run.

**PORTED.**

## #8 — Trade controller event `0xa` registered null → **PORTED**

`code/Game/Component/tradecomponent_states.cpp:620`. The address in the file's own
comment (`b240`) is `0x14005b240`, not `0x14000b240` — the short form cost one wrong
decompile.

Retail `0x14005b240` inlines the whole of `StateMachine::Tick`, which is why ~90 lines
of decompile implement a two-line handler. Read against our own port of that function
(`StateMachine_TickReimpl`, `statemachinecomponent.cpp:455`) the inlined loop matches
exactly, with `time = System::Time(0.0)`, `param3 = 0` (it dispatches node slot `+0x30`,
not `+0x38`) and `param4 = 1` (the `bVar2 = true` cleared after one pass). The prologue —
`if (sm[+9] < 0) StateMachine_Tick(sm, Time(0), 0, 0)`, then the slot-8 request with the
node re-read — is verbatim `ControllerLive::RequestStateTransition(sm, 2)`
(`controller_state_table.h:317`). So retail is:

```
RequestStateTransition(sm, 2);
StateMachine_TickReimpl(sm, 0, 0, 1);   // run the transition NOW, not next frame
return 1;
```

which is what `TradeCtrl_Event0A_Dismissed` now does. The immediate tick is the whole
point of the handler: dismissing a merchant has to take effect on the click.

Nothing in our tree posts event `0xa` today (the dismiss button lives in a trade-dialog
path that is not wired), so this is currently unreachable — but a null in an event table
is a landmine the moment it *is* wired, and the port costs nothing.

**PORTED.**

## #9 — `AutoPickupWork` slots 1/2 generic shims → **CLOSED**

Retail, disassembled in full:

```
1400294d0  MOV RAX, qword ptr [0x1401c6830] ; RET     <- slot 1
1400294c0  MOV EAX, dword ptr [0x1401c6828] ; RET     <- slot 2 (GetTypeId)
```

Neither is a dtor thunk — the file's comment calling slot 1 a "dtor thunk" is a misnomer.
They are the two halves of the standard `REGISTER_TYPE` storage pair: a dword type id and
a qword type-info/name pointer, both zero in the image
(`inspect_memory_content 0x1401c6820` = 32 zero bytes) and filled at type registration.

- Slot 2: our `AutoPickupWork_GetTypeId` returns `Object::GetTypeIdByName("AutoPickupWork")`,
  which is the value the registration writes into `0x1401c6828`. Same number.
- Slot 1: `get_xrefs_to 0x1401c6830` returns exactly one reference — the slot-1 function
  itself. Nothing in the exe reads that cell through any other path, and nothing in our
  tree calls Work-family vtable slot 1. `Work_Slot1` returning null is indistinguishable.

**CLOSED** — the shims are behaviourally equal to retail; no port needed.

## #10 — Work base dtor / dtor-thunk / GetTypeId shims → **PORTED (one real deviation)**

Same analysis as #9 for the Work base pair (`0x14002ca50` reads `0x1401c6d00`,
`0x14002ca40` reads `0x1401c6cf8`; the only xref to `0x1401c6d00` is its own reader).
`Work_GetTypeId` / `Work_Slot1` are equivalent.

The scalar dtor was **not** equivalent. Retail `0x14002ca60`:

```
*(void**)this = &vtable_Work;
Object::Internal::~Internal(this);
if (flags & 1) MemFree(this);
return this;
```

`Work_GenericScalarDtor` (`work_base_standalone.cpp:50`) omitted the vtable re-stamp, so
every Work leaf without its own ported `*_ScalarDtor_Impl` entered `~Internal` still
carrying its LEAF vtable where retail has already walked the chain down to Work's. The
ported leaf dtors (ClearStorage, MarketWork, Pickup, …) all do the stamp; this shim was
the odd one out. Restored via `Work_StampBaseVtable`.

**PORTED.**

## #11 — Background music: ported, opt-in, never proven → **PORTED, default-on**

`MusicPlayer::Update` (`0x1400a40e0`) is registered (`reg_code_lane5.cpp:48`) and was
gated `BANISHED_LIVE_MUSIC=1` because the authoring lane could not run the game.

A one-shot `[MusicPlayer] PLAY` log was added immediately after the
`Audio::Scene::PlayEffect` call — the only line that can prove the mixer was handed a
track. The measurement run produced, in `standalone_error.log`:

```
2565: [MusicPlayer] MusicTracks.rsc -> 00000271db93d780 tracks=15
2566: [MusicPlayer] PLAY track=12/15 ext=271db912ee8 scene=00000271db7aa138 -> handle=1536
```

The lazy `EnsureTrackList` Find resolved, all 15 tracks parsed out of `MusicTracks.rsc`,
the month/mood filter selected one, and `PlayEffect` returned a live non-zero handle
against a real `Audio::Scene`. No crash, no `[MusicPlayer] SKIP`.

Per CLAUDE.md a proven feature graduates: `MusicEnabled()` is now default-on with
`BANISHED_LIVE_MUSIC=0` as the kill-switch.

**PORTED / default-on.**

## #12 — `RemovalTool::CalcPath` drafted, not landed → **CLOSED (already landed)**

`final_steps.md` is ground-truthed 2026-06-19 and PLAN §1.3 already flags it as history.
The draft in `notes/wave_drafts/` was landed long ago:

- body: `code/Game/Player/keystone_a_removalcalcpath.cpp` (`RemovalCalc_Slot13`)
- registration: `standalone/reg_keystone_a_removal.cpp:41`
  `RegisterExeAddr(0x1401035e0, &RemovalCalc_Slot13)`

`0x1401035e0` does not appear in the measurement run's unregistered census. Nothing to do.

**CLOSED.**

## #13 — MarketWork AVL cluster + `LongTermPlan` → **CLOSED (already landed)**

Grepping the addresses against today's registry:

| addr | registered at |
|---|---|
| `0x14002a9f0` | `reg_marketwork_bst.cpp:53` |
| `0x14002aaa0` | `reg_marketwork_bst.cpp:62` |
| `0x14002ac40` | `reg_marketwork_bst.cpp:63` |
| `0x14002b290` | `reg_marketwork_bst.cpp:68` |
| `0x140115e60` (LongTermPlan) | `reg_citizenai_growup.cpp:35` |
| `0x1400eff80` | not by address — it is vtable slot 4, `marketwork.cpp:2145` |

`0x1400eff80` is `MarketWork::Execute`, not `MarketWork_Serialize`; the PLAN entry
inherits that misnomer from `final_steps.md`. It is wired into the MarketWork vtable
(`0x140161710` slot [4]) and all five documented stages of the 1,408-line retail body are
present (`marketwork.cpp:997, 1161, 1409, 1592, 1724`). None of these addresses appear in
the census.

**CLOSED.**

## #14 — `EditTool::SelectEntity` null path returns 0 → **CLOSED**

`edittool.cpp:1044` and `:1138` guard on `fn_EditTool_SelectEntity` being null. That
pointer is `ResolveAddr(Exe, 0x1400fd3b0)` (`engine_funcs.cpp:2907`), and `0x1400fd3b0`
**is** registered — `standalone/reg_edittool_select.cpp:32` →
`EditTool_SelectEntity_Impl`, called from `address_registry.cpp:858`. `0x1400fd3b0` does
not appear in the census.

Both guard sites are inside `EditTool_HarnessSelectEntity` / `EditTool_HarnessSelectNearby`
— harness entry points, not gameplay. The real click path is `edittool.cpp:918`, which
uses the same (non-null) pointer. The "M2 unported?" text in those two printfs is a stub
comment that outlived its blocker.

**CLOSED** — no gap; the guards are dead defensive code in harness functions.

## #15 — `SubArrayDeepCopy` clears count but not capacity → **PORTED**

Retail `0x14001fdb0` (stride 0x14) and `0x14001fe60` (stride 0x02) both open with

```c
MemFree(*param_1);
param_1[1] = 0;          /* ONE qword store over 0x08..0x0f */
*param_1  = 0;
```

`param_1[1]` is a `longlong` covering **both** the count at `+0x08` and the capacity at
`+0x0c`. `serialize_struct60b.cpp:76` cleared only the count, leaving a stale capacity
behind whenever the source array was empty. Fixed.

While there: both addresses were on the unregistered-`.text` list, and both are resolved
by `engine_funcs.cpp:4947,4949` into `fn_Struct60B_SubArrayCopyStride14/_02` — pointers
that have been null since the day they were written, so only the inline fallback ever ran.
Registered in `standalone/reg_closeout_lane_d.cpp` to the same body, so the executed code
is unchanged and the pointers stop lying.

**PORTED.**

## #16 — `AmbientEmitterComponent` find needs a counting run → **CLOSED (measured: leave unregistered)**

`docs/waves/lanes/integrator_queue.md` set the criterion: `0x140002280`
(`Entity::FindComponent<AmbientEmitterComponent>`) may be registered **only if every
description declares at least 2 emitters**, because `CitizenComponent_helper_a710`
(`0x14012a710`) asks for emitter index 1 and reads `entry[1].handle` at stride 0x10 with
no bound check. Registering it in an earlier attempt crashed on the first founding-housing
assignment (ACCESS_VIOLATION, READ of 0x10).

The 16-line trace cap could not answer that, so the trace was given a histogram. The
measurement run:

```
[ambientemit] CENSUS calls=500  count0=0 count1=435 count2plus=65  max=2
[ambientemit] CENSUS calls=1000 count0=0 count1=435 count2plus=565 max=2
```

**435 of 1000 refreshes see a description with exactly ONE emitter**, and the maximum any
description declares is 2. The criterion fails. Registering `0x140002280` would hand
`a710` a component whose emitter array has one entry and it would read `entry[1]` off the
end — the same unguarded deref, now with a measured cause rather than an inferred one.

The finder itself (`entity_finders_disaster.cpp`) is ported, correct and one line from
being registered the day `a710`'s consumers are made index-safe or the descriptions grow
a second emitter.

**CLOSED — deliberate leave, with the count.**

---

## The fourteen unregistered `.text` addresses

Cross-referenced against the measurement run's census
(`[UnregCensus] ==== 56 distinct unregistered addresses ====`).

| addr | who asks | verdict |
|---|---|---|
| `0x140002280` | `firecomponent_ignite.cpp:98`, `tornadocomponent.cpp:239`, `citizen_assignworkplace.cpp:123`, `engine_funcs.cpp:3862` | **CLOSED — deliberate leave.** Item #16 above; census `total=2 boot=2`. |
| `0x140012a60` | nobody | **CLOSED — census artifact.** Only reference is the word "was" in `engine_funcs.cpp:1341`; no live `ResolveAddr`, absent from the run census. |
| `0x140017050` | nobody | **CLOSED — census artifact.** Reimplemented inline; `engine_funcs.h:5106` records the removal. Absent from the run census. |
| `0x14001fdb0` | `engine_funcs.cpp:4947` | **PORTED** (item #15). |
| `0x14001fe60` | `engine_funcs.cpp:4949` | **PORTED** (item #15). |
| `0x1400786b0` | nobody | **CLOSED — census artifact.** `ScenarioDialog_Ctor` is wired directly (`reg_nullsweep.cpp`); the address survives only in comments. Absent from the run census. |
| `0x1400938d0` | `gameupdateloop.cpp:583` | **CLOSED — deliberate leave (active harm).** `Weather::DrawUpdate`; registering double-writes fog/sun/cbuffer (`integrator_queue.md`, lane 4). Census `total=1 boot=1`. |
| `0x140097fb0` | nobody | **CLOSED — census artifact.** `fn_SetEngineNextLoop` was replaced; `engine_funcs.h:3381`. Absent from the run census. |
| `0x14009e8c0` | `gamegrid_serialize.cpp:112` | **CLOSED — deliberate leave (active harm).** `Game::BuildPathfindingSystems`; `gameupdateloop_load.cpp:1562-3` already builds the `+0x68/+0x70/+0x78` subset, so registering builds all three twice and leaks the seeded objects. Census `total=1 boot=1`. |
| `0x1400f2f00` | `pickup.cpp:110` | **CLOSED — must stay unregistered.** It is `Pickup::Execute`, and `pickup.cpp:828` uses it as a *dispatch-or-fallback*: `if (s_fn_Slot4_Original) return s_fn_Slot4_Original(...)`. In standalone there is no "original" — registering it binds the pointer to `Pickup_Slot4_Impl` itself and the dispatcher calls itself forever. The port is already what runs. (The `ok` flag at `pickup.cpp:164` is false because of this one pointer; no caller reads the return, and the re-resolve was already made one-shot for perf.) |
| `0x1400fcc90` | `edittool.cpp:248` | **CLOSED — deliberate leave.** EditTool deselect; it doubles as a per-frame deactivate slot, and our `0x140100380` screen-rect helper fails for citizens, so a live `cc90` wipes the selection outline every frame. Measured previously as c9e0=1 vs cc90=8973 (`reg_player_render.cpp`). The tool-switch caller is served by wiring our own vtable slot 5 instead. Census `total=1 boot=1`. |
| `0x140101ee0` | `gameupdateloop.cpp:543` | **CLOSED — deliberate leave (active harm).** `EntityUI::Startup`; would double-insert the eight tool-mode AVL nodes (`integrator_queue.md`, lane 6). Census `total=1 boot=1`. |
| `0x1401026a0` | `gameupdateloop.cpp:490` | **CLOSED — deliberate leave (active harm).** `EntityUI::Update`; would double-drive camera and tool every frame. Census `total=1 boot=1`. |
| `0x140128f50` | `buildcomponent.cpp:344`, `buildcomponent_state8.cpp:127`, `destroycomponent_state6.cpp:128` | **PORTED — a real gap, and it was hiding a wrong argument.** See below. |

### `0x140128f50` — the one real gap in the fourteen

The census shows exactly three boot-time requests, one per consumer, all of which
null-guard and skip; `destroycomponent_state6.cpp:477` even logs
`[Collapse] pick-mesh rebuild SKIPPED — 0x140128f50 NOT registered`. The effect is that
the pickable volume never followed the model's construction or collapse frames: it stayed
pinned to whichever sub-mesh was current at `OnAddedPass` time.

The port has existed all along — `PickingComponent_RegisterMesh`
(`pickingcomponent.cpp:167`) — and is a faithful refresh: it opens with the same
`OnCreate` teardown retail's `(**(code**)(*param_1 + 0x38))()` performs, so the
`pickingId < 0` guard that follows is always satisfied rather than being a
"skip if already registered" test.

Registering it was **not** safe as the tree stood. Retail's parameter (decompile of
`0x140128f50`) is the PickingComponent object: it reads the vtable at `*param_1`, the
owner IBP at `param_1+2`, the pick description at `param_1[3]`, the id at `param_1[5]`
and the userData at `param_1[6]`. `buildcomponent.cpp:529` passes `*sslot` (the object,
correct) and `destroycomponent_state6.cpp:472` passes `picking` (correct), but
`buildcomponent_state8.cpp` passed `entSlot`, the **address** of the InternalBasePtr, at
four call sites — one indirection too many, which would have made the object pointer be
read as a vtable on the very first refresh. The three consumers read the same array
(`bc+0x68`) and disagreed about it only because the calls were dead code.

Fixed (deref at all four sites) and registered.

**Verification run** (same harness, 11:15, no `BANISHED_LIVE_*` set at all):
`standalone_crash.log` empty, clean self-quit, zero ACCESS_VIOLATION / REDZONE /
EXEC-of-0 lines. `0x140128f50` is gone from the census, which drops from 56 to 55
distinct unregistered addresses and the `.text` band from 9 rows to 8. Music played with
no env var set, confirming the default-on graduation:
`[MusicPlayer] PLAY track=0/15 ext=21a01652408 scene=0000021a00123c80 -> handle=2560`.

---

## Files touched outside `standalone/reg_closeout_lane_d.cpp`

| file | why |
|---|---|
| `code/Game/Work/clearstorage.cpp` | #7: `ClearStorage_Serialize_Impl` + vtable slot 3 |
| `code/Game/Work/work_base_standalone.cpp` | #10: Work base vtable stamp in the generic scalar dtor |
| `code/Game/Component/tradecomponent_states.cpp` | #8: `TradeCtrl_Event0A_Dismissed` + table entry |
| `code/Game/Game/serialize_struct60b.cpp` | #15: capacity clear + the two exported leaves |
| `code/Game/Game/unreg_lane5_musicplayer.cpp` | #11: proof-of-play log, default-on |
| `code/Game/Component/ambientemittercomponent.cpp` | #16: emitter-count histogram in the existing trace |
| `code/Game/Component/buildcomponent_state8.cpp` | `0x140128f50`: deref the IBP at four call sites |

`tools/dispatch_slot_map.py --check`: **0 name mismatches** (14 pre-existing shadowed
`IO::Serializer` addresses in `address_registry.cpp`, unchanged by this lane).
`standalone/address_registry.cpp` was **not** edited — the new file self-registers from a
static initializer, the same pattern `REGISTER_TYPE` uses, into the never-cleared
function-local registry in `resolve_standalone.cpp`.
