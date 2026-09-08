# Wave 9 lane RES — the resource-loading spine

Branch `wave9/lane-res`, cut from `fd30180`.

**Assignment:** `?FindOrLoad@ExternalFactory@Object@@`, `?Clean@ExternalFactory@Object@@`,
`?LoadResources@UpdateLoop@Engine@@`.

**Delivered:** `Clean`, plus the twelve unexported `Collection::Set<ResourceEntry>` bodies
underneath it, with a lockstep oracle that PASSES.
**Declined:** `FindOrLoad` and `LoadResources`, with the closure counted — see §5.

---

## 0. The brief's central relayed claim was FALSE, and checking it was the first thing done

The brief said, and asked me to verify rather than accept:

> Lane COLL has since LANDED that whole family (`runtime/src/collection/avl_trees.{cpp,h}`)
> … so FNHOOK's "no oracle possible" is no longer right for the first two steps.

**The conclusion is right. The premise is wrong.** Lane COLL landed three *other*
instantiations — a 0x50-byte node (`Set<Pair<u32,TypeRecord>>`), and two 0x30-byte nodes
(`Map<String,String>`, `Set<Pair<Filename,u64>>`). Grepped across `runtime/`, `code/`,
`engine/`, `dx11/` and `standalone/`, the four addresses FNHOOK named — `0x18002ce60`,
`0x18002d370`, `0x18002d860`, `0x18002d990` — appeared **nowhere in the tree**, in any form.

What *had* landed, and what nobody connected to this row, is **lane INPUT's
`runtime/src/collection/set_avl_e24.{h,cpp}`**: the same 0x18-byte-element node header
(parent +0x18 / left +0x20 / right +0x28 / balance +0x30 / height +0x32, sizeof 0x38) at a
completely different set of addresses (`0x180023060` / `0x180023170` / `0x180023570` /
`0x1800236a0`), emitted by MSVC for `Input::KeyMap`'s tree. So the *layout* was already
pinned and `static_assert`ed by another lane; the *bodies* were not.

> **Two lanes independently pinned the same node header for two different instantiations,
> and neither knew.** The lesson is not that the brief was careless — it is that
> "instantiation" and "layout" are different axes, and the tree's file names are organised
> by the first while the reuse lives in the second. `set_avl_e24.h`'s own banner explains
> the E16-vs-E24 split; nothing told it a third and fourth E24 tree existed.

---

## 1. What landed

`runtime/src/object/external_set.{h,cpp}` — twelve bodies, all registered by ADDRESS
(none is exported; checked against the retail export directory and against
`Runtime-x64-profile.dll`'s 5,182 names), plus `Clean` registered by BOTH its mangled name
and its address.

| retail | what | evidence read |
|---|---|---|
| `0x18002d860` | rotate left | disassembly |
| `0x18002d990` | rotate right | disassembly |
| `0x18002d370` | AVL retrace | disassembly |
| `0x18002e6e0` | node element ctor (`Filename` copy + two raw qwords) | disassembly |
| `0x18002e740` | node alloc (0x38, align 8, null-safe) | disassembly |
| `0x18002ce60` | insert core | disassembly |
| `0x18002ca00` | insert (builds the 0x18 element on its stack) | disassembly |
| `0x18002ee40` | release element + free node | disassembly |
| `0x18002cf60` | erase (3 cases, PREDECESSOR splice) | disassembly |
| `0x18002dd70` | find | disassembly |
| `0x18002e3d0` | `Array<Filename>::DestroyRange` | disassembly |
| `0x18002dff0` | `Array<Filename>::Resize` | disassembly |
| `0x1800be310` | `ExternalFactory::Clean` | disassembly |

`runtime/src/object/external_set_probe.cpp` — the lockstep oracle.
`runtime/src/object/rt_object_port.cpp` — one registration hook, gated
`BANISHED_RT_EXTERNAL` × `BANISHED_RT_RESSET`.

### The byte comparison against lane INPUT's E24 family — done, not assumed

Three bodies are the same algorithm at different addresses. avl_trees.cpp's precedent is
that this must be *checked*, because the family has already produced four near-misses that
passed green runs and differed only in an alignment immediate:

* rotate left `0x18002d860` vs `0x180023570` — **identical instruction for instruction.**
* rotate right `0x18002d990` vs `0x1800236a0` — **identical.**
* retrace `0x18002d370` vs `0x180023170` — **NOT identical.** First difference at +0x23:
  `0x18002d370` carries the node in R8 (`MOV R8,RAX`), `0x180023170` in RDX. Nothing else
  differs and nothing writes `*slot` between the two points, so they are semantically one —
  the same no-difference lane COLL recorded for `0x18000adc0` vs `0x18002a310`.

**They are transcribed and registered separately anyway.** `RegisterRuntimeAddr` is the door
a by-address caller jumps through; a body registered at an address it was never read from
carries no evidence of its own, and the next lane would have to re-derive which
instantiation it came from.

---

## 2. Verification — a lockstep probe that PASSES, and a live `Clean` that did real work

`BANISHED_RT_RESSET_PROBE=1`. Two **private** `Set`s, the same script, ours through the
ported bodies and theirs through the shipped ones reached with `ResolveRuntimeRaw` (RAW,
never `ResolveAddr` — that consults the port registry first and would have compared ours to
ours). Compared after **every step** on shape, balance, height, parent linkage and element
payload.

A live diff is impossible here and the reason is specific, not general: retail's insert
**returns early on an equal key** (`0x18002cf36`), so an ours-then-theirs replay at a real
call site inserts the same key twice and the second call does nothing — vacuously green.
That is the trap lane D recorded on `KeyMap::RegisterInput`, and it is why Wave 3 wrote this
family off as "no oracle possible". It is not; the subject just must not be the live tree.

Run of 2026-09-08, exe identity line `[ResSet] registered 13 doors (… probe=ARMED)`, read
from that run's own log (`docs/runtime_port/evidence/lane_res/probe_on_autostart.txt`),
`BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120`:

```
[ResSet] LOCKSTEP PROBE -- 77 steps, 0 shape/payload mismatches, 0 parent-linkage failures
[ResSet]   find: hits=49 misses=1 disagreements=0
[ResSet]   effectiveness (branches taken INSIDE the ported bodies): root=1 L=35 R=19 eq=2 |
           cmp </=/> = 529/138/253 | nullKey search/node = 8/2 | rot L/R = 26/39
           pre L/R = 7/11 | retraceIters=365 | erase 0/1/2 = 10/2/8 predSplice=5 rootSplice=0
[ResSet]   negative controls: 4 of 4 FIRED
[ResSet]   VERDICT: PASS -- 77 lockstep steps, every watched branch taken, every negative
           control fired.
[ResSet] Clean: visited=5683 evicted=913 bitCleared=5683 arrayGrow=7 cleanBit=5 generation=1
```

`rc=0`, zero-byte crash log, world built, clean self-quit.

**`Clean` is not a `calls=0` port.** It ran on the live factory, walked 5,683 nodes,
**destroyed 913 `External` objects and erased 913 nodes out of the live tree through our
`Erase`**, and the process went on to a clean exit. That is real coverage of `Erase`,
`Retrace`, both rotations, `Find`, `ArrayResize` and `ArrayDestroyRange` on the real
subject, on top of the synthetic lockstep.

### The probe reported itself UNDERPOWERED twice before it passed, and that is the point

Rule 6 says a probe needs an effectiveness control counted INSIDE the subject, and that a
control which misses is reported rather than explained away. Both happened:

| run | verdict | what it said |
|---|---|---|
| 1 | **NOT A PASS** | `*** UNDERPOWERED -- 2 branch(es) … NEVER TAKEN: nullNodeKey erase1Child` |
| 2 | **NOT A PASS** | `*** UNDERPOWERED -- 1 branch(es) … NEVER TAKEN: nullNodeKey` |
| 3 | **PASS** | all 17 watched branches taken, 4 of 4 negative controls fired |

Two script defects, each fixed by understanding *why* the branch was unreachable:

* **`erase1Child`** (erase case 2, right child only) cannot be guaranteed by a fixed erase
  list — whether any node has exactly a right child depends on every rotation that ran
  before it. It is now **hunted**: walk our tree for such a node, take its KEY, and erase
  that key from both sides. Picking from our tree and re-finding by key in theirs is what
  keeps both sides on the same subject; handing theirs a pointer into our tree would be a
  wild write, and picking independently would silently compare two different operations.
* **`nullNodeKey`** (the comparator arm where the NODE's key is null) only fires when a
  LATER insert descends past the null-key node, which sits at the extreme left of the tree.
  With printable keys only, nothing ever reached it. Two keys beginning with `!` and `#`
  fixed it: `_stricmp` folds case so the lowest folded first byte among the other keys is
  `a`, and punctuation is not folded.

### A finding the probe produced about the port, not about itself

Run 1 also reported `find: hits=31 misses=19 disagreements=0`. The 18 in-script misses were
**exactly the 18 keys containing `/`** — measured with a temporary per-key diagnostic rather
than guessed at.

> **`Find` at `0x18002dd70` requires a DELIMITER-ADJUSTED key. A lookup with a raw `/` path
> silently misses, and it misses IDENTICALLY in the shipped body.**

`Filename`'s copy constructor adjusts delimiters on the way into the node; nothing adjusts
the search key. Retail guarantees the adjustment at every call site — `ExternalFactory`'s
own lookup at `0x1800be0e0` calls `AdjustDelimiters` **four times** before it descends. The
probe was asking a question retail never asks, so it now calls `AdjustDelimiters` on every
search key, exactly as retail's callers do, and finds 49 of 49.

`findDisagree = 0` throughout: ours and theirs missed the same keys for the same reason.
**This is a property of the port's contract that anyone writing a caller must know**, and it
would have been an invisible trap for the lane that eventually ports `FindOrLoad`.

### A/B control

Same exe, `BANISHED_RT_RESSET=0` (`control_resset_off.txt`): `rc=0`, zero-byte crash log,
identical teardown tail, `[RtPort] Object: 26 exports registered` against 39 with the port
on — the 13 are exactly this lane's. No `[ResSet]` line appears, which is the negative half
of the identity check.

---

## 3. The door set-diff — closed 14, opened 0

Rule 2: a lane that closes 3 doors and opens 4 has gone backwards, so the count is a
**set-diff**, not a before/after against an older tree.

The worktree this lane ran in carried **other lanes' uncommitted work** (see §6), so
absolute totals are not attributable. The measurement below is therefore *my three files in
versus out of the same tree*, taken minutes apart:

| instrument | without lane RES | with lane RES | delta |
|---|---:|---:|---|
| `addr_door_audit` by-address call sites | 301 | 304 | +3 |
| `addr_door_audit` **distinct addresses** | 204 | 204 | **0 — no new address door** |
| `addr_door_audit` registered addresses | 1190 | 1203 | **+13** |
| `addr_door_audit` **UNREGISTERED** | 19 | 19 | **0** |
| `unsatisfied_export_scan` names with no port and no mint | 126 | 125 | **−1 (`?Clean@ExternalFactory@Object@@`)** |
| `port_bypass_audit` bypasses, both doors | 0 | 0 | 0 |
| `port_bypass_audit` ADDR-REGISTERED | 64 | 64 | 0 |
| `port_bypass_audit` NO-EXPORT | 165 | 169 | +4 |
| `port_bypass_audit` UNPORTED | 96 | 98 | +2 |
| `rawdoor_audit` UNGUARDED-CALL | 0 | 0 | **0** |
| `detour_target_audit` | clean | clean | clean |

`runtime_census.py` after: **4632 total / 1013 called / 782 ported / 77.2% of called
covered**; Object 191 / 50 / 30 / 60.0%.

**The +6 that are not regressions, named rather than waved past:**

* `NO-EXPORT +4` — `0x180009c30` (the two-parameter node free, registered by
  `allocator.cpp`) and the probe's **three** `ResolveRuntimeRaw` sites. The probe's three
  are oracle-only and die correctly at cutover: with no DLL they resolve null and the probe
  prints `SKIP … NO COMPARISON WAS MADE` rather than running a comparison against nothing.
* `UNPORTED +2` — `external_set.cpp:120` and `:129`, the `MemAlloc` (`0x1800c7a50`) and
  `MemFree` (`0x1800c7ad0`) doors. They read UNPORTED because `allocator.cpp`'s
  registrations sit behind `BANISHED_RT_ALLOC`, which the source-scanner sees as off. This
  is the **house-standard allocator door**, byte-identical in intent to the ones in
  `avl_trees.cpp`, `set_avl.cpp` and `set_avl_e24.cpp`, and WAVE9.md row E records the
  allocator as READY TO ARM. It is not a new hole; it is the same hole four other files
  already stand in.

Net for the cutover gate: **one exported name and thirteen address doors closed, zero
opened, `distinct addresses` unchanged at 204.**

---

## 4. `ResLoadGuard` — the constraint I was given, and why it is NOT satisfied

The brief was explicit: `FindOrLoad`'s ported body must take `ResLoadGuard` (`g_resCS`,
recursive), because lane FNHOOK's serialize-lock fix is implemented as a byte patch on the
DLL's own bodies and therefore **does not exist at cutover**.

**I did not port `FindOrLoad`, so I did not put the guard in, and the hole lane FNHOOK
described is still open exactly as FNHOOK left it.** Nothing in this lane changes that, and
nothing in this lane pretends to. Said plainly because the constraint was stated as
non-negotiable and a reader skimming the deliverables could otherwise assume it was met.

Two things worth handing on, though:

1. **The lock is not needed by anything this lane landed.** `Clean` runs at world teardown
   on the main thread, and the twelve Set bodies are reached today only from `Clean`. The
   race FNHOOK measured is between `FindOrLoad` and `External::Load` during the menu load.
2. **When `FindOrLoad` does land, `Clean` will need the guard too**, and that is new
   information: `Clean` erases from the same tree `FindOrLoad` inserts into. It is
   single-threaded today only because the loader thread does not call it. A future lane that
   guards `FindOrLoad` and leaves `Clean` unguarded has re-opened the race on the erase side.
   Noted here rather than pre-emptively locking a body that has no contention today, which
   would be a lock with no measurement behind it.

---

## 5. What I DECLINED, and the blocker I measured myself

### `?FindOrLoad@ExternalFactory@Object@@` (`0x1800bf320`, 274 instructions) — DECLINED

The body itself is not the problem; it decompiles cleanly and its structure is understood
end to end (cache hit → `Add` if `_loadEnabled`; cache miss → build
`<dataDir><dir><filename>_<identifier>.crs`, `External::Load` it, re-point its `_path`, and
`Add`). **The blocker is two of its callees, and I re-derived it rather than inheriting it:**

| callee | size | what it actually needs |
|---|---:|---|
| `0x1800bd360` `ExpandRelativePath` | **834 instructions** | `ExternalFactory::_opaque30` / `_opaque38` — the *path stack*, which `External.h` records as **NOT PINNED**; `IO::FileSystem::ExistsInPackfile`; `System::WideString`; `GetFileAttributesW` |
| `0x1800bc620` `External::Load` | 210 | opens with `CompressedStream::CompressedStream` **and** `BinaryFileStream::BinaryFileStream`, then drives `IO::Serializer` through the shared encoding vtable at `0x1801554a8` |

**Two of these three sub-blockers have shifted since they were last written down, and one has
not** — checked, because three "stale blockers" dissolved on re-checking this wave:

* `??0CompressedStream@IO@@QEAA@XZ` **is now registered** (`compressedstream.cpp:955`).
  WAVE9.md lane W's statement that it is "unported and unminted" is out of date.
* `?Commit@`, `?Create@`, `?CompressWindow@CompressedStream@IO@@` and
  `?CloseFile@FileSystem@IO@@` are **still on `unsatisfied_export_scan`'s list**, and
  `External::Load` calls all four.
* The encoding vtable `0x1801554a8` still **has no exported name and no mint row**, which is
  lane W's finding and the one that has not moved.

**The reason for declining is not the size — it is the door arithmetic.** Porting
`FindOrLoad` today would REMOVE one name from `unsatisfied_export_scan` while ADDING two
addresses (`0x1800bd360`, `0x1800bc620`) to the unregistered list, and the DLL-less boot
would die one frame deeper for the same reason it dies now. That is precisely the "closes 3,
opens 4" trade rule 2 exists to stop, and it would have made the census move in the right
direction while the boot got no better.

**The order for whoever takes it**, now that the tree half is done:

1. `0x1800c64f0` `Filename::GetPathExtension` — ~40 instructions, pure, and its
   `String::SubString` callee (`0x1800c9230`) is already ours. An ordinary `RT_DIFF` oracle
   applies. *Not done here because it has no caller in our tree today and would be a
   `calls=0` port* (`reference_uncalled_port_is_not_verified.md`).
2. `0x1800bd360` `ExpandRelativePath` — the big one. Its 834 instructions are ~90 % refcount
   release boilerplate around six `String` concatenations and two `ExistsInPackfile` probes,
   but it needs `_opaque30`/`_opaque38` pinned first. **`GetPathStackDirectory` and
   `GetPathStackFilename` are the two functions that read them** and are the place to start.
3. `0x1800be0e0` — the factory's internal lookup, ~100 instructions, closure now complete
   (its `0x18002dd70` is registered by this lane). It appends `":resource"` when the path
   carries no identifier, and calls `AdjustDelimiters` **four times**.
4. `0x1800be570` `Add`, 123 instructions; needs `0x1800bb090` (an ~0x18-byte local whose
   type is not yet identified) and one virtual call through `External` vtable slot 5.
5. `0x1800bc620` `External::Load` — blocked on the `IO::Encoding` vtable, above.
6. `FindOrLoad` itself, **with `ResLoadGuard` in the body**.

### `?LoadResources@UpdateLoop@Engine@@` (`0x1800dd7d0`) — DECLINED, one level of blocker deeper

It calls `FindOrLoad` **three times** and cannot precede it. Beyond that it needs
`0x1800bec10`, `0x1800ab180` (a `FileSystem` predicate), `0x1800cab50`, `0x1800c5300`
(`Persistence`), `0x1800c9730` (`String::Format`) and `0x1800c0390`, plus a walk over
`IO::Modification`'s 0x80-stride mod array at `0x1801fc6b0`. Reversed far enough to be sure
the blocker is `FindOrLoad` and not something cheaper; not reversed further, because that
would be work whose result cannot be used until step 6 above.

---

## 6. HONEST LIMITS

1. **The exe I measured contains other lanes' uncommitted work.** The worktree this session
   was placed in carried modifications to `runtime/src/object/internal.cpp` (lane UID),
   `runtime/src/collection/array.cpp`, `runtime/src/input/input_win.cpp`,
   `runtime/src/math/rt_math_port.cpp`, `runtime/src/ui/dialog.cpp`,
   `runtime/src/video/context.cpp`, `runtime/types/ui/SpriteContext.h`, and two untracked
   files from other lanes. **None of it was staged or committed by me.** Every number in §2
   and §3 is a *within-tree* comparison (probe on/off, files in/out) taken minutes apart on
   that same contaminated tree, so the contamination cancels. Absolute totals quoted from
   `runtime_census.py` do **not** cancel and should be re-taken by the integrator on a clean
   merge.
2. **Two of the twelve bodies have no ours-vs-theirs comparison at all.** `ArrayResize`
   (`0x18002dff0`) and `ArrayDestroyRange` (`0x18002e3d0`) are reached only from `Clean`,
   and `Clean` has no replay. Their only evidence is (a) transcription from the
   disassembly, (b) the live `arrayGrow=7` / `evicted=913` counters, and (c) the fact that
   the process survived 913 evictions and exited clean. **That is weaker than the other
   ten**, and it is weaker in the specific direction that matters: the treble-growth arm
   (`newSize == oldSize+1 && oldSize != 0`) is the one `Clean` takes, and the *other* arm —
   an arbitrary resize — is never executed by anything.
3. **The shrink arm of `ArrayResize` and the `dst == nullptr` guard are dead code today.**
   Transcribed because retail has them; never executed by any run in this lane.
4. **`Clean`'s three-phase composition is verified structurally, not differentially.** The
   probe covers its primitives. Nothing compares `Clean`-as-a-whole against the shipped
   `Clean`, and nothing can: running it twice against one factory double-frees every
   `External` it evicted.
5. **`Insert` / `InsertCore` / `AllocNode` / `PairCtor` have live coverage of ZERO.** Nothing
   in our tree inserts into the resource tree, because `FindOrLoad` is not ported. They are
   verified *only* by the synthetic lockstep (55 inserts, both sides). The moment
   `FindOrLoad` lands they become the hottest bodies in this file (~22.6k calls per
   menu-to-gameplay run, from lane FNHOOK's `[ResLoad] census: FindOrLoad=22591`), and they
   will then be exercised for the first time on the real subject.
6. **Retail's unguarded deref in `Clean` is KEPT and has never been observed to fire.**
   `0x1800be451`/`0x1800be454` reads address `0x8` when a key collected in phase 1 is not
   found in phase 2. Retail asserts that cannot happen; our port asserts it the same way. On
   the one run measured it did not happen (913 of 913 keys were found). A null check here
   would skip the behaviour rather than restore it, and would hide the desync the fault
   diagnoses.
7. **One run, not three.** §2's numbers come from a single `AUTOSTART`+pause-menu cell plus
   one A/B control. No stress cell, no save/load cell, no tutorials. The probe is
   deterministic (identical counters across runs 2 and 3 where the script was unchanged), so
   the lockstep result is stable; **`Clean`'s live numbers are not claimed to be.**
8. **No perf measurement.** WAVE9.md's own standing rule is that one run cannot resolve the
   5 % gate, so none is offered. `Clean`'s added `printf` runs a handful of times per
   session; the twelve Set bodies are on the resource-load path, which `FindOrLoad` does not
   yet reach.
9. **I did not run the DLL-less cutover boot.** That is the integrator's, per the brief.

---

## 7. Files

```
runtime/src/object/external_set.h          NEW — layout, counters, declarations
runtime/src/object/external_set.cpp        NEW — twelve Set bodies + ExternalFactory::Clean
runtime/src/object/external_set_probe.cpp  NEW — the lockstep synthetic-subject oracle
runtime/src/object/rt_object_port.cpp      +6  — registration, BANISHED_RT_RESSET
docs/runtime_port/WAVE9_LANE_RES.md        NEW — this file
docs/runtime_port/evidence/lane_res/probe_on_autostart.txt
docs/runtime_port/evidence/lane_res/control_resset_off.txt
```

Switches: `BANISHED_RT_RESSET=0` kills the twelve doors and `Clean`;
`BANISHED_RT_RESSET_PROBE=1` arms the lockstep oracle (opt-in — `RtWaveEnabled` returns
true when unset, which is right for a port and wrong for a probe).

---

## 8. Handed on, in priority order

1. **`FindOrLoad`'s remaining closure is §5's six steps**, and steps 1, 3 and 4 are now
   ordinary verified porting work rather than blocked — the tree they all sit on is landed.
2. **`Find` needs a delimiter-adjusted key** (§2). Anyone writing a caller must call
   `AdjustDelimiters` first or the lookup silently misses. It cost this lane one run to
   discover and it is not written down anywhere else.
3. **`Clean` will need `ResLoadGuard` when `FindOrLoad` gets it** (§4). Same tree, erase
   side.
4. **`0x1800c64f0` `Filename::GetPathExtension` is ~40 instructions with a working oracle
   and no caller.** It is free the moment step 2 of §5 needs it, and pointless before.
5. **`set_avl_e24.h`'s node layout now serves two instantiations in two directories.** If a
   third turns up, the useful search is by NODE HEADER, not by container name — §0.
