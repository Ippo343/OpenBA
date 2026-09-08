# The controlled cutover boot — runbook

Written by Wave 9 lane C2, which owns the minted DATA and asked the question
this procedure answers. **Assumes you have not read
[WAVE9_LANE_C2.md](WAVE9_LANE_C2.md).** Everything you need is here.

## What this is, and what it is not

It is a **reversible diagnostic**, run repeatedly. Move `Runtime-x64.dll` aside,
boot with the mint registered, read where it dies, move the DLL back, fix the
one row it named, repeat. Nothing is deleted; the DLL comes back after every
attempt, including successful ones.

**It is not lane G.** Removing `LoadLibrary` is the *recording* of something
already observed here. It is also not a substitute for the pre-cutover gates —
those had to be recorded first precisely because their instruments die with the
DLL, and they are recorded.

**What it produces that no static analysis can**: the ORDER in which things are
first dereferenced. Lane C2 enumerated 14 boot-path rows whose singleton is
zero after cutover, each with a reader on a known-executing path — but
"known-executing" comes from where the reader *lives*, not from an observed
sequence. Which one the boot hits first is not derivable on this side of the
cutover. The distance travelled per attempt is the progress metric.

---

## PREREQUISITE — one code change, without which every attempt is uninformative

`standalone/main.cpp:1645` currently reads:

```cpp
if (!ResolveInit()) {
    fprintf(stderr, "[Standalone] FATAL: Could not load Runtime-x64.dll\n");
    ...
    return false;
}
```

and `ResolveInit()` (`standalone/resolve_standalone.cpp`) returns `false` when
`LoadLibraryA("Runtime-x64.dll")` fails. **So with the DLL moved aside, the
process exits here — before `RuntimePortInit()` at `main.cpp:1708`, before the
mint registers anything, before a single ported body runs.** You would learn
nothing, and the run would look like "cutover fails", which is not a finding.

The change needed is an env-gated allowance, not a behaviour change:
`ResolveInit` must be permitted to continue with `g_modules[1].handle == nullptr`
when a variable such as `BANISHED_RT_NO_DLL=1` is set, leaving the Runtime
delta at zero and letting the caller proceed. Everything downstream already
copes with a null module handle by design — `DllExport`/`DllAddr` in the mint
return `nullptr`, `ResolveExportRawImpl` returns `nullptr`, and the shadow check
prints an explicit `DLL=ABSENT` banner.

**Whoever makes that change should also confirm that
`HookRuntimeMessageBoxW()` (`main.cpp:1657`) degrades rather than faults with no
Runtime module — it already logs a warning on failure, but that path has never
run with the module genuinely absent.**

Until that lands, do not run this procedure: it cannot produce an informative
result.

## PREREQUISITES — state of the tree

1. **Lane X's 14 tier-1 writer ports have landed.** Before that, attempt 1 dies
   on the first of them and tells you only what lane C2 already wrote down.
2. **The allocator is armed** (`BANISHED_RT_ALLOC=1`). This is not optional
   here and it is not lane C2's call: with no DLL there is no other heap, so an
   unarmed allocator makes the first `MemAlloc` resolve to null. Confirm with
   lane E that its GO gate is armed, not merely ready.
3. **The exe is freshly built and deployed, and you know its timestamp.** Quote
   it in every result. `mingw32-make sa-deploy`, then
   `ls -l "C:/GOG Games/Banished/reimpl-x64.exe"`.
4. **You hold the game directory.** One lane at a time builds and runs; a second
   `reimpl-x64.exe` in the directory will make `cp` fail with "Device or
   resource busy" and can make you measure someone else's binary.
5. **No `reimpl-x64.exe` is running**: `tasklist | grep -i reimpl` returns
   nothing.

## Reassurance before you move anything

`reimpl-x64.exe` does **not** import `Runtime-x64.dll` through its PE import
table — verified with `pefile` against the deployed exe: 19 imported DLLs, none
of them Runtime or VideoDX11. The DLL is reached only through `LoadLibrary`.
So moving it aside does not prevent Windows from starting the process, and the
boot gets far enough to be informative. (The *original* `Application-x64.exe`
does import it normally — that one will not start with the DLL absent. Do not
use it to sanity-check this procedure.)

`VideoDX11-x64.dll` is already fully replaced by `dx11/` and is never loaded.
Leave it alone.

---

## The procedure, one attempt

### 1. Move the DLL aside

```bash
cd "C:/GOG Games/Banished"
mv Runtime-x64.dll Runtime-x64.dll.absent
ls -l Runtime-x64.dll*          # expect ONLY the .absent file
```

Rename, do not delete, and do not move it out of the directory — a rename in
place is the cheapest thing to undo and the hardest to get wrong under pressure.

### 2. Boot

```bash
cd "C:/GOG Games/Banished"
rm -f standalone_debug.log standalone_error.log standalone_crash.log
BANISHED_RT_NO_DLL=1 \
BANISHED_RT_MINT=1 \
BANISHED_RT_MINT_CTOR=1 \
BANISHED_RT_ALLOC=1 \
BANISHED_RT_STRICT=1 \
BANISHED_RT_MINT_SHADOW=1 \
BANISHED_AUTOSTART=1 \
./reimpl-x64.exe
```

What each one is for:

| variable | why |
|---|---|
| `BANISHED_RT_NO_DLL=1` | the prerequisite above; without it you exit at `main.cpp:1645` |
| `BANISHED_RT_MINT=1` | **the point of the exercise.** Registers the minted vtables, `BaseTypeInfo` and storage by name AND by address. Without it every resolve misses and you are testing nothing |
| `BANISHED_RT_MINT_CTOR=1` | runs the three load-time constructions (`_loader`, `_rng`, the type chain). Retail's CRT did these; nothing else will |
| `BANISHED_RT_ALLOC=1` | there is no other heap |
| `BANISHED_RT_STRICT=1` | **your best attribution instrument.** Records every resolve that missed the port registry, with the return address of the first asker. With the DLL gone those all return null, so this list is literally "what the process asked for and did not get" |
| `BANISHED_RT_MINT_SHADOW=1` | prints `DLL=ABSENT (post-cutover; oracle gone)`. It cannot compare anything — that is the proof the run really had no DLL |
| `BANISHED_AUTOSTART=1` | drives menu → New Game unattended, so distance travelled is comparable between attempts |

Do **not** set `BANISHED_RT_DIFF=1`. The oracle it drives compares against the
DLL; with no DLL it can only add noise.

Do not use `tools/rt_run.sh` for this. It calls `tools/boot_state.sh --wait`,
which is tuned for a boot that is expected to succeed, and it deletes the logs
you may want from a previous attempt. Launch directly.

### 3. Read the outcome, in this order

1. `standalone_crash.log` — **first, always.** Fault address, registers, stack.
2. `standalone_error.log` — the `[RtMint] FATAL:` traps print here, as does the
   `[RtResidue]` list at exit.
3. `standalone_debug.log` — `[RtMint]`, `[RtPort]`, `[BootState]`, `[AutoStart]`
   progress lines.
4. `standalone_boot_state.txt` — one machine-readable line.

### 4. Restore the DLL — every time, pass or fail

```bash
cd "C:/GOG Games/Banished"
mv Runtime-x64.dll.absent Runtime-x64.dll
ls -l Runtime-x64.dll
```

**Non-negotiable, including after a successful boot.** Every differential
oracle, every `RtOriginal` call site, `tools/resolve_fallback_audit.py` and
every other lane's measurement depend on that file being present. Leaving it
renamed silently breaks other lanes' work in ways that look like port defects.

If you are interrupted, the recovery is the same command; the file is never
anywhere but next to its own name.

---

## What a SUCCESSFUL failure looks like

An attempt is a success if it names **one row** to fix. Any of these qualify:

* **A mint trap.** `standalone_error.log` contains

  ```
  [RtMint] FATAL: call through a NOT-YET-PORTED minted vtable slot
  [RtMint]   vtable : ??_7Geometry@Graphics@@6B@
  [RtMint]   slot   : 0
  [RtMint]   target : (no export name; DLL-internal)
  ```

  This is the best possible outcome: the vtable and slot are named, and the fix
  is to port that slot's function. A `pure-virtual call` variant of the same
  message means something dispatched through a slot retail guarantees is never
  called — that is a mis-typed pointer or a mis-minted table, not missing work.

* **A null-ish fault with an attributable stack.** `standalone_crash.log` shows
  a fault address **below `0x10000`** (a null pointer plus a field offset — the
  offset itself is a strong clue: `null+0x640` was lane E3's allocator) and a
  stack whose top frames name one of our accessors. Symbolize with
  `nm standalone/build/reimpl-x64.exe`; the exe's image base is `0x140000000`.

* **`EXEC-of-0`.** The VEH handler labels this explicitly. It means a null
  function pointer was called — for this wave, a vtable slot that was left
  null rather than trapped, or a resolve that returned null and was called
  anyway. Note the caller frame.

In all three cases, record the attempt as: **exe timestamp, the named row or
address, how far the boot got, and the `[RtResidue]` count.**

## What an UNINFORMATIVE result looks like, and what to do

| symptom | meaning | action |
|---|---|---|
| Process exits with `FATAL: Could not load Runtime-x64.dll` | the prerequisite change is not in the build you deployed | stop; deploy a build that has it |
| No `[RtMint] registered N by name, M by address` line in `standalone_debug.log` | the mint never registered — `BANISHED_RT_MINT` was not set, or init did not reach `RuntimePortInit` | fix the env or read where init stopped; the run tested nothing |
| Shadow banner says `DLL=present` | the DLL was still there. The rename did not take, or a copy exists elsewhere on the search path | `ls -l Runtime-x64.dll*`, check `C:\Windows\System32` and the working directory |
| A hang: no crash log, no new output for >60s | not attributable. Most likely a wait on something the DLL used to provide | `tools/boot_state.sh` for a blocked dialog; if not that, capture a stack with the debugger before killing, then treat as inconclusive |
| Fault at a wild address (not small, not in our image) with a stack of `??` | not attributable to a row | do not guess. Re-run with `BANISHED_REDZONE=watch` and consider that this is heap corruption, not a missing symbol |
| Crash inside `MemAlloc`/`MemFree` on attempt 1 | the allocator is not armed, or is armed and wrong | this is lane E's gate, not a mint row |

**An uninformative attempt is not a failed attempt — it is a failed
measurement.** Fix the measurement and re-run rather than drawing a conclusion.

## What to capture, every attempt

Archive under `docs/runtime_port/wave9_cutover/attempt_<NN>/`:

```
standalone_crash.log        (if any)
standalone_error.log
standalone_debug.log
standalone_boot_state.txt
notes.md                    exe timestamp, env used, verdict, the ONE row named
```

The logs are the evidence and they are cheap; the crash log in particular cannot
be reconstructed. Copy them before the next attempt overwrites them.

**The progress metric is distance travelled**, and it is comparable between
attempts only if the env is identical. Record it as the last of these reached:
`ResolveInit` → `RuntimePortInit` → `[RtMint] registered` → `Engine::Startup` →
`[BootState] RUNNING` → `[AutoStart] menu settled` → `[AutoStart] sequence
fired` → first frame → gameplay.

## Two expectations that are NOT defects

* **`[RtResidue]` will not be empty**, and that is fine here. With the DLL gone
  every unported resolve misses; the list is the point, not the failure. Its
  emptiness is a gate for lane G, measured with the DLL PRESENT.
* **The shadow check reports nothing comparable.** It prints the `DLL=ABSENT`
  banner and stops. It is included so the log records, in the run's own words,
  that there was no DLL.
