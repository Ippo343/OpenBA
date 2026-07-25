# CitizenComponent::OnRemove — Next-Session Brief

Written 2026-07-25 at the end of the upgrade-path session. Branch:
`wave/upgrade-path-complete` (6 commits, nothing pushed, tree clean).

---

## Where things stand

**The building upgrade path is DONE through the real UI.** Wood house → stone
house works end-to-end by clicking, for both a genuinely-constructed house and a
force-completed start-town house: demolish → haul-away → refund → replacement
foundation spawns → replacement is constructed → finished panel is correct.
`BANISHED_LIVE_DESTROY` is now default-on, so no env var is needed for it.

Six commits landed this session, each self-contained:

| Commit | Fixes |
|---|---|
| `0f97d16` | HighlightComponent scene-id sentinel — in-world freeze on first citizen death |
| `69b41cd` | WorkPlaceUI stale weak-ref guard — crash on building click (**containment only**) |
| `30c602e` | Stamp force-completed start-town buildings as built — starter-house upgrade spawned no foundation |
| `f43fbef` | Live-site table — restores `State8Finalize` for every constructed building |
| `330710b` | Register `WorkPlaceComponent::SetAccepting` — pause button called address 0 |
| `1e0da88` | Graduate `BANISHED_LIVE_DESTROY` to default-on |

---

## THE TASK: port `CitizenComponent::OnRemove` (0x140113ca0)

### Root cause (VERIFIED via Ghidra decompile + tree grep)

The town's citizen list has an **add with no remove**:

| | Address | Status |
|---|---|---|
| `CitizenComponent_OnAdd` — appends to `townMgr+0xa0` | `0x140114000` | ported **and** registered (`standalone/reg_citizen_employ_cascade.cpp:25`) |
| `CitizenComponent_OnRemove` — removes from `townMgr+0xa0` | `0x140113ca0` | **absent from the tree entirely** |

Citizens are added to `townMgr+0xa0` and never removed, so **every death leaves a
permanent dangling pointer** in the town's own list. `Object::InternalBasePtr::
CheckPtr` is an EMPTY function in the shipping Runtime
(`code/Game/Object/internal_factory_audit.h:34`) — retail's correctness comes from
clearing refs explicitly at removal, and `OnRemove` is that clearing.

Note `ObjAudit` reports `bad=0` while this is happening, and that is consistent:
the InternalFactory **handle table** is intact. It is the *town's lists* that hold
stale pointers. Do not take `bad=0` as evidence this is fixed.

### Scope: SIX ref classes, not one

Retail `OnRemove` clears all of these before the entity dies (all VERIFIED from the
decompile of 0x140113ca0, in this order):

1. `FUN_1401176e0`, `FUN_140115a50`, `Entity_impl_59b0`, `Entity_impl_1650` — prologue teardown
2. `CitizenComponent_AssignWorkplace(self, null, -1)` — workplace link `+0x238`
3. `CitizenComponent_sub_2040(self+600, ...)` — the `+0x258` back-ref, when non-null
4. **family / children** — `+0x250` (spouse) / `+0x260` (child list). Two branches:
   - no spouse (`+0x250 == 0`): per child, if `child+0x3c <= desc(+0x288)+0x44`
     (under age) → `HandleLifeEvent(child, 4)`; else re-home the child —
     `sub_1fa0`, `AssignWorkplace(null,-1)`, `FUN_140002cc0(child+0x208+0x28)`,
     `FUN_14012c3f0(meshInst, 2, 0)`, null the `+0x208` slot
   - spouse present: loop `CitizenComponent_sub_1fa0` until `+0x260` drains, then
     clear the spouse's `+0x250` back-ref and own `+0x250`
5. `CitizenComponent_AssignHousing(self, null)` — housing `+0x248`, when `+0x248 != 0`
6. **profession list** — when `self+0x1f0 != *(gw+0xb0)+0x30` (i.e. not already the
   default profession): `CitizenComponent_impl_c3d0(townMgr, ..., self+0x1f0)` then
   `CitizenComponent_PostLifeEvent(...)` with event `0xf` (replacement found, arg
   `self+0x28`) or `0x10` (none found)
7. `TradeWork_impl_3480(gw+0x120, self+0x10)` — trade-work list
8. **`FUN_140002cc0(townMgr+0xa0, ref)`** — the citizen-list removal we crashed on

### Building blocks that already exist

- `FUN_140002cc0` — the array REMOVE helper (your own note: "FUN_140002cc0 = REMOVE
  not add"). Resolved in `attendancecomponent.cpp:107`, `cemeterycomponent.cpp:183`.
- `CitizenComponent_AssignHousing` `0x140111ee0` — registered (`reg_citizen_employ_cascade.cpp`)
- `CitizenComponent_AssignWorkplace` `0x1401163d0` — in use (`builder_pool_driver.cpp:105`)
- `FUN_14012c3f0` — registered (`reg_component_people.cpp:59`)
- `CitizenComponent_HandleLifeEvent` — ported (`citizen_lifeevent_handler.cpp`)

### Fix shape

Port it properly and register at `0x140113ca0`. **A partial port registered at that
address is a trap** — retail callers (4 xrefs) would silently skip whichever
cleanups were omitted, converting a loud crash into quiet corruption. Either do all
eight steps or gate the whole thing behind an opt-in env flag until verified
(suggest `BANISHED_LIVE_CITIZEN_ONREMOVE`), per the usual graduation policy in
`code/Game/Component/component_factory_base.cpp`.

Step 4 fires life events, so it needs care — `HandleLifeEvent` is the same chain
that produced this morning's freeze.

Once this lands, the `WorkPlaceUI` guard from `69b41cd` can be retired, and the
five still-unguarded readers below are protected at the source.

### FIX THIS FIRST (one line, trivial)

`engine/types/CitizenComponent.h:34` mislabels the address:

```cpp
constexpr uintptr_t OnCreate = 0x140113ca0; // vtable[7]   <-- WRONG
```

`0x140113ca0` is `OnRemove`; `OnCreate` is `0x140114900`. Check for users of the
constant before renaming.

### Still-exposed readers (unguarded, same no-op CheckPtr)

Per-tick (dangerous):
- `code/Game/Component/citizencomponent.cpp:1438`, `:1843`, `:2192`
- `code/Game/Component/citizen_employ_cascade.cpp` — strategies `cbf0`/`ca20`/`c5f0`/`c4b0`
- `code/Game/Component/citizen_assign_citizens.cpp:317`

On UI interaction only:
- `code/Game/Dialog/workplaceui.cpp:832` — guarded by `69b41cd`

**Not yet checked:** whether the other town registries have the same add-without-
remove hole — `townMgr+0x20` (residences), `+0x100` (profession BST), `+0xd0`
(pending-build sites). Worth auditing the same way: find the OnAdd, find whether
its OnRemove counterpart is ported AND registered.

---

## Second task: workplace pause — `accepting=false` eviction path

`0x14012fc00` = `WorkPlaceComponent::SetAccepting`, ported at
`code/Game/Component/workplacecomponent.cpp:372` but **`accepting=true` path only**.
Registered as of `330710b`, so the pause button no longer crashes — but pausing
currently just sets `+0x50 = 0` without evicting anyone.

### The eviction loop (VERIFIED, decompiled 0x14012fc00)

```c
while (wpc+0x60 > 0) {                       // roster count
    citizen = ((wpc+0x58)[0]);               // always the FIRST entry
    if ((citizen+0x1f0)->+0x4c == 0) CitizenComponent_sub_ffc0(citizen, &out);  // 0x14010ffc0
    else                             CitizenComponent_sub_0470(citizen, &out);  // 0x140110470
    CitizenComponent_AssignHousing((wpc+0x58)[0], &out);                        // 0x140111ee0
}
```

**All four helpers are already ported AND registered** (checked explicitly, because
"ported but unregistered" was the entire cause of `330710b`):
`0x14010ffc0` + `0x140110470` → `reg_component_people.cpp`; `0x140111ee0` →
`reg_citizen_employ_cascade.cpp`; `0x14012c3f0` → `reg_component_people.cpp:59`.

### Two hazards

1. **Infinite-hang risk.** The loop re-reads `+0x60` each pass and always takes
   entry `[0]`, so it terminates only if `AssignHousing` removes the worker from the
   roster. `g_wpcRosterOnly` (declared `workplacecomponent.cpp:218`) makes the
   attach/detach helpers do the bare roster-list mutation *only* — which is exactly
   what keeps the count decrementing. So the guard is both required and sufficient.
   Hold it across the loop.
2. **Freeze risk.** The pause path calls `Graphics::Scene::RemoveObject`/`AddObject`
   directly on the MeshInstance — the same call that froze the game in
   `HighlightComponent` (`0f97d16`), and the reason `g_wpcRosterOnly` exists.

### Pause indicator

`FUN_14012c3f0(meshInst, idx, 0)` with `idx` 2, 3, 4 on the pause path (the
`accepting=true` path uses 1 and 0). **Unresolved:** the registered port of
`0x14012c3f0` is described as a citizen health/status-icon flag setter
(`reg_component_people.cpp:24`) but `fc00` calls the same address on a building
**MeshInstance**, and different files in the tree describe it both ways. Confirm the
port is generic before reusing it.

### Also stale

`code/Game/Component/citizencomponent.cpp:239` (and `:91-93`) assert
*"0x14012c3f0 is unmapped in standalone"* and deliberately route around it. It **is**
registered now (`reg_component_people.cpp:59`). Correct the comment.

### Partly inferred, needs a second look

The `meshInst+0xf0` suppression-mask semantics in the pause branch: the `== 0`
branch looks unreachable after an `OR`, which probably means the decompiler's
pointer-arithmetic width was misread. Re-check before implementing.

---

## Open threads, unowned

- **The 77% stall.** Very first run of the session: an upgraded starter house
  reportedly stalled at 77%. No diagnosis accounts for it — by the final
  understanding, both upgrade clicks in that run were pure no-ops (`LIVE_DESTROY`
  was off), so a partially-progressed 77% should not have been possible. Either the
  number came from a different building, or something else is going on. Watch for it.
- **Player-placed buildings and OUTPUT storage.** `f43fbef` fixed
  `State8Finalize` being skipped for every constructed building, which also skipped
  the OUTPUT storage reconfigure. Player-placed (non-upgrade) buildings were
  therefore also affected and should now be silently fixed — unverified.
- **Builder throughput.** Still ~1 unit per trip; construction far slower than
  retail. Pre-existing, see `project_builder_work_throughput` memory.

---

## Repro recipe

Interactive (what was used all session):

```
BANISHED_AUTOSTART=1 BANISHED_BUILD_DEMO_HOUSE=1 BANISHED_FORCE_SPEED=5 \
BANISHED_BUILD_TRACE=1 BANISHED_DESTROY_TRACE=1 [BANISHED_OBJAUDIT=1]
```

- `BANISHED_LIVE_DESTROY` is no longer needed (default-on).
- `BANISHED_FORCE_SPEED=5` **pins speed and disables the pause/speed buttons**
  (`gameupdateloop.cpp:769-773` rewrites `+0x30`/`+0x34` every frame). Drop it to
  use the toolbar; the speed array is `{1,2,5,10}` so 5 is the third button.
- Builders must be assigned by hand (town hall → Professions). This is intended —
  `BuilderPool_Tick`'s auto-seating is gated behind `BANISHED_BUILDER_AUTOPOOL`
  (off) since `55cfa3c` made builder count player-driven. Set it to `1` to
  auto-staff sites and skip the manual step.
- Automated upgrade repro: add `BANISHED_TOOL_HARNESS=upgrade` (~8 min; the demo
  house must build first).

Logs land in `C:/GOG Games/Banished/`. **Read `standalone_crash.log` first** — the
`[REDZONE-DUMP]` lines mislead; `EXEC of 0` means a null function pointer, which in
this codebase usually means an exe address is missing from the registry. Symbolize
with `addr2line -f -C -e standalone/build/reimpl-x64.exe 0x140<offset>` using the
`reimpl-x64.exe + 0xNNNN` offsets from the trace.

For a **hang** rather than a crash, attach to the live process — this is how the
freeze was caught:

```
gdb -p <pid> -batch -ex "set pagination off" -ex "thread apply all bt 18"
```

Rising CPU while hung means spinning, not a lock.
