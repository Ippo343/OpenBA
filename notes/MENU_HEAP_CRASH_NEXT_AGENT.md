# Menu heap crash — RESUME-HERE plan for the next agent (2026-06-04)

> Read this WITH the memory file `project_menu_heap_crash.md` (full history + reversals). This doc is the
> actionable plan.

## ★★★★ 2026-06-15 SESSION 4 — UAF SOURCE CAUGHT: GetDataDirectory AddRefs a FREED fs+0x08 data-dir String ★★★★
Resume here. Session 3 proved the UAF is Runtime-internal but couldn't see it (its `filename_dtor_check.cpp`
wrapped the GLOBAL fn-pointers = only OUR calls). Session 4 **caught the exact instruction + victim**.

**The instrument (NEW, kept): `standalone/filename_runtime_hook.cpp`, env `BANISHED_FNHOOK=1` (default-inert).**
Detours the ACTUAL Runtime Filename refcount functions (byte-patch + trampoline, same mechanic as
allocator_lock.cpp) so EVERY caller — incl. Runtime-internal — routes through a refcount==0 (freed) check
BEFORE the dec/inc, logging a deep symbolized stack. Detoured:
  - `~Filename`            0x180006980  (dec)
  - `operator=(String&)`   0x180006770  (dec dst-old + inc src)
  - `operator=(Filename&)` 0x180007d40  (dec dst-old + inc src)
  - `Filename(const String&)` ctor 0x1800067d0  (inc src AddRef)   ← THE CULPRIT
Install in main.cpp after InstallFreeLock (InstallFilenameRuntimeHook). Test harness: `notes/fnhook_capture.sh`
(BANISHED_NO_FS_RELATIVE=1 BANISHED_FNHOOK=1; preserves standalone_error.log per crash to notes/wp_runs/).

**KEY RESULT — dtor + op=(String) fired 0× across 6 crashes (incl. a confirmed bucket-12 link crash); adding
the from-String ctor + op=(Filename) → FIRED IMMEDIATELY + DETERMINISTICALLY.** The corrupting op is:
  **`Filename(const String&)` ctor doing `inc [String−4]` on a String with refcount==0 (a FREED bucket-12 block).**

**THE VICTIM (pinned via Ghidra):** the immediate caller is `IO::FileSystem::GetDataDirectory`
(Runtime 0x180007bd0; the `inc` site is +0x2a = 0x180007bfa). Its entire body is
`Filename::Filename(retval, this+8)` — i.e. it AddRef-copies the FileSystem's **data-directory Filename at
fs+0x08** (fs = `IO::FileSystem::_singleton` @ Runtime **0x1801fc6a8**). **That fs+0x08 String's refcount is 0
= it has been FREED** while the FileSystem still holds it. Every later `GetDataDirectory` (called by FindOrLoad
path resolution) AddRefs the dead String → writes 1 into the freed block's free-list-link high dword
(block+0xC = String−4) → the deterministic **+0x100000000 bit-32 flip** → wild Allocate later = the crash.

**Call chain (run 1/4/5, identical every time):** menu load `splash_loop_stubs → async_loader →
menudialogloopt_stubs` → Runtime `External::Load` (0x1800db140, frame +0x4d) → FindOrLoad → 0xbd831
(ExpandRelativePath area) → **GetDataDirectory 0x7bfa → Filename(String&) inc on freed fs+0x08 String.**
The freed block currently holds dialog-resource path bytes (`Dialog\StringTable.rsc`, `Dialog\SharedElements.rsc`,
`C:\GOG Games\Banished\Wi[nData]`) — incidental reuse content; the victim IS the data-dir String.

**Both crash faces confirmed at full speed + UNIFIED:** the bucket-12 link-corruption crash (Runtime+0xC74C3
WRITE) AND the `MaterialInstance::BuildParameters` null-texture READ (Runtime+0x103933) both occur; the UAF
report correlates with the link face (run 4 even SURVIVED with the UAF present = Heisenbug: the bit-32 flip
sometimes lands on a fatal free-list path, sometimes benign/missing-texture). NO_GEOM→0 still holds.

**NOT `FileSystem_MakePathsRelative`** (filesystem_setup.cpp): the harness runs `BANISHED_NO_FS_RELATIVE=1` and
run-1's path was still absolute `C:\GOG Games\…` → relativization did NOT fire. So a **Runtime-internal
over-release** drives fs+0x08's String to 0 during geometry's ~230 FindOrLoad path resolutions (each calls
GetDataDirectory → AddRef+release; one path nets an extra release, OR fs+0x08's baseline refcount is set too
low for standalone).

### The over-release site + the function (session 4, found)
- **`BANISHED_FNHOOK_DATADIR=1`** (added) watches the fs+0x08 String: logs RELEASE(dec) ops at refcount≤2 +
  any op at ≤0, AND **detours the real Runtime `MemFreeStatic` (0x1800c7ad0)** to log when the data-dir
  String's BLOCK is actually freed (`[FNHook] *** DATADIR-FREE ***`), with the caller stack.
- **The freeing MemFree is inside `Object::ExternalFactory::ExpandRelativePath` (Runtime 0x1800bd360)** —
  caught at +0x446 (0xbd7a6), called from FindOrLoad (0xbf35e). ExpandRelativePath calls `GetDataDirectory`
  (AddRef) and chains `System::operator+` to build the full resource path, with a long inlined refcount
  cleanup. The freeing dec there is just where the eroded refcount finally crosses 0 — the **EXTRA releases
  accumulate over the ~230 prior ExpandRelativePath/FindOrLoad calls during geometry**; this is RETAIL Runtime
  code (so retail doesn't erode — a STANDALONE-specific condition makes the data-dir String's baseline erode).
  The freed-then-AddRef'd path is literally `C:\GOG Games\Banished\WinData\…` = the data-dir String itself
  (not incidental reuse).

### FIX-TEST RESULT (session 4): pinning the data-dir String is NECESSARY-BUT-INSUFFICIENT
`code/Game/Game/filesystem_setup.cpp` `FileSystem_PinDataDir()` (env **`BANISHED_PIN_DATADIR=1`**, default OFF;
wired in game_main.cpp RunGameMain) bumps fs+0x00/+0x08/+0x10 String refcounts by 0x100000 after Engine::Startup.
A/B (BANISHED_NO_FS_RELATIVE=1, process-death detector):
- **PIN ON: 4 crashes / 24 (~17%) — UNCHANGED from the ~20% baseline.** So the pin is NOT a sufficient fix.
- **BUT with `PIN + FNHOOK`, alive runs show UAF=0** (vs UAF=2 on no-pin alive runs) — the pin DOES protect the
  data-dir String; its specific AddRef-on-freed no longer fires. The pin applied correctly (PinDataDir-msgs=3).
- **⇒ CONCLUSION: the over-release in `ExpandRelativePath` is GENERAL** — it erodes path-String refcounts
  broadly, not just the data-dir String. The data-dir String was merely the most-reused / most-visible victim
  (every FindOrLoad touches it via GetDataDirectory). Pinning ONE String just moves the fatal flip to another
  eroded path-String (and/or the `MaterialInstance::BuildParameters` null-texture face Runtime+0x103933 = a
  resolved-pointer that the same factory-state bug nulls). NO single-String pin will fix it.

### DECISIVE NEXT STEP (session 5) — fix the GENERAL over-release in the path/factory load
The bug is a Runtime-internal over-release of resource-path Strings during `ExternalFactory::ExpandRelativePath`
/ `FindOrLoad` (0x1800bd360 / FindOrLoad), exercised ~230× by geometry. It is RETAIL code, so a STANDALONE
condition makes it erode. Hunt that condition:
- **Generalize the FNHOOK watch to ALL path Strings, not just fs+0x08.** In filename_runtime_hook.cpp, drop the
  `str==CurrentDataDirString()` filter and instead log any RELEASE that takes a String to 0 (free) whose stack
  is inside ExpandRelativePath/operator+ — find the dec with NO matching AddRef. The full ExpandRelativePath
  decompile (this session's transcript) has many inlined `*(int*)(X-4) -= 1; if 0 MemFree` blocks + several
  `System::operator+` chains and a GetDataDirectory called TWICE (top + packfile-fallback). Suspect: a
  `System::operator+` overload whose ref contract we feed wrong, or a temp double-freed on one branch.
- **DLL-mode A/B (the cleanest discriminator):** run version.dll mode (original exe geometry code + same
  Runtime) with FNHOOK installed; if the data-dir UAF does NOT fire there, the trigger is a standalone-only
  difference our geometry FindOrLoads introduce (encoding/ref-mode state, or the 235/358 factory-registration
  gap producing a half-constructed object). If it DOES fire in DLL mode too, it's a genuine our-reimpl bug in
  the geometry serialize feeding ExpandRelativePath a bad path.
- **Or the targeted instrument:** detour `System::operator+` (the String concat the path build uses) + the
  GetDataDirectory return-temp lifetime, and diff standalone vs retail refcount deltas for ONE FindOrLoad.
- KEEP `FileSystem_PinDataDir` (env-gated, off) as a diagnostic lever — it cleanly proves "data-dir face vs
  other face" in future A/Bs. Build: `mingw32-make standalone-debug` (cd repo root first). Deploy to
  `C:/GOG Games/Banished/reimpl-x64.exe`. Reap: `wmic process where "name='reimpl-x64.exe'" delete`.

---

## ★★★ 2026-06-15 SESSION 3 — the UAF is RUNTIME-INTERNAL, not in our explicit Filename calls (major narrowing) ★★★
Resume here. Big results this session (several dead theories KILLED, mechanism decoded exactly):

1. **All-strides zeroing fix applied + MEASURED — NOT the residual fix.** Generalized the stride-8
   in-place-grow zeroing to the shared `GenericArrayResize` (both copies: serialize_arrays.cpp +
   serialize_resize_helpers.cpp now zero new slots UNCONDITIONALLY on both realloc AND in-place-grow,
   matching the original exe). Faithful + correct (KEEP), but **full-speed crash rate UNCHANGED at ~20%**
   (4/20). So the residual face is NOT the dirty-in-place-grow class.

2. **NEW reliable full-speed test harness** (`notes/fullspeed_detect.sh`, `notes/fullspeed_capture.sh`):
   detect crashes by PROCESS DEATH (tasklist), NO redzone/gdb (true full speed), `BANISHED_NO_FS_RELATIVE=1`.
   stdout/standalone_debug.log is LOST on hard-kill — **stderr/standalone_error.log is the reliable channel**
   (FrameLimiter prints there; key on it, NOT the stdout "MenuDLT::Draw"). On a fatal crash the CrashHandler
   writes a full dump (regs + module+off + native heap-walk + stack) to standalone_crash.log.

3. **3 full-speed crash dumps captured** (`notes/wp_runs/fs_crash_{4,5,11}.txt`): 2/3 = the bucket-12
   free-list bit-32 face is STILL happening (Runtime+0xC74C3/4EA Allocate, corrupt fwd link
   `0xffffffff00000000` = block+0xC=refcount=−1); the clobbered block holds a resource-path String
   ("Font\FontMedium…resource"). 1/3 = the MaterialInstance NULL-read (Runtime+0x102C9F, RCX=0). Same two
   faces as before — the stride-8 fix did NOT clear them at full speed.

4. **Filename-size theory FALSIFIED.** Disassembled the Runtime ctors (objdump on Runtime-x64.dll): both
   `Filename::Filename()` (0x180006220) and `Filename(char*)` (0x180026cf0) do `movq $0,(rcx)` = **8 bytes**.
   System::Filename = a single 8-byte System::String*. The `fnameBuf[8]` buffers are CORRECTLY sized.
   STOP chasing the stack-overflow side-lead.

5. **Exact refcount idiom decoded** (objdump; addresses via Python GetProcAddress, the engine_funcs.cpp
   numeric fallbacks are STALE — ResolveExport is what actually resolves): String ptr → char data;
   **refcount at string−4**; alloc base string−8. `~Filename`(0x180006980)=`decl [string−4]`+free-if-0;
   `operator=`(0x180006770)=release-old + `incl [src−4]` AddRef-new; `GetPath`(0x180102970→real
   0x180006250) returns an OWNED (AddRef'd) copy; `GetPathForReferenceId`(0x1800bf1d0) returns OWNED
   (AddRef'd). **The corrupting instruction is a `decl`/`incl [string−4]` on an ALREADY-FREED String**
   (block+4 = refcount slot = the free-list-link high dword → ±1 = the bit-32 flip).

6. **ALL our External serialize helpers VERIFIED FAITHFUL** (line-diffed vs Ghidra decompile this session):
   `FUN_140007140` (7140, called 227×/load) and `FUN_140020150` (the +0x210 MaterialInstance array, the one
   session 2d left un-diffed) — both match our reimpls exactly; every Ctor/GetPath/AssignOp paired with a
   Dtor; Struct60B_Resize zeroes new elements; refcounts balanced. NOT the imbalance.

7. **Geometry CONFIRMED as the crash source at full speed** (reliable detector): `BANISHED_NO_MENU_GEOM=1`
   → **0 process-death crashes / 15** (just hangs). vs ~20-36% baseline. (The old "NO_GEOM→0" was measured
   with the unreliable crash-log detector; now confirmed by process death.)

8. **★ DECISIVE NEW FINDING: the corrupting refcount op is NOT at our explicit Filename calls.** Added an
   env-gated interposer `standalone/filename_dtor_check.cpp` (`BANISHED_FNDTOR_CHECK=1`, default-inert) that
   wraps the global `fn_Filename_Dtor` AND `fn_Filename_AssignOp`: before delegating, it reads the operand
   String's refcount and logs the caller if refcount ≤ 0 (i.e. a UAF release/AddRef at OUR call site).
   **Installed correctly (real dtor=0x…6980) but fired 0× across 8 full-speed crashes.** ⇒ The over-release/
   AddRef-on-freed happens INSIDE Runtime code (the real ~Filename/operator= called internally by Runtime,
   or a Runtime String op), operating on state OUR geometry deserialize sets up — NOT in our serialize
   helpers. This kills the entire "our helper over-releases a Filename" family (6 sessions' focus).

### WHERE THE BUG IS NOW (session-3 conclusion)
A **Runtime-internal** String refcount op corrupts a freed bucket-12 path-String, triggered by what our
GEOMETRY deserialize feeds the Runtime (NO_GEOM=clean; our explicit Filename ops are clean). Geometry's
Runtime touchpoints = ~230 `ExternalFactory::FindOrLoad(MaterialInstance)` calls (+0x150/+0x158, the +0x210
array, and 227× Struct60B +0x00) + GetReferenceId/GetPathForReferenceId on the factory ref-table. Leading
hypothesis (= session 2's, now the ONLY surviving one): our geometry load leaves the **Runtime ExternalFactory
ref-id table / resource cache** holding an over-/under-referenced path String, which a later Runtime op
(another FindOrLoad, the factory's own bookkeeping, or MaterialInstance::Serialize loading a SpriteSheet/
texture) releases-or-AddRefs after free.

### DECISIVE NEXT STEP (session 4) — catch the Runtime-internal source write
Our-call-site interposition is exhausted (clean). Need the EXACT Runtime instruction:
- **(A) Dynamic HW watchpoint, self-armed.** When the engine Free (0x1800c6c00 / fn_MemFreeStatic) frees a
  bucket-12-sized block whose body looks like a String (small header + ASCII chars), program DR0 (via
  GetThreadContext/SetThreadContext on all threads, or AddVectoredExceptionHandler + single-step) to watch
  block+4 (the refcount slot) for WRITE; on fire, log RIP + stack. ASLR-robust (address derived at runtime).
  This pins the exact Runtime `decl/incl` and its caller. Build on standalone/redzone_alloc.cpp (it already
  hooks free + walks the heap).
- **(B) Poison-on-free + scan.** Hook Free to write a sentinel (e.g. 0x7F7F7F7F) into freed bucket-12 blocks'
  +4; at geometry checkpoints scan freed blocks for +4 != sentinel ⇒ caught the UAF window (gives "when/which
  block", not the instruction — pair with A for the instruction).
- **(C) Wrap the factory ops.** Interpose `fn_ExternalFactory_FindOrLoad` / `GetReferenceId` /
  `GetPathForReferenceId` (globals our code calls) to dump, per call during geometry, the returned External's
  internal path-String pointer + its refcount, and watch for a refcount that goes 1→0→(used). Cheaper than A,
  tests the ref-table hypothesis directly.
- **(D) DLL-hook-mode A/B** (version.dll, original exe geometry code runs, same Runtime): if it does NOT
  corrupt, the trigger is a standalone-only difference (incomplete type/factory registration: 235 registered
  /358 stubs — a MaterialInstance/SpriteSheet load may resolve differently → a half-constructed object with
  off refcounts). Confirms whether to chase factory-registration completeness vs a genuine Runtime bug.

### Build/test (session-3 harness)
- `mingw32-make standalone-debug` → cp standalone/build/reimpl-x64.exe to "C:/GOG Games/Banished/".
- Full-speed crash rate: `RUNS=20 bash notes/fullspeed_detect.sh` (CRASH=process death; key on stderr
  FrameLimiter for "alive", NOT stdout).
- Capture dumps: `bash notes/fullspeed_capture.sh` (preserves standalone_crash.log per crash).
- UAF-at-our-sites check (now proven clean): `BANISHED_FNDTOR_CHECK=1` (logs to standalone_error.log).
- Reap zombies with `wmic process where "name='reimpl-x64.exe'" delete`.
- Kept this session (all default-inert/correct): the all-strides zeroing fix; filename_dtor_check.cpp;
  the two .sh harnesses; the 3 fs_crash dumps.

---

## ⚠️ 2026-06-04 SESSION 2g — CHECKPOINT: the fix is REAL but the FULL-SPEED menu is STILL broken (residual corruption) ⚠️
HONEST status after validating at FULL SPEED (not just GDB):
- **The stride-8 fix (6ae0019) is real** (GDB watchpoint A/B proves it eliminates the bucket-head free-list
  corruption). But it is **NOT sufficient** — at FULL SPEED the menu still crashes/hangs at `MainLoop::Run`,
  never reaching the screen. There is **≥1 residual corruption source** (one captured full-speed crash showed
  the free-list links INTACT → a DIFFERENT face than the bucket-head one I fixed).
- **NOT a regression I introduced**: the session-start commit `a619eac` (before ANY of my changes) ALSO
  crashes/hangs identically at full speed (2/3 runs died with `[REDZONE-DUMP] free-list ...`). The full-speed
  menu was already broken; my GDB-only validation masked it. The memory's "~75% clean menu" is likely a STALE
  older state (predates the a34e1f6/2e49454/d3f9405 prior-session commits — one of THOSE may have regressed
  full-speed; worth bisecting `a34e1f6..a619eac` at full speed).
- **KEY METHODOLOGY GAP for next session: GDB CHANGES THE BUG.** Under GDB (and BANISHED_REDZONE) the menu
  progresses far (that's how the watchpoint caught it + I validated the fix); at full speed it fails.
  **Validate fixes at FULL SPEED**, not just under the debugger.
- **FS-relative (a682b15, default-on) converts the full-speed CRASH into a 95%-CPU HANG** at MainLoop::Run.
  Net-negative for usability + a debugging confound. For full-speed crash-hunting, run
  `BANISHED_NO_FS_RELATIVE=1` (to see the actual crash, not the hang) — or revert a682b15. (Not reverted this
  session, per user.)
- **Detection gotcha (cost real time):** a 0-byte `standalone_crash.log` is a NON-FATAL VEH first-chance log,
  NOT process death. Detect crashes by **process death** (`tasklist` alive=0), NOT crash.log existence. The
  full-speed menu currently NEVER reaches `[MenuDLT::Draw]` — it hangs/crashes at `MainLoop::Run` right after
  the game_main menu-scene PRE-LOAD (MenuScene::Factory/Slot5 logs appear, then `Calling MainLoop::Run` is the
  last line).

### NEXT SESSION — hunt the residual full-speed corruption
1. FULL-SPEED crash detector: loop launch (NO gdb), `BANISHED_NO_FS_RELATIVE=1`, sleep ~15s, check `tasklist`
   alive=0 (crash) vs alive=1 (hang/menu). `BANISHED_REDZONE=1` still gives the survivable `[REDZONE-DUMP]`
   heap dump on a full-speed crash — use it to characterize the residual face (free-list bucket/link vs a
   live-object null like MaterialInstance).
2. Bisect: does the stride-8 fix REDUCE the full-speed crash rate vs a619eac (several runs each)?
3. For the RESIDUAL face (links intact at crash), watch a different victim, OR run the redzone heap WALKER at
   full speed (`BANISHED_REDZONE_SCAN=<N>` / `BANISHED_REDZONE_WATCH=<ms>`) to catch live-block corruption, OR
   re-audit the OTHER stride arrays (20/14) + the Struct60B +0x00 ExternalPtr raw-copy on in-place grow.
4. Defensive complete fix to try: make `GenericArrayResize` zero new slots UNCONDITIONALLY for ALL strides
   (allocator returns DIRTY memory; harmless for data arrays) — verify at FULL SPEED.
5. Also possible separate issue: the menu-CPU loader spin (95% CPU) — [[project_menu_cpu_loader_thread]].

## ✅ 2026-06-04 SESSION 2f — stride-8 fix (committed 6ae0019): REAL fix for the bucket-head face (GDB-validated; NOT sufficient at full speed — see 2g)
`Reimpl_Array_Resize_Stride08` used the shared `GenericArrayResize`, which only zero-fills new slots on the
REALLOC path — leaving them DIRTY on **in-place grow** (capacity already sufficient). The original
`FUN_140001bb0` zeros new slots UNCONDITIONALLY (both paths); the engine's boundary-tag allocator returns
DIRTY memory, so this is load-bearing. For the **stride-8 External* pointer arrays** (Geometry +0x210
MaterialInstance, MenuScene +0x40 GraphicsMesh / +0x60 Particle exrefs), a dirty new slot keeps a **stale
freed-External pointer**; the serialize then runs `External::GetPath`/a Filename refcount op on the
already-freed `System::String`, doing `*(int*)(strPtr-4) += 1` which lands on the freed block's free-list link
high dword (`block+4`) → the deterministic `+0x100000000` bit-32 flip → wild `UnlinkFreeTag`/`Allocate`.
FIX = a dedicated stride-8 resize body matching `FUN_140001bb0` byte-for-byte (zero new slots unconditionally).

**VALIDATION (HW watchpoint on the bucket-12 head, redzone build, GDB; FS-relative isolated off both arms):**
- baseline (no fix): **1 watchpoint catch + 9 SIGSEGV / 29 runs** (~34% corruption events)
- fixed:            **0 catches + 0 SIGSEGV / 40 runs**  (P(0/40 | 34%) ≈ 1e-7)
This is THE fix. It eliminates the bucket-head free-list corruption that unifies both crash faces (and the
"missing menu images" non-fatal face). NOTE: at full speed the fatal crash was always RARE (the corruption
mostly manifests non-fatally as missing textures); GDB's slowdown amplifies it to a SIGSEGV/catch, which is why
the watchpoint loop is the sensitive detector. (Earlier "100% crash" measurements were a FALSE POSITIVE — the
0-byte `standalone_crash.log` is a non-fatal VEH first-chance log, NOT process death; verify crashes by process
death, not crash.log existence.)

**REMAINING TO CONFIRM:** whether the "missing images/buttons" (non-fatal texture-null face) is FULLY gone with
the fix or a residual second source remains. Same-root prediction = gone (the fix removes the bad-block hand-out
that nulls MaterialInstance textures). Needs a visual check of the deployed fix build. If residual: re-run the
session 2d audit on the OTHER stride arrays / Struct60B for the same un-zeroed-in-place-grow class.

## ★★★ 2026-06-04 SESSION 2c — WATCHPOINT FIRED: bit-32 free-list-link corruption CONFIRMED + caught ★★★
The `alloc+0x88` watchpoint DOES fire — but only under the **redzone build** (`BANISHED_REDZONE=1`), which
surfaces the bucket-head face (the no-redzone grind kept hitting the MaterialInstance face instead). Driver
`notes/wp_driver.sh` (now redzone + `wmic`-reap) caught it on run 40 (`notes/wp_runs/run_40.txt`):
- Watchpoint on bucket-12 head: **Old value `0x3390140` → New value `0x103390140`** = `0x3390140 + 0x100000000`
  (the deterministic **bit-32 flip**, confirmed).
- Writing instruction: `System::MemoryAllocator::UnlinkFreeTag+0x174` `mov %rdx,0x28(%r8,%rcx,8)`
  (r8=allocator `0x5ff700`, rcx=bucket **12**, rdx=the value). **rdx arrived ALREADY corrupt** = `0x103390140`,
  while r11/r14 hold the CORRECT `0x3390140` and rdi=`0x1ffffffffff` (the free-list relative-offset mask). So
  `UnlinkFreeTag` merely **propagated** an already-corrupt link from the free tag at `0x3390140` into the
  bucket head; the head write is NOT the source.
- Backtrace = pure Runtime **MainMenu dialog load**: `UnlinkFreeTag ← Allocate ← AllocateLocked(ours) ←
  String::SetLength ← System::operator+ ← FindOrLoad ← Dialog::operator= ← ButtonDescription::Serialize ←
  External::Load ← ... (Ribbon/GroupDescription::Serialize)`. Our code is NOT on this stack (only the
  AllocateLocked lock-wrapper) → the corruption was planted EARLIER (geometry deserialize) into a then-live
  object whose field later became this free block's link.
- **This UNIFIES both faces**: one root = a bit-32-corrupted free-list link planted by our geometry code →
  later `Allocate` either crashes following the wild link (bucket-head face, Runtime+0xC74C3 / UnlinkFreeTag)
  OR returns a bad/overlapping block that becomes a MaterialInstance with a NULL texture ptr (BuildParameters
  face). NO_GEOM avoids both.
- The corrupt value's low 32 bits == the block's own low address; the flip is +0x100000000 = setting bit 0 of
  the link's HIGH dword (`block_link+4`). This matches a `*(int32*)(stalePtr-4) += 1` **refcount increment
  through a stale Filename/String pointer** that lands on a free/again-to-be-free block's link high-dword
  (the static-audit agent's #1 hypothesis). Caveat to reconcile: zeroing OUR heap allocs did NOT fix it — but
  the Filename temporaries are 8-byte STACK buffers (`uint8_t fnameBuf[8]` etc.), not heap, so zeroing our
  heap allocs wouldn't neutralize a stale stack-Filename refcount op.

### ★★★ ROOT MECHANISM DECODED (session 2c, 2nd catch run_5_SAVED.txt) — Filename/String USE-AFTER-FREE ★★★
Enhanced catch dumped the corrupt free block + full `UnlinkFreeTag` disasm. CONCLUSIVE:
- Free-list links are **relative offsets**: `UnlinkFreeTag` does `rdx=*(tag)`, decodes `next = tag + (rdx &
  0x1ffffffffff)` (mask in rdi), stores it as the bucket head, then `and %rdi,(%rdx)` derefs `next` → AV if wild.
- The corrupt link qword at `tag+0` = **`0x0000000100000000`**: low dword 0 (the real/legit offset), **high
  dword (tag+4) = 1**. That makes the offset `0x100000000` → `next = tag + 4GB = 0x103340140` (wild).
- **`tag+4` is exactly where the block's live `System::String` kept its REFCOUNT.** The block still contains the
  freed path strings (`Dialog\…rsc:resource`, `Game\Terrain\TerrainList.rsc:resource`, `C:\GOG Games\…`) at
  `tag+8` onward, with refcount slot at tag+4.
- So: a `System::String` (a resource-path Filename) was **freed** (block linked into bucket-12, high dword
  cleared to 0), and then **our geometry code did a refcount op on the already-freed String**, writing `1` back
  to `tag+4` (use-after-free) → corrupts the free-list link's high dword → bit-32 → wild allocator write later.
- This is a **Filename/String refcount IMBALANCE / use-after-free** in our geometry serialize helpers
  (serialize_externals.cpp / serialize_externals_typed.cpp / serialize_arrays.cpp /
  serialize_struct60b_serialize.cpp) — a Filename/String whose backing String we over-release (Dtor one too
  many / fail to incr) so it's freed while a stack `fnameBuf`/`pathBuf` still points at it, then a later
  `fn_Filename_Dtor`/`AssignOp`/copy increments-or-touches the dead refcount. (Explains why heap-zeroing didn't
  help: the imbalance is in refcount bookkeeping, not an uninitialized buffer.)

### ★ Session 2e — ABSOLUTE-PATH hypothesis FALSIFIED (clean A/B) ★
An agent decompiled the whole FileSystem chain + implemented path relativization and A/B-tested it:
- **Premise was wrong**: standalone DOES run the FileSystem setup. `Engine::Startup` (Runtime 0x1800db8a0,
  called from game_main.cpp) → `FileSystem::Startup` (0x1800abc50) → `SetupDirectories` (0x1800abd00) mounts
  base `WinData/*.pkg` and sets the 3 dir Filenames (fs `0x1801fc6a8`: +0x00 source, +0x08 data, +0x10 pack).
  `Modification::SetupFileSystem` (0x1800aebb0) IS invoked via `Engine::Loader::Load` (0x1800dc050); with 0
  mods it's a near no-op. Absolute paths come from `GetRootDirectory` (0x1800ad650) = exe dir — SAME as the
  real game. `FindOrLoad` dedup (via `ExpandRelativePath` 0x1800bd360) works regardless.
- **A/B (same binary, BANISHED_REDZONE=1)**: abs paths 12/12 crash, relative paths 12/12 crash, baseline
  10/10. **Path length/content has ZERO effect.** => the corrupt block merely HAPPENING to hold path strings
  is incidental (bucket-12 = ~30-byte allocs = often small Strings). The `+0x100000000` write to a freed
  block's `+4` is still the mechanism, but it is a **genuine OOB write in the geometry deserialize that does
  NOT depend on resource-path content**. Drop the path-length lead entirely.
- LEFT IN PLACE (uncommitted, correct + harmless, default-on, opt-out `BANISHED_NO_FS_RELATIVE=1`):
  `code/Game/Game/filesystem_setup.cpp` (`Game::FileSystem_MakePathsRelative`) + game_main.cpp wiring. It
  genuinely shortens resource paths and MAY help the menu-CPU loader churn ([[project_menu_cpu_loader_thread]])
  — but it is NOT the crash fix. Decide whether to keep or make inert.

### Session 2d — refcount AUDIT progress (the obvious helpers are NOT the bug)
Diffed our reimpl against the original exe Filename/refcount sequence:
- **`FUN_14001b8e0`** (single ExternalPtr) and **`FUN_14001bcc0`** (External array) — our
  `serialize_externals.cpp` / `serialize_arrays.cpp` match them EXACTLY: every `Filename::Filename` /
  `operator=` / `~Filename` / `String::Serialize` / `GetPath` / `GetPathForReferenceId` is balanced and
  placed identically (per-iteration Ctor/Dtor of the working `fnameBuf`; GetPath/GetPathForRefId return a
  by-value owned `Filename` into a temp that we then `~`). NOT the bug.
- **`fn_Filename_AssignOp` resolves correctly** to `??4Filename@System@@QEAAAEAV01@AEBV01@@Z` (the real
  refcounting `operator=`): its exe dispatch slot `0x1401529e0` is NOT registered in standalone, so it falls
  back to the correct `ResolveExport`. (If it were a non-refcounting copy, `op=(fnameBuf,pathBuf)` wouldn't
  incr → `~pathBuf` frees the shared String → `fnameBuf` dangles → UAF. Ruled out.)
- So the imbalance is NOT in the two primary External helpers. REMAINING SUSPECTS (narrowed): (1) the
  Struct60B per-element `FUN_14001ffa0` + typed MaterialInstance array `FUN_140020150` (`serialize_struct60b_
  serialize.cpp`) — not yet line-diffed; (2) `fn_Filename_Ctor`/`fn_Filename_Dtor`/`g_fn_String_Serialize`
  resolutions (verify the exe dispatch slots they try FIRST aren't registered to a wrong fn); (3) a
  **standalone-specific absolute-path interaction**: standalone disables `Modification::SetupFileSystem` so
  resources resolve to long `C:\GOG Games\Banished\…` ABSOLUTE paths (the corrupt block literally held one);
  the catch backtrace shows the trapping Allocate was inside `String::SetLength ← System::operator+`
  (path concat) in `FindOrLoad` — our geometry loads may leave the shared `ExternalFactory` path cache /
  ref table holding an over-/under-referenced absolute-path String that the later dialog load frees+touches.

### DECISIVE pin (session 3) — dynamic refcount watch (static audit of the main helpers came up clean)
Catch the SOURCE increment directly: under the redzone watchpoint run, when it stops at `UnlinkFreeTag` the
corrupt block addr is in r11/r14 (e.g. `0x3340140`); but that's too late. Instead, since the corrupt blocks
cluster (`0x3340140`/`0x3390140` across catches — bucket-12, ~30-byte String path buffers), try: (a) compare
DLL-hook mode (original exe code) vs standalone for the SAME load with the redzone shim — if the original
DOESN'T corrupt, binary-search which of our reimpl helpers, when swapped back to the trampoline, stops it;
(b) OR wire enough of `Modification::SetupFileSystem` to make paths RELATIVE (short) and see if the crash
vanishes — confirms the absolute-path lifetime interaction; (c) OR line-diff `FUN_140020150`/`FUN_14001ffa0`.

### FIX-HUNT NEXT STEP (session 3) — original plan
Audit the System::Filename/String refcount BALANCE in our 4 serialize helpers against the Runtime semantics of
`fn_External_GetPath`, `fn_Filename_Ctor`, `fn_Filename_AssignOp`, `fn_Filename_Dtor`, `g_fn_String_Serialize`
(decompile each in Ghidra-Runtime to learn whether each INCREMENTs/borrows/CONSUMEs a ref). The flow per
element is `Ctor(fnameBuf); GetPath(*slot,pathBuf); AssignOp(fnameBuf,pathBuf); Dtor(pathBuf); String_Serialize(
fnameBuf); ... Dtor(fnameBuf)` — find the op whose ref semantics we modeled wrong (prime suspect: does
`fn_External_GetPath` RETURN a borrowed ref that we then `Dtor` (over-release), or does `g_fn_String_Serialize`
on LOAD replace fnameBuf's String without releasing the old, etc.). To confirm a fix: rebuild, then the redzone
loop (`notes/wp_driver.sh` style, or `zero_ab.sh` reused) should drop from ~100% crash to 0, AND the menu
background image should render (same root as the "missing bg image" symptom). The watchpoint
(`notes/catch_wp.gdb`, redzone) is the regression check — it should never fire post-fix.

### (older) catch the SOURCE write (one level up from UnlinkFreeTag) — SUPERSEDED, mechanism now known
The head watchpoint catches the propagation; now catch the write that sets bit-32 on the *block's* link. Options:
1. **Re-derive + watch the live object**: under the redzone watchpoint run, when it catches at UnlinkFreeTag,
   the corrupt tag is at r11/r14 (e.g. `0x3390140`); its link high-dword is at `tag_link+4`. The wild write
   hit that address while the block was LIVE, during geometry. Re-run watching that address is hard (varies),
   BUT: instrument OUR geometry serialize to, after each helper, scan recently-freed/live blocks for a link
   whose high dword != 0 — or simpler, **audit the refcount path**: every `fn_Filename_Dtor` / `fn_Filename_AssignOp`
   / `g_fn_String_Serialize` call in serialize_externals.cpp / serialize_externals_typed.cpp /
   serialize_arrays.cpp / serialize_struct60b_serialize.cpp operates on an 8-byte stack Filename — find the one
   that can hold a STALE/garbage string pointer (e.g. `fn_External_GetPath(*slot, pathBuf)` when `*slot` is a
   non-null garbage External, then `fn_Filename_Dtor(pathBuf)` decrements at garbage-4). The `*slot` would have
   to be non-null garbage; check whether ANY External slot/refField our geometry reads is un-initialized in a
   path zeroing-our-heap doesn't cover (Runtime-allocated container, or a stack/struct field).
2. **Disassemble `UnlinkFreeTag` from entry** (Runtime `??UnlinkFreeTag@MemoryAllocator@System@@`) to find the
   exact offset where rdx (the corrupt link) was loaded from the tag — confirms the link field offset, then
   correlate to what our code writes at that offset of a live object.

## ★★ 2026-06-04 SESSION 2 — DOMINANT CRASH PRECISELY PINNED (major reframe) ★★
The "wild bit-32 write on the bucket-12 head (alloc+0x88)" that the rest of this doc chases is the **RARE**
face — under GDB it essentially never fires (watchpoint on alloc+0x88 ran ~11 times, 0 catches). The
**DOMINANT** crash (what actually kills ~all runs) is a **NULL texture deref**, now pinned exactly:

- **Crash site**: `Graphics::MaterialInstance::BuildParameters+0x1363` (Runtime+0x103933).
  Disasm of the loop: `count = *(int*)(this+0x58)` (=1, correct); `data = *(void**)(this+0x50)`;
  per element it does `mov (rcx),rax` (rax = the param's *resolved* object pointer) then
  `cmpl $0x4, 0x2c(rax)` → **rax is NULL** → AV reading null+0x2c.
- **The object**: `this` = a `MaterialInstance` for **`Dialog\Sprite\Background\MainMenuMaterial.rsc:resource`**
  (the menu dialog's background material), loaded via `SpriteSheet::Serialize → External::Load → FindOrLoad`
  during `Stub_Load → fn_DialogLoopT_Load` (the MainMenu.rsc dialog load). NOT during the geometry deserialize
  itself — geometry runs earlier (splash chain) and leaves state that makes THIS later load fail.
- **The null**: the material's single parameter is **`"diffuse"`** (name string intact in the heap) which
  references texture **`Dialog\Sprite\Background\MainMenuTexture.rsc:resource`** (path string intact in the
  heap). The param descriptor is fully intact; only the **resolved texture pointer** (element[0]+8) is NULL
  → the texture's `FindOrLoad` returned NULL.
- **Evidence favors a RESOURCE/FACTORY-STATE failure, NOT a heap OOB write**: (a) the redzone crash dump
  reports **heap metadata intact** (no tag/free-link corruption); (b) the param name + texture path strings
  are perfectly intact — only the resolved pointer is a CLEAN null (an OOB overrun would leave garbage and/or
  corrupt the adjacent descriptor); (c) **zeroing every one of OUR allocations did NOT change the crash**
  (A/B below). So the leading hypothesis is now: **our geometry MaterialInstance deserialize poisons the
  Runtime `ExternalFactory` reference table / resource cache**, so the dialog's later `FindOrLoad` of
  MainMenuTexture.rsc returns null. (The rare bucket-head bit-32 write may be a separate or downstream face.)

### How it was pinned (reproduce the inspection)
- New launch config **"Launch Standalone (debug, redzone)"** (added to .vscode/launch.json): sets
  `BANISHED_REDZONE=1`. **With redzone the crash is ~100% deterministic in ~1s** (vs intermittent ~37%
  without) — ideal for a reliable GDB stop. CAVEAT: redzone perturbs timing; `BANISHED_NO_MENU_GEOM=1` does
  NOT cleanly stop the redzone crash (it instead misaligns the stream → camera=zeros → different failure), so
  don't use NO_MENU_GEOM as a clean control under redzone.
- At the line-185 breakpoint (geometry_serialize.cpp), run `-exec handle SIGSEGV stop nopass print` then
  `disable 2`, then `continue` → GDB stops cleanly at the AV (the app's own crash handler does NOT run, so no
  hang). Then `x/30i $pc-110`, `info registers`, and walk `this`(=r15) +0x50/+0x58.
- **Zombie reaping**: redzone-crashed processes hang unkillable by taskkill/SIGTERM — reap with
  `wmic process where "name='reimpl-x64.exe'" delete`. (taskkill //IM and `timeout` SIGTERM do NOT work.)

### A/B test infrastructure added this session (kept, all env-gated/default-inert)
- `standalone/redzone_alloc.cpp`: new **`BANISHED_REDZONE_ZERO=1`** mode — zero-fills every allocation handed
  out by our `fn_MemAlloc` shim (PAD=0, no layout change). A/B result: **ZERO=0 → 15/15 crash, ZERO=1 →
  15/15 crash** → zeroing our allocations does NOT fix it → NOT a stale/uninitialized read in our buffers.
  (Caveat: only zeros OUR allocations, not Runtime's; the MaterialInstance/texture are Runtime-allocated.)
- Scripts (in notes/): `catch_wp.gdb` (alloc+0x88 watchpoint), `wp_driver.sh` (relaunch-until-watchpoint),
  `zero_ab.sh` (robust launch/poll-crash.log/wmic-reap A/B loop). `wp_runs/` = raw gdb run dumps.

### Session 2b — texture IS path-loaded; the crash is a HEISENBUG (instrumentation suppresses it)
Batch-GDB trace (`notes/fol_trace.gdb`, logs every `FindOrLoad` path arg `*(char**)$rdx`, NO inferior calls —
`strstr` in a breakpoint condition/`if` is ABANDONED by GDB because another of the ~90 threads interrupts the
inferior call, so it cannot be used here):
- **MainMenuTexture.rsc IS FindOrLoad'd by PATH** during the dialog load — the full menu dialog sequence is
  `Dialog\MainMenu.rsc...` → `Dialog\Sprite\Background\MainMenu.rsc:resource` → `...MainMenuMaterial.rsc` →
  `...MainMenuTexture.rsc` (FOL #~2818-2831 of ~3957). So it's a plain path load, NOT a ref-id lookup; the
  factory-ref-id-poisoning theory is weaker than expected.
- **The FindOrLoad breakpoint (~3957 break/continue) SUPPRESSED the crash** — that run reached the menu clean
  (exit 124 timeout, got all the way to Audio\UI\*.rsc loads). The crash is **timing/layout-sensitive**: redzone
  (per-alloc mutex) → ~100% crash in ~1s; heavy main-thread instrumentation → 0% crash. A true Heisenbug.
- **`BANISHED_SYNC_ENGINE_LOADER=1` does NOT help** (5/5 still crash under redzone) → NOT the engine-loader
  thread race. So the timing-sensitivity is NOT that background loader.
- Net: the texture's resolved pointer is NULL in crashing runs but the resource clearly CAN load (clean runs +
  the instrumented run load it). So either (a) `FindOrLoad` intermittently returns NULL due to heap/loader
  state, or (b) the resolved pointer is WRITTEN non-null then NULLED by a layout-dependent OOB write. Can't
  distinguish by observation (it suppresses the crash).

### DECISIVE NEXT STEP (session 3) — GUARD PAGES (the right tool for a Heisenbug OOB write)
Live observation suppresses the crash, so stop trying to *watch* it and instead make any overrun fault AT the
write, layout-independently: add a redzone mode that VirtualAlloc's each of OUR `fn_MemAlloc` allocations into
its own region with a trailing `PAGE_NOACCESS` guard page (classic PageHeap, but for OUR allocations — the
engine's boundary-tag allocator never guards). A contiguous overrun of any of our geometry buffers then faults
**at the exact overrunning instruction** regardless of heap layout (so it does NOT Heisenbug away like the
downstream null does). Implement in `standalone/redzone_alloc.cpp` (`BANISHED_REDZONE_GUARD=1`): on alloc,
`VirtualAlloc(size rounded to page + one guard page, PAGE_READWRITE)`, place the user block at the END of the
last data page so its tail abuts the guard page, `VirtualProtect(guardPage, PAGE_NOACCESS)`; track base for
free (`VirtualFree`). Caveats: only catches CONTIGUOUS overruns of OUR buffers (not wild/indexed writes — but
this DOMINANT face may well be contiguous; the bucket-head face was the one called "wild/indexed"); heavy
(page per alloc) but fine for a diagnostic run under GDB. Run under "Launch Standalone (debug, redzone)" + the
guard env; GDB stops at the faulting write → that's the bug. Trace file: `notes/fol_trace.gdb`.

### (lower priority) Alt next step — texture-slot hardware watchpoint
If guard pages miss it (wild write, or the overrun is into a Runtime block past OUR guard), then it's likely
`FindOrLoad` genuinely returning NULL: break at `MaterialInstance::Serialize` for MainMenuMaterial (filter
without strstr — e.g. ignore-count to the dialog phase, ~FOL #2570+), find the param's texture-ptr slot
address, HW-watch it to see if it's written non-null then nulled (OOB) vs never written (load returned null).

### (original) Find why FindOrLoad returns NULL when geometry ran:
1. Under the redzone GDB config, set a breakpoint on `Object::ExternalFactory::FindOrLoad` (or the texture
   `External::Load`) and inspect the path arg + return value for MainMenuTexture.rsc — confirm it returns null.
2. Compare the `ExternalFactory` reference-id table / resource cache state right before that lookup vs a
   no-geometry run. Prime suspect: our `Struct60B_ArraySerializeMaterialInstance_Body` (FUN_140020150,
   +0x210 array) and `ExternalPtrSerializeGeneric_7140` (Struct60B +0x00) refMode path —
   `GetReferenceId`/`GetPathForReferenceId` may register a WRONG ref-id → poisons the factory so the later
   texture lookup resolves null. Verify our refId round-trip against the original FUN_140020150 disassembly.
3. Alternatively/also: the dialog material loads textures the geometry materials already touched — check for a
   shared-resource cache entry our geometry load left in a bad state.

(The plan below — pinning the bucket-head wild write — is the RARE face; deprioritize unless the texture-null
lead dead-ends.)

---
## (Earlier plan — the RARE bucket-head face; deprioritized, see above)

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
