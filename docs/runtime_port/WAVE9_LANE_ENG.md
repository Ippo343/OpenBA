# Wave 9, lane ENG — `Engine::Engine` ctor / dtor / Startup / Shutdown

2026-09-08, `wave/upgrade-path-complete`. Worktree rebased onto **`8b00995`** (the
branch tip at the time of writing; the worktree was created at `965361d`, three
waves stale, and was `git reset --hard`'d before any conclusion was drawn — see
§9). **Source only.** `g++ -fsyntax-only` and a real `-c` + `nm` on every file, in
both the debug and the release (`-DBANISHED_STRIP_LOGS -include log_strip.h`)
configurations. The integrator owns the build and the game directory.

These four are the wall cutover boot attempts 04, 05 and 06 all died on:
`code/Game/Game/game_main.cpp` resolves all four, finds them null, and returns 1
or 2 out of `RunGameMain` before the engine starts.

**Delivered**

| File | What |
|---|---|
| `runtime/src/engine/engine_lifecycle.cpp` | the four bodies + 4 helper ports, 8 registrations, shadows, effectiveness counters |
| `runtime/src/engine/engine_lifecycle_win.cpp` | the 3 OS calls (`CoInitializeEx`, `CoUninitialize`, `FreeLibrary`) — PLAN §4.1 platform seam |
| `runtime/src/engine/rt_engine_port.cpp` | one aggregator call + kill switch; corrected the stale holdback paragraph |
| `runtime/src/engine/engine.cpp` | **comments only** — Wave 8 lane B's holdback table and prose said "allocator, NO-GO", which is now a lie |
| `standalone/main.cpp` | one line: `RtEngineLifecycleSummary()` beside `RtDiffSummary()`, plus its file-scope declaration |

Kill switch: **`BANISHED_RT_ENGINELIFE=0`**.

---

## 1. Was the "allocator, NO-GO" blocker real or stale? — **STALE, and inverted**

It was correct when written and every fact under it is still true:
`System::MemoryAllocator` is embedded at `Engine` offset 0 (`GetMemoryAllocator`
is `MOV RAX,RCX; RET`), `Engine::Startup` seeds three allocator fields and calls
its init at `0x1800db905`, `Engine::Shutdown` calls `0x1800c75b0`. What changed
is the verdict those facts were attached to:

* Wave 9 lanes **E / E2 / E3** built the shadow oracle `WAVE2_ALLOCATOR.md`
  demanded and ran it. The allocator port is complete.
* Lane **E4** wired the cutover arming path and **cutover attempt 06 logged**
  `[RtAlloc] ARMED (cutover: Runtime-x64.dll absent) -- 27 exports registered`.
  With the DLL gone our allocator is the only allocator in the process.

So "we must not take over the allocator" has become "we already have", and the
holdback's own premise now argues the other way. Wave 8 lane B was right on its
evidence; this is a transport failure of the kind this wave keeps paying for, and
the four call sites that asserted the stale reason are corrected in this commit
rather than left to mislead the next reader.

## 2. The coordinator's question: **does the port create the arena? YES, and it must**

The overlap warning was that lane E already owns "Engine::Startup's three direct
field writes and Shutdown", and that the allocator armed at cutover — so perhaps
the port must *not* create the arena a second time. **It must create it, once,
and nothing else does.** Evidence, not judgement:

1. `RtSystemAllocatorRegister()`'s cutover branch calls `RtAllocSwapArmCutover`
   (stores two function pointers, returns) and then `RegisterAllocatorExports()`
   (27 `RegisterRuntimeExport` calls). **Neither constructs a `MemoryAllocator`
   and neither reserves a byte.** "ARMED" means the names point at our bodies.
2. Lane E2 printed the ground truth at the one instant that matters:
   `[RtAllocShadow] Engine::Startup ENTRY (min=512MB max=0MB): real arena
   live=0 bytes=0 ranges=0, skipped so far=0`.
3. A full rel32 scan of the DLL's `.text` finds **zero** calls to
   `?Startup@MemoryAllocator@System@@QEAA_N_K0@Z` — MSVC inlined it into
   `Engine::Startup` and nowhere else.
4. It is structurally impossible for the arena to predate it: `rtalloc::Construct`
   leaves `pageSize` at 0 and `AddNewMemoryRange` divides by `pageSize`.

So there is no double-creation to avoid. **How** it creates it is where lane E's
work is honoured rather than duplicated: retail's five steps at
`0x1800db8d9`-`0x1800db917` (`minRangeSize`, `maxTotalSize`, `pageSize` from
`GetSystemInfo`, `AddNewMemoryRange`, and `Error::SetText` with the string at
`0x180155e48`) **are `MemoryAllocator::Startup` inlined, verbatim** — the same
operations in the same order, and the string at `0x180155e48` was read out of the
DLL and byte-compared with the literal in `rtalloc::Startup`. The port therefore
calls `?Startup@MemoryAllocator@System@@QEAA_N_K0@Z` and does not re-derive them.

That also keeps the DLL-present path unchanged: with `BANISHED_RT_ALLOC` unset
that name resolves to the DLL's own `0x1800c7620`.

Rejected alternative: *arm from `Engine::Startup` and stop arming early.*
`RtSystemAllocatorRegister()` must run inside `RuntimePortInit`, because ~1,180
sites resolve `MemAlloc`/`MemFree` before `GameMain` and a later registration
would leave each of them frozen on a null (`WAVE9_LANE_H.md` §5).

## 3. The real cutover hazard is the `.data` globals, not the allocator

Retail's four bodies write **fifteen absolute `.data` addresses**. With the DLL
absent every one is unmapped, so a literally byte-faithful port would be fifteen
wild writes on the first boot. Handling, per address:

* **Ten** are `?_singleton@…` DATA exports whose owning constructor MSVC inlined
  into `Engine::Startup`. Wave 9 **lane X** ported those constructors and each
  publishes through `RT_DATA_SLOT` (the port-aware door). This body therefore
  **calls the constructor by its exported name** instead of replaying the inlined
  stores. Byte-equivalence was checked per class, e.g. `??0Configuration@Video@@`
  (`0x1800f6f10`) is instruction-for-instruction the block at
  `0x1800dbc2f`-`0x1800dbc87`, singleton store included.
* `0x1801fe720` / `0x1801fe728` + `CALL 0x1800ca860` **are**
  `?Startup@Timer@System@@SA_NXZ` (`0x1800ca7c0` writes the same two constants
  and tail-jumps to the same `PlatformStartup`). Called by name; lane B3 owns the
  storage.
* `0x1801fe748` (`?_singleton@Engine@1@`) and `0x1801fc709`
  (`?_hideDisplay@Error@System@@`) are exported DATA — `RT_DATA_SLOT`.
* `0x1801fc6d0` is **not exported in either build** — the only unnamed address in
  these four bodies. See §6.

### 3.1 Two constructors lane X's fourteen missed — closed here

`System::Configuration` (`??0Configuration@System@@QEAA@XZ`, `0x1800c51b0`,
ord 113) and `Achievement::AchievementManager`
(`??0AchievementManager@Achievement@@QEAA@XZ`, `0x18009eb00`, ord 28) both
publish a singleton and are both inlined at their only call site, which is why a
lane looking for standalone constructor bodies did not see them. Ported and
registered here as lane X's **#15 and #16**.

This is not cosmetic: `?_singleton@AchievementManager@Achievement@@0PEAV12@EA`
has **two live readers in our own tree**
(`code/Game/Game/gameupdateloop_serialize.cpp:189` and
`code/Game/Game/eventlog_achievement.cpp`). Without a ported writer the mint at
cutover hands out storage nothing ever writes, and both readers see null forever.

## 4. Oracle — there isn't one, and this lane will not manufacture one

Each of the four runs **once per process** and each mutates global state a replay
would mutate twice: a second `Startup` creates a second `FileSystem`, a second
`Video::Device` and a second 512 MB arena; a second `Shutdown` double-frees every
subsystem. `RT_ORIGINAL_ONCE` against the shipped body here is not a comparison,
it is a second side effect. So: **shadow post-conditions + did-work counters**,
with the load-bearing terms written by somebody else's code.

| body | shadow terms (the non-tautological ones in bold) |
|---|---|
| ctor | `*?_singleton@Engine@1@ == this`; `_platformHandle == arg`; fields zeroed; **`minBin == 0xbf`, `pageSize == 0`, `ranges == null`** (written by `rtalloc::Construct`) |
| Startup | `_startupComplete == 1`; **`rangeCount >= 1`, `ranges != null`, `pageSize != 0`** (written by `AddNewMemoryRange`); `minRangeSize == (u64)minMB << 20` (the only arithmetic in the function); **all 16 subsystem pointers non-null** (written by the 16 constructors called); singleton still `this` |
| dtor | both singleton slots null |
| Shutdown | phase-1 step count > 0 |

Deliberately **not** asserted: `_allocator.ranges == nullptr` after
`MemoryAllocator::Shutdown`. That body releases the ranges and leaves the pointer
dangling, exactly as retail does; asserting it would be
`reference_oracle_must_not_compare_unwritten_bytes.md` in reverse.

The effectiveness control is one line at exit, always on:

```
[RtEngineLife] ctor=1 startup=1 (failAt=0) dtor=1 shutdown=0 (phase1 steps=0)
```

`failAt` is a small integer naming which of Startup's 13 failure returns fired.

## 5. What I could NOT verify — say it plainly

* **No run of any kind.** Constraint of the lane. Everything below is source
  reasoning plus disassembly.
* **`Engine::Shutdown` has `calls=0` and will stay at 0.** `game_main.cpp`
  resolves `?Shutdown@Engine@1@QEAAXXZ` only to satisfy its own null guard
  (line 426) and then calls its own `ShutdownEngineStaged()` (line 924). Nothing
  in this project calls the real one, so no run this wave can exercise a single
  line of that 225-instruction body. It is ported because a null there is what
  returned 2 out of `RunGameMain`, **not** because it is verified. It is the
  exact shape `reference_uncalled_port_is_not_verified.md` warns about and it is
  shipped anyway, with this notice, because the alternative is no boot at all.
* **Arming this lane disables the allocator shadow driver.**
  `RtAllocShadowInstall` arms by detouring the DLL's `Engine::Startup` at
  `0x1800db8a0`; our port never executes those bytes, so the detour never fires
  and the driver reports `skipped_pre_startup=N`, i.e. **INVALID**. Any
  `BANISHED_RT_ALLOC_SHADOW=1` run must also set `BANISHED_RT_ENGINELIFE=0`.
  Noted in the source at the switch and here.
* **The DLL-present integration run now boots through my `Engine::Startup`.**
  This is the first port in the project on the process's single boot path. If the
  wave's DLL-present numbers move, `BANISHED_RT_ENGINELIFE=0` is the A/B.

## 6. The only guard in the file, and why it is not a "fixed" faithful crash

`0x1800db62d` (inside `Shutdown`) does `FreeLibrary` on a module held in an
**unexported** global at `0x1801fc6d0`. At cutover `ResolveAddr` returns 0 for it
because the module is not mapped, and the step is skipped. That is a genuine
standalone-vs-retail difference — retail always has its own `.data`; with no DLL
that variable *does not exist* — not a defensive null check on retail's logic.
The same applies to `0x180053440`, an unnamed scalar deleting destructor reached
by address. Both are inside the body nothing calls.

Everywhere else the unguarded derefs are kept, including the two that look worst:
`Startup` stores a null allocation into the Engine field and then calls the
subsystem's `Startup` **on the null** (clearest at `0x1800dba7e`/`0x1800dba85`),
and the two `_noConfigure` stores at `0x1800dbcb3`/`0x1800dbcc5` re-read
`_videoConfiguration` and dereference it unguarded.

## 7. Audits — both denominators, same base commit (`8b00995`)

Measured by removing my two new files and stashing my three edits, running, then
restoring. Lane GFX's two untracked files were present for one reading and were
excluded from both (see §9).

| audit | BEFORE | AFTER |
|---|---|---|
| `rawdoor_audit.py` | **481** sites scanned, 0 UNGUARDED-CALL, 34 DEFERRED-CALL | **481**, 0, 34 — *unchanged; this lane opens no raw door* |
| `port_bypass_audit.py --by-name`, ADDRESS door | **196** sites, 0 reach a ported fn; ADDR-REGISTERED=32 DETOURED=3 NO-EXPORT=83 UNPORTED=78 | **198** sites, **0**; 32 / 3 / **85** / 78 |
| … NAME door | **408** raw-door sites, **0** bypass | **408**, **0** |
| … BY-NAME cross-check | **624** ported names, 397 raw-door statements, **0** the parser did not see | **632** ported names (+8), 397, **0** |
| `rt_data_door_audit.py` | PASS; 3 `RT_RAW_OK`; 4 UNCLASSIFIED | PASS; 3; 4 — *unchanged* |
| `runtime_census.py` TOTAL | 4632 total / **847** called / **603** ported / 2 detour / **71.2 %** | 4632 / **848** / **607** / 2 / **71.6 %** |
| … Engine row | 108 / 29 called / **15** ported / **51.7 %** | 108 / 29 / **19** / **65.5 %** |
| … ports with no caller | **63 exports, 60 addresses** | **67 exports, 64 addresses** |

The +2 ADDRESS-door NO-EXPORT rows are `0x180053440` and `0x1801fc6d0`, both §6.

### 7.1 The 4 new "ports with no caller" — triaged, as the census demands

The census says a new name here **has not been triaged**. These four have:

| name | caller | verdict |
|---|---|---|
| `??0Configuration@System@@QEAA@XZ` | direct C++ call from `RtEngine_Engine_Startup`, same file | the accepted category in `WAVE8_5.md` §2 ("exercised by a direct C++ call from a sibling port"); exercised on every boot |
| `??0AchievementManager@Achievement@@QEAA@XZ` | same | same |
| `??2Device@Video@@SAPEAX_K@Z` | same | same |
| `??3Device@Video@@SAXPEAX@Z` | direct C++ call from `RtEngine_Engine_Shutdown` | **weakest of the four**: its only caller is the body with `calls=0`. Its correctness rests on the disassembly alone (it is `MemFree`, verbatim, 20 instructions read in full) |

### 7.2 A census blind spot this lane hit — reported, NOT fixed

`runtime_census.py`'s `called` column is built from
`RE_EXPORT = ResolveExport(Raw)?\s*\(\s*(?=")`. **`RT_EXPORT_ONCE` is invisible
to it** — the macro expands to `RtResolveCached((MANGLED), &cache)`, so no
`ResolveExport("` text exists at the call site. Tree-wide there are **129
`RT_EXPORT_ONCE` sites naming 90 distinct symbols**; **60 of those sites are this
lane's**, so ~69 sites / ~40 names were already invisible before today. My
`Engine::Startup` resolves 30-odd Runtime exports and the census's `called`
column moved by **+1** (the one `ResolveAddr`).

Same family as `reference_census_cannot_see_nonliteral_registrations.md`. **I did
not change the tool** — moving a shared denominator mid-wave is what
`reference_documenting_a_check_can_delete_it.md` is about. It needs a lane.

## 8. Falsifiable predictions for the next DLL-less boot

Run: `BANISHED_RT_NO_DLL=1 BANISHED_RT_MINT=1 BANISHED_RT_STRICT=1
BANISHED_RT_ALLOC=1 BANISHED_AUTOQUIT=1`, DLL renamed aside.

**P1.** These four leave the `[RtUnresolved]` list, and `RunGameMain` no longer
returns 1 or 2 at line 421/426:
`??0Engine@0@QEAA@PEAX@Z`, `??1Engine@0@QEAA@XZ`,
`?Startup@Engine@1@QEAAIPEBDII_N@Z`, `?Shutdown@Engine@1@QEAAXXZ`.
The log must reach `[GameMain] Constructing Engine (storage=...)` and
`[GameMain] Engine::Startup(cmdLine=..., 512, 0, false)...`.

**P2.** The `Engine` **constructor completes**. It has only two dependencies and
both are ported (`??0MemoryAllocator@System@@QEAA@XZ` → lane X →
`rtalloc::Construct`, and the `?_singleton@Engine@1@` data slot). Expect
`[GameMain] Engine constructed.`

**P3.** `Engine::Startup` **will NOT reach its end.** I checked all 65 dependencies
this file resolves against every `RegisterRuntimeExport` literal in the tree:
**26 ported, 39 not.** The first unported one on the path is, in order:

1. `?SetCommandLine@Configuration@System@@QEAAXAEBVString@2@@Z` (step 2, right
   after the arena and the `System::Configuration` ctor)
2. then `?Startup@FileSystem@IO@@QEAA_NXZ`
3. then `?Startup@Persistence@IO@@QEAA_NXZ`, `?RefreshList@Modification@IO@@`,
   `?Startup@Factory@Object@@`, `?PlatformStartup@Application@1@`,
   `?Startup@Configuration@Video@@`, `?EnumerateVideoModes@Device@Video@@`,
   `?Startup@Configuration@Input@@`, `?PlatformConfigure@Engine@1@`,
   `?PlatformStartup@Device@Video@@`, `?PlatformStartup@Device@Audio@@`,
   `?PlatformStartup@Device@Input@@`

So the expected outcome is: the arena is created (`[RtAllocShadow]`-style
`ranges=1`, or `[RtEngineLife] startup=1 failAt=0` **absent**), then
`[RtUnresolved] ?SetCommandLine@Configuration@System@@QEAAXAEBVString@2@@Z`
followed by an **EXEC-of-0** in `standalone_crash.log` whose return address
symbolises inside `RtEngine_Engine_Startup`. **That is progress, not a defect of
this lane** — it is the next row of the same wall, and it is now *named* instead
of anonymous. If instead the boot dies **before** `[GameMain] Engine constructed.`,
this lane is wrong and the ctor is where to look.

**P4.** Three DATA symbols this lane needs are **absent from
`standalone/rt_data_mint_tables.inc`** (grepped by name over the whole
table; 77 of its rows carry a `0x1801f…` Runtime `.data` address):
`?_singleton@Engine@1@0PEAV11@EA`, `?_singleton@Configuration@System@@0PEAV12@EA`,
`?_hideDisplay@Error@System@@0_NA`.
(`?_singleton@AchievementManager@Achievement@@` and
`?_singleton@MemoryAllocator@System@@` **are** minted, lines 1553 and 1567.)
So at cutover those three stores are skipped by the `if (slot)` and each will
appear once on `[RtUnresolved]`. Consequence: `?Get@Engine@1@SAPEAV11@XZ` and
`System::Configuration`'s getter have no singleton. **Lane C2 / the mint owns
this**; it is three rows, and nothing could have asked for them before, because
until this commit nothing in the tree resolved those names.

**P5.** With the DLL **present**, the wave's integration run is unchanged in every
observable except three new lines (`[RtPort]   Engine lifecycle (lane ENG): 8
exports registered …`, the `[RtEngineLife]` summary, and 4 new `[RtDiff]` sites
with `calls=1 divergences=0`). If any *other* `[RtDiff]` site changes, or the boot
regresses, `BANISHED_RT_ENGINELIFE=0` restores the old behaviour exactly.

### 8.1 Lane VID's open question, answered

> *with the DLL gone, nothing in our tree currently starts the video device.*

**My ported `Startup` does start it**, in retail's four steps:
`??2Device@Video@@SAPEAX_K@Z(0x108)` → `??0Device@Video@@QEAA@W4DeviceType@01@@Z`
with **DeviceType 0** (lane X's negative control proved the `CMOVE` publishes
`?_singleton@Device@Video@@` only for type 0, so that argument is load-bearing) →
`?EnumerateVideoModes@Device@Video@@QEAA_NXZ` (result **not** tested by retail) →
later `?PlatformStartup@Device@Video@@AEAA_NXZ`.

But only step 2 is ported (lane X). **`EnumerateVideoModes` and
`PlatformStartup@Device@Video@@` are both unported**, so at cutover the sequence
reaches the device constructor and then dies. So: the *call chain* is no longer
missing, and the *bodies* still are. Lane VID's `CreateVideoContext`
(`0x1800dd500`) is downstream of all of this — it is reached from the `MainLoop`
ctor, which `game_main.cpp` runs **after** `Engine::Startup` returns — so on the
predictions above the boot will not get that far this iteration.

## 9. Base-commit check, and a `git stash` collision to be aware of

* The worktree was created at **`965361d`** — pre-runtime-port, no
  `docs/runtime_port/` at all. It was `git reset --hard`'d to `d382e12` **before**
  any file was read, and later rebased to **`8b00995`** when lane VID's
  `engine.cpp` changes landed. My `engine.cpp` edits and lane VID's coexist
  cleanly (verified: lane VID's own-thunk paragraph and my corrected holdback
  table are both present, and the file compiles).
* **`git stash` is shared across all worktrees of this repo.** While I had a
  stash outstanding, a concurrent `git stash push` from another lane meant my
  `git stash pop` dropped **lane GFX's** stash into my worktree
  (`docs/runtime_port/WAVE9_LANE_GFX.md`, `runtime/src/graphics/wave9_lane_gfx.cpp`).
  I did **not** commit them; copies are preserved at
  `…/scratchpad/laneGFX_rescued/` and were removed from my tree so they could not
  contaminate my audit denominators (they moved `rawdoor_audit` 481 → 483 while
  present). **Lane GFX should check whether its working copy is intact.**

---
---

# Lane ENG, iteration 2 — the Timer regression, and two rows off the boot queue

2026-09-08, on **`bd2fdd0`** (attempt 08's tip; the worktree was rebased onto it and
`git log --oneline -1` confirms iteration 1 is merged in as `e86a5a7`). **Source only**;
`-fsyntax-only` in BOTH the debug and the release (`-DBANISHED_STRIP_LOGS -include
log_strip.h`) configurations, plus a real `-c` + `nm` on each new file. **No new Win32 API
and no new import library** — `-lole32` from iteration 1 is still the only addition this
lane has needed.

Attempt 08 confirmed all four of iteration 1's predictions, including the exact next symbol.
This iteration does three things: fixes the regression that port caused, closes the two rows
the queue reached, and answers both of WAVE9.md's "(?) no writer found" rows.

## 10. The `INT_DIVIDE_BY_ZERO` was a PORT/STORAGE SPLIT, not a missing call

```
Exception 0xC0000094 (INT_DIVIDE_BY_ZERO)   Runtime-x64.dll + 0xDD3F9
1800dd3f7  CQO
1800dd3f9  IDIV qword ptr [0x1801fe730]     ; Timer::_systemTicksPerSecond == 0
```

0x1800dd3f9 is inside `Engine::MainLoop::Run` (0x1800dd3c0 + 0x39), **held back** by Wave 8
lane B and therefore still the DLL's body, reading the DLL's own `.data`.

The coordinator's diagnosis was "your `Startup` does not call `Timer::PlatformStartup`". It
does — `engine_lifecycle.cpp` calls `?Startup@Timer@System@@SA_NXZ`, whose ported body
tail-calls `PlatformStartup`, and **all five of our copies were correct**. The mechanism is
one step further along and it matters, because the fix is different:

> **A ported function that OWNS storage instead of PUBLISHING it into the exported slot is
> invisible until an UNPORTED consumer reads the copy it did not write.**

Lane B3's `Timer` port deliberately owns its five statics (`timer.h` explains why, and it was
the right call — the DLL's `_secondsPerTick` is zero until `Engine::Startup`, and a consumer
that divided by it got a zero). Lane X's fourteen constructors are immune because each
publishes through `RT_DATA_SLOT`. **The `Timer` is the only ported dependency of
`Engine::Startup` that owns rather than publishes — which is exactly why it is the only one
that broke.**

Fix: `PublishTimerStatics()` in `runtime/src/system/timer.cpp`, called from both
`RtSystem_Timer_PlatformStartup` and `RtSystem_Timer_Startup` (the second covers the
QPF-failed arm, where the first returns early having published nothing). It mirrors all five
values into the exported slots through `RT_DATA_SLOT` — the DLL's `.data` today, lane C2's
mint at cutover, **where the same `IDIV` divides by the same zero**, so this was on the
critical path in both worlds. It restores retail's own effect: the same five addresses, the
same five values, at the same moment.

It is fixed in `timer.cpp` rather than in my `Engine::Startup` on purpose — every future
caller of `Timer::Startup` needs it, not just mine. Lane B3 owns that file; the change is one
function and two call sites, all marked.

### 10.1 The sweep the coordinator asked for — "does everything that used to happen still happen?"

Every ported callee of `Engine::Startup`, checked for storage it owns rather than publishes:

| callee | storage | verdict |
|---|---|---|
| `?Startup@MemoryAllocator@System@@` | writes the sub-object at `Engine+0` | not a global — ✓ |
| `?MemAlloc@` / `?MemFree@` | `?_singleton@MemoryAllocator@` via `RT_DATA_SLOT` | publishes — ✓ |
| `?Set@String@System@@`, `??1String@System@@` | none | ✓ |
| **`?Startup@Timer@System@@`** | **five private statics** | **OWNED — the defect, fixed** |
| lane X's 12 constructors | each `?_singleton@…` via `RT_DATA_SLOT` | publishes — ✓ |
| `?Startup@Configuration@Audio@@` | writes only the object passed in (`audio.cpp:677`) | ✓ |
| lane ENG's `Configuration` / `AchievementManager` ctors | `RT_DATA_SLOT` | publishes — ✓ |

**One case in the whole set.** `rtalloc::g_largestRequestedSize` is ours alone (nothing in
retail reads it) and `g_plat` is our platform seam, so neither shadows a DLL global.

## 11. Both WAVE9.md "(?) no writer found" rows are answered

Neither disposition is "dead, leave it zeroed". `tools/rt_mint_writer_audit.py` missed both
for the same reason: the store is inside a function the scan had, but its *value* comes from
an OS call or a helper, so it does not look like an initialiser.

* **`?_systemInitialTick@Timer@System@@0_JA`** — writer is
  `?PlatformStartup@Timer@System@@CA_NXZ` (0x1800ca860), the `QueryPerformanceCounter` at its
  tail, alongside `_systemTicksPerSecond` (QPF) and `_systemSecondsPerTick` (1/QPF). Its only
  DLL-internal caller is `Engine::Startup` (0x1800db9d2). **Load-bearing**, per §10.
* **`?_errorString@Error@System@@`** (0x1801fc720, 0x2000 bytes) — writer is
  `?SetText@Error@System@@SAXPEBD_N@Z` (0x1800c5fa0, ord 3982), and it **APPENDS**: it walks
  to the current NUL (0x1800c5fd4-0x1800c5fe8), computes the room left out of 0x2000, and
  concatenates through a copy helper at 0x1800c9020 — which is why a store scan attributed
  nothing. `code/Game/Game/game_main.cpp:482` READS this buffer to report why `Engine::Startup`
  failed, so zeroed storage is correct only until something calls `SetText`, and then our own
  diagnostic goes quiet. **Disposition: port `SetText`** (54 instructions, one callee).
  **NOT ported here** — it is only on `Engine::Startup`'s failure arm, so by the
  control-flow-reach rule it buys nothing this iteration.

WAVE9.md's table row and its "Open" paragraph are updated in this commit.

## 12. Two rows off the boot queue, in control-flow-reach order

| # | export | retail | ord | instrs | status |
|--:|---|---|--:|--:|---|
| 1 | `?SetCommandLine@Configuration@System@@QEAAXAEBVString@2@@Z` | 0x1800c47a0 | 3849 | 90 | **ported** |
| — | (unexported) the config map's BST find | 0x18002ace0 | — | 50 | **ported**, by address |
| 2 | `?Startup@FileSystem@IO@@QEAA_NXZ` | 0x1800abc50 | 4097 | 38 | **ported** |

Both names checked against the export directory with pefile before a line was written.
Kill switches: `BANISHED_RT_CONFIG=0`, `BANISHED_RT_FSSTARTUP=0` — each separate, because
each sits on the process's single boot path.

### 12.1 Row 1 closes CONDITIONALLY, and the condition is stated

`SetCommandLine`'s three callees — `?ParseCommand@` (ord 3310, 109 instrs), `?ParseValue@`
(ord 3311, 62) and the unexported insert at 0x18002a090 — are **not** ported, so by the
triage rule this closes nothing. It closes anyway because **the loop body never runs on this
project's boots**: `standalone/main.cpp:1930` passes `lpCmdLine ? lpCmdLine : ""`, no harness
here passes an argument (every knob is an env var), and `String::Set(&tmp, "")` yields
`data != null` with `length == 0`, so retail's own `JLE` at 0x1800c47d7 returns before
`ParseCommand`. **Launch `reimpl-x64.exe` with ANY argument at cutover and the next fault is
`?ParseCommand@Configuration@System@@CA?AVString@2@AEAHAEBV32@@Z`.** The loop is written out
faithfully regardless, so the DLL-present path (where all three resolve to the shipped DLL)
is unchanged.

The insert subtree was declined deliberately: 0x18002a090 → 0x18002a170 is a red-black insert
calling a node allocator (0x18000f970) and a rebalance (0x18002a310), none exported. That is
a Collection lane's work with a real oracle available, and the config map is the worst place
in the engine to guess.

The **find** (0x18002ace0) was ported instead, because unlike the insert it has **no callees
at all** — 50 instructions, two inlined `strcmp`s, two pointer follows — and row 2 plus three
sites in `SetupDirectories` call it. Its comparison is written out rather than delegated to
`std::strcmp`, because the C library only specifies the SIGN and this code branches on
`>= 0`: a libc returning an UNSIGNED-char difference takes the other branch on any byte
`>= 0x80`, which a path or a mod name can contain.

### 12.2 Row 2 closes and immediately exposes the next wall

`FileSystem::Startup` is 38 instructions around two calls, and **porting it does not get past
step 3**. It moves the fault to **`?SetupDirectories@FileSystem@IO@@QEAAXXZ`** (0x1800abd00,
ord 4029) — **722 instructions with thirteen distinct callees**, of which
`0x1800ad650`, `0x1800ab180`, `0x1800ac880`, `0x1800881b0`, `0x1800ace00`, `0x1800accc0`,
`0x1800c5300`, `0x1800c5210`, `0x1800c5280`, `0x1800c9730`, `0x1800c6970` and `0x1800cab50`
are unported. `?LoadPackages@FileSystem@IO@@QEAAXXZ` (0x1800ab460, ord 3209) is behind it.
That is a lane, not a tail-end, and naming it precisely is the value this row bought.

The literal at 0x1801556e8 is `onlypkg` (read with `read_memory`): the two gate bytes at
`FileSystem+0x18/+0x19` are that command-line switch. On an empty command line `found` is
false and neither gate is touched by this body beyond the unconditional `+0x18 = 1`.

## 13. Audits — both denominators, base `bd2fdd0`

| audit | BEFORE | AFTER |
|---|---|---|
| `rawdoor_audit.py` | **481** sites, 0 UNGUARDED-CALL, 34 DEFERRED-CALL | **481**, 0, 34 — *unchanged* |
| `port_bypass_audit.py --by-name`, ADDRESS door | **198** sites, 0 reach a ported fn; ADDR-REG=33 DETOUR=3 NO-EXPORT=85 UNPORTED=77 | **200**, **0**; 33 / 3 / **87** / 77 |
| … NAME door | **408** raw-door sites, **0** bypass | **408**, **0** |
| … BY-NAME cross-check | **636** ported names, 397 statements, **0** missed | **638** (+2), 397, **0** |
| `rt_data_door_audit.py` | PASS; 3 `RT_RAW_OK`; **5** UNCLASSIFIED | PASS; 3; **10** |
| `runtime_census.py` TOTAL | 4632 / **848** called / **611** ported / 72.1 % | 4632 / **848** / **612** / **72.2 %** |
| … ports with no caller | **67 exports, 65 addrs** | **69, 67** |

**The two moves that need explaining, not waving away:**

* **UNCLASSIFIED 5 → 10.** All five new rows are `PublishTimerStatics`' `RT_DATA_SLOT` reads
  (`timer.cpp:138/140/142/144/146`). The tool buckets them `via (not a call)` because it
  cannot see through the macro — the **same shape** as the five that were already there
  (`allocator.cpp:649`, `audio.cpp:880`, `dialog.cpp:454`, `input_win.cpp:127`,
  `dynamic_resources.cpp:168`), each of which is a singleton slot reached the same way. The
  audit's actual verdict line is unchanged: **PASS, no minted DATA symbol is read through a
  raw door.** `RT_DATA_SLOT` is the port-aware door; that is the whole point of the fix.
* **`ported` moved by only +1 for 2 registrations.** That is §7.2's blind spot again: the
  census's `called` column cannot see `RT_EXPORT_ONCE`, and both new names are reached from
  `engine_lifecycle.cpp` through it, so they are ported-but-not-`called` and cannot be
  counted. Two new "ports with no caller" rows (+2 exports, +2 addrs) are the same fact seen
  from the other side; both ARE called, on every boot, from `Engine::Startup`.

## 14. Falsifiable predictions for attempt 09

**DLL-PRESENT first, because that is the regression:**

**Q1.** The `INT_DIVIDE_BY_ZERO` at `Runtime-x64.dll + 0xDD3F9` is GONE with
`BANISHED_RT_ENGINELIFE` default-on, and the gameplay cell reaches `GameMain returned 0`
with exit 0 — the same result `BANISHED_RT_ENGINELIFE=0` gives today. If it still divides by
zero, the publish did not reach the DLL's `.data` and `RT_DATA_SLOT` is resolving somewhere
else; check with `BANISHED_RT_TIMER=0`, which removes the port and restores the DLL's own
`Timer::Startup`.

**Q2.** Two new `[RtDiff]` sites appear (`System::Configuration::SetCommandLine`,
`IO::FileSystem::Startup`), each `calls=1 divergences=0`. `SetCommandLine`'s shadow reports
`len=0` and an UNCHANGED map root — if `len` is not 0, something passed a command line and
prediction Q4 applies instead.

**DLL-LESS (`BANISHED_RT_NO_DLL=1 BANISHED_RT_MINT=1 BANISHED_RT_STRICT=1
BANISHED_RT_ALLOC=1 BANISHED_AUTOQUIT=1`):**

**Q3.** `?SetCommandLine@Configuration@System@@QEAAXAEBVString@2@@Z` and
`?Startup@FileSystem@IO@@QEAA_NXZ` LEAVE the `[RtUnresolved]` list. Distance travelled moves
again: past `Engine::Startup` steps 2 and 3.

**Q4.** The next fault is **`[RtUnresolved] ?SetupDirectories@FileSystem@IO@@QEAAXXZ`**
followed by an EXEC-of-0 whose return address symbolises inside **`RtIo_FileSystem_Startup`**
(not `RtEngine_Engine_Startup` — the frame moves one level down, which is itself the check
that row 2 landed). If instead the fault is inside `RtSystem_Configuration_SetCommandLine`,
the command line was NOT empty and §12.1's condition failed; if it is
`?_singleton@Configuration@System@@0PEAV12@EA` on `[RtUnresolved]` with no crash, the mint row
predicted in iteration 1's P4 is still missing and `FileSystem::Startup` skipped its
`onlypkg` lookup (harmless — `found` would have been false anyway).

**Q5.** Four mint rows are still needed and nothing this lane did adds them:
`?_singleton@Engine@1@0PEAV11@EA`, `?_singleton@Configuration@System@@0PEAV12@EA`,
`?_hideDisplay@Error@System@@0_NA` (all from iteration 1's P4) and now
`?_systemSecondsPerTick@Timer@System@@0NA` (0x1801fe740). Each shows once on `[RtUnresolved]`
and its store is skipped. None is fatal on the path reached in attempt 09; the first two
become fatal as soon as anything calls `Engine::Get()` or `Configuration::Get()`.

## 15. What I could not verify, again

No run of any kind. The `Timer` fix is reasoned from the disassembly and the crash address;
**it is the first thing attempt 09 should read**, because a wrong `RT_DATA_SLOT` target
would leave the DLL's `.data` zero and reproduce the identical crash. `Engine::Shutdown`
remains `calls=0` and unverified (§5). `SetText` is identified, not ported.

---
---

# Lane ENG, iteration 3 — three more rows, one of them complete, and a bypass I created and caught

2026-09-08, on **`12901b6`** (attempt 09's tip; `git log --oneline -1` confirms iterations 1
and 2 are merged in). **Source only**; `-fsyntax-only` in BOTH debug and release
(`-DBANISHED_STRIP_LOGS -include log_strip.h`), plus a real `-c` + `nm` on each new file.
**No new Win32 API and no new import library.** Nothing under `runtime/src/io/filesystem*`
was touched — lane FS owns `SetupDirectories` and `LoadPackages`.

## 16. What landed

| # | export | retail | ord | instrs | closes? |
|--:|---|---|--:|--:|---|
| 14 | `?Startup@Configuration@Input@@QEAA_NXZ` | 0x1800e0d30 | 4085 | 43 | **COMPLETELY** |
| 14 | (unexported) `Input::Configuration::Load` | 0x1800e0c80 | — | 25 | — |
| 4 | `?Startup@Persistence@IO@@QEAA_NXZ` | 0x1800b7ee0 | 4103 | 45 | exposes 1 |
| — | `?SetText@Error@System@@SAXPEBD_N@Z` | 0x1800c5fa0 | 3982 | 54 | diagnostic |
| — | `?StrAppend@String@System@@SAPEADPEADHPEBD@Z` | 0x1800c9020 | 4113 | 44 | — |

All five names checked against the export directory with pefile first. Switches:
`BANISHED_RT_INPUTCFGSTART=0`, `BANISHED_RT_PERSSTARTUP=0`, `BANISHED_RT_ERROR=0`.

### 16.1 Step 14 closes COMPLETELY — because I re-checked a two-iteration-old list

`Input::Configuration::Startup` + its unexported `Load` have exactly three callees, and
**all three are already ported**: `MemAlloc` (lane E), `?Load@KeyMap@Input@@QEAAXXZ`
(**lane IOA**, `keymap_load.cpp:303`) and `?Get@Persistence@IO@@QEBA_NPEBD0AEA_N@Z`
(Wave 4 lane C, `persistence.cpp:309`). `KeyMap::Load` was UNPORTED when iteration 1
enumerated the queue; lane IOA landed it since. **Re-checking the stated blocker rather than
trusting my own two-iteration-old list is what turned this from "closes and exposes one row"
into "closes"** — the lesson this wave keeps paying for, in the cheap direction for once.

The four registry literals were read out of the DLL with `read_memory`, not retyped from
`runtime/types/input/Input.h`: 0x180155200 `Input`, 0x180157d30 `MouseClipping`,
0x180157d40 `Legacy`, 0x180157d48 `MouseDisplay`. They agree with the header.

### 16.2 Step 4 closes and exposes exactly one named row

`Persistence::Startup` moves the fault to **`?ReadSettings@Persistence@IO@@QEAAXXZ`**
(0x1800b84b0, ord 3462, 256 instructions — the registry reader), with
`?GetSaveDirectory@FileSystem@IO@@` (lane FS's class) behind it. Both are reached BY NAME, so
each arrives as one `[RtUnresolved]` line rather than an anonymous EXEC-of-0.

The one line a tidy rewrite would drop is retail's self-assignment guard at 0x1800b7f17
(`CMP RCX,[RAX]; JZ skip`): without it, assigning the string to itself would release the
buffer and then AddRef freed memory.

## 17. `SetText` — ported, and it comes with a ⛔ that gates arming it

Taken, on the coordinator's argument: `game_main.cpp:482-497` reads `_errorString` to say
WHICH of the sixteen `Engine::Startup` steps failed, and there are a dozen faults left.

**But `standalone/rt_data_mint_tables.inc:1538` mints `?_errorString@Error@System@@0PADA` as
EIGHT BYTES, and the buffer is 0x2000.** The generator was misled by the mangling — MSVC
decays a `static char[N]` member to `PAD` (`char *`) — and the disassembly is unambiguous:

```
0x1800c5fba  LEA RBP,[0x1801fc720]              ; the ADDRESS, not a pointer load
0x1800c5fc1  MOV EDI,0x2000                     ; the capacity passed to StrAppend
0x1800c5fd4  CMP byte ptr [0x1801fc720],CL      ; byte 0 OF THE BUFFER
0x1800c6034  MOV byte ptr [RAX+RBP*1-1],0       ; a store at RBP + room - 1
```

**With that row as it stands, the first `SetText` after cutover writes up to 8192 bytes into
an 8-byte slot and scribbles over every minted row after `kStorageBytes[16]`.** Fix is one
row in lane C2's file: size `8` → `0x2000`, kind `"char*"` → a byte buffer. Until it lands,
**hold this file with `BANISHED_RT_ERROR=0`** — that switch exists for this and nothing else.
DLL-present is unaffected (the slot is the DLL's own 0x2000 buffer). I did not edit the mint
table: widening a row shifts every `kStorageBytes` offset after it, which is the generator's
business, and a shared table edited by two lanes mid-wave is how denominators rot.

`StrAppend` gets a **real differential oracle** — it is a pure function of (dst, capacity,
src) — with the input buffer snapshotted BEFORE our write, because `dst` is both input and
output (`reference_oracle_must_snapshot_aliased_inputs.md`). Two details preserved that a
rewrite loses: it returns `dst` on the no-room path and `dst + strlen(dst)` otherwise, and
0x18008a290 is **memmove, not memcpy** (its prologue at 0x18008a2a0 computes `RDX - RCX` and
branches to a backward copy at 0x18008a5fa).

## 18. ⚠️ MY OWN PORT CREATED A LIVE BYPASS, AND THE CHECKER CAUGHT IT IN THE SAME COMMIT

Registering `?SetText@` turned `runtime/src/system/allocator.cpp:128` from `OK-UNPORTED` into
a real bypass — `FatalOutOfMemory` held a `RtOriginal<>` raw door on it, correct while
nothing was ported, a registry-bypassing call to the DLL the moment something was:

```
BYPASS   runtime/src/system/allocator.cpp:128  (in FatalOutOfMemory)
    ?SetText@Error@System@@SAXPEBD_N@Z   [live path]
```

Fixed in the same commit: that one door is now `ResolveExport`. **`?Display@Error@System@@`
on the next line stays a raw door on purpose** — it has no port, and converting a door on a
symbol with no port changes only which residue list it prints in
(`reference_converting_a_door_is_not_satisfying_it.md`).

This is worth recording as a rule, because it is the mirror of §10's: **porting a symbol can
CREATE a bypass at a raw door that was correct until that moment.** The door does not change;
the world around it does. `tools/port_bypass_audit.py` is the only instrument that sees it,
and it only sees it if you run it AFTER the registration — which is why the before/after
discipline is not bookkeeping.

## 19. `Engine::Shutdown` — declined again, now with a measurement rather than a shrug

Asked: is there a cheap way to exercise it? **There is a cheap way and it is harmful, so no.**
A lane-X-style probe on a zeroed `Engine` would take every `if (ptr)` false and execute only
the two unconditional tails — about 4 of 225 instructions — and one of them is
`CoUninitialize()` (0x1800db88b), which on a thread that called `CoInitializeEx` exactly once
tears COM down for the process; the other is `MemoryAllocator::Shutdown` on a zeroed
allocator, i.e. `VirtualFree(nullptr)`. **2 % coverage for a process-wide side effect.** The
honest banner stays, and `[RtEngineLife]` keeps saying `shutdown=0` out loud.

## 20. Does the harness ever pass a command line? — no, and here is the search

`?ParseCommand@` goes live the moment anything does. Every launch site in the repo:

| site | command |
|---|---|
| `Makefile:241, 247, 257, 261` | `start reimpl-x64.exe` |
| `tools/rt_run.sh:39` | `env "$@" ./reimpl-x64.exe` — `"$@"` is `VAR=value` env, not argv |
| `tools/stress_run.sh:94` | `./reimpl-x64.exe` |

**No argument anywhere**, and every knob in this project is an environment variable. One
limit I cannot check from here: `.vscode/launch.json` is not present in the worktree, so if a
debug configuration sets `"args"`, that run would enter `ParseCommand`. Worth one `grep` by
someone with the file.

## 21. Audits — both denominators, base `12901b6`

| audit | BEFORE | AFTER |
|---|---|---|
| `rawdoor_audit.py` | **483** sites, 0 UNGUARDED-CALL, 34 DEFERRED-CALL | **483**, 0, 34 — *unchanged* |
| `port_bypass_audit.py`, ADDRESS door | **200** sites, 0 reach a ported fn; ADDR-REG=33 DETOUR=3 NO-EXPORT=87 UNPORTED=77 | **201**, **0**; 33 / 3 / **88** / 77 |
| … NAME door | **410** raw-door sites, **0** bypass | **411**, **0** — *via 1, see §18* |
| … BY-NAME cross-check | **643** ported names, 399 statements, **0** missed | **647** (+4), 400, **0** |
| `rt_data_door_audit.py` | PASS; 3 `RT_RAW_OK`; **10** UNCLASSIFIED | PASS; 3; **12** |
| `runtime_census.py` TOTAL | 4632 / **849** called / **615** ported / 72.4 % | 4632 / **849** / **616** / **72.6 %** |
| … System / Input | 58 / — | **59** / **18 of 22 (81.8 %)** |
| … ports with no caller | **71 exports, 69 addrs** | **74, 72** |

* **UNCLASSIFIED 10 → 12** is exactly `error.cpp:112` and `:115`, the two `RT_DATA_SLOT`
  reads of `?_errorString@` and `?_stopApplication@` — the same `via (not a call)` shape as
  the ten already there. The verdict line is unchanged: **PASS**.
* **`ported` +1 for 5 registrations** — §7.2's blind spot again (`RT_EXPORT_ONCE` is
  invisible to the census's `called` column), and the **+3 "ports with no caller"** rows are
  the same fact from the other side. Triaged: `?StrAppend@` is called by `SetText` in the
  same file (a direct C++ sibling call, the accepted `WAVE8_5.md` §2 category);
  `?Startup@Persistence@IO@@` and `?Startup@Configuration@Input@@` are called on EVERY boot
  from `RtEngine_Engine_Startup` through `RT_EXPORT_ONCE`, which the census cannot see.

## 22. Falsifiable predictions for attempt 10

**DLL-PRESENT:** unchanged, `exit=0`, `GameMain returned 0`. Four new `[RtDiff]` sites
(`System::Error::SetText`, `System::String::StrAppend`, `IO::Persistence::Startup`,
`Input::Configuration::Startup` + `Input::Configuration::Load`), each `calls>=1
divergences=0`. **`StrAppend` is the one to watch** — it has a true byte-for-byte oracle
against the shipped body, so a non-zero `divergences` there is a real port defect, not a
shadow's opinion. If `SetText` reports `calls=0`, nothing failed, which is the good outcome.

**DLL-LESS**, with lane FS's `SetupDirectories` **NOT yet landed** (i.e. attempt 10 run
today): **unchanged from attempt 09** — the fault is still
`[RtUnresolved] ?SetupDirectories@FileSystem@IO@@QEAAXXZ`, EXEC-of-0 inside
`RtIo_FileSystem_Startup`. **None of this iteration's five ports is on the path to that
fault** and I predict no movement in distance travelled. That is the expected result, not a
failure: these rows are paid forward.

**DLL-LESS, once lane FS lands** — this is the prediction that matters, and it names the
symbol, the enclosing function and the frame direction:

1. `Engine::Startup` step 4 runs. `?Startup@Persistence@IO@@` is now ours, so the fault is
   **`[RtUnresolved] ?ReadSettings@Persistence@IO@@QEAAXXZ`**, EXEC-of-0, enclosing function
   **`RtIo_Persistence_Startup`**, and the frame moves **DOWN** again (`RtEngine_Engine_Startup`
   → `RtIo_Persistence_Startup` → fault), exactly as `RtIo_FileSystem_Startup` did in attempt
   09. If the fault is instead inside `RtEngine_Engine_Startup` itself, step 4's port did not
   register — check `BANISHED_RT_PERSSTARTUP`.
2. After `ReadSettings`, step 5 is `?RefreshList@Modification@IO@@QEAAXXZ` (0x1800af5f0,
   **934 instructions**), then step 6 `?Startup@Factory@Object@@QEAA_NXZ` (0x1800c0630, 212).
   **Step 14 will not fault at all** — §16.1.

**Mint rows still owed** (unchanged plus one): `?_singleton@Engine@1@`,
`?_singleton@Configuration@System@@`, `?_hideDisplay@Error@System@@`,
`?_systemSecondsPerTick@Timer@System@@0NA`, and now `?_stopApplication@Error@System@@0_NA`
(not in the table at all). Plus the ⛔ **size fix** for `?_errorString@` in §17, which is a
correctness bug rather than a missing row.

## 23. What I could not verify this iteration

No run. `StrAppend`'s oracle is written but has never executed. The `_errorString` mint-size
defect is read out of the table and the disassembly, not observed as a corruption — it cannot
be, because `SetText` has never run at cutover. `Engine::Shutdown` is still `calls=0`.
