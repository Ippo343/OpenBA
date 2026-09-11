# WAVE 22 LANE TIMER — the five `System::Timer` statics

Branch `wave22/timer`, cut from `wave22/base` (`37706651`). Subject (WAVE22_KICKOFF.md §1):

```
DATA   ?_secondsPerTick@Timer@System@@0NA
DATA   ?_ticksPerSecond@Timer@System@@0_JA
DATA   ?_systemSecondsPerTick@Timer@System@@0NA
DATA   ?_systemTicksPerSecond@Timer@System@@0_JA
DATA   ?_systemInitialTick@Timer@System@@0_JA
```

## 0. Denominator

Examined: all 5 rows, cross-checked three independent ways — (1) a source grep of
`runtime/`, `engine/`, `code/`, `standalone/`, `tools/`; (2) `tools/runtime_census.py`'s
own row classification and caller list; (3) `Runtime-x64.dll` ground truth via GhidraMCP
`get_xrefs_to` on all five retail addresses (every READ/WRITE/DATA ref in the whole DLL,
not a sample); plus the existing `tools/rt_mint_writer_audit.py` cross-check. **Not
checked**: `Runtime-x64-profile.dll` (assumed same layout for this class; not verified
separately — the profile DLL is not the shipped route). Not run: any boot / `sa-run` /
`sa-stress` measurement — this lane is source-and-binary only per WAVE22_PLAN.md §7
("TIMER... binary-and-source work until its verification step... queued behind ALLOC"),
and the verdict below does not need a boot to establish (it is derived from what the DLL's
own machine code does, which a boot would only be reproducing). Not attempted: actually
promoting and booting to *watch* the predicted regression happen — deliberately not done,
per CLAUDE.md's rule against manufacturing a crash to prove a point when the mechanism is
already provable from the disassembly.

## 1. Task 0 — grep result: this cluster is NOT absent, it is fully written

All five symbols already have a complete, byte-faithful port: **`runtime/src/system/timer.cpp`**
(Wave 9 lane B3), 325 lines, values published through `PublishTimerStatics()`, registered
unconditionally by default (`RtSystemTimerRegister()`, gated only by
`BANISHED_RT_TIMER=0` as an opt-*out*, not opt-in — see `rt_system_port.cpp`). The class's
ten function exports (`Startup`, `PlatformStartup`, `Shutdown`, `PlatformShutdown`,
`GetTicks`, `CurrentTime`, `CurrentTimeTicks`, `SecondsToTicks`, `TicksToSeconds`,
operator=) are **all ported and registered**. `tools/runtime_census.py` confirms:
`?SecondsToTicks`, `?Startup@Timer`, `?TicksToSeconds` all read `ported=1`.

The five **DATA** rows are a different animal from the function rows and are why they read
as a gap: nobody calls `RegisterRuntimeExport` on the *mangled data name*. `timer.cpp`
owns C++ globals (`g_ticksPerSecond` etc.) and *publishes* them into the door named by
each mangled string via `RT_DATA_SLOT`, but publishing through a macro door is not the
same as being the thing a `RegisterRuntimeExport` literal names, so
`tools/runtime_census.py`'s `called ∩ registered` intersection never fires for these five
— they show up correctly as `DATA, ported=0` in the census CSV even though the class that
owns them is otherwise fully ported. **This is not a body to write.** All five are already
in `standalone/rt_data_mint_tables.inc`'s `kMintStorage[]` (confirmed by direct grep —
lines ~2255, 2277-2280 in the current tree), with two of them (`_ticksPerSecond`,
`_secondsPerTick`) additionally carrying explicit init-byte payloads (`kInit38`,
`kInit13`) matching the two compile-time constants `timer.cpp`'s header derives from the
disassembly (1000000 and 1e-6). **None of the five is in
`standalone/reg_mint_promoted.cpp`** (grepped; zero matches) — so none is registered
unconditionally as a minted row. That is the entire reason all five still read `ported=0`.

So Task 2 ("mint/publish what is mintable") reduces to one question: **is it safe to add
these five to `reg_mint_promoted.cpp`?** §3 below answers it, and the answer is
route-scoped and, for the DLL-present route, **no**.

## 2. Task 1 — who writes each static, and is it ours

`tools/rt_mint_writer_audit.py` (existing Wave 9 lane C2 tool; scans the DLL's `.text` for
rip-relative stores to each minted row's VA and attributes them to the containing export)
plus direct GhidraMCP `get_xrefs_to` on each retail address:

| row | retail VA | DLL-side writer(s) found by the byte scanner | ground truth (Ghidra xrefs) |
|---|---|---|---|
| `_ticksPerSecond` | `0x1801fe720` | `Engine::Startup` (inlined store) + `Timer::Startup` | WRITE at `1800db9af` (inlined in `Engine::Startup`) and `1800ca7d5` (`Timer::Startup` body, 0x1800ca7c0) |
| `_secondsPerTick` | `0x1801fe728` | `Engine::Startup` (inlined) + `Timer::Startup` | WRITE at `1800db9ca` and `1800ca7e0` |
| `_systemTicksPerSecond` | `0x1801fe730` | `Timer::PlatformStartup` | WRITE at `1800ca899` |
| `_systemInitialTick` | `0x1801fe738` | none found by the byte scanner (see below) | address-of at `1800ca8ba` inside `PlatformStartup` — it is the out-parameter to `QueryPerformanceCounter`, a form the byte scanner (which looks for `MOV [rip+d32], reg/imm`) cannot see; Ghidra correctly classes it `[DATA]` (address taken), not `[WRITE]` |
| `_systemSecondsPerTick` | `0x1801fe740` | `Timer::PlatformStartup` | WRITE at `1800ca8c5`, and **zero other references of any kind in the whole DLL** — this global is write-only in retail; nothing in `Runtime-x64.dll` ever reads it (confirmed: `CurrentTime`'s `(float)(ticks * _secondsPerTick)` uses the *other* constant, not this one) |

**All four retail writer functions (`Engine::Startup`, `Timer::Startup`,
`Timer::PlatformStartup`, and the QPC-out-param inside `PlatformStartup`) are ours.**
`Engine::Startup` is ported unconditionally
(`runtime/src/engine/engine_lifecycle.cpp:1090-1091`, `RegisterRuntimeExport` +
`RegisterRuntimeAddr`, no env gate other than the allocator-platform refusal check) and its
own body calls `Timer::Startup` through `RT_EXPORT_ONCE`
(`engine_lifecycle.cpp:565-572`), which resolves through the port registry to
`RtSystem_Timer_Startup` — **retail's own `Engine::Startup`/`Timer::Startup`/
`Timer::PlatformStartup` bodies are dead code in this build**; they never run once the DLL
is loaded under our port, because the exported names they'd be reached by all resolve to
our functions first.

`tools/rt_mint_writer_audit.py` marks `?_secondsPerTick`/`?_ticksPerSecond`'s writers
`[PORTED]` but marks `PlatformStartup`'s writes for the other three **unmarked** (reads as
"not ported" in that tool's own table). **That reading is a false negative, not a real
gap**, and it is worth recording as a census blind spot rather than re-litigating by hand
each time: `PlatformStartup` is called *directly* from C++ inside `RtSystem_Timer_Startup`
and from `TimerEnsurePlatformStarted()` (`timer.cpp:212,188`) — an ordinary function call,
not a `RT_EXPORT_ONCE`/`RT_DATA_SLOT`/`ResolveAddr` macro door — so
`tools/runtime_census.py`'s row set (built from macro-door literals) never creates a row
for `?PlatformStartup@Timer@System@@CA_NXZ` at all; grepping the census CSV for it returns
nothing. It is registered (`RegisterRuntimeExport` literal at `timer.cpp:318`), it does
run, and it is the thing that actually writes all three QPC-derived statics today — the
audit tool's `[PORTED]` tag just can't see a callee reached by a plain function call. **All
five statics are, in fact, written by our own code on every run where `Engine::Startup`
runs**, which is every standalone run.

## 3. Task 1, continued — the readers, and why that makes promotion route-scoped

The writer side is clean. The **reader** side is where the split-brain risk actually lives,
and it is real. `get_xrefs_to` on all five retail addresses in `Runtime-x64.dll` returns
dozens of READ references from functions that are **not ported**:

- `_ticksPerSecond`/`_systemTicksPerSecond`/`_systemInitialTick`: `SleepThread` (x3),
  `Engine::MainLoop::Draw`, `Engine::MainLoop::Frame`, `Video::Finish`, and the DLL's own
  (unported) `CurrentTime`/`CurrentTimeTicks` bodies. `Engine::MainLoop::Run` also reads
  them, but `Run` **is** ported (`runtime/src/engine/mainloop_run.cpp`, Wave 9 lane ADDR)
  and reads through `ResolveAddr(Module::Runtime, ...)`, not a raw address, so it is not at
  risk — see below.
- `_secondsPerTick`: the widest fan-out of the five — dozens of unported DLL internals
  across Graphics (`ParticleSystem`, `Update`/`Play`/`Blend` for animation), UI
  (`CloseDialog`, `ShowDialog`, `UpdateStandAloneUI`), Save
  (`UpdateSave`/`UpdateLoad`/`UpdateDelete`/`SaveLoad`/`SerializeText`), and
  `Timer::SecondsToTicks`/`TicksToSeconds`'s own retail bodies.
- `_systemSecondsPerTick`: zero readers in the DLL (write-only, per §2).

Every one of those readers is **DLL machine code reading a hard-coded RVA** — `MOV`/`DIVSD`
against `0x1801fe72*` literals baked into `Runtime-x64.dll`'s `.text` at compile time. They
do not go through `ResolveExport`, `RT_DATA_SLOT`, or any door this project's registry can
intercept. The only way their answer changes is if something else writes that same memory
address.

**Today (DLL present, unpromoted), that is exactly what happens, by construction, not by
luck.** `PublishTimerStatics()` writes through `RT_DATA_SLOT`, which calls
`RtResolveCached` → `ResolveExport(mangled)`. Since none of the five names is registered
via `RegisterRuntimeExport`, `ResolveExport` falls through to `GetProcAddress` against the
loaded DLL and returns the **real DLL address** (confirmed: `get_xrefs_to` on all five
shows an `Entry Point [EXTERNAL]` reference, i.e. each is a genuine PE export of
`Runtime-x64.dll`, not merely an internal symbol). So our writer currently writes into the
*same* memory the DLL's dead `Engine::Startup`/`Timer::Startup`/`PlatformStartup` bodies
would have written, and every unported DLL-internal reader above sees a correct,
freshly-written value. This is precisely what `timer.cpp`'s own header describes as
"restores retail's own effect" — verified here against the full DLL xref set rather than
against the one consumer (`MainLoop::Run`) the header names, which turns out to have since
been ported and is no longer the risk.

**Promoting (adding these five to `reg_mint_promoted.cpp`) changes this.**
`RtMintPromotionStorage(symbol, va)` (`standalone/rt_data_mint.cpp:793-814`) always hands
back `r.storage` — a pointer into `kStorageBytes[]`, our own array, **never the real DLL
address**, regardless of whether the DLL is loaded. Once a name is registered that way,
`ResolveExport` finds our registration first (the port registry is consulted before
`GetProcAddress`, project-wide) and returns our storage instead. `PublishTimerStatics()`'s
own `RT_DATA_SLOT` calls would then write into our mint storage — and the real DLL address
those dozens of unported functions still read would **never be written again**, frozen at
whatever it held when `RtMintRegisterPromotedRows()` first ran (0, since the promoted-row
storage starts zeroed). For `_ticksPerSecond`/`_systemTicksPerSecond` that reproduces the
**exact IDIV-by-zero regression `timer.cpp`'s own header documents** (`Exception
0xC0000094` at the DLL's `IDIV qword ptr [_systemTicksPerSecond]`), except the previous
instance of that bug had exactly one unported reader (`MainLoop::Run`, since ported); this
one would hit `SleepThread`, `MainLoop::Draw`, `MainLoop::Frame`, `Video::Finish`, and the
DLL's own `CurrentTime`/`CurrentTimeTicks` on the very next call — i.e., **immediately**,
well before any gameplay-specific code runs, since `SleepThread` fires every idle frame.

## 4. Verdicts, per row, route-scoped

| row | mintable at cutover | promotable today (DLL present) | evidence |
|---|---|---|---|
| `?_ticksPerSecond@Timer@System@@0_JA` (int64) | **yes** | **no** | writer ported (Engine::Startup+Timer::Startup); unported DLL readers (SleepThread, Draw, Frame, Finish, Run's retail body) would read frozen storage |
| `?_secondsPerTick@Timer@System@@0NA` (double) | **yes** | **no** | writer ported; by far the widest unported-reader fan-out (Graphics/UI/Save) |
| `?_systemTicksPerSecond@Timer@System@@0_JA` | **yes** | **no** | writer ported (PlatformStartup, mis-tagged unported by the byte-scan audit — see §2); same unported-reader set as `_ticksPerSecond`; this is the row that already caused a divide-by-zero once |
| `?_systemInitialTick@Timer@System@@0_JA` | **yes** | **no** | writer ported (QPC out-param, invisible to the byte scanner — see §2); same reader set |
| `?_systemSecondsPerTick@Timer@System@@0NA` (double) | **yes, moot** | **no regression either way** | writer ported; **zero readers anywhere in the DLL**, so promoting this one row alone would not itself crash anything — but it would still leave the DLL's copy permanently stale, and there is no reason to promote it alone while its four siblings can't be, since every known consumer of the group reads the group together |

**No row was promoted this lane.** All five are "mintable" in the narrow sense that a
correct minted VALUE exists and our own code is the sole writer that matters — but
`reg_mint_promoted.cpp` has no DLL-presence-aware tier: a row placed there is registered
unconditionally on every run, including today's DLL-present standalone builds, and for
four of these five rows that registration is a same-boot regression, not a latent one.
This is the correct, honest reading of WAVE22_KICKOFF.md rule 9 ("a static retail WRITES
cannot be minted without split brain") *inverted*: here retail's write is already dead and
ours has already replaced it cleanly, but **retail's own unported READERS are the split
brain**, on the far side of the exact same address. The project has no existing name for
that failure shape; recommending it get one is §6.

## 5. What was fixed

`runtime/src/system/timer.cpp:133-135` carried a now-false claim
("`_systemSecondsPerTick` is NOT in `rt_data_mint_tables.inc`") — the row is there today
(the table has grown since Wave 9; the old line-number citations no longer point at
anything meaningful either). Replaced with the corrected status and a forward pointer to
this document, so the next lane that reads this file gets the promotion hazard up front
rather than rediscovering it from a boot log. No functional code changed — comment only.

## 6. What this lane recommends but does not do

**Nothing in the tree currently detects "this row's promotion is safe with the DLL present
but not without it."** `reg_mint_promoted.cpp`'s own warrant process (§ header comment,
items 1-3: `BANISHED_RT_MINT_SHADOW=1` readiness + a real boot + a written warrant) checks
whether the row's *value* is correct; it has no step that asks whether the row's retail
address has *other* readers who are not going through the door the promotion redirects.
That is exactly the same shape of blind spot Wave 22's own ALLOC lane is checking for
gated registrations (§5 of WAVE22_PLAN.md: "nothing detects a GATED port") — this is its
sibling: **nothing detects a PROMOTABLE-LOOKING row whose promotion breaks an unported
DLL-internal reader of the same address.** A mechanical check would be: for a candidate
promotion, enumerate DLL-internal xrefs to the row's VA (exactly what §3 did by hand) and
refuse if any of them lies outside the set of currently-ported functions. Flagging this
for the same instrument-backlog conversation ALLOC's lane raises, rather than building it
under this lane's own scope.

## 7. Checker output

```
$ tools/dispatch_slot_map.py --check
checked 110 ResolveExport registrations, 0 name mismatch(es); 0 address(es)
registered more than once (a non-ResolveExport winner means the checked line is dead code)
```
Unaffected by this lane (no registrations added or changed) — run to confirm the lane's
comment-only edit left the checked surface untouched, per WAVE22_KICKOFF.md's baseline
requirement.

`tools/runtime_census.py --csv` re-run at lane close: unchanged from kickoff for these
five rows (`ported=0`, kind `DATA`, `System` namespace) — expected, since no registration
was added. `tools/rt_mint_writer_audit.py` re-run clean (no crash, negative control still
0 missed).

## 8. What was NOT checked

- No live boot, `sa-run`, or `sa-stress` run — source-and-binary only, per schedule (§0).
- `Runtime-x64-profile.dll` was not independently checked for the same layout; this class
  is assumed unchanged between the shipped and toolkit DLLs, not verified.
- `Application-x64.exe` and `VideoDX11-x64.dll` were not searched for direct references to
  these five addresses — they are Runtime-internal private statics with no import-table
  presence in the exe (unlike the `.rdata` dispatch slots), so an exe-side raw reference
  would have to go through one of the exe's own IAT slots already covered by
  `dispatch_slot_map.py --check`, which stayed green.
- The `[DATA]`-classified reference at `1800ca8ba` (the QPC out-param) was read as "this is
  the write, just in a form the byte-scanner can't see" rather than independently
  disassembled instruction-by-instruction; it is consistent with `timer.cpp`'s own
  transcription of `PlatformStartup` (`QueryPerformanceCounter(&g_systemInitialTick)`), and
  that transcription was treated as authoritative rather than re-derived from scratch.
