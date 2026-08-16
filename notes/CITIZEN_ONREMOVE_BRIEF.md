# Post-OnRemove open threads — status after the 2026-07-25 clean-up pass

Supersedes the previous revision of this file (commit `cb43160`), which listed six open
threads left behind by `CitizenComponent::OnRemove` (`eb6d8aa`). Five are closed below; the
sixth (family links / birth-rate soak) is partly done and the remaining part is stated
explicitly. Branch `wave/upgrade-path-complete`.

---

## Closed this pass

### OPEN 1 — `FUN_1400f8400` is ported (was: unported, blocked DESTROY_WORKERS)

`code/Game/Component/citizenai_work_scheduler.cpp` → `CitizenAI_WorkScheduler_Rekey`,
registered at `0x1400f8400` from that file's own batch.

The claim that it was "blocked behind an unported AVL-node-move cluster" was wrong. All four
callees were already ported *and* registered:

| callee | address | where |
|---|---|---|
| `CitizenAI_helper_cb50` | `0x14002cb50` | `WS_cb50`, same file |
| `TradeDescription_impl_ca60` | `0x14001ca60` | `reg_work_cellbst.cpp` |
| `TradeDescription_impl_9a40` | `0x140029a40` | `reg_work_cellbst.cpp` |
| `FUN_14000e320` | `0x14000e320` | `reg_work_base.cpp` |

What it does: the scheduler files every posted work item twice — once in the flat id-map at
`sched+0x30` (0x30 nodes keyed by the int work id, payload at `+0x08`) and once inside a
per-PROFESSION bucket at `sched+0x120` (0x38 nodes keyed by the u64 profession key, each with
its own id-keyed inner tree at `+0x08` and a count at `+0x10`). `sched` is `*(gameWorld+0x118)`
— the WORK manager, not the town manager. Citizens only ever see items in their own bucket, so
a building that changes profession mid-task has to move its in-flight item across. `ca60`
(erase) and `9a40` (insert) maintain the bucket counts themselves, which is why retail never
touches `+0x10` here.

Both call sites are wired: `WorkPlaceComponent_EnterBuilderProfession` (f520) and
`WorkPlaceComponent_LeaveBuilderProfession` (f670). Both `NOT-IMPLEMENTED` logs are gone.

**It is reached, but it has never had anything to move — see NEW-1 below.** Do not record
this as "verified end-to-end". What is verified: f520 runs, releases the roster to 0, and
calls `Rekey` with a live work id; `Rekey` then takes retail's own first early exit.

`BANISHED_LIVE_DESTROY_WORKERS` is **graduated to default-on** on that evidence plus an A/B:
the same scenario with the flag off reaches the upgrade spawn and then dies the same way, so
the flag introduces no new failure. (The failure it does not fix is NEW-2.)

Also in that port, the dropped `IsDerivedFrom(desc, DAT_1401c8118)` check is restored — but
read the retail decompile before "fixing" it further: if that check FAILS, retail dereferences
`null+0x20` and crashes. It is an assertion, not a branch, so there is no retail behaviour to
be faithful to on the failing side. We run it for diagnostics, log once if it rejects a
description we do have, and trust the description anyway.

### OPEN 2 — `0x14012fb00` registered

`WorkPlaceComponent_SetMaxWorkers` (was the file-local `WPC_SetWorkerCapacity_fb00`) is now
registered from `reg_destroy_workers.cpp`.

The old reasoning for keeping it unregistered — "registering would put the ungated WorkPlaceUI
spinner onto the eviction loop" — was backwards. The eviction IS what retail's worker spinner
does. Correction to the previous brief: this was **not** a live crash. `fn_WorkplaceComp_
SetMaxWorkers` is only reached from `WorkplaceUI_AdjustWorkerCount`, which is only reached from
the exe port `WorkplaceUI_OnRemove` (`0x1400877e0`) — and that function is in no vtable and no
registry, because the SA `WorkPlaceUI` promotion routes its spinner through `W_OnRemove`'s seat
cascade instead. A landmine defused, not a crash fixed.

### OPEN 3 — duplicate `0x14012e5c0` resolved

`0x14012e5c0` is `WorkComponent::CalcMaxWorkers`: it reads the `WorkDescription` at
`workComp+0x18` and returns either the flat capacity (`desc+0x24`) or a footprint-scaled one
(`desc+0x30` divisor, `desc+0x34` flag), clamped to `[1, base]`. It is not terrain speed —
`HuntComponent::Update` merely reuses the same `workComp+0x30` cache as its divisor, which is
where that name came from.

The two registrations had **identical bodies**, so the last-write-wins ordering never actually
mattered. `HuntComponent_ComputeTerrainSpeed` is deleted along with its registration;
`TutBase_ComputeWorkCapacity` → `WorkComponent_CalcMaxWorkers` and `TutBase_GetWorkCapacity`
→ `WorkPlaceComponent_GetMaxWorkers`, both still registered from `reg_tutorial_base.cpp`.

### OPEN 4 — finders renamed

Ground truth is `standalone/factory_registrations.csv`:

| type-id global | real name | finder (old → new) |
|---|---|---|
| `DAT_1401c8070` | **WorkComponent** | `fn_FindComponent_Workplace` → `fn_FindComponent_Work` |
| `DAT_1401c80e0` | **WorkPlaceComponent** | `fn_FindComponent_BuildState2` → `fn_FindComponent_WorkPlace` |
| `DAT_1401c80a8` | **WorkDescription** | `g_TypeId_WorkplaceJobType` → `g_TypeId_WorkDescription` |
| `DAT_1401c7c80` | **RawMaterialComponent** | `fn_FindComponent_WorkplaceC` → `fn_FindComponent_RawMaterialC` |
| `DAT_1401c7b30` | **NaturalResourceComponent** | `fn_FindComponent_BuildStateC` → `fn_FindComponent_NaturalResourceC` |
| `DAT_1401c7cb8` | **RawMaterialDescription** | `fn_FindComponent_RawMaterialC` (0x140006330) → `fn_FindDescription_RawMaterial` |

`g_TypeId_StorageLocationComponent` → `g_TypeId_WorkComponent`.

**Every call site was already passing and using the right object** — verified against the
retail decompiles of `WorkPlaceUI::OnCreate` (`0x140087080`) and `WorkPlaceUI::Serialize`
(`0x140087940`), which use both finders side by side: `FUN_140002e10` (WorkPlaceComponent) for
`+0x48` profession / `+0x60` roster, `FUN_140003050` (WorkComponent) for `+0x18` WorkDescription
and `+0x30` capacity. Only the names were wrong. Misleading locals were renamed with them
(`buildComp` → `wpComp`, `wpComp2` → `parentWorkComp`, `workerCount` → `workId`).

> **Renaming hazard worth remembering.** A blind `sed` of `fn_FindComponent_WorkplaceC` →
> `fn_FindComponent_RawMaterialC` collided with an existing `fn_FindComponent_RawMaterialC`
> bound to a *different* address (`0x140006330`), silently repointing `buildroad.cpp:548` from
> the description finder to the component finder. It compiled cleanly. When renaming into a
> namespace you do not control, grep for the *target* name first, not just the source.

---

## Still open

### OPEN 5 — family links / gender: half done

**Corrected premise.** The previous brief said `FUN_140117080` "is guarded off in standalone".
It is not: it is fully ported (`CitizenComp_DeferredInit_117080`,
`citizencomponent_lifecycle.cpp`) and registered (`reg_citizen_25.cpp:55`), and the BIRTH path
calls it live (`citizencomponent.cpp` → `s_fn_FUN_140117080`). Every citizen born in game gets
retail's real name/age/gender/lifespan.

What is hand-rolled is the **founding** spawn only: `starttown.cpp`'s `SpawnOneCitizen` stands
in for retail's `sub_1040`, and reproduces 117080's effects piecemeal (mesh variant, spawn
transform, name, colour, age, laborer seed, housing). "Restore 117080" for that path is not a
small change — it would double-apply most of those.

Done this pass:

* **Gender write ungated.** It used to sit inside `FamilyLinksOn()`, which made the founding
  generation the one cohort in the game with no gender at all (`+0x38 == 0` for everyone →
  `helper_fe40`'s opposite-gender test can never pass, and the whole cohort passes the
  female reproduction gate). Retail writes `+0x38` for every colonist unconditionally.
  Family **cross-linking** (spouse `+0x250`, child ring) stays behind
  `BANISHED_LIVE_CITIZEN_FAMILY` — that genuinely changes population dynamics.
* **`kidVariant` off-by-one fixed.** Verified in `CitizenComponent_impl_0dd0` (`0x1400a0dd0`):
  `bVar12` is initialised `true` once, above the family loop, and only toggled per child.
  `sub_1040` derives gender as `(uint)(param_5 == '\0')`, so the first child is gender 0, and
  `SetVariant` gets `(ushort)(gender == 0)` = variantIdx **1**. Ours started at 0, putting
  every founding child's gender/mesh one step out of phase with vanilla for the whole spawn.
* **`[FamilyCensus]` now reports `gender(+0x38) female=N male=M`** so the A/B is measurable.

Verified on a defaults-only run (no env flags but the census): 23 citizens, `female=12
male=11`, stable and crash-free to frame 12000 with zero guard trips. Before the ungate this
would have read `female=23 male=0`.

⭐ **Deferred to next session (explicitly, by the user): the marriage / birth-rate soak.**

* **Birth-rate A/B.** With gender real, roughly half the founding adults stop passing the
  `+0x38 == 0` reproduction gate, so the birth rate should drop by about half. Not yet
  measured over a long soak. Do it before `BANISHED_LIVE_CITIZEN_FAMILY` graduates.
* **Marriage soak.** A grown child must reach `desc+0x48`, have `host == 0` and a workplace
  `+0x238`. Nothing new appeared in 36k frames previously — but that run had every colonist
  reading as female, so `helper_fe40`'s opposite-gender test could not pass by construction.
  **The earlier negative result proves nothing.** Re-run it now that gender is real.

Tooling for both is in place: `BANISHED_FAMILY_CENSUS=<f>` now reports
`gender(+0x38) female=N male=M` alongside the spouse/ring/partner/host counts, and
`BANISHED_FORCE_SPEED=10` makes a long soak practical.

### NEW-1 — the scheduler's id-map at `sched+0x30` is empty ⭐

Found while trying to exercise the re-key. At the moment `f520` fires on a staffed,
mid-construction site with a live work id (`workComp+0x28 = 2`):

```
[BuildTrace] Rekey id=2: NOT in the sched+0x30 id-map
             (root=0000000000000000 rootId=-1 count=0 buckets=2) — nothing to move
```

`sched+0x120` has its 2 profession buckets, so work items ARE being filed there — but the
flat id-map at `sched+0x30` has a null root and count 0. Something on the insert side is not
landing. `CitizenAI_WorkScheduler_Insert` does call `s_9a40(schedB + 0x30, &keyNode, &mapVal)`
and the registered `WorkCellBST_Insert_9a40` body looks correct (it even bumps the count at
`root+8`, which reads 0), so the next step is to find out whether that call happens, whether
its result is immediately removed (`ead0`/`eb50` at the head of `Post`), or whether `+0x30`
is the wrong field for this map.

Two consequences beyond the re-key:

* `CitizenAI_WorkScheduler_Post` walks the same map to find the payload and write
  `workComp+0x2c` = the work's stored priority. With the map empty it takes the miss branch
  **every time** and writes 0. Every posted work has had priority 0 since that port landed.
* `workComp+0x28` holds ids the map does not know about, so it may be going stale.

Also worth noting: `Insert`'s comment calls `sched+0x120` a "per-work-entity tree", but
retail's `f8400` keys the same tree by PROFESSION (`wpc+0x48`, the Builder key). Both are
consistent only if the work's vtable slot `+0x50` returns the profession key. Confirm which,
and fix whichever comment is wrong.

### NEW-2 — upgrade path use-after-free (pre-existing, reproduces with all flags off)

After `[DestroySM] UPGRADE SPAWN`, a queued `DropOffRawMaterial` still names the demolished
subject at `+0x38` while its own fields are already torn down:

```
DropOffRawMaterial::AbstractImpl RUN self=… src(+0x38)=<the demolished house>
                                 resType(+0x28)=0 count(+0x24)=-1
→ ACCESS_VIOLATION inside Entity_FindComponentByExeTypeId, READ of address 6
```

Confirmed with `BANISHED_LIVE_DESTROY_WORKERS=1` **and** `=0` — same last log line, same
cause, different fault address (it walks a different part of the freed block). This is the
removal-lifecycle dangling-weak-ref item already flagged as the next systemic piece of work;
it is what stops the upgrade path from being called clean, and it is not caused by anything
in this pass.

### OPEN 6 — pause click on the final binary

Still a manual, two-minute check: click pause on a staffed workplace with no env flags and
confirm no crash and no `[StatusIcon] GUARD:` line. Automated runs cannot inject that click.

---

## Testing notes

* **EXPIRED 2026-08-15:** BANISHED_BUILDER_AUTOPOOL / BANISHED_PASTURE_HERDERS were RETIRED 2026-08-15 — the flags no longer exist and are ignored. Seating is done by the panel-click driver (standalone/panel_staffing_driver.cpp), which auto-arms on unattended runs; BANISHED_AUTOSTAFF=0 disables it.
* ~~The upgrade harness needs `BANISHED_BUILDER_AUTOPOOL=1`.~~ Without it,
  `BuilderPool_Tick` returns at `if (!AutoPoolOn())` (auto-seating is spinner-driven by
  default), so no builder is ever seated, `BuildComponent+0x48` stays 0 forever and the
  harness sits at `delivered=0 required=30` until its 36000-frame timeout. This cost a whole
  wasted run. Full command:

  ```
  BANISHED_AUTOSTART=1 BANISHED_TOOL_HARNESS=upgrade BANISHED_BUILD_DEMO_HOUSE=1 \
  BANISHED_BUILDER_AUTOPOOL=1 BANISHED_LIVE_DESTROY_WORKERS=1 BANISHED_BUILD_TRACE=1 \
  ./reimpl-x64.exe
  ```

* Build labour is slow — roughly 3 units per 1600 frames, so the 30-unit demo house needs
  ~20k frames before the upgrade click can fire. Budget several minutes of wall clock.

---

## Lessons worth not re-learning

* **A small integer where a heap pointer belongs means the WRONG OBJECT, not a corrupted
  right one.**
* **Never alignment-test a FUNCTION pointer.** Only 30 of 132 component `GetTypeId`
  functions are 8-aligned.
* **An unflagged guard on the default path is a behaviour change you have not noticed yet.**
* **Test with defaults only.**
* **"Ported but unregistered" resolves to address 0.** Now three times in this project
  (`SetAccepting`, `f670`, `SetMaxWorkers`). Before writing a second copy, and before
  believing a "not ported" note, grep the address.
* **A rename can silently repoint a call site** when the new name already exists elsewhere.
  See the hazard box under OPEN 4.
* **A negative soak result is only evidence if the mechanism under test could have fired.**
  The 36k-frame "no marriages" result was measured with gender universally 0.
