# CitizenComponent::OnRemove — CLOSED 2026-07-25. Open threads for next session.

Superseded the original brief (commit `8f4f337`). The task it described is DONE and
graduated; everything below is what is still open. Branch
`wave/upgrade-path-complete`, commit `eb6d8aa`.

---

## What landed

| Feature | Flag | State |
|---|---|---|
| `CitizenComponent::OnRemove` (0x140113ca0) | `BANISHED_LIVE_CITIZEN_ONREMOVE` | **default-on** |
| `SetAccepting` pause path (0x14012fc00) | `BANISHED_LIVE_WPC_PAUSE` | **default-on** |
| BuildUI wrong-finder fix | none (unflagged) | **in** |
| StatusIcon array-entry guard | none (unflagged) | in, one deviation |
| Founding family links + marriage finder | `BANISHED_LIVE_CITIZEN_FAMILY` | opt-in |
| DestroySM worker release (12f520/12f670) | `BANISHED_LIVE_DESTROY_WORKERS` | opt-in |
| BuildComponent slot 7 → townMgr+0xd0 | `BANISHED_LIVE_BUILD_ONREMOVE` | opt-in |

Test tooling added: `standalone/death_harness.cpp` —
`BANISHED_KILL_CITIZENS=<n>` / `BANISHED_KILL_AT_FRAME=<f>` kill citizens on cue,
`BANISHED_KILL_FAMILY=<n>` waits for family links to exist then kills those citizens
(the only practical way to reach OnRemove's step-4 branch), and
`BANISHED_FAMILY_CENSUS=<f>` reports spouse/ring/partner/host counts. Natural deaths are
decades of game time away, so this is how any removal path gets exercised.

---

## OPEN 1 — `FUN_1400f8400` unported (blocks DESTROY_WORKERS graduation)

The work-scheduler re-key: moves a pending task node between profession buckets inside
`mgr+0x120`, keyed by work id. Reachable whenever a WorkComponent still holds a work id
(`+0x28 >= 0`). Both `FUN_14012f520` and `FUN_14012f670` need it; both log
`[DestroyWorkers] NOT-IMPLEMENTED FUN_1400f8400` and continue. Consequence: a work item
in flight when demolition starts keeps its pre-demolition profession key, so the wrong
profession's citizens can be offered it. Blocked behind an unported AVL-node-move cluster
(`CitizenAI_helper_cb50`, `TradeDescription_impl_ca60` / `_9a40`, `FUN_14000e320`).

Also in that port, `WorkPlaceComponent_LeaveBuilderProfession` (the pre-existing f670
body) DROPS retail's `IsDerivedFrom(desc, DAT_1401c8118)` check, so where retail resets
`+0x48` to 0 we may write `desc+0x20`.

**Verify first:** demolish/upgrade a STAFFED building with `BANISHED_LIVE_DESTROY_WORKERS=1`
and confirm `[DestroyWorkers] f520 re-seat ... roster=0` and that the `FUN_1400f8400`
line does NOT appear.

## OPEN 2 — the worker spinner calls address 0 (live latent crash)

`code/Game/Dialog/workplaceui.cpp:80` uses `fn_WorkplaceComp_SetMaxWorkers`, which is
NULL because **0x14012fb00 is unregistered**. The up/down worker spinner in the workplace
panel — the same panel as the pause button — calls address 0 today. Independent of
everything in `eb6d8aa`. Registering it would fix the crash but also puts that ungated UI
path onto the `AssignHousing` eviction loop; `destroycomponent_workerrelease.cpp` has a
file-local port of `FUN_14012fb00` that could be registered instead.

## OPEN 3 — `0x14012e5c0` registered twice, correctness depends on batch order

`HuntComponent_ComputeTerrainSpeed` (`reg_component_nature.cpp:120`, called at
`address_registry.cpp:525`) and `TutBase_ComputeWorkCapacity` (`reg_tutorial_base.cpp:43`,
called at `:635`). `RegisterExeAddr` is last-write-wins, so TutBase currently wins — which
happens to be the CORRECT semantics (the Ghidra body computes footprint-scaled work
capacity from `desc+0x24/+0x30/+0x34`; the Hunt "terrain speed" name looks like a
mis-identification). The f520 capacity calculation depends on that ordering. Reordering
the batches would silently produce garbage capacity. Resolve properly: confirm the real
semantics, delete the wrong registration, rename.

## OPEN 4 — rename the misnamed component finders

`engine/funcs/engine_funcs.h:752` `fn_FindComponent_Workplace` resolves **c8070
StorageLocation**; `:768` `fn_FindComponent_BuildState2` resolves **c80e0
WorkPlaceComponent**. This cost three crashes and a long wrong-diagnosis detour. Do NOT
change either helper's behaviour — five call sites (`engine_funcs.h:4974`,
`workplaceui.cpp:68`, `:301`) genuinely want the c8070 one. Rename both, then re-check
`workplaceui.cpp:198`/`:239`, which use the c8070 finder and then read `+0x18` for the
WorkplaceJobType while `:234` uses the c80e0 finder — decompile WorkPlaceUI @0x140087080
to settle which retail uses.

## OPEN 5 — family links: make gender retail-accurate, then soak

`FUN_140117080` (per-citizen deferred init) is guarded off in standalone, and it is what
retail uses to write gender at `+0x38`. `starttown.cpp` currently derives gender from the
mesh variant instead (`variantIdx 0 ⇔ gender 1`) — the value is identical because retail
sets variant FROM gender and the mapping is bijective, but the mechanism is ours. Work out
why 117080 is guarded off and restore it; that also makes the BIRTH path real
(`CitizenComp_helper_1b90` → `DeferredInit_117080` → `sub_1fa0` is already correct and
just never fires).

Then soak for marriages: a grown child must reach `desc+0x48`, have `host == 0` and a
workplace `+0x238`. Nothing new appeared in 36k frames. **Expect birth rate to roughly
halve** once gender is real — `+0x38 == 0` (female) gates reproduction and the whole town
currently counts as female. Worth an explicit A/B before graduating.

Also pre-existing: `kidVariant` starts at 0 where retail's `bVar12` starts at 1, so our
children's gender/mesh sequence is offset by one from vanilla.

## OPEN 6 — confirm pause on the final binary (2 minutes)

The pause path was verified live on the PRE-thinning binary (two clicks, real citizens
evicted, closed icon confirmed visually above the building and in the dialog status bar).
The committed binary has not had a pause click; the only changes since were guard removals
that never fired. Click pause on a staffed workplace with no env flags and confirm no
crash and no `[StatusIcon] GUARD:` line.

---

## Lessons worth not re-learning

* **A small integer where a heap pointer belongs means the WRONG OBJECT, not a corrupted
  right one.** A roster base of `8` with count > 0 produced three increasingly elaborate
  lifecycle-corruption theories before the one-line finder bug was found.
* **Never alignment-test a FUNCTION pointer.** Only 30 of 132 component `GetTypeId`
  functions are 8-aligned. A guard that did this rejected 77% of components and made
  `MultiObjectComponent` permanently unfindable, silently sending icons down the wrong
  positioning branch.
* **An unflagged guard on the default path is a behaviour change you have not noticed
  yet.** A guard justified only by a hypothesis should be dropped; one justified by an
  observed fault can stay.
* **Test with defaults only.** The guard misfire was invisible in every flag-heavy run.
* **"Ported but unregistered" resolves to address 0.** It bit this project again here
  (`FUN_14012f670` was fully ported and simply never registered). When a feature is dead,
  check registration before writing a second copy.
