# Component GAP Wave — Agent Addendum (2026-06-19, branch wave/component-gap)

Read `notes/COMPONENT_REG_WAVE_BRIEF.md` FIRST for the base process, then this.

This wave closes the **residual 73 Component gap addresses** (wave 1 already did the easy
77). Many of these 73 are genuinely HARD (Level-2 reimpls / self-resolving passthroughs).
Your job is to **classify each address** and only register the **closure-safe** ones.

## The three classifications (decide per address)

1. **CLOSURE-SAFE LEAF** → port + register. Criteria, ALL must hold:
   - `get_function_by_address(X)` Entry == X (real function entry, not mid-function).
   - Its ENTIRE exe sub-callee closure is already registered OR resolvable
     (every `FUN_140xxxx` it CALLs is in `notes/_reg_snapshot.txt`, OR is a Runtime
     export resolved via `ResolveExport`, OR an engine_funcs `fn_` pointer). If it calls
     even ONE unregistered exe `FUN_140xxxx`, it is NOT closure-safe → DEFER.
   - Not a self-resolving passthrough (body does `ResolveAddr(Exe, X)` for the same X →
     infinite recursion → SKIP).
   Port faithfully into the SAME component `.cpp`, register in your reg file.

2. **CALLER-BUG (Level-1 fix)** → the literal in the `.cpp` is wrong:
   - Entry != X (X is mid-function), OR a dropped-digit typo (e.g. 0x1400035b0 vs
     0x1400335b0), OR points at an unrelated function.
   - Find the TRUE callee: decompile the ORIGINAL function that the code/Game method
     reimplements, read what it ACTUALLY calls at that site, confirm with
     `get_xrefs_to(trueAddr)` showing a CALL from that original at the cited instruction.
   - Fix the `ResolveAddr` literal in the `.cpp`. If the true callee is itself
     closure-safe, register it; else note it as a defer.

3. **COMPLEX DEFER** → leave unregistered, document the reason (addr + why) in your report.
   These are: deep closures with unregistered sub-callees, core-sim Updates, functions
   needing uninitialized subsystems (audio/event/UI), worker-thread spin-loops, large
   serialize/ctor cascades. Do NOT register — a wrong/half-null port is SILENT corruption,
   strictly worse than the safe no-op skip.

## KNOWN complex defers — STATUS 2026-07-19: ALL CLOSED except the permanent skip.
- `0x140106420` AnimalComponent::Update — PERMANENT SKIP (self-resolving passthrough;
  registering it to itself = infinite recursion, and no faithful body exists — a real
  closure needs the full 452-inst Update + FindHabitat/SetBounds/MoveTo-action chain).
  Re-confirmed by audit 2026-07-19; the guarded null passthrough is the correct no-op.
- citizencomponent lifecycle — CLOSED (all ported + registered):
  `0x140112990` ConsumeFood, `0x140112ae0` ConsumeFuel, `0x140111590` FindWorkplace,
  `0x1401116f0` FindStorage, `0x140111840` TryFindPartner, `0x140111ee0` AssignHousing
  (reg_component_gap_A.cpp), `0x1401163d0` AssignWorkplace (reg_citizen_assignworkplace.cpp),
  `0x140113af0` PostLifeEvent (reg_eventlog_citizen.cpp),
  `0x140116dd0` HandleLifeEvent — ported 2026-07-19, death/cemetery/tombstone chain,
  7 fns (citizen_lifeevent_handler.cpp + reg_citizen_lifeevent_handler.cpp),
  `0x1401127d0` UpdateHealth — ported 2026-07-19, full disease subsystem, 8 fns
  (citizen_updatehealth.cpp + reg_citizen_updatehealth.cpp).
  Audit 2026-07-19 found + fixed one latent gap: type-id 0x1401c3f78 (AttendanceDescription,
  was misnamed "Residence"/"AttendanceSubComp") was unregistered, making the sub_40f0
  worker-slot gate always-false and silently defeating FindWorkplace/FindStorage/
  TryFindPartner. Now registered + filled in reg_economy_typeids.cpp.
- rawmaterialcomponent — CLOSED: `0x1400f72e0` Work_Ctor25 (reg_work_base.cpp),
  `0x14012ec60` work-post (citizenai_work_scheduler.cpp). Both audit-verified faithful.
- NOTE `0x140111b90` is the partner-candidate/repro helper, NOT UpdateHealth
  (disambiguated; stale comments in citizencomponent.cpp cleaned).

## FINAL citizen-subsystem sweep (2026-07-19) — SUBSYSTEM CLOSED
Full two-direction audit (192 Ghidra citizen-prefix fns vs 810 registered addrs +
every ResolveAddr site in citizen files). Result: everything registered, inlined,
vtable-wired (LiveVtable/REGISTER_TYPE), or dead — after closing the two real gaps:
- `0x14008c3d0` CitizenList::AssignWorker — the top-level 4-strategy seat cascade
  (cbf0→ca20→c5f0→c4b0); was the ONLY live-gameplay null (unguarded call from
  workplaceui worker-add id 0x1450). Ported in citizen_employ_cascade.cpp,
  registered in reg_townhallui.cpp. (notes/component_goals.md:136 +
  entity_goals.md:91 said "already resolved" — that was stale until now.)
- `0x1401c7d28` ResidenceDescription type-id — second instance of the
  unregistered-tid silent-gate bug (defeated the education/residence cast gate in
  CitizenComp_helper_a4f0 + residencecomponent). Registered + filled in
  reg_economy_typeids.cpp; `s_TypeId_Storage1c7d28` renamed `s_TypeId_ResidenceDesc7d28`.

Known non-citizen leftovers deliberately NOT closed here (out of scope):
- Teardown dtors `0x1400c3d30`/`0x1400f8ff0`/`0x1401024d0` (fn_EntityList_Shutdown
  UNGUARDED at gameupdateloop.cpp:1965 — return-to-menu/exit path would crash;
  the other two are member-guarded). Ghidra names carry Citizen* prefixes but they
  are consumed as engine dtors — name-vs-use discrepancy to reconcile whenever the
  shutdown path gets ported.
- `0x140021ba0` bucket-tree serialize (save path), `0x14005fe10`/`0x140060000`/
  `0x140060380` debug cheats — resolved-but-null, fine until those paths are live.

## Allocator / engine func gotchas (from base brief)
- Alloc: `fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)size, align)`; free: `fn_MemFreeStatic(ptr)`.
  NO `fn_MemAllocStatic`. `fn_MemFree` is always NULL — use `fn_MemFreeStatic`.
- Check `engine/funcs/engine_funcs.h` for existing `fn_` pointers before adding.
- Many bodies may ALREADY EXIST in the component `.cpp` (written wave 1, never registered
  because closure wasn't ready). grep the addr first — if a body+typedef exist, you only
  need to closure-check and (if safe) add the reg line.

## Snapshot files (Read these)
- `notes/_reg_snapshot.txt` — all 546 currently-registered/resolvable addrs (your closure
  whitelist). An addr is closure-safe only if every exe callee is in here.
- `notes/_comp_gap.txt` — the 73 gap addrs + their files.

## Deliverables (per agent)
- Edits to YOUR disjoint component `.cpp` files only (callee bodies + any missing call-site
  typedef/signature corrections + Level-1 literal fixes).
- ONE new file `standalone/reg_component_gap_<BATCH>.cpp` with
  `void RegisterComponentGap_<BATCH>()` registering ONLY the closure-safe addrs (and
  corrected caller-bug literals that are closure-safe). Match the format of
  `standalone/reg_component_nature.cpp`. Guard with `#ifdef STANDALONE_EXE`.
- A REPORT (your final message) listing, per addr: classification + one-line reason +
  whether the port is MECHANICAL (trust) or INTRICATE (lead must Ghidra-verify).
- Do NOT edit `standalone/address_registry.cpp`. Do NOT commit. Do NOT build the whole tree.
  Do NOT sub-fork. The LEAD integrates + Ghidra-verifies intricate ports + builds + A/B.
