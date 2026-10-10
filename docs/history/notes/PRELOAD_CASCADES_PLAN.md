# Plan: Complete the in-game resource preload (BANISHED_GAME_PRELOAD) via agent teams

Goal: re-enable `BANISHED_GAME_PRELOAD=1` (GameUpdateLoop GetResourceFilename → "Game/Game")
so the full HUD/dialog resource tree loads and the in-game **toolbar / HUD renders**
(currently absent — bottom-right is empty; see `standalone/ingame_verify.png`).

## Background (read first)
- **The premise that "FindOrLoad dedup is broken" is FALSE.** Measured 2026-06-17: 2314 ExtLoads /
  2312 unique → dedup works; the preload progresses + terminates each load. `ExpandRelativePath →
  Find → External::Load → Add` are correct. Modification::SetupFileSystem is NOT the issue.
  (Memory: `project_findorload_dedup_works_factory_cascade.md`.)
- The preload is blocked by **two cascades**, hit in order as the loader walks the tree:
  1. **Factory cascade** — types reached by the tree that still have `REGISTER_NULL_FACTORY` stubs.
     A stub returns NULL → `IsDerivedFrom(NULL)` crash @Runtime+0xC039A → process dies.
     First one hit + FIXED this session: **Toolbar** (commit 07531ea).
  2. **Serialize-misalignment cascade** — a reimpl `*_Serialize` consumes the wrong byte count →
     a later `String::Serialize` reads a bogus wide-string length → over-reads past the resource →
     **zero-progress `CompressedStream::Read` spin** (engine Read has no guard) → loader HANGS.
     First one hit: **`UIDescription_Serialize`** on `Dialog\StandardDialog.rsc:entity` (a
     `StringTable` ExternalPtr drifts). Same bug class as the multi-session StatGoal saga.
- Reproduce / iterate: `make sa-deploy`, then from `C:\GOG Games\Banished`:
  `BANISHED_AUTOSTART=1 BANISHED_GAME_PRELOAD=1 BANISHED_FOL_TRACE=1 ./reimpl-x64.exe`
  - Factory blocker → last line of `standalone_error.log` is `[NullFactory] Hit for type: X`.
  - Serialize blocker → trace freezes (no NullFactory); `gdb -p PID -batch -ex "thread apply all bt"`
    shows the loader thread in `CompressedStream::Read ← BinaryReader::Serialize(ushort*) ←
    String::Serialize ← <the drifting *_Serialize>`.
  - Launch via `cmd //c start "" reimpl-x64.exe` if you need the window to actually RENDER (a bash
    `&` launch can come up at 120x0 → DrawFailed loop; world still builds either way).
  - Kill zombie: `gdb -p PID -batch -ex kill`.

---

## PHASE 0 — Build two discovery accelerators FIRST (do not skip; they make both cascades parallelizable)

The cascades are painful because each blocker currently CRASHES or HANGS the whole run, so you only
learn ONE blocker per run. Convert each failure mode into a *non-fatal, enumerable diagnostic* so a
SINGLE run lists the entire remaining cascade. Then the wiring/fixing fans out to agents.

**Accelerator A — "skip-deserialize" NullFactory (enumerate the WHOLE factory cascade in one run).**
External::Load only deserializes when `versionFromData == createdObj->vtable[3]()` (GetFileVersion);
on mismatch it SKIPS the body (no field reads, no crash) and returns the empty object. So replace the
crashing `LogAndReturnNull` stub (factory_registrations_bulk.cpp) with one that allocates a tiny
object carrying a minimal vtable whose slot+0x18 (GetFileVersion) returns a sentinel that can NEVER
match (e.g. 0x7FFFFFFF) and slot+0x10 (GetTypeId) returns `GetTypeIdByName(name)`, slot+0x00 a benign
dtor. Gate behind `BANISHED_FACTORY_ENUM=1` (default off → keep current crash behavior). One run then
logs every `[NullFactory-ENUM] need factory: X` for the full tree instead of dying on the first.
(Risk: a skipped/empty object may be dereferenced later in the SAME run → a second-order crash; that's
fine — we only need the enumeration, and each enumerated type gets a real factory anyway. If a
second-order crash truncates the list, the run still advanced past the first N, so iterate a few times.)

**Accelerator B — zero-progress guard detour on `CompressedStream::Read` (enumerate serialize hangs).**
Detour Runtime `IO::CompressedStream::Read` (0x1800aa780; the spinning loop is documented in
`project_menu_cpu_loader_thread.md`) like the existing `filename_runtime_hook.cpp` detours. In the
wrapper, if a Read makes zero progress for N consecutive iterations (window.end==pos, bytesLeft>0),
LOG the current resource path (from the active FindOrLoad — reuse the FOLTrace last-filename) + a
`RtlCaptureStackBackTrace` (names the drifting `*_Serialize`), then return false / a sentinel to break
the spin instead of looping forever. Gate behind `BANISHED_READ_GUARD=1`. Converts every serialize
hang into a logged `[ReadGuard] drift in <Serialize fn> loading <resource>` and lets the loader limp
forward to reveal the NEXT one. (The loader will likely crash downstream from the truncated read — ok,
same as A: we just want the enumerated list of drifting serializers.)

With A+B both on, 1–3 runs should produce: (a) the full list of types still needing real factories,
and (b) the full list of `*_Serialize` functions that drift. THAT list is the work-queue the agent
teams consume.

---

## PHASE 1 — Factory cascade (cheap, highly parallel)

Per-type pattern is mechanical and proven (Toolbar this session; Scenario/NameDatabase/Disease/
ProfessionList before). For each type X:
1. If `code/Game/.../x.cpp` already has Dtor/Serialize/Cleanup/Init reimpls (many do), reuse them.
   Otherwise decompile the exe ctor/factory/Serialize and write them (Toolbar.h-style typed header +
   static_asserts).
2. Read the exe vtable (Ghidra `read_memory` at the type's Vtable addr) → build the N-slot array;
   the load-critical slots are Dtor(+0x00), GetTypeId(+0x10 → `GetTypeIdByName("X")`),
   GetFileVersion(+0x18 → the constant the original returns), Serialize(+0x28).
3. `REGISTER_TYPE(X, "<exact parent string>", &X_Factory, &X_CreateInPlace)` + `RegisterXVtable()`.
4. Wire `RegisterXVtable()` in `standalone/address_registry.cpp`; delete the `REGISTER_NULL_FACTORY(X…)`
   line in `factory_registrations_bulk.cpp`.
Template to hand each agent: `code/Game/Game/toolbar.cpp` (this session) + `scenario.cpp`.

**Agent-team shape (Workflow or parallel Agent spawns):**
- Phase 1a (serial, ~1 step): run Accelerator A → get the type list. The MAIN loop does this, not an agent.
- Phase 1b (parallel, worktree isolation REQUIRED — agents touch separate `x.cpp` but ALL edit
  `address_registry.cpp` + `factory_registrations_bulk.cpp`): one agent per type (or per small batch).
  Each agent: decompile in Ghidra, write the factory, return a STRUCTURED result {type, vtableAddr,
  size, version, files_changed, registry_line_to_add, bulk_line_to_remove}.
  - **Ghidra is single-shared-state** — agents must NOT drive Ghidra concurrently. Either (i) the main
    loop pre-extracts each type's decompile/vtable/version up front and passes it to agents as data
    (preferred — agents then only write C++), or (ii) serialize the Ghidra phase.
- Phase 1c (serial merge, MAIN loop): apply each agent's `address_registry.cpp` + `bulk.cpp` edits
  (these two files are the merge chokepoint — do them centrally to avoid conflicts), build once, run
  Accelerator A again to confirm 0 remaining factory types. (See `feedback_agent_merge_safety`.)

Scale: 146 NullFactory stubs total, ~65 non-UI, but only the subset in the "Game/Game" tree is hit —
the preload churned 2312 resources before the FIRST one (Toolbar), so the reachable set is likely a
few-to-low-dozens. Enumerate before sizing the fleet.

---

## PHASE 2 — Serialize-misalignment cascade (expensive, partially parallel)

Each is a deep byte-diff. The StatGoal precedent (`project_menu_cpu_loader_thread.md`) is the playbook:
1. Identify the drifting `*_Serialize` (Accelerator B names it).
2. Diff the reimpl's per-field read sequence against the ORIGINAL exe disassembly of that Serialize,
   AND against a working sibling. The drift is usually: a missing/extra field, a version-gate applied
   where the original doesn't (or vice-versa — do NOT blanket-apply gates, see the lesson in
   `project_menu_cpu_loader_thread.md`), a wrong serializer slot, or an array stride/count mismatch.
3. Decisive tool: per-read stream-position diff **retail (DLL-hook mode) vs standalone** — log each
   `CompressedStream::Read` (pos+size) during the failing resource's deserialize in both builds; the
   read where the position first diverges pins the exact field.
4. Fix the one `*_Serialize`, rebuild, re-run.

First known target: **`UIDescription_Serialize`** (`code/Game/Component/uidescription.cpp`) drifting on
`Dialog\StandardDialog.rsc:entity` (a `StringTable` ExternalPtr). Note UIDescription is used by menu
dialogs that load FINE, so the drift is a field combination StandardDialog has that menu dialogs don't —
diff this resource specifically.

**Agent-team shape:** discovery (Accelerator B) is serial; the actual fixes are deep and mostly
sequential PER BUG, but DIFFERENT drifting serializers in DIFFERENT files are independent — fan out one
agent per distinct `*_Serialize` (worktree isolation), each doing the byte-diff for its function. Give
each agent: the resource path that drifts, the Ghidra decompile + disassembly of the original Serialize,
and the StatGoal writeup as the method. Adversarially verify each proposed fix (does the per-read
position now match retail end-to-end?) before merging — a wrong "fix" can introduce a DIFFERENT drift.

---

## Suggested Workflow skeleton (one phase per Workflow call; stay in the loop between them)
```
phase('enumerate-factories')  // main loop runs Accelerator A, parses the type list
phase('wire-factories')       // pipeline/parallel: 1 agent per type (data passed in, no concurrent Ghidra), worktree
phase('merge+verify')         // main loop: apply registry/bulk edits centrally, build, re-run A → expect 0
phase('enumerate-serialize')  // main loop runs Accelerator B, parses the drift list
phase('fix-serialize')        // parallel: 1 agent per drifting *_Serialize, worktree, byte-diff vs retail
phase('verify')               // main loop: build, full preload run → expect [GUL::Load] EXIT, HUD renders
```
Gate the fleet size on the enumerated counts. Adversarially verify serialize fixes (per-read position
match) — they're the ones that silently re-break. Merge `address_registry.cpp` + `factory_registrations_bulk.cpp`
centrally (the two cross-cutting files). Ghidra: pre-extract, don't drive it from parallel agents.

## Done when
`BANISHED_GAME_PRELOAD=1` run reaches `[GUL::Load] EXIT — world built` with no NullFactory and no Read
spin, and `cmd //c start` launch shows the in-game **toolbar/HUD rendered** in the bottom-right.
Then flip BANISHED_GAME_PRELOAD default ON in `standalone/gameupdateloop_stubs.cpp`.
