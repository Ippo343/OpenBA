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
| **E. Allocator** | Arm `BANISHED_RT_ALLOC`, revisit the NO-GO, and take the heap. Includes `Engine::Startup`'s three direct field writes and `Shutdown`. | `sa-stress` clean under `BANISHED_REDZONE=watch` |
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

1. **`BANISHED_RT_DIFF_INVERT=1`** — run the DLL's body FIRST on the scratch. A one-flag
   change to the replay macro. On the first body tried it crashed in 20 seconds inside the
   allocator with the diverging value's digits in a free block. It surfaces every body
   that double-manages an allocation **by crashing on it** — no checker to maintain, and
   it tests the SHAPE not the symptom, so it finds the ones nobody suspected.
2. **A runtime assert** — before replaying, walk the scratch for heap pointers; after our
   body runs, check whether any now addresses a block whose `{length, refcount}` header at
   `[-8]` reads zero. Name the function and abort. This converts "someone must remember
   `widgets_input.cpp` lines 53-58" into a self-reporting harness.
3. **A DID-WORK counter** beside `calls=`, so a site cannot look covered on early-outs.

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
| **Differential oracle, full matrix** | `BANISHED_RT_DIFF=1` across the harness cells | ⚠️ **NOT MET 2026-09-08 — 6 cells clean, 3 with findings** (lane M, [WAVE9_LANE_M.md](WAVE9_LANE_M.md), 22 runs on ONE binary stamped 2026-09-07 23:29:48). **Clean, each with a FIRING negative control:** main menu 181 sites/975,139 calls, gameplay 314/29.3M, save+load 335/54.4M, menus-Achievements 195/1.7M, menus-Mods 182/2.2M, tutorial 0 328/8.8M. `calls=` was checked, not just `divergences=`: of 417 distinct sites exactly **two are never called in any cell** — `UI::EditElement::SetText` (a documented NO-ORACLE port, so this row does not cover it) and `Audio::Scene::Serialize` (covered by lane S's save-trace oracle, not by this row). **OPEN: (1)** `UI::NumberElement::SetValueInt` diverges 9/17/10 over exactly 12,896 calls in tutorials 1-3 — an **ORACLE** defect, and NOT cosmetic: the replay is a shallow memcpy, so both branches run `CacheText` from the same buffer pointer and the oracle **hands the DLL a pointer into memory our own call has already released** — a use-after-free given to retail's own body on ~10 calls per run — and **under ORDER INVERSION it demonstrably corrupts the allocator's free list** (ACCESS_VIOLATION, `bucket 8: free block has CORRUPTED forward link`, with `"272"`, the input of diverging call #3, sitting inside a freed block). ⚠️ *Corrected 2026-09-08 by lane M against its own first wording:* “double-releases” was an INFERRED mechanism, not an observed one — in the shipped order retail leaves `+0xd0` untouched and no shipped-order run crashed, so a second free by retail is **not established**. The verdict is unchanged; the mechanism claim is narrowed to what was measured. Fix is to make it a NO-ORACLE port, never a byte-range exclusion. **(2)** `Picking::Scene::Intersect`, 90 of 20,868 in **1 stress run of 4**, `.z` off by 3.8e-6 against a port whose own comment demands bit-exactness — and **a clean re-run does NOT clear it**, because the rays are mouse-driven and simply were not cast. `hitT <= bestT` vs `<` is a hypothesis, not a cause. **(3)** an intermittent shutdown crash freeing `0x000001d9_fffffffa` (the half-written-pointer signature) which **destroys the whole `[RtDiff]` report for that run**. **(4)** two menu routes crash independently of the oracle. **NOT DRIVEABLE by this harness:** the in-game save WRITE path and world-teardown-to-menu. |
| **Minted DATA vs the DLL** | `BANISHED_RT_MINT_SHADOW=1` (lane C2). **Not byte-for-byte** — a filled slot holds OUR address by construction, so a byte compare would fail on exactly the slots that represent progress. It compares the MAPPING (the DLL's slot holds function X; ours holds our registered port of X, and never a DLL address), plus `BaseTypeInfo` fields by `strcmp`, excluding `next` and BSS contents. | **PASSED 2026-09-08**, exe stamp `00:24:27`, `rt_run[mintctor] rc=0`: **429 ground-truth slot comparisons / 0 MISMATCH**, 78 slots minted from our own ported code / 0 MISMATCH, **165 `BaseTypeInfo` field comparisons / 0 MISMATCH**, 0 slot-count suspects, 0 symbols absent, `clean: 0 problem(s)` at **both** `phase=init` and `phase=exit`. 14 of 46 vtables complete. `typeId INFO` 0 at init / 33 at exit, the predicted `Factory::Startup` ordering. |
| **Constructed statics vs the DLL** | `BANISHED_RT_MINT_CTOR=1 BANISHED_RT_MINT_SHADOW=1` (lane C2). The DLL's `.CRT$XC` array constructs three of the minted symbols at load; the mint reproduces them and the check compares each pair's non-null WORD SHAPE (values cannot match — different vtable, different HANDLEs, a `time()` seed). Catches a ctor that leaves the handles or the vtable null. | **PASSED 2026-09-08**, same run: **3 of 3 built, shape compared on 3, 0 SHAPE MISMATCH.** One `SHAPE-INFO` at exit — `?_loader…+0x30` (`Loader::_loop`) set in the DLL, null in ours, because nothing hands our Loader a loop. **The run also FOUND a defect**: `?_rng@Particle@Graphics@@` reported `DLL=WRITTEN` at init, refuting lane C2's earlier "never seeded" claim; it is constructed by initialiser `0x180149650` with a `_time64`-seeded MT19937. Fixed, and the sweep that missed it now matches a field reference, not only a base address ([WAVE9_LANE_C2.md](WAVE9_LANE_C2.md) §2). |
| **Allocator shadow** | `BANISHED_RT_DIFF=1 BANISHED_RT_ALLOC_SHADOW=1` on `sa-run` + `sa-stress` + autoload, plus a deliberate perturbation that must go red | **GATE STEP 2 MET 2026-09-07 — GO, meaning READY TO ARM, NOT ARMED** ([WAVE9_LANE_E2.md](WAVE9_LANE_E2.md)). All four conditions on one binary: `divergences=0` on three routes, **12,750,873** shadowed operations, all four skip counters zero, and a negative control that goes red (392,243). `BANISHED_RT_ALLOC` was never set — the heap was not taken. Three defects found, **all in the ORACLE, none in the port**: (1) it armed on `MemoryAllocator::Startup`, which has **zero callers** (MSVC inlined it into `Engine::Startup`; a rel32 scan of the whole DLL `.text` finds none, and finds 348 for `Allocate`) — it read `armed=0 observed=0 skipped_pre_startup=1,788,875`, **a green summary over nothing**; (2) the replay map deleted by writing `nullptr`, which is also its linear-probe terminator, so every delete truncated colliding keys' probe chains; (3) **the PRESCRIBED negative control cannot fail** — `minBin=0xbe` is overwritten by `AddNewMemoryRange` while arming, so accepting it would have rested the entire gate on a probe only ever seen green when broken. Replaced by a geometry control that fires on the first perturbed call. **STILL OPEN:** the arena-growth path never executed — a coverage control reports `ranges ours=1 theirs=1` on all three runs, so the second `MemoryRange`, the range-table realloc and every `rangeIndex != 0` comparison are unexercised by all 12.75M ops. **Close before gate step 4.** |
| **`[RtRawResidue]` verdicts** | every row ORACLE-ONLY / REAL DEPENDENCY / DATA READ / DEAD | **MET 2026-09-08** (lane V, [WAVE9_LANE_V.md](WAVE9_LANE_V.md) §7 + `wave9_rawresidue_verdicts.csv`): **104 of 104 rows carry a verdict, zero undetermined** — REAL DEPENDENCY 26, DATA READ 29 (26 minted, **3 NOT**), ORACLE-ONLY 38, DEAD 11. Round 1 read 70/29/3/2; every round-1 finding had since been acted on, so **re-running the census after the tree moved changed the answer** — the `IO::Serializer` gate repair alone moved 22 rows to ORACLE-ONLY. **Nothing was defaulted to DEAD**: a DEAD verdict required a literal-and-alias sweep finding no raw-door site, with the port-aware door and registration named. ⚠️ **OPEN, handed to lane C2 — a cutover CRASH:** `??_7MeshInstance@Graphics@@6B@`, `??_7EdgeMeshInstance@Graphics@@6B@`, `??_7EdgeMesh@Graphics@@6B@` (`mesh.cpp:271/275/279`) are read via `RtVtable()` → `ResolveExportRaw` and **stamped into slot 0 by a ported ctor**, and are NOT in the mint — at cutover that is a NULL vtable in every such object and an EXEC-of-0 on the first virtual call. **And the door is wrong as well as the table**: `RtVtable()` is `ResolveExportRaw` for *all* callers including the already-minted `??_7Base@Object@@6B@`, and the raw door is `GetProcAddress`, which never consults the port registry — so adding table rows alone would not fix it. |
| **`resolve_fallback_audit.py` reports 0 SITES** | not 0 defects — 0 **sites** | **MET 2026-09-07, lane F** ([WAVE9_LANE_F.md](WAVE9_LANE_F.md)): **0 sites, 0 defects, exit 0.** Deleted in two batches — 137 whose primary names a registered port, then the 26 still on the DLL — with the count checked against the audit after each, because a site *vanishing* reads exactly like a clean pass. Replaced by a null-resolve policy: `ResolveExport`/`ResolveOrdinal` now record and NAME every miss (`[RtUnresolved]`), non-fatal by default because on cutover day every unported symbol misses and aborting on the first one prints one name instead of the work list. |
| **`rawdoor_audit.py` reports 0 UNGUARDED-CALL** | a raw call outside a guard reaches the DLL on an ordinary run today, and is a **null call** the moment the DLL is gone | **MET 2026-09-08** (lane V2, [WAVE9_LANE_V2.md](WAVE9_LANE_V2.md)): **0 UNGUARDED-CALL, 518 sites scanned**, self-test green. 28 -> 0; denominator 555 -> 519 -> 518, and **555 - 519 = 36 = exactly the number of sites converted**, per file — a conversion removes a site from the population, so the drop MUST equal the conversion count, and it was checked because a vanishing site reads exactly like a clean pass. **27 of 28 were REAL DEPENDENCY** -> `RT_EXPORT_ONCE`. **1 was ORACLE-ONLY and must NEVER be converted**: `widgets_layout.cpp` `RemoveGraphs` — that function IS the registered port of its own name, so `RT_EXPORT_ONCE` would resolve to ITSELF and recurse to stack death. It was also sitting OUTSIDE its guard between `RT_DIFF_BEGIN` and `RT_DIFF_SCALAR`, calling retail every frame; now inside `if (rt_diff_on__)`. **A blanket conversion — the obvious way to drive this gate to zero — would have shipped that crash.** 0 DATA READ; **0 DEAD, because none could be PROVEN unreachable and none was asserted.** ⚠️ **Two caveats that survive the green number.** (1) `rawdoor_audit` has a MEASURED blind spot: its resolve-and-call regex is per-line, so a site whose argument list opens on the NEXT line is invisible. Measured, not guessed — 364 immediate-call sites, 83 line-wrapped, **4 of those UNGUARDED and invisible to the audit** (`audio.cpp:451,760`, `filesystem.cpp:139,145`), one of which sits TWO LINES above two sites lane R did convert *because the checker flagged those and not it*. All six converted; probe kept as `docs/runtime_port/wave9_laneV2_wrapscan.py`. **The checker is measured, NOT fixed** — the fix is to paren-match forward. (2) **Reachability is established for NOTHING here** — the evidence is structural, never "a run executed it", and the 36 conversions have never been EXECUTED, twelve of them on the save/load path. Each resolves an unported-or-detoured name so `ResolveExport` returns the same pointer `ResolveExportRaw` did, but only an autoload cell settles it. ⛔ **0 UNGUARDED-CALL IS NOT "NO NULL CALLS AT CUTOVER" — lane V, 2026-09-08.** `ResolveExport` misses the port registry for an unported symbol and **falls through to `ResolveExportRawImpl`** (`standalone/resolve_standalone.cpp:692`), which returns null with no module handle. So converting `RT_ORIGINAL_ONCE` → `RT_EXPORT_ONCE` on a symbol that has **no port and no mint entry** changes only WHICH RESIDUE LIST IT PRINTS IN. It is still a null call the moment the DLL is gone. **14 of lane V2's conversions are in exactly that state** — `?UpdateList@SaveLoad`, `??0CompressedStream`, `?Stop@Device@Audio` and eleven more. **25 rows remain real cutover work: 11 still on the raw door + 14 migrated-but-unsatisfied.** This gate row is necessary and NOT sufficient; the sufficient condition is a port or a mint entry, which is lane B/C/E work. |
| **`port_bypass_audit.py --by-name` reports 0 on BOTH doors** | a ported function reached through the wrong door is invisible to the census *and* to the diff oracle | **MET 2026-09-08** (`b3fc8b1`, lane H): **ADDRESS door 189 sites / 0; NAME door 437 sites / 0**; `--by-name` cross-check **573 ported names, 378 raw-door statements mention one, 0 the parser did not see** (non-zero exit if that is ever not 0). **The blind spot this row used to carry is CLOSED.** It read "0 means none of the shape it can see", and that was true: `called` meant *an argument list follows on this LINE*, so the accessor shape — resolve into a local, call it a statement later — classified `OK-RESOLVE-ONLY`. `value_types.cpp` held **three** live-path `String::Release` bypasses and the audit reported **one**. The classifier now follows the assignment target through the **brace-matched enclosing function** (never a line window). Proved against a known answer, not by the number moving: on a scratch snapshot of the exact tree where the three existed, same 364-site denominator, **OLD tools give BYPASS=1 and NEW give BYPASS=3** — kept as `tools/fixtures/bypass_value_types_before.cpp` under `--self-test`. A second defect fell out of the cross-check: mangled names were read from the door's own LINE, so a site whose name sits on a **continuation line** — the normal formatting here — got **no verdict at all**. 76 such sites; the population was 361 when it should have been **437**. None was a bypass, but they were counted as scanned while receiving no verdict. ⚠️ **Still re-run after every LANDING, not after every edit** — `--by-name` does not change that a correct raw call becomes a bypass when a DIFFERENT lane ports its name, with nothing in that file changing (`Timer::SecondsToTicks`/`TicksToSeconds` did exactly this). |
| **H. Perf gate** | The deferred +7.7% breach. The user deferred it at the Wave 4 boundary explicitly *to this stage*. With the DLL gone the seam's `GetProcAddress`/registry indirection can be compiled out entirely, which is the main reason it was deferred here. Three runs minimum — `reference_one_perf_run_cannot_resolve_the_gate.md`. | ≤5% over the recorded baseline |

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
