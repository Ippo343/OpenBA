# Wave 9, lane H — "caches a null forever" is a class, not a bug

2026-09-08, `wave/upgrade-path-complete`. **Source only. Nothing was built, deployed or run**
— lane N holds the game directory. Every file passes
`g++ -std=c++17 -fsyntax-only -Wall -Wextra` (a syntax check, not a build).

Follows [WAVE9_LANE_E3.md](WAVE9_LANE_E3.md) §8, where `?_singleton@MemoryAllocator` turned
out to be one instance of an idiom rather than one bug. The scope was: enumerate the class
properly, convert it, prove the conversion changes nothing *today*, and find any site whose
null is load-bearing before sweeping.

---

## 1. The defect, restated precisely

```cpp
static T* p = reinterpret_cast<T*>(ResolveExport("?_singleton@..."));
return p;
```

**The first caller fixes the answer for the whole process.** A miss caches null, and a
registration that lands later — which is exactly how cutover mints the exported DATA the DLL
used to provide — can never be picked up. Nothing says which symbol was missing: the consumer
either swallows the null behind an `if` and silently does nothing, or dereferences it several
frames away where the fault address names nothing.

The correct form already existed in the tree (`runtime/src/object/internal.cpp:143`,
`runtime/src/object/type_system.cpp:65` — `if (!g_x) g_x = Resolve(...)`), so nothing here is
invented, only applied. And the property that makes this lane worth spending: **the retry form
is behaviour-identical while the DLL answers**, so unlike every other cutover repair it is
verifiable before cutover rather than on the day no instrument survives.

---

## 2. The enumeration — and the denominator I was working from was wrong

`tools/resolve_cache_audit.py` (new). It joins physical lines into logical **statements**
before matching, because an earlier census tool in this project silently missed multi-line
constructs (`reference_census_three_blind_spots.md`) and most of these resolves span two or
three lines.

My first pass reported **206** frozen sites and I nearly reported that number. It was wrong,
and wrong in the most dangerous direction: **`RT_ORIGINAL_ONCE` and `RT_EXPORT_ONCE` are
macros that manufacture this exact idiom**, and their 282 call sites contain no
`ResolveExport(` text at all, so the tool could not see a single one of them. The audit now
counts the macro spellings too. Real population:

| bucket | what it means | before |
|---|---|---:|
| **FROZEN** | a null is cached for the life of the process | **481** |
| RETRY | re-resolves on a miss; the correct form | 79 |
| DIRECT | not cached | 1015 |
| | **total doors** | **1575** |

The 481 splits as: 212 `RT_ORIGINAL_ONCE`, 64 `RT_EXPORT_ONCE`, 139 hand-rolled
code/address, 40 hand-rolled `??_7` vtable, **26 hand-rolled exported DATA**.

`ResolveAddr(Module::Exe, …)` is deliberately excluded throughout: the exe does not go away
at cutover, so those doors do not change their answer.

### Severity is not uniform, and it is not a matter of taste

| kind | what a cached null does | count |
|---|---|---:|
| exported **DATA** (`?_singleton@`, `?_typeInfo@`) | swallowed by an `if`, or dereferenced in another subsystem. **Silent, and far from its cause.** | 26 |
| `??_7` vtable | stamped into an object; dies as **EXEC-of-0** at the first virtual call, and the VEH handler names the detour | 40 |
| code / function pointer | **EXEC-of-0** at the call | 139 + 64 |
| `RT_ORIGINAL_ONCE` | the oracle door — see §6, it is frozen **by design** | 212 |

So the swallowing subclass the brief asked for is the **26 DATA sites**, and it is now
enumerated rather than sampled. My earlier "I checked 4 of 22 and found 3" undercounted the
population and overcounted my confidence; the real membership is below.

### The 26, ranked by how early cutover executes them

Tier 1 is provable, tiers 2 and 3 are the order `Engine::Startup` brings subsystems up and
should be read as approximate.

| tier | symbol | sites |
|---|---|---|
| **1 — first, before anything else can allocate** | `?_singleton@MemoryAllocator@System@@` | `runtime/src/system/allocator.cpp:647` |
| 2 — inside `Engine::Startup` | `?_singleton@Device@Video@@` (×3), `?_singleton@Configuration@Video@@` (×2), `?_singleton@Persistence@IO@@` (×3), `?_singleton@Device@Input@@`, `?_singleton@Device@Audio@@`, `?_singleton@MainLoop@Engine@@` | `video/device.cpp:123,128,133`, `video/buffers.cpp:111,115`, `ui/element.cpp:276`, `input/input.cpp:93,588,616`, `audio/audio.cpp:875`, `engine/engine.cpp:200,209` |
| 3 — first frames / first load | `?_singleton@FileSystem@IO@@` (×3), `?_singleton@AchievementManager@Achievement@@`, `?_singleton@AnimationManager@Graphics@@`, and six `?_typeInfo@…` | `achievement/achievement_manager.cpp:264,274,281`, `graphics/mesh.cpp:331`, `achievement/goal.cpp:167`, `engine/loops.cpp:287`, `ui/element.cpp:263,268`, `code/Game/Game/gameupdateloop*.cpp` (×4), `standalone/achievement_probe.cpp:310` |

Tier 1 is worst not because its consequence is worse but because it happens **first**: it
fails before any other subsystem gets a chance to fail more loudly, and its fault is a read of
`null + 0x640` inside `MemAlloc` with nothing naming `_singleton`.

---

## 3. What changed

**One helper, two macro bodies, 26 call sites.**

`runtime/funcs/rt_resolve_cache.cpp` (new) — `RtResolveCached(mangled, &cache)`:

* a **hit** is cached and returned on every later call, so the steady-state cost is one plain
  load and there is no `GetProcAddress` in the hot path — which matters, because two of those
  in `System::Time` once cost 5.6 points of sim time
  (`reference_rtoriginal_is_getprocaddress.md`);
* a **miss** is not cached. The next call retries, so a late registration is picked up;
* a miss is **named once and counted**. `RtResolveMissCount()` / `RtResolveMissSummary()`.

`runtime/funcs/rt_port.h`:

* `RT_EXPORT_ONCE` now goes through the helper. **That one edit moves 64 call sites**, none of
  which needed touching.
* new `RT_DATA_SLOT(T, MANGLED)` for exported data slots.
* `RT_ORIGINAL_ONCE` deliberately unchanged — see §6.

The 26 DATA sites become uniform three-liners:

```cpp
 MemoryAllocator** SingletonSlot() {
-    static MemoryAllocator** slot = reinterpret_cast<MemoryAllocator**>(
-        ResolveExport("?_singleton@MemoryAllocator@System@@0PEAV12@EA"));
+    MemoryAllocator** slot = RT_DATA_SLOT(MemoryAllocator**,
+        "?_singleton@MemoryAllocator@System@@0PEAV12@EA");
     return slot;
 }
```

`standalone/main.cpp` gains one line: `RtResolveMissSummary()` beside `RtDiffSummary()`.

**Also corrected, a stale comment that was wrong in both halves.**
`runtime/src/video/device.cpp:120` said these singletons went through `ResolveExportRaw`
"because the registry holds function redirections only, and nothing has registered — or can
register — a data slot before cutover." The code called `ResolveExport`, not Raw; and
`RegisterRuntimeExport` is a plain name→pointer map that does not care whether the pointer is
code or data, so a data slot **can** be registered and cutover is exactly when it must be.
That comment, left standing, would have argued a future reader out of lane C2's mint.

### Interaction with lane C2

This lane and C2's are two halves of one repair and they only work together. C2 mints
`_singleton` as real storage via `RegisterRuntimeExport`; that registration is **only
reachable** because `ResolveExport` is registry-first and these sites now retry instead of
having frozen a null before C2's registration ran. Neither half is sufficient alone.

---

## 4. Proving it changes nothing — the denominator, before and after

A converted site contains no `ResolveExport(` text, so it would **vanish** from the first
version of the audit — and a site vanishing reads exactly like a clean pass. That has happened
twice in this wave (`reference_documenting_a_check_can_delete_it.md`). The audit therefore
counts the macro spellings, so a conversion is a bucket **move**:

```
BEFORE (clean worktree at HEAD, same tool):  FROZEN=417  RETRY=143  DIRECT=1015  doors=1575
AFTER  (working tree):                       FROZEN=391  RETRY=169  DIRECT=1016  doors=1576
```

Every unit accounted for:

* FROZEN −26 / RETRY +26 — the 26 hand-rolled DATA conversions;
* DIRECT +1, doors +1 — `rt_resolve_cache.cpp`'s own `ResolveExport` call, the one door this
  lane adds;
* nothing else moved, and **nothing disappeared**.

**The tool cannot show the macro edit, and I am not going to let the numbers imply it did
nothing.** It keys on the macro *spelling*, and the spelling did not change — only the
contract behind it. So the 64 `RT_EXPORT_ONCE` sites are counted RETRY in *both* columns even
though they were frozen-in-fact before this lane. Contract-level, the true movement is:

| | before | after |
|---|---:|---:|
| frozen **in fact** | **481** | **391** |
| retry in fact | 79 | 169 |

**90 sites left the frozen class: 26 by hand, 64 by one macro edit.**

Reconciling the audit against grep, because they disagree and the difference matters: grep
finds 218 lines containing `RT_ORIGINAL_ONCE(`; the audit finds 212 sites. The gap is 5
comment-only lines plus one statement at `runtime/src/engine/loops.cpp:828` carrying two uses.
212 + 5 + 1 = 218. The audit counts **statements**, which is what "a site" means here; grep
counts lines. Stated because an unexplained six-unit gap in an audit is how the last two
tool defects in this wave looked.

### The live measurement — prepared, NOT taken

The point of this lane is that it is verifiable today. That verification needs the directory,
which lane N holds, so it is specified here and owed:

```bash
mingw32-make standalone-debug && mingw32-make sa-deploy
tools/rt_run.sh h1 -- BANISHED_RT_DIFF=1 BANISHED_AUTOSTART=1 \
                      BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=60
tools/rt_run.sh h2 -- BANISHED_RT_DIFF=1 BANISHED_AUTOLOAD=Save9 \
                      BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=60
BANISHED_RT_DIFF=1 tools/stress_run.sh --debug --frames 1500
```

Pass conditions:

1. `[RtResolve] 0 misses — every cached resolve in this run found its symbol.` **A single
   `[RtResolve] MISS` line means a converted site is resolving to null today**, which the old
   frozen form would have hidden. Finding one would be a result, not a failure of the lane.
2. The `[RtDiff] ==== N sites, M calls, D divergences ====` totals match a same-binary run
   with the conversion reverted. **State the site count as well as the divergence count** —
   sites appearing or disappearing is the signal, and a matching `divergences=0` over a
   different denominator proves nothing.
3. Boot reaches gameplay and the achievement census is unchanged.
4. `[RtResolveEarly]` — reported, not gated. It answers a different question from this
   lane's conversion and does not pass or fail it. Zero scopes the early-snapshot class as
   theoretical on that path; non-zero is a finding to hand on.

_Superseded by §7: the runs were taken on 2026-09-08. Condition 2 as written turned out
not to be measurable and was replaced with a set comparison — §7.3-7.4._

There is no negative control to design here: the conversion is not a measurement, it is a
behaviour change whose claim is "nothing changed". The falsifier is condition 2's denominator.

---

## 5. What this does NOT fix — a second defect class from the same root, live TODAY

`if (!cache) cache = Resolve(...)` still freezes a **non-null** answer. So a site whose first
call happens **before `RuntimePortInit()`** captures the DLL's address permanently and bypasses
a port registered afterwards — the port-bypass defect
(`reference_bypassed_port_needs_a_source_checker.md`), frozen in, and it is a live bug today
rather than a cutover one.

Retry does not touch it, because retry only re-resolves a null. Anyone reading "frozen → retry"
as a fix for early snapshotting will be wrong. The real fix is ordering (resolve after
`RuntimePortInit`) or not caching at all, and it needs its own lane and its own audit — the
audit's DIRECT bucket (1016) and its ordering are exactly the material.

### 5.1 It is now MEASURED rather than argued

No existing instrument can see this class. The site is spelled correctly and goes through the
right registry-aware door, so `tools/port_bypass_audit.py` finds nothing; and the diff oracle
reports the port clean for the reason it always does when a port is bypassed — our body never
runs, so it never disagrees. **Only the ordering is wrong.**

So the ordering is measured directly, and it costs nothing to carry on the runs this lane
already owes. `runtime/funcs/rt_resolve_cache.cpp` records every resolve that happened
**before `RuntimePortInit()` returned AND missed the port registry at the time**, with the
caller's return address. At summary each recorded name is asked of the registry *again*:

| verdict | meaning |
|---|---|
| **registered NOW** | the port exists and this caller did not get it. If the site caches, that is a **live port bypass** |
| still absent | benign — nothing has ever ported it, so reaching the DLL early is the same answer it would get later |

Hooked into the three **registry-aware** doors only: `ResolveExport`, `ResolveOrdinal`,
`ResolveAddr(Module::Runtime)`. `ResolveExportRaw` is deliberately not hooked — that door is
*supposed* to reach the shipped body, so an early raw resolve is not a bypass.

Two limits, stated so the number is not over-read. It cannot tell whether the CALLER caches
— that is what the recorded return address is for (`nm` against
`standalone/build/reimpl-x64.exe`, image base `0x140000000`). And a *distinct-site* cap of 256
means a pathological run could truncate the listing, though the total count keeps rising.

`RtResolveMarkPortInitDone()` is called at the end of `RuntimePortInit()`
(`standalone/resolve_standalone.cpp`), and `RuntimeExportIsRegistered()` is new there because
`ResolveExport`'s return value cannot distinguish a registry hit from a DLL hit — and that
distinction is the whole question.

**Either answer is worth having.** Zero means the class is theoretical on that path and can be
scoped as such. Non-zero names the specific sites, and we will know we have live port bypasses
in the tree today. **Do not fix any it finds** — name them and hand them on; that is a
different lane with a different oracle.

---

## 6. Requirement 4 — is any null here load-bearing?

Lane V2 found one raw door that must never be converted (`RemoveGraphs` **is** the registered
port of its own name, so resolving through the registry would return itself and recurse to
stack death). Checked for the equivalent before sweeping:

**`RT_ORIGINAL_ONCE` is the load-bearing case, and it is 212 sites.** It is the *oracle* door:
`ResolveExportRaw`, deliberately registry-blind, whose entire job is to reach the **shipped**
body. Converting it to the retry helper would (a) route it through `ResolveExport` and make
every oracle compare ours against ours, and (b) at cutover retry a `GetProcAddress` against an
absent module on every call. WAVE9.md §2 already says these call sites must be **compiled
out**, not made to return null. Left untouched, deliberately.

**Self-resolve candidates: 3, all checked, none recursive.** The audit's `--selfres` mode
lists names that are both resolved (non-Raw) and registered — 455 tree-wide, which is the
design, since that is how our call sites reach our ports. Narrowing to *the same file doing
both*, which is the `RemoveGraphs` shape, leaves three:

| site | resolves | recurses? |
|---|---|---|
| `runtime/src/engine/loops.cpp:947` | `?Load@Loader@Engine@@…` | **no** — a different function in the file registers it; documented as intentional |
| `runtime/src/ui/widgets_input.cpp:194` | `?SetValue@NumberEditElement@UI@@…` | **no** — the ported `NumberEditElement_SetValue` (`:316`) does not call the accessor; only `:438/:454/:537`, other functions, do |
| `runtime/src/system/thread_win.cpp:142` | `?ThreadFunc@Thread@System@@…` | **no** — `RtSystem_Thread_ThreadFunc` (`:284`) does not call it; only the ctor at `:168` does, which is the intent |

None of the three is in the conversion set (all are CODE, not DATA). **No load-bearing null
was found in the 26 converted sites.**

---

## 7. Files, and the do-not-touch list

Touched: `runtime/funcs/rt_resolve_cache.cpp` (new), `tools/resolve_cache_audit.py` (new),
`runtime/funcs/rt_port.h`, and the 14 files holding the 26 sites, plus one line in
`standalone/main.cpp`.

Not touched, as instructed: anything in the WorkComponent / work-scheduler family (none of the
26 is in it), `standalone/rt_data_mint*.*` (lane C2's), and `BANISHED_RT_ALLOC` was not armed
and not set.

## 8. Carried forward, belonging to no current lane

* **WAVE9_LANE_E2.md §5.2** — the allocator's `Shutdown`, `~MemoryAllocator`, `operator=`,
  `GetAllocationSize`, `GetAllocatedSize`, `Get`: never exercised by the shadow, gate step 4.
* **WAVE9_LANE_E2.md §5.3** — the 5% `sim ms/iteration` gate, three runs per config on the
  perf exe (`reference_one_perf_run_cannot_resolve_the_gate.md`), gate step 4.
* **§5 above** — early-snapshot port bypass, a live defect class needing its own lane.
* **The allocator is READY TO ARM AND IS NOT ARMED.** Nothing in this lane changes that; the
  decision is the wave lead's and has not been made.

---

## 7. RESULTS — 2026-09-08. The conversion is PROVED INERT, and my pass condition was not measurable

Ten runs. **The live proof passes** — but not on the criterion I wrote, because that criterion
turned out to be unmeasurable at the sample size it assumed. That is the main finding of this
section and it is recorded before the numbers.

### 7.1 Provenance

| | |
|---|---|
| treatment | built from tip `57da6a1`, **stamp 2026-09-08 01:46:22** |
| control | the same tip with lane H's two commits (`8055bf7`, `57da6a1`) `git revert`ed in a detached worktree, **stamp 01:51:01** |

Self-evidencing, not just the stamp: the treatment emits two prefixes **no earlier binary in
this project can produce**, and the control — correctly — emits neither (`grep -c RtResolve` = 0):

```
[RtResolve] 0 misses — every cached resolve in this run found its symbol.
[RtResolveEarly] 3 early registry-aware resolve(s), 3 distinct site(s). ...
```

### 7.2 The denominator, re-measured against the new tip

Lane N landed between the write-up and the run, so the before/after pair was re-taken rather
than quoted from §4 (quoting a stale before-column against a fresh after-column would
attribute lane N's landings to my conversions):

```
BEFORE (tip minus lane H's two commits):  FROZEN=417  RETRY=143  DIRECT=1015  doors=1575
AFTER  (tip):                             FROZEN=391  RETRY=169  DIRECT=1016  doors=1576
```

**Identical to §4.** Lane N added no resolve doors, so §4 needs no correction. Re-measured
rather than assumed.

### 7.3 My pass condition could not be met by ANY pair of runs — including two of the same binary

I specified: the `[RtDiff] ==== N sites, M calls, D divergences ====` line must match a control
with the conversion reverted, **N compared and not just D**. The lead held me to it. It came
back:

```
treatment h1  331 sites          control c1  329 sites
```

A mismatch. Before reading anything into it I ran the control **again, same binary, same
cell** — the repeatability control I should have specified in the first place:

```
control c1  329 sites      control c1b  331 sites      control c1c  329 sites
```

**The same binary gives 329, 331, 329.** The site count is nondeterministic ±2, so no pair of
single runs can satisfy the criterion — not even two runs of the identical binary. As written
it was strict-sounding and unusable, which is the shape this wave has caught five times in
other people's criteria and once, now, in mine.

### 7.4 The replacement is stronger: compare the SET, not the count

An `RT_DIFF_BEGIN` site registers when its function is first *entered*, so the count is a
function of which code paths a run happened to take. The **set** is comparable, and a
difference names itself. Diffing the two control runs immediately identified the ±2:

```
Math::Transform::Identity()
Video::Context::SetInverseScale
```

Two runs per binary then give a stable core (intersection) and a full population (union):

| | treatment | control | verdict |
|---|---:|---:|---|
| stable core (∩ of 2 runs) | 329 | 329 | **byte-identical set** |
| union (∪ of 2 runs) | 331 | 331 | **byte-identical set** |
| flaky sites | the same 2 | the same 2 | same on both binaries |

`diff` output on both comparisons: empty. And the deeper cell agrees on a single pair — H2
(`AUTOLOAD=Save9`) came back **354 sites on both binaries with an identical set**.

That is a better instrument than the one I asked for: it discriminates, it survives the noise,
and when it fails it hands you the site name instead of an integer.

### 7.5 Every run

| run | binary | cell | sites | diffed calls | divergences | `[RtResolve]` misses |
|---|---|---|---:|---:|---:|---:|
| h1 | treatment | autostart | 331 | 29,843,927 | **0** | **0** |
| h2 | treatment | `AUTOLOAD=Save9` | 354 | 54,570,583 | **0** | **0** |
| h3 | treatment | stress 1500f | 369 | 129,181,750 | **0** | **0** |
| t1a | treatment | autostart | 329 | 29,005,207 | **0** | **0** |
| t1b | treatment | autostart | 331 | 28,866,618 | **0** | **0** |
| t2 | treatment | `AUTOLOAD=Save9` | 354 | 54,387,798 | **0** | **0** |
| c1 | control | autostart | 329 | 28,766,912 | 0 | n/a |
| c1b | control | autostart | 331 | 29,179,135 | 0 | n/a |
| c1c | control | autostart | 329 | 28,864,194 | 0 | n/a |
| c2 | control | `AUTOLOAD=Save9` | 354 | 54,435,389 | 0 | n/a |

**325,855,883 diffed calls on the treatment binary, 0 divergences, 0 resolve misses.** H3 did
not hit the known teardown crash (`standalone_crash.log` 0 bytes), so no re-run was needed.

### 7.6 VERDICT

| condition | result |
|---|---|
| `[RtResolve]` misses = 0 on every treatment run | **PASS** (6 of 6) |
| `[RtDiff]` site **set** matches the control | **PASS** — core and union identical on H1, identical on H2 |
| `[RtDiff]` site **count** matches the control | **not measurable** — ±2 on the same binary; superseded by the set comparison |
| divergences = 0 | **PASS** (10 of 10 runs) |
| boot reaches gameplay | **PASS** on all cells |

**90 sites moved out of the frozen class and nothing observable changed.** `0 misses` is the
direct evidence: every one of the 26 converted data slots and 64 `RT_EXPORT_ONCE` sites
resolved its symbol on every run, so the retry path was never taken and the conversion is
inert while the DLL answers — which is exactly the property that made this lane verifiable
today instead of on cutover day.

## 8. `[RtResolveEarly]` — the early-snapshot class is NOT LIVE on these paths

Identical on all three cells:

```
[RtResolveEarly] 3 early registry-aware resolve(s), 3 distinct site(s).
[RtResolveEarly]   benign  caller=…f1a8  addr 0x1800c6e60
[RtResolveEarly]   benign  caller=…f5a5  addr 0x1800c6c00
[RtResolveEarly]   benign  caller=…db21  "?_singleton@Factory@Object@@0PEAV12@EA"
[RtResolveEarly] 0 of 3 are registered now -- those are the candidates.
```

**Three early resolves, zero of them registered now, so zero live bypasses.** All three are
structural rather than accidental:

1. `0x1800c6e60` = `MemoryAllocator::Allocate`, resolved by `InstallAllocatorLock()`. It runs
   before `RuntimePortInit()` **by design** — lane E moved it there so the lock owns the
   prologues before the allocator swap can contend for them (WAVE9_LANE_E3.md §1). `Allocate`
   is not registered because `BANISHED_RT_ALLOC` is off, so there is no port to miss.
2. `0x1800c6c00` = `MemoryAllocator::Free`, `InstallFreeLock()`. Same.
3. `?_singleton@Factory@Object@@` — resolved from inside a `*PortInit()` body
   (`runtime/src/object/external.cpp:673`, which assigns `g_pFactorySingleton` immediately
   before its `RegisterRuntimeExport` calls). Port-init code resolving while port-init runs is
   early *by construction* and unavoidable. It is exported DATA that nothing registers, and
   the tree's other site for the same name (`runtime/src/object/type_system.cpp:66`) already
   uses the retry form.

So on the autostart, save-load and stress paths the class is **theoretical**, and can be
scoped that way. Two limits on that conclusion, stated rather than buried: the instrument sees
only the three **registry-aware** doors (`ResolveExportRaw` is deliberately excluded — it is
*supposed* to reach the shipped body), and "not live on these three paths" is not "not live
anywhere". Nothing was fixed; there was nothing to fix.

## 9. Handed on

* **`Math::Transform::Identity()` and `Video::Context::SetInverseScale` enter on some runs and
  not others**, on both binaries, in the same cell. Not this lane's and not a defect on its
  face — but it means **any** future comparison of `[RtDiff]` site counts across builds is
  noisy by ±2, and anyone reading a 2-site delta as signal will be chasing nothing. Compare
  sets.
* **§5's early-snapshot class stays open as a class**, measured-not-live on three paths. It
  still has no instrument outside this counter, and the counter only covers registry-aware
  doors.
* Unchanged and belonging to no lane: WAVE9_LANE_E2.md §5.2 and §5.3 (gate step 4).
* **`BANISHED_RT_ALLOC` was unset in all ten runs. The allocator remains READY TO ARM AND NOT
  ARMED.**
