# Standalone Registration Waves — Plan (waves 2+)

Written 2026-06-18 after wave 1 (Component). Branch with wave 1: `wave/component-people`
(commits `ebbc385` wiring + `dda1096`/`6f84661` Level-1 address fixes). See
[[project_component_reg_wave]] in memory and `notes/COMPONENT_REG_WAVE_BRIEF.md`.

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

## ⚠️ The blocker that gates real verification

In-game runtime validation is **blocked**: autostart deterministically stalls at
`MenuDLT::UpdateCamera frame=5` (never reaches the 60-frame settle → no New Game → no world
build) on this base branch. A/B-proven this is **base-branch behavior, not the waves**. So every
wave is build/static-validated + A/B-no-regression, but the registered reimpls are **not
exercised in-game**. The "autostart reaches world build / 140fps tick" state was branch
`fix/ingame-worldgen-factory-cascade` (unmerged) — see [[project_starttown_a3300]].

**Decision for next session — pick a track:**
- **Track A (coverage):** grind waves 2→N. Mechanical, adds real (latent) correctness, but none
  of it is in-game-verifiable until the stall is fixed.
- **Track B (unblock first):** fix the menu frame-5 stall / the loader path FIRST (ties to the
  documented FindOrLoad/loader work, [[project_next_goals_findorload_dedup]] /
  [[project_menu_cpu_loader_thread]]), or rebase onto a branch that reaches in-game, so waves
  become verifiable. **Recommended** if the goal is a working standalone, not just coverage.
- Reasonable hybrid: do wave 2 (Work) to keep momentum, then pivot to the stall.

## Separate backlog — Level-2 reimpls (NOT a registration wave)

Wave 1's Level-1 fixes pointed 13 CitizenComponent/RawMaterial/StorageDisplay call sites at the
correct callees, but those callees have no reimpl yet (still null→skipped in standalone). Writing
them = genuine deep-reimpl of the CitizenComponent lifecycle (priority #1 module). True callees:
0x140113af0 AssignState, 0x140116dd0 HandleLifeEvent, 0x140112990 ConsumeFood, 0x140112ae0
ConsumeFuel, 0x140111590 FindWorkplace, 0x1401116f0 FindStorage, 0x1401163d0 AssignWorkplace,
0x140111ee0 AssignHousing, 0x140111840 TryFindPartner, 0x1401127d0 UpdateHealth, 0x1400f72e0
Work_Ctor25, 0x14012ec60 register-draw-item, 0x14009f3e0 tile→world (shared, 100+ callers — high
reuse). Also open: nomad register-vs-teardown semantics (0x140103480 is NOT an address bug).

## Start-here checklist (next session)

1. `git checkout wave/component-people` (or merge it forward first — decide branch strategy).
2. Recompute the gap (commands above); confirm Work/Action splits still match.
3. Decide Track A vs B (above).
4. If Track A wave 2: spawn 3–4 agents (NO worktree), each owning a disjoint Work-file batch,
   brief = `notes/COMPONENT_REG_WAVE_BRIEF.md` + safety rules above + "report reg lines as text,
   don't commit/build/sub-fork." Lead wires + builds + A/B + commits.
