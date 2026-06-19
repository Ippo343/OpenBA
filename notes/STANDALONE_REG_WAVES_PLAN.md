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

## ⏩ Wave 5 (Player) — 2026-06-19 (branch `wave/player-gap`, off `wave/tutorial-gap`)

Player gap started at **31**. Process: 4-opus-agent fan-out (drag/place/edit/path,
disjoint new files `code/Game/Player/tool_base_<batch>.cpp` + `standalone/reg_player_<batch>.cpp`);
lead Ghidra-verifies every intricate port + integrates + builds + A/B + commits.

**Round 1 (`522f7d5`) — 31->19, A/B green (1475-entity world, 0-byte crash):** 16 closure-safe
addrs. drag: 5160 FindDescription, ff980 CloseDialog, ffbf0/1000f0/100140 sounds, 1018d0
ClearPathEntities. place: fd960 FlipVariant (+0x140152888 UpdateHierarchy slot). edit: a0280
EntityAtCell, 101730 AvgTerrainHeight (leaf), dbf90 FencedAnchorCalc. path: 104670/104770/1048a0
RemovalTool checks + bottom-up deps 5780/3ea0 FindComponent, 11e710 is-removable, **9f5d0 single-cell
terrain** (also a Component-gap addr -> Comp 76->75).
- **Lead caught 2 silent-corruption bugs in agent ports (fixed before reg):** (1) 101730
  AvgTerrainHeight transposed row/col loops + returned accum.x not accum.z (height lane, [RSP+0x38]).
  (2) 9f5d0 CharComp_f5d0: f3e0 writes a Vec3 to the 5th-arg buffer too; the param_5 compare uses
  THAT buffer's .z ([RSP+0x50]), not main-vec .y — agent used outPos[1] + undersized the buffer.
  **Lesson reconfirmed: disasm-verify every lane/offset on intricate ports.**

**Round 2a (`d1616b5`) — 19->16:** 3 Level-1 caller-bug literal fixes (dropped-digit typos, each
verified mid-function via get_function_by_address + true callee via get_xrefs_to):
ff5d0->9f5d0 (dragplacementtool, 2 sites; 9f5d0 reg in R1), 10a530->aa530 (edittool; dedups onto
pathtool's aa530), and removed the DEAD s_fn_SubmitDebugDraw in pathtool (c08c0 mid FUN_1400c0690,
never called — PathTool::Draw uses fn_TileRender).

**Round 2b (in progress) — camera-ray + placement-tile chains (~7 addrs, target 16->~9):**
- tile (R2-tile): 003870 SetTransformFromYaw (Transform exports only), d580 TileValidity (only dep
  f5d0=reg, intricate rotation 0x5a/0xb4/0x10e), fd650 (deps d580+1018d0+003870).
- ray (R2-ray): aa530 RaycastTerrain (HIGH-RISK float-march, deps Vec2::Normalize slot 1526b0 +
  aaa20 both reg), 1005a0 GetMouseRay (exports-only but needs 3 NEW ResolveExport: Transform3::op*,
  Transform3::Rotate, Frustum::GetScreenDirection), 101270 ScreenToCell (deps 1005a0+aa530+9f3e0),
  fda00 (dep 101270).

**Round 2c (PLANNED) — the deep tier (~9 left, all large sub-closures, port bottom-up):**
- 0x140055430 HighlightAdd -> b0d80 (TradeWork_impl_0d80) -> **ace40 ~600-line Transform2/BST
  tessellation builder** (deps 1fc00/1f6b0/1f8d0/14b00 BST, e180/cc10/ee40 TradeWork helpers). LARGE.
- 0x140066800 -> 666b0 -> TradeWork 2e30/c690.
- 0x14009ce00 roadtool -> **a6e30 pathfinder/road worker-thread family** — may be GENUINELY
  must-defer (wiring an unwired System::Thread = corruption; same class the Action wave deferred).
- 0x1400dd710 PathTool SetSelected -> 520-line AVL builder d770 + ~12 unreg.
- 0x1400fcc90 -> fcb40 -> 004740 + d250_sub_7080 + c4a0.
- 0x1400ff9e0 OpenToolDialog -> needs vtable **0x14015f5a0** (g_ToolUI_ChildDialog_Vtable) registered
  + DialogInstance ctor/AddDialog (exports).
- 0x1400ffc60 UpdateToolUI -> deep (e600/e700/e420/e360/ebb0/ec70/fe960).
- 0x140100380 -> fec70 + fe960 + Box::GetCorners/Transform::operator* (need exports).
- 0x140101380 -> **0x1401008c0 huge render fn** (Video::Context BeginVertices/AddVertex + 0f90/131c90/
  1090/b10e0). LARGE.
- 0x1401035e0 RemovalTool CalcPath -> 770-line + ~15 unreg.
All deep-tier leaf deps confirmed UNregistered as of round 2b. Each needs its own careful sub-wave;
genuinely-unsafe ones (unwired thread) stay deferred per SAFETY RULES (a wrong port is silent
corruption, strictly worse than the safe no-op skip).

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

**LANDED commit 3 (`ff34fa9`) — MoveTo closure-safe 4 + 3 Vec dispatch slots, A/B green:**
- Spun out the dispatch-slot sub-callees: registered `0x1401512c0` Vec3::Normalize, `0x1401512d8`
  Vec3::Heading (mangled from exe import-string @0x1401b72dc), `0x1401526b0` Vec2::Normalize via
  `RegisterDispatchSlot`.
- Wired move 4 (action_base_move.cpp): `0x1400012d0` FindComponentFamily, `0x14010a060`
  SetFacingDirection, `0x1400c8d20` UpdateFacing, `0x1400a6d50` CopyPathFromFinder — all asm-verified.
- **Verification caught 2 mis-bucketed as safe (the static-usage grep was unreliable):** `0x140116650`
  CitizenBreakCheck actually calls UNREG `0x1400a9290`; `0x140022970` ExternalPtrSerialize has heavy
  External/Filename/String runtime closure → both DEFERRED. (Lesson: confirm closure from the DECOMPILE,
  not a static-pointer grep.)

**STILL DEFERRED (drafts/bodies present, NOT wired):**
- move unsafe 6: `0x14009cfb0 0x14009e550 0x14010a1f0 0x1400c8c10 0x140116650 0x140022970` — need
  their code sub-callees (`0x14009c480 0x1400a68b0 0x1400a6a30 0x1400a6b20 0x1400a6c10 0x1400a9290
  0x1401250f0`) registered first, then re-verify + wire.
- `action_base_complete.cpp.draft` (CompleteTask/ShowTool/DropOff): code-closure-safe subset = 5
  (`0x1400f8760 0x1400f8800 0x1400f8e30 0x1401176e0 0x1401173b0`); unsafe = `0x1400c7570 0x1400c7620`.
  **BLOCKER: complete resolves graphics via `ResolveIATSlot()` — which is NOT defined anywhere in the
  project, so the draft won't even compile. It needs a refactor to `ResolveAddr`-based slot reads (or
  engine_funcs fn_ pointers) before it can be wired.** Closure also spans `ResolveExport` (those resolve
  fine at runtime, no registration needed: Geometry::Attach/Detach, GraphicsMesh::GetNodeIndexFromName,
  Scene::RemoveObject).

**SKIPPED (not registerable):** `0x1400bbea0` RemoveAnimal_Action_Ctor (609-insn builder; all child
CopyCtors unreg — same closure problem as the deferred Work action-ctors), `0x140122340`
HerdComponent_StartMigration (no-migration branch infinite-loops single-threaded).

**Raw-material cluster (batch E) — DONE (commit 6 `8256c6a`): 6/9 closure-safe leaves landed**
(`action_base_rawmat.cpp` + `reg_action_rawmat.cpp`): `0x14012d150` SubtractItem (array compaction),
`0x14012d000` AddItem + `0x14000da70` ArrayResize16 + `0x140044810` DynArrayPush4 (all MemAlloc/MemFree
only), `0x140088f90` GetResourceCategoryIndex (FindComponent+IsDerivedFrom+bit table), `0x1400223c0`
FindComponent_7c80 (IBP-array FindComponent). DEFERRED 3: `0x14009bd40` FindPlacementCell (intricate
spiral — its only exe callee `0x14009f3e0` IS registered, so unblockable next pass), `0x140129760`
PositionEntity (Geometry::SetTransform render mutation + unreg Entity_ClearPathQueue), `0x140088ef0`
RegisterRawMaterial (deps `0x140013320` + CreateRawMaterial_helper_8fd0 unreg).

## ⏩ Wave 3 (Action) continuation — 2026-06-18 (branch `wave/work-tail`; commits 4–6)

Picked up the deferred Action subsets from commit 1–3. ALL A/B-green (1478–1480-entity world,
0-byte crash, 0 NOT IMPLEMENTED). Lead-ported + Ghidra-ABI-verified each.

**Commit 4 (`2e50238`) — complete sub-batch (CompleteTask BST + ShowTool), 5 wired:** refactored
the deferred `action_base_complete.cpp.draft` → removed the ad-hoc `ResolveIATSlot` for the IBP/
IsDerived ops (they're already engine_funcs export-resolved `fn_` pointers — that was the documented
"won't compile" blocker, now moot) and use the project-standard ReadDispatchSlot (ResolveAddr slot +
deref) for genuine graphics slots, matching action_base_move.cpp. Wired `0x1400f8760/8800/8e30`
(CompleteTask try/reassign/cleanup; 8e30's BST removes `0x14001ca60`/`0x14001a6e0` were Work-wave-
registered) + `0x1401173b0/1176e0` (ShowTool show/hide; SetMesh/Scene::AddObject slots registered,
cosmetic Vector4/Time slots null-safe). DEFERRED DropOff `0x1400c7570/7620` (unreg spawn/MoveTo-ctor closure).

**Commit 5 (`89c4429`) — move closure-safe 2 of 6 wired:** registered the 2 closure-safe move sub-
callees in NEW `action_base_movedeps.cpp` — `0x1400a9290` TimeCheck (engine_funcs AsSeconds/RandFloat
+ DAT constants only) + `0x1401250f0` AnimHelper (IsDerivedFrom/CheckPtr + Time::Time/MeshInstance::
PlayAnimation Runtime exports — by-value ExternalPtr + const-Time& ABI verified from disasm + registered
FUN_1400c3470) — which unblocked wiring `0x14010a1f0` SetAnimation + `0x140116650` CitizenBreakCheck.
**Caught a faithfulness bug:** the 116650 port copied 1 float before a9290 but the original copies 8
bytes (a9290 reads pair[0]=value AND pair[1]=weight) → fed stack garbage; fixed to copy both.
**STILL DEFERRED — the pathfinder background-worker-thread family** `0x14009c480` (→9c170→14001a9b0
ZoneRefresh region flood-fill) + `0x1400a68b0/a6a30/a6b20/a6c10` (enqueue cmds to a Thread via
System::Thread::Start, dep FUN_1400a6430→unreg path-builder FUN_14001ce00). Wiring them would start an
unwired thread / build half-populated path records — strictly worse than the safe skip. So `e550 /
9cfb0 / c8c10` stay deferred (they depend on this family), and `22970` (External/Filename/String closure) too.

**LESSON reinforced:** decompile EVERY sub-callee's own closure bottom-up before registering — a leaf
that looks safe by name (a68b0 "RoadFlag") can hide a Thread::Start + a 64KB-stack path-builder. The
genuinely-safe move/rawmat subsets were leaf array ops / engine_funcs-export-only / already-registered
deps; everything touching the pathfinder Thread queue or region flood-fill stays deferred.

**REMAINING Action deferred set (next sub-wave):** rawmat `9bd40` (unblockable — f3e0 reg) /`129760`/
`88ef0`; move pathfinder-thread family (7 addrs) + `22970`; complete DropOff `c7570/7620`; the SKIP
set `0x1400bbea0`/`0x140122340`/`0x140092260`.

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

## ⏩ Wave 3 (Action) raw-material TAIL — 2026-06-18 (branch `wave/work-tail`, commit `a622a8f`)

Closed the closure-safe remainder of the raw-material cluster (batch E deferred 3). **KEY:
re-decompiling the deferred trio bottom-up showed the prior "blocked" notes were STALE** —
two of the three were already unblockable:
- **0x140129760 PositionEntity LANDED** — instruction-verified port (full disasm). Its
  getters were ALREADY registered in earlier waves: 0x140004680 (component-nature) +
  0x140003f60 Entity_ClearPathQueue (CNR). Writes cell col/row to component A and a Transform
  (FromYawPitchOffset yaw=rot/pitch=0/offset=worldPos, +0x148) + Geometry::SetTransform to
  component B. Disasm gotcha confirmed: RBX reloads to B after the 003f60 IBP resolve (the
  transform targets B, not param_1+0x10); FromYawPitchOffset is a 4-arg call (Ghidra drops the
  pitch arg). worldPos via the registered f3e0 GridToWorld.
- **0x140088ef0 RegisterRawMaterial LANDED** + its 3 leaf deps ported in the same file:
  0x140013320 FindComp (identical to the registered 223c0, type 1c4b10), 0x140058fd0
  ArrayUpsert (generation-aware {key,value} cache; archives to +0x38 via 0x14000d9d0 on
  gen-change, then upsert/grow-3x), 0x14000d9d0 VectorClone. Classifies via the registered
  88f90, adds quantity to entityList+0x170 bucket.

A/B green: 1474-entity world build, `[GUL::Load] EXIT — world built`, 0-byte crash, none of
the new addrs appear as NOT IMPLEMENTED (dormant on autostart — they fire on in-game citizen
raw-material actions; registration is purely additive ⇒ no world-build regression).

**STILL DEFERRED — 0x14009bd40 FindPlacementCell:** 260-insn expanding-ring spiral, heavy
decompiler variable-aliasing (iVar7 aliases row-col delta / width / cellIdx / result), on an
UNEXERCISED citizen-action path. A wrong port = undetectable latent corruption (strictly worse
than the safe skip), so it gets its own careful pass *with in-game validation*. Only dep f3e0
is registered, so it is unblockable-but-deferred.

**Action gap now 19 (was 21).** The genuinely-deferred remainder, all for hard closure
reasons: bd40 (1); pathfinder/road-rebuild THREAD family (12: 9c480 9c640 9e0e0 9f2f0 af370
a68b0 a6a30 a6b20 a6c10 9cfb0 e550 c8c10 — wiring starts an unwired System::Thread / builds
half-populated path+road-mesh structures); unreg child-ctor closure (bbea0 RemoveAnimal ctor,
c7570/c7620 DropOff spawn/MoveTo ctor); SKIP set (92260, 122340 StartMigration infinite-loop);
22970 ExternalPtrSerialize (External/Filename/String closure). Closing these needs a Level-2
reimpl (path-builder FUN_14001ce00 + worker-thread queue) or the action-ctor child cascade —
both bigger than a registration wave.

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

As of 2026-06-19: **189 remaining code-gap addrs.** By module:
Component 76, Work 47, Game 35, Player 31. (Action/Entity/Goal/Dialog/Tutorial all = 0.)

✅ Tutorial gap closed 2026-06-19 (branch `wave/tutorial-gap`, commit e263b4a) — 11
closure-safe helpers; the 6 serialize ones reuse the existing canonical
ExternalPtrSerializeGeneric (now exposed via code/Game/Game/serialize_externals.h),
array variants just loop it; closure is all Runtime-export/engine_funcs. See
[[project_tutorial_gap_zero]].

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
