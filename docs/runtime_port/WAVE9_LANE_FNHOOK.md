# Wave 9 — lane FNHOOK: `Object::ExternalFactory` / `External` resource loading

**Branch:** `wave9/lane-fnhook` · **Base:** `ba7bc60` (integration tip, attempt 17)

## Summary in one paragraph

The lane was sent to port four `ExternalFactory`/`External` rows and to deal with the
`[FNHook] could not resolve …` lines every cutover attempt prints. It ported none of the
four, and the reason is worth more than the ports would have been: **every place in the
tree that patches or reads the shipped DLL body was resolving its target through the
port-registry door, and six of the thirteen were pointed at our own reimplementation.**
The rule ("anything that patches the DLL must resolve through the `*Raw*` doors") was
already written in `standalone/resolve.h` and repeated in `standalone/runtime_detour.h`;
all thirteen sites broke it, because the installer took an *already-resolved* address and
left the resolve to the caller. That is
fixed by construction — the installer now takes a Ghidra address and resolves it raw
itself — and a new checker, `tools/detour_target_audit.py`, fails closed on the shape.
The measured effect: `BANISHED_READ_GUARD`, the loader-drift diagnostic, had been
**completely inert** since the `CompressedStream` port landed; after the fix its wrapper
served **104,069,297 calls** in one run. Separately, the resource-loader serialize lock
was extracted from the byte patch it was welded to, so a future ported `FindOrLoad` can
take the same lock in its own body — the thing lane SYS2 said was the single most
important item for the integrator.

---

## 1. THE FINDING: thirteen patch/shipped-body targets, six aimed at our own code

### 1.1 What the rule says, and what the code did

`standalone/resolve.h`, above the `*Raw*` declarations:

> Anything that patches, detours, trampolines to, or diff-tests against the ORIGINAL DLL
> code must resolve through the *Raw* variants below, or it will happily patch our own
> reimplementation once the port registers.

`standalone/runtime_detour.h` repeats it and even claims the installers do it for you:

> Resolve the target through `ResolveRuntimeRaw` / `ResolveExportRaw`, never `ResolveAddr`
> / `ResolveExport` … `InstallRuntimeDetour*` below already do this for you.

They did not. `RuntimeDetourInstall` / `DetourRuntimeFunc` took a `uintptr_t addr` that the
caller had already resolved, and **all nine of its call sites resolved it with
`ResolveAddr(Module::Runtime, …)`** — the door that consults the port registry FIRST
(`reference_resolveaddr_consults_port_registry_first.md`). Two more sites in
`standalone/allocator_lock.cpp` hand-roll the same patch, one more in
`filename_runtime_hook.cpp` hand-rolls the `MemFreeStatic` watch, and one reads a shipped
callee pointer (`SetWindow`) — thirteen in all, and every one of them did the same thing.

`ResolveAddr` is not "the DLL". For a registered address it returns **our** function, and
the installer then tries to overwrite our GCC prologue with a jump, checks it against the
MSVC bytes it expects, fails, and prints one line into a 72 KB stderr log:

```
[FNHook] Filename::~Filename prologue at 00007ff6dea82508 mismatch — refusing to patch
                                        ^^^^^^^^^^^^^^^^ the EXE's base, not the DLL's
```

The address in that line is the tell and nobody had read it. The DLL was at
`0x7ffcafa…` in the same run.

### 1.2 Which ones were live, measured

Baseline run (`ab_fnhook/evidence/baseline_error.log`, the tree at `ba7bc60`,
`BANISHED_FNHOOK=1 BANISHED_READ_GUARD=1`):

| detour target | Ghidra | registered port? | baseline | after |
|---|---|---|---|---|
| `Filename::~Filename` | `0x180006980` | yes (`filename.cpp:471`) | **mismatch, dead** | detoured |
| `Filename::operator=(String&)` | `0x180006770` | yes (`filename.cpp:472`) | **mismatch, dead** | detoured |
| `Filename(const String&)` | `0x1800067d0` | yes (`filename.cpp:470`) | **mismatch, dead** | detoured |
| `CompressedStream::Read` | `0x1800aa780` | yes (`compressed_stream_read.cpp:449`) | **mismatch, dead** | detoured |
| `CompressedStream::SetWindow` (callee ptr) | `0x1800a9aa0` | yes (`…:453`) | **our body** | shipped body |
| `Filename::operator=(Filename&)` | `0x180007d40` | no | detoured | detoured |
| `ExternalFactory::FindOrLoad` | `0x1800bf320` | no | detoured | detoured |
| `External::Load` | `0x1800bc620` | no | detoured | detoured |
| `Video::Device::InitializeInterfaces` | `0x1800f79f0` | no | detoured | detoured |
| `InternalFactory::PersistObject` | `0x1800bff10` | no | (probe, off) | (probe, off) |
| `MemoryAllocator::Allocate` (allocator_lock) | `0x1800c6e60` | no | patched | patched |
| `MemoryAllocator::Free` (allocator_lock) | `0x1800c6c00` | no | patched | patched |
| `MemoryAllocator::MemFreeStatic` (data-dir watch) | `0x1800c7ad0` | yes (`allocator.cpp:1459`) | **would mismatch, dead** | patches shipped body |

**Six broken, seven correct — and the seven were correct only because nobody had ported
them yet.** Five of the six are proved broken-then-fixed by the runs below; the sixth,
the `MemFreeStatic` data-dir watch, sits behind a second flag
(`BANISHED_FNHOOK_DATADIR`) that this lane did not exercise, so it is called by
inspection and by the shared cause, not by measurement. The two `allocator_lock` rows are the interesting ones: they work today and
are scheduled to break, because lane E's allocator port lands on exactly those two
addresses. The same run prints `[RtResolveEarly] 0 of 2 are registered now` about them,
which is the countdown, not the all-clear.

### 1.3 The unported rows are the negative control

`FindOrLoad`, `External::Load` and `InitializeInterfaces` are not ported, so both doors
return the same address and both builds must detour them identically. They do — same DLL
address, same patch length, in both logs. A difference there would have meant the change
did something other than what it says.

### 1.4 Why no existing checker caught it

`tools/port_bypass_audit.py` classifies a `ResolveAddr(Module::Runtime, X)` whose `X` is a
registered port as **`ADDR-REGISTERED`**, i.e. correct — and for an ordinary caller it *is*
correct; that is the whole point of the seam. **A detour installer is the one caller for
which reaching our port is the defect**, and the audit's model has no way to tell the two
apart. It reported `0 bypasses` on a tree with six mis-aimed patch targets, and it was not
wrong to: it was answering a different question. `rawdoor_audit.py` looks at the opposite
direction (raw doors *called* on a live path) and is equally blind to this.

> **A checker that asks "does this caller reach our port?" cannot audit the one caller
> that must not.** The same fact — `ResolveAddr` returning our body — is the success
> condition for 290 sites and the defect for thirteen.

### 1.5 The fix, and why it is structural rather than thirteen edits

`RuntimeDetourInstall` / `DetourRuntimeFunc` now take the **Ghidra address** and call
`ResolveRuntimeRaw` themselves. The parameter type is what let every caller get it wrong,
so the resolve moved to the side of the boundary where no caller can. The two hand-rolled
patches in `allocator_lock.cpp` were switched to `ResolveRuntimeRaw` in place (behaviour
identical today, since neither address is registered).

`tools/detour_target_audit.py` is new and fails closed. It checks two shapes:

1. the target argument of `RuntimeDetourInstall` / `DetourRuntimeFunc` / `InstallDetour` /
   `InstallDetourRSPSafe` / `InstallVtableDetour`, inline or through a local assigned
   earlier in the same function;
2. a pointer handed to `VirtualProtect(…, PAGE_EXECUTE_*)` — the hand-rolled shape, which
   is how it found the two `allocator_lock` sites that the grep for installer names
   missed.

**Negative control:** a throwaway file with the three original shapes (inline resolve,
local-assigned resolve, correct raw form) was scanned before deletion; the checker flagged
exactly the two wrong ones and passed the right one. Tree state now:

```
detour_target_audit: clean -- every detour target resolves RAW
```

---

## 2. Effectiveness control: the detours were installed — did they RUN?

"`[FNHook] detoured X`" says the bytes were written. It does not say the wrapper was
entered, and the wave has paid twice for that distinction already. Each wrapper now
increments a counter and an `atexit` census prints them, so a live-but-unreached detour is
visible instead of inferred.

One run, `BANISHED_AUTOSTART=1 BANISHED_FNHOOK=1 BANISHED_READ_GUARD=1`, driven through a
full world build to the pause menu and a clean self-quit
(`ab_fnhook/evidence2/instr_error.log`):

```
[FNHook] census: ~Filename=0 operator=(String)=0 operator=(Filename)=0 Filename(String)=184642
[ResLoad] census: FindOrLoad=22591 External::Load=5686
[ReadGuard] census: Read=104069297
```

Read that carefully, because three of the numbers are zero and it is not a failure:

* **`Read=104,069,297`.** `BANISHED_READ_GUARD` — the loader-drift diagnostic, a full
  reimplementation of `CompressedStream::Read` with a zero-progress guard — had been
  installing *nothing* since the `CompressedStream` port landed. It now serves a hundred
  million DLL-internal calls per run, and the world still builds and the process still
  exits cleanly with an empty crash log.
* **`Filename(String)=184,642`** — newly live, and busy.
* **`~Filename=0` and `operator=(String)=0`** — newly live and **never reached**. Not a
  contradiction with 184k constructions: our own callers go to the registered ports, and
  the DLL's internal code *inlines* the release (`dec [rax-4]; if 0 → MemFreeStatic`)
  rather than calling `~Filename`. That inlining is stated in this file's own 2026-06
  header comment; the census is the first thing to confirm it.
* **`operator=(Filename)=0`** — this one was *already* live in the baseline and also reads
  zero. Nobody calls it. So the counters immediately separate "the fix made it live" from
  "it is live and has no callers", which is the whole reason to count inside the function.

**Regression control, same exe, no diagnostic flags** (`evidence2/default_error.log`):

```
[FNHook] census: ~Filename=0 operator=(String)=0 operator=(Filename)=0 Filename(String)=0
[ResLoad] census: FindOrLoad=22585 External::Load=5686
[ReadGuard] census: Read=0
```

Every changed target sits behind an opt-in env gate, so a default run is unchanged:
`External::Load` identical at 5,686, `FindOrLoad` 22,585 vs 22,591 (ordinary run-to-run
variance in a world build), same world, same clean exit, empty crash log. **The five
newly-live detours cost the default path nothing because they are not on it.**

### Lock discipline

Both A/B pairs ran inside one `tools/game_dir_lock.sh acquire lane-FNHOOK 900` window each,
logs copied out before `release`. The acceptance gate was **not** the on-disk md5 — it was
an identity string this lane's build is the only one that emits, read out of the run's own
log:

```
[FNHook] installed (abort=0 maxEvents=80 target-resolution=RAW) …
[ResLoad] serialized: … (recursive CS, target-resolution=RAW)
```

The baseline log does not contain `target-resolution=RAW`; the fixed logs do. Six other
lanes were contending for the directory throughout (the second window waited 30 s behind
lane CTX, the third over ten minutes behind lanes INPUT, SAVE2 and GFX3).

---

## 3. The serialize lock, moved out of the byte patch

Lane SYS2's §4.3 is right and this lane confirmed it end to end. The menu-load race fix
(`FindOrLoad` and `External::Load` under one recursive `CRITICAL_SECTION`; ~33 % crash rate
→ 0 of 24 runs) is implemented **as a byte patch on the DLL's own bodies**. With no DLL
there is nothing to patch, so at cutover the fix does not exist:

```
[FNHook] could not resolve ExternalFactory::FindOrLoad
[FNHook] could not resolve External::Load
[ResLoad] serialized: FindOrLoad=0 External::Load=0 (recursive CS)
```

Two threads are measured in that phase, so this is a live correctness hole independent of
any port. What this lane did about it:

* the lock moved to `standalone/res_load_lock.{h,cpp}` with a `<windows.h>`-free header so
  `runtime/src/` may include it (PLAN §4.1 decision 1);
* it **initializes on first use**, not at an install site that may never run. The old code
  called `InitializeCriticalSection` inside `InstallFindOrLoadWatch()`; a ported body
  taking that CS at cutover would have been entering an uninitialised critical section —
  a random hang, i.e. the exact failure the lock exists to prevent;
* `InitOnceExecuteOnce` rather than a function-local static, because the first caller may
  be a Runtime-created loader thread and MinGW's thread-safe-statics guard is CRT
  machinery we are told to keep out of detoured code (PLAN §4.3 risk 5);
* the kill switch `BANISHED_NO_FOL_LOCK` is read **once** and cached — read per call, a
  mid-run `_putenv_s` could turn the lock off between an acquire and its release and leak
  it;
* the detour wrappers now call `ResLoadLockAcquire/Release` instead of touching the CS,
  and the header states in as many words that **a ported `FindOrLoad`/`External::Load`
  must take `ResLoadGuard` in its own body.** Recursion makes port-and-detour coexistence
  safe while both binaries are loaded.

**This does not yet close the hole**, and the honest statement is that it cannot be closed
without the port: with `FindOrLoad` unported there is no body of ours to put the guard in.
What is closed is the trap — the lock is now reachable, initialised and documented, so the
lane that ports `FindOrLoad` adds one line rather than rediscovering six sessions of
crash archaeology.

---

## 4. The four rows: dispositions, with the blockers re-checked

### 4.1 `?SaveText@External@Object@@` `0x1800bb9d0` — **BLOCKER HOLDS** (re-checked independently)

WAVE9.md row 26 and lane SYS2 §4.4 both block it on `IO::TextFileStream`. Re-checked from
scratch rather than inherited: `TextFileStream` appears in **exactly two places in the
whole tree, both comments** (`runtime/src/object/external.cpp:46`,
`runtime/types/object/External.h:85`). No port, no type header, no registration. `SaveText`
drives the `+0x28 Serialize(IO::Stream&)` slot with one. **Still blocked; needs a
`TextFileStream` lane first.** Three-for-three on stale blockers does not make a fourth
one stale.

### 4.2 `?LoadResources@UpdateLoop@Engine@@` `0x1800dd7d0` — blocked on 4.3, unchanged

Calls `FindOrLoad` three times. Cannot precede it.

### 4.3 `?FindOrLoad@ExternalFactory@Object@@` `0x1800bf320` — NOT PORTED, and the blocker has PARTLY DISSOLVED

274 instructions. Its call graph, with today's port status:

| callee | what | ported? |
|---|---|---|
| `0x1800bd360` | `ExpandRelativePath`, **834 instructions** | no |
| `0x1800be0e0` | the cache lookup (ends in `FUN_18002dd70`, the 0x18 `Set::Find`) | no |
| `0x1800be570` | `ExternalFactory::Add`, 123 instructions | no |
| `0x1800bc620` | `External::Load`, 210 instructions | no |
| `0x1800c64f0` | a `Filename` helper | no |
| `0x1800c6610`, `0x1800c6740`, `0x1800c6880`, `0x1800c6970` | `Filename` helpers, `AdjustDelimiters` | **yes** |
| `0x180007bd0` | `FileSystem` helper | **yes** |
| `0x1800c9440`, `0x1800c92d0` | `String` ops | **yes** |
| `0x1800c7ad0` (×12) | `MemFree` | **yes** |

**9 of 13 callees are already ours.** The remaining cost is `ExpandRelativePath` (the big
one), `Add`, `External::Load`, and the lookup helper.

**And the oracle argument has changed since Wave 3 wrote it.** Lane C's stated blocker was:

> Porting them means porting `Collection::Set` insert/erase/rebalance **for the 0x18-wide
> element specifically** (the node trap forbids reusing another width's body) … running
> both implementations would mutate the one shared tree twice.

The first half is now much cheaper than it reads. `ExternalFactory::Add` reaches the insert
through `0x18002ca00` (45 instructions — a wrapper that builds a temp `Filename` and copies
the 0x10 payload) → `0x18002ce60` (73 instructions, the insert) → `0x18002d370` (72, the
retrace) → `0x18002d860` / `0x18002d990` (the two rotations). Reading `0x18002d860`:

```
18002d86c: MOV RCX,qword ptr [RDX + 0x18]   ; parent
18002d891: MOV qword ptr [R8 + 0x18],RAX    ; parent
18002d895: MOV RAX,qword ptr [R8 + 0x20]    ; left
18002d8a6: MOV qword ptr [R8 + 0x20],RDX    ; left
18002d88d: … [RDX + 0x28]                   ; right
```

parent `+0x18`, left `+0x20`, right `+0x28` — **exactly the `ResourceSet::Node`
static_asserts already in `runtime/types/object/External.h:192-194`**, and structurally
identical to the four bodies lane VID2 ported in `runtime/src/collection/set_avl.cpp` for
the 0x10 element (parent `+0x10`, left `+0x18`, right `+0x20`). The node trap is real about
*addresses*; it is not a claim that the algorithm differs. So:

> **The 0x18 instantiation is lane VID2's already-verified algorithm at a different node
> offset, and lane VID2's lockstep probe is a REAL ORACLE for it** — drive our
> parameterised bodies and the shipped `0x18002d370` / `0x18002d860` / `0x18002d990` over
> the same synthetic node array, comparing after every step. That is not "run ours and
> theirs on the one shared tree"; it is the shape `set_avl_probe.cpp` already implements.

What is still genuinely unsolved is the *stateful* half: `FindOrLoad` itself mutates the
one live factory tree, so it cannot be run twice. The recommended decomposition, handed on
rather than attempted here:

1. parameterise `set_avl.cpp` on the node offsets, register the 0x18 triple, and verify it
   with the existing probe against the shipped bodies. **This has an oracle today.**
2. port `ExpandRelativePath` (834 instructions, pure path arithmetic on `Filename`/`String`
   whose helpers are already ours) — **a pure function, so an ordinary `RT_DIFF` oracle
   applies.**
3. port `Add` and the lookup helper on top of (1).
4. port `External::Load` and `FindOrLoad` last, verified *structurally* — after each call,
   walk the live tree and assert AVL invariants plus the key set, which is what Wave 3
   asked for ("a shadow-tree checker that walks both trees for structural equality after
   every mutation — which is a real oracle").
5. **`ResLoadGuard` in the body of 4.** Non-negotiable; see §3.

Steps 1 and 2 are ordinary verified porting work and neither existed as a stated option
before this lane read the call graph. **The row is still NO-GO for a single lane-day; it is
no longer "no oracle possible".**

### 4.4 `?Clean@ExternalFactory@Object@@` `0x1800be310` — NOT PORTED, same argument

~140 instructions, drives the same tree through erase instead of insert, and frees every
evicted `External` through vtable slot 0. Same decomposition, same order: it follows (1).

### 4.5 The three by-address doors the brief listed

`0x180007d40` (`:417`), `0x1800bf320` (`:585`), `0x1800bc620` (`:600`) were on
`addr_door_audit.py`'s unregistered list. **Registering them would have been wrong** —
they are byte-patch targets that must be null at cutover, and registering `0x1800bf320`
is precisely the trap lane SYS2 warned about. They are now `ResolveRuntimeRaw` doors,
which is what they always were semantically, and the audit's unregistered count fell
42 → 39 with `filename_runtime_hook.cpp` gone from its concentration list (measured by
re-adding the converted sites in a throwaway file, not quoted from an older run). That is a
correction to the instrument's view, not a port.

---

## 5. Verification summary

| check | result |
|---|---|
| `tools/detour_target_audit.py` (new) | clean; fires on all three original shapes in a negative control |
| `tools/port_bypass_audit.py` | 0 bypasses on both doors (unchanged) |
| `tools/rawdoor_audit.py` | 0 `UNGUARDED-CALL` (unchanged) |
| `tools/addr_door_audit.py` | UNREGISTERED 42 → 39 (measured both ways) |
| A/B run, `FNHOOK`+`READ_GUARD` | 5 detours dead → live; identity line `target-resolution=RAW` read from the run's own log |
| effectiveness census | `Read=104,069,297`, `Filename(String)=184,642`, three structural zeroes explained |
| default-flags control | byte-for-byte same behaviour, `External::Load=5686` both runs, empty crash log |
| world build | reached in every run (`BuildTerrain EXIT`, 154 `startTown` lines), clean self-quit |

## 6. Files touched

```
standalone/runtime_detour.h          RuntimeDetourInstall now takes a GHIDRA address
standalone/filename_runtime_hook.cpp raw resolves, the lock extraction, the exit census
standalone/allocator_lock.cpp        two hand-rolled patch targets -> ResolveRuntimeRaw
standalone/persistobject_probe.cpp   pass the Ghidra address
standalone/video_interface.cpp       pass the Ghidra address
standalone/res_load_lock.{h,cpp}     NEW — the serialize lock, first-use init, in-body API
tools/detour_target_audit.py         NEW — fails closed on a mis-aimed detour target
tools/lane_fnhook_run{,2}.sh         the A/B harnesses used above
docs/runtime_port/WAVE9_LANE_FNHOOK.md
```

## 7. Handed on, in priority order

1. **`allocator_lock.cpp` is now correct but lane E must still be told**: when the
   allocator port registers `0x1800c6e60` / `0x1800c6c00`, the lock patches the shipped
   bodies and the port serves our callers — decide deliberately whether both are wanted,
   because that is two lock layers on one heap.
2. **The `FindOrLoad` decomposition in §4.3**, whose first two steps have oracles today.
3. **The serialization hole is still open at cutover** and closes only with the port.
4. **`port_bypass_audit.py`'s `ADDR-REGISTERED` verdict is not a pass for every caller**
   (§1.4). Worth a sentence in that file next time somebody edits it.
