# WAVE 11 — lane OBJLIFE (Class B, the 5 Object lifecycle rows)

**Worktree** `C:/code/GIT/wt-wave11-objlife` · **branch** `wave11/objlife` ·
cut from `fa4fa86`.

---

## 1. Per-row verdicts

| # | row | verdict | evidence |
|---|---|---|---|
| 1 | `??0Internal@Object@@QEAA@XZ` | **HELD BACK** (ported since Wave 3, body re-verified, gate left shut) | body re-derived from the bytes at 0x1800bfd10; holdback mechanism in §4.2 |
| 2 | `??1Internal@Object@@UEAA@XZ` | **HELD BACK** (ported since Wave 3, body re-verified, gate left shut) | body re-derived from the bytes at 0x1800bfdb0; holdback mechanism in §4.2 |
| 3 | `??1Factory@Object@@QEAA@XZ` | **PORTED — UNVERIFIED** (oracle written, never run) | §3.4; run blocked on the game-dir lock |
| 4 | `??1InternalFactory@Object@@QEAA@XZ` | **PORTED — UNVERIFIED** (oracle written, never run) | §3.4; run blocked on the game-dir lock |
| 5 | `??1ExternalFactory@Object@@QEAA@XZ` | **PORTED — UNVERIFIED** (oracle written, never run) | §3.4; run blocked on the game-dir lock |

Census, measured with `tools/runtime_census.py --namespace Object --unported`:

```
                total  called  ported  covered
before fa4fa86    191      56      32    57.1%
after             191      56      35    62.5%
```

**Set-diff, not the count.** Exactly three rows left the unported set and
**nothing entered it**:

```
LEFT:     ??1ExternalFactory@Object@@QEAA@XZ
          ??1Factory@Object@@QEAA@XZ
          ??1InternalFactory@Object@@QEAA@XZ
ENTERED:  (none)
```

The `internal` (non-namespaced) bucket moved `called 153 -> 154`,
`ported 120 -> 121` — one new address resolved and immediately satisfied. This
port therefore did **not** grow the unsatisfied cutover surface, unlike Wave
10B where four faithful ports grew `called` by nine. That is not luck: every
helper address these bodies introduce is registered in the same commit.

---

## 2. The correction this lane opens with

**Rows 1 and 2 were not unported work.** The brief lists them among the 24-row
Object gap, and the census does report them under `--unported`, but
`runtime/src/object/internal.cpp` has had **complete, registered bodies for
both since Wave 3**, at lines 976-983 (shadow) and 1020-1027 (table tier). The
census's own footnote says so explicitly and always has:

```
runtime/src/object/internal.cpp    8  (opt-in; inside if(table)/if(shadow)/if(checkPtr))
A gated registration is not a port. Arm it, measure it, then move it.
```

So the deliverable for rows 1-2 is **not authoring — it is the arming
decision**, and that is what §4 is about. Anyone scoping a future wave off the
`--unported` list should read that footnote before costing a row: a body that
is ported but gated is indistinguishable from an absent one in the row list,
and this is the second time in three waves that has cost someone scoping
effort (`reference_half_registered_port_reads_as_absent`).

**Both bodies were nonetheless re-derived from the disassembly this wave**
rather than accepted on Wave 3's word, because a written recipe from a good
lane has been wrong before. They are faithful, including the two things most
likely to be "cleaned up":

* `??1Internal`'s **first statement overwrites the object's vptr qword
  UNREAD** (`LEA RAX,[0x180154720]; MOV [RCX],RAX` at 0x1800bfdc6) and its last
  statement overwrites it again with the Base vtable. The port reproduces both.
  This is the fact that makes `??_7DialogController@UI@@6B@` permanently
  unpromotable, and **nothing in this lane changes that verdict**, which is
  independent and permanent.
* The swap-remove in `??1Internal` has **no validation of `_factoryIndex`** —
  not in retail and not in the profile build. If the index is stale the second
  store lands wherever the allocator has since put that block. The port keeps
  it unguarded. The fix for that family is upstream (unregister before freeing),
  never a guard here.

---

## 3. Rows 3-5 — the three factory destructors

### 3.1 The check, ported before the code

The brief's warning was live, not hypothetical: **one of these three iterates
and destroys, and the other two only free.** Each verdict below is read off the
disassembly at the address named, not transcribed.

| row | address | verdict | what it actually does |
|---|---|---|---|
| `??1Factory` | 0x1800071a0 → 0x180008110 | **ITERATES AND DESTROYS** | walks the whole `TypeNode` tree (0x180008250 recurses left +0x38, right +0x40), destroys each node's `value.children` Array (node+0x20), frees each node |
| `??1InternalFactory` | 0x18002f860 | **ONLY FREES** | `~Array<int32>(this+0x10)` then a tail jump to `~Array<Internal*>(this+0x00)`. Nothing else. |
| `??1ExternalFactory` | 0x1800be760 | **BOTH, per member** | `_pathStack` destroys its elements then frees; `_references` and `_resources` each walk a tree releasing one String per node |

Two ways to get this wrong, both of which the disassembly rules out:

* Writing `??1Factory` as "free the block" would **leak one TypeNode plus one
  heap buffer per registered type** — the exact leak-under-a-false-byte-identity
  claim the brief describes.
* Writing `??1InternalFactory` as "destroy each element" — the tidy symmetry a
  reviewer would ask for — would be a **double free on every live object**.
  `_objects` holds *non-owning* `Internal*`; each Internal removes itself in
  `??1Internal`.

The third is the sharpest. `_references` and `_resources` are both String-keyed
Sets, and their subtree destructors release **different slots**:

| helper | node size | children | released slot |
|---|---|---|---|
| 0x180008340 (`avl_trees.cpp`, pre-existing) | 0x30 | +0x18 / +0x20 | **+0x00** |
| 0x18002ebf0 (`_references`, this lane) | 0x30 | +0x18 / +0x20 | **+0x08** |
| 0x18002eea0 (`_resources`, this lane) | 0x38 | +0x20 / +0x28 | **+0x00** |

`avl_trees.cpp` already carries a comment saying a "tidy" symmetric release of
`+0x08` in *its* body would corrupt the heap on the first call. 0x18002ebf0 is
the mirror image — its String is the value, not the key. Merging the two into
one "obviously shared" body would corrupt the heap in one direction and leak
plus wild-decrement in the other. Retail keeps them separate and so does this.

### 3.2 The seventeenth J1 address

`??1InternalFactory`'s tail jump lands on **0x18002fb70**, which is the
`Collection::Array<T>::~Array` family body — the seventeenth address of it, and
the note in `array.cpp` had predicted a seventeenth would turn up because that
family is bounded **below** by a caller-driven survey, never above.

The identity claim is **measured, not eyeballed**. A raw byte-diff of two copies
of one function reports differences that are only distances, so both bodies were
disassembled and every RIP displacement and rel32 resolved to its **absolute**
target before comparing:

```
29 instructions on each side, 29 identical.
The ONE textual difference is an INTERNAL forward branch (je), whose ENCODED
displacement is the same byte (0x36) on both sides — it reads as a difference
only because absolute-resolution is the wrong normalisation for an internal
branch.
External operands resolve identically on both sides:
  0x1801fc718 (allocator singleton) · 0x18014b390 (EnterCriticalSection)
  0x1800c6c00 (Free)                · 0x18014b388 (LeaveCriticalSection)
```

It is registered in `array.cpp` (the family's owner), not in this lane's file,
so the family keeps one owner and no registration is shadowed.

### 3.3 The five helper bodies are SHARED instantiations

Scanning every `E8`/`E9` rel32 in `.text` and resolving its target gives the
in-DLL caller count for each body this lane registered:

| address | body | in-DLL callers |
|---|---|---|
| 0x180008110 | `~Factory`'s real body | 9 |
| 0x180008250 | TypeNode subtree free | 4 (2 are its own recursion) |
| 0x18002fb70 | J1 `~Array<T>` | 5 |
| 0x18002ebf0 | `_references` subtree free | 11 |
| 0x18002eea0 | `_resources` subtree free | 10 |
| 0x18002e9e0 | `~Array<PathStackEntry>` | 4 |
| 0x18002e8d0 | PathStackEntry destroy-range | 3 |

Two consequences, and it is worth being precise about which is which:

* **None of those callers is affected by our registration.** They are in-DLL
  `rel32` calls, and *we intercept only at doors* — a `rel32` is not one. So
  registering these addresses changes nothing for the DLL's own behaviour with
  the DLL present.
* **But the ports are correct for all of them by construction.** MSVC emitted
  ONE body per shape and reused it, which is the same argument the J1 family
  rests on: a body that frees a node does not need to know what type the node
  holds, only where its children and its one destructible slot are. So these
  registrations are worth more than the three rows suggest — at cutover the
  10-and-11-caller bodies will be needed by whatever ports those other call
  sites' owners eventually.

**Which of the four callees these bodies reach are ours today**, since a port
that reaches the DLL for half its work is worth saying out loud:

| callee | owner | ours by default? |
|---|---|---|
| 0x1800093e0 `~Array<T*>` | `avl_trees.cpp` | **yes** — ungated |
| 0x18001c750 `~Array<T>` | `array.cpp` (J1) | **yes** — `BANISHED_RT_COLLECTION` is a kill switch, default-on |
| 0x18002fb70 `~Array<T>` | `array.cpp` (J1, added here) | **yes** — same switch |
| 0x180009c30 `SetNodeFree` | `allocator.cpp` | **no** — the allocator wave is opt-in (`BANISHED_RT_ALLOC=1`), so with the DLL present the node free lands in the DLL |

That last row is not a defect: the address door is port-aware, so arming the
allocator moves it without touching this lane, and at cutover the allocator is
armed by necessity (there is no DLL to fall through to). It is recorded because
"our port frees the nodes" is only true today in the sense that *our port asks
the right function to*.

The one thing the shared-body fact does **not** license is merging 0x18002ebf0 and
0x18002eea0, or either of them with `avl_trees.cpp`'s 0x180008340. Retail kept
three separate bodies because the shapes differ (§3.1), and the shared-body
argument applies *within* a shape, never across.

### 3.4 The oracle — and why `calls=0` here is not a finding

**These three rows have exactly one caller in the entire tree —
`RtEngine_Engine_Shutdown` — and that body itself has `calls=0`.** The full
chain, traced rather than asserted:

* `runtime/src/engine/engine_lifecycle.cpp` is the only file that names any of
  the three (`RT_EXPORT_ONCE(VoidThisFn, "??1Factory@Object@@QEAA@XZ")` and its
  two siblings), and it names them inside `RtEngine_Engine_Shutdown`.
* `code/Game/Game/game_main.cpp:167` resolves
  `?Shutdown@Engine@1@QEAAXXZ` into `s_fn_Engine_Shutdown` — and that pointer is
  only ever **printed** (line 419) and **null-checked** (line 426). It is never
  invoked. Nothing else in the tree calls the real Engine::Shutdown either.
* `ShutdownEngineStaged()` is what actually runs, and it **deliberately stops
  short of exactly this chain**. Its own comment: *"We deliberately stop short
  of the dtor+free chain … because those crash silently in standalone — most
  likely tripping over leaked dialog/UI objects that MenuDLT's destroy stub
  doesn't release."*

So:

> **Which of the four `calls=0` causes applies: "nothing in the tree calls the
> caller".** Not "the port is bypassed", not "the body always took the branch
> the oracle does not cover", not "the registration is dead". No gameplay run,
> with any env var, can execute a line of these three bodies — and this is not
> an accident of coverage that a longer run would fix. The replacement shutdown
> omits the chain **on purpose**, and will keep omitting it until standalone has
> a real `MenuDLT::Destroy`. A future lane that makes that shutdown call the
> stock body is what turns these three into gameplay-exercised rows.

**"No route" is not "no oracle."** `runtime/src/object/factory_dtors_probe.cpp`
(`BANISHED_RT_FACTORYDTOR_PROBE=1`) builds two structurally identical subjects
from scratch, tears one down with the **shipped** body via `RtOriginal` — never
`ResolveExport`, which after registration would compare ours to ours — and one
down with ours.

Two screens, **scored separately**, because a union score cannot tell "both
screens work" from "one is blind and the other covered for it":

| screen | what it reads | catches |
|---|---|---|
| **1 — bytes** | the whole subject image, `memcmp` after teardown | a wrong closing store (qword where retail writes a DWORD, a field zeroed retail leaves alone) |
| **2 — allocator** | the live DLL allocator's own `_allocCount` (+0x638) and `_allocatedBytes` (+0x630) deltas across each teardown | **the leak class** — how many blocks and bytes the teardown actually handed back |

Screen 2 is the load-bearing one for this row set, because a `~Factory` that
skipped the tree walk leaves **identical struct bytes** and returns N fewer
blocks. Screen 1 is structurally blind to the very defect the brief named.

**Negative controls**, each aimed at a screen that actually reads its subject:

| control | the injected defect | must fire | must stay silent |
|---|---|---|---|
| A | `~Factory` frees the root node but not the subtree — literally "only frees the block" | screen 2 | screen 1 |
| B | `~InternalFactory` destroys `_persistIds` and forgets `_objects` | screen 2 | — |
| C | `~ExternalFactory` widens the two closing DWORD stores to qwords | screen 1 | screen 2 |

A and C are deliberate mirror images so neither screen's score can be borrowed
from the other's.

**Results — NOT OBTAINED.** The oracle is written, compiles, and is wired in at
both call sites, but it has **never been run**: the shared game directory was
held continuously by other Wave 11 lanes for this lane's whole window (one 900s
blocking `acquire` timed out while queued behind two of them). Rows 3-5 are
therefore **PORTED, UNVERIFIED**, and are reported that way.

A written oracle is not a measurement. Marking a row VERIFIED because the
instrument for verifying it exists is precisely the substitution this project
keeps re-learning, so the cell stays empty rather than optimistic.

To finish the cell, with the lock held:

```
tools/game_dir_lock.sh acquire objlife        # never piped
cp standalone/build/reimpl-x64.exe "$GAME/reimpl-x64.exe"
tools/rt_run.sh factdtor --timeout 300 -- BANISHED_AUTOQUIT=200 \
    BANISHED_RT_FACTORYDTOR_PROBE=1
grep FactoryDtorProbe "$GAME/standalone_error.log"
```

A boot-only run suffices — the probe fires from `RtObjectPortInit`, before the
window opens. `BANISHED_AUTOQUIT` overriding the New Game path is harmless here,
but nothing else may be read from such a run: it covers nothing past the menu.

**What a reader must check in the output**, not just the verdict words:

* three real cells with `screen1(bytes)=SAME` **and** `screen2(alloc)=SAME`;
* in each, `blocks theirs == ours == expected` — equality with the `expected=`
  column this file computed is what rules out two noise deltas cancelling;
* `CONTROL SCORE screen1 1/1, screen2 2/2`, scored separately;
* the two asymmetry lines present — CONTROL-A invisible to screen 1, CONTROL-C
  invisible to screen 2. If either is missing, one screen is doing the other's
  work and the separate scores are not independent after all.

**What the oracle cannot reach**, stated rather than implied:

* The trees are **hand-built here**, not built by the engine's own inserts. The
  shape (7 nodes, both children populated, some child arrays null and some
  real) exercises every branch but is not a shape retail produced.
* It compares the **total** of the frees, not their order. A body freeing the
  same blocks in a different order would pass.
* Recursion depth here is 3; retail's would be ~20. Nothing about stack depth
  is tested.
* Screen 2 reads a **process-wide** counter, so any other allocation between
  the two snapshots is noise. The probe defends against reading noise as a
  finding by printing an `expected=` column it computed from the builder — a
  cell where `theirs == ours == expected` cannot be a lucky cancellation of
  noise, and a cell where they differ from `expected` in the same direction is
  noise rather than a divergence. Read all three numbers, never just the verdict.
* The probe **refuses to report at all** if the allocator singleton is not
  readable, rather than falling back to a bytes-only run. A screen-1-only
  verdict would be a green board over exactly the defect class this row set is
  about, which is worse than no verdict.

---

## 4. Rows 1-2 — the arming decision, and why the gate stays shut

**Verdict: HELD BACK. The gate is left closed, and NOT for lack of evidence
about the bodies.**

### 4.1 The bodies are fine

Both were re-derived from the bytes this wave (§2) and are faithful. The Wave 3
predictive shadow — the real kind of oracle, where ours predicts and theirs runs
— recorded **93,387 calls each on the ctor and dtor with 0 divergences**, and
the exe reaches them through a port-aware door: `RuntimePortInit()` runs at
`standalone/main.cpp:1734`, *before* `AddressRegistryInit()` at 1755, so the
exe's IAT slots 0x140152768 / 0x140152770 bind to our bodies once the tier is
armed. There is a real route and it is a busy one.

This lane also fixed the instrument that was missing: the table tier had **no
attendance counter at all**. `RtObjectInternalAttendance()` now prints
`ctorRuns` / `dtorRuns` at exit, always, including zero, and distinguishes *"not
registered on this run"* from *"REGISTERED BUT NEVER ENTERED"* by asking
`RuntimeExportIsRegistered`. Those are opposite findings and a silent log cannot
tell them apart.

### 4.2 The reason the gate stays shut is a CROSS-LANE hazard, not a doubt

`internal.cpp`'s header carries the argument that makes this tier safe:

> *"There is ONE vtable, and it is THEIRS … so the ported constructor stamps the
> DLL's vtable by name. An object built by our ctor is byte-for-byte
> indistinguishable from one built by the DLL's."*

**That bullet is conditionally true, and its condition is not visible from the
site where it is asserted.** `VtblInternal()` / `VtblBase()` reach
`??_7Internal@Object@@6B@` and `??_7Base@Object@@6B@` through **`ResolveExport`,
which consults the port registry first**. Measured on this worktree:

| fact | check |
|---|---|
| both symbols are MINTED | present in `standalone/rt_data_mint_tables.inc` |
| neither is PROMOTED today | absent from `standalone/reg_mint_promoted.cpp` |
| both are **Wave 11 lane OBJDATA's Class A rows** | the wave brief's own table |
| a promotion binds them by name | `reg_mint_promoted.cpp` registers each as `RegisterRuntimeExport("<symbol>", st)` |

So the moment those two rows are promoted, `ResolveExport` starts handing back
**our minted vtable**, and:

> objects built by **our** ctor (the doors we intercept) carry the minted vptr,
> while objects built by the **DLL's** ctor (its own in-DLL `rel32` callers,
> which we do not intercept) carry 0x180154720.

That is **two vtables in play on one class** — PLAN.md §4.3 risk 3 verbatim, the
exact hazard the bullet exists to deny.

**Neither lane creates it alone.** It needs the TABLE tier armed *and* those two
`??_7` rows promoted, and the two lanes are in the same wave. Today neither
holds, so no shipped build is affected.

Arming rows 1-2 is therefore no longer a decision this lane can make by itself.
A gate that costs nothing today and prevents a silent split-brain tomorrow stays
shut until the interaction is resolved. The annotation is in place at
`internal.cpp`'s bullet so the next reader of that safety argument meets its
condition rather than inheriting it.

### 4.3 The fix, named but deliberately not made

`VtblInternal()` / `VtblBase()` should resolve **RAW** (theirs, while the DLL is
present) and fall back to the registry only when it is absent — which decouples
the two lanes completely. That change is not made here because **it also decides
what cutover sees**, where the minted vtable is the only one there is. That
belongs to whoever owns the mint, not to this file.

### 4.4 What a future lane should measure before lifting the gate

1. The attendance line reads a large non-zero `ctorRuns`/`dtorRuns` — not
   "booted fine".
2. The two `??_7` rows are either unpromoted, or the RAW-resolve fix from §4.3
   has landed.
3. The shadow tier is re-run on the tip and still reports 0 divergences with
   `calls=` in the tens of thousands.

---

## 5. Things that contradict the brief, or that the next lane should know

1. **Rows 1-2 were already ported.** See §2. The brief's framing ("these are
   foundational … a defect here is a defect everywhere") is right about the
   stakes and wrong about the state; the work was the gate, not the body.

2. **`??1Factory`'s "and nothing else" is not quite right.**
   `engine_lifecycle.cpp:821` says *"`??1Factory@Object@@` (0x1800071a0) is
   `JMP 0x180008110` and nothing else"*. 0x1800071a0 is an MSVC EH-frame thunk:
   it spills RCX, writes the `-2` unwind state slot, adjusts RSP and *then*
   tail-jumps. The frame slots have no effect on the object, so the conclusion
   drawn there (using the exported name is byte-equivalent) survives — but a
   reader who took "and nothing else" literally would look for a 5-byte JMP and
   not find one. **Both addresses are registered onto the one body** for that
   reason.

3. **`runtime/types/object/External.h` includes `TypeInfo.h` from INSIDE its own
   `namespace RtObject` block** (line 322). Whichever of the two headers a
   translation unit sees first therefore decides whether the reflection types
   are `RtObject::TypeNode` or `RtObject::RtObject::TypeNode` — the include
   guard makes the second inclusion a no-op, so the loser's spelling silently
   wins. This is a latent trap: reordering the includes in a file that uses both
   will not fail to compile, it will change what `TypeNode` means.
   `factory_dtors.cpp` pulls `TypeInfo.h` in first and says why.

4. **`??_7DialogController@UI@@6B@` is untouched.** Its unpromotable verdict
   rests on `??1Internal`'s unread vptr overwrite, which this lane reproduced
   rather than changed. Nothing here is grounds to revisit it.

5. **The three ports are `QEAA` and were checked against the export directory**,
   not assumed. `??1Internal` is the only `UEAA` of the five. `AEAA` would have
   meant *private*, not unexported — this DLL exports its private members — so
   no address door in this lane is justified by the word "unexported".
