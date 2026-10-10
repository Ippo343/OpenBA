# Wave 9 — Lane B3: lane V's REAL DEPENDENCY rows, ported by class

2026-09-07, branch `wave/upgrade-path-complete`. Read [WAVE9.md](WAVE9.md),
[WAVE9_LANE_V.md](WAVE9_LANE_V.md) (the work list) and [PLAN.md](PLAN.md) §4/§6 first.

**This lane wrote code and did not build or run anything**, per its brief — lane R owns the
exe. Every body below was read out of the shipped `Runtime-x64.dll` in Ghidra as
**disassembly, not decompiled C** (`reference_ghidra_c_output_vs_disassembly.md`), by
**address** via `disassemble_function` (never `get_function_metrics`, whose `address`
parameter is broken in this bridge). Every source file was checked with the Makefile's own
flags — `-std=c++17 -Wall -Wextra -Wno-cast-function-type -fno-strict-aliasing` and the full
include set — **zero warnings, zero errors, eleven files**.

## Result in one line

**One class completed (`System::Timer`, all ten exports plus its five data statics), 18
functions ported (10 Timer + 4 String + 3 UI + `Thread::OnCompleted`), 9 REAL DEPENDENCY rows
and 1 DATA READ row closed, 0 new unnamed-internal rows created**, plus one Wave 7 holdback
reclaimed whose stated blocker had been stale since Wave 2, and **two trapped vtable slots
filled by one function**.

All four standing gates pass at the end of the lane: `port_bypass_audit.py` **0 BYPASS on both
doors**, `resolve_fallback_audit.py` **0 sites, exit 0**, `export_name_audit.py` **661 names, 0
phantoms**, `rawdoor_audit.py` **27 UNGUARDED-CALL, down from 31 at lane start**.

## 1. Classes touched — and only one of them is complete

**A first draft of this table claimed four classes complete, and three of those claims were
wrong.** They were made from "every member this lane's rows named is now ported", which is
not the same predicate. Checked properly — the shipped DLL's export directory grouped by
class, against every `RegisterRuntimeExport` in `runtime/src/` — `UI::StringTable` has 27
exports and `UI::NumberEditElement` has 19, of which this tree registers five and three. The
corrected table:

| Class | Ported here | Class complete? |
|---|---|---|
| **`System::Timer`** | `Startup`, `Shutdown`, `PlatformStartup`, `PlatformShutdown`, `GetTicks`, `CurrentTime`, `CurrentTimeTicks`, `SecondsToTicks`, `TicksToSeconds`, `operator=` — **10 of 10 exports** — plus **the five data statics** | **YES**, including its data |
| `System::String` | `ToInteger`, `Release`, `FormatParamCount`, `FormatParamList` | no, but **no raw-door row remains** on the class |
| `UI::StringTable` | `GetIndexFromName` | no — 5 of 27 exports; the ctor/dtor, `Serialize` and the whole `Type`/`CreateNewObject` reflection block are unported |
| `UI::NumberEditElement` | `SetValue` | no — 3 of 19; `FinalizeText`, the ctor/dtor and the reflection block are unported |
| `UI::Element` | `RemoveChildren` | no — `Element` is large; this closes its one raw-door row |
| `System::Thread` | `OnCompleted` | no — but this one function fills slot 1 of **two** minted vtables; see §10 |

PLAN §4.1 decision 5's class-atomicity rule is about **an object our ctor builds carrying our
vtable**, and none of the three partial classes above has a ported ctor, so no object in the
process is half-ours as a result of this lane. `System::Timer` is the one class here with no
instance state and no vtable at all.

New files: `runtime/src/system/timer.cpp`, `runtime/src/system/timer.h`.
Modified: `string.cpp`, `value_types.cpp`, `rt_system_port.cpp`, `application_win.cpp`,
`engine/engine.cpp`, `engine/loops.cpp`, `save/saveload.cpp`, `ui/sprite.cpp`,
`ui/widgets_layout.cpp`, `ui/widgets_input.cpp`.

Census at the end of the lane: **843 called / 589 ported / 69.9%**. The lane brief recorded
843/586, but three other lanes landed during this one, so **the delta is not attributable**
and is not claimed. What *is* attributable is the row list in §2.

## 2. Lane V rows now satisfied — the number that matters for the gate

**Nine REAL DEPENDENCY rows, and one DATA READ row.**

| Row | How it is satisfied |
|---|---|
| `?SecondsToTicks@Timer@System@@SA_JM@Z` | ported; the two live sites (`value_types.cpp:246`, `loops.cpp`) converted `RT_ORIGINAL_ONCE` → `RT_EXPORT_ONCE` |
| `?TicksToSeconds@Timer@System@@SAM_J@Z` | ported; same two sites |
| `?ToInteger@String@System@@SAHPEBD@Z` | ported; `widgets_input.cpp:134`'s accessor `ResolveExportRaw` → `ResolveExport` |
| `?Release@String@System@@AEAAXXZ` | ported, and all three `value_types.cpp` call sites converted — **but see §9: this row's first draft claimed the conversion was done when it was not, and a gate caught it** |
| `?FormatParamCount@String@System@@SAHPEBD@Z` | ported; `string.cpp:709`'s raw call is now a direct same-TU call |
| `?FormatParamList@String@System@@SA?AV12@AEBV12@PEBV12@@Z` | ported; `string.cpp:735` likewise |
| `?GetIndexFromName@StringTable@UI@@QEBAHAEBVString@System@@@Z` | ported; both `sprite.cpp` sites (:581, :629) now call it directly |
| `?RemoveChildren@Element@UI@@QEAAXXZ` | ported; `widgets_layout.cpp:84` |
| `?SetValue@NumberEditElement@UI@@QEAAXH@Z` | ported; `widgets_input.cpp:149` accessor → `ResolveExport` |
| `?_secondsPerTick@Timer@System@@0NA` (**DATA READ**, lane V §4's "lane C2 must add these six") | **closed by a port, not a mint** — see §3 |

`tools/rawdoor_audit.py` goes **31 → 27 UNGUARDED-CALL** over the lane (557 → 553 sites
scanned). The audit only sees resolve-and-call-in-one-expression, so it does not see the six
accessor-shaped rows above; the row table is the measurement, the audit is the cross-check.

**Zero new unnamed-internal (lane D) rows.** Every callee of every body here is either in the
same translation unit, already ported, or an ordinary export. This was a selection criterion,
following WAVE9.md's "prefer ports whose callees are already ours" — `IO::Persistence::Set`
and the `Video::*Buffer::Platform*` family were **declined partly because they are not**
(§4).

## 3. A correction to lane V, and why `?_secondsPerTick` is a port and not a mint

WAVE9_LANE_V.md §4 lists `?_secondsPerTick@Timer@System@@0NA` among the six data rows lane
C2 must add, with this reason:

> `?_secondsPerTick` is the awkward one: it is written at runtime by `System::Timer`'s init
> from `QueryPerformanceFrequency`, so minting a *value* for it is wrong; the port has to own
> the initialisation.

**The conclusion is right and the reason is wrong**, and the difference decides who does the
work. `Timer::Startup` (0x1800ca7c0) contains no QPC at all:

```
MOV   EAX,0xf4240              ; 1,000,000
MOVSD XMM1,[0x1801616d0]       ; 1.0   (read: 00 00 00 00 00 00 f0 3f)
CVTSI2SD XMM0,RAX
MOV   [0x1801fe720],RAX        ; _ticksPerSecond = 1000000
DIVSD XMM1,XMM0
MOVSD [0x1801fe728],XMM1       ; _secondsPerTick = 1e-6
JMP   0x1800ca860              ; tail into PlatformStartup
```

The engine tick is a fixed microsecond. `_secondsPerTick` and `_ticksPerSecond` are
compile-time constants written by a store; it is `_systemTicksPerSecond`,
`_systemSecondsPerTick` and `_systemInitialTick` that are QPC-derived, and those belong to
`PlatformStartup`. So `SecondsToTicks`/`TicksToSeconds` — the two hot rows — depend on a
constant and carry **no ordering hazard at all**.

**The ordering fact that IS load-bearing, and that neither lane had:**
`?Startup@Timer@System@@SA_NXZ` has **no internal caller in the shipped DLL** (`get_xrefs_to`:
the export directory and the entry point, nothing else). `Engine::Startup` (0x1800db8a0)
**inlines the body** — 0x1800db9aa..0x1800db9ca writes the same two constants to the same two
globals itself, then makes a direct `CALL 0x1800ca860`. A direct internal call cannot reach
the port registry, and `Engine::Startup` is lane E's standing NO-GO. **Nothing would ever
have initialised our copies**, so this port initialises the two constants at namespace scope
and lets `PlatformStartup` own the three QPC values, with an idempotent ensure-started on the
two absolute-time readers.

`runtime/src/system/timer.h` exposes all five, and the four consumers that were reading the
DLL's slots through the raw data door now read ours:
`application_win.cpp` (all four slots, in `Application::SleepThread`), `engine/engine.cpp:217`
and `save/saveload.cpp:163`. Two of those are also **strictly safer**: the DLL's slot is zero
until `Engine::Startup` runs, and `saveload.cpp`'s helper *divided by it* behind a
`? *p : 0.0`.

## 4. Deferrals, each with its reason

Nothing here is "ran out of time"; each is a named obstacle.

| Item | Reason |
|---|---|
| `?PlatformConstruct@{Image,Index,Vertex}Buffer`, `?PlatformDestruct@ImageBuffer` (4 rows) | **Lane V's row text says "port or stub … (dx11 factory call)". They are not trampolines.** 0x1800f2f40 / 0x1800f3020 take a **critical section** (`EnterCriticalSection` 0x18014b390 / `LeaveCriticalSection` 0x18014b388, owner block at `0x1801fe7a8 + 0x60`, with a `GetCurrentThreadId` recursion counter at +0x88/+0x8c) and splice the buffer into a **process-global intrusive doubly-linked list** headed at `0x1801fe780`. Porting one mutator while the DLL's others exist splits one list across two heads. They need two data slots minted (lane C2) and the lock (lane E), and per WAVE9.md §2 **no diff site may go in them** because they can run off the main thread. |
| `?CacheText@TextElement@UI@@AEAAXXZ` | 112 instructions with its own helper chain (0x18011e8e0). **Its stated blocker is now dissolved** — Wave 7 named `String::FormatParamList`, which this lane ported — so it is ordinary work for the next lane, not a holdback. |
| `?DateAsString@DateTime` / `?TimeAsString@DateTime` | Need `System::WideString` (`GetDateFormatW` at 0x18014b130 into a wide buffer, then 0x1800caf40/0x1800cacd0 to convert), which no wave has touched. Also **note for whoever does port them: `this` is never read** (`MOV RBX,RDX` takes the return slot; RCX is dead) — they format the *current* time, so an ours-then-theirs oracle has a genuine flake window at a second boundary and needs a clock freeze, not just a replay. |
| `?Set@Persistence@IO@@QEAAXPEBD0H@Z` | Its lookup is `0x18002a090`, an unexported `Collection::Set<Pair<String,String>>` internal — porting it would **create a lane D row**, the one bucket with no export name. Same class that blocks `Input::KeyMap::Load` (WAVE9.md row 11). |
| `IO::MemoryStream` / `IO::CompressedStream` ctor/dtor/`Open` (7 rows) | `CompressedStream` is a 20-member class carrying the codec (`Commit`, `CompressWindow`, `DecompressFromSource`, `SetWindow`, `System::Compression`); `compressedstream.cpp`'s header argues at length that the compressed byte format is bit-identical *because* none of it is reimplemented. That argument has to be replaced, not deleted, and it is a lane. |
| `?GetDataDirectory@FileSystem`, `?PersistObject@InternalFactory`, `?FindFree3DEffect@Scene`, `?Stop@Device@Audio`, `Save::SaveLoad::Update*`, `Graphics::AnimationState::*`, `Geometry::SetBounds` | Not reached this session. `?Stop@Device@Audio` is WAVE8_5's stored-completion-callback root cause and should not be taken without the device fake. |

## 5. Oracles, per function, with the reaching path

**I cannot build or run**, so each row names the command and the harness path that would
invoke it. `calls=0` is not verification (`reference_uncalled_port_is_not_verified.md`), so
the last column says plainly which rows will not have one.

Command for every row below: `BANISHED_RT_DIFF=1` on the named harness cell, then read
`standalone_debug.log` for the site name.

| Function | Oracle | Reaching path |
|---|---|---|
| `Timer::SecondsToTicks` | **full diff**, `RT_DIFF_SCALAR`, exact | `make sa-run` — `System::Time`'s ctor, i.e. every `Update(const Time&)`. Millions of calls. |
| `Timer::TicksToSeconds` | **full diff**, exact float compare (no tolerance — a byte-faithful float routine must reproduce the rounding) | same; also `Time::AsSeconds` and `loops.cpp:826`'s round-trip site, which now tests **ours against theirs** rather than DLL-against-DLL (comment corrected in place) |
| `String::FormatParamCount` | **full diff** — pure over a `const char*` | `make sa-run` — every `String::FormatIndexed`, i.e. every parameterised UI string |
| `String::FormatParamList` | **full diff**, replayed into an independent result; inputs are const and the AddRef/Release pair cancels. Guarded against `ret` aliasing an input (`reference_oracle_must_snapshot_aliased_inputs.md`) | same |
| `String::ToInteger` | **full diff** — pure | `BANISHED_AUTOMENU_BUTTON=7` / any `NumberEditElement` edit; also `EditElement::GetValue` |
| `StringTable::GetIndexFromName` | **full diff** — `this` and the key are const, nothing allocates | `make sa-run-menu` — every `UI::String(table,name)` construction during dialog build |
| `Element::RemoveChildren` | **NO ORACLE**, and intrinsically so: it *destroys* the children, so a second run frees a freed list, and "a copy of the state" is not constructible — the children are separately allocated objects with their own destructors. Post-condition shadow only (array empty). | `BANISHED_AUTOMENU_BUTTON=7 BANISHED_AUTOMENU_CLICKS=...` — dialog teardown |
| `String::Release` | **NO ORACLE**, intrinsic: the refcount is in the one shared block, so a replay decrements twice and frees a live string. A second `String` referring to the same block *is* the same counter. Post-condition shadow only. | `make sa-run` — constant |
| `NumberEditElement::SetValue` | **NO ORACLE** (SetText mutates the live element and rebuilds its sprite text). **Execution witness only** — a shadow here would have been a second spelling of the clamp, and two spellings of one expression agree by construction: N green rows that did no work (`reference_probe_needs_an_effectiveness_control.md`). Read its `calls=`, not a verdict. | `BANISHED_PAUSEMENU_BUTTON=…` — any spinner or number field |
| `Timer::CurrentTime`, `CurrentTimeTicks` | **NO ORACLE and calls=0** — see §6 | none |
| `Timer::Startup`, `Shutdown`, `PlatformStartup`, `PlatformShutdown`, `GetTicks`, `operator=` | no site. `Shutdown`/`PlatformShutdown` are a bare `RET` in the shipped build and `operator=` is `MOV RAX,RCX; RET`, so three of the six have no behaviour to verify; the other three are **unverified**, registered so the names are not missing at cutover | none |

## 6. Two ports land with `calls=0`, said out loud

`Timer::CurrentTime` and `Timer::CurrentTimeTicks` are registered and **unverified**, and the
reason is worth recording because it is a property of the function:

* They return an offset from an **epoch** — a `QueryPerformanceCounter` sample taken at
  `PlatformStartup`. Ours and the DLL's are two different samples at two different moments,
  so ours-vs-theirs differs by the interval between them, forever, and that difference is
  *correct*. There is nothing for a differential oracle to say.
* The epoch-mixing hazard that implies is bounded by measurement rather than argument: **no
  file under `code/`, `runtime/`, `engine/`, `standalone/` or `dx11/` names
  `?CurrentTime@Timer` or `?CurrentTimeTicks@Timer` at all** (swept 2026-09-07). No ported
  caller can observe our epoch. The DLL's own consumers (`MainLoop::Run`/`Frame`/`Draw`,
  `Application::SleepThread`, `Video::Finish`) read the globals directly and stay wholly on
  theirs.
* At cutover `Engine::Startup` is ours, calls our `PlatformStartup`, and there is one epoch
  again.

`Application::SleepThread` is the one ported body that now measures against **our** epoch,
and it is safe for a reason and not by luck: it only ever subtracts two readings taken inside
the same call, so a shifted origin cancels exactly.

## 7. Comments and banners corrected in place

Per the lane brief's rule that a registration which makes a comment false must fix it:

1. `system/value_types.cpp` — "Timer is out of this lane's scope; its `_secondsPerTick` is a
   runtime QPC-derived global, not a constant we can hardcode". Both halves wrong; replaced
   with the disassembly and the `RT_EXPORT_ONCE` reason.
2. `engine/loops.cpp:358` — "**Another lane** has since PORTED both". No registration of
   either name existed anywhere in the tree when that was written (`grep` over `code/`,
   `runtime/`, `engine/`, `standalone/`, `dx11/`). It is true *now*, because of this lane;
   named accordingly.
3. `engine/loops.cpp:827` — "Both sides of this comparison call the DLL". No longer true: the
   left side is our Timer, so the site now tests the composition **and** the two conversions.
4. `system/string.cpp:690` — "FormatParamCount and FormatParamList are NOT in this lane's
   scope and are left on the DLL".
5. `ui/sprite.cpp:570` — "GetIndexFromName is NOT in this lane's scope … calling the DLL's
   own copy here costs nothing".
6. `ui/widgets_layout.cpp:79` — "shared Element/ContainerElement behaviour this lane has no
   business reimplementing".
7. `ui/widgets_input.cpp:141` — the `NumberEditElement::SetValue` holdback. **The stated
   blocker was stale by seven waves**: Wave 7 lane D held it back over "the variadic
   `System::String::Format`, which this lane declined to reproduce", and
   `?Format@String@System@@SA?AV12@PEBDZZ` has been registered in `string.cpp` since Wave 2.
   Fourth instance of `reference_holdback_blockers_are_stale_check_them_first.md` in this
   wave.

## 8. Two things the next lane needs, that are not mine to fix

* **`tools/rawdoor_audit.py --self-test` now FAILS**, and it is right to. It asserts the
  checker still finds the two WAVE9_LANE_V.md §1d defects (`audio.cpp` `?IsPaused`,
  `widgets_layout.cpp` `?GetSprite`); **both have since been repaired** — the calls are now
  inside `if (rt_diff_on__)`, verified by reading both sites. The self-test needs inverting
  to assert they are gone, plus a synthetic positive case so it keeps its effectiveness
  control. Those files are lane R's; I did not touch them.
* **A ported function's callees are the thing to check before choosing it.** Two of my five
  deferrals are deferrals *because* the callee would have been an unnamed internal or a
  shared global list, not because the body was hard. WAVE9.md's "prefer ports whose callees
  are already ours" is the most actionable sentence in the document and deserves to be a
  selection rule for lane B and B2 as well, not just a note.

## 9. A regression this lane introduced, what caught it, and what did not

`tools/port_bypass_audit.py` reported **1 BYPASS** after my first round:
`value_types.cpp` reaching `?Release@String@System@@AEAAXXZ` through `RtOriginal` on a live
path, i.e. running the DLL's body while my port sat registered, counted by the census and
reported clean by the oracle. **Not an oracle readback** — it releases a temporary the
function itself built, so the call is the work; converted to `RT_EXPORT_ONCE`, which is
cached exactly as `RT_ORIGINAL_ONCE` is and so costs nothing.

Two things about it are worth more than the fix.

**First, my own §2 said the conversion was already done.** The row read "ported;
`value_types.cpp`'s three raw calls now reach it through the registry". It was written from
the intent, not from the file. Porting a function and converting its callers are two edits,
and I did one — which is the same failure the wave has now recorded sixteen times, arrived at
from the other direction.

**Second, and the reusable part: the gate could only see one of the three.** `value_types.cpp`
had three raw `Release` sites. Only the one that resolves and calls **in a single expression**
was reported; the other two store the pointer in a local and call it on the next line, which
the checker classifies `OK-RESOLVE-ONLY`. That is the same blind spot WAVE9_LANE_V.md §7
states for `rawdoor_audit.py`, now demonstrated on `port_bypass_audit.py` as well:

> The far more common pattern — resolve into a cached accessor, call it from elsewhere — is
> invisible to it.

So **a clean `port_bypass_audit.py` is a floor, not a ceiling.** After porting a name, grep
the tree for the *mangled string* and read every hit, because the accessor-shaped ones will
not come to you. Doing that on this lane's nine ported names is what found the other two, and
also `widgets_input.cpp`'s `StringToInteger()` accessor and
`RealNumberEditElementSetValue()`, neither of which any checker reported. The `OK-RESOLVE-ONLY`
bucket went 10 → 8 as a result.

## 10. `Thread::OnCompleted` — one instruction, two trapped vtable slots

`?OnCompleted@Thread@System@@UEAAXXZ` (0x18000c590) is a bare `RET`: the base class's
do-nothing completion hook, dispatched from `ThreadFunc`'s slot 1. Ported and registered
because it fills slot 1 of **both** `??_7Thread@System@@` (0x180155f20, `[_purecall,
OnCompleted]`) and `??_7Loader@Engine@@6B@` (0x180157a08), which inherits it — one function,
two traps, and lane C2's mint picks it up with no regeneration.

**No diff site, as a rule rather than a judgement.** It runs on a worker thread, and
`runtime/funcs/rt_diff.cpp` has zero synchronisation primitives, so a site there would race in
every worker (WAVE9.md §2). It carries lane P's opt-in execution witness
(`BANISHED_RT_THREADFUNC_TRACE=1`, one line on first dispatch) instead, and the property that
makes that honest holds here too: **zero lines means zero dispatches.** There is also nothing
for an oracle to compare — the shipped body is `RET`, so ours and theirs agree on every input
by construction and a green row would be a no-op.

### `?Run@Loader@Engine@@UEAAXXZ` — DECLINED, and the cost of taking it, measured

Porting it would complete `??_7Loader` outright, which is the argument for it. Against it,
from the disassembly at 0x1800dbfc0:

* **No oracle is possible.** It runs on the worker thread (so no diff site, per above) *and*
  it mutates a shared singleton — it writes `ExternalFactory::_singleton + 0x41` and `+ 0x00`
  around the load and restores them after. Running ours then theirs would run the load twice
  against one factory.
* **It writes two fields nothing in the tree has established.** `+0x41` (a byte flag) and
  `+0x00` (an int copied from `UpdateLoop + 0x20`) are not in `runtime/types/object/External.h`.
  It also reads `Engine::_singleton + 0x6c0` and that object's `+0x100`, neither documented.
  Four undocumented offsets on two process-wide singletons, with no way to check them.
* **Its one real callee is a WAVE9.md holdback.** `0x1800dd7d0` is
  `?LoadResources@UpdateLoop@Engine@@QEAAXXZ` — row 18 of the triage table, "PORT after the IO
  holdbacks". Calling it by name is legitimate, but the ordering says Loader follows it.
* It is **not exercised by default**: lane C2 gated `ConstructLoader()` behind
  `BANISHED_RT_MINT_CTOR=1` because it starts a real OS thread and three events.

The good news, and the correction to my own instinct on first reading it: **all three of its
callees are named exports**, not `FUN_` internals — `?LoadResources@UpdateLoop@Engine@@`,
`?_singleton@Engine@1@0PEAV11@EA` and `?_singleton@ExternalFactory@Object@@0PEAV12@EA`. So
porting it creates **no lane D row**, only two DATA rows and four offsets that need
establishing. It is a real, bounded piece of work for a lane that can drive
`BANISHED_RT_MINT_CTOR=1` and watch it — not something to land unverified into a vtable that
nothing has exercised yet.
