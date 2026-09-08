# Wave 9 — Cutover

Prep opened 2026-09-07 on `wave/upgrade-path-complete`, following [PLAN.md](PLAN.md) §4.2
wave 9. Nothing is ported yet; this document exists to replace that row, which said
**"Remove `LoadLibrary` of Runtime; retire the §2.2 patches", size S**.

**That row is wrong, and it is wrong in a way that would have cost a session to discover.**
The measurements below are what the row should have said.

## The one success criterion

PLAN §5, the only unchecked box in the whole document:

> `reimpl-x64.exe` boots to gameplay with `Runtime-x64.dll` absent from the game directory.

Everything here is in service of that single line.

## What "S" missed

### 1. The gap is 319 items, not "a LoadLibrary call"

`tools/runtime_census.py`, 2026-09-07, after the three tool defects below were fixed
(`docs/runtime_port/census_20260907_prewave9.csv`, per-item list in
`docs/runtime_port/wave9_lanes.txt`):

**823 called, 504 ported (61.2%) — 319 unported.**

| Kind | Count | What it is |
|---|---|---|
| function | 145 | ordinary named exports |
| unnamed DLL internal, by address | 57 | `ResolveAddr(Module::Runtime, 0x1800xxxxx)` with no export name — the `FUN_` pool |
| ctor/dtor | 51 | `??0` / `??1`; these bind the class-atomicity rule below |
| `??_7` vtable | 28 | exported DATA |
| `?_singleton@…` | 18 | exported DATA |
| `?_typeInfo@…` (`BaseTypeInfo`) | 14 | exported DATA |
| ordinal | 4 | |
| DLL `.data` global, by address | 3 | `0x1801f…`, e.g. the `MainLoop` singleton region |

By namespace: internal 60, Video 59, UI 48, Graphics 28, System 23, Object 22, IO 17,
Achievement 16, Engine 14, Audio 8, Input 7, Math 5, ordinal 4, Application 3, Save 3,
Utility 1, Volume 1.

**60 of the 319 are DATA, not code** — this table originally said 17 singletons / 59, and
both numbers were wrong by one. Lane C's harvest
([WAVE9_LANE_C.md](WAVE9_LANE_C.md)) found 18 distinct `?_singleton@…` symbols with no
duplicates; a first manual grep had missed `IO::FileSystem`, `Video::Configuration` and
`Video::Device`. A vtable, a singleton slot and a `BaseTypeInfo` cannot be "ported" in the
sense the per-function recipe means — there is no body to reimplement. They have to be
*minted*, with a layout that every ported and unported consumer agrees on.

### 2. Every verification mechanism this project has built dies at cutover

This is the part that makes Wave 9 unlike waves 1–8, and it is not a scheduling detail — it
decides the order of the work.

| Mechanism | What happens when the DLL is gone |
|---|---|
| `BANISHED_RT_DIFF` differential oracle | **Gone.** It compares our result against the DLL's. There is no second implementation to diff against. Every one of the 79.0M diffed calls that closed Wave 8 was borrowed from a DLL that will not be there. |
| `RtOriginal` / `RT_ORIGINAL_ONCE` | **Gone.** It is `GetProcAddress` against a module that is not loaded (`reference_rtoriginal_is_getprocaddress.md`). Every call site must be compiled out, not left to return null. |
| "Stamp the DLL's own exported `??_7`" | **Gone.** Waves 6 and 7 both avoided a detour because `??_7Geometry` and all 68 UI vtables are exported, so a ported ctor copies the DLL's vtable byte for byte. With no DLL there is no vtable to copy: every ported ctor must mint one whose slots ALL point at ported functions. |
| `BANISHED_RT_PORT=0` A/B | **Gone.** The kill-switch routes to the DLL. |
| `BANISHED_RT_DIFF` — **a second, separate limit, and it is not about cutover** | **The oracle is not thread-safe, and never was.** `runtime/funcs/rt_diff.cpp` contains ZERO synchronisation primitives (verified 2026-09-07: no mutex, no atomic, no `Interlocked`, no `CRITICAL_SECTION`) — registration is a bare `g_sites[g_siteCount++]` and counting is a bare `site->calls++`. So **no diff site may be placed in a body that runs off the main thread**: it would be a data race in every worker the game creates and would produce a count nobody could trust. Wave 9 lane P hit this porting `System::Thread::ThreadFunc` and shipped an opt-in execution witness (`BANISHED_RT_THREADFUNC_TRACE=1`) instead of a diff site — a positive reason, recorded in the port. **Open question this raises for waves 1-8:** every published `calls=`/`divergences=` figure assumed single-threaded access. Most ported bodies are main-thread and the counts were large and stable, so this is a caveat rather than a known error — but it has never been checked, and "never checked" is how the three census defects survived. |
| `tools/resolve_fallback_audit.py` (the guarded-fallback audit) | **Gone, and it takes 166 source sites with it — see below.** It answers "is this fallback address the entry point of the function this mangled name denotes" by comparing against the shipped DLL's export directory. With no DLL there is no export directory, so the audit cannot answer its own question at exactly the moment the fallbacks stop being dead. Run with the DLL absent it exits **2** and says so, rather than passing quietly. |
| The detour question | **Moot, and that is the good news.** "The DLL's own internal callers" (§4.1 decision 3) is only a problem while the DLL runs its own code. It never needed a detour in 8 waves; at cutover it stops being a category. The count stays 0 forever. |

**The consequence, stated plainly: nothing may be ported during Wave 9 that has not been
diff-verified before the DLL is removed.** Waves 1–8 could port and verify in the same
sitting. Wave 9 cannot. The verification budget has to be spent *first*, on the DLL that is
still there, and the removal is the last commit, not the first.

That inverts the per-function recipe (§6 step 6) for this wave only, and it is why the
holdbacks matter more than usual: **the 32 functions Wave 8 held back, the 5 handle-table
mutators, the 4 `ExternalFactory` mutators and the whole allocator all have to come back and
be verified against the DLL now**, while an oracle still exists.

### 3. The allocator NO-GO stops being optional

`runtime/src/system/allocator.cpp` + `allocator_win.cpp` are 27 registrations behind
`BANISHED_RT_ALLOC`, with a written **NO-GO** verdict ([WAVE2_ALLOCATOR.md](WAVE2_ALLOCATOR.md)).
Wave 8 found the NO-GO is really four functions wide, not the ctor/dtor pair PLAN predicted:
`Engine::Startup` writes three allocator fields directly and `Shutdown` calls the allocator's
shutdown.

With the DLL absent there is no other allocator. Every `MemAlloc`/`MemFree` in the process
becomes ours, including the ones inside ported bodies that currently allocate through the
DLL's heap. `?MemFree@…` and `?MemAlloc@…` are the two highest-count entries in the residue
census below, which is what you would expect and also what makes this the single riskiest
item in the wave: `str-4`/`str-8` header conventions, the boundary-tag layout, and the
half-written-pointer corruption family all live here.

**This is the one item that cannot be staged.** A heap cannot be half-ours.

### 4. `dx11/` is part of the cutover surface and was not being counted

`dx11/` is our VideoDX11 replacement and the existence proof that a DLL can be removed —
but it *imports from Runtime-x64.dll* exactly as the shipped plugin did, and 60 of those
imports were invisible to the census until 2026-09-07 (defect 1 below). Video's called count
goes 69 → 112 on that correction alone. Any cutover plan that starts from `code/` and
`runtime/` and forgets `dx11/` is short by ~50 functions.

## The tool this wave needed first: `BANISHED_RT_STRICT`

`tools/runtime_census.py` answers "how much is ported" **from source**. It cannot answer the
question the cutover gate actually asks, which is **"does anything still ASK the DLL for
something at runtime"** — because the source list is a superset that counts every resolve
site in the tree, including ones no reachable path executes.

`BANISHED_RT_STRICT=1` (`standalone/resolve_standalone.cpp`, commit `f95c1af`) records every
`ResolveExport` / `ResolveOrdinal` / `ResolveAddr(Module::Runtime)` that misses the port
registry and falls through to the shipped DLL, with the return address of whoever asked, and
prints one `[RtResidue]` line per distinct miss beside `[RtCensus]`.

It is **log-only by design**. It does not null the result or hand back a trap stub: 59 of the
misses are exported DATA, and poisoning those corrupts the process long before the call that
would name the culprit. Strict mode observes. Deleting the DLL is what simulates the DLL
being gone.

**An empty `[RtResidue]` list across the full harness matrix is the real cutover gate.**

Two runs so far, both kept in `docs/runtime_port/`:

| Run | How | Distinct requests still reaching the DLL |
|---|---|---|
| `wave9_residue_autostart.txt` | `BANISHED_AUTOSTART=1 BANISHED_AUTOQUIT=1` | **260** |
| `wave9_residue_gameplay.txt` | `BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=120` | **309** |

Kinds, from the 309-entry gameplay run: 100+ ordinary functions, ~57 unnamed DLL internals by
address, ~39 ctors/dtors, 27 `?_typeInfo@…`, 19 `.data` globals by address, 11
`?_singleton@…`, 7 `??_7` vtables, 2 ordinals. 47 of them are reached only from `dx11/`.

Counts are per *resolve*, not per call — most are pointer-table fills at init, so `?MemFree@…`
showing 7 does not mean seven frees. The residue measures the **resolve surface**, which is
exactly what the cutover gate is about.

### A correction, recorded rather than quietly fixed

**This document first claimed the residue set was path-independent, and that was wrong.**

The evidence for it looked strong: an `AUTOSTART`+`AUTOQUIT` run and a `BANISHED_AUTOLOAD=Save9`
run produced *byte-identical* 260-entry lists, which was written up as `EngineFuncsInit` filling
the `fn_*` tables eagerly at boot.

The real explanation is duller and the lesson is sharper. **`BANISHED_AUTOQUIT=1` injects a Quit
click at the MAIN MENU** (`[AutoStart] menu settled — injecting Quit click`). Neither run ever
reached gameplay. They were byte-identical because **they were the same path**, and the boot half
of that path is genuinely deterministic. Two runs agreeing is evidence about the runs, not about
the program — and "different env vars" is not the same as "different path".

Re-run through the pause menu, which does reach gameplay, the residue is **309**: a strict
superset, **49 new entries and none lost**. The 49 are exactly what a menu-only run cannot
reach — `Achievement::ReadAchievements`, `AchievementManager::CreateInstance`,
`SaveLoad::Draw`, `Audio::Scene::StopAll`, `Matrix::Inverse`, `~UserInterface`, five Video
`*Data` dtors, and five more unnamed internals.

So the residue is **monotone in coverage, and lane 0's matrix is required after all** — which is
what the lane table said before this section was written. The cheap-lane-0 conclusion is
withdrawn.

**And the fact was already written down.** WAVE3.md's "Notes for later waves" says it plainly:

> **Check `calls=`, not `divergences=`.** This wave's first integration run reported 22
> registrations and zero divergences while most sites never executed — because
> `BANISHED_AUTOQUIT` drives menu→**Quit**, not menu→New Game.

Six waves later the same env var produced the same wrong conclusion in a different tool. That is
worth more than the correction itself: **this project's failures are not usually unknown facts,
they are known facts that did not reach the next wave.** It is the same shape as the two Wave 5
phantom manglings that Wave 8 had to fix again because Wave 5 diagnosed them and never fixed them
at source, and it is what PLAN §6 step 8 exists to prevent. The mitigation here is not another
note in a wave doc nobody opens — it is that `BANISHED_AUTOQUIT`'s behaviour is now in the
`BANISHED_RT_STRICT` memory entry and in lane 0's row of the table above, where someone reaching
for the tool will actually meet it.

**What survives, and is the useful half:** an entry, once present, never disappears on a longer
run. That makes the union across the matrix a sound work list, and makes any *shrink* between two
runs a tool bug rather than a fact — the shape of
`reference_truncated_tool_output_manufactures_calls_zero.md`, where a `tail -80` faked sixteen
sites disappearing.

**And it is still not a reachability proof.** An eagerly-resolved name appears whether or not
anything ever calls through the pointer — the opposite hat of
`reference_uncalled_port_is_not_verified.md`. Residue answers "what does the process ASK the DLL
for", which is the right question for *cutover* (an unresolvable import is fatal whether or not
it is called) and the wrong one for *verification*.

## What the residue census found on its first run

Comparing 260 runtime misses against the census's static list found **three defects in the
census, in both directions**. All three are fixed in `f95c1af`.

1. **Undercount, 51 functions.** `dx11/` resolves through its own
   `Dx11ResolveRuntimeImport(mangled, iat)` and passes the name from a table literal, not at
   the call site, so `RE_EXPORT` saw none of it. All 60 dx11-only names were then checked
   against the shipped export directory: **0 phantoms**.
2. **Overcount, 6 functions.** A `RegisterRuntimeExport` inside an `if (<flag>)` block that
   is off by default was counted as a live port; `GATED_OFF_FILES` only knew whole files.
   All six are WAVE3.md's NO-GO handle-table tier in `runtime/src/object/internal.cpp`, and
   the residue census caught four of them *arriving at the DLL* while the census called them
   ported. Object goes 28 → 22 — **which is what WAVE3.md said all along.** The wave doc was
   right and the tool was wrong, for a full wave.
3. **18 registrations passing a `const char* const kX = "?Mangled…"` name** are now resolved
   rather than warned about. Naming the mangling once, and using it at both the
   `RT_ORIGINAL_ONCE` site and the registration, is good practice — the oracle and the
   registration cannot then drift — so the census resolves it. UI 98 → 101.

Net: **the honest census is 823 / 504 / 61.2%**, not the 772 / 507 / 65.7% recorded at the
Wave 8 boundary. The denominator grew and the numerator shrank, and both moves were the tool
being wrong rather than the port changing.

This is the same lesson as `reference_uncalled_port_is_not_verified.md` and
`reference_census_cannot_see_nonliteral_registrations.md`, arriving for the third time:
**every progress number this project has published came from a tool that had not itself been
checked against the process.** The residue census is the first thing in the tree that asks
the process.

## A porting a function can GROW the cutover surface

Measured on the first holdback closed by porting. `KeyMap::RegisterInput` landed and the census
went **823 called / 504 ported → 825 / 505**: +1 ported, **+2 called**. Net coverage did not
move (61.2% both times).

The two new entries are the port's own dependencies. Retail's insert calls two unexported
`Collection::Set` internals — the node allocator `0x180023060` and the AVL rebalance
`0x180023170` — and a faithful port calls them the same way, by address, exactly as `ClearAll`
already did. Each `ResolveAddr(Module::Runtime, …)` is a new row in the **unnamed DLL internals**
bucket, which is the one bucket with no export name to work from (lane D, the `FUN_` pool).

**Consequences for lane sequencing:**
* **`called` is not a fixed denominator counting down to zero.** A port that reaches into DLL
  internals moves work from the named column into the hardest unnamed one. A wave that measured
  progress purely as "ported / called" could port for a day and show no movement while doing
  real work — or worse, look like it was going backwards.
* **Prefer ports whose callees are already ours**, and when a port must call an internal, treat
  that internal as a lane D row created by the port, not as a free dependency.
* The two rows this one created are shared `Collection::Set<E24>` internals, so they are paid for
  once and every other `KeyMap` mutator (`Load`, and Wave 3's tree mutators) reuses them. That is
  the good case; it is not automatic.

## Proposed lane order

The order is forced by §2 (verify while the oracle exists) and §3 (the heap cannot be half
ours), not chosen.

| Lane | Scope | Gate |
|---|---|---|
| **0. Residue matrix** | Run `BANISHED_RT_STRICT=1` across the whole harness matrix — menu, mods menu, all 4 tutorials, `Save9.sav` autoload, `sa-stress`, and gameplay through the pause menu. Union the residue lists; entries are monotone, so the union is sound and any *shrink* is a tool bug. **Use a route that reaches gameplay** — `BANISHED_AUTOQUIT=1` quits at the MAIN MENU and cost this document one wrong finding (see the correction above); menu-only 260 vs gameplay 309. **No attended run is needed for `Input::Device` after all** — see the CLOSED section below. | A single work list that is what the process reaches, not what the tree mentions |
| **S. The save-stream A/B oracle** — **BUILT AND PROVEN 2026-09-07**, `runtime/src/io/save_trace.{h,cpp}`, `BANISHED_RT_SAVE_TRACE=1` | Replays both bodies into two independent **synthetic** sinks and compares the **emission sequence** — not the bytes on disk, because a desync IS a different sequence of typed writes, and because a synthetic sink needs no `MemoryStream` and no codec. First subject: `Audio::Scene::Serialize`, **32,772 emissions identical, 0 diverged.** | An identical emission sequence |
| **A. Reclaim the holdbacks** | Port + diff-verify, against the still-present DLL, everything held back in waves 3–8. **Triaged below into a per-function disposition** — P (1, a probe), B (10, blocked on a named dependency), N (7, allocator-bound or oracle-hard), D/X (14, stubs or already-unblocked). Start with X: four `ExternalFactory` mutators whose stated blocker was a Wave 4 dependency that has since landed. | 0 divergences and non-zero `calls=` for the ports; a written reason for each stub |
| **B. The 145 named functions + 51 ctors/dtors** | The ordinary port wave, class-atomic (§4.1 rule 5). | 0 divergences |
| **C. Data: 39 vtables, 18 singletons, 14 `BaseTypeInfo`, 10 other `.data` statics** | Mint them. **DONE as of 2026-09-07 — see [WAVE9_LANE_C2.md](WAVE9_LANE_C2.md).** Three corrections to this row: the count is derived by PE SECTION not name prefix (60 -> 70), it is a MOVING TARGET that grows as other lanes port classes (70 -> 81 when lane B2 landed the `Video::*Data` companions), and a vtable does NOT have to wait for its class: slots are filled from the port registry at runtime, so an unported slot is a NAMED TRAP and the table upgrades itself as lane B lands ports. | `static_assert` on every offset; the mapping shadow check and the constructed-statics shape check in the gate checklist below |
| **D. The 57 unnamed internals** | **The known-hard one.** PLAN §4.3 risk 4 records the `FUN_` pool as MEASURED AND UNSOLVED and CLOSED TO AUTOMATION: all three levers (hash/fuzzy, string anchors, call-graph) run to a fixed point at 2% touched, 9 names proposed, none verified, and no PDB exists. 57 is a far smaller target than the pool, and each has a call site that says what it is used for — that is the lever, not the name. | Each one either ported, or proved unreachable by lane 0 |
| **E. Allocator** | Arm `BANISHED_RT_ALLOC`, revisit the NO-GO, and take the heap. Includes `Engine::Startup`'s three direct field writes and `Shutdown`. | ✅ **MET 2026-09-08 — both halves, on two different runs, and they had to be different runs.** **(i) Arming, DLL ABSENT (attempt 06):** attempt 05 **refused to arm** and was right to by its own logic; lane E's retire-then-guard ordering fix is why 06 does. Attempt 05 **refused to arm** and was right to by its own logic; lane E's ordering fix (retire-then-guard) is why attempt 06 does. The evidence is a DLL-absent run, which is the only place this can be shown: `[RtAlloc] ARMED (cutover: Runtime-x64.dll absent) -- 27 exports registered, no detour installed because there are no DLL-internal callers to redirect. ?Allocate@/?Free@ point at the COMPOSED entry points, which take the allocator critical section.` **So the heap is already ours BEFORE `GameMain` runs**, which matters for lane ENG: retail creates the arena inside `Engine::Startup` at 0x1800db905 because it has nowhere else to, and a faithful port would create it a SECOND TIME. That is the one question in this row nobody has answered yet. The `[AllocLock] could not resolve Allocate (0x1800c6e60) / Free (0x1800c6c00)` lines in the same run are the two `benign` `[RtResolveEarly]` rows, not defects — those addresses are simply dead with no DLL. **(ii) The stated pass condition, DLL PRESENT:** `BANISHED_REDZONE=watch` + `sa-stress`, run 2026-09-08 (`perf/results/20260908_040535_laneE_redzone`). **3,000 frames to a clean self-quit, 802 citizens, 15,251 entities, `[REDZONE-HEAP] watchdog thread started (interval=1ms)` and ZERO violations reported**, plus `[RtResolve] 0 misses` and only the two known-benign `[RtResolveEarly]` rows. Arming is not the same claim as not corrupting the heap, which is why both were required. ⚠️ **Do NOT read this run's perf number.** `report.txt` says `sim ms / iteration 0.19 -> 0.30 +55.2% REGRESSION / PLAN 5% gate FAIL` — that is **the watchdog's own cost**, a 1 ms polling thread over the whole heap, not a regression in the port. A redzone run is a CORRECTNESS instrument and its timings are not comparable to a baseline; recording that FAIL as a perf finding would be the same category error as reading a debug-build number, and this wave has a standing rule that one run cannot resolve the 5% gate anyway. Frames were 3,000 rather than the harness default 6,000. |
| **F. Retire the §2.2 patches — AND the fallback idiom, 166 sites** — **FALLBACK HALF DONE 2026-09-07 (0 sites); §2.2 TRIAGED, ALL CARRIED, see [WAVE9_LANE_F.md](WAVE9_LANE_F.md) — and PLAN §2.2 A is WRONG that `filename_runtime_hook.cpp` is only diagnostics: `InstallFindOrLoadWatch()` is default-on and is the menu-crash fix** | The four byte-patch files, the crash handler's Runtime instruction offsets (§2.2 C, kept and narrowed by Wave 3), the `MessageBoxW` IAT swap, and the direct layout reads in §2.2 B. **Plus a countable item this table did not have until Wave 9 lane D measured it: all 166 `ResolveExport(...); if (!x) ResolveAddr(Module::Runtime, ...)` fallback branches must be DELETED before lane G.** With the DLL gone, no `ResolveExport` can succeed, so every one of the 166 becomes the live path *simultaneously* and jumps at an address in a module that is not mapped. The replacement for the audit is not another tool — it is leaving nothing to audit. | Each one deleted, not carried; `resolve_fallback_audit.py` reports 0 sites |
| **G. Remove `LoadLibrary`** | The actual cutover. Delete the DLL from the game directory and boot. **Nothing here may run for the first time on cutover day** — every gate below needs the DLL to compare against and stops working the moment it is gone. | The PLAN §5 box |

### The prefix-replay hazard is a CLASS, and `calls=` hid it — lane M sweep, 2026-09-08

Finding 1 (`NumberElement::SetValueInt`) is not one bad oracle. **164 `RT_DIFF_BYTES`
prefix-replay sites across 29 files**; filtering to bodies that both memcpy a scratch AND
reach a string/refcount/alloc name leaves **nine**. The matrix's own `calls=` column then
splits them, and the split is the finding:

| Prefix-replay body | max `calls` across the whole matrix | Read |
|---|---:|---|
| `NumberElement::SetValueInt` | 12,896 (**only 10 did work**) | Finding 1 |
| `NumberElement::SetValueFloat` | **NEVER REGISTERED** | **the SAME bug** — identical body, same `CacheText`, same hazard. Silent only because nothing calls it |
| `NumberEditElement::SetMinMax` | **NEVER REGISTERED** | **top priority.** Its replay shares `base._text`, and `RealNumberEditElementSetValue()` swaps that refcounted pointer — same mechanism, **zero coverage to have caught it** |
| `DocumentTextElement_SetText_UiString` | NEVER REGISTERED | reads SAFE (`UI::String` argued POD, and it undoes its write) — **verify, do not assume** |
| `Element::Element` (ctor) | never registered under that name | check the registration before concluding |
| `SpinnerElement::SetMinMax` | 4 | reaches the same path; **4 calls is not coverage** — check how many DID WORK |
| `ElementDescription` ctor | 2 | same |
| `Element::SetToolTip` | 10 | `ReplayGuard`, POD `UIString`. Lowest concern |
| `MeshInstance::UpdateHierarchy` | 48,742 | heavily exercised, clean |

**Three of the nine are never called anywhere in the matrix, and two of those three share
Finding 1's exact mechanism.** Their zero-divergence status is worth nothing — it is
`reference_uncalled_port_is_not_verified.md` sitting directly on top of the defect the
matrix just found.

These are **CANDIDATES NEEDING A WRITTEN DISPOSITION EACH**, not verdicts. A static
checker would have `port_bypass_audit.py`'s exact blind spot — it cannot follow a call
into a callee, so `CacheText` is visible only where it happens to be named at the call
site. **A checker reporting 0 here would be lying.**

**And the reason it hid for a wave: `calls=` counts calls ENTERED, not calls that DID
WORK.** 12,886 of 12,896 took an unchanged-value early-out and compared identical, so the
site looked richly covered while carrying **ten** real comparisons. `calls=0` reading as
clean is the known trap; **a LARGE `calls=` reading as coverage is the same trap one level
up.**

Three mechanisms to make the rule enforceable rather than remembered (lane M building
1-3, source-only):

1. **The RELEASED-POINTER DETECTOR.** ⛔ **IT IS NOT A TREE-WIDE SWEEP — lane N, 2026-09-08.**
   It is called from inside `RtDiffReportBytes` **AFTER the `memcmp(...) == 0` early
   return**, so it only ever examines comparisons that **already differ** — and so does
   its self-test. In the 10-cell matrix, **7 cells printed NO self-test line at all**
   (`menu_only`, `gameplay`, `saveload`, `tutorial0`, `menus_ach`, `menus_mods`, `stress`
   — every cell with 0 byte-compare divergences), so the detector never executed and by
   its own contract **their silence means nothing**. The 3 that printed `ALIVE` are
   exactly the 3 with a byte divergence.
   **This is a FOURTH blind spot and it is CIRCULAR with limit 2 below**: a released
   pointer is invisible when the two sides do not differ (allocator recycling), and the
   detector only looks when they DO. **The matrix therefore swept ONE site, not 170**
   (`UI::NumberElement::SetValueInt`, the already-known defect). **This must not close a
   gate row.** For a real sweep the detector has to move ABOVE the memcmp early-return,
   running on every byte compare — a cost question, but the only way it sees the
   recycling case. Original claim, now falsified:
   Fires only when all three hold: the same slot holds a different pointer on each side;
   the side the DLL ran on points at a block whose `{len,rc}` header at `[-8]` reads
   `{0,0}`; ours points at a live one. Reports as **HARNESS DEFECT, explicitly not a port
   defect**, and says in its own message not to exclude the bytes.
   It carries a **positive AND negative self-test that run automatically**, for a reason
   worth copying: the detector's only natural control is `SetValueInt` on tutorials 1-3,
   **which disappears the moment lane N lands the fix** — leaving an instrument that
   cannot distinguish "nothing is wrong" from "I stopped working". So it builds a
   synthetic pair that IS the defect and one that is NOT, and prints `ALIVE` or
   `*** BROKEN — its silence means NOTHING ***` on the first byte comparison of every run.
2. **`BANISHED_RT_DIFF_INVERT=1`** — run the DLL's body FIRST on the scratch. ⚠️ **Lane M
   WITHDREW its own "one-flag change to the replay macro" claim: THERE IS NO REPLAY
   MACRO.** The ours-then-theirs ordering lives inside each hand-written body, so no
   change to `RT_DIFF_BYTES` can reorder them. It ships as an opt-in macro pair
   (`RT_DIFF_REPLAY_PRE`/`_POST`), ~2 lines per site, off by default, arming code logging
   and FLUSHING the site name **before** the original is called so a crash names its own
   function. **No site has adopted it, so today INVERT is a NO-OP and sweeping on it would
   measure nothing** — `calls=0`-reads-as-clean in a third costume. It is the ESCALATION
   applied to what the detector names, not the sweep.
2. **A runtime assert** — before replaying, walk the scratch for heap pointers; after our
   body runs, check whether any now addresses a block whose `{length, refcount}` header at
   `[-8]` reads zero. Name the function and abort. This converts "someone must remember
   `widgets_input.cpp` lines 53-58" into a self-reporting harness.
3. **A DID-WORK counter** beside `calls=`, so a site cannot look covered on early-outs.

### `Picking::Scene::Intersect` — MECHANISM FOUND: a MOVED TERM (a real port defect)

Lane N, 2026-09-08. The one non-harness divergence the matrix produced is a **genuine port
defect**, and it is the kind the port's own no-tolerance comment exists to catch.

Retail pulls the ray ORIGIN into sub-mesh local space by adding the inverse transform's
**TRANSLATION LAST** — a single `addss` against a register already holding the fully
accumulated rotated sum. `0x180124B56`-`0x180124BB4`, three times, identically:

```
movss xmm0,[rbp-0x58] / addss xmm0,xmm3   -> local.x
movss xmm0,[rbp-0x54] / addss xmm0,xmm2   -> local.y
movss xmm0,[rbp-0x50] / addss xmm0,xmm7   -> local.z
```

`localOx` had that shape. **`localOy` and `localOz` did NOT** — they read `t + a + b + c`,
which in C is `(((t+a)+b)+c)`: **the translation folded in FIRST** and the rotated terms
accumulated on top of it. One moved term, in two of three components. Fixed by
parenthesising the rotated part so the translation is added last, with the instruction
evidence and a *"do not simplify this"* note in the comment.

**Why it hid, and why the rate story now coheres:** a moved term only changes the ROUNDED
result where the translation and the rotated part **nearly cancel** — a small region of one
mesh. So it is invisible on essentially every ray, and a real-ray run casts only a handful
through that region. That is exactly why it presented as 1-in-23,654 and as "1 of 4 stress
runs", and why **a clean re-run was never going to exonerate it**.

**The diagnostic that cracked it — CONTIGUITY.** Scaled to 244,824 rays / 29,867 hits the
effect became deterministic: **21 divergences**, all on entity id=0, all from one ring
origin, and **the diverging targets form CONTIGUOUS PATCHES of the target grid, not
scattered points.** A contiguous patch is a region of one *surface*, which says the cause
is arithmetic specific to a VALUE RANGE rather than a random rounding coincidence — and
that is what sent lane N back to the local-space transform instead of the triangle test.
`p.x` on those rays is 41.31 / 41.50 / 41.68 / 41.87 — the same values as all three
real-ray observations.

Both controls had to be REPAIRED before the fixture was usable: `hits=` killed two useless
ray sets, and the **1-ULP negative control fires at 7,520 of 7,536 hits** where the
association control it replaced fired at **0 of 525** and was underpowered rather than
informative.

⏳ **VERIFICATION IN FLIGHT** on exe `2026-09-08 00:54:19`: the same 244,824-ray sweep,
predicted **21 → 0 with `hits=` unchanged at 29,867**. Closing on a deterministic repro
with a working negative control, not on a lucky clean run.

⚠️ **Caveat not buried:** the TRANSLATION PLACEMENT is proven for all three components, but
retail's `xmm7` chain for `localOz`'s *interior* rotated-sum order has **not** been fully
traced. `localOy`'s was (`((A-B)+C)+D`, port matches).

### `Picking::Scene::Intersect` — QUANTIFIED, and one claim WITHDRAWN

Lane N, 2026-09-08. **Reproduced a THIRD time on a clean `921eb23` tree with the lane's
own fixture PARKED**, so it is not an artifact of the instrument: `stress` call #18548,
`p.z` ours `0.091594696` vs theirs `0.0915985107`, `x` and `y` bit-identical, normal
identical.

**Δz is EXACTLY 2^-18 = 3.814697e-6 in ALL THREE independent observations** (lane M's,
lane N's at `-0.2141`, and this one at `+0.0916`). 2^-18 is one ULP of a float in
`[32,64)` — the magnitude of `p.x ≈ 41.3-41.9`, which is also the magnitude of `hitT` for
these rays. So **`hitT` differs by EXACTLY ONE ULP**, `z` inherits it, and `x` survives
bit-identical because `Δ(hitT·dir.x)` usually rounds away. **Rate: 1 in 23,654 calls.**
Our own binary disassembles to plain `mulss`/`addss` at the hit block — **no FMA
contraction** — so the `outPos` arithmetic is not the difference; `hitT` itself is.

⛔ **WITHDRAWN by lane N:** its earlier statement that the green negative control was
*"evidence against the association hypothesis"*. **It was UNDERPOWERED.** The control had
**525 hits** against an effect rate of **1 in 23,654** — expected occurrences **0.02**. It
says nothing about the hypothesis either way; it only proves the fixture cannot detect a
rare effect at that sample size. The green result remains a real fixture defect (a control
must fire on the geometry it is run on), but the inference drawn from it was wrong and is
not to be carried forward. **A control that cannot fail and a control that is merely too
small look identical from the outside — both come back green.**

The decisive experiment is now specifiable rather than a guess: a sweep large enough for a
**stable, countable divergence rate** (>100k hitting rays, ~4-10 expected divergences),
then A/B each candidate association in the local-space ray transform and keep the one that
takes the count to zero.

### The released-pointer detector is a FLOOR — how to read a clean sweep

Lane M, `f9b8d36`. **A clean detector sweep means "no site of the shape it can see"** —
the same sentence the `port_bypass_audit.py` gate row carries, for the same reason.
**The detector alone MUST NOT close a gate row.** Three measured limits:

1. **It only looks INSIDE the compared byte range, and that range is a LOWER BOUND.**
   `widgets_input.cpp`'s own header says `sizeof(OurStruct)` is a lower bound on the real
   retail object, so the oracle "is silent about anything past it". Finding 1 was
   catchable *only because* `+0xd0` falls inside `sizeof(NumberElement)` = `0xE0`.
   **`TextElement`'s cached-text pointer at `+0xf8` does not.** A released pointer parked
   past the boundary is invisible to the detector for precisely the reason it is
   invisible to `RT_DIFF_BYTES`.
2. **It needs the two sides to DIFFER.** If our body releases a buffer and the DLL's body
   reallocates into the same address — likely, since it is the block just freed — nothing
   differs and the detector never runs. Finding 1's own data shows that recycling
   happening one call later.
3. **It knows ONE header convention**, `{length, refcount}` at `[-8]`. A refcounted object
   with a different header, or a released non-string allocation, reads as ordinary memory.

The two mitigations are the **self-test** (proves it can still fire) and **INVERT** (needs
no difference and no header convention — it just corrupts the heap and crashes, in the
function that did it). That is the argument for adopting INVERT at whatever the detector
names.

**⏳ ORDERING, and it expires:** the detector's acceptance test — watching it fire on a
REAL defect rather than a synthetic one — is the tutorial-1 cell **while `SetValueInt` is
still broken**. Lane N's fix removes exactly that defect. Run the detector BEFORE the fix
lands, or the instrument reaches cutover having never been seen to fire on anything real.

### Finding 3 — the family is ENUMERATED, and retail HARD-FAULTS where our port guards

Lane N, 2026-09-08, `6b45035`. Source + Ghidra only.

**Five trees** the scheduler (`*(gameWorld+0x118)`) keeps per posted work item, each with
its insert, its erase (`0x1400F8E30` — the function that faulted) and the KEY the erase
uses:

| # | Tree | Erase key |
|---|---|---|
| 1 | `sched+0x10` primary AVL | `*(workObj+0x24)`, **RE-READ at erase** |
| 2 | `sched+0x30` id map | `taskIdx` (parameter) |
| 3 | `sched+0x40..` 14 category subtrees | `taskIdx` |
| 4 | `sched+0x120` profession buckets | **never erased — by design, in both implementations** |
| 5 | `bucket+0x08` inner id tree | `slot50(workObj)`, **RE-READ at erase** |

**No path in this family frees a still-linked node.** Every free in 1/2/3/5 goes through
`ca60`/`a6e0`, which unlink first — verified line by line including the two-child
in-order-predecessor splice. The one bulk free that does NOT unlink (`e320`) is used here
in exactly one place, on `WS_cb50`'s own stack temporary, and every call site passes a null
subtree so it never runs.

**So it cannot be a free-without-unlink. It must be an ERASE THAT NO-OPS while the destroy
after it proceeds** — and rows 1 and 5 are the only two that can, because they are the only
two whose key is re-read out of the work object at erase time. `8e30` step 5 (vtable
`+0x48`, `+0x40`, `DeletePtr` on the task IBP, `ca60` on `sched+0x30`) runs
**UNCONDITIONALLY**, with no dependence on step 4 having found anything. A drifted key
therefore leaves a bucket entry whose payload is freed one instruction later. That the key
CAN drift is not speculation: `CitizenAI_WorkScheduler_Rekey` exists solely to move an item
between buckets when a building changes profession.

**⚠️ RETAIL HARD-FAULTS WHERE OUR PORT SILENTLY GUARDS.** Retail `0x1400F8F5C`-`0x1400F8F5E`
is `xor ecx,ecx / mov rdx,[rcx+0x8]` — a read of address `0x8` — on **both** the empty-tree
and the not-found exits of the bucket search. Retail asserts that a work's own slot-10 key
always names an existing bucket and **dies immediately if it does not**. Our port has
`if (node != nullptr)` and carries on. **This port has been running in a state retail
treats as impossible, and nothing has ever checked.** The asymmetry matters: retail DOES
tolerate the ENTRY being absent from a bucket that exists, so only the missing-BUCKET case
is retail-fatal. **The guard was LEFT IN** — deleting it would move the crash, not fix it —
but note this inverts the standing rule: the usual failure is *fixing a faithful crash with
a null check*; here a null check has been **masking** one.

**`BANISHED_WORKSCHED_PROBE=1`** (behaviour-unchanged) counts per `8e30` call and reports at
exit: `unfiled` / `entryNotFound` / `bucketNotFound` / `bucketRootNull`. **Falsifiable both
ways** — if all three non-healthy counters come back ZERO on a stress run, Finding 3 is NOT
this mechanism and the enumeration has ELIMINATED the whole family rather than found the
culprit.

**Eliminated with evidence, not merely unexamined:** work-id reuse (ids are monotonic,
wrapping at `0x7FFFFFFE`, so a stale `workComp+0x28` cannot alias a live item in 1,200
frames — and the id-map lookup IS the liveness test); teardown ordering
(`EntityList::Shutdown` runs before `m_game` is freed, so the scheduler is alive
throughout); `TreeNodeFree_2ce60` and the tutorial tree; `ca60`'s AVL erase;
`abi_retslot_audit` clean.

**Slot/offset audit — one hit, in a COMMENT rather than code.** All five walkers of the
`0x30` node agree (lesser `+0x18`, greater `+0x20`), but `WorkComponent_CheckWorkSlot`'s
header labelled the two children **backwards** while its code was correct. Corrected: in a
family that has now paid three times for exactly this confusion, a comment contradicting
its own code and its four siblings is the next mix-up waiting to happen.

### ⛔ TWO CUTOVER BLOCKERS the clean cells did not show — lane W, 2026-09-08

Lane W executed its nine ports across four cells: **0 divergences in 0 sites** every time
(319/31.9M, 324/32.0M, 353/37.4M, 344/16.1M), zero-byte crash log. The findings are in what
`calls=` and the BRANCH counters said, not in the divergence column.

**1. THE BRANCH CUTOVER TAKES FIRST IS THE ONLY ONE NEVER EXECUTED.**
`ListLink[first-in-list]` and `ListUnlink[list-emptied]` are `calls=0` in **every cell for
all three buffer classes** — 31 construct calls all *appended*, 10 destruct calls all
*unlinked*. **This is structural, not luck:** the DLL's own constructors never reach our
ports, so its lists are never empty by the time ours run, and our 31 calls are only the
ones from our own code. **At cutover the mint owns the three list heads, they start NULL,
and every construct is ours — so the FIRST BUFFER THE RENDERER BUILDS takes the one branch
never compared against retail.** The oracle that would close it mutates the live global list
the DLL's `RecreateDynamic` walks, so it was not built while holding the directory.
*Generalise this question: any branch reachable only when OUR side owns a structure from
empty is unreachable on every DLL-present run, and therefore unverifiable by construction.*

**2. THE `IO::Encoding` BLOCKER IS MEASURED LIVE ON BOTH SAVE AND LOAD.**
`CompressedMemoryStream::CompressedMemoryStream` is `calls=1` in the save cell **and** the
load cell, and its first two statements resolve `??0CompressedStream` and `??0MemoryStream`
— **both unported and unminted**. So **at cutover the first save and the first load each
call through NULL before doing any work.** It cannot be closed by porting: the shared
encoding vtable at `0x1801554a8` has **no exported name and no row the mint can carry**.

**Two ports have ZERO CALLERS anywhere and are reported UNVERIFIED, NOT CLEAN:**
`{Index,Vertex}Buffer::PlatformDestruct` never register a site in any cell — their only
callers are the unported `~IndexBuffer`/`~VertexBuffer`. The shared body is verified 10× via
`ImageBuffer` and the link offsets 10× each via the construct siblings, but **the
composition is unproved**.

**A defect in the lane's own probe, kept on the record:** the first branch counter read
**exactly INVERTED**, because `RtDiffReport` does not register a site, so "report if taken,
register if not" printed only the branches that NEVER ran — which reads like neither branch
ran. **A coverage instrument that is itself wrong reads as coverage.**

### ⛔ "Caches a null forever on the first call" is a CLASS of ~22 — lane E3, 2026-09-08

`SingletonSlot` is **not one function**. Under `runtime/` + `dx11/` + `engine/` there are
**111 function-local statics caching a Runtime resolve**; narrowed to the ones resolving
exported DATA (`?_singleton@`, `?_typeInfo@`, `?_firstTypeInfo@`) — the symbols that stop
existing at cutover — **~57 sites covering at least 14 distinct singletons** (MemoryAllocator,
IO::FileSystem, IO::Persistence, Object::Factory, Object::InternalFactory,
Achievement::AchievementManager, Audio::Device, Video::Device, Video::Configuration,
Input::Device, Input::Configuration, Engine::MainLoop, Graphics::AnimationManager,
Save::SaveLoad) plus ~10 `_typeInfo`s. That matches this document's own census of 17 + 14
among the 319 unported.

| Form | Count | Behaviour on a null |
|---|---:|---|
| function-local `static` — **FROZEN** | **22** | the first caller fixes the answer **for the process**; a later registration cannot repair it |
| `if (!g_x) g_x = Resolve(...)` — **RETRY** | 5 | re-resolves each call; a registration that lands later still works |

**The correct form is ALREADY IN THE TREE** — `runtime/src/object/internal.cpp:143` and
`runtime/src/object/type_system.cpp:65`. Nothing needs inventing; it needs applying to the
other 22.

**Severity splits by DISTANCE FROM THE CAUSE, not by count:**
* 33 of the 34 single-line frozen statics resolve `??_7` vtables. A cached null there is
  stamped into an object and dies as **EXEC-of-0 at the first virtual call** — loud, and the
  VEH handler already names it.
* The dangerous subclass is **data slot + a guard that swallows the null**. Confirmed at
  three sites so far: `?_singleton@MemoryAllocator`, `?_singleton@Device@Audio`
  (`audio.cpp:879`, `return slot ? *slot : nullptr;`), and the `video/device.cpp:123` pair
  pending a caller check. **Only 4 of 22 were checked — that membership is a FLOOR.**

`_singleton@MemoryAllocator` ranks worst because **cutover executes it FIRST**: it fails
before any other subsystem gets a chance to fail more loudly, and `null+0x640` names nothing.

**The property that makes this fixable now:** the retry form is **behaviour-identical while
the DLL answers**, so unlike the mint and unlike every other cutover repair, this one is
**verifiable TODAY** rather than on the day no instrument survives.

**Also recorded as gate-step-4 items belonging to no current lane:** §5.2
(`Shutdown`/dtor/`operator=`/accessors) and §5.3 (the 5% perf gate, three runs per config on
the perf exe). **The allocator is READY TO ARM AND IS NOT ARMED.**

### Finding 3 — ORACLE-INDEPENDENT, scheduler family ELIMINATED, and the stack moved AGAIN

Lane N, 2026-09-08, exe `01:33:19`, self-evidencing `[WorkSchedProbe] ARMED`.

**1. THE CRASH IS ORACLE-INDEPENDENT — PROVED.** The stress cell with `BANISHED_RT_DIFF`
UNSET faulted anyway (rc=3, ACCESS_VIOLATION). **Finding 3 is a real defect in the port, not
an artifact of the diff harness.** A second oracle-off run of the same cell came back rc=0,
so intermittency is unchanged at ~4 in 10 — the oracle is not what makes it fire.

**2. THE SCHEDULER FAMILY IS ELIMINATED.** `8e30calls=280 unfiled=280 entryNotFound=0
bucketNotFound=0 bucketRootNull=0` — every erase found its entry and erased it, and no call
reached either state retail `0x1400f8f5e` would have faulted on. **The erase-no-op mechanism
derived from source is NOT the cause**, and the five-tree enumeration has ruled out the
family rather than found the culprit — the outcome the probe was built to be able to state,
reported with the weight a hit would have carried. The `InternalFactory` handle-table audit
was also run **during teardown for the first time** (its only scan point had been inside
`Update`): `bad=0` at every checkpoint including shutdown-enter (count=92,777) and
post-entitylist (count=738), `freed-while-registered total=0`. So mode (A) of
`internal_factory_audit.h` — *"the next `~Internal` writes `*(int*)(freedPtr+0x08) = idx`"*,
which has **exactly this crash's signature** — did not occur either.
⚠️ **CAVEAT, not buried: both clean tallies come from runs that did NOT crash.** A clean
probe on a non-crashing run is much weaker than a clean probe on a crashing one.

**3. A DEFECT IN THE PROBE ITSELF, and the general rule it yields.** The first version armed
its exit tally via `atexit` — **and the crash handler ends in `ExitProcess`, which skips
`atexit` entirely.** So on the one run where the probe fired, it printed `ARMED` and then
NOTHING, making the all-zero outcome **indistinguishable from "the probe is not in this
binary"**. Both tallies now print from the crash handler too, beside `Dx11SeamReport`, which
already existed for exactly this reason.
> **A diagnostic whose output is destroyed by the event it diagnoses is not a diagnostic.**

**4. THE STACK MOVED AGAIN — third distinct path.** REDZONE named the corrupt block this
time: `CORRUPT block @00000238035f8248 tag=0x0000000200000010 size=34359738432`, and the
path symbolizes to `GameUpdateLoop_Shutdown` → `EntityList_Shutdown_c3d30 **+0x22e**` →
`EntityList_RemoveEntity +0x3b3` → `ComponentEntity_Dtor` → `DtorBody` →
`RtObject_InternalBasePtr_DeletePtr` → `Inert_ScalarDtor` → `MemFree` → `FreeLocked`.
**Note `+0x22e`, not the `+0x104` mark loop reported before — this is `EntityList::Shutdown`'s
LATER DRAIN LOOP.** The corrupt tag is `{low=0x10, high=2}`: the same two-small-integers
shape as the other three, now on an allocator **boundary tag** rather than a link.
`Inert_ScalarDtor` is the shared stand-in destructor for components with no real dtor, and
its own source comment describes the mode-(A) corruption **which the audit measured absent**.
**The shape matches; the documented mechanism is measured absent; there is no replacement
mechanism yet.**

⚠️ **Three distinct stacks have now been reported for this fault** (lane M's `Tutorial_Dtor`
→ `TreeNodeFree`, disproved; lane N's `WorkComponent` task-cleanup, family now eliminated;
and this drain-loop path). **It should not be assumed these are one defect.**

### The frozen-cache class: 481 not 206, converted 90, live proof still OWED — lane H, `8055bf7`

**The denominator was wrong in the dangerous direction.** The first pass said **206** frozen
sites. `RT_ORIGINAL_ONCE` and `RT_EXPORT_ONCE` are **macros that manufacture this exact
idiom**, and their 282 call sites contain no `ResolveExport(` text, so the tool saw none of
them. Real population: **FROZEN 481 · RETRY 79 · DIRECT 1015 · 1,575 doors.** The 481 splits
212 `RT_ORIGINAL_ONCE`, 64 `RT_EXPORT_ONCE`, 139 hand-rolled code, 40 hand-rolled `??_7`,
**26 hand-rolled exported DATA**.
*Fifth distinct form of the wrapper blind spot this wave: **a macro that manufactures an
idiom is invisible to a checker keyed on that idiom's text.***

**(1) The swallowing subclass is 26 sites, ENUMERATED not sampled** — all exported DATA
(`?_singleton@`, `?_typeInfo@`), the only kind where a cached null is *swallowed* rather
than dying loudly as EXEC-of-0. The earlier "3 of 4 checked" **both undercounted the
population and overcounted its confidence**. Ranked by cutover execution order:
**tier 1 is `?_singleton@MemoryAllocator` ALONE** (provably first — nothing can allocate
before it); tier 2 the nine `Engine::Startup` singletons (Video::Device ×3,
Video::Configuration ×2, IO::Persistence ×3, Input::Device, Audio::Device, Engine::MainLoop);
tier 3 the eleven first-frame/first-load ones. Tiers 2-3 are marked APPROXIMATE.

**(2) Converted with a NAMED REFUSAL.** New `RtResolveCached()` caches a **hit** (steady
state is one plain load — no `GetProcAddress` in the hot path), **retries** a miss, and
**names it once** with a counter. 26 sites → `RT_DATA_SLOT`; then **one edit to the
`RT_EXPORT_ONCE` macro body moved 64 more that needed no touching**. **90 sites left the
frozen class.**
Also corrected a stale comment at `video/device.cpp:120` claiming the registry *"holds
function redirections only, and nothing has registered — or can register — a data slot before
cutover."* **Both halves false**, and left standing it would have argued a future reader out
of lane C2's mint. **This lane and C2's are two halves of one repair: the mint is only
reachable because these sites now retry.**

**(3) Denominator, before and after** — the audit now counts the macro spellings, because
otherwise a converted site **vanishes**, which reads exactly like a clean pass:
```
BEFORE (clean worktree at HEAD, same tool):  FROZEN=417  RETRY=143  DIRECT=1015  doors=1575
AFTER  (working tree):                       FROZEN=391  RETRY=169  DIRECT=1016  doors=1576
```
FROZEN −26 / RETRY +26 = the conversions; DIRECT +1 / doors +1 = the helper's own resolve.
**The tool CANNOT show the macro edit** — it keys on a spelling that did not change, only the
contract behind it — so contract-level the move is **481 → 391**, said explicitly rather than
letting the numbers imply the macro edit did nothing. Audit-vs-grep reconciles exactly:
212 sites + 5 comment-only lines + 1 statement carrying two uses = grep's 218.

**(4) Load-bearing nulls — `RT_ORIGINAL_ONCE` IS the `RemoveGraphs` equivalent, at 212
sites.** It is the oracle door, registry-blind BY DESIGN; converting it would make every
oracle compare ours against ours, then retry `GetProcAddress` against an absent module at
cutover. **Left untouched.** Self-resolve candidates: 455 names are resolved-and-registered
tree-wide (that is the design); narrowing to *the same file doing both* — the actual
`RemoveGraphs` shape — leaves **3** (`loops.cpp:947`, `widgets_input.cpp:194`,
`thread_win.cpp:142`), all checked at body level, **none recurses**, none in the conversion
set.

**✅ LIVE PROOF PASSED 2026-09-08** (lane H, `43c0235`, treatment stamp `01:46:22`; self-evidencing
— treatment emits `[RtResolve]`/`[RtResolveEarly]`, prefixes no earlier binary can produce, control
emits neither). **10 runs, 325,855,883 diffed calls on the treatment, 0 divergences, `[RtResolve]
0 misses` on all six treatment runs.** H3 stress ran clean (129.2M calls, 0-byte crash log). All 26
converted data slots and 64 `RT_EXPORT_ONCE` sites resolved every time, so the retry path was never
taken and **the conversion is inert while the DLL answers** — which is exactly what it had to prove.
Denominator re-measured against the new tip: `417/143/1015 → 391/169/1016`, identical to §4, so lane
N added no doors and the doc needed no correction.

⚠️ **THE LANE'S OWN PASS CONDITION WAS NOT MEASURABLE, and it caught that before reading anything
into it.** It had specified "the `[RtDiff]` SITE COUNT must match a reverted-build control". Treatment
331 vs control 329 — a mismatch. It then **ran the control against ITSELF: 329 / 331 / 329.** *The
same binary, same cell.* The count is **nondeterministic ±2**, so **no pair of single runs could ever
satisfy that criterion, not even two of the identical binary.** Strict-sounding and unusable — the
shape this wave has caught five times in other tools and now once in its own.
**Replacement: compare the SET, not the count.** It survives the noise AND names the offender —
diffing two control runs immediately identified the ±2 as `Math::Transform::Identity()` and
`Video::Context::SetInverseScale`. Stable core (∩ of 2 runs) 329 both sides, union (∪) 331 both sides,
**byte-identical sets, `diff` empty on both**; the deeper Save9 cell 354 sites on both binaries,
identical set.
➡️ **Handed on: the two flaky sites make ANY cross-build site-count comparison noisy by ±2. Anyone
reading a 2-site delta as signal will chase nothing. Compare sets.**

**✅ THE EARLY-SNAPSHOT CLASS IS MEASURED NOT-LIVE ON THESE PATHS.** Identical on all three cells:
**3 early registry-aware resolves, 0 of 3 registered now → ZERO live bypasses.** The design is what
makes that meaningful — the counter **re-queries the registry at summary**, separating "resolved
early" (common, mostly harmless) from "resolved early AND thereby missed a port that exists" (the
actual defect). All three are structural: `InstallAllocatorLock()` and `InstallFreeLock()` run before
`RuntimePortInit()` **by design** and have no port to miss while `BANISHED_RT_ALLOC` is off, and
`?_singleton@Factory@Object@@` is resolved from inside a `*PortInit()` body — port-init code resolving
during port-init is early **by construction**. Nothing fixed; there was nothing to fix.
**`ResolveExportRaw` is deliberately NOT hooked** — that door is *supposed* to reach the shipped body,
so an early raw resolve is not a bypass, and hooking it would have inflated the count with 212
`RT_ORIGINAL_ONCE` sites correct by design.
⚠️ **Two limits, stated so the number is not over-read:** the counter sees only the three
registry-aware doors, and **"not live on these three paths" is not "not live anywhere"** — the class
stays OPEN with no instrument beyond this counter.

### ⛔ NEW LIVE DEFECT CLASS: the EARLY SNAPSHOT — "frozen → retry" does NOT fix it

Lane H, 2026-09-08, flagged against its own work.

**Retry only re-resolves a NULL. A non-null answer is still frozen.** So a site whose first
call happens **before `RuntimePortInit()`** captures the DLL's address permanently and
**bypasses a port registered afterwards**. That is the port-bypass defect
(`reference_bypassed_port_needs_a_source_checker.md`) frozen in — and unlike everything else
in this section it is **LIVE TODAY, not at cutover**.

**Anyone reading "frozen → retry" as a fix for early snapshotting will be wrong.** The 90
converted sites are protected against a *null* resolve, not against an *early* one.

Why no existing instrument sees it: the site is spelled correctly, resolves through the
right door, and `port_bypass_audit` finds nothing — **only the ORDERING is wrong**. A source
checker cannot see it, and the diff oracle reports the port clean because our body never
runs. It joins the list of things visible only to a runtime observation.

**The material is the DIRECT bucket (1,016 sites) plus its ordering.** Needs its own lane and
its own audit. **Suggested measurement, since the machinery now exists:** `RtResolveCached`
already counts and names its misses — have it also record any resolve that happens **before
`RuntimePortInit()` returns**, which converts "is this class live?" from an argument into a
number, on an ordinary run, today.

**Also carried forward, belonging to no current lane:** WAVE9_LANE_E2 §5.2 (the allocator's
`Shutdown`, `~MemoryAllocator`, `operator=`, `GetAllocationSize`, `GetAllocatedSize`, `Get`
— never exercised by the shadow) and §5.3 (the 5% `sim ms/iteration` gate, three runs per
config on the perf exe). **The allocator is READY TO ARM AND IS NOT ARMED.**

### Finding 3 — the "latent wild write" reading is RULED OUT; three stacks, and lane M was right

Lane N, 2026-09-08, `85f1eca`. **All three probe tallies now come from runs that ACTUALLY
CRASHED** — the caveat that made the earlier clean tallies weak.

**1. THE HEAP IS INTACT WHEN TEARDOWN BEGINS.** Bracketing teardown with the allocator
boundary-tag walk (`BANISHED_TEARDOWN_HEAPWALK`) on a run that crashed:
`'shutdown-enter': no tag corruption` and `free-list forward links also intact`.
**So "one latent wild write during the sim with a moving victim" is RULED OUT. The
corruption is produced by the TEARDOWN CASCADE ITSELF** — for stack A, in the window between
the 135th and 136th `8e30` call. That is the discrimination that was worth more than another
repro, and it halves the hunt.

**2. Eliminations now hold ON A CRASHING RUN:** `WorkSchedProbe 8e30calls=136 unfiled=135`,
all three failure counters 0 — the 136th call IS the one that died, so **the scheduler family
is eliminated on the run that faulted**. `ObjAudit` at shutdown-enter `count=92492 bad=0`,
freed-while-registered 0 — **mode (A), whose signature matches this crash exactly, did not
happen.** And the fault reproduces with `BANISHED_RT_DIFF` unset: **oracle-independent**.

**3. THREE DISTINCT STACKS, tabulated rather than merged:**

| | Path | Corrupt value |
|---|---|---|
| **A** | `EntityList_Shutdown +0x104` (mark loop) → … → `8e30`, faulting on `int k = *e` | `{low = 2 or 0x22, high = that run's heap prefix}` |
| **B** | `EntityList_Shutdown +0x22e` (drain loop) → `RemoveEntity` → `Inert_ScalarDtor` → `FreeLocked` | boundary tag `0x0000000200000010` |
| **C** | `Tutorial_Dtor +0x330` → recursive `TreeNodeFree_2df90` | node ptr `0x0000000200000020` |

**A and C are the same shape** — a live pointer's LOW DWORD replaced by a small integer, and
for A the high half is provably that run's heap prefix. **B is a boundary tag, so B may be a
DIFFERENT DEFECT**; the data does not settle it and lane N does not pretend otherwise.

**4. A reading lane N WITHDRAWS of its own:** that `0x200000010` is a free-block tag
`{size,2}`. The allocator stores size in the low 41 bits
(`blkBytes = (tag & 0x1ffffffffff) * 4`, `redzone_alloc.cpp:867`), so a 16-byte block tags as
**4**. It is genuine corruption, not a misread tag.

**WRITER STILL NOT FOUND, and not forced.** What the next lane inherits: the victim is one
instruction; the write shape is **a 4-byte store of a small integer over a pointer's low
half**; and because of result 1 **a hardware write watchpoint only has to cover TEARDOWN, not
1,200 frames of sim** — which makes `standalone/hw_watch.cpp` cheap in a way it was not this
morning.

### The two menu crashes — DISPOSITIONED, plus a boot-path blocker on no gate row

Lane M, 2026-09-08, source + disassembly.

**4(a) Goals — NOT A DEFECT, NOT REACHABLE, NOT A CUTOVER BLOCKER.** Retail derefs it
unguarded too: `0x1400698ba: MOV RAX,[0x1401c3dd8]; MOV RAX,[RAX+0x150]` — two MOVs, one `*`
in C, exactly what `goaldialog.cpp:98` writes, and retail's own `MainMenuDialog::OnCommand`
dispatches id 10 straight to `GoalDialog_Ctor` with **no world check**. **Our port is
faithful including the missing guard**, so a null check here would be the forbidden fix.
Retail's protection is not a guard — **the button does not exist**: `MainMenu.rsc` places
seven buttons and `buttonGoal`, though *defined* as a `ButtonDescription`, appears in **no
`_elements` list anywhere in the shipped resource tree**, `Pause.rsc` included. Only
`BANISHED_AUTOMENU_BUTTON=10` produces it, by injecting the `OnCommand` directly.
⚠️ **Correction to a closed claim:** `project_achievements_menu_two_defects.md` records
"ACHIEVEMENTS + GOALS BUTTONS CLOSED". Achievements is real (`buttonAchievement` is placed,
`menus_btn8` clean). **The Goals half wired a case for a button that is never placed** — not
a defect in the closure, but *a claim broader than its evidence*.

**4(b) Mods — the CRASH is harness misuse; the EMPTY LIST is real and open.** `GetMod`
(`0x18000da80`) is four instructions and cannot fail (`MOVSXD; SHL 7; ADD [RCX+8]; RET`), so
`RAX=0x80` **proves a valid singleton holding an EMPTY array, not a null one** — `GetCount`
returned 0 without faulting, which a null `this` could not do. Retail's UI never raises a
click for a row that does not exist, so guarding it would mask nothing. `Loader::Load`'s
`RefreshList` guard was checked against retail instruction by instruction
(`0x1800dc07d`-`0x1800dc0ad`) and **our port is equivalent** — retail's extra `TEST DL,DL`
branches are implied by the outer `CMP DL,AL; JZ`. So the empty list is not that port.
**Reachable at cutover: crash NO, empty list YES — every boot.** Cell needed:
`BANISHED_AUTOMENU_BUTTON=7 BANISHED_AUTOMENU_CLICKS=p,101,m6`, reading
`RefreshList`/`SetupFileSystem`/`pkm` in the RAW log (the archived cells cannot answer it —
the distillation dropped the `[ModProbe]` lines).

**⛔ 4(c) A CUTOVER BLOCKER ON NO GATE ROW — and it is a CLASS.** Verified independently.
`rt_data_mint_tables.inc:1470` mints `?_singleton@Modification@IO@@2PEAV12@EA` with
**`ctorInit = nullptr`**. `MintConstructedStatics()` skips every row with no `ctorInit`, so at
cutover it is **8 zeroed bytes nothing ever writes** — while `RegisterMintedData()` registers
every storage row **by export name** (`rt_data_mint.cpp:559`), so `ResolveExport` returns OUR
storage instead of the DLL's. `ModificationSingleton()` returns `*p` = **null**, and
`RtEngine_Loader_Load` — registered at `0x1800dc050`, **ON THE BOOT PATH** — dereferences it.
**It cannot bite today**: the mint only registers under `BANISHED_RT_MINT=1`, which is
cutover-only, so no run we have done could have caught it and none we can do will, until the
day it matters.
**Why it is a class:** a storage row is safe only if something WRITES it after cutover —
(a) a CRT initialiser we reproduce (**3 of 31 rows**), (b) a PORTED function that writes the
slot as retail does, or (c) **nothing, in which case it is zero forever**. Lane E3 found an
instance of (c) independently — `?_singleton@MemoryAllocator`, whose `Construct` does
`if (slot) *slot = self` so the write is **skipped without a word**. **Two independent
instances, from two lanes, by two routes.** Enumeration assigned to lane C2: name the writer
for all 31 rows and classify each. **The shadow may already show this** — a row printing
`DLL=WRITTEN` but `minted=zero` at EXIT is exactly a category-(c) candidate, and that data is
in a report already read.

### `IO::Encoding` UNBLOCKED — a vtable can be minted by ADDRESS — lane C2, `c77b26f`

**"Unexported therefore unmintable" was half wrong.** A vtable needs CONTENTS and an ADDRESS,
not a name, and `RegisterRuntimeAddr` / `ResolveAddr(Module::Runtime, x)` key on exactly
that. `0x1801554a8` is now minted with `symbol == nullptr`, **5 slots**, and the count
carries the *stronger* of the two proofs — the table ends **exactly** where
`??_7BinaryFileStream@IO@@6B@` begins, so the live adjacency re-check (`7d25f9a`) checks the
same half of the proof here as everywhere else. It is a table **head**, not an interior:
`0x180155488..0x1801554a0` are four `_purecall` slots of the abstract base.

⚠️ **MINTING IS ENABLING, NOT SUFFICIENT** — stated by the lane rather than left to be
discovered. All five slot targets are unexported `.text` internals and **none is ported**, so
all five mint as **NAMED TRAPS**. That converts the cutover failure from *stamping NULL and
taking an EXEC-of-0 several frames from the cause* into *a trap naming the vtable and slot*.
A better failure, not the absence of one. **It does remove the stated reason the two ctors
could not be ported:** `??0CompressedStream` (`0x1800aa3d0`) and `??0MemoryStream`
(`0x1800ae710`) are **both exported** and are now ordinary port work — returned to lane W.

**FIVE consumers, not two:** `??0CompressedStream`, `??0MemoryStream`, `??0BinaryFileStream`,
`UserInterface::SaveAllSettings`, `DialogInstance::OnOpened` — all stamping it at `+0x00` of a
fresh heap object that then goes to `stream+0x10`, the identical seven-instruction shape at
all five sites. Porting the two ctors leaves the other three trapping.

**THE GENERAL FORM — 30 sites, now 0 uncovered.** This is the round-5 bug one door over:
`RegisterRuntimeExport` covers only NAME-asking callers, and **30 sites in this tree ask for
exported DATA by ADDRESS**. The mint now registers **both doors for every row**: 27 are a
minted row's base; **2 are an INTERIOR FIELD of `?_loader`** (`+0x08` runFlags, `+0x28`
hFinished, read by `engine_funcs.cpp`) and are aliased explicitly, since exact-match
registration does not cover an offset; **1 is an unexported `.data` static with no row at
all** — `0x1801fc6d0`, `IO::Modification`'s `HMODULE`, `FreeLibrary`'d at shutdown — now
minted.

**Census blind spot, fourth distinct shape:** the two ctors are ABSENT FROM THE CENSUS even
though our ported `CompressedMemoryStream` ctor calls them — it reaches them through
`RT_EXPORT_ONCE`, **a macro**.

**Predicted deltas, written BEFORE the run** (baseline 437 slots / 47 vtables / 165 fields /
0/0/0, clean both phases): vtables **47→48**, ground-truth **437→442**, storage rows
**30→31**, minted-from-our-code **unchanged** (all 5 new slots are traps), `purecall`
unchanged, `BaseTypeInfo` **165**, MISMATCH/suspects/absent **0**, plus a new line
`carried by ADDRESS : 1 row(s)`. **A move anywhere else is a finding, not noise.**

⚠️ **LIMIT, stated rather than glossed:** the address-door registration **only executes under
`BANISHED_RT_MINT=1`**, which is cutover-only, so **no shadow run exercises it**. Its evidence
is the static 30-of-30 coverage table plus one map insert per row. **Do not read the next
clean shadow run as having proved it.**

### ⛔ 25 of 32 MINTED STORAGE ROWS ARE A NULL DEREF ON FIRST BOOT — lane C2, `e3c5c88`

`tools/rt_mint_writer_audit.py` scans the DLL's `.text` for rip-relative stores landing
inside a minted row, attributes each to its containing function, and cross-references the
census. **All 32 rows, enumerated, not sampled:**

| | count |
|---|--:|
| **(a)** reproduced CRT ctor | 3 — `_firstTypeInfo`, `_loader`, `_rng` |
| **(b)** a PORTED writer | 4 — `_frameIndex`, `AnimationManager`, `MainLoop`, `SaveLoad` |
| **(c)** writer exists, NOT ported → **zero forever** | **23** |
| **(?)** no writer found at all | **0** — both were found 2026-09-08, see below. ⚠️ **Both were found by a CRASH, not by a search, and the first one's mechanism is NOT what the integrator first recorded.** The initial reading was "the ported `Engine::Startup` replaced the shipped one without calling `Timer::PlatformStartup`". **Wrong: it does call it, and all five of our Timer values were correct.** `?Startup@Timer@System@@` resolves to lane B3's port, which by deliberate design **OWNS its own five statics rather than publishing them into the exported slot**, so the DLL's `0x1801fe730/738/740` stayed zero and `MainLoop::Run` — still the shipped body — divided by one of them. The rule is therefore sharper than "reproduce the side effects": **a ported function that OWNS storage instead of PUBLISHING it is invisible until an UNPORTED CONSUMER reads the copy it never wrote.** No instrument we have can see that — the mint, the census and every door would call lane B3's Timer correct, *because it is*; the defect lives only in the seam between a ported writer and an unported reader, and it vanishes once both sides are ported, which is precisely why it had to surface now. Fixed in `timer.cpp` (`PublishTimerStatics`, five `RT_DATA_SLOT` mirrors) rather than in `Engine::Startup`, so every future caller inherits it; the same `IDIV` reads our own minted zero at cutover, so this was never DLL-present-only. A bounded sweep of the same class over all seven ported callees of `Engine::Startup` found **exactly one** case. `?_errorString` ← `?SetText@Error@System@@` (0x1800c5fa0, ord 3982), which **APPENDS** — it walks to the NUL and concatenates, which is why a store scan attributed nothing to it; `game_main.cpp:482` reads that buffer to report why Startup failed, so leaving it zeroed silences our own diagnostic. |

`Modification` and `MemoryAllocator` — found independently by two lanes from two directions —
are two of the 23. The four (b) rows were verified **by reading our ports**, not by trusting
a census flag.

**⛔ MY PROPOSED SHORTCUT WAS WRONG, and the lane showed why.** I suggested the shadow's
`DLL=WRITTEN` + `minted=zero` at exit would flag category (c). It cannot: **with the mint
unregistered — every run we can do — a PORTED writer (b) and an UNPORTED writer (c) both
write the DLL's copy and leave ours zero. Indistinguishable BY CONSTRUCTION.** The recorded
"23 written by the DLL / 3 of ours" is a **superset containing every (b)**: it flags ~25 rows
and localises none. So this class was **not** visible in a report we had both read — what was
visible was a superset with no discriminating power. **The discriminator is static** — who
writes the slot, and is that writer ours — which is why this is a tool and not a log line.
What the shadow line *does* prove is narrower: which rows get written **at all** on a route.
Necessary, not sufficient.

**Both controls caught something, and both in the DANGEROUS direction (toward false safety):**
* **Negative control** — 17 store sites for 9 symbols read independently from Ghidra's xrefs
  and re-checked every run, with the tool refusing to report if the byte scan cannot
  reproduce them. It immediately caught the scan missing **`FF 05` (`INC dword [rip+d32]`)** —
  how `_frameIndex` is written: **incremented, never assigned**, so a store-only scan called
  it unwritten. **A false (c).**
* **Attribution** — nearest-preceding-export credited `??0Scene@Picking@@`, a *ported*
  function, with the CRT initialisers' writes to `_firstTypeInfo`/`_loader`/`_rng`, moving
  rows from (c) into (b) and **hiding the bug**. `.pdata` was the obvious fix and was
  **REJECTED WITH A MEASUREMENT** rather than adopted because it sounded authoritative:
  6,045 entries that skip frame-less leaf functions, so `??0BaseTypeInfo`'s own store falls
  outside every range and the row loses its real writer — the same dangerous direction. The
  initialiser array is exact, so it is carved out by address instead.

**DISPOSITION, adopted as wording: PORT THE WRITER, WITH THE SINGLETON WRITE AS A NAMED
ACCEPTANCE CRITERION.** Almost all 23 are "the writer is a ctor nobody has ported yet", and
those objects must be constructed by someone after cutover regardless — so the port is needed
anyway. What the enumeration adds is that **the port must also write the singleton**; a port
that forgets reproduces lane E3's silent skip exactly. This is the one place where a
normally-complete port is still wrong.

**BOTH (?) ROWS ARE ANSWERED — 2026-09-08, lane ENG, and neither disposition is "dead".**
They were found by disassembly, not by re-running the scan, which is why the scan missed them:
in both cases the store is inside a function the scan had, but its *value* comes from an OS
call, so it does not look like a constant initialiser.

* **`?_systemInitialTick@Timer@System@@0_JA` (0x1801fe738)** — the writer is
  **`?PlatformStartup@Timer@System@@CA_NXZ` (0x1800ca860)**, `QueryPerformanceCounter(&it)` at
  its tail, together with `_systemTicksPerSecond` (QPF) and `_systemSecondsPerTick` (1/QPF).
  Its ONLY caller in the shipped DLL is `Engine::Startup` (0x1800db9d2), by a direct internal
  call. **Not dead — load-bearing:** `Engine::MainLoop::Run` divides by
  `_systemTicksPerSecond` at 0x1800dd3f9, so a zero there is `INT_DIVIDE_BY_ZERO` on the first
  frame. That is not hypothetical; lane ENG's `Engine::Startup` port caused exactly that
  crash DLL-present, and the fix is in `runtime/src/system/timer.cpp`
  (`PublishTimerStatics`).
* **`?_errorString@Error@System@@` (0x1801fc720, 0x2000 bytes)** — the writer is
  **`?SetText@Error@System@@SAXPEBD_N@Z` (0x1800c5fa0, ord 3982)**. It is an **APPEND**, not
  an assignment: it walks to the current NUL (0x1800c5fd4-0x1800c5fe8), computes the
  remaining room out of 0x2000, and concatenates. The scan found no write because the store
  is inside a copy helper (0x1800c9020) reached with the buffer in a register. `SetText` has
  many callers, including `Engine::Startup`'s allocator-failure arm, and
  `code/Game/Game/game_main.cpp:482` READS this buffer to report why Startup failed — so
  zeroed storage is right only until something calls `SetText`, and then it is our own
  diagnostic that goes silent. **Disposition: port `SetText`** (54 instructions, one callee),
  not "leave it zeroed".

**And the three `Timer` rows are confirmed, not double-fixed.** Lane B3 does own that storage
and that was the right call — what was missing is that owning it is not the same as
*publishing* it. Generalised, because it is a class and not an incident: **a ported function
that OWNS storage instead of PUBLISHING it into the exported slot is invisible until an
UNPORTED consumer reads the copy it did not write.** Lane X's fourteen constructors are
immune because each writes through `RT_DATA_SLOT`; the `Timer` was the only ported dependency
of `Engine::Startup` that owns storage instead, which is exactly why it was the only one that
broke. `PublishTimerStatics()` now mirrors all five values into the exported slots — the
DLL's `.data` today, the mint at cutover, where the same `IDIV` would divide by the same
zero. `?_systemSecondsPerTick@Timer@System@@0NA` (0x1801fe740) is **not** in
`rt_data_mint_tables.inc` and needs a row.
**REACHABILITY IS NOT YET RANKED** — a null singleton that nothing reads is not a crash, and
which of the 23 are touched on the boot path is what decides whether this blocks lane G.

### The teardown crash is NOT a pre-cutover gate item — reclassified 2026-09-08

**Every other row on the checklist is there because its INSTRUMENT DIES AT CUTOVER**, so an
unrecorded result can never be recorded again. The teardown crash is the exception, and lane
N is what proved it: **it is ORACLE-INDEPENDENT — it reproduces with `BANISHED_RT_DIFF`
unset.** It needs no comparison against the shipped body, no residue census, no shadow. So it
is exactly as huntable the day after cutover as today, arguably more so since lane G removes
a variable. **It is a real open defect that must be fixed; it does NOT gate lane G**, and
holding the cutover for it would be holding it for the one item that loses nothing by
waiting.

**Lane N's answer to "will you name the writer": NO — and the reason IS the result.**

**⚠️ THE INSTRUMENT SUPPRESSES THE FAULT.** A silent, shim-independent boundary-tag scan
called between teardown steps (`BANISHED_TEARDOWN_BRACKET=16`) gave **10 of 10 stress runs
clean, zero faults**, against a prior rate of ~4 in 10 — **⛔ **THE p-VALUE AND THE SUPPRESSION READING ARE BOTH WITHDRAWN — lane N, 2026-09-08.** Control runs settled it: **6 of 6 clean with the bracket OFF on the SAME binary**, giving **16 consecutive clean runs** — which refutes BOTH candidate readings. **NOT "the instrument suppresses it"** (it stays silent with the scan off), and **NOT "the lanes E/H merge fixed it"** (build `02:05:47` already contained that merge and **faulted on its first run**). What remains is that the bracket code — which performs **no writes** and is inert with its env var unset — changes the code and stack **LAYOUT** of `EntityList_Shutdown_c3d30` and the timing of the cascade. So the best-supported reading is a **LAYOUT- or TIMING-SENSITIVE fault**, which is also the only one that comfortably covers the intermittency, the three victims, and one write shape landing sometimes on a pointer's low half and sometimes on a tag's high half. **The `p ≈ 0.006` was quoted against a ~40% baseline measured across a MIX of builds and env combinations; no controlled baseline was ever established on one fixed binary, so no rigorous probability attaches.** Strong informal evidence, nothing more. **NOTHING HERE FIXES THE DEFECT** — the next person will otherwise build the same bracket, watch it go quiet, and close it.

**WHY DR0-DR3 CANNOT BE USED HERE** (write this down so nobody rediscovers it): a hardware
watchpoint needs an ADDRESS, and **this victim's address is not knowable until after it has
been written** — a different block, bucket and heap offset every run. The `entitylist.cpp`
precedent (arm after first detection, catch the re-write) **does not transfer**, because that
victim was re-corrupted on later frames while this one faults **immediately on first use**.
The arm-on-detection path was built anyway (`BANISHED_WSWATCH`) and never got the chance to
fire.

**THE STRONGEST LEAD ANYONE HAS HAD, and it is a SOURCE hunt needing no directory:** the
tag-case decode is not `{size, 2}`. Verified arithmetically on the last crash —
`fault address == RBX + (tag & 0x1ffffffffff)*4` exactly — so the **LOW** dword is a
legitimate size field and it is the **HIGH** dword that took the 2, making that variant
**a 4-BYTE STORE AT `tag+4`**. That is precisely the
`reference_load_wild_write_high_dword_7fc` family: **a String/IBP release doing `dec [p-4]`
on a pointer that is not a refcounted String buffer, where `p-4` IS the preceding block's tag
high dword.**

**A control is in flight that distinguishes two readings producing identical evidence:**
*"my instrument suppresses it"* vs *"the lanes E/H merge fixed it"* — six runs with the
bracket OFF on the SAME binary. Crash ⇒ heisenbug; clean ⇒ the merge fixed it, which would be
worth more than the hunt.

### LANE G IS AN ITERATION, NOT A TERMINAL COMMIT — reframed 2026-09-08

Lane C2, ranking the 14 tier-1 rows: *"What I cannot establish statically is the ORDER of
first deref among the 14 — which row a first cutover boot hits first. Each has a reader on a
known-executing path, but that comes from where the reader lives, not an observed sequence."*

**That is the category that argues for a CONTROLLED FIRST CUTOVER BOOT** — expect to fail,
read the crash address, fix one row, repeat — **rather than more analysis.**

This reframes lane G. It had been treated as a single terminal commit gated behind perfect
prior analysis. It is better understood as a **reversible diagnostic that can be run
repeatedly**:

* **Rename `Runtime-x64.dll` aside, boot with `BANISHED_RT_MINT=1`, read where it dies, restore the DLL.**
* Fully reversible — nothing is deleted, and the DLL is restored after every attempt.
* It answers the ONE question static analysis cannot: the ORDER in which the missing pieces
  are actually reached.
* Each failure names one row, which is fixed and the boot repeated. The distance travelled
  per attempt is the progress metric.
* **`LoadLibrary` is only removed once a boot survives to gameplay** — that commit stops
  being an act of faith and becomes a recording of something already observed.

⚠️ **What this does NOT license:** it is a diagnostic, not the cutover. The pre-cutover gates
still had to be recorded first *because their instruments die* — that reasoning is unchanged,
and every one of them is now recorded. What the DLL-absent boot adds is the reachability
ordering that no instrument on this side of the cutover can produce.

### ⚠️ DIAGNOSTICS ARE SPLIT ACROSS TWO STREAMS — an absence check on one is not an absence

Lane E, 2026-09-08, after an absence check of mine nearly produced a false refutation of a
correct diagnosis.

| Prefix | Stream | File |
|---|---|---|
| `[RtPort]` | **stdout** (`printf`) | `standalone_debug.log` |
| `[RtDiff]`, `[RtAllocShadow]`, `[RtResolve]`, `[RtResolveEarly]`, `[AllocLock]`, `[RtUnresolved]`, `[RtResidue]` | **stderr** | `standalone_error.log` |

**So "grep the error log" is right for most of this project's diagnostics and SILENTLY WRONG
for `[RtPort]`.** A falsifiable criterion that names "the log" without naming the stream is
not falsifiable — it will report absent whenever the line it seeks is on the other side.
**Name both streams in any pass/fail criterion, and grep both.**

This is the same failure shape the wave has hit in five checkers — reading a subset of the
evidence and treating absence in the subset as absence in fact.

### The allocator could not arm: an ORDERING DEADLOCK, found before the run

Lane E, checking its own wiring rather than executing E4-b:

* Lane E had moved `InstallAllocatorLock()` / `InstallFreeLock()` to **before**
  `RuntimePortInit()` — correctly; it fixed the silent-lock-loss trap in §7.2.
* So by the time `RtSystemAllocatorRegister()` runs, the prologues **are** patched,
  `FindPatchedPrologue()` returns `"MemoryAllocator::Allocate (0x1800c6e60)"`, and it refuses.
* The retire step, `AllocatorLockRetireForPort()`, lives **inside** `RtAllocSwapInstall`,
  which is only reached **after the guard passes**.

**THE GUARD PREVENTS US FROM EVER REACHING THE THING THAT WOULD SATISFY IT.** E4-b would have
failed on it. The guard's actual intent is *"refuse if a FOREIGN patch owns these
prologues"* — and ours is not foreign, we know how to retire it. Fix: **retire first, then
guard**, so anything still patched afterwards is genuinely foreign and the refusal stands.
Done with a third seam (`RtSystemAllocatorSetLockRetirer`) rather than by weakening the
guard, because *enforced in code rather than by convention* is the property that makes the
atomicity claim worth anything.

**At cutover this is sidestepped by construction:** with no DLL, `InstallAllocatorLock()`
cannot resolve `0x1800c6e60`, logs `could not resolve Allocate` and returns false, so there
is no patch to detour over. The critical section is not lost — it **moves**, from the
prologue patch to the composed stubs bound to `?Allocate@`/`?Free@`. That is why the cutover
path is a **different** path and not a weaker one: the detour exists solely to redirect the
DLL's own internal rel32 callers, and with no DLL there are none, so registering the exports
IS the complete swap and atomicity has nothing left to protect.

### Build/measurement provenance — the standing rule and its limit

**Only the lane holding the game directory builds, and it quotes the exe stamp in every
result.** Adopted 2026-09-08 after two lanes built into the shared object dir 67ms apart
and produced exes of identical size — a binary that is neither lane's tree makes every
measurement on it unattributable.

**But the stamp is only half the guard, and lane M is right about the other half:** a
raced build produces a *plausible-looking stamp on a binary that is neither lane's tree*.
The signal that actually caught it was **"identical size, 67ms apart"**, not the stamp.

**The strongest provenance is SELF-EVIDENCING output.** Lane C2's run printed
`constructed 3 of the 3` — a line only the new code can emit — so the binary is proved to
be its own by what the run SAID, not by when the file was written. Prefer a result that
names itself over one that has to be alibied.

### ⚠️ Reading the residue gate AFTER lane C2 (2026-09-08)

**"An empty `[RtResidue]` list is the cutover gate" no longer means what it says on an
ordinary run.** Lane C2 (`8b7d9ae`) moved **57 DATA reads from the RAW door to the
port-aware door**, which moves them from `[RtRawResidue]` to `[RtResidue]`. That is the
CORRECT direction — a deliberate bypass becoming a measurable request — but:

* a residue run now shows **~57 new `[RtResidue]` rows, and that is EXPECTED, not a
  regression**. They are exactly the exported DATA the mint carries;
* **they only disappear once the mint is REGISTERED** (`BANISHED_RT_MINT=1`), which is a
  cutover-day setting. So the residue gate cannot be evaluated to empty on a
  DLL-present run for these symbols, and a reader who expects empty will read a
  correct state as a failure;
* the corresponding rows in lane V's 104-row table were verdicted as raw-door
  requests and **are no longer raw-door requests**.

**Also: any "ports" number derived from `runtime_census.py` in this document is a
FLOOR.** The census cannot list a port registered by ADDRESS that no resolve site asks
for — which is exactly the twelve `??_G` deleting-destructor thunks, reached only by the
minted vtable itself, and a minted vtable is not a resolve site. The mint finds them at
runtime through `RtPortLookupAddr`. **Check the DELTAS, not the absolutes.**

### The PRE-CUTOVER GATE CHECKLIST — run and RECORD each while the DLL still exists

Every one of these compares against the shipped DLL. After lane G there is nothing to
compare against, so a gate not recorded before cutover can never be recorded at all.

| Gate | Command | Status |
|---|---|---|
| **libm bit-exactness** | `mingw32-make libm-sweep` — exits non-zero on any divergence AND on a negative control that finds nothing | **PASSED 2026-09-07, recorded in `wave9_libm_sweep_gate.txt`**: 1,493,087,610 comparisons, 0 differing bits on all five; negative control fires on 4 of 5 (`sqrtf` reporting 0 for the control too is a TRUE result — the retail body is a bare `sqrtss` and IEEE sqrt is correctly rounded) |
| **Differential oracle, full matrix** | `BANISHED_RT_DIFF=1` across the harness cells | ⚠️ **NOT MET — but findings 1 and 2 are CLOSED 2026-09-08 (lane N, [WAVE9_LANE_N.md](WAVE9_LANE_N.md)); only finding 3 and the two menu crashes remain.** **Clean cells, each with a FIRING negative control** (lane M, 22 runs; re-confirmed by lane N on a clean `921eb23` tree, exe `2026-09-08 00:43:02`): main menu 181/1.1M, gameplay 314/29.1M, save+load 336/54.1M, tutorial 0 328/8.8M, menus-Achievements 195/1.7M, menus-Mods 182/2.4M. `calls=` was checked, not just `divergences=`: of 417 distinct sites exactly **two are never called in any cell** — `UI::EditElement::SetText` (a documented NO-ORACLE port) and `Audio::Scene::Serialize` (covered by lane S, not by this row). **(1) CLOSED — ORACLE defect.** `NumberElement::SetValue` (both overloads) ends in the DLL's private `CacheText`, which swaps a refcounted `String` at `+0xD0`; the prefix replay worked from a SHALLOW copy and handed retail's own body a pointer ours had already released. Lane M's detector **acceptance test was run FIRST, while the defect still existed** — self-test `ALIVE`, detector fired by name, block header `{len=0,rc=0}`, `theirs(N)==ours(N+1)` in the log, captured verbatim. Fixed by making `SetValueInt`, `SetValueFloat` and `NumberEditElement::SetMinMax` **NO-ORACLE ports**, never a `+0xd0` exclusion: tutorial-1 **10 → 0**, site count unchanged at 335. Layout corrected from disassembly (`+0xD0` owning refcounted String, `+0xC0` `Font*`, `+0xE0` `bool`, **size 0xE8 not 0xE0** — the old size made the replay read a byte past its own stack object), and the three-way `+0xD0` disagreement resolved **without touching `TextElement.h`**: a `NumberElement` is not a `TextElement`, so both headers were right about their own class. **(2) CLOSED — a REAL PORT DEFECT, a MOVED TERM.** Retail adds the inverse transform's translation LAST, one `addss` against the fully accumulated rotated sum (`0x180124B56`-`BB4`); `localOx` did, `localOy`/`localOz` read `t + a + b + c`, folding the translation in FIRST. Only visible where translation and rotated part nearly cancel — the whole 1-in-23,654 story, and why **a clean re-run could never have exonerated it**. Proved on a deterministic fixture with the **denominator unchanged**: 244,824 rays / 29,867 hits, **21 → 0**; whole run 353 sites / 109,085,103 calls / 0 divergences; still 0 after the merge. The tie-break hypothesis is disproved twice and annotated in the source so it cannot be re-proposed. **(3) OPEN — reproduced 4× in ~9 runs, THREE DISTINCT STACKS, and the corruption is produced BY TEARDOWN ITSELF.** ⛔ *An earlier claim here that "lane M's stack was wrong" is WITHDRAWN by lane N as its own error — it generalised from a single symbolized sample, and a later crash symbolizes to exactly lane M's path. **Lane M was right.*** The three paths, tabulated rather than merged: **A** `EntityList_Shutdown +0x104` (mark loop) → `ActCt_TaskCleanup_8e30`, faulting on `int k = *e`; **B** `EntityList_Shutdown +0x22e` (drain loop) → `RemoveEntity` → `Inert_ScalarDtor` → `FreeLocked`, a boundary tag; **C** `Tutorial_Dtor +0x330` → recursive `TreeNodeFree_2df90`. **A and C share a shape** (a live pointer's LOW DWORD replaced by a small integer, A's high half provably that run's heap prefix); **B is a boundary tag and MAY BE A DIFFERENT DEFECT** — the data does not settle it. **THE DECISIVE RESULT: the heap is INTACT when teardown begins and corrupt by the time it faults** (boundary-tag walk on a CRASHING run: no tag corruption at `shutdown-enter`, free-list links intact). **So "one latent wild write during the sim with a moving victim" is RULED OUT** — the teardown cascade produces the corruption, for A between the 135th and 136th `8e30` call. All eliminations now hold ON CRASHING RUNS: scheduler family (`8e30calls=136 unfiled=135`, all failure counters 0 — the 136th IS the call that died) and the handle table (`count=92492 bad=0`), so mode (A), whose signature matches this crash exactly, did not happen. Oracle-independent (faults with `BANISHED_RT_DIFF` unset). **WRITER NOT FOUND, not forced.** The victim is one instruction; the write shape is a 4-byte store of a small integer over a pointer's low half; and because the heap is clean at teardown entry **a hardware write watchpoint now only has to cover TEARDOWN, not 1,200 frames of sim** — which makes `standalone/hw_watch.cpp` cheap in a way it was not this morning. It is a verification blocker in practice, not theory — it destroyed lane N's own merged-tree confirmation run *after* the sweep had completed cleanly. **(4) OPEN:** two menu routes crash independently of the oracle. **NOT DRIVEABLE by this harness:** the in-game save WRITE path and world-teardown-to-menu. |
| **Minted DATA vs the DLL** | `BANISHED_RT_MINT_SHADOW=1` (lane C2). **Not byte-for-byte** — a filled slot holds OUR address by construction, so a byte compare would fail on exactly the slots that represent progress. It compares the MAPPING (the DLL's slot holds function X; ours holds our registered port of X, and never a DLL address), plus `BaseTypeInfo` fields by `strcmp`, excluding `next` and BSS contents. | **PASSED 2026-09-08**, exe stamp `00:24:27`, `rt_run[mintctor] rc=0`: **429 ground-truth slot comparisons / 0 MISMATCH**, 78 slots minted from our own ported code / 0 MISMATCH, **165 `BaseTypeInfo` field comparisons / 0 MISMATCH**, 0 slot-count suspects, 0 symbols absent, `clean: 0 problem(s)` at **both** `phase=init` and `phase=exit`. 14 of 46 vtables complete. `typeId INFO` 0 at init / 33 at exit, the predicted `Factory::Startup` ordering. |
| **Constructed statics vs the DLL** | `BANISHED_RT_MINT_CTOR=1 BANISHED_RT_MINT_SHADOW=1` (lane C2). The DLL's `.CRT$XC` array constructs three of the minted symbols at load; the mint reproduces them and the check compares each pair's non-null WORD SHAPE (values cannot match — different vtable, different HANDLEs, a `time()` seed). Catches a ctor that leaves the handles or the vtable null. | **PASSED 2026-09-08**, same run: **3 of 3 built, shape compared on 3, 0 SHAPE MISMATCH.** One `SHAPE-INFO` at exit — `?_loader…+0x30` (`Loader::_loop`) set in the DLL, null in ours, because nothing hands our Loader a loop. **The run also FOUND a defect**: `?_rng@Particle@Graphics@@` reported `DLL=WRITTEN` at init, refuting lane C2's earlier "never seeded" claim; it is constructed by initialiser `0x180149650` with a `_time64`-seeded MT19937. Fixed, and the sweep that missed it now matches a field reference, not only a base address ([WAVE9_LANE_C2.md](WAVE9_LANE_C2.md) §2). |
| **Allocator shadow** | `BANISHED_RT_DIFF=1 BANISHED_RT_ALLOC_SHADOW=1` on `sa-run` + `sa-stress` + autoload, plus a deliberate perturbation that must go red | **GATE STEP 2 MET 2026-09-07 — GO, meaning READY TO ARM, NOT ARMED** ([WAVE9_LANE_E2.md](WAVE9_LANE_E2.md)). All four conditions on one binary: `divergences=0` on three routes, **12,750,873** shadowed operations, all four skip counters zero, and a negative control that goes red (392,243). `BANISHED_RT_ALLOC` was never set — the heap was not taken. Three defects found, **all in the ORACLE, none in the port**: (1) it armed on `MemoryAllocator::Startup`, which has **zero callers** (MSVC inlined it into `Engine::Startup`; a rel32 scan of the whole DLL `.text` finds none, and finds 348 for `Allocate`) — it read `armed=0 observed=0 skipped_pre_startup=1,788,875`, **a green summary over nothing**; (2) the replay map deleted by writing `nullptr`, which is also its linear-probe terminator, so every delete truncated colliding keys' probe chains; (3) **the PRESCRIBED negative control cannot fail** — `minBin=0xbe` is overwritten by `AddNewMemoryRange` while arming, so accepting it would have rested the entire gate on a probe only ever seen green when broken. Replaced by a geometry control that fires on the first perturbed call. **ARENA-GROWTH HOLE — CLOSED 2026-09-08** (lane E3, `c86c6ea`, exe `2026-09-08 01:19:24`, `BANISHED_RT_ALLOC` unset in all four cells, all figures read out of the raw 1,342-line log). **G1/G2/G3 PASS on every criterion:** `ranges` ours/theirs **4/4, 5/5, 5/5**; `reserved` 64/80/80 MB matching; `grow` 3/4/4; `tableCopy` **1/2/2**; `crossRange` 0.5M/2.9M/2.1M; **12,757,581 operations, 0 divergences**, four skip counters 0, growth machinery exercised 11 times, no cell with `grow=0`, no ladder needed. `tableCopy` follows the predicted 0→2→4→6 schedule exactly. **⚠️ Had my original `N>1` been kept, G1 would have stopped at `ranges=2`, `tableCopy` would have been 0, and the `memcpy`+`release` branch would have gone UNEXECUTED BEHIND A GREEN RESULT.** **G4 is a valid RED:** `divergences=1,172,233` **and** `grow=3` — caught on the very call that created range 1 (`ours res=33558528` vs `theirs res=33554440`, difference 4,088) with the geometry terms still agreeing, which is exactly what adding `totalReserved` to the tuple bought. Same binary, one env var: 1.66M clean → 1.17M red. **BONUS:** `theirs res` = 2×16MB **+8** proves range 1's size was `want`, not `minRangeSize` — retail took the **oversized-single-request** branch too, so BOTH routes to a new range are covered. (The `COVERAGE reserved` line printed `>>20`, making it read as "every range took `minRangeSize`" — a cosmetic format bug producing a WRONG FACT; recorded.) ⛔ **BUT A NEW BLOCKER, and it is nastier than the hole it closed.** Asked of the allocator, the owns-a-structure-from-empty question has NO hot-path hole — the shadow replica IS our side owning a virgin arena, so `Allocate`/`Free`/`AddNewMemoryRange`/`GetFreeTagForSize`/`Link`/`Unlink`/`CommitToPage`/`ReleaseToPage` and **both** `ExpandMemoryRangeTable` branches have now run in exactly the configuration cutover creates. **Outside it, `Construct` (`??0`) has NEVER RUN and fails SILENTLY at cutover:** `SingletonSlot()` resolves `?_singleton@MemoryAllocator@System@@0PEAV12@EA` — **unminted exported data, nothing in the tree mints it** — and caches the result in a **function-local static**, so the first caller fixes the answer for the process and registering it later cannot repair it. `Construct`'s write is `if (slot) *slot = self`, so **with no DLL the write is skipped without a word**, `Get()` returns null forever, and the first `MemAlloc` faults on `null+0x640` several frames from anything naming `_singleton`. Fix (NOT done — unverifiable while the DLL still answers): mint `_singleton` as real storage registered before `RuntimePortInit` returns, and make `SingletonSlot()` **REFUSE RATHER THAN CACHE A NULL**. Previous specification: **ARENA-GROWTH HOLE — SPECIFIED 2026-09-08, lane E3 ([WAVE9_LANE_E3.md](WAVE9_LANE_E3.md)), RUN QUEUED.** Growth condition established against RETAIL'S BYTES, not our transcription: a rel32 scan finds exactly three `AddNewMemoryRange` call sites and **only `0x1800c6ecf` inside `Allocate` is a live growth trigger** (the others are the dead `Startup` and the inlined arming call for range 0). A second range is created iff one `Allocate` wants `want` bytes and no linked free block anywhere is ≥ `want`. ⚠️ **The pass criterion was wrong as I specified it: `N > 1` is NOT enough, require `N ≥ 3`.** Capacity goes 0→2→4 and **only the 2→4 step takes `ExpandMemoryRangeTable`'s `memcpy` + `release(src)` branch**, so `ranges=2` leaves the range-table copy unexecuted while reading as a pass. **And the question dissolved rather than being answered:** no workload reaches it (12.75M ops across three cells all ended at `ranges=1`), but **the 512 MB is OUR OWN literal** at `code/Game/Game/game_main.cpp:439`, not the engine's. So neither "test it" nor "argue it" — **shrink the arena via the new `BANISHED_ARENA_MB` (unset ⇒ 512, byte-identical) and re-run the cells we already have**, with the EXISTING oracle covering the growth path on the real allocator with the real DLL beside it. A probe would have been a worse instrument on a synthetic subject. At 16 MB route B (a single request larger than any free block) comes for free, which 512 MB can never reach. **Controls, with POWER COMPUTED — the standard the rest of this wave lacked:** effectiveness counters `grow` / `tableCopy` / `crossRange` count what ACTUALLY happened, and the driver prints `grow=0 — the arena-growth path did NOT run in this cell` explicitly. New negative control `NEGCTL=grow` bumps `minRangeSize` AFTER range 0 exists, so it is silent for the whole single-range life and diverges on the very first growth: **per-event detection probability 1, expected divergences ≥ 10⁵**, versus `=size`'s ~17 sampled. **G4 passes only on `divergences > 0` AND `grow ≥ 1`; a green `=grow` WITH `grow ≥ 1` means the oracle is blind and every green growth run must be discarded.** That conditional structure is exactly what lane N's 0-of-525 underpowered control lacked. Previously recorded: the arena-growth path never executed — a coverage control reports `ranges ours=1 theirs=1` on all three runs, so the second `MemoryRange`, the range-table realloc and every `rangeIndex != 0` comparison are unexercised by all 12.75M ops. **Close before gate step 4.** |
| **`[RtRawResidue]` verdicts** | every row ORACLE-ONLY / REAL DEPENDENCY / DATA READ / DEAD | **MET 2026-09-08** (lane V, [WAVE9_LANE_V.md](WAVE9_LANE_V.md) §7 + `wave9_rawresidue_verdicts.csv`): **104 of 104 rows carry a verdict, zero undetermined** — REAL DEPENDENCY 26, DATA READ 29 (26 minted, **3 NOT**), ORACLE-ONLY 38, DEAD 11. Round 1 read 70/29/3/2; every round-1 finding had since been acted on, so **re-running the census after the tree moved changed the answer** — the `IO::Serializer` gate repair alone moved 22 rows to ORACLE-ONLY. **Nothing was defaulted to DEAD**: a DEAD verdict required a literal-and-alias sweep finding no raw-door site, with the port-aware door and registration named. ⚠️ **OPEN, handed to lane C2 — a cutover CRASH:** `??_7MeshInstance@Graphics@@6B@`, `??_7EdgeMeshInstance@Graphics@@6B@`, `??_7EdgeMesh@Graphics@@6B@` (`mesh.cpp:271/275/279`) are read via `RtVtable()` → `ResolveExportRaw` and **stamped into slot 0 by a ported ctor**, and are NOT in the mint — at cutover that is a NULL vtable in every such object and an EXEC-of-0 on the first virtual call. **And the door is wrong as well as the table**: `RtVtable()` is `ResolveExportRaw` for *all* callers including the already-minted `??_7Base@Object@@6B@`, and the raw door is `GetProcAddress`, which never consults the port registry — so adding table rows alone would not fix it. |
| **`resolve_fallback_audit.py` reports 0 SITES** | not 0 defects — 0 **sites** | **MET 2026-09-07, lane F** ([WAVE9_LANE_F.md](WAVE9_LANE_F.md)): **0 sites, 0 defects, exit 0.** Deleted in two batches — 137 whose primary names a registered port, then the 26 still on the DLL — with the count checked against the audit after each, because a site *vanishing* reads exactly like a clean pass. Replaced by a null-resolve policy: `ResolveExport`/`ResolveOrdinal` now record and NAME every miss (`[RtUnresolved]`), non-fatal by default because on cutover day every unported symbol misses and aborting on the first one prints one name instead of the work list. |
| **`rawdoor_audit.py` reports 0 UNGUARDED-CALL** | a raw call outside a guard reaches the DLL on an ordinary run today, and is a **null call** the moment the DLL is gone | ✅ **MET 2026-09-08 (integrator) — 481 sites, 0 UNGUARDED-CALL, 0 STALE, exit 0.** The eight "enumerated false positives" below are now *suppressed by an allowlist that actually works*, and finding that out was the point. **THE ALLOWLIST WAS KEYED BY LINE NUMBER, AND ALL THIRTEEN ENTRIES HAD DRIFTED** — `rt_port.h:99` was at `:133`, `dialog.cpp:600` at `:742`, all seven `element.cpp` rows off by one. It had been suppressing **nothing**, for an unknown number of runs, and the "8 known false positives" this row quoted were raw findings re-dismissed *by hand* every time anyone read it. Lane Z surfaced one drifted key (its own insertion moved `dialog.cpp:600` to `:738`); checking the rest showed the whole table was dead. **This is the third fixed-position key to rot in this wave** (the fixed-line comparison window twice before) — the lesson is not "pick better keys" but that a key which decays with edits ABOVE the site fails SILENTLY and in the safe-looking direction. Fixed both halves: entries are now keyed on the site's own normalised TEXT, and **an entry that suppresses nothing is reported as STALE and fails the audit**, so the next drift is loud. Effectiveness control run on the new mechanism itself — perturbing one pattern yields *both* `STALE ALLOWLIST` and the site resurfacing as `UNGUARDED-CALL`, exit 1. The `value_types.cpp` entry was DELETED rather than re-keyed: that site is self-guarded by an early return and is no longer a site at all, so an entry for it would report STALE forever. **Previously (superseded):** reads 8 UNGUARDED-CALL of 469 sites, all eight enumerated false positives of one shape. Lane W found this on the integration tip while the row still said 0, and correctly did NOT touch the shared gate instruments other lanes were quoting. Of the ten it reported, **one was a real repair**: `picking/scene.cpp:778` `DiffScratchScene` had its guard in its CALLERS — the same shape already repaired in `RtDiffRandomShadow` — so the helper now early-outs on `!RtDiffEnabled()`, which is both correct and legible to the checker (`diff_only_fn_depth`). That also cleared the lone `port_bypass` BYPASS, which was the same site. **The remaining eight, read individually:** `rt_port.h:133` is the **DEFINITION of the `RT_ORIGINAL_ONCE` macro**, not a call site; the seven `ui/element.cpp` sites sit inside `if (replay.copy)`, where `ReplayGuard`'s ctor **returns early unless `rt_diff_on__`**, so `copy` is non-null only when diffing — a correct guard the checker cannot see. **Left standing rather than exempted**, because teaching a checker to ignore a shape is how it stops finding that shape. Note the population also moved 518 → 469 as lane H's macro-body conversion changed what the audit sees. Previously recorded: **MET 2026-09-08** (lane V2, [WAVE9_LANE_V2.md](WAVE9_LANE_V2.md)): **0 UNGUARDED-CALL, 518 sites scanned**, self-test green. 28 -> 0; denominator 555 -> 519 -> 518, and **555 - 519 = 36 = exactly the number of sites converted**, per file — a conversion removes a site from the population, so the drop MUST equal the conversion count, and it was checked because a vanishing site reads exactly like a clean pass. **27 of 28 were REAL DEPENDENCY** -> `RT_EXPORT_ONCE`. **1 was ORACLE-ONLY and must NEVER be converted**: `widgets_layout.cpp` `RemoveGraphs` — that function IS the registered port of its own name, so `RT_EXPORT_ONCE` would resolve to ITSELF and recurse to stack death. It was also sitting OUTSIDE its guard between `RT_DIFF_BEGIN` and `RT_DIFF_SCALAR`, calling retail every frame; now inside `if (rt_diff_on__)`. **A blanket conversion — the obvious way to drive this gate to zero — would have shipped that crash.** 0 DATA READ; **0 DEAD, because none could be PROVEN unreachable and none was asserted.** ⚠️ **Two caveats that survive the green number.** (1) `rawdoor_audit` has a MEASURED blind spot: its resolve-and-call regex is per-line, so a site whose argument list opens on the NEXT line is invisible. Measured, not guessed — 364 immediate-call sites, 83 line-wrapped, **4 of those UNGUARDED and invisible to the audit** (`audio.cpp:451,760`, `filesystem.cpp:139,145`), one of which sits TWO LINES above two sites lane R did convert *because the checker flagged those and not it*. All six converted; probe kept as `docs/runtime_port/wave9_laneV2_wrapscan.py`. **The checker is measured, NOT fixed** — the fix is to paren-match forward. (2) **Reachability is established for NOTHING here** — the evidence is structural, never "a run executed it", and the 36 conversions have never been EXECUTED, twelve of them on the save/load path. Each resolves an unported-or-detoured name so `ResolveExport` returns the same pointer `ResolveExportRaw` did, but only an autoload cell settles it. ⛔ **0 UNGUARDED-CALL IS NOT "NO NULL CALLS AT CUTOVER" — lane V, 2026-09-08.** `ResolveExport` misses the port registry for an unported symbol and **falls through to `ResolveExportRawImpl`** (`standalone/resolve_standalone.cpp:692`), which returns null with no module handle. So converting `RT_ORIGINAL_ONCE` → `RT_EXPORT_ONCE` on a symbol that has **no port and no mint entry** changes only WHICH RESIDUE LIST IT PRINTS IN. It is still a null call the moment the DLL is gone. **14 of lane V2's conversions are in exactly that state** — `?UpdateList@SaveLoad`, `??0CompressedStream`, `?Stop@Device@Audio` and eleven more. ⚠️ **AND 25 IS A FLOOR — lane W measured 141.** Residue records what a run ASKED FOR: `?UpdateSave` / `?UpdateDelete` sit in the SAME `switch` as three rows lane V did list, same door, same disposition, and appear in ZERO rows of the verdicts CSV because no cell saved or deleted. The source-side question is answered by `tools/unsatisfied_export_scan.py`: **141 names have neither a port nor a mint entry, 138 with a live site.** Reachability is established for none of them; the scan states its own blind spot (2 wrapper resolves) as a number, and its effectiveness control is that all nine of lane W's ports dropped out of it. **25 rows remain real cutover work: 11 still on the raw door + 14 migrated-but-unsatisfied.** This gate row is necessary and NOT sufficient; the sufficient condition is a port or a mint entry, which is lane B/C/E work. |
| **`port_bypass_audit.py --by-name` reports 0 on BOTH doors** | a ported function reached through the wrong door is invisible to the census *and* to the diff oracle | **MET 2026-09-08** (`b3fc8b1`, lane H): **ADDRESS door 189 sites / 0; NAME door 437 sites / 0**; `--by-name` cross-check **573 ported names, 378 raw-door statements mention one, 0 the parser did not see** (non-zero exit if that is ever not 0). ⛔ **NOT CLOSED — a THIRD face found 2026-09-08 by lane W.** `--by-name` reports 0 while missing a LIVE-PATH bypass on `?SetBounds@Geometry`: the accessor **returns** the resolved pointer and the call is one frame out, so it classifies `OK-RESOLVE-ONLY`. Lane W found it by reading, not by the checker. **Treat every 0 from this gate as "none of the shape it can see".** The two faces below were closed; the class was not. What DID catch the follow-on regression was the audit's **DENOMINATOR moving**, not its verdict — converting the six `Platform*` names turned two more `mesh.cpp` accessors into bypasses with nothing in those files changing. Previously recorded: It read "0 means none of the shape it can see", and that was true: `called` meant *an argument list follows on this LINE*, so the accessor shape — resolve into a local, call it a statement later — classified `OK-RESOLVE-ONLY`. `value_types.cpp` held **three** live-path `String::Release` bypasses and the audit reported **one**. The classifier now follows the assignment target through the **brace-matched enclosing function** (never a line window). Proved against a known answer, not by the number moving: on a scratch snapshot of the exact tree where the three existed, same 364-site denominator, **OLD tools give BYPASS=1 and NEW give BYPASS=3** — kept as `tools/fixtures/bypass_value_types_before.cpp` under `--self-test`. A second defect fell out of the cross-check: mangled names were read from the door's own LINE, so a site whose name sits on a **continuation line** — the normal formatting here — got **no verdict at all**. 76 such sites; the population was 361 when it should have been **437**. None was a bypass, but they were counted as scanned while receiving no verdict. ⚠️ **Still re-run after every LANDING, not after every edit** — `--by-name` does not change that a correct raw call becomes a bypass when a DIFFERENT lane ports its name, with nothing in that file changing (`Timer::SecondsToTicks`/`TicksToSeconds` did exactly this). |
| **H. Perf gate** | The deferred +7.7% breach. The user deferred it at the Wave 4 boundary explicitly *to this stage*. With the DLL gone the seam's `GetProcAddress`/registry indirection can be compiled out entirely, which is the main reason it was deferred here. Three runs minimum — `reference_one_perf_run_cannot_resolve_the_gate.md`. | ≤5% over the recorded baseline |

### A LIVE DEFECT NO AUDIT COULD SEE, and the oracle that passed it 14 times a run

Integrator, 2026-09-08, `c1e5e10`. The shared string-keyed BST find at `0x18002ace0` --
used by `System::Configuration`, `IO::FileSystem::Startup`, `FileSystem::SetupDirectories`
and `IO::Persistence` -- was ported with a one-index defect in the `strcmp` retail inlines
inside it. `TreeStrCmp` returned **0 for every pair of distinct strings sharing a first
byte**, which is every key in a settings map, because they are all prefixed by their
section name. Zero feeds the find's two arms as "not less than" and "greater or equal",
so the walk returned **the first node sharing a first byte**: asking for `GameLastStart`
got back the node for `GameShowWeather`.

Retail increments the index BEFORE the compare that decides whether to loop
(`INC RDX` at `0x18002ad2a`, then `CMP CL,[RAX]`), so it exits with the index already on
the mismatching byte. The port tested `a[i+1] == b[i+1]` in the `while` condition and
incremented only in the body, so on the exiting comparison it never incremented and
subtracted two bytes it had just proved equal.

**Why four waves of oracles passed it: the population was uniform.** Thirteen of the
fourteen `Game*` integers in the registry are `"0"`, and `strtol` of the wrong node's
value (`"true"`) is also `0`. Ours and theirs agreed on 13 of 14 calls because every value
in the population was the same value. It surfaced only once the registry held one non-zero
one -- `GameLastStart=2`, written by our own autostart runs.

**It was found by RUNNING.** The DLL-present cell moved 359 sites / 0 divergences to
375 / 1. One A/B on the newest lane's kill switch (`BANISHED_RT_PERSSTARTUP=0`) reproduced
it identically and cleared lane ENG. A probe printing the composed key beside the returned
node made it a two-line diagnosis: `composed="GameLastStart" nodeKey="GameShowWeather"`.

**Third live instance of the `ResolveAddr` registry rule.** `persistence.cpp` resolves
`0x18002ace0` by address and its comment states it always asks the DLL so the lane can be
judged standing alone -- but `configuration.cpp:287` registers a port at that address, so
it had been calling ours since that port landed. A **Configuration** defect therefore
surfaced as a **Persistence** divergence.

Fix verified against a register-by-register transcription of `0x18002ad04..ad3d` over
3,000,000 random pairs biased to long shared prefixes, 1,768,805 containing bytes `>= 0x80`
(the signed/unsigned split the file's banner already warned about): **0 disagreements on
the exact value**. Effectiveness control: the old version disagrees with that same
transcription on **497,647** of those pairs. Live: **375 sites / 18,320,495 calls / 0
divergences**, and `LastStart` now reads 2 where it read 0.

### `_errorString` was minted at 8 bytes and is an 8 KB BUFFER -- `4a2d449`

Found by lane ENG from the disassembly, before it ran. `?_errorString@Error@System@@0PADA`
decodes as `char*`, so the generator gave it 8 bytes; `?SetText@` (`0x1800c5fa0`) does
`LEA RBP,[0x1801fc720]` -- the address IS the buffer -- and carries capacity `0x2000`.
**MSVC encodes a static `char buf[N]` data member as `PAD`, indistinguishable from a real
pointer at the name level**, so the mangled suffix cannot be trusted for array statics.
The first cutover `SetText` would have written 8 KB into an 8-byte slot, and `SetText` is
the diagnostic path -- it fires exactly when something else has already gone wrong.

Corroborated independently, because a size read off one function is one witness: **zero**
of the DLL's exports fall inside `0x1801fc720+0x2000`, while the neighbours are packed a
byte apart either side of it (`_disableHandler` `0x708`, `_hideDisplay` `0x709`,
`_stopApplication` `0x70a`, `_rootOnlyUpdates` `0x70b`). An 8 KB hole in that density is a
buffer, not luck. Also mints `?_stopApplication@Error@System@@0_NA` (ord 4818), absent
entirely. Storage 37 -> 38 symbols, 3,136 -> 11,328 bytes, delta exactly `0x2000`.

### Cutover attempt 10 -- the wall did NOT move, as predicted

Same fault as attempt 09: `[RtUnresolved] ?SetupDirectories@FileSystem@IO@@QEAAXXZ`,
EXEC-of-0, `VCALL-of-0` with a null vtable. Lane ENG predicted exactly this -- its five
ports are all behind or beside that wall, not on it, so "no movement" was the falsifiable
call and it held. Lane FS owns the wall.

Audits on this tip: `rawdoor` **483 sites / 0 UNGUARDED-CALL / 34 DEFERRED**, `port_bypass`
**ADDRESS 201 / 0, NAME 411 / 0**, `rt_data_door` **PASS, 3 `RT_RAW_OK`, 0 UNCLASSIFIED**,
census **849 called / 616 ported / 72.6%**.

### Cutover attempt 11 -- THE WALL MOVED, and lane FS's prediction was 4-for-4

Merged lane FS (`SetupDirectories` + 11 callees) plus lanes BOOT2, OBJ2 and VID2.
Attempt 11, exe stamp `2026-09-08 08:02`:

| lane FS predicted | observed |
|---|---|
| symbol `?LoadPackages@FileSystem@IO@@` | ✅ `[RtUnresolved] ?LoadPackages@FileSystem@IO@@QEAAXXZ` |
| enclosing `RtIo_FileSystem_Startup` | ✅ `RtIo_FileSystem_Startup` |
| offset LARGER than `+0x55` | ✅ **`+0x140`** (attempt 09 was `+0x55`) |
| stack depth STAYS PUT | ✅ caller still `RtEngine_Engine_Startup +0x22C`, same depth |

**This prediction deliberately broke the previous two iterations' pattern** (both had
the frame moving down), so it was a real risk rather than an extrapolation, and it
named the falsifier in advance: a frame moving *down* would have meant
`SetupDirectories` faulted internally. It did not.

**`SetupDirectories` ran to completion, and the crash dump proves it independently of
the symbol name.** The faulting object's `this[+0x18] = 0x101` — that is the packfile
gate lane FS documented (`??0FileSystem` writes `0x100`; `Startup` sets the low byte
only AFTER `SetupDirectories` returns). And `this[+0x00]` points at a buffer holding
`C:\GOG Games\resource\`, a path `SetupDirectories` built. Two independent
confirmations that the row closed.

**⚠️ A lane FS holdback is now STALE BY DESIGN.** It declined `ExistsInPackfile`
because the packfile gate is 0 — true at `SetupDirectories` time. By `LoadPackages`
the same gate reads 1, and `LoadPackages` opens with
`CMP byte ptr [RCX+0x18],0x0 / JZ <ret>`, so the whole body is live. Whoever takes
`LoadPackages` must re-derive that holdback rather than inherit it.

`?LoadPackages@FileSystem@IO@@QEAAXXZ` is retail `0x1800ab460`, ord 3209, ~300
instructions: two array-growth loops over the package list, a BST insert per entry
keyed on name, and ten-odd callees. It takes the allocator lock directly
(`?_singleton@MemoryAllocator@System@@` at `0x1801fc718`, `+0x640`, through the
dispatch slots at `0x18014b390` / `0x18014b388`).

### FIFTH CENSUS BLIND SPOT, and it cannot see the wall the boot is dying on

Found by lane OBJ2 (2026-09-08) because its own `ported` count refused to move after
an unconditional, literal registration. Independently re-measured by the integrator,
and the honest number is **larger** than the lane's.

`tools/runtime_census.py` spells its `called` door as

    RE_EXPORT = re.compile(r'ResolveExport(?:Raw)?\s*\(\s*(?=")')

a **literal** `ResolveExport(` only. It therefore cannot see `RT_EXPORT_ONCE` or
`RT_DATA_SLOT`, both of which expand to `RtResolveCached`. Measured over
`runtime/ code/ standalone/ engine/ dx11/`:

| door | distinct names | invisible to the census |
|---|--:|--:|
| `RT_EXPORT_ONCE` + `RT_DATA_SLOT` — **live, dies at cutover** | 162 | **113** |
| `RT_ORIGINAL_ONCE` — oracle-only, correctly NOT a cutover dependency | 130 | 13 |

**So `called` understates the cutover denominator by 113, not by 106.** The split
matters and the lane's single figure blurred it: 13 of the invisible names are the
oracle door, which *should* be excluded, so quoting one number would have overstated
the real gap while understating the live one.

**The detail that makes this urgent rather than tidy:
`?SetupDirectories@FileSystem@IO@@` and `?LoadPackages@FileSystem@IO@@` are both in
the invisible set.** Those are the symbols attempts 09-11 faulted on. The census has
been unable to see the wall the entire time it has been quoted as
"the honest denominator... what has to work for reimpl-x64.exe to boot without the
DLL". Nineteen of the 113 are on the FileSystem/Factory bring-up alone, including
every ctor and dtor of the four singletons `Engine::Startup` constructs.

**Concentration:** 53 of the 113 sites are in `runtime/src/engine/engine_lifecycle.cpp`
— which is precisely the `Engine::Startup` queue this whole wave exists to get
through. The blind spot is not spread thinly; it is aimed at the wave's own target.

**NOT FIXED YET, deliberately.** Lane OBJ2 declined to change the tool mid-wave
because moving a shared denominator invalidates every other lane's before/after, and
that was the right call. Lane PKG is in flight and takes its own before/after from
this tool. The fix — teach the `called` scan the two macro spellings — lands as one
commit, with one owner, once PKG reports; and the commit must state the
discontinuity so no one diffs a number across it. Same law as
`reference_documenting_a_check_can_delete_it`: **diff the denominator across a
change, and never silently move it.**

### Scoring the round's predictions

| lane | prediction | outcome |
|---|---|---|
| ENG | no movement before FS lands | ✅ attempt 10 unchanged |
| FS | `?LoadPackages@`, in `RtIo_FileSystem_Startup`, offset > `+0x55`, depth unchanged | ✅ **4 for 4** at `+0x140` |
| OBJ2 | no movement; still `?SetupDirectories@`, same frame | ❌ **falsified** — attempt 11 advanced to `LoadPackages`, because lane FS landed on the same tip. The lane named this dependency itself; its *conditional* secondary (EXEC-of-0 inside `RtObject_Factory_Startup` at the `Set` insert, frame moving DOWN, preceded by a named diagnostic) is untested because `LoadPackages` comes first |

A falsified prediction that names its own dependency is worth more than a vague one
that survives; OBJ2's was falsified by an event it explicitly flagged.

### Three corrections the lanes made to EARLIER lanes' triage

1. **`?Startup@Device@Video@@` (0x1800f7270) and `?Shutdown@` (0x1800f7280) are
   one-instruction `JMP` thunks** onto `PlatformStartup` (0x1800f8260) /
   `PlatformShutdown` (0x1800f83f0). Lane VID declined them as "not boot path"; the
   same body IS on the boot path under the other name — our own ported
   `Engine::Startup` calls `?PlatformStartup@Device@Video@@` at
   `engine_lifecycle.cpp:719`, and it is unsatisfied. **One port closes three rows.**
   Two of VID's three stated blockers are also stale: `?SetText@Error@System@@` is
   ported (`error.cpp:286`), and `CreateVideoInterface` (0x1800f7930) contains no
   `LoadLibrary` — that belongs to `InitializeInterfaces`, whose slots
   `standalone/video_interface.cpp` already owns.
2. **Wave 4's `ReadSettings`/`WriteSettings` holdback was HALF wrong.** Its
   load-bearing reason was "**both** mutate the real registry". False for
   `ReadSettings`: its only ADVAPI32 calls are `RegEnumValueW` and
   `RegQueryValueExW`, both pure reads. `RegSetValueExW`/`RegDeleteKeyW` appear only
   in `WriteSettings`. One verdict was reached for a pair that needed two.
   `WriteSettings` stays held back on narrowed grounds (Shutdown path, not boot) —
   and lane BOOT2 found retail passes **value** names from `RegEnumValueW` to the
   **subkey** deleter `RegDeleteKeyW`, so its stale-settings cleanup is a no-op.
3. **`CreateVideoContext` (0x1800dd500): both blockers stale.** The allocator NO-GO
   is discharged (gate row E, met 2026-09-08 — the heap is ours before `GameMain`),
   and the ctor's "our code allocates what the DLL's dtor frees" argument **inverts
   at cutover**: with no DLL there is no such dtor, and leaving the ctor unported
   means the allocation never happens at all. It is a cluster, not a row:
   `CreateVideoContext` + `Context` ctor + dtor + `operator delete`.

### Cutover attempt 12 -- the port EXECUTES, and the frame moved DOWN into it

Lane PKG's `LoadPackages` merged (`aa5e494`). Attempt 12, exe stamp
`2026-09-08 08:29`:

```
#0  ???                              EXEC of 0
#1  RtIo_FileSystem_LoadPackages  +0x111     <-- NEW FRAME, our port is running
#2  RtIo_FileSystem_Startup       +0x140     <-- was #1 in attempt 11
#3  RtEngine_Engine_Startup       +0x22C
```

`[RtUnresolved] ?FindFiles@FileSystem@IO@@QEBAX...` — the row lane PKG named in its
own commit title as the one it exposes.

**The two attempts corroborate each other, which is worth more than either alone.**
Attempt 11 faulted AT `RtIo_FileSystem_Startup +0x140`; attempt 12 shows that same
`+0x140` as a live CALLER one frame up. So `+0x140` was the `LoadPackages` call site,
and the row genuinely closed rather than being skipped or short-circuited. A port that
merely returned early would have left the frame count unchanged.

The ladder is now three consecutive rungs where the fault moved to the symbol the
previous lane named in advance: `SetupDirectories` -> `LoadPackages` -> `FindFiles`.

### ⛔ TWO NULL DOORS UPSTREAM OF EVERYTHING -- lane PKG, and they outrank the row

Lane PKG re-derived the `ExistsInPackfile` holdback it inherited and it **lands the
other way**, for a reason that generalises:

- Lane FS declined it because the packfile gate at `FileSystem+0x18` is 0. **True
  inside `SetupDirectories`, and SPENT the moment `Startup` sets that byte to 1** —
  which it does before `LoadPackages`.
- From that instant **every `Exists()` takes the packfile arm first**. `?Exists@` is
  already ported (`filesystem.cpp:151`) and reaches `?ExistsInPackfile@` (0x1800ab180,
  ord 2322) **and** `?PlatformExists@` (0x1800adc70, ord 3378) through live
  `RT_EXPORT_ONCE` doors **with nothing behind either**.

That is two null calls at cutover on a path essentially everything takes, and **no
instrument reports them**: a satisfied-looking port-aware door is exactly what
`rawdoor_audit` and `port_bypass_audit` are built to pass. They are found only by
`unsatisfied_export_scan.py`'s question — "does this name have a port OR a mint entry"
— which is the one the gate rows keep proving is the *sufficient* condition while the
door audits prove only the necessary one.

**A holdback can be correct where it was written and spent two functions later.** Lane
FS's was not stale by drift and not wrong when made; the state it depended on is
deliberately changed by the code in between. Re-derive an inherited blocker against
the state at YOUR call site, not the state at the site it was written for.

### The row was SIZED WRONG in my brief, and the lane said so

`LoadPackages` is **430 instructions, not ~300**. My brief read the two array-growth
loops as writing to two different arrays; they both append to the **same** array at
`+0x20` and differ only in the source they read. It also omitted a third loop (the
path -> `{packIndex, fileIndex}` index build) and the sort entirely. Recorded because
an integrator's sketch of a row becomes the lane's starting frame, and a wrong one
costs the lane its first pass.

**Third independent witness to the census defect.** Lane PKG's `ported` count did not
move and IO stayed 37/55 despite four `RegisterRuntimeExport` calls, for the same
reason lane OBJ2's did not: the literal `ResolveExport(` door cannot see
`RT_EXPORT_ONCE`. Measured at 113 live names in `6a0d360`.

### The ladder finally has an EFFECTIVENESS CONTROL, and it passed exactly

Every rung so far has been "the fault moved, therefore the port worked" — which is an
inference, not a measurement. Lane PKG supplied the control with its prediction and it
was run:

| run | switch | fault | stack |
|---|---|---|---|
| attempt 12 | `BANISHED_RT_PKG` **on** | `?FindFiles@` | `#1 RtIo_FileSystem_LoadPackages +0x111` / `#2 ..._Startup +0x140` |
| attempt 12-AB | `BANISHED_RT_PKG=0` | `?LoadPackages@` | `#1 RtIo_FileSystem_Startup +0x140` |

**Same binary, one environment variable, and the wall moves back exactly one rung** —
the extra frame disappears and `+0x140` returns from being a caller to being the
faulting site. So the advance is caused by the port and by nothing else that changed
between builds. Cheap, decisive, and it should be run on every future rung: a kill
switch that restores the PREVIOUS rung's exact fault is the strongest cheap evidence a
port did the work attributed to it.

### Lane PKG's prediction: 4 for 4, with a falsifier LADDER

| predicted | observed |
|---|---|
| symbol `?FindFiles@FileSystem@IO@@` (0x1800ad7a0, ord 2350) | ✅ |
| enclosing `RtIo_FileSystem_LoadPackages` — **"a symbol that does not exist in the binary today"** | ✅ |
| frame moves **DOWN** one level | ✅ `#1/#2/#3` exactly as listed |
| offset small-ish within #1 (FindFiles is ~11% into a 430-instruction body) | ✅ `+0x111` |

What made it better than lane FS's equally-correct call is that it ranked **four
falsifiers by sharpness** and gave each a free tell — including the one that would have
been *more* interesting than the prediction: "no fault at all" split into *gate byte 0,
body skipped* (`calls=1`, shadow absent) versus *body ran and mounted nothing*
(`calls=1` plus a shadow printing `pkgs=0`). A prediction that tells you how to read
its own failure is worth more than one that is merely right.

### ⚠️ "Invisible to the census" is NOT "a null call at cutover"

Lane PKG re-measured the census defect from its own tip: **168 `RT_EXPORT_ONCE` /
`RT_DATA_SLOT` names, 119 invisible** (the integrator's 113 plus 6 net new from this
lane), 53 of them in `engine_lifecycle.cpp` — **more than a third of the whole defect
in one file** — and 27 reachable from `runtime/src/io/` alone, which is why IO reads
37/55 and did not move despite four new registrations.

**But the lane also drew the distinction the number invites you to skip:** a name with
no literal `ResolveExport(` spelling is invisible to the census, and that is a
*denominator* defect. It is only a **live hole** if it ALSO has no port and no mint
entry. Lane PKG's own four are now ported and still invisible. The two questions have
different instruments and different answers:

| question | instrument | current answer |
|---|---|---|
| what does our tree depend on? (denominator) | `runtime_census.py` `called` | understated by ~119 |
| what will be NULL when the DLL goes? | `unsatisfied_export_scan.py` | 151 names, 148 with a live site |

Conflating them would inflate the cutover work list by every name a lane has already
closed. Both `?ExistsInPackfile@` and `?PlatformExists@` appear on **both** lists,
which is exactly why they are dangerous: invisible to the census, satisfied-looking to
the raw-door and bypass audits, and null at cutover. **Three instruments blind to the
same two symbols.**

### Attempt 13: the boot LEAVES the name-resolution phase, and the failure changes CLASS

Lane FIND merged (`5a34d07`): `?ExistsInPackfile@`, `?PlatformExists@` and
`?FindFiles@` all CLOSE, plus two unexported `Array<Filename>` helpers.
`unsatisfied_export_scan` **152 -> 149** unsatisfied, 149 -> 146 with a live site,
**denominator unmoved at 789** and a set-diff showing exactly three names removed and
nothing added. That accounting — unmoved total plus set-diff — is what makes "+3
closed" a measurement rather than a claim, and it is the standard the other lanes
should copy.

Attempt 13 runs **eight frames deeper than attempt 12** and no longer dies on a
missing name:

```
#9 RtEngine_Engine_Startup            +0x22C
#8 RtIo_FileSystem_Startup            +0x140
#7 RtIo_FileSystem_LoadPackages       +0x111   <- attempt 12 died HERE
#6 RtIo_FileSystem_FindFiles          +0x92
#5 FindFilesImpl                      +0x18F
#4 AppendFilename                     +0x2D
#3 RtColl_ArraySetSizeFill_180026880  +0x39
#2 rtcoll::SetSizeCore                +0x181
#1 rtcoll::MemAlloc                   +0x81    <- EXEC of 0
```

**A/B control, run and passed exactly:** `BANISHED_RT_FIND=0` on the same binary puts
the fault back on `?FindFiles@` at `RtIo_FileSystem_LoadPackages +0x111` — attempt
12's exact site. Second consecutive rung where the kill switch restores the previous
rung's fault precisely.

### ⛔ THE NEW WALL IS NOT A MISSING NAME -- it is the ALLOCATOR, reached BY ADDRESS

```
[AllocLock] could not resolve Allocate (0x1800c6e60)
[AllocLock] could not resolve Free (0x1800c6c00)
[REDZONE-DUMP] no allocator singleton
```

Every rung so far has been "symbol X has no port". This one is different in kind:
`?Allocate@MemoryAllocator@System@@` and `?Free@` are reached **by ADDRESS**
(`0x1800c6e60` / `0x1800c6c00`) by the Collection array code that lane PKG and lane
FIND both call, and with the DLL gone there is nothing at those addresses and nothing
registered for them. So `rtcoll::MemAlloc` calls a null pointer eight frames below
anything that names an allocator.

**This partly CORRECTS gate row E's recorded prediction.** Row E predicted the
allocator would fail silently at cutover because
`?_singleton@MemoryAllocator@System@@0PEAV12@EA` is "unminted exported data, nothing in
the tree mints it", faulting on `null+0x640`. **The singleton IS minted** —
`rt_data_mint_tables.inc:1572`, `0x1801fc718`, 8 bytes. The prediction that the
allocator breaks at cutover was right; the *mechanism* named was not. The live
mechanism is the unregistered by-address entry points, which no name-based instrument
can see because **the call site never mentions a name at all** — the deepest form of
the door problem this wave keeps rediscovering.

`[RtAllocSwap]` already states the intended design ("the composed stubs, which take
the allocator CS at self+0x640"), so the gap is registration, not design.

**NEXT ACTION for whoever picks this up: register the allocator's own entry points by
address** so by-address callers reach our port, then re-run. That is a smaller job than
any rung since attempt 08, and it is the last thing standing between the boot and
whatever comes after `FileSystem::Startup`.

### Attempt 14: the allocator rung CLOSES, and the boot clears the whole IO/startup phase

The fix is `RegisterAllocatorAddresses()` in `runtime/src/system/allocator.cpp`, called
from the **cutover branch only** of `RtSystemAllocatorRegister`. Four addresses:

| address | symbol | who reaches it by address |
|---|---|---|
| `0x1800c7a50` | `?MemAlloc@` (static, locking) | `runtime/src/collection/array.cpp`, `runtime/src/graphics/scene.cpp` |
| `0x1800c7ad0` | `?MemFree@` (static, locking) | `array.cpp`, `runtime/src/ui/widgets_input.cpp`, `standalone/filename_runtime_hook.cpp` |
| `0x1800c6e60` | `?Allocate@` (member) | `engine/funcs/engine_funcs.cpp:1631` |
| `0x1800c6c00` | `?Free@` (member) | no live reader; registered so the pair cannot drift |

**Cutover-only is load-bearing, not caution.** On the DLL-present path the swap is a
detour, so an unregistered `ResolveAddr` hands back retail's address and calling it
lands in our port anyway — and registering would hand
`standalone/allocator_lock.cpp` OUR function to patch, since `ResolveAddr` consults the
port registry FIRST. The run confirms allocator_lock stands down correctly at cutover
(`[AllocLock] could not resolve Allocate` — expected, and never the wall).

**A second defect the same crash was hiding.** All three by-address call sites cached
the resolved pointer in a plain function-local `static`, so any caller that ran before
arming would have frozen a null in for the life of the process. Changed to cache a hit
and retry a miss (the `RtResolveCached` contract). Without this the registration alone
would have been an ordering lottery.

Result — `[RtAlloc] cutover: 4 allocator ADDRESSES registered`, then:

```
#4 Game::RunGameMain
#3 RtEngine_Engine_Startup      +0x22C
#2 RtIo_FileSystem_Startup
#1 RtIo_FileSystem_LoadPackages +0x184   <- attempt 12 died at +0x140
   VCALL-of-0: this=0x250039802d0, vtable=0, and this[+0x00..+0x38] are ALL ZERO
```

Immediately above it in `standalone_error.log`:
`[RtResolve] MISS "??0PackageFile@IO@@QEAA@XZ" -> NULL`. `LoadPackages` allocates a
`PackageFile`, the constructor resolves to nothing, so the vtable is never installed
and the next virtual call on the zeroed object executes address 0. **The wall is the
`IO::PackageFile` class.**

> **CORRECTION (lane PKG2, `271ebb5`) — the two sentences above are wrong about the
> mechanism, and the error is kept visible because it is easy to repeat.** This is **not**
> a virtual call and no vtable is involved. `RAX=0`, `RCX=RDX=` the fresh 0xF0 block,
> `RIP=0`: it is
> `RT_EXPORT_ONCE(PackageFileCtorFn, "??0PackageFile@IO@@QEAA@XZ")(block)` called through
> a **null function pointer**. The object read as all-zero because nothing had run on it
> yet — not because a vtable was missing.
>
> A null call whose first argument is a freshly allocated block is INDISTINGUISHABLE from
> a VCALL-of-0 in the register file: `this` points at plausible zeroed memory and `RIP`
> is 0 either way. The two diagnoses send you to different places — "the vtable was never
> installed" sends you hunting a mint row, "the function pointer was null" sends you to
> the resolve. **The `[RtResolve] MISS` line above the dump is what actually names the
> fault**, and the crash handler's own label is a guess it is not entitled to make.
>
> The conclusion (the wall is `IO::PackageFile`) was right; the reasoning was not.

### The surface is now BOUNDED: 43 symbols

The boot no longer dies in the name-resolution phase — it clears `FileSystem::Startup`,
`Engine::Startup`, the video/UI registration pass, and reaches `[AutoStart] ENABLED`.
`BANISHED_RT_STRICT` records **43 distinct unresolved symbols** across the whole run
(`wave9_attempt14_error.txt`). That is the remaining cutover surface as measured, not
estimated — though it is what this run ASKED for, so each rung that falls can expose
more.

Two of the 43 are the handoff's lead #1: `?Startup@Device@Video@@` and `?Shutdown@` are
one-instruction JMP thunks onto `PlatformStartup`/`PlatformShutdown`, so one port closes
both rows plus the unsatisfied call in our own `Engine::Startup`.

### The ADDRESS-door census: 62 doors no name-based instrument can see

Attempt 13's wall was the first that was not a missing name, so it implied a census
nobody had run: every `ResolveAddr(Module::Runtime, 0x...)` in `runtime/`, `code/`,
`engine/`, `standalone/` and `dx11/`, minus every address anything registers.

| | |
|---|---|
| by-address call sites | 172 |
| literal `RegisterRuntimeAddr(0x...)` | 508 |
| mint-registered (table-driven) | 507 |
| **unregistered** | **62** |

**The first run of this scan said 94, and it was wrong in the same way as everything else
this wave.** It counted registrations by grepping a LITERAL `RegisterRuntimeAddr(0x...)`,
and the mint does not spell its addresses at the call site — it walks three generated
tables and registers each row through a variable (`rt_data_mint.cpp:578/583/588`). All
507 minted addresses were invisible, so 32 registered doors were reported unregistered.
Corrected in `4ef55b6`. That is the FOURTH time this project has paid for the same
defect (Wave 4 lane D's `kName*` constants, the dx11 resolver, the macro doors, now the
mint tables), which is enough to promote it to a rule:

> **An instrument that greps for a literal is blind to the same fact spelled through a
> variable, and the blindness never announces itself.**

**This is a WATCH LIST, not a defect list.** An unregistered door is only fatal if it is
REACHED with the DLL absent. `standalone/allocator_lock.cpp` prints "could not resolve"
and stands down, which is correct and was never the wall. Each row needs the call site
read — the same triage the unregistered dispatch slots got.

Concentrations, and how they were routed:

| n | file | routed to |
|---|---|---|
| 22 | `engine/funcs/engine_funcs.cpp` | split below |
| 7 | `runtime/src/graphics/scene.cpp` | lane GFX2 |
| 5 | `runtime/src/input/input.cpp` | lane SYS2 (owns `?Update@Device@Input@@`) |
| 5 | `runtime/src/picking/scene.cpp` | unrouted |
| 4 | `runtime/src/io/filesystem_packages.cpp` | **lane PKG2 — the wall's own file** |
| 4 | `standalone/filename_runtime_hook.cpp` | lane SYS2 (owns `?FindOrLoad@`) |

The four in `filesystem_packages.cpp` — `0x18007eefc`, `0x180008340` (a tree free),
`0x1800264a0` (a sort), `0x180025a80` (a BST insert) — sit inside `LoadPackages`, which
is where attempt 14 dies. They are the likeliest rungs after `??0PackageFile@IO@@`, and
they are Collection internals: an AVL rebalance written blind silently corrupts a tree.

The 22 in `engine_funcs.cpp` are not one thing. Reading each site split them cleanly:

- **Video Context lifecycle (2)** — `fn_VideoCtx_Ctor` `0x1800f1820`, `fn_VideoCtx_Delete`
  `0x180018500`. The neighbouring `fn_VideoCtx_Dtor` goes by NAME
  (`??1Context@Video@@`), so the lifecycle reaches the DLL through two different doors
  and **all three are null at cutover**. Independent corroboration that the
  `CreateVideoContext` cluster is the right scope. → lane VID.
- **Dialog lifecycle (4)** — `fn_DialogInstance_Ctor` `0x180114870`, `fn_TabElement_AddTab`,
  `fn_DialogInstance_AddControllerTab`, `fn_DialogInstance_AddController6`. The
  DialogInstance **constructor** goes by address, so it never appeared on any name-based
  list even though the dtor did. A dialog built through a null ctor fails exactly as
  attempt 14 does: an all-zero object whose first virtual call jumps to 0. → lane UIC.
- **The mods cluster (11)** — `ModEntry` copy-ctor/assign/dtor, `fn_Map_Insert`,
  `fn_ModArrayCopy`, and `Modification` GetCount/GetMod/Swap/Enable/RefreshList/SaveList.
  The mods menu is a live default-on feature, so this is running surface, not
  speculative. → **lane MOD**, dispatched off this finding alone.
- **`fn_ParseCommand` / `fn_ParseValue`** (`0x1800c4a00`, `0x1800c4930`) — consistent with
  the standing note that `?ParseCommand@` goes live the moment anything passes a
  non-empty command line. Unrouted, low priority while nothing does.
- **`g_pLoader` data (2)** (`0x1801fe7c0`, `0x1801fe7e0`) and `0x18006f330` — unrouted.
  The two `g_pLoader` slots are DATA the mint does not cover; check them before the next
  cutover attempt.

### Attempt 15: the eight-lane wave lands, and the rung moves within the same frame

Eight lanes merged (PKG2, VID2, UIC, UIW, GFX2, SYS2, MOD, PICK). Coverage 69.4% →
**72.6%**, unregistered address doors 62 → 44, `UNGUARDED-CALL` 3 → **0**, port bypasses
0 on both doors.

```
#3 RtEngine_Engine_Startup      +0x22C
#2 RtIo_FileSystem_Startup
#1 RtIo_FileSystem_LoadPackages +0x1BA     <- attempt 14 died at +0x184
   [RtResolve] MISS "?ReadPackfile@PackageFile@IO@@QEAA_NAEBVFilename@System@@@Z"
```

**Lane PKG2 predicted this exactly** — "expect the frame to stay in
`RtIo_FileSystem_LoadPackages` with the fault moving forward from the ctor call to the
`ReadPackfile` call". Symbol, frame and direction: 3 for 3. That is the fourth
consecutive rung where a lane's prediction held.

**The crash handler mislabelled it as `VCALL-of-0` again** — and this time the object
even *has* a vtable (a minted one, at `reimpl-x64.exe + 0x745E40`), which makes the wrong
label far more persuasive than it was at attempt 14. It is still a null CALL:
`RCX` = the `PackageFile`, `RDX` = the `Filename`, `RIP` = 0. The `[RtResolve] MISS` line
is again the thing that names the fault. The three null slots visible in the vtable dump
(`+0x48`, `+0x50`, `+0x58`) are a red herring — nothing called them.

That the object now carries a correctly minted vtable is itself the measurement that lane
PKG2's constructor port works at cutover.

### The OTHER half of the cutover surface: the 16 orphan ADDRESS doors, named

Attempt 19 enumerated the NAME-door residue. The address door has no such
luxury: **an unregistered address door prints no `[RtUnresolved]` line and no
`[RtResolve] MISS` — it is a null call with no diagnosis.** That is exactly why
these survived nineteen attempts while the name residue got listed for free.

They are not anonymous, though. `RVA = addr - 0x180000000` against the shipped
export directory names 9 of the 16 outright:

| address | name | call site |
|---|---|---|
| `0x1800af5f0` | **`?RefreshList@Modification@IO@@`** (ord 3528) | `engine_funcs.cpp:3141` |
| `0x1800dd3c0` | `?Run@MainLoop@Engine@@` (ord 3601) | `game_main.cpp:176` |
| `0x180114870` | `??0DialogInstance@UI@@` ctor (ord 147) | `engine_funcs.cpp` |
| `0x1801133b0` | `?AddController@DialogInstance@UI@@` (ord 1542) | `engine_funcs.cpp:2974` |
| `0x180113470` | `?AddController@DialogInstance@UI@@` (ord 1541) | `engine_funcs.cpp:2977` |
| `0x1800da130` | `?AddTab@TabElement@UI@@` (ord 1589) | `engine_funcs.cpp:2971` |
| `0x1801189f0` | `?CacheText@NumberElement@UI@@` (ord 1679, **private**) | `widgets_input.cpp:220` |
| `0x18003ffe0` | `??9String@System@@` (ord 1290) | `widgets_input.cpp:232` |
| `0x1800c4930` / `0x1800c4a00` | `?ParseValue@` / `?ParseCommand@Configuration@System@@` | `engine_funcs.cpp:1729/1727` |

**THE COORDINATION FINDING: `?RefreshList@Modification@IO@@` IS REACHED BY BOTH
DOORS.** It is the symbol that killed attempt 19 by name, and `engine_funcs.cpp`
also calls it by address. Closing only the name door would have left the address
caller pointing at nothing — and *silently*, since only the name door reports.
Lane MOD was told. Three more (`DialogInstance` ctor + both `AddController`) went
to lane UID and three widget doors to lane UIW2, so each lane's closure count
covers both doors rather than half of one.

Seven addresses have **no export name at all**, including `0x1801fe7c0` and
`0x1801fe7e0` — which are in **`.data`**, one and two 0x20-strides past the
MainLoop singleton at `0x1801fe750`, so they are almost certainly mint rows
rather than bodies. Those nine went to a new lane ADDR.

Two standing cautions apply to every row above. **Registering an address is not
automatically safe** — some slots are null ON PURPOSE so a dispatch-or-fallback
path takes the fallback, and a few are read under the WRONG name and work only
while null. And **porting `?Run@MainLoop@Engine@@` will GROW the surface**: lane
CTX measured seven Wave 6 `Context` setters that have zero live callers only
because they are reached by a direct near-JMP the registry cannot redirect from
inside `MainLoop::Draw`, and they all go live at once when that side is ported.

### Attempt 20: THE FAILURE BECOMES OURS — 27 unresolved names down to 17, and the fault moves INSIDE our own code

Tip `9f7d6d9` (all eight farmed lanes plus the PKG3 oracle fix), exe md5
`d3c02467b450d328d21038da71384009`, verified byte-identical to a build of that
commit made in a dedicated verification worktree — not inherited from whatever a
previous lane had left deployed, which is how a cutover result gets attributed to
the wrong binary.

| | attempt 19 | attempt 20 |
|---|---|---|
| dies at | `Engine::Startup +0x338` | `Engine::Startup +0x3a3` |
| failure | null `CALL RAX` on an unresolved symbol | **our own ported code faults** |
| `[RtUnresolved]` names | 27 | **17** |

Ten names closed by the eight lanes: `RefreshList@Modification`,
`??1DialogInstance`, `Clean@ExternalFactory`, `Update@Device@Input`, the three
widget setters, `AddLine@SpriteContext` and both `SetScissor` overloads.

**THE FAILURE CHANGED CLASS AGAIN, AND THIS TIME IN THE DIRECTION THAT MATTERS.**
Every previous wall was a symbol nobody had ported — the boot reached a hole and
fell through it. This one is a fault *inside a body we wrote*:

```
ACCESS_VIOLATION   Fault: READ of 0x0000000000000028
RAX=8  RBX=8  RDX=8
#0  FactoryStartupRun(FactorySet*) + 0x1b6
#1  RtObject_Factory_Startup + 0xb0
#2  RtEngine_Engine_Startup + 0x3a3
```

**And the port is almost certainly RIGHT.** `type_system.cpp`'s pass 2 carries a
comment that predicts this exact crash, written when the body was transcribed:

> `FAITHFUL: LEA RBP,[RCX+8] on a null RCX. … a parentName naming an
> unregistered type yields (TypeRecord*)8 and faults on the next line, exactly as
> retail does.`

`p` is null because the descend found no node for `parentId`; `parentRec` becomes
8; the next line reads `+0x20` off it — `0x28`. The registers agree exactly. So
**a type's `parentName` names a type that is not in the chain, and only at
cutover** — with the DLL present the same path works. Retail's unguarded deref is
load-bearing: it relies on every named parent being registered, so a guard would
skip the behaviour rather than restore it. The job is to find who failed to
supply the type, not to defend the deref.

Prime suspect, and it is a recorded failure of the same subsystem: the mint
generator **mints what our SOURCE names and is blind to what its own minted
vtables READ** — ten `BaseTypeInfo` once came out null, including the two roots
of the reflection chain. A minted type whose `parentName` names something nothing
registers produces precisely this. Farmed to lane TYPE.

**Do not chase `chain=416 nodes=415` while looking at this.** Lane COLL settled
it: `AutoSaveDialog` is registered TWICE and the duplicate arm correctly returns
the existing node without incrementing. `ReportKeyCollisions()` names it.

Expected-at-cutover and now correctly worded, after lane ENGIO found the old
message was wrong: `[VideoInterface]` no longer claims a DX9 fallback that cannot
happen with no Runtime to load VideoDX11 — it says instead that **something must
call `VideoInterfaceFillDx11Slot()` from the ported `InitializeInterfaces`,
because nothing else fills the slot.** That is a real cutover gap the old wording
had hidden, and I had read the old line and moved on.

### Attempt 19: THE BOOT NAMES ITS OWN REMAINING SURFACE — 27 symbols, and only ONE is reached

Tip `fd30180` (lanes GFX3 + FNHOOK + COLL merged). Exe md5 `1cef3ae7…`, DLL renamed
to `.absent`, restored immediately after.

**The packfile-sort wall is GONE.** Lane COLL's `0x1800264a0` closed it, and the
boot walked all the way from `SetupDirectories` through `LoadPackages` and the
type-system build (`358` factory registrations, both `Loading.rsc` target descs
found) into `Engine::Startup`. The fault moved from `LoadPackages +0x274` to
`Engine::Startup +0x338` — six rungs in one attempt.

**The failure changes CLASS again, and this is the important part.** Every prior
attempt died somewhere the log did not name, and the diagnosis was archaeology:
symbolize a frame, guess the callee, disassemble. This one died on a line the run
had already printed:

```
[RtResolve] MISS "?RefreshList@Modification@IO@@QEAAXXZ" -> NULL. Nothing has
registered it and the shipped DLL does not export it (or is absent). ... If this
is cutover, this symbol still needs minting.
```

then a null call through RAX with `RCX=RDX=` the `Modification` object. **The
boot is now self-diagnosing.** The crash handler still labels this `VCALL-of-0`
and prints `vtable=0x101`, which is not a vtable — `this[+0x00]` is data. That
label has now misled twice (I misread attempt 14 the same way and lane PKG2
corrected me); it is a `CALL RAX` on a null, not a virtual dispatch.

**What that unlocks: the residue is no longer estimated, it is ENUMERATED.** The
run's own `[RtUnresolved]` lines list **27 symbols** — the complete live cutover
surface on the boot path — each with the address that asked for it. Exactly ONE
(`RefreshList`) was actually CALLED; the other 26 were resolved-and-stored during
lazy init and are null calls waiting downstream. That distinction matters: the
wall count is not 27 rungs, it is 27 symbols that will surface over some smaller
number of attempts.

This is the first attempt whose output could be turned directly into a work
breakdown with no Ghidra archaeology at all, and it was farmed out the same hour:

| lane | symbols |
|---|---|
| MOD | `RefreshList@Modification` (**the live wall**), `SaveText@External` |
| RES | `FindOrLoad@ExternalFactory`, `Clean@ExternalFactory`, `LoadResources@UpdateLoop` |
| UID | `ShowDialog`, `CloseDialog`, `??1DialogInstance`, `OnClosed@DialogInstance` |
| UIW2 | `SetCurrentItem@ComboElement`, `SetParameter@SliderElement`, `SetProgressTextParameter@ProgressElement`, `CreateInstance@ElementDescription`, `Serialize@ElementDescription` |
| UIF | `Update@UserInterface`, `Draw@UserInterface`, `AddLine@SpriteContext`, `SetScissor@SpriteContext` x2 |
| GFXR | `Inverse@Matrix` (**prerequisite, ordered first**), `Draw@Renderer`, `BuildParameters@MaterialInstance`, `Serialize@MeshInstance`, `Update@AnimationManager` |
| ENGIO | `Update@Device@Input`, `Update@Scene@Audio`, `OnVideoError`, `OnVideoRestart` |

Three of those had a decomposition already done by a lane that DECLINED them —
INPUT wrote the `GetDeviceState` recorder recipe for `Update@Device@Input`, FNHOOK
wrote §4.3 for `FindOrLoad`, CTX wrote the ordering warning that makes `Inverse`
lane GFXR's first commit. **A decline that leaves a recipe is worth more than a
port that leaves none**, and this attempt is where that paid.

**Expected-at-cutover, confirmed not a defect:** `[FNHook] could not resolve
ExternalFactory::FindOrLoad` / `External::Load` / `Video::Device::InitializeInterfaces`
are lane FNHOOK's installers now resolving RAW against an absent DLL, which is
the correct behaviour for an unported target. Before FNHOOK's fix they resolved
through the port registry and would have silently aimed at our own prologue.

Address doors at this attempt: **UNREGISTERED 16**, 198 distinct. Census 1003
called / 753 ported / 75.1%.

### Attempt 18: `ReadPackfile` RETURNS — the wall is the packfile sort

Lane DESTROY ported `0x180026b50` (attempt 17's fault address). The boot now runs
`ReadPackfile` **to completion** and comes back up into `LoadPackages`:

```
attempt 15/16   LoadPackages +0x1BA    ?ReadPackfile@ missing
attempt 17      inside ReadPackfile    DestroyPkgEntryRange -> EXEC of 0
attempt 18      LoadPackages +0x274    RCX = packfiles, RDX = 4  -> EXEC of 0
```

`RDX = 4` is `packfileCount` — the four packages lane PKG3 measured. The fault is the
sort at **`0x1800264a0`** (`filesystem_packages.cpp:767`), an unregistered by-address
door. **Fifth consecutive wall that is a door with no name at the call site**, and
`[RtUnresolved]` gained **nothing** between attempts 17 and 18 — a name-based instrument
would have reported no change at all across a rung that moved the boot forward twice.

That file's own comment already anticipated this lane: *"A clean `std::sort` here would
compile, run, and silently pick different winners… transcribing it would be an unverified
guess at a precedence rule."* Owned by lane COLL.

### Attempt 17: THE LADDER MOVES — the boot is now INSIDE `ReadPackfile`

`?ReadPackfile@PackageFile@IO@@` landed (lane PKG3), and with it the whole
`LoadPackages` chain that attempts 12–16 died on. Coverage **74.1%**.

```
#6 RtIo_FileSystem_LoadPackages
#5 RtIo_PackageFile_ReadPackfile              <- attempts 15/16 died BEFORE this
#4 RtIO::ArraySetSizeFill
#3 rtcoll::RtColl_ArraySetSizeFill_1800298f0
#2 rtcoll::SetSizeCore
#1 rtcoll::DestroyPkgEntryRange +0x??          <- EXEC of 0
```

`?ReadPackfile@` is gone from the residue list entirely. The last line before the
crash is `[AutoStart] ENABLED`, and the boot is executing **our** transcription of a
1,864-instruction function.

**The new wall is an unregistered BY-ADDRESS door — the same class as attempt 13.**
`rtcoll::DestroyPkgEntryRange` (`array.cpp:757`) forwards to `0x180026b50`, the
packfile-entry destroy helper, which `tools/addr_door_audit.py` lists as
unregistered. With no DLL it resolves to nothing and the call executes address 0.

That is a one-address rung, and it is the *fourth* time this wave that the wall has
turned out to be a door with no name at the call site — the failure class no
name-based instrument can see.

### Attempt 16: NO MOVEMENT, and that is the correct result

Between attempts 15 and 16 the tree gained lane PKG2's rungs 2 and 3 (the
`CompressedStream` read path; `OpenFile`/`ReadFile`), lane VID2's `PlatformStartup`
cluster, and the shadowed-registration fix. Coverage 72.6% → **73.4%**.

The boot dies in **exactly the same place**:

```
attempt 15   RtIo_FileSystem_LoadPackages +0x1BA   ?ReadPackfile@ -> NULL
attempt 16   RtIo_FileSystem_LoadPackages +0x1BA   ?ReadPackfile@ -> NULL
```

Byte-identical offset, same symbol. **This is what a correct decomposition looks like
from the outside, and it must not be read as a stall.** Rungs 2 and 3 deliberately built
the layers *beneath* `ReadPackfile` — the codec, the windowed stream, the file open and
read. `ReadPackfile` itself is rung 4 and is not ported yet, so the ladder cannot advance
until it is.

What the unchanged fault site DOES measure is that none of that work broke anything on
the boot path, which is not free information: three lanes' worth of ports landed between
the two runs and the process reached the identical instruction.

**Record a no-movement attempt as a result.** The temptation is to skip publishing it, or
to go looking for a different rung to make the ladder move — both would be worse than the
honest reading, and the second would abandon a decomposition that is working.

### THE EFFECTIVENESS CONTROL WAS WRONG TWICE, AND BOTH TIMES IT SAID PASS

Lane PKG2's rung 2 (`CompressedStream` read path). The project already knows a probe
needs an effectiveness control. This is the next lesson down: **the control itself is
code, it can be wrong, and when it is wrong it fails in the direction that says PASS.**

| version | what it counted | why it was worthless |
|---|---|---|
| v1 | changes to `activeWindow` | `Open@CompressedMemoryStream` hardcodes **one** window, so the value is 0 forever. It reported "LRU unverified" on a run that evicted on **every block** |
| v2 | reads that did not refill | dominated by the **fast path**. Said "LRU EXERCISED". **A false pass** |
| v3 | branches *inside* `SetWindow` | correct — and it immediately exposed that the CORPUS was inadequate too: sequential reading never returns to a resident block, so `scanHit` was 0 at **any** window count. A third configuration (4 windows + a rewinding schedule) took it to 3 |

Without v3, **three of the four bodies would have been "verified" by a green run that
never entered their interesting branch.**

The rule that generalises: **count branches taken INSIDE the function under test, not
proxies observed from outside it.** Every outside proxy here was either structurally
pinned (v1) or swamped by the common path (v2). And when the control finally works, it
tells you about your CORPUS as well as your code — a control that cannot go to zero is
not measuring coverage.

Corollary, for anyone reading a dashboard: `Read` reports `calls=0 worked=225`.
**`calls=0` means "no self-comparison", not "never ran"** — a lockstep probe is the
oracle there, and the column that would normally be the alarm is structurally zero.

### SEVEN blind instruments in one session — and the rule they all share

The eight-lane session found blind spots in seven separate measuring tools. They are not
seven unrelated bugs. Five are the SAME defect:

| # | instrument | blind to | cost |
|---|---|---|---|
| 1 | `runtime_census.py` `called` | the `RT_EXPORT_ONCE` / `RT_DATA_SLOT` macro doors | listed `?SetupDirectories@` and `?LoadPackages@` as **ports with no caller** while the boot was dying on them |
| 2 | the address-door census (mine) | the mint's table-driven `RegisterRuntimeAddr((uintptr_t)v.dllVa, …)` | reported **94 unregistered doors when the true figure was 62** — committed while measuring the same class of defect |
| 3 | `unsatisfied_export_scan.py` | `RT_DATA_SLOT` | true denominator is **812, not 789** — 23 names invisible |
| 4 | `port_bypass_audit.py` `ADDR_SITE` | a named constant operand (`MainLoopAddr::CreateVideoContext`) | **20 sites tree-wide**, and it hid a real defect: an exported function reached through the address door |
| 5 | `rawdoor_audit.py` `diff_only_fn_depth` | a one-line aggregate initialiser's closing brace | reported guarded `RT_ORIGINAL_ONCE` calls as UNGUARDED-CALL |

> **A GREP FOR A LITERAL IS A LOWER BOUND, AND IT NEVER SAYS SO.** Five separate tools
> here have reported a confident number that was simply smaller than reality, because the
> same fact can be spelled through a variable, a named constant, a macro or a generated
> table. Before quoting any grep-based census, ask whether its operand can be spelled
> indirectly — and check the count against an independent source.

The other two are a different and nastier family — **instruments whose output cannot
falsify them:**

| # | instrument | defect |
|---|---|---|
| 6 | the `Factory::Startup` self-check | its `theirs=` side was `snprintf(b, "chain=%d nodes=%d", chain, chain)` — **a constructed expectation formatted to look like a measurement of retail.** It runs no DLL body at all |
| 7 | `RT_DIFF_SHADOW` | emits its strings **only on a mismatch**, so a population description carried inside one is invisible on exactly the runs where you would want it |

Number 6 cost real work: it reported `ours=nodes=415 theirs=nodes=416`, lane UIC read it
the only way it can be read — retail gets 416, we lose an insert — and passed a false
divergence to another lane. The `416` on the "theirs" side was `chain` printed twice.
Both are now fixed; the self-check prints `EXPECTED (no DLL run): nodes==chain==N`.

> **If a control is only visible when the test fails, it is not a control** (lane UIC).
> And a check whose expected side is true by construction is not a check.

The open question underneath #6 is now answered EMPIRICALLY rather than asserted:
`ReportKeyCollisions()` walks the chain and NAMES any two `BaseTypeInfo` sharing a
`TypeNameCRC`, distinguishing a hash collision (one type unreachable by id) from the same
name registered twice — and says explicitly that if no duplicate exists, the shortfall IS
a lost insert and must not be dismissed.

### Two rules about ORACLES, from lane PICK — both found AFTER a PASS

Lane PICK's grid probe returned `comparisons=45 mismatches=0`, every effectiveness
counter non-zero, no GAP line, and both negative controls firing. It then interrogated
its own pass and found two holes. Both generalise.

**1. An oracle observing a result only through a LOSSY TRANSFORM is blind to everything
the transform discards.** The grid comparison could not verify the float bodies at all:
`BoxTransform`'s output reaches it only through `(int)(f * invCellSize)`, and that
truncation swallows any error smaller than one cell. **A 1-ULP AABB error would have
sailed through green.** The fix was a direct bitwise comparison on the float bodies' own
inputs — 57 cases, tolerance zero, off-axis and non-normalised rotations.

The general form, which applies well beyond Picking: a float checked through a quantiser
or a comparison-to-threshold, a decompressor checked on its output bytes but not its
stream cursor or checksum state, a serialiser checked on the file but not the object.
Ask what your comparison point *cannot see*.

**2. Compare in LOCKSTEP, not at the end.** The script now applies each of 17 indexed
steps to both subjects and compares immediately, instead of running each subject to
completion and comparing once. *"A defect that cancels out — push a link on the free list
in the wrong order, pop it back in the wrong order — compares equal at the end while
every query in between saw a different grid."* An oracle whose only observation point is
the end state cannot distinguish "never wrong" from "wrong, then wrong again".

### The advisory lock nobody was told about

`tools/game_dir_lock.sh` was written mid-session, after contention had already cost three
lanes their runs. It was then broadcast only to the lanes the integrator happened to be
replying to — so lane PICK was **clobbered twice inside a window it held**, by lanes that
had never heard of the lock. Its first run produced a plausible, complete log from the
pre-lane build.

**An advisory lock introduced mid-flight protects nobody until every participant is told,
and the failure is silent for anyone whose acceptance test is the on-disk md5.** Three
lanes independently converged on the same fix: **gate on an identity line that only your
build emits, read out of the run's own log** (`[RtPort] Picking: 9 exports + 19 grid
addresses registered`, `[RtPort] IO: N exports registered`, a lane-specific registration
count). The md5 tells you which exe is on disk at the moment you look; the log line tells
you which exe *ran*. Derive the expected string from your own build each time — lane
PICK's changed the moment its registrations grew.

### A probe that ALLOCATES cannot run from a `*PortInit` (lane PKG2)

Lane PKG2's `Decompress` probe crashed writing through `null+0x640`. Nothing in the
crash named the probe. The cause: it was called from `RtSystemPortInit`, and **that init
runs before the allocator publishes its singleton** — so the very first allocation
dereferences a null `_singleton` and lands on the critical section at `+0x640`.

`+0x640` in a fault address is the allocator's lock offset and is therefore a signature:
it means "something took the allocator's CS before the allocator existed". Schedule a
probe that allocates on its first *real* call instead, the way lane PICK's grid probe
self-schedules.

### Two more rules the lanes paid for

**A family found by a caller-driven survey is bounded BELOW, not above** (lane UIC). Lane
D found eleven `Array<T>::~Array` addresses; lane UIF found a twelfth and wrote "there
are twelve"; lane UIC found the thirteenth and fourteenth inside `?AddController@`. A J1
address is invisible to a caller-driven survey until something in our tree calls it, so
"there are N" was never a count — it was a lower bound wearing a count's clothes.

**A row can be a dead pointer AND a live dependency at once** (lane MOD). Five of the
eleven `fn_*` pointers in the Modification cluster are resolved and never called — but
two of those ADDRESSES are live through a different door (`0x180010110` from
`loops.cpp:681` on every world load, `0x18002a090` from `persistence_registry_win.cpp:266`
on every boot). The census keys on the address, so the address grep alone over-reports
urgent ports and the identifier grep alone misses the deep copy that runs on every load.
Neither view is sufficient on its own.

### The `dirAlias` finding — a local that is written, passed, released and NEVER READ

Lane FIND found that `FindFiles`' `dirAlias` at `[RSP+0x58]` is built, handed to
`AdjustDelimiters`, released, and never read again. **It is not dead code.**
`AdjustDelimiters`' first phase rewrites `/` to `\` **in place, through the
refcount-shared buffer**, so the local exists solely for its side effect on a string
somebody else is holding.

That is precisely the shape a careful reimplementation deletes as obviously dead. Worth
carrying as a rule rather than a note: **a local that is written, passed to a helper,
released, and never read is a SIDE-EFFECT CARRIER until proven otherwise** — check
whether the helper mutates through a shared buffer before dropping it.

## Lane A's work list: the 32 wave-3-to-8 holdbacks, triaged

Produced by the fix wave (see [WAVE8_5.md](WAVE8_5.md)), which also closed four of them and
dissolved five stated blockers. What follows is what Wave 9 lane A inherits.

### The 32 wave-3-to-8 holdbacks: triaged, with a cutover disposition each

These are the largest inherited item and they are **not paperwork** — most are real porting work
that needs a dependency ported first. What was missing was not effort, it was a *decision per
function*, so that lane A is a work list rather than a re-reading of five lane docs.

**The reframe that makes the list tractable:** at cutover a held-back function cannot "stay on
the DLL", because there is no DLL — but it does not automatically need a faithful port either.
It needs to **not be a missing symbol**. So every row below resolves to one of *port*, *probe*,
or *stub with a written reason* — and the three dead `IO::Serializer` overloads above are the
precedent for the third being a legitimate answer.

| # | Holdback | Disposition |
|---|---|---|
| **P — dissolvable by probe: DONE 2026-09-07** | | |
| 1 | `UpdateLoop::IsRunning` (lane A) | **CLOSED — 5 cases, 0 mismatches, true branch REACHED.** Retail is two instructions: `MOV RAX,[0x1801fe750]; CMP RCX,[RAX+0x20]` — `this` is COMPARED, never dereferenced, which is what makes the false branch safe to probe with pointers that are not `UpdateLoop`s. Cases: the running loop (true), **`_nextLoop` (+0x28) — the near miss, because a port comparing the pending loop instead of the current one would pass a one-sided test**, null, an unrelated stack address, and the `MainLoop` itself. The probe prints whether the true branch was reached, so five agreeing falses cannot read as a pass. |
| **B — blocked on a named dependency; ordinary port work once it lands** (**1 of the 10 PORTED AND VERIFIED 2026-09-07**) | | |
| 2-4 | `Audio::Scene::Update`, `Audio::Device::Update`, `FreeChannelPlatform` | **PORT after a DEVICE-LEVEL FAKE, not after `SoundBuffer`/`SoundEffect`** — see the shared-root-cause section below. A faithful port may CALL those classes by address, as `ClearAll`/`RegisterInput`/`Scene::Serialize` do with `Collection` internals, so what is needed from them is LAYOUT, not ports. The irreducible blocker is the stored completion callback. `Scene::Update` carries a second one: it consumes `System::Random`, so even a perfect device fake needs the RNG snapshotted too. |
| 5-7 | `Audio::Scene::StopAll` `0x1800df150`, `Pause` `0x1800df070`, `StopEffect3D` | **PORT with 2-4.** Read directly: both `StopAll` and `Pause`→`PauseEffect`→`0x1800de200` end in `CALL RAX` on the completion callback stored in the channel record, plus `FreeChannelPlatform`'s COM `Stop` under a lock. The state is bounded (256 x 0x70 channels + `paused[5]`) so snapshot/restore WOULD work — **but a callback that has already run into game state cannot be un-run**. `Pause` also early-returns when the flag already matches, so a naive ours-then-theirs would compare nothing and report green. |
| 8 | `Audio::Scene::Serialize` `0x1800e0050` | **PORTED AND VERIFIED 2026-09-07 — 32,772 emissions identical, 0 diverged**, by lane S. The stated blocker was answered by a fact the tree had since Wave 4 (retail's Encoding offsets, bool `+0x78` / uint `+0x98` / int `+0xa0`, in `rt_io_port.cpp`'s warning block). 40 instructions touching one Encoding slot (`kScalarInt +0xa0`) and one ReaderWriter slot (`kArrayBool +0x20`, n=5 for `Scene::paused`). Its two "unexported callees" are ordinary `Collection` internals called by address as `ClearAll` and `RegisterInput` already do. |
| 9 | `Engine::MainLoop::Run` (lane B) | **PORT after the allocator (row 12-15).** Lane B records the allocator embedding as the reason it is a holdback. |
| 10 | `Input::KeyMap::RegisterInput` (lane D) | **PORTED AND VERIFIED 2026-09-07 — 50 calls, 0 divergences.** Lane D held it back because it could not pin `FUN_180023060`'s first argument. Reading the callee answers it: **RCX is never read** — `0x180023097 MOV RCX,RBX` destroys it before any use. The call sites agree in the stronger form: RCX is `this` at one site and a zero-extended id byte at the other two, and **a value that differs like that across call sites cannot be a parameter the callee reads.** Oracle: **our writer, their readers** — there is no replay (a second insert of the same id returns early, making replay vacuous), so the insert is verified by asking the DLL's own `GetDeviceForId`/`GetInputForId`/`GetIdForDeviceInput` to read it back, the last of which walks the WHOLE tree and so exercises our linkage and the rebalance through code we did not write. |
| 11 | `Input::KeyMap::Load` (lane D) | **STILL BLOCKED — and this row's own first draft was wrong to bundle it with 10.** `Load` does not touch `FUN_180023060` at all, so the resolved ABI does nothing for it. It is blocked on three separate things: an unexported registry-string parse helper (`FUN_18007f034`), a `System::String::Format` allocation pair per binding, and a lookup into a **second, differently-typed** `Collection::Set<Pair<String,String>>` inside `IO::Persistence`. Lane D called it the densest of its 21 in every dimension, and it still is. |
| **N — NO-GO until the allocator is ours; then mandatory** | | |
| 12-15 | `Engine::Engine` ctor, `~Engine`, `Engine::Startup`, `Engine::Shutdown` (lane B) | **PORT AS PART OF LANE E.** `Startup` writes three allocator fields directly and `Shutdown` calls the allocator's shutdown, which is why lane B found the NO-GO is four functions wide rather than the ctor/dtor pair PLAN predicted. These cannot be done before the heap and must not be done after it. |
| 16 | `Input::Device::Update` (lane D) | **NO ORACLE BY CONSTRUCTION, and it is not optional.** Four unexported, unnamed internal helpers plus DirectInput8 COM pointers past `Device+0x11D0` (unmapped). Replaying it drains the input queue twice, so "call ours, call theirs, compare" corrupts the state being compared. A wrong port is a game that silently stops responding to keyboard and mouse. **This is the hardest single item in the wave** and deserves its own lane with a bespoke oracle (record-and-replay of the queue, not call-twice). |
| 17 | `Input::Device::EnableMouseClipping` (lane D) | **PORT with 16** — same object, same unmapped region. |
| 18 | `Engine::UpdateLoop::LoadResources` (lane A) | **PORT after the IO holdbacks.** |
| **D — dead by construction; stub with a reason, do not port** | | |
| 19-20 | `MainLoop::OnVideoError`, `OnVideoRestart` (lane B) | **STUB.** Wave 8 measured exactly ONE internal caller each — the static thunks the `MainLoop` ctor installs into `Video::Device::_singleton` +0x50/+0x58, reached only on device lost/restore. This is the detour case Wave 8 found and declined. At cutover they need to exist, not to be faithful; a port would be dead code reporting `calls=0`. **If a lane does port them, the device-lost path has to be forced, or the port is unverifiable.** |
| 21-24 | `Save::~SaveLoad`, `Save::Draw`, and lane E's shadow markers (`Application::PlatformUpdate`, `SleepThread`, `YieldToThreads`) | **STUB or PORT-WITHOUT-ORACLE, decided per function.** Lane E states these have no oracle by construction. |
| **X — already resolved by earlier waves, listed for completeness** | | |
| 25 | `ExternalFactory::SerializeReferenceTable` `0x1800bcb60` (Wave 3) | **UNBLOCKED, but NOT cheap — this row's own "cheapest row in the table, do it first" was wrong and is withdrawn.** Reading it: ~200 instructions that tear down and rebuild a `Collection::Set`, do reference-counted `System::String` traffic through the allocator (the `INC`/`DEC [RAX-4]` header pattern, `SUB RCX,8` then free), run an in-order tree walk with an inlined string compare, **and dispatch through the `IO::Encoding` vtable at +0x18/+0x20/+0x98/+0xa0 on the SAVE PATH** — the same hazard filed under row 8, which the first draft of this table put in a different category. A silent save desync is this project's most-cited failure mode; this wants a lane, not a slot. |
| 26 | `External::SaveText` `0x1800bb9d0` (Wave 3) | **STILL BLOCKED, on a class no wave has touched.** It drives the `+0x28 Serialize(IO::Stream&)` slot with an **`IO::TextFileStream`**, and WAVE4.md is explicit that `TextFileStream` (with `FileStream`, `BinaryFileStream`, `VisitorStream` and the codec) was NOT in Wave 4. Needs `TextFileStream` first. |
| 27-28 | `ExternalFactory::FindOrLoad` `0x1800bf320`, `Clean` `0x1800be310` (Wave 3) | **NO ORACLE, and it was never a Wave 4 dependency.** Both rewrite the shared resource AVL tree, so running ours and theirs mutates the one tree twice, and a private replica is exactly what a stateful port must not keep. Same class of problem as `Input::Device::Update` (row 16): they need a bespoke oracle, not a later wave. |
| 29-32 | 5 handle-table mutators + `CheckPtr` (Wave 3, opt-in NO-GO) | **ARM AND MEASURE.** These are the `if (table)` / `if (shadow)` / `if (checkPtr)` blocks the census was wrongly counting as ported (see above). `CheckPtr` is a bare `RET` in every shipped build, so registering a real one silently re-enables ~60k indirect calls/frame — that is a perf decision, not a correctness one. |

**What this triage changes.** Two of the four groups are not porting work at all: **P** is one
probe on machinery that now exists, and **D** is six functions that need a stub and a sentence
rather than a faithful body. That leaves **B**, **N** and **X**, which are genuine
porting work with named prerequisites, and inside them **`Input::Device::Update` needs a bespoke
oracle rather than more time.**

**A correction to this table's own first draft, which is the reason to write it out per
function.** It began by asserting that all four `ExternalFactory`/`External` mutators were
"held back on a Wave 4 dependency that has since landed", by analogy with
`project_jack_of_all_trades_closed.md` — where both listed blockers turned out to be already
fixed and nobody had re-run the census. Checking each one instead of the group shows a **1/1/2
split**: only `SerializeReferenceTable` is actually unblocked; `SaveText` needs
`IO::TextFileStream`, which WAVE4.md says in as many words it did NOT port; and `FindOrLoad` and
`Clean` were never blocked on Wave 4 at all — they have **no possible oracle**, because both
rewrite the one shared AVL tree. A group summary ("held back for no oracle / Wave 4") that was
accurate about the group was wrong about three of its four members, and only a per-function pass
catches that. Which is the argument for this table.

None of these can be closed by writing a body in this session without an oracle, and doing so
would be the exact failure `reference_uncalled_port_is_not_verified.md` warns about — a green
`calls=0` line over unverified code. They are lane work with a decided disposition, which is the
honest end state for them before the wave opens.
## Still open going into the wave

Everything inherited now has either a verification or a written disposition. What remains is
work, not undecided work:

* **The 31 remaining holdbacks** — triaged above. **P is DONE** (`UpdateLoop::IsRunning`,
  closed by probe). What is left is B (10, blocked on a named dependency), N (7,
  allocator-bound or oracle-hard) and D/X (14, stubs or, for one of them,
  `SerializeReferenceTable`, genuinely unblocked and lane A's cheapest row). Lane A executes
  that table.
* **`Input::Device::Update` needs a bespoke oracle**, not more time — call-twice corrupts the
  DirectInput queue it is reading. It is the hardest single item in the wave.
* **The perf gate**, deferred by the user to lane H.
* **The 1-ULP libm answer is a bound, not a bit-exact result.** If lane B wants the stronger
  answer it needs a deterministic world-gen fingerprint, which nothing in the tree emits today.

## Sizing

Not S. Waves 6 (113 fns) and 7 (101 fns) were sized L and XL. Wave 9 is **319 items across
17 namespaces, including the four things every previous wave was allowed to defer** — the
allocator, the `FUN_` pool, the data exports and the holdbacks — **with the diff oracle
switched off partway through.** Call it XL with a hard ordering constraint, and expect lane D
to be the one that decides the schedule.

---

## THE CUTOVER WALLS ARE PREDICTABLE IN ADVANCE — 172 minted slots that can only trap

Every attempt so far has discovered its wall by *hitting* it: boot, crash, read
`standalone_crash.log`, name the symbol, port it, boot again. That is one wall per
run, and each run costs a game-dir lock window on a contended lock.

**The wall set is computable from the tree without booting at all.** A minted
vtable slot is

```
{ "?Serialize@BinaryWriter@IO@@UEAAXAEAHPEBD@Z", 0x1800a8fd0ull, RT_MINT_NAMED },
```

carrying **both** a mangled name and a Runtime address. With the DLL present the
name resolves out of the export table. **With the DLL absent there is no export
table**, so the slot survives only if that address or name has a ported body
registered in our tree; otherwise it holds its per-slot trap
(`RT_MINT_TRAP(v,s)`) and a call through it names itself.

`tools/mint_wall_predict.py` joins the 311 `RT_MINT_NAMED` rows against every
literal `RegisterRuntimeExport` / `RegisterRuntimeAddr` in `runtime/`,
`standalone/`, `engine/` and `code/`. At `071864f`:

| | count |
|---|---:|
| `RT_MINT_NAMED` rows | 311 |
| registered by name or address — **survive cutover** | 80 |
| address mentioned in the tree but not literally registered — **needs a human** | 59 |
| **no registration and no mention — TRAP IF CALLED** | **172** |

**THE 172 IS AN UPPER BOUND, NEVER A LOWER ONE, AND THE TOOL SAYS SO IN ITS OWN
DOCSTRING.** It matches literal registrations only, so a registration spelled
through a variable or a named constant is invisible to it — the same blind spot
`runtime_census.py` has had five times. **A row it clears is genuinely clear; a
row it flags may still be registered.** Re-check a row before porting it: this
project has three times had a lane port something the tree already had, and a
duplicate registration is last-binding-wins, which silently makes one body dead
code.

**Validated against a known positive rather than trusted for reporting a
plausible number.** Lane ENC-2 ported `?Serialize@BinaryWriter@IO@@` at
`0x1800a8fb0` and `0x1800a8fd0`; both are present in the mint table and both come
back **CLEARED**. An instrument that flagged everything would have produced a
similar-looking headline.

### The set is not 172 problems — it is a handful of families

```
25  Visitor@IO             8  SpriteSheet@UI          7  ImageBuffer@Video
23  BinaryReader@IO        8  EdgeMesh@Graphics       7  DialogController@UI
22  BinaryWriter@IO        7  ParticleInstance@Gfx    6  MeshInstance@Graphics
                           7  MaterialInstance@Gfx    6  LoaderLoop@Engine
```

**The top three are 70 of the 172 and are ONE family**: the IO scalar-
serialization dispatch, one tiny body per scalar type in reader/writer/visitor
variants. Lane ENC-2 has already ported two of them, so the shape, the
registration mechanism and the framing conventions are established in-tree; and
lane GFXD independently traced `0x1800a88d0` / `0x1800a8fd0` as the `rw_vtbl`
slot at `+0xa0` (`kScalarInt`) reached from `Serialize@MeshInstance`, so the
family is load-bearing rather than busywork. **Dispatched as lane SERIAL.**

The write path also has a real oracle: **a write-path `Serialize` is replayable**
— drive our body and the shipped body against the same input and byte-compare the
stream. This project recorded "no oracle possible" for four `Serialize` ports
across three waves before noticing that.

### Why this changes the shape of the remaining work

The static export scan has been treated informally as the cutover gate. **It is
not one, and this wave measured that.** Merging lanes GFXD and RRES together — 8
exported bodies, one verified on 22,595 live differential calls with 0
divergences — moved `addr_door` UNREGISTERED 14 → 14, unsatisfied names 118 →
118, outside-a-diff-guard 115 → 115, names resolved 861 → 862. **Two headline
instruments flat for a full round of real work**, because they list names *our
source* resolves through a door and nothing of ours called those bodies before.
A port cannot remove a name from a set it was never in.

So the honest gate is the pair: the **runtime `[RtUnresolved]` residue from an
actual DLL-less boot** (what the boot path really touches) and this **mint-wall
prediction** (what it would touch if it got further). The first is exact and
narrow; the second is broad and an upper bound. Neither alone is the gate, and
the static export scan is neither.

---

## Attempt 22: PLAT'S PREDICTION HOLDS — the boot crosses `Engine::Startup` to `+0x62B`, and the wall is a plain unported export

Tip `b381bb8` (lanes PLAT, GFXD, RRES, UIW2 merged, plus the mint-wall tooling
and the RETSLOT ABI fix). Exe md5 `169d380785900fceac2714aa40840d0b`, verified
equal built-vs-deployed. DLL renamed to `.absent`, **restored and verified
immediately after** (`no .absent left behind`). Logs copied out before the lock
was released.

### The ladder moved

| attempt | fault site | wall |
|---|---|---|
| 19 | `Engine::Startup +0x338` | null call, 27 unresolved names |
| 20 | `Engine::Startup +0x3a3` | our own code faulted, 17 unresolved |
| 21 | `engine_lifecycle.cpp:638` | `?PlatformStartup@Application@1@AEAA_NXZ`, `EXEC of 0` |
| **22** | **`Engine::Startup +0x62B`** | **`?EnumerateVideoModes@Device@Video@@QEAA_NXZ`** |

**Lane PLAT wrote its §8 prediction before the run: `Engine::Startup` would pass
fail-points 6/11/12/13 and stop at whatever the next unported export is, rather
than at `EXEC of 0` in `engine_lifecycle.cpp:638`. IT HELD.** None of PLAT's four
`PlatformStartup` members appears anywhere in the residue. PLAT also named the
two places to look first if the prediction failed — its `WindowProc`, and
`DirectInput8Create` receiving the exe's module handle — and **neither is
implicated**: the stack is `Engine::Startup +0x62B` ← `Game::RunGameMain +0x25D`
← `WinMain +0x20C`, still inside startup, before any message pumping or input
initialisation.

### TWO DIAGNOSTIC FIELDS POINTED ELSEWHERE AND BOTH ARE STRUCTURALLY INCAPABLE OF BEING RIGHT

This is the transferable part of the attempt, because both fields look
authoritative in a log.

**1. The crash handler said `VCALL-of-0`** on an object whose first `0x40` bytes
are all zero:

```
Fault: EXEC of 0000000000000000
VCALL-of-0: this=000002a6403382a8 vtable=0000000000000000
```

That is the handler's **heuristic**, reading `[RCX]` and reporting what it finds.
It cannot distinguish a virtual call through a null vtable from an ordinary call
through a null function pointer whose `RCX` happens to hold a zeroed object. Here
it was the latter.

**2. `[RtUnresolved]` said `asked by 00007ff664585925`** — which symbolises to
**`RtResolveCached` itself**. The "asked by" field records the **reporting site,
not the call site**, so it can never name the caller. Every `[RtUnresolved]` line
in every run has this property.

**What actually settled it was the disassembly**, and it took three steps:

```
1402d8f8c:  call  1402d874a <...Engine_Startup...UlvE19_clEv>   ; resolver lambda 19
1402d8f91:  mov   0x208(%rbp),%rdx
1402d8f98:  mov   %rdx,%rcx
1402d8f9b:  call  *%rax                                          ; <-- faults, RAX = 0
1402d8f9d:                                                       ; <-- crash return address
```

`RCX == RDX == [rbp+0x208]`, which matches the crash dump's `RCX=RDX=0x2a6403382a8`
exactly. Lambda 19 loads a name string from `0x14050b000`, and reading that
address out of `.rdata` gives the answer directly:

```
14050b000  3f456e75 6d657261 74655669 64656f4d   ?EnumerateVideoM
14050b010  6f646573 40446576 69636540 56696465   odes@Device@Vide
14050b020  6f404051 4541415f 4e585a00            o@@QEAA_NXZ.
```

**Take the wall from the binary, not from the log's own hint.** The log named the
right symbol on its last line — `[RtResolve] MISS "?EnumerateVideoModes@..." ->
NULL` — but "last line before the crash" is a correlation, and the two fields
that look like causation both point at the resolver.

### The wall

`?EnumerateVideoModes@Device@Video@@QEAA_NXZ` — **ordinal 2319 at
`0x1800f8610`**, verified against the shipped export directory rather than
assumed. A **plain export**: no `AEAA`-means-private trap, no unexported
internal. An ordinary port.

### Two further cutover gaps the run named itself, both real and neither the wall

```
[VideoInterface] *** DETOUR FAILED *** Video::Device::InitializeInterfaces
    (0x1800F79F0) could not be resolved.
[VideoInterface]   With the DLL ABSENT (cutover) it means something MUST call
    VideoInterfaceFillDx11Slot() from the ported InitializeInterfaces --
    nothing else fills the slot, and nothing loads VideoDX9 either.

[FNHook] could not resolve ExternalFactory::FindOrLoad
[FNHook] could not resolve External::Load
[ResLoad] serialized: FindOrLoad=0000000000000000 External::Load=0000000000000000
```

The second is lane RRES's ground and is **live confirmation of why that lane's
deliverable had to be stated as a call-graph fact**: the census was flat for its
four ported bodies, while the cutover boot shows exactly why the row matters.

### Residue

**17 distinct `[RtUnresolved]` names**, unchanged in count from attempts 20/21 but
not in content — PLAT's three are gone. `Registry: 1617 registered, 21 stubs hit`.
