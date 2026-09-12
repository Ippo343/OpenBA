# Runtime port — direction (2026-09-12)

Written after a sanity check of six days of "wave" work. Facts first, then what changes.

## 1. What the audit found

**The goal has not changed:** `reimpl-x64.exe` stops loading `Runtime-x64.dll`. Everything
the exe reaches inside the DLL must exist in our tree. Nothing else counts.

**The progress metric was measuring the wrong thing.** `tools/runtime_census.py` reports
`ported / called`, where `called` is what our *source* names directly. Every faithful port
calls what the original called, so the denominator grows with each port and the board can
say 89.5% while the exe is nowhere near booting alone. The wave docs knew this ("a faithful
port grows `called`") and kept quoting the percentage anyway.

**The real number, computed once from the DLL's own call graph** (`tools/cutover_closure/`,
Ghidra, 2026-09-12): from our 1,472 roots the closure reaches **1,751 functions**;
**488 are unported**, of which 90 are C-runtime routines our own CRT already provides.
**398 engine functions, 126 KB of code, remain.** 272 of them are unnamed `FUN_` internals;
only 16 are over 1 KB. The Wave 29 close said the remaining body work was 14.

**The cutover boot was being used as the planner.** 33 attempts, each dying at the *next*
missing function. With 398 to go that is 398 boots. A boot is a gate; the closure is the plan.

**The process had become self-referential.** Since 2026-09-06: 1,217 commits; 340k lines of
docs against 194k of runtime source; 157 tools; ~330 distinct `BANISHED_RT_*` env flags;
598 memory files, mostly one-paragraph "lessons" about defects in the previous wave's
instruments. Wave 29's headline findings are all about instruments, not the game. Many
"ported" bodies are wrapped in oracle scaffolding several times longer than the port.

**What is sound and stays:**
- The registry seam (`RegisterRuntimeExport/Addr`, `ResolveExport` consults it first).
- Binary-compatible `runtime/types/` with `static_assert` offsets.
- The diff oracle (`BANISHED_RT_DIFF=1`) while the DLL is present. It has found real
  defects in every wave. Use it; do not build more of it.
- The mint (`standalone/rt_data_mint.cpp` + `tools/rt_mint_gen.py`): at cutover the DLL's
  exported vtables/typeinfo/singletons must exist somewhere, and a hand-built vtable whose
  unported slots abort *by name* is the right shape while our bodies are C functions. It is
  a generator; treat it as one. Regenerate, don't narrate.
- The cutover boot (`make cutover`) as the final gate for what the closure cannot see
  (virtual dispatch through non-census slots, stored callbacks).

## 2. What stops

1. **No more waves, lanes, integrators, kickoff/close/lane documents.** One person or agent
   works the worklist. Parallel agents are fine for *disjoint batches of functions*; they
   report a diff and a closure delta, not an essay.
2. **No new instruments.** No new `tools/*.py` screens, audits, censuses, controls of
   controls. If a question needs a tool, the answer is almost always the closure or `grep`.
3. **No new `BANISHED_RT_*` flags.** A port registers unconditionally. The only runtime-port
   variables that remain meaningful: `BANISHED_RT_DIFF` (oracle), `BANISHED_RT_NO_DLL`
   (cutover, now implies `RT_MINT` + `RT_ALLOC`), `BANISHED_RT_STRICT` (residue census).
   Existing per-port and per-probe flags are dead weight: remove them as the files are touched,
   never add to them.
4. **No status in `CLAUDE.md`.** Status lives in `STATUS.md` (one screen). `CLAUDE.md`
   carries stable facts and pointers.
5. **No "lessons" memory files.** A memory is a user preference or a binary fact. The 544
   existing lessons were archived on 2026-09-12 (`memory/archive/`), not loaded.
6. **The census percentage is retired** as a headline. `runtime_census.py` still feeds
   `build_roots.py`; nobody quotes its ratio.

## 3. The loop

```
1. tools/cutover_closure/build_roots.py            # census -> roots
2. Ghidra: tools/cutover_closure/CutoverClosure.java
3. tools/cutover_closure/report.py                 # -> docs/runtime_port/closure/WORKLIST.md
4. port a batch from WORKLIST.md (leaves first, smallest first within a namespace)
   - decompile, name it in Ghidra if it is FUN_, write the body in runtime/src/<ns>/
   - register unconditionally; static_assert the layout
   - if a DLL-present route reaches it cheaply, run once with BANISHED_RT_DIFF=1
5. every ~30 ports: make cutover  -> does the wall move past the batch? note it in STATUS.md
6. regenerate (1-3). The ENGINE_TO_PORT number must only go down.
```

Batch size is whatever fits a session; the per-function recipe in `PLAN.md` §6 still applies.
A body that the closure says is reachable is worth porting even if no route exercises it
today; that is the whole point of computing the closure.

**What "done" means:** `make cutover` boots to the in-game world and quits cleanly with
`Runtime-x64.dll` absent. Then the DLL-present machinery (oracle, RtOriginal, detours, mint
shadow, ~300 flags) is deleted, and the C-style `RtNs_Class_Method(self, …)` bodies can be
folded into real classes for readability, which is the project's stated end state.

## 4. Where the old material went

- `PLAN.md` — facts and recipe still valid; process sections superseded (banner at top).
- `WAVE*.md`, `cutover3x/`, `evidence/` — history. Not required reading.
- `tools/attic/` — 70 one-off lane/wave scripts. `tools/cutover_run.sh` (was
  `cutover_attempt32.sh`) is the gate, wrapped by `make cutover`.
- `~/.claude/projects/…/memory/archive/` — 544 wave-status and lesson memories with their
  old indexes.
