# Wave 9 — Lane 0: the residue matrix

**Question the lane answers:** *what does `reimpl-x64.exe` still ASK `Runtime-x64.dll`
for, across everything the harness can drive?* Not what the tree mentions — what the
process reaches.

Nine runs, all exited cleanly — `DumpRuntimeResidue()` runs after `RunGameMain` returns
(`standalone/main.cpp:1847`), so a killed run reports nothing and every number below comes
from a run that quit through the game's own path.

**The whole matrix was run twice, and the numbers below are from the second, pinned pass.**

| Pass | Source | Debug exe (cells 1–8) | Perf exe (cell 9) |
|---|---|---|---|
| 1 | cells 1–8: `e9d81d9`. Cell 9: `e9d81d9` **+ the one-token `std::printf` fix** — see below | built 19:17, deployed 20:12:54, cells ran 20:13–20:17 | built 20:18, cell ran 20:19 |
| 2 (**the one reported**) | `357221a` + the strict-mode change below | built **20:36:53**, cells ran 20:37–20:41 | built **20:42:40**, cell ran 20:42 |

Pass 1 is superseded for two independent reasons, both raised by the integrator while it
was in flight, and both are why pass 2 exists:

**A correction from the integrator, and it is the same point I was pressed on.** This
document first said pass 1 ran "an exe built from `e9d81d9`". That cannot be true of the
stress cell: `e9d81d9` **does not compile** for log-stripped targets (tool defect 1), so
the perf exe necessarily contains the `std::printf` fix and is `e9d81d9`+1 token. The two
halves of pass 1 were therefore built from *different* source states — exactly the thing
I was warning about elsewhere in this file. It changes no number (the fix is a log line in
a path none of these runs execute), and that is the point: an attribution is either
accurate or it is not, and "close enough to change nothing" is a judgement made after
knowing the answer.

Two further honesty notes on pass 1, since it is being audited: cells 1–8 ran a debug exe
built at 19:17, **before this session opened**, and I attribute it to `e9d81d9` because
that was HEAD with a clean tree at session start — attribution by circumstance, not a
measurement of what that compiler ran. Pass 2 has no such gap: I built both exes myself,
from a state I recorded.

* **Attribution.** Pass 1 straddled lane E's landing (`InstallAllocatorLock` /
  `InstallFreeLock` moved to *before* `RuntimePortInit`, `standalone-debug` rebuilt at
  20:23:47). Every pass-1 cell ran against the deployed file stamped 20:12:54 and
  `tools/rt_run.sh` refuses to launch a second instance, so no cell can have straddled the
  rebuild — but **I did not record the deployed stamp per cell at the time**, only for the
  first three runs, and reconstructing the rest from the deploy interval is inference, not
  measurement. Pass 2 records it per cell (all nine identical, shown above).
* **The instrument itself was incomplete** — see "The instrument had three blind doors".

**Does the allocator-lock reorder change the residue? Measured, not assumed: no.** All nine
pass-2 port-residue lists are **byte-identical sets** to their pass-1 counterparts (258,
306, 320, 314, 320, 318, 313, 263, 312 — same counts *and* same members, checked with
`git show 357221a:<file>`). A reorder that moves code across `RuntimePortInit` plausibly
could have moved a resolve; it did not.

## The numbers

| # | Cell | How | Exe (pass 2) | Port-registry | *Raw* doors |
|---|---|---|---|---|---|
| 1 | menu | `BANISHED_AUTOQUIT=1` (front-end menu → Quit) | 20:36:53 | **258** | 55 |
| 2 | gameplay | `BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=120` | 20:36:53 | **306** | 77 |
| 3 | Save9 autoload | `BANISHED_AUTOLOAD=Save9` + pause-menu quit, **.pkm mods mounted** | 20:36:53 | **320** | 91 |
| 4a | tutorial 0 | `BANISHED_AUTOTUTORIAL=1` + pause-menu quit | 20:36:53 | **314** | 85 |
| 4b | tutorial 1 | `BANISHED_AUTOTUTORIAL=01` | 20:36:53 | **320** | 85 |
| 4c | tutorial 2 | `BANISHED_AUTOTUTORIAL=2` | 20:36:53 | **318** | 85 |
| 4d | tutorial 3 | `BANISHED_AUTOTUTORIAL=3` | 20:36:53 | **313** | 84 |
| 5 | mods menu | `BANISHED_AUTOMENU_BUTTON=7 BANISHED_AUTOMENU_CLICKS=1001,5001,d0,4001,101,m6 BANISHED_MODAL_ANSWER=1` | 20:36:53 | **263** | 59 |
| 6 | sa-stress | `BANISHED_RT_STRICT=1 make sa-stress STRESS_ARGS="--frames 1200"` (**perf exe**) | 20:42:40 | **312** | 83 |

The *Raw* column did not exist in pass 1 — those doors were dark. See "The instrument had
three blind doors".

**Union: 333 distinct requests**, or **316 after folding** — 62 of the `addr:0x1800…`
keys are addresses the shipped DLL *exports*, so `addr:0x1800b9960` and
`??0Serializer@IO@@QEAA@AEAVStream@1@H@Z` are one function asked through two doors.
Every count in the delta section below is on the folded set.

All six matrix cells ran. Nothing was dropped.

Raw per-cell files: `docs/runtime_port/wave9_residue_{menu,gameplay_pause,save9,
tutorial1,tutorial01,tutorial2,tutorial3,modsmenu,stress}.txt` (the `[RtResidue]` block
straight out of `standalone_error.log`, no filtering, no `tail`). Union with symbolised
requesting return addresses: `docs/runtime_port/wave9_residue_union.txt`.

## Monotonicity: what actually holds

The lane brief said residue entries are monotone in coverage, so a *shrink* is a tool bug.
That is true **only between nested paths**, and the matrix is mostly not nested. Measured
containment:

* `menu` ⊆ **every** other cell. That is the real monotonicity check and it passes: 0 lost
  entries in 8 comparisons.
* `gameplay` ⊆ `stress`.
* `tut0` ⊆ `tut1`, `tut2`; `tut3` ⊆ `tut1`, `tut2`; `tut2` ⊆ `tut1`.
* `save9`, `tut1`, `mods` and `stress` are pairwise incomparable. `save9` misses 6 entries
  `gameplay` has (`??0`/`??1RenderBuffer@Video`, `Achievement::CreateInstance`,
  `?_typeInfo@Achievement`, two internals) because a loaded world never runs the
  new-game path, and `gameplay` misses 20 that `save9` has. **Neither is a tool bug**;
  they are sibling paths, not a run and its superset. Anyone re-running this matrix should
  compare against `menu`, not against whichever cell ran last.

**Only 3 of the 316 are reached by a single cell**: `addr:0x18002fea0` and
`addr:0x1800764c0` (save9 only), `addr:0x1800136f0` (stress only —
`Array<GraphicsMesh::TriangleList>::SetSize`, `runtime/src/graphics/mesh.cpp:201`).
The matrix is far more redundant than expected, which is the useful negative result:
**a cheap 2-cell matrix (menu + one deep run) would have found 313 of 316.** The
expensive part of this lane bought 3 rows and the containment facts.

A free control fell out of a failed build: cell 1 was run once on the **debug** exe and
once on the **perf** exe (the `sa-deploy` that should have put the debug exe back had
failed — see tool defect 1). Both produced **258, identical sets.** The residue is a
property of the source, not of the build.

## Reached vs. the static census

`docs/runtime_port/wave9_unported_20260907.txt` lists **320** called-but-not-ported items.
The union reaches **316**. They overlap in **274**.

| | Count | Meaning |
|---|---|---|
| reached **and** on the static list | **274** | the real cutover work list |
| **static-only** — listed unported, not reached by a port-registry door | **46** | of which **14 were reached through a *Raw* door** (see below), leaving **32** genuinely unreached |
| **residue-only** — reached, not on the list | **42** | 39 census blind spots + **3 live bypasses** |

### The 3 live bypasses — the finding other lanes need

Three functions the census calls **PORTED** were asked of the DLL anyway, in every cell:

| Function | Asked from | Ported in |
|---|---|---|
| `??0Serializer@IO@@QEAA@AEAVStream@1@H@Z` (`addr:0x1800b9960`) | `code/Game/Tutorial/tutorialgoaldialog.cpp:40` | `runtime/src/io/serializer.cpp:636` |
| `??1Serializer@IO@@QEAA@XZ` (`addr:0x1800b99b0`) | `code/Game/Tutorial/tutorialgoaldialog.cpp:41` | `runtime/src/io/serializer.cpp` |
| `?RandFloat@Random@System@@QEAAMXZ` (`addr:0x1800c7d40`) | `code/Game/Component/herdcomponent.cpp:80` | Wave 2 System |

**Cause:** `ResolveAddr(Module::Runtime, …)` consults `RtAddrReg()` only
(`standalone/resolve_standalone.cpp:323`). A port registered with
`RegisterRuntimeExport(name, fn)` is invisible to a caller that asks by ADDRESS. So a
call site written before the port landed silently keeps using the DLL, the differential
oracle never sees those calls, and the census — which folds address→name through the
export directory — reports the function as ported.

This is `reference_uncalled_port_is_not_verified.md` wearing its third hat: not "a port
with no callers", but **a port whose callers go around it**. At cutover these are three
missing symbols that every progress number says are done.

Cheap fixes, in preference order: (a) make `ResolveAddr(Module::Runtime, …)` fall back to
the export map (address → exported name → `RtExportReg`) — one lookup, fixes the whole
class including any future site; (b) change the three call sites to `ResolveExport`.
Not done here: lane 0 does not port and does not edit the seam.

### The 39 census blind spots (a defect report for `tools/runtime_census.py`)

Reached in 7–9 cells each, and absent from the census entirely. Four source idioms, all
of which defeat `RE_EXPORT` / `RE_ADDR` because both regexes require the literal **at the
call site**:

| Idiom | Example | Hidden |
|---|---|---|
| a template/function wrapper around `ResolveExport` | `ResolveFn<void(*)(void*)>("?ClosePackages@FileSystem@IO@@QEAAXXZ")`, `code/Game/Game/game_main.cpp:402` | 5 (`ClosePackages`, three `PlatformShutdown`, `AchievementManager::RemoveAll`) |
| a named `constexpr const char* kSym_…` fed to that wrapper | `code/Game/Dialog/mod_dialog_vtables.cpp:89` | 4 `DialogInstance` slots |
| a table walked in a loop: `ResolveExport(s.mangled)` | `standalone/reg_element_typeids.cpp:143` | 19 UI `?_typeInfo@…` + `??_7ElementDescription@UI` |
| a template wrapper around `ResolveAddr`, or a named address constant | `RtAddr<ArrayCopyFn>(0x180014c70)`, `runtime/src/graphics/mesh.cpp:183`; `ResolveAddr(Module::Runtime, MainLoopAddr::OnVideoError_static)`, `runtime/src/engine/engine.cpp:360` | 5 `Collection::Array` internals + `CreateVideoContext`, `OnVideoError_`, `OnVideoRestart_`, `Device::InitializeInterfaces`, `Application::UnregisterMessageCallback` |

This is the **fifth** instance of the same blind spot (dx11's own resolver, address-only
bucketing, non-literal `RegisterRuntimeExport` names, gated registrations, and now
non-literal `ResolveExport`/`ResolveAddr`). The pattern is worth stating once: **the census
scans for a shape, not for a call**, so every new resolve idiom is invisible until a
process-level measurement finds it. `BANISHED_RT_STRICT` is that measurement, and it
should be run whenever a census number is published.

### The static-only entries — 46, and **14 of them were reached after all**

**Correction to this document's first version, which said 46 entries were "not reached by
any of the nine runs".** That was true of the port-registry doors and false of the process.
Once the *Raw* doors were instrumented (next section), **14 of the 46 turned up** — they
were being read through `ResolveExportRaw` all along, which strict mode could not see:

```
??_7CompiledExternal@Object  ??_7CompressedMemoryStream@IO  ??_7DialogController@UI
??_7Goal@Achievement         ??_7GoalInstance@Achievement   ??_7ImageBuffer@Video
??_7ImageBufferData@Video    ??_7RenderStateData@Video      ??_7Thread@System
?ThreadFunc@Thread@System    ?ToInteger@String@System       ?_singleton@Device@Audio
?_typeInfo@GoalInstance@Achievement                         addr:0x18007fbfc
```

They are exactly the shape the door was hiding: exported **DATA** (vtables, a singleton, a
`BaseTypeInfo`) read by a ported ctor to stamp its object, plus `Thread`'s two members and
lane D's `_beginthreadex`. **The honest static-only count is 32**, and the lesson is the
one this lane is now repeating for the second time in its own report: *an absence measured
by an instrument is a fact about the instrument until every door is instrumented.*

The remaining 32 are still "not reached by any of these nine runs", which is a lower bound
on reachability, not a proof. Most name a feature the matrix simply never triggers:
`FrameCopyImageBuffer` + `ImageBuffer::SaveImageToFile` (the screenshot key),
`CompressedStream::Read`/`SetWindow` and `FileSystem::operator=`
(`standalone/filename_runtime_hook.cpp`), `DialogInstance::StartTimer`
(`autosavedialog.cpp` — no autosave fired in 1,200 frames), `fencedtool.cpp` internals,
`tornado.cpp`'s `Particle::_rng`, `Thread::ThreadFunc` + `??_7Thread@System`, and
`AchievementManager::SetAchievementMet` (no achievement was awarded in any cell).
Lane D and lane C can use this list as a *priority* signal — a row nothing reaches is
cheaper to stub than to port — but each still needs its own written reason, exactly as
the three dead `IO::Serializer` overloads got one.

## The instrument had three blind doors

Raised by lane D: `ResolveRuntimeRaw` neither consults the port registry nor records
residue, so `0x18007fbfc` — the DLL's own MSVC `_beginthreadex`, on the path of every
`System::Thread` — appeared in **no** residue run. That is not a fact about the program;
it is the tool manufacturing an absence, the shape of
`reference_truncated_tool_output_manufactures_calls_zero.md`.

**Complete door inventory** (every way this process can ask `Runtime-x64.dll` for
something), audited across `code/`, `runtime/`, `engine/`, `standalone/` and `dx11/`:

| Door | Instrumented before | Now | Real call sites |
|---|---|---|---|
| `ResolveExport(name)` | yes | yes | the tree, and all of `dx11/` via `Dx11ResolveRuntimeImport` (which calls it) |
| `ResolveOrdinal(n)` | yes | yes | a handful |
| `ResolveAddr(Module::Runtime, a)` | yes | yes | the tree |
| `ResolveExportRaw(name)` | **no** | **yes** (`raw:`) | 101 sites — `RtOriginal` / `RT_ORIGINAL_ONCE`, `VerifyBody`, and DATA reads |
| `ResolveRuntimeRaw(a)` | **no** | **yes** (`rawaddr:`) | 7 — `rt_libm.cpp` ×5, `thread_win.cpp` `_beginthreadex`, `runtime_detour.cpp` ×2 |
| `ResolveOrdinalRaw(n)` | **no** | **yes** (`raword:`) | **0** outside `resolve_standalone.cpp` |
| direct `GetProcAddress` on the Runtime handle | n/a | n/a | **none** outside `resolve_standalone.cpp`. `dx11/src/devicedata.cpp:336` is `d3d11.dll`/`dxgi.dll`, not Runtime; `standalone/renderdoc_integration.cpp` is `renderdoc.dll` |
| `GetModuleHandleA("Runtime-x64.dll")` + raw memory access | n/a | n/a | `standalone/main.cpp:540,729,812` — the crash handler's instruction offsets and the §2.2 byte patches. These read/patch DLL memory rather than request a symbol; they are lane F's item, and they are listed here so the inventory is complete rather than "complete except the ones I decided did not count" |

The change (`standalone/resolve_standalone.cpp`, `resolve.h`) is contained: the three Raw
entry points record under their own key prefixes and delegate to new `…RawImpl` bodies,
which is also what stops `ResolveExport`'s fall-through from double-counting every
port-registry miss as a raw hit. `DumpRuntimeResidue()` now prints two blocks —
`[RtResidue]` (unchanged meaning) and `[RtRawResidue]`.

### What the newly-lit doors show

**104 distinct raw requests** across the same nine runs (`wave9_rawresidue_union.txt`;
per-cell files `wave9_rawresidue_<cell>.txt`): 103 by name, 1 by address
(`0x18007fbfc`, in 7 of 9 cells — lane D's example, now visible), 0 by ordinal. Per cell:
55 / 77 / 91 / 85 / 85 / 85 / 84 / 59 / 83.

| Class | Count | What it means at cutover |
|---|---|---|
| **census-blind** | 54 | Not in the census's list at all. Mostly exported DATA (`??_7…` vtables, `?_singleton@…`, `?_typeInfo@…`) read through a wrapper — `RtOriginal<T>(m)`, `RtVtable(name)`, `VerifyBody(name,…)` — so the literal is not at the `ResolveExportRaw` call site the regex matches. Same idiom class as the 39 blind spots above; the count of hidden requests is therefore **93**, not 39. |
| **ported, body still calls the shipped one** | 34 | We ported it and our own body asks the DLL for the original. Many are `VerifyBody` at registration (diagnostic, dies with the DLL harmlessly) — but some are live `RtOriginal` calls, and **those return null at cutover**. Needs a per-site verdict; the union file tags each row. |
| **also an unported port-registry miss** | 16 | `MemAlloc`/`MemFree` and friends. Porting fixes both doors at once. |

`rt_libm.cpp`'s five `ResolveRuntimeRaw` calls did **not** appear in any cell — worth a
note rather than a conclusion: it means that initialiser did not run in these nine runs,
not that the libm dependency is gone.

### The gate, restated honestly

> An empty `[RtResidue]` list across the harness matrix is the real cutover gate.

That sentence (WAVE9.md, and the first version of this document) is **necessary but not
sufficient**, and it was *unsound* as written for the whole life of the tool — from
`f95c1af` on 2026-09-07 until the change described above, roughly the interval in which
lane 0's first matrix was measured. Three of the six doors were dark, and one of them
(`ResolveExportRaw`, 101 sites) is the one ported bodies use most.

The gate is now: **`[RtResidue]` empty, AND every `[RtRawResidue]` row carrying a written
verdict** — oracle-only (evaluated only under `BANISHED_RT_DIFF`, so it dies with the
oracle) or a real dependency that must be replaced. 104 rows currently have no verdict.

### Where the residue is asked from

Folded, by the directory of the requesting return address:

| Origin | Requests |
|---|---|
| `engine/funcs/` | 84 |
| `runtime/src/` | 77 |
| **`dx11/` only** | **52** |
| `standalone/` | 49 |
| `code/Game/` | 35 |
| more than one of the above | 17 |
| unattributable (`audio.cpp:?`) | 2 |

**52 of the 316 are reached only from `dx11/`** — our own VideoDX11 replacement, which
imports from Runtime exactly as the shipped plugin did (WAVE9.md §4). They are almost
entirely `Video::*Data` ctors/dtors and `Device::Adapter` / `GraphicsInterface` members,
all requested from `VideoDX11::Dx11ResolveRuntimeImport`. A cutover plan that starts from
`code/` and `runtime/` is short by these 52.

## Tool defects found

1. **`runtime/src/object/internal.cpp:917` used `std::printf`, which breaks every
   log-stripped build.** `standalone/log_strip.h`'s `#define printf(...)` cannot rewrite a
   qualified name, so `std::printf` expanded to `std::((void)0)`. `make standalone`,
   `standalone-perf` and therefore `sa-stress` all failed to compile at `e9d81d9`.
   **Fixed** (one token, `std::printf` → `printf`) — it is the only code change this lane
   made, and cell 6 could not run without it. The header already documents the rule; a
   `grep -rn 'std::printf'` in CI would keep it.
2. **`tools/runtime_census.py` reports a different number depending on which Python runs
   it.** With `pefile` importable (the venv, `tools/ghidra_mcp/.venv/Scripts/python.exe`):
   **830 called / 512 ported**. With the system Python 3.12, which has no `pefile`:
   **1016 called / 616 ported**, `internal` 66 → 307. `load_export_addrs()` returns `{}`
   on `ImportError` and the census "degrades to its old address-blind behaviour rather
   than failing" — silently, with no line of output saying so. Every address the export
   directory would have folded onto its name becomes its own `internal` row instead.
   This produced a wrong intermediate finding in this lane (57 phantom "census blind
   spots") that stood until the cause was found. **Fix: print one line when the fold map
   is empty.** Not done here — the tool is in use by other lanes right now.
3. `BANISHED_RT_STRICT` records only the **first** requesting return address per key
   (`ResidueEntry::firstCaller`). For a name asked from several sites the union shows one.
   That was enough for every question this lane asked; a lane chasing a specific consumer
   must not assume the printed caller is the only one.
4. Symbol attribution in the `-O0` exe is unreliable for file-static functions: nearest-
   preceding-`nm`-symbol put `RegisterElementTypeIds`'s requests under
   `RegisterEconomyTypeIds`. The `-O2` exe named it correctly. The union file carries both
   columns for that reason.

## Two things this lane did NOT prove

* **Residue is not reachability.** An eagerly-resolved name appears whether or not anything
  ever calls through the pointer (`EngineFuncsInit` alone accounts for 84 rows). The union
  is the right list for *cutover* — an unresolvable import is fatal whether or not it is
  called — and the wrong list for deciding what needs a faithful body.
* **316 is a floor, not a ceiling.** Nine runs of one harness on one machine. The
  static-only 32 are the visible evidence that the matrix has holes, and the screenshot,
  autosave and achievement paths are named holes.
* **The full request surface is 316 + 104 = 420 rows, and only the first number was ever
  reported before today.** Any earlier statement of the form "the residue is N" — including
  the two in WAVE9.md (260, 309) and the first version of this document — was measuring
  three of the six doors.

## Hand-off

* Work list, port-registry doors: `docs/runtime_port/wave9_residue_union.txt`, 333 raw /
  316 folded rows, each with the cells that reached it and the code that asked.
* Work list, *Raw* doors: `docs/runtime_port/wave9_rawresidue_union.txt`, **104 rows**,
  each tagged with the disposition it still needs. This half is new and nobody has triaged
  it.
* Lane B/C/D should treat the **274 reached ∧ unported** rows as the wave's actual scope,
  the **32 static-only** rows as stub-first candidates, and the **3 bypasses** as a
  correctness bug in code that already counts as ported.
* Re-running the matrix: `tools/rt_run.sh <label> --timeout N -- BANISHED_RT_STRICT=1 …`
  for cells 1–5, `BANISHED_RT_STRICT=1 make sa-stress` for cell 6, and mount the `.pkm`
  from `WinData/_mods_off` for the save9 and mods cells only (they were moved back
  afterwards; the stress cell ran with them unmounted).

## The two pre-existing reference runs are now stale — do not union them in

`wave9_residue_autostart.txt` (260) and `wave9_residue_gameplay.txt` (309) were taken at
17:03/17:21 on an exe built before `b662372..e9d81d9`. Against this lane's runs they show
**5 entries lost and 2 gained**, and every one of the 7 is explained by work that landed in
between, not by a tool fault:

* lost, because they were PORTED since: `Audio::Scene::Pause`, `::StopAll`,
  `::StopEffect3D` (Wave 8.5 close-out), `Input::KeyMap::RegisterInput`,
  `Object::InternalBasePtr::Serialize`.
* gained: `addr:0x180023060` and `addr:0x180023170`, the two `Collection::Set` internals
  `RegisterInput`'s faithful port now calls — the "porting can GROW the cutover surface"
  effect WAVE9.md records, observed a second time from the other side.

Unioning the old files with the new ones would put five already-ported functions back on
the work list. The union in this lane uses the nine runs of 2026-09-07 20:12–20:19 only.
