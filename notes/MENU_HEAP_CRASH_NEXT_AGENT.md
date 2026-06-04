# Menu heap crash — RESUME-HERE plan for the next agent (2026-06-04)

> Read this WITH the memory file `project_menu_heap_crash.md` (full history + reversals). This doc is the
> actionable plan. The crash is precisely characterized; the goal now is to pin the ONE wild-write instruction.

## TL;DR of where we are
- **Bug**: standalone `reimpl-x64.exe` menu crashes ~25-37% of runs with engine-allocator **free-list
  corruption** (Allocate writes a wild node @Runtime+0xC74C3). A second face is a `MaterialInstance::Setup`
  NULL-read during UI Draw — same root corruption hitting a different victim by heap layout.
- **The deserialize is CORRECT** (debugger-verified: camera transform = valid unit quaternion + sane world
  position; `arr30=15533` is the real 15K-prop count). Earlier "f1a0 misalignment" theories were MISREADS —
  ignore them. This is a genuine **wild OOB write**, not a serialize misalignment.
- **Precise signature**: a DETERMINISTIC **+0x100000000 (bit-32) flip** of a **bucket-12** free-list pointer.
  Catches: `0x23E_BB4D0140` vs valid `0x23D_BB4D0140`; `0x242_80000140` vs valid `0x241_80000140`. Always
  byte+4 of the 8-byte value +1 → ~4GB off-heap → AV. The free block's TAG stays intact → NOT a contiguous
  overrun; it's a **wild/indexed single-bit write** (physical-predecessor "overflower" is an unreliable hint).
- **Victim location**: the allocator's **bucket-12 head at `alloc+0x88`** (corrupt byte at `alloc+0x8C`). The
  `MemoryAllocator` struct lives on the **MAIN-THREAD STACK** (`g_pMemAllocSingleton ≈ RSP+0x1000`). So the
  corrupting write targets a STACK address → a wild computed-pointer write OR a 32-bit-truncation store.
- **Locus**: `BANISHED_NO_GEOM=1` (skips the menu Geometry deserialize 04d0+f1a0) → **0 crashes**; baseline
  ~37%. So the write is in OUR `code/Game/Game/geometry_serialize.cpp` (04d0/f1a0) + its helpers
  (serialize_struct60b*, serialize_arrays, serialize_externals*, serialize_heightmap_mesh,
  serialize_image_upload). RZ_CHECK/CheckLinks between geometry STEPS never trip → the write lands close to a
  `FindOrLoad`/External::Load, with no checkpoint between it and the crashing Allocate.

## Committed state (branch `diag/menu-heap-serializer-reimpl`, off main)
- `2e49454` — **IO::Serializer reimpl** (`engine/types/io_serializer.h`, `runtime/src/io/serializer.cpp`),
  env-gated `BANISHED_REIMPL_SERIALIZER` (DEFAULT-INERT: unset → tail-calls Runtime original). Plus the
  diagnostics: `RedzoneAlloc_SetDumpSink`+`CrashLogSink` (crash dump now survives to standalone_crash.log),
  `RedzoneAlloc_CheckLinks`, the `BANISHED_NO_MENU_GEOM` lever.
- `d3f9405` — dump enhancement: reconstruct the overflower when a **bucket head** is directly clobbered.
- All behaviorally inert in a normal (no-env) run. `git stash`/leave `.claude/settings.local.json` alone.
- Stream/Serializer layout (from the reimpl): Stream `+0x08` loading-flag(int,0=load) / `+0x10`
  encoding-factory; Serializer `[0]`=stream `[8]`=encoding; `GetVersion` reads the FILE version from the
  encoding (ctor `version` arg is IGNORED). Menu stream = **CompressedFileStream** (decompressed-pos counter
  at `+0x90`, CHUNK-granular — useless for fine measurement; fine offset is inside the BinaryReader encoding).

## DECISIVE NEXT STEP — conditional HW watchpoint on the allocator bucket-12 head
The victim field is runtime-findable (`g_pMemAllocSingleton`), so ASLR is NOT a blocker. Plan:
1. `mingw32-make standalone-debug`; launch under GDB via the **vscode-debugger MCP**, config
   **"Launch Standalone (debug)"** (menu-mode, auto-deploys). Set an early breakpoint (e.g.
   `standalone/main.cpp` after EngineFuncsInit, or `menudialogloopt_stubs.cpp` Stub_Load entry) so the
   allocator exists but the menu scene hasn't loaded yet.
2. Read the allocator + heap range:
   - `g_pMemAllocSingleton` → `ALLOC`. Bucket-12 head = `*(void**)(ALLOC+0x88)`.
   - Heap range: `RANGES=*(void**)(ALLOC+0)`, `HSTART=*(void**)RANGES`, `HEND=*(void**)(RANGES+8)`.
3. Set a **conditional HW data watchpoint** on `ALLOC+0x88` (8 bytes) that fires ONLY on a corrupt value
   (ignores the constant legit Allocate/Free link writes). The legit writes store an in-range free block or
   0; the corrupt write stores an out-of-range value. GDB syntax via the debug console (drive it with the
   debugger MCP `evaluate_expression` context="repl", prefix `-exec`):
   `-exec watch -location *(unsigned long long*)(ALLOC+0x88) if (*(unsigned long long*)(ALLOC+0x88) != 0 && (*(unsigned long long*)(ALLOC+0x88) < HSTART || *(unsigned long long*)(ALLOC+0x88) >= HEND))`
   (substitute the literal ALLOC/HSTART/HEND values). If the MCP can't set HW watchpoints, fall back to
   driving gdb.exe directly in a terminal with the same `watch … if …`.
   - NB the corruption is bit-32 (+0x100000000); a cheaper condition is `(value >> 40) != (HSTART >> 40)`.
4. `continue`. GDB stops at the EXACT instruction that writes the corrupt value → that's the wild write.
   Get the stack (expect one of our geometry_serialize / struct60b / arrays / externals helpers, OR a Runtime
   fn fed a bad pointer/index by us). Inspect the source register/index that produced the wild address.
5. If the watchpoint is too slow (full-speed HW watchpoints on Win64 should be fine) or the allocator moves,
   re-derive ALLOC at the breakpoint each run.

## Side-leads (cheaper, lower probability — try if the watchpoint stalls)
- **System::Filename size**: our typed-extptr serializers (`serialize_externals_typed.cpp`,
  `serialize_arrays.cpp`, `serialize_struct60b_serialize.cpp`) build Filenames in `uint8_t fnameBuf[8]` /
  `pathBuf[8]` / `tmpFn[8]`. Decompile `System::Filename::Filename` (Runtime) + `get_struct_layout` — if
  Filename is >8 bytes, those ctors stack-overflow. (Caveat: that hits the LOCAL frame, not the far allocator,
  so it doesn't cleanly match the signature — but worth ruling out.)
- **32-bit truncation**: grep our geometry helpers for any `*(uint32_t*)` / `int` store into a pointer-sized
  slot, or a capacity/offset computed in 32-bit that could set bit 32 of an adjacent 64-bit field.
- **The bucket-12 size class**: decompile the allocator's bucket-size mapping (GetFreeTagForSize near Runtime
  0x1800c6e60) to learn what byte-size bucket 12 holds (~296B seen once), then look for that exact-size alloc
  in the geometry path.

## How to repro / test (no debugger)
- Build+deploy: `mingw32-make standalone-debug` → copy `standalone/build/reimpl-x64.exe` to
  `C:\GOG Games\Banished\reimpl-x64.exe`. (Sandbox blocks `Remove-Item` under `C:\GOG`; use
  `dangerouslyDisableSandbox` for the PowerShell run-loop; zombie-kill via `wmic process where processid=N delete`.)
- Run menu-mode (NO `BANISHED_AUTOSTART`): IDLE-timeout (~20s) = menu loaded OK; **exit 0x3 = crash**.
  `BANISHED_REDZONE=1` enables the survivable crash dump (`standalone_crash.log`, WRITE-THROUGH) with the
  free-list-link pass + overflower reconstruction. A clean A/B baseline: `BANISHED_NO_GEOM=1` → 0 crashes.
- **Test CORRECTLY**: a hang also looks like idle-timeout — confirm the menu reaches `[MenuDLT::Draw]` in
  `standalone_debug.log`, don't just trust "no crash in N s". (Disabling the geometry deserialize HANGS the
  menu — camera deserializes to zero — so it is NOT a viable fix; it must run.)

## When fixed
Re-run the menu loop ~30× (BANISHED_REDZONE=1) → expect 0 exit-0x3. Strip the in-tree RZ_CHECK / sub_c780 +
modelcomponent diagnostic printfs if desired (guarded no-ops). Merge the branch. Then the menu is stable.
