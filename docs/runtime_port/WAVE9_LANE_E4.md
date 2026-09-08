# Wave 9, lane E4 — the allocator's arming path was wired; its safety guard was blocking it

2026-09-08, `wave/upgrade-path-complete`. **Source only so far** — this compiles and links
(exe stamp 2026-09-08 03:36:05) but has not been run. The DLL-absent proof is owed and
specified in §5.

---

## 1. Correction: "BANISHED_RT_ALLOC appears only in comments" is not right

The brief for this lane says the allocator port "is complete, gate-passed, and has no arming
path", on the evidence that `BANISHED_RT_ALLOC` appears only in comments. It does not. The
arming path exists and is correctly gated:

```cpp
// runtime/src/system/rt_system_port.cpp:47-50
// Opt-in, not a kill switch: absent env means OFF.
{ const char* e = std::getenv("BANISHED_RT_ALLOC");
  if (e && std::strcmp(e, "0") != 0 && std::strcmp(e, "false") != 0)
      n += RtSystemAllocatorRegister(); }
```

The grep missed it because the read is `std::getenv("BANISHED_RT_ALLOC")` on a continuation
line inside a braced block, and `RtSystemAllocatorRegister` is reached through an `extern int`
declaration at `:34` rather than a header. Same family as the blind spots this wave has been
cataloguing — the name is there, the checker's shape was not.

**"Ready to arm and not armed" was accurate. There is an arm to pull.** What is true is
something more specific and more useful.

## 2. The actual cause: the guard is a DLL-present concept

`RtSystemAllocatorRegister()` refuses in this order, and at cutover it dies on the third:

```cpp
if (const char* patched = FindPatchedPrologue()) { /* REFUSE */ }
```

```cpp
const char* FindPatchedPrologue() {
    const void* alloc = ResolveExportRaw("?Allocate@MemoryAllocator@System@@QEAAPEAX_KH@Z");
    const void* freeF = ResolveExportRaw("?Free@MemoryAllocator@System@@QEAAXPEAX@Z");
    if (!alloc || !freeF) return "unresolved (Runtime-x64.dll not loaded?)";
    ...
```

With the DLL renamed aside both raw lookups return null, so the guard reports
`"unresolved (Runtime-x64.dll not loaded?)"` and the function returns 0 **having registered
nothing**. That is why `MemAlloc`/`MemFree` were on attempt 05's unresolved list.

The attempt-05 log should therefore contain, verbatim:

```
[RtPort] MemoryAllocator: REFUSING to arm -- unresolved (Runtime-x64.dll not loaded?) is already patched. ...
```

**Worth confirming in the attempt-05 log**, because if that line is *not* there the diagnosis
is wrong and something earlier in the chain refused instead.

Even past the guard, `RtAllocSwapInstall` would have failed: it calls
`AllocatorLockRetireForPort()` (nothing to retire) and then `InstallRuntimeDetourChecked` on
two DLL addresses that are not mapped. So there were two independent DLL-present assumptions
on the arming path, and both would have blocked it.

### Why that is the right shape to fix rather than a bug to patch out

The guard and the detour are correct while the DLL is loaded. Both exist for one reason: **the
DLL's own internal callers reach `Allocate`/`Free` by direct rel32 `CALL`, never through the
export table**, so registering the exports redirects our ~1,180 call sites and nothing else,
and a half-swap means we allocate while the DLL frees.

**With no DLL there are no internal callers.** Registering the exports *is* the complete swap,
and the atomicity requirement has nothing left to protect against. So cutover does not need a
weaker version of the DLL-present path — it needs a different, simpler one.

## 3. What landed

A second seam, `RtAllocCutoverArmer`, parallel to the detour installer rather than a fallback
inside it — the two do genuinely different jobs and collapsing them would hide that.

| file | change |
|---|---|
| `runtime/types/system/MemoryAllocator.h` | `RtAllocCutoverArmer` typedef + `RtSystemAllocatorSetCutoverArmer()` |
| `runtime/src/system/allocator.cpp` | `RuntimeDllPresent()`, `PlatformSeamMissing()`, the cutover branch, and the export list factored into a shared `RegisterAllocatorExports(allocateEntry, freeEntry)` used by both paths |
| `standalone/rt_alloc_swap.cpp/.h` | `RtAllocSwapArmCutover()` — installs no detour, hands back the composed stubs |
| `standalone/main.cpp` | `RtSystemAllocatorSetCutoverArmer(&RtAllocSwapArmCutover)` beside the detour installer, before `RuntimePortInit()` |

### The critical section is the part that does not come for free

On the DLL-present path the lock reaches the raw entry points either through
`allocator_lock.cpp`'s prologue patches or through the composed detour stub. **At cutover
neither exists** — there is no prologue to patch and no detour — and retail's own
`Allocate`/`Free` take no lock at all (`allocator_lock.h`). Registering the raw ported bodies
would hand out an unlocked `?Allocate@`/`?Free@` and reintroduce exactly the free-list race
`allocator_lock.cpp` was written to fix.

So the cutover armer hands back the **same composed stubs** the detour would have installed,
and those are what `RegisterAllocatorExports` binds to the two member exports.
`rtalloc::MemAlloc`/`MemFree` take the lock themselves, faithfully, as retail's statics do.

### Requirement 2 — seam first, and refuse by NAME

`RtSystemAllocatorRegister()` still installs the platform seam before anything else and still
refuses when it is incomplete. The refusal now **names the first missing entry**:

```
[RtPort] MemoryAllocator: REFUSING to arm -- platform seam incomplete, first missing
entry is 'lockInit (InitializeCriticalSectionAndSpinCount)' ...
```

`g_platInstalled` is one bool over ten function pointers, and lane X spent a probe on a
half-built seam when the message said only "not installed". Same reasoning as lane H's named
refusal: a null that is reported by name costs minutes, and a null that is swallowed costs a
session.

### DLL presence is asked structurally, not from an env var

`RuntimeDllPresent()` is `ResolveExportRaw` of the two allocator entry points. `Raw`
deliberately bypasses the port registry, so it cannot be fooled by our own registrations, and
it does not depend on anyone remembering to set `BANISHED_RT_NO_DLL`. A boot that lost the DLL
for some other reason gets the same correct branch.

## 4. Default-off, and how that will be demonstrated

Nothing runs unless `BANISHED_RT_ALLOC` is set: `rt_system_port.cpp` calls
`RtSystemAllocatorRegister()` only inside that env test, so an unset run executes none of this
code at all. The two new `main.cpp` lines only store function pointers.

The demonstration is the lane W / lane C2 shape — a self-evidencing line only an armed build
can emit, plus a control showing an unset run is unchanged. Two new lines, and **neither can
be produced by any earlier binary**:

```
[RtAlloc] ARMED (cutover: Runtime-x64.dll absent) -- N exports registered, no detour
          installed because there are no DLL-internal callers to redirect. ...
[RtAlloc] ARMED (atomic swap taken, Runtime-x64.dll present) -- N exports registered ...
```

The `[RtAlloc]` prefix itself is new; the old success line was `[RtPort] MemoryAllocator:
ATOMIC SWAP TAKEN`.

## 5. The runs this lane owes

Needs the game directory. Restore `Runtime-x64.dll` afterwards **without fail** in every case.

```bash
mingw32-make standalone-debug && mingw32-make sa-deploy

# E4-a  CONTROL: unset, DLL present. Must be indistinguishable from today.
tools/rt_run.sh e4a -- BANISHED_RT_DIFF=1 BANISHED_AUTOSTART=1 \
                       BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=60
#   expect: NO [RtAlloc] line at all, [RtDiff] site SET matching lane H's
#   (count is noisy ±2 — WAVE9_LANE_H.md §7.3, compare sets)

# E4-b  ARMED, DLL PRESENT. The first time the port has ever owned the live heap.
tools/rt_run.sh e4b -- BANISHED_RT_ALLOC=1 BANISHED_AUTOSTART=1 \
                       BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=60
#   expect: [RtAlloc] ARMED (atomic swap taken...), [AllocLock] patches RETIRED,
#   boot to gameplay, clean quit

# E4-c  ARMED, DLL ABSENT — the one this lane exists for.
#   rename Runtime-x64.dll aside, then
#   BANISHED_RT_NO_DLL=1 BANISHED_RT_MINT=1 BANISHED_RT_STRICT=1 \
#   BANISHED_RT_ALLOC=1 BANISHED_AUTOQUIT=1
#   expect: [RtAlloc] ARMED (cutover...), and MemAlloc/MemFree GONE from both
#   the [RtUnresolved] list and the [RtResidue] census. Restore the DLL.
```

**The pass condition for E4-c is the residue census, not the boot.** Attempts 04 and 05 both
reach `GameMain` and return 1 through its own guard because Engine's four entry points are
null; that is unchanged by this lane and is not its business. What E4-c must show is those two
names leaving the unresolved list. If the boot gets *further* than attempt 05, that is a
bonus, not the criterion.

## 6. What arming means for the two gate-step-4 items

Asked for explicitly, and the answer differs sharply between them.

### §5.2 (Shutdown, ~MemoryAllocator, operator=, GetAllocationSize, GetAllocatedSize, Get)

**Arming turns this from hypothetical into load-bearing, and it splits into three tiers, not
one.** It should not be carried forward as a single line any more.

| | under arming | read |
|---|---|---|
| **`Get`** | called by `rtalloc::MemAlloc`/`MemFree` on **every allocation in the process** | **Goes from never-executed to the hottest function in the game.** The shadow drives `Allocate`/`Free` directly and has never once called `Get`. It is three instructions and it now also runs through lane H's `RT_DATA_SLOT` retry helper, so the risk is low — but "low risk" and "never executed" are different claims, and this is the one that must be exercised before arming rather than after. |
| **`Shutdown`, `~MemoryAllocator`** | run at process teardown, releasing every range | **Do not arm these into the open teardown crash.** There is an oracle-independent teardown crash firing ~4 in 10 on the stress route (lane N). Arming makes our `Shutdown` live at exactly the moment that crash happens, which would contaminate an investigation that is currently narrowing well. If arming lands before that closes, `BANISHED_RT_ALLOC` becomes a variable in it and lane N's bisection gets an extra dimension. |
| `operator=`, `GetAllocationSize`, `GetAllocatedSize` | read-only or unreached | Genuinely low priority. `operator=` (the 0x670 `memcpy`) has no caller I can find. |

**Priority verdict: §5.2 rises, but only two thirds of it.** `Get` should be exercised before
E4-b. The teardown pair should be sequenced *after* lane N's crash closes, or arming should be
declared a variable in that hunt. The accessors can stay where they are.

At cutover none of this is optional — every one of them becomes ours — which is an argument
for exercising them while the DLL is still there to compare against, not for deferring.

### §5.3 (the 5% `sim ms/iteration` gate, three runs per config)

**Rises, and becomes measurable for the first time.** Until now there was nothing to measure:
the port had never owned the heap. Armed with the DLL present, every allocation in the process
runs DLL-caller → detour → composed stub (CS + `RedzoneAlloc_TeardownOp` + ALLOCSTATS) → our
body, which is strictly more work than today's `allocator_lock` path — that path already pays
the CS and the diagnostics and then runs *the DLL's* body. So the honest expectation is a
regression of "our body vs theirs, plus one indirect", on the single hottest subsystem in the
process.

This is exactly the Wave 2 shape (`reference_one_perf_run_cannot_resolve_the_gate.md`), and it
needs the perf exe and three runs per config, not the `-O0` debug build every measurement in
this lane's family has used. **I would not arm by default without that measurement**, and it
should be taken on E4-b (DLL present, armed) where a clean A/B against `BANISHED_RT_ALLOC`
unset is available — at cutover there is no A/B, because there is no other allocator to
compare against.

## 7. Not done, deliberately

`BANISHED_RT_ALLOC` was not set in any run and is not default-on. Nothing here arms it; it
makes arming *possible* at cutover, which it was not before. The decision to arm remains the
wave lead's.
