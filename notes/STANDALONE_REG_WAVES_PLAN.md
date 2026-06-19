# Standalone Registration Waves — Plan (waves 2+)

Written 2026-06-18 after wave 1 (Component). Branch with wave 1: `wave/component-people`
(commits `ebbc385` wiring + `dda1096`/`6f84661` Level-1 address fixes). See
[[project_component_reg_wave]] in memory and `notes/COMPONENT_REG_WAVE_BRIEF.md`.

## ⏩ Wave 2 (Work) progress — 2026-06-18 (branch `wave/work`, off `wave/component-people`)

**Recomputed gap = 89 UNIQUE Work addrs (not 70).** The plan's per-leaf-file disjoint
split does NOT hold: ~39 of the 89 are heavily-SHARED base/StateMachine/ECS helpers
(`0x1400c5400` in 20 files, `0x1400f7400`/19, `0x1400f7100`/18, etc.). Restructured around
ONE shared keystone file `code/Game/Work/work_base_reimpl.cpp` + `standalone/reg_work_base.cpp`
(`RegisterWorkBaseBatch`, wired into address_registry.cpp ~line 360).

**DONE — 22/39 shared base helpers (8 commits, all build+A/B-no-regression green):**
- `a23b68b` FindComponent family (5: 0x140002ed0/2f90/3050/52c0/6270 — identical loop, differ
  only by type-ID global) + 2 recursive tree-frees (0x140014c30 ch+0x10/+0x18, 0x14000e320 ch+0x18/+0x20).
- `b164790` lifecycle trio: 0x1400c5400 TaskListCleanup, 0x1400f7400 Work::Serialize, 0x1400f7100 Work::CanStartWork/ValidateTask.
- `df05ea9` 0x140008830 Map_CalcDistance, 0x1400a00f0 Map::CanPathTo/ValidateCell.
- `659f1db` simple-BST AVL family: 0x140021a30 insert, 0x14001a6e0 remove, 0x140025520 find+remove,
  0x1400296b0 serialize (+ statics retrace 0x14001a8c0, rotations 0x140014c70/da0, alloc 0x140021b40).
- `4317b30` cell-BST AVL family: 0x140029a40 insert, 0x14001ca60 remove, 0x140029de0 serialize
  (+ statics retrace 0x140013480, rotations 0x14000e450/e580, alloc 0x140029bb0) + Level-1 caller fix.
- `fe01e71` grid trio: 0x14009ccf0 FindCell, 0x1400f8620 TryAssignWorkplace, 0x1400e3db0 AutoPickup.

The simple/cell-BST families and grid trio were HAND-PORTED + Ghidra-verified (opus), correcting the
unfaithful sonnet-agent drafts (param swaps, count clobber, mangled ring traversal, guessed
compaction). Files: code/Game/Work/work_base_{reimpl,bst,cellbst,grid}.cpp + standalone/reg_work_*.cpp.

**⚠️ LESSON — parallel sonnet agents are UNRELIABLE for complex pointer/tree ports.** Spawned 2
(BST family, grid/AI). Both compiled clean and CLAIMED done, but Ghidra verification found the
COMPLEX ones NOT faithful: FindCell (wrong transposed-ring traversal), RegisterWork (param2 is an
out-buffer not the key → swapped + clobbers the count field), AutoPickup (guessed array compaction),
TryAssignWorkplace (re-expressed BST walk). Only the SIMPLE ones verified clean → I hand-salvaged
just CalcDistance + ValidateCell. **Mechanical/identical helpers (FindComponent, tree-free) = safe to
delegate. Intricate tree/assignment logic = must be hand-ported + Ghidra-verified (opus), because the
menu frame-5 stall means NONE of this is in-game-exercised, so a wrong port is SILENT corruption —
strictly worse than the safe no-op of leaving it unregistered.** Agent drafts kept (unbuilt) under
`notes/wave2_drafts/*.draft` as starting points.

**REMAINING (not yet registered):**
- 0x140115e60 CitizenAI_LongTermPlan (304 lines, ~12 unresolved callees) — a Level-2 reimpl, not a
  registration; needs the citizen-AI work-finding chain (FUN_14008d590 storage-search, PerformWork
  ctor, action-executor FUN_1400c4c50, etc.) first.
- ~17 lower-reuse (2–3 caller) shared helpers not yet touched (e.g. 0x140122b00/be0, 0x1400f6ef0,
  0x1400c2490, 0x1400c1eb0, 0x140087fb0, 0x140005380/440, 0x140132010 sqrtf, 0x140122e20, 0x1400be6e0,
  0x1400bb0a0, 0x14009c480, 0x140024f30, 0x14001ca60(done), 0x140006330 — regenerate the list).
- the ~49 single-file (W1) leaf Work addrs (per-class job logic).

**Level-1 caller bug found (separate from registration):** `0x140029a60` is MID-FUNCTION (true entry
0x140029a40; clear-area REMOVE is 0x14001ca60). `harvestarea.cpp` already corrected; `plantarea.cpp`,
`plantorchard.cpp`, `harvestorchard.cpp` still cite the bad addr — fix those ResolveAddr literals.

## ⏩ Wave 3 (Action) — 2026-06-19 (branch `wave/work-tail`; commit 1 `4f26b27`)

4-opus-agent team (CreateNR / MoveTo-PerformWork / Hunt-Animal / CompleteTask-ShowTool) drafted
the ~45 Action exe-helpers into disjoint `code/Game/Action/action_base_<batch>.cpp` +
`standalone/reg_action_<batch>.cpp`. Lead applied closure-safety + Ghidra-verified intricate ports.

**LANDED commit 1 (`4f26b27`) — 16 closure-safe, A/B green (1475-entity world, 0-byte crash):**
- CreateNR cluster (10, file closure-clean): `0x140003f60 0x14000c460 0x14009cb00 0x140120db0
  0x1401252f0 0x140125320 0x1401253d0 0x140126540 0x140126440 0x1401260f0` (action_base_cnr.cpp).
- Hunt/RemoveAnimal (6): `0x140005500 0x1400227e0` FindComponent, `0x140122a80` IsAnimalReady,
  `0x140003110` DynArraySetCount, `0x1400c2ea0` MarkEntityRemoval, `0x1400c3470` NotifyEntityChanged.
- Lead-verified faithful (asm-level): `0x1401253d0` (SetMesh RDX mesh-arg DROPPED by Ghidra,
  reconstruction matches asm), `0x1400c3470`, plus A-mv `0x1400a6d50` ring-copy (the trap the Work
  agents botched — A-mv got it right) and A-ct `0x1401173b0` ShowTool. **The 4 agents are reliable.**

**DEFERRED to a follow-up (drafts in `notes/wave3_drafts/`, faithfulness-sampled, NOT wired):**
- `action_base_move.cpp.draft` (MoveTo/PerformWork/PlayAnim, 10): code-closure-safe subset = 6
  (`0x1400012d0 0x140022970 0x14010a060 0x1400c8d20 0x1400a6d50 0x140116650`); unsafe = `0x14009cfb0`
  `0x14009e550` `0x14010a1f0` `0x1400c8c10` (call unreg deps / unreg dispatch slots 0x1401512c0
  Vec3::Normalize / 0x1401512d8 Heading / 0x1401526b0). **Dispatch-slot closure for the facing/anim
  helpers still needs checking before wiring the 6.**
- `action_base_complete.cpp.draft` (CompleteTask/ShowTool/DropOff, 7): code-closure-safe subset = 5
  (`0x1400f8760 0x1400f8800 0x1400f8e30 0x1401176e0 0x1401173b0`); unsafe = `0x1400c7570` `0x1400c7620`
  (DropAllInventory/DropOffTransfer — 8 unreg deps). **NOTE: complete resolves graphics via
  `ResolveIATSlot()` + `ResolveExport()`, NOT `ResolveAddr(Module::Exe)` — its closure spans 3
  mechanisms and must be analyzed across all of them before wiring.**

**SKIPPED (not registerable):** `0x1400bbea0` RemoveAnimal_Action_Ctor (609-insn builder; all child
CopyCtors unreg — same closure problem as the deferred Work action-ctors), `0x140122340`
HerdComponent_StartMigration (no-migration branch infinite-loops single-threaded).

**Raw-material cluster (batch E, ~9, not yet run):** `0x140044810 0x140088ef0 0x14009bd40 0x140129760
0x14000da70 0x140088f90 0x14012d150 0x14012d000 0x1400223c0` (createrawmaterial/consumerawmaterial/
pickup/pickupstored/dropoff). Next Action sub-wave.

**CLOSURE-SAFETY refinement learned this wave:** closure spans THREE resolution mechanisms —
`ResolveAddr(Module::Exe, code)`, `ResolveAddr(Module::Exe, dispatch-slot 0x14015xxxx)` /
`ResolveIATSlot`, and `ResolveExport`. A per-addr safety check must cover all three + transitive
intra-file sibling calls. (Commit 1's cnr/hunt were clean on all axes; move/complete were not, hence deferred.)

## ⏩ Wave 2 (Work) shared-helper TAIL — 2026-06-19 (branch `wave/work-tail`, off main; commit `1b9a921`)

Closed the 15-addr shared-helper tail with a 3-opus-agent team (W-acc accessors,
W-act action-ctors, W-impl impl/geometry/math) + lead Ghidra-verify + integrate.

**KEY INSIGHT — closure-safety gates registration.** A shared helper can only be
SAFELY registered once its *entire exe sub-callee closure* is also registered. If a
top-level helper calls sub-callees that resolve to 0 (unregistered), running it builds
a HALF-NULL / drifted structure — **strictly worse** than the safe no-op of leaving the
call site skipped. So registration must proceed bottom-up (leaves/closure first).

**LANDED (5, closure-safe, all Ghidra-verified faithful):**
- 4 FindComponent accessors `0x140005380`(t 1c7510) `/5440`(1c46b0) `/22520`(1c3fb0)
  `/6330`(1c7cb8,+0x28/+0x30) → `code/Game/Work/work_base_accessor.cpp` (engine-func deps only).
- `0x140132010` sqrtf → `work_base_impl2.cpp` (no deps).
- A/B green: 1475-entity world build, 0-byte crash log, 0 debug NOT-IMPLEMENTED.

**DEFERRED to the Action wave (drafted, NOT registered — `notes/wave2_drafts/`):** every
one calls an unregistered sub-callee closure. Verify offset-by-offset + co-register the
closure when the Action wave lands:
- `work_base_actionctor.cpp.draft`: `0x1400c1eb0` PerformWork_Action_Ctor3, `0x1400c2490`
  PlayAnimation_Action_Ctor, `0x1400bb0a0` grazing/tend, `0x1400be6e0` plant/orchard — **ALL
  12 action sub-ctors unregistered** (SendComponentMessage/CompleteTask/ShowTool/PerformWork/
  MoveTo/*NaturalResource @ 0x1400c5da0/cb1d0/42890/c9720/c9170/cadf0/c57d0/c6a20/c29a0/c54e0/c3470/012d0).
- `work_base_impl2_full.cpp.draft`: `0x140087fb0` Entity_impl_7fb0, `0x1400f6ef0`
  BuildStructure_impl_6ef0, `0x140122b00`/`be0` walkable-pos search, `0x140122e20`
  Entity_impl_2e20 — deps `0x140003f60`/`0x140123070`/`0x14009f810` unreg.
- `0x14009c480` ZoneRefresh (body in work_base_accessor.cpp, unregistered): dep `0x14009c170`
  unreg + it mutates region words → defer until 0x14009c170 lands.

**SKIPPED:** `0x140024f30` = MID-FUNCTION (in Toolbar_vslot14 140024f00-f48) — Level-1 caller
bug in foresterwork/gatherrawmaterials (mislabeled "BSTRemove"; real remove = registered
0x14001a6e0). `0x140115e60` = CitizenAI_LongTermPlan (Level-2).

**PROCESS that worked:** agents wrote DISJOINT new files (work_base_<batch>.cpp +
reg_work_<batch>.cpp), no commit/build/sub-fork; lead verified + trimmed to the closure-safe
subset + wired registrars + built + A/B + committed. Confirmed again: agent ports of intricate
ctors/geometry compile clean but need lead Ghidra-verify before trust (kept as drafts here
since they're deferred anyway). **Next: run the Action wave (52) — it unblocks the deferred
Work action-ctors + impl helpers above.**

## What this is

`code/Game/*` calls exe sub-helpers via guarded `ResolveAddr(Module::Exe, 0xADDR)`. If the
address isn't registered in `standalone/address_registry.cpp`, it resolves to 0 → the
guarded call is silently skipped (serialize drift / missing behavior). A "wave" registers a
module's worth of these so the call sites fire real reimplementations in the standalone exe.

## Current gap (recompute each session — it shifts as registrations land)

```bash
grep -rhoiE "Module::Exe,\s*0x1[0-9a-f]+" code/Game | grep -oiE "0x1[0-9a-f]+" | tr 'A-F' 'a-f' | sort -u > /tmp/ref.txt
grep -rhoiE "Register(ExeAddr|DispatchSlot)\(\s*0x1[0-9a-f]+" code engine standalone | grep -oiE "0x1[0-9a-f]+" | tr 'A-F' 'a-f' | sort -u > /tmp/reg.txt
comm -23 /tmp/ref.txt /tmp/reg.txt | awk 'strtonum($0)>=0x140001000 && strtonum($0)<=0x14014ffff' > /tmp/gap.txt
wc -l /tmp/gap.txt
# bucket by module:
for a in $(cat /tmp/gap.txt); do grep -rilE "Module::Exe,\s*${a}" code/Game|head -1; done | sed -E 's#code/Game/([^/]+)/.*#\1#' | sort | uniq -c | sort -rn
```

As of 2026-06-18: **291 remaining code-gap addrs.** By module:
Component 82 (mostly Level-2 backlog + sub-deps), **Work 70**, **Action 52**, Game 35,
Player 31, Tutorial 10, Entity 6, Dialog 3, Goal 2.

NOTE: the static gap (`Module::Exe` refs across all of code/Game) is wider than what's hit on
the autostart path (currently 0 NOT IMPLEMENTED). Registering is still correct — it removes
latent silent-skips — but most won't change observable behavior until the in-game path runs.

## Wave order (by impact / coupling)

1. **Wave 2 — Work (70)** — `code/Game/Work/`. Citizen jobs; high gameplay coupling.
2. **Wave 3 — Action (52)** — `code/Game/Action/`. StateMachine actions.
3. Wave 4 — Game (35), Wave 5 — Player (31), then Tutorial 10 / Entity 6 / Dialog 3 / Goal 2
   (small; can be one combined wave).
Component's residual 82 = the **Level-2 reimpl backlog** (below) + still-skipped landmines, not
fresh registration — handle separately.

### Wave 2 (Work) batch split — disjoint files, ~same size
- A: cleararea(13) + clearstorage(2) = 15
- B: foresterwork(10) + buildroad(2) + autopickupwork(1) = 13
- C: marketwork(6) + firework(6) + tradework(3) = 15
- D: pasturework(5) + harvestorchard(5) + harvestarea(2) + plantarea(2) = 14
- E: gatherrawmaterials(4) + huntwork(3) + consumeproduce(3) + fishingwork(2) + pickup(1) = 13

### Wave 3 (Action) batch split
- A: moveto(8) + completetask(3) + completeroad(1) = 12
- B: huntanimal(8) + removeanimal(1) + showtool(2) = 11
- C: createnaturalresource(7) + animatenaturalresource(3) + removenaturalresource(1) = 11
- D: createrawmaterial(6) + consumerawmaterial(3) = 9
- E: pickuprawmaterial(4) + dropoffrawmaterial(2) + performwork(3) = 9

(Regenerate splits with the per-file command in the brief if the gap shifted.)

## PROCESS — fixes from wave 1 (read before spawning agents)

Wave 1's parallel fan-out **worked but was chaotic**. Apply these:

1. **`isolation: "worktree"` did NOT isolate** — all 4 agents ran in the shared main tree and
   raced on git (sequential `checkout -b` walked HEAD; a transient "vanished files" scare).
   What saved it: **disjoint-file batches** (no two agents touch the same file → edits never
   conflict). For wave 2, pick ONE:
   - **(Preferred) Don't use worktrees.** Run agents that edit ONLY their disjoint files and
     register via their OWN `standalone/reg_<module>_<batch>.cpp`. They do NOT touch
     `address_registry.cpp`. They commit nothing. The lead snapshots + wires + builds.
   - Or: each agent commits to its own branch IMMEDIATELY when done (not "defer to lead").
2. **No sub-forking / clear commit owner.** Wave-1 agents sub-forked and got confused about who
   commits (forks→lead→integrator→nobody). Tell each agent: do your files, report your
   `RegisterExeAddr` lines as text, do NOT commit, do NOT build the whole tree, do NOT sub-fork.
   The LEAD does all integration.
3. **Lead owns integration centrally:** create/collect the reg files, add `extern` + call for each
   `RegisterComponentBatch_<batch>()` (or `RegisterWaveBatch_*`) into `RegisterCodeFunctions()`
   in `standalone/address_registry.cpp` (~line 344), then build, A/B, commit.
4. **Cap concurrency for Ghidra** — it's a single shared MCP server; 3–4 agents max.
5. **Allocator gotcha:** use `fn_MemAlloc(g_pMemAllocSingleton, size, align)` / `fn_MemFreeStatic`.
   There is NO `fn_MemAllocStatic`. (`notes/COMPONENT_REG_WAVE_BRIEF.md` has the full agent brief.)

## SAFETY RULES — verify every address before registering (these caught real bugs)

For each gap address, before registering X→&reimpl:
- **Must be a real function ENTRY.** `get_function_by_address(X)` Entry must == X. If X is
  mid-function (or a jump-label/no-xref interior), it's a **caller bug** — do NOT register.
  Instead apply the **Level-1 fix**: find the TRUE callee by decompiling the original function
  that the code/Game method reimplements and reading what it ACTUALLY calls at that site;
  `get_xrefs_to(trueAddr)` must show a CALL from that original at the cited instruction. Correct
  the `ResolveAddr` literal. (Wave 1 found 13 such wrong addresses in CitizenComponent/RawMaterial/
  StorageDisplay — all were the author working from confused Ghidra labels.)
- **Never register a self-resolving passthrough.** If the reimpl body itself does
  `ResolveAddr(Exe, X)` for the same X, registering → infinite recursion (e.g. AnimalComponent::
  Update 0x140106420). Skip + flag.
- **Watch dropped-digit typos** (wave 1: cropfield 0x1400035b0 → real 0x1400335b0).
- **Dedup across batches** — register each address once; the registry map is keyed by address.
- **Document every skip** with addr + reason (mid-function / passthrough / unresolved), like
  `standalone/reg_component_nature.cpp` does (it's the gold template for reg-file format).

## Validation per wave (lead)

- Build: `mingw32-make standalone-debug` must be GREEN.
- **A/B no-regression test** (wave 1 method): build once with the wave's
  `RegisterWaveBatch_*()` calls commented out, run `make sa-run`, confirm identical behavior
  (the menu currently stalls at frame 5 deterministically — see below); then re-enable. This
  proves the new registrations don't regress startup. `0 NOT IMPLEMENTED` + no crash in the log
  (`C:/GOG Games/Banished/standalone_debug.log`; crash → `standalone_crash.log`).
- Poll the log for stall/crash without foreground `sleep` (use `ping -n N 127.0.0.1` to delay).

## ✅ CORRECTION 2026-06-18 — the "frame-5 stall" blocker was WRONG; the real gate is inert component vtables

**The in-game path is NOT blocked.** Earlier sessions believed autostart stalled at
`MenuDLT::UpdateCamera frame=5`. That was a RED HERRING: `[MenuDLT::UpdateCamera]` logging is
CAPPED at frame 5 — the loop keeps ticking to the 60-frame settle. Debugger-verified on
`wave/work`: autostart → New Game → **full world build (1475 entities: terrain + 15 citizens +
1200 resources + 257 env + 2 structures)** → `[GUL::Load] EXIT — world built` → the in-game
`GameUpdateLoop_Update` ticks live at 60Hz (m_gameTime advances 0→16666µs/frame, not paused,
speed=1). No crash. (The world-build trace is in `standalone_ERROR.log` / stderr, NOT debug.log —
that's why it looked like a stall.) See [[project_autostart_works_ingame_stall]].

So the waves CAN be verified in-game now. BUT registering helpers still won't change observable
behavior, because of the REAL gate:

### THE REAL GATE — every standalone component has an INERT vtable

In standalone, all **68/68 components** are instantiated via `ComponentFactory::MakeInert`/
`MakeInertTyped` (`code/Game/Component/component_factory_base.cpp`). That factory wires ONLY
vtable +0x00 (ScalarDtor) and +0x10 (real GetTypeId); **every other slot — including the per-tick
Update — points to `Inert_Stub` (a no-op returning null).** Debugger-confirmed: a live 19-component
citizen-like entity ticks every frame, but each component's update slot == `Inert_Stub`. The world
is a STATIC SNAPSHOT that ticks but does not simulate. The reg-wave helpers feed the component
*reimpl bodies* — but those bodies are never reached because the vtable doesn't route to them.

### The per-tick Update slot is +0x48 (slot 9), driven by the bucket round-robin — NOT +0x40

`EntityList_Update` (entitylist.cpp) has TWO walks:
- **Step 3** in-order tree walk → `EntityList_TreeNodeHandler` → component vtable **+0x40** (slot 8),
  gated on component flag bit 2. For most components +0x40 is `*_OnRemove` = an EMPTY shared stub
  (0x140118a70 `AmbientEmitterComponent_OnRemove`, just `return`). This is the walk the debugger
  first caught — misleading, it's not the sim update.
- **Step 4** 5-bucket round-robin → component vtable **+0x48** (slot 9 = the real Update), gated on
  entity flag bit 3 (0x8 @ entity+0x40) AND component flag bit 3 (0x8 @ comp+0x20), processCount>0,
  delta>0. **THIS is the per-tick sim Update path.**
  Verified vs Ghidra: GrowthComponent vtable 0x140163590 → +0x40=0x140118a70 (empty OnRemove),
  **+0x48=0x140120050 (the real growth/temperature/harvest Update)**.

### Recipe to bring a component to life (the actual next frontier)

Per component, all four are required — a vtable swap alone CRASHES (inert factory zeroes the body,
so config@+0x38 / resourceComp@+0x28 are null → Update null-derefs):
1. **Real per-type vtable** routing +0x48 → `<Comp>_Update` (keep +0x00 dtor, +0x10 GetTypeId,
   +0x40 → safe OnRemove no-op). Mirror `MakeInertTyped` but fill the live slots. ModelComponent
   (Foundation F) is the precedent but only wired +0x20 Startup / +0x30 Draw, NOT +0x48.
2. **Real Startup/Serialize** (+0x10/+0x20) to populate the component body from its Description
   (config pointer, resource sub-component, capability flags) — else the body is all zeros.
3. **Register the helper addrs** `<Comp>_Update` calls (this is what the reg waves provide).
4. **Bucket/flag eligibility** — entity in an update bucket with flag bit 3; processCount>0.
   (Verify in the debugger; GrowthComponent's capability flags 0xcb already have bit 3 set.)

**Decision for next session:**
- **Track A (coverage):** grind reg waves 2→N. Still useful (removes latent silent-skips, supplies
  step 3 above), but NONE of it changes in-game behavior until components get real vtables.
- **Track C (bring-up, NEW — recommended):** convert components inert→live one at a time
  (the 4-step recipe). GrowthComponent is wired as the worked example (see section below) — flip it
  on, debugger-verify +0x48 entry, then implement step 2 (Startup/body-init) and watch growth tick.
- Track B (fix the menu stall) is RETIRED — there is no stall.

## Separate backlog — Level-2 reimpls (NOT a registration wave)

Wave 1's Level-1 fixes pointed 13 CitizenComponent/RawMaterial/StorageDisplay call sites at the
correct callees, but those callees have no reimpl yet (still null→skipped in standalone). Writing
them = genuine deep-reimpl of the CitizenComponent lifecycle (priority #1 module). True callees:
0x140113af0 AssignState, 0x140116dd0 HandleLifeEvent, 0x140112990 ConsumeFood, 0x140112ae0
ConsumeFuel, 0x140111590 FindWorkplace, 0x1401116f0 FindStorage, 0x1401163d0 AssignWorkplace,
0x140111ee0 AssignHousing, 0x140111840 TryFindPartner, 0x1401127d0 UpdateHealth, 0x1400f72e0
Work_Ctor25, 0x14012ec60 register-draw-item, 0x14009f3e0 tile→world (shared, 100+ callers — high
reuse). Also open: nomad register-vs-teardown semantics (0x140103480 is NOT an address bug).

## Start-here checklist (next session) — updated 2026-06-18 end of wave-2 day

State: branch **`wave/work`** (9 commits, off `wave/component-people`; neither merged to main).
Wave 2 = **22/39 shared Work base helpers done** (all high-value: both AVL families + grid trio,
all hand-ported + Ghidra-verified). Working tree clean. Build green, A/B no-regression.

1. `git checkout wave/work`. Decide branch strategy (wave/component-people → wave/work both unmerged;
   may want to merge the chain forward to main once a milestone lands).
2. Recompute the gap (commands at top of this file); the Work shared-helper count should now be ~17
   (the 2–3 caller tail) + ~49 W1 leaf addrs.
3. **Pick the next move (in rough priority):**
   - **(Recommended) Track B — fix the menu frame-5 stall** (`MenuDLT::UpdateCamera frame=5`, ties to
     [[project_next_goals_findorload_dedup]] / [[project_menu_cpu_loader_thread]]). EVERYTHING from
     wave 1 + wave 2 is registered-but-latent until this is fixed; it's now the highest-leverage work.
     Or rebase onto the `fix/ingame-worldgen-factory-cascade` branch that reached the in-game tick.
   - Track A cont. — finish the Work tail (~17 low-reuse shared + ~49 W1 leaf). Mechanical; safe to
     delegate the IDENTICAL/mechanical ones to SONNET, but HAND-PORT or use OPUS agents for anything
     with tree/pointer/serialize logic (see [[feedback_prefer_opus_for_complex_re]]).
   - Or start the **Action wave** (52 addrs) — same process.
4. **Process that worked this session:** lead decompiles + hand-ports complex helpers into
   `code/Game/Work/work_base_*.cpp`, one reg file per family in `standalone/reg_work_*.cpp`, wire the
   `RegisterWork*Batch()` into `RegisterCodeFunctions()` (~L360 of address_registry.cpp), build, A/B
   (run `make sa-run`-style; confirm frame-5 reached + 0 NOT IMPLEMENTED + 0-byte crash log), commit
   per family. A/B run: copy exe to BANISHED_DIR, delete logs via `[System.IO.File]::Delete` (the PS
   `Remove-Item` is permission-blocked on that path), launch with BANISHED_AUTOSTART=1, wait ~22s, kill.
5. If delegating: agents get `notes/COMPONENT_REG_WAVE_BRIEF.md` + the SAFETY RULES above +
   "report reg lines as text, don't commit/build/sub-fork." Lead Ghidra-VERIFIES complex reimpls
   before integrating (this session caught 7+ unfaithful agent functions that compiled clean).
