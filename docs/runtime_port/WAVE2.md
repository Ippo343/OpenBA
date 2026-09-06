# Wave 2 — System + Collection

Landed 2026-09-06 on `wave/upgrade-path-complete`, following [PLAN.md](PLAN.md) §4.2.
Wave 1 proved the seam on pure leaf maths. Wave 2 is the first wave to touch the
foundation classes that everything above them embeds — `String` alone has 2,388
inbound call edges inside the DLL — and the first to reach a subsystem the wave
deliberately refused to take live.

## Result

| | |
|---|---|
| Ported | **53 of the 60 System functions our tree calls** (String 18, Filename 11, value types 17, CriticalSection/Thread 8; a few register with no caller and are counted honestly) |
| Census | `System 88.3%`, `Math 98.3%`, `Volume 100%`; 873 called, 139 ported (15.9%) overall |
| Diff oracle | **22,733,915 calls across 45 sites, 0 new divergences** |
| Of which `Random::Rand(uint)` | **16,035,064 calls, 0 divergences** — the hottest function in the port, bit-exact |
| Residual | 99 divergences, all in the two Wave 1 sin/cos-fed sites, all 1 ULP, unchanged in kind from the pre-wave baseline (88) |
| Collection | layouts recovered as headers, nothing ported — see [WAVE2_COLLECTION.md](WAVE2_COLLECTION.md) |
| MemoryAllocator | fully reversed and ported, **opt-in, verdict NO-GO** — see [WAVE2_ALLOCATOR.md](WAVE2_ALLOCATOR.md) |

Pre-wave baseline, same harness, Wave 1 code: 32 sites, 5,735,801 calls, 88 divergences in
2 sites. The wave added 13 sites and 17M calls and introduced no new divergence.

## What landed

| Piece | Files |
|---|---|
| Wave entry point | `runtime/src/system/rt_system_port.cpp` — one `RtSystemPortInit()`, five per-lane `Register` functions, so no two lanes ever edit the same file |
| String + Filename | `runtime/types/system/{String,Filename}.h`, `runtime/src/system/{string,filename}.cpp` |
| Value types | `runtime/types/system/Value.h`, `runtime/src/system/value_types.cpp` |
| Threading, Win32 seam | `runtime/types/system/Thread.h` (platform-neutral), `runtime/src/system/thread_win.cpp` |
| Allocator | `runtime/types/system/MemoryAllocator.h`, `runtime/src/system/{allocator,allocator_win}.cpp` |
| Collection layouts | `runtime/types/collection/Collection.h` |
| Export-name audit | `tools/export_name_audit.py` |

## Environment variables added

| Variable | Effect |
|---|---|
| `BANISHED_RT_SYSTEM=0` | Leave the whole System namespace on the DLL |
| `BANISHED_RT_STRING=0` / `_FILENAME=0` / `_VALUE=0` / `_THREAD=0` | Per-class kill switches |
| `BANISHED_RT_ALLOC=1` | **Opt-in, off by default, and it refuses to arm** while the allocator lock holds the prologues. Verdict is NO-GO; see WAVE2_ALLOCATOR.md |

## The findings that change other waves

### 1. The DLL exports 147 vtables. PLAN's hazard D said none were resolvable.

`??_7Thread@System@@6B@` is a real PE export at `0x180155f20`, and it is not a special
case: 147 `??_7` symbols are exported (UI 68, Graphics 20, Video 19, IO 15, Object 7,
Engine 5, …). Wave 2's `Thread` ctor/dtor stamp the DLL's own vtable **by name** instead
of mirroring slots out of a live object. Two of the four classes PLAN §2.2 hazard D lists
as needing that workaround — `Graphics::MaterialInstance` and `UI::SpriteSheet` — have
exported vtables too, so `code/Game/Dialog/thumb_vtables.cpp`'s slot-copying was never
necessary for them.

**Do not over-read this.** An exported vtable makes the ADDRESS available. Whether
stamping it is SAFE is a per-class argument, and `Thread`'s was easy for a specific
reason: it is an always-subclassed base whose slot 0 is `_purecall`, so no object ever
dispatches through the base vtable. Re-make that argument per class.

### 2. There is no `Collection::Map`.

PLAN §4.3 risk 1 said "`Map` is the AVL tree behind `Map_Insert`/`Map_Find`". Those are
names *we* invented in Ghidra. Template-id counts across the 4,926 exports: `InternalPtr`
103, `ExternalPtr` 94, `Array` 44, `InterpolatedState` 57, `HashGrid` 4, `Set` 2,
`LinkedList` 1, **`Map` 0**. A "map" is `Set<Pair<K,V>, Alloc, K, Compare>`, which is why
`Set` carries separate `Key` and `Compare` parameters —
`Set<Filename, DefaultAllocator, Filename, AVLCompare>` appears verbatim in
`PackageFile::BuildPackageFile`'s signature. Full evidence and per-container confidence in
[WAVE2_COLLECTION.md](WAVE2_COLLECTION.md).

### 3. The allocator is not what the plan described, and the risk is different.

Not bucketed free lists: a **single-arena boundary-tag allocator with 192 segregated
first-fit bins**, `sizeof == 0x670`, **embedded at offset 0 of `Engine::Engine`**. And the
`str-4`/`str-8` header PLAN §4.3 risk 2 attributed to the allocator is
**`System::String`'s** — `SetLength` allocates `len+9` and sets `data = user+8`, so
`data-8` is the length, `data-4` the refcount, and the allocator's boundary tag is a
further 8 bytes back at `data-0x10`. The allocator header has no refcount.

The brief's nightmare — "we allocate from our heap while the DLL frees into its own" —
**cannot happen**: the port drives the live `_singleton`'s own tags, bins and ranges. One
heap, one metadata set. The real hazard is an ordering trap, below.

### 4. A `ResolveExport` name that does not exist fails SILENTLY, and 22 of ours don't exist.

`ResolveExport` returns null for a name the DLL does not contain, and nearly every call
site falls back to `ResolveAddr(Module::Runtime, …)` — so the pointer ends up valid and
the game works. The bug only appears when someone ports that function:
`RegisterRuntimeExport` is keyed on the NAME, so a site that resolved by ADDRESS never
sees the port. It registers, the census counts it, the diff harness reports `calls=0`,
and the summary looks green. That is WAVE1.md defect 4, and it would have made Lane A's
entire `Filename` port inert.

Two were found this wave by porting the functions:

| Our name | The DLL's actual export |
|---|---|
| `?AdjustDelimiters@Filename@System@@QEAAXXZ` | `…@AEAAXXZ` — a **private** member |
| `?GetPathIdentifer@…@QEBA?AVString@2@XZ` | `…?AV12@XZ` — returns **Filename**, not String |

`tools/export_name_audit.py` now checks all of them in one pass: **620 distinct
`ResolveExport` names, 22 not in the DLL** after this wave's fixes (Wave 1 hand-fixed four
more). Not every hit is a defect — some are deliberate take-the-fallback idioms — but each
is a function a future wave cannot port through its name, so each needs a verdict.

One of the 22 was `?MemAlloc@MemoryAllocator@System@@SAPEAXII@Z`, a phantom: only the
`_KH` (size_t, int) form exists. Six call sites used it as their only fallback behind
dispatch slot `0x140152b60`; the slot registration is what has been carrying every call.
All six now name the real export. The typedefs were already `(uint64_t, uint32_t)`, so the
swap is ABI-identical.

### 5. A gated registration is not a port — the census said 95% for code that never ran.

`RtSystemAllocatorRegister()` returns 0 unless `BANISHED_RT_ALLOC` arms it, but
`runtime_census.py` counts `RegisterRuntimeExport` calls by scanning source, so it read
27 allocator registrations as live and reported System at 95%. That is the same
overstatement as a diff site reporting `divergences=0` on `calls=0`. The census now knows
about `GATED_OFF_FILES` and reports them separately, under the line "a gated registration
is not a port. Arm it, measure it, then move it."

## Performance at the wave boundary — the 5% gate is BREACHED

PLAN §5 requires `sa-stress` `sim ms / iteration` within 5% of the pre-port baseline at
each wave boundary. It is not.

| Run | `sim ms / iter` | vs baseline |
|---|---|---|
| baseline `perf/baselines/current.csv` (2026-09-05, pre-Wave-1) | 0.19 | — |
| Wave 2, ports ON | 0.21 | **+6.7%** |
| Wave 2, ports ON (repeat) | 0.21 | **+7.2%** |
| Wave 2, `BANISHED_RT_SYSTEM=0` (A/B control) | 0.20 | **+4.5%** |

The two ON runs agree, so this is signal, not noise. Splitting it with the wave's own kill
switch: **about 2.5 points are Wave 2's**, and **about 4.5 points predate it** — the
baseline was recorded before Wave 1 landed, so that share is Wave 1 plus ordinary drift.
`stress_report.py` reports "0 regressions over threshold" because its own threshold is
looser than PLAN's gate; the harness passing is not the gate passing.

Wave 2's own ~2.5% is the expected shape: every ported call now goes through the runtime
registry indirection, and `Random::Rand` alone is 16M calls per run. That is a real cost
and it is not obviously acceptable — it is the price of the seam, paid on the hottest
function in the engine.

**Open, and owned by Wave 3, not silently accepted.** Two things have to happen: the
pre-Wave-1 share needs attributing (Wave 1 never ran `sa-stress` at all — see below), and
the registry indirection on hot leaf functions needs measuring properly with
`BANISHED_SAMPLE`. Do not re-baseline to make the number go away; re-baselining is how a
5% gate becomes a 0% gate that measures nothing.

### The perf build had been broken since Wave 1

`sa-stress` did not compile. `standalone/log_strip.h` strips logging with
`#define printf(...) ((void)0)`, which a qualified `std::printf` call defeats —
`error: expected unqualified-id before '(' token`. `runtime/src/math/transform.cpp` has
used `std::printf` since `e635a93`, so the perf build has been failing since Wave 1
landed, and nobody saw it because WAVE1.md records that `sa-stress` was never run.
All of `runtime/src/` now uses bare `printf`.

This is the memory note `reference_perf_measurement_traps_icons_and_stale_exe.md` playing
out exactly: a failed build measures the OLD exe. Had the stress run been attempted
without reading the build output, it would have benchmarked a binary with no Wave 2 in it
and reported a clean result.

## Defects found

### A. `Thread`'s ctor dropped `CREATE_SUSPENDED` (port bug, caught in review)

Retail `0x1800ca510` writes `[RSP+0x20] = 4` — `CREATE_SUSPENDED` — and calls
`ResumeThread` only after the three `CreateEventW` calls. The first draft passed `0`. The
worker would then start immediately, and `ThreadFunc`'s first act is to wait on
`{hStart, hStop}`, which the ctor has not created yet: a wait on uninitialised handles.
The port already had the *order* right; the suspend flag is what makes the order matter.

### B. `Thread`'s ctor used the wrong CRT (port bug, caught in review)

Retail calls the DLL's own `_beginthreadex` at `0x18007fbfc`. The draft called MinGW's, so
a thread whose per-thread CRT state we set up would run `ThreadFunc` — DLL code, DLL CRT —
and unwind out through our `_endthreadex`. Now resolved by address so the whole thread
lifetime stays on the DLL's CRT, as retail has it. (PLAN §4.3 risk 5.)

### C. Two exports resolved under manglings the DLL does not contain (PRE-EXISTING)

See finding 4. Latent, not live: both had address fallbacks.

### D. `?MemAlloc@…@SAPEAXII@Z` is a phantom (PRE-EXISTING)

See finding 4. Contained by the dispatch-slot registration, and already noted in
`standalone/address_registry.cpp:1316` — the note was right and nobody had acted on it.

### E. `Set` counts typed 64-bit where retail keeps 32 (PRE-EXISTING, latent, NOT fixed)

`engine/types/EntityList.h:73,75` types two `Set` counts as `int64_t`. Retail maintains
only the low 32 bits and never writes `+0x0c`. Every current use is a read
(`code/Game/Entity/entitylist.cpp:442,443,473,474,577,583,599`), so it is harmless today
and becomes a clobber the first time someone assigns through it. Left alone deliberately:
changing a field width in a shared layout header mid-wave, with four lanes writing, is how
you turn a latent bug into a live one. Fix it as its own change, with a run.

## What was deliberately NOT done

* **The allocator is not live.** Its shadow proof has never executed a single instruction,
  and the lane reported no numbers rather than numbers it did not have. `BANISHED_RT_ALLOC`
  is opt-in and additionally hard-refuses to arm while `standalone/allocator_lock.cpp`
  holds the two prologues — because `RuntimePortInit()` (`standalone/main.cpp:1599`) runs
  BEFORE `InstallAllocatorLock()` (`:1660`), so arming it would make the lock's `memcmp`
  guard decline and leave the allocator running **unlocked**. The lane proposed reordering
  the installs; that was declined for this wave. Reordering the default boot path — used by
  100% of runs — to protect an opt-in path used by nobody is the wrong trade. The refusal
  makes the failure unreachable instead. The reorder is recorded in WAVE2_ALLOCATOR.md §4
  as the prerequisite for whoever takes it live.
* **`Time::Time(float)` and `Time::AsSeconds` are forwarders, not reimplementations.** They
  delegate to the DLL's `Timer::SecondsToTicks`/`TicksToSeconds` because the conversion
  depends on `Timer::_secondsPerTick`, a `QueryPerformanceFrequency`-derived global that
  was not in scope. The census counts them as ported; they are passthroughs. Revisit when
  Timer is ported.
* **`Thread::ThreadFunc` and the base vtable are used from the DLL as-is**, by design — see
  finding 1.
* **WideString, CRC, Compression, Timer** appear in PLAN's Wave 2 scope line but are not
  called by our tree, so they were not ported. "Called" is the honest denominator
  (PLAN §4.1 decision 1: static link, no drop-in DLL).
* **No detours were installed**, as in Wave 1. Registration redirects OUR call sites only;
  the DLL's internal calls to `String`, `Filename` and `Random` still land in the DLL. That
  is safe here because these ports share the DLL's allocator and its exact layouts, so an
  object is interchangeable whichever side made it. It stops being safe the moment a class
  has state we own — which is exactly why the allocator is the one thing held back.

## Notes for later waves

* **Check `calls=`, not just `divergences=`.** Wave 1 learned this; Wave 2 confirms it from
  the other direction. The export-name audit exists because a port can be perfectly correct
  and never execute. A green summary over `calls=0` is not evidence of anything.
* **The `Set` node trap.** Node pointer offsets are a function of `sizeof(T)`, so MSVC
  emits a separate insert/find/rebalance per instantiation. One ported `Set` body cannot
  serve two element widths. This is `reference_one_bst_find_two_node_layouts.md` again, and
  Wave 3 (Object) will meet it immediately.
* **A `FreeTag*` is the USER pointer of a free block (`tag + 8`), not its header.**
  `GetFreeTagForSize`, `LinkFreeTag` and the two `UnlinkFreeTag` overloads disagree with
  each other about which of the two they take. Getting it backwards is a one-qword error
  that reads as a perfectly plausible pointer.
* **`String::operator+=` returns a fresh String BY VALUE** through a hidden slot, not
  `String&` — but the returned object shares `this`'s buffer via a single AddRef, so it is
  indistinguishable from `this`, which is why nobody has ever noticed the refcount
  round-trip per `a += b`. Three arguments where the name implies two
  (`RCX=this, RDX=ret, R8=rhs`), and Ghidra drops `R8` because the inner `operator+` is
  itself a hidden-return-slot call — the WAVE1 defect-5 trap, hit again.
* **`~String`'s two branches are asymmetric and must stay that way.** On the freeing branch
  the object is left holding `data - 8`, a dangling pointer; on the sharing branch it is
  nulled. A port that "tidied" both to null would be indistinguishable in the dtor and
  would diverge in `Set()` and `operator=`, which inline the same sequence.
