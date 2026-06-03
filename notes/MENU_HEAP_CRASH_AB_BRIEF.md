# Menu heap crash — A/B hunt brief (for the next agent)

> ## ✅ RESOLVED 2026-06-02 — read this first
> The hypothesis below (achievement/goal path-string buffer overflow) was **WRONG** — those path
> strings were just data in flight on the loading thread (a red herring; attribution was correctly
> flagged unreliable). The real cause was a **concurrent-allocation data race**: the engine allocator
> `System::MemoryAllocator::Allocate` (Runtime `0x1800c6e60`) takes **NO lock** (only `Free` takes the
> CS at `allocator+0x640`), and our standalone **AsyncLoader** ran `next->vtable->Load` (heavy
> External::Load alloc) on a **worker thread** while the **main thread** ran `ChunkedSceneLoader::Tick`
> (12-15K menu-scene props → heavy alloc). Two threads racing the unlocked free-list → clobbered link →
> wild-ptr AV. Smoking gun: rz_crashtrace_7 shows **two threads simultaneously faulting inside Allocate
> on the same wild ptr** (worker via FindOrLoad, main via WinMain).
> **FIX**: `standalone/async_loader.cpp` now runs the load **synchronously on the caller thread by
> default** (set `BANISHED_ASYNC_LOAD=1` for the old worker). ~85ms hitch, scene chunking still animates.
> Validated: baseline 3 crashes/14, sync 0 crashes/15+. Full writeup in memory `project_menu_heap_crash.md`.
> The original (now-disproven) hunt plan is preserved below for historical context.

## Goal
Pinpoint and fix the **intermittent menu/startup heap crash** in the standalone exe
(`reimpl-x64.exe`). It is **free-list poisoning** in the engine allocator caused by a
**path-string buffer overflow during achievement loading**. The diagnostic tooling below already
characterized it; your job is to localize the exact buggy write (A/B method) and fix it.

## What is already known (do NOT re-derive — see also memory `project_menu_heap_crash.md`)
- Allocator = `System::MemoryAllocator` (custom boundary-tag free-list over VirtualAlloc; NOT the
  Win heap). `Allocate`=Runtime `0x1800c6e60`, `MemFree`→`Free`=`0x1800c7ad0`. Block: 8-byte tag
  at `block+0`, user at `block+8`, `blockBytes=(tag&0x1ffffffffff)*4`. Free blocks chain per size
  bucket via a signed-41-bit RELATIVE offset at `block+8`; bucket heads at `allocator+0x28+bucket*8`.
- Crash = `Allocate` follows a **clobbered free-block link** → wild ptr → AV. Caught by the
  standalone `CrashHandler` (`SetUnhandledExceptionFilter`) → **exit code 0x3**; the heap walk it
  prints lands in **standalone_debug.log** (`[REDZONE-DUMP]` lines), the register/stack trace in
  **standalone_crash.log**.
- **Intermittent (~1/3 of runs) even with a fixed scene** → heap-layout/ASLR dependent, NOT scene
  dependent. Crash happens on a **worker thread** (standalone AsyncLoader) doing
  `FindOrLoad → External::Load`.
- The corrupting bytes decode to **path strings**: `C:\GOG Games\…`, and inline
  **`Game\Achievement\Educated.rsc:over4years`** (the "Educated" achievement, goal `over4years`).
  Achievements are Runtime-loaded but `FindOrLoad` **Goal externals that are OUR `REGISTER_TYPE`'d
  reimpls** (`code/Game/Goal/*.cpp`: StatGoal/TimeGoal/PopulationGoal/BuildingGoal +
  FinishTutorialGoal). Leading hypothesis: **our goal factory under-allocates, or a goal `Slot5`
  deserialize / Filename/String path-copy writes the absolute path past its heap buffer (or a
  use-after-free writes into a freed Filename block).**
- "Which block overflowed" attribution from the walker is UNRELIABLE (it's a wild/index/UAF write,
  not a clean contiguous overrun; suspect-block sizes varied 56/112/288B; one "IS OURS" hit was a
  STALE table entry — a freed MeshInstance block reused by the achievement loader).

## Diagnostic tooling (already committed on branch `diag/menu-heap-crash-tooling`)
`standalone/redzone_alloc.{cpp,h}` — interposes `fn_MemAlloc`/`fn_MemFreeStatic`; installed in
`main.cpp` after `EngineFuncsInit`. Env vars (all no-op unless `BANISHED_REDZONE` set):
- `BANISHED_REDZONE=1` — enable shim + live-alloc tracking table (for backtraces).
- `BANISHED_REDZONE_PAD=<n>` — tail canary bytes. **KEEP 0** (padding shifts layout → HIDES the bug).
- `BANISHED_REDZONE_SCAN=<n>` / `BANISHED_REDZONE_WATCH=<ms>` — in-process scanners; they PERTURB
  timing and rarely catch this. Prefer the crash-handler dump instead (below).
- `BANISHED_MENU_SCENE=0..9` — pin the menu scene (handy; not required).
- The **CrashHandler** calls `RedzoneAlloc_HeapDump` (shim-independent) → walks boundary tags AND
  free-list links, prints the corrupt block + the corrupting payload (with ASCII) + (if
  `BANISHED_REDZONE=1`) the preceding block's alloc backtrace. This is the zero-perturbation path.

### Repro loop (native, ~1/3 crash rate)
```powershell
$env:BANISHED_MENU_SCENE='0'   # optional; $env:BANISHED_REDZONE='1' for backtraces
for ($i=1;$i -le 20;$i++){
  $p=Start-Process "C:\GOG Games\Banished\reimpl-x64.exe" -WorkingDirectory "C:\GOG Games\Banished" -PassThru
  if($p.WaitForExit(22000)){ if($p.ExitCode -eq 3){ Copy-Item "C:\GOG Games\Banished\standalone_debug.log" "rz_crash_$i.log"; break } }
  else { Stop-Process $p.Id -Force }
  Get-Process reimpl-x64 -EA SilentlyContinue|%{cmd /c "wmic process where processid=$($_.Id) delete"} # zombie killer
}
```
Build/deploy: `mingw32-make standalone-debug` then copy to `C:\GOG Games\Banished\reimpl-x64.exe`
(can't use `sa-deploy` if a zombie holds the file — kill via `wmic process where processid=N delete`).
Symbolize OUR backtrace frames: `addr2line -f -C -e standalone/build/reimpl-x64.exe <0x140000000+offset>`
(reimpl ImageBase = 0x140000000). Runtime frames: Ghidra, addr = 0x180000000 + (raw − runtimeBase),
runtimeBase = (the `real alloc=` value in the `[REDZONE] installed` log line) − 0xc6e60.

## A/B plan (your task)
1. **Confirm the locus**: stub/skip achievement loading at startup and check the crash vanishes.
   - Find where achievements load (Runtime `AchievementManager` startup, triggered from
     Engine/Application startup that our `game_main.cpp` / `main.cpp` drives, OR a Runtime call we
     make). Skip it (guard the call / empty the achievement dir lookup) and run the loop ~20×.
   - Crash gone → confirmed achievement-load locus. Crash remains → widen (it may be another
     `FindOrLoad`-on-worker-thread path; re-check the `[REDZONE-DUMP]` payload strings).
2. **Bisect within the achievement/goal deserialize**:
   - Identify the `over4years` goal type (decode the achievement resource, or instrument our goal
     factories/`Slot5` with a log + a post-deserialize `RedzoneAlloc_HeapDump("after <Goal>")`).
   - For each goal type our reimpl handles: verify factory alloc size == real type size (Ghidra),
     and that `Slot5`/Load + any Filename/String path write stays within the allocation. The bug is
     a heap buffer that receives a path/String longer than allocated (or a UAF write to a freed
     Filename). The recurring corrupting value `0x00007ffX_d6a34dc0` (low-32 constant) may help ID it.
3. **Fix** the under-allocation / OOB write; re-run the loop ~30× with `BANISHED_REDZONE=1` → expect
   zero `[REDZONE-DUMP]` corruption and zero exit-0x3. Then strip the in-tree `RZ_CHECK` checkpoints
   if desired (they're guarded no-ops) and keep the tooling.

## Gotchas
- Zombie `reimpl-x64.exe` survives taskkill/Stop-Process → `wmic process where processid=N delete`.
- Don't trust the in-process scan/watchdog to catch it (timing perturbation). Use the crash-handler dump.
- Two threads can crash → CrashHandler/dump prints twice; ignore the duplicate.
- The live-table has STALE entries (Runtime-internal frees bypass our `ShimFree`) → a "IS OURS"
  backtrace can be the block's previous owner. Cross-check payload contents, not just the table.
