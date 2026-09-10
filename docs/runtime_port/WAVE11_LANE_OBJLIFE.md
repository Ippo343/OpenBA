# WAVE 11 — lane OBJLIFE (Class B, the 5 Object lifecycle rows)

**Worktree** `C:/code/GIT/wt-wave11-objlife` · **branch** `wave11/objlife` ·
cut from `fa4fa86`.

---

## 1. Per-row verdicts

| # | row | verdict | evidence |
|---|---|---|---|
| 1 | `??0Internal@Object@@QEAA@XZ` | **HELD BACK — ARM-READY** | shadow 126,764 calls / 0 divergences; armed run 122,030 entries, clean exit · §4 |
| 2 | `??1Internal@Object@@UEAA@XZ` | **HELD BACK — ARM-READY** | shadow 126,758 calls / 0 divergences; armed run 122,024 entries, clean exit · §4 |
| 3 | `??1Factory@Object@@QEAA@XZ` | **PORTED + VERIFIED** | synthesised differential, run `factdtor6`: both screens SAME, 10/10/10 blocks · §3.4 |
| 4 | `??1InternalFactory@Object@@QEAA@XZ` | **PORTED + VERIFIED** | same run: both screens SAME, 2/2/2 blocks · §3.4 |
| 5 | `??1ExternalFactory@Object@@QEAA@XZ` | **PORTED + VERIFIED** | same run: both screens SAME, 19/19/19 blocks · §3.4 |

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

**Results — run `factdtor6`, 2026-09-09.** Three real cells, both screens green,
and the block counts equal to the `expected=` column this file computed
independently of either side:

```
~Factory           screen1=SAME screen2=SAME  blocks theirs=10 ours=10 expected=10  bytes 712/712
~InternalFactory   screen1=SAME screen2=SAME  blocks theirs= 2 ours= 2 expected= 2  bytes 144/144
~ExternalFactory   screen1=SAME screen2=SAME  blocks theirs=19 ours=19 expected=19  bytes 768/768
VERDICT: 3 real cells, screen1 failures 0, screen2 failures 0
```

Negative controls, **scored separately**:

```
CONTROL-A only-frees-root  screen1=SAME          screen2=DIFFER  blocks 10 vs 1   <- designed
CONTROL-B half-done        screen1=DIFFER        screen2=DIFFER  blocks  2 vs 1
CONTROL-C wide-stores      screen1=DIFFER        screen2=DIFFER  blocks 19 vs 19
CONTROL SCORE  screen1 1/1 caught   screen2 2/2 caught
```

**CONTROL-A is the one that matters and it behaved exactly as designed:**
invisible to screen 1, caught by screen 2, 10 blocks against 1. That is the
proof that screen 2 is load-bearing rather than decorative — the leak class this
row set is about is *structurally invisible* to a byte comparison, and here is a
run where it was.

### 3.5 One prediction of mine FAILED, and it indicts the instrument

I predicted CONTROL-C would be **invisible to screen 2**. It was not: screen 2
reported DIFFER. The reason is not the injected defect.

```
real ~ExternalFactory cell   blocks 19/19   bytes theirs=768  ours=768
CONTROL-C cell               blocks 19/19   bytes theirs=776  ours=768
```

The **block** column behaved exactly as predicted — 19 against 19, silent. It is
the **byte** column that fired, and it fired on the *theirs* side, in a cell
where both sides tear down identically-built subjects. Eight bytes appeared on
the DLL side of one cell and not another.

So `_allocatedBytes` is **not a pure function of what was freed**: it records the
size the allocator actually carved, which depends on bin and coalescing state,
and that state has moved by the time the fifth and sixth cells run. Since
`allocEqual` is `blocks equal AND bytes equal`, one noisy byte total flips the
whole screen.

Consequences, stated rather than smoothed over:

* **The three real verdicts are unaffected.** In every real cell both columns
  agreed exactly *and* matched the independently computed `expected=`. Two noise
  deltas cancelling to hit `expected` in all three cells is not a live worry.
* **The byte half of screen 2 is noise-susceptible; the block half was stable
  across all six cells.** A future user of this probe should read the block
  column as the verdict and the byte column as corroboration.
* **My asymmetry claim was half right and I have not rewritten it to look
  wholly right.** The design intent — that neither screen's score could be
  borrowed from the other's — survives on CONTROL-A, which is the direction that
  actually protects against the defect class. It failed on CONTROL-C, in the
  harmless direction (a control firing on *more* screens than predicted), and
  the `expected=` column is what let me tell that from a real divergence in
  under a minute. That column existing at all is the reason this paragraph is a
  diagnosis and not an open question.

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

## 4. Rows 1-2 — the arming decision, measured

**Verdict: HELD BACK — ARM-READY.** Every measurement the gate was waiting for
is now green; what is missing is not evidence but a decision that belongs to the
integrator, for the reason in §4.3.

### 4.1 What the runs showed

Four runs under one lock hold, all clean-exiting, all `standalone_crash.log`
zero bytes.

**The predictive shadow oracle, re-run on this tip** (ours predicts, theirs
runs, compare — not an `RT_DIFF_SHADOW` self-check):

```
Object::Internal::Internal  [shadow]   calls=126764   divergences=0
Object::Internal::~Internal [shadow]   calls=126758   divergences=0
```

That is a third more traffic than the 93,387 Wave 3 recorded, and still zero.

**The TABLE tier armed** — our bodies driving the live handle table:

```
[RtInternalTable] ctorRuns=122030 dtorRuns=122024 clearRuns=0 resetRuns=0
                  (our ported bodies drove the LIVE handle table this many times)
```

**The attendance counter is a real control, and it discriminated three ways:**

| run | line | reading |
|---|---|---|
| baseline (tier unarmed) | `ctorRuns=0 … not registered on this run` | the quantity **varies with the thing it gates** |
| shadow | `ctorRuns=0 … REGISTERED BUT NEVER ENTERED` | correct: the shadow wrappers are registered, the table bodies are not the ones running |
| table armed | `ctorRuns=122030` | the bodies actually ran |

A counter that read non-zero in the baseline would have been tracking frames
rather than the tier, and the armed run's number would have meant nothing.

**A cross-check nobody designed and it is the strongest single number here.**
Ctor and dtor differ by exactly **6** in both the shadow run (126764/126758)
*and* the armed run (122030/122024). Two independent runs, different tiers,
different totals, the same residue of six objects alive at exit. Our bodies and
the DLL's leave the table in the same state.

### 4.2 The cross-lane hazard: real, but NOT triggered on this merge

An earlier draft held these rows back because promoting
`??_7Internal@Object@@6B@` / `??_7Base@Object@@6B@` would make
`VtblInternal()`/`VtblBase()` — which reach them through registry-aware
`ResolveExport` — hand back **our minted vtable**, so objects built by our ctor
would carry a different vptr from objects the DLL's own `rel32` callers build.
Two vtables on one class: PLAN.md §4.3 risk 3, the hazard `internal.cpp`'s
header bullet exists to deny.

**The mechanism is real and the annotation stays. The trigger is not present.**
Lane OBJDATA promoted **no `??_7@Object` row at all** — all occurrences in its
`reg_mint_promoted.cpp` are comments. I had verified those rows were *minted and
promotable* and then reasoned about what a promotion **would** do; I never
checked whether one **had** happened. The integrator did. That half of my
finding was an inference standing next to measured facts, which is what made it
read as measured.

The durable half is that the two lanes' exit conditions were **mutually
triggering**: OBJDATA's expiry for `??_7Internal` is "attendance > 0", and the
only thing that moves that counter is this tier — so a future lane satisfying
that expiry *literally* walks into the hazard. That is now a co-condition in
`WAVE11.md`.

### 4.3 Why the gate is still shut, given all of that

Not for lack of evidence. Three reasons, in order:

1. **This lane wrote the instrument that measures it.** The attendance counter,
   the baseline control and the tier are all mine, from the same session. A
   green board from an instrument its author also armed is worth less than the
   same board read by someone who did not build it. The numbers are here for
   exactly that reading.
2. **Ungating changes ~122k calls per run on the busiest path in the engine**,
   for every other lane in the wave, at merge time. The cost of the gate staying
   shut one more wave is one environment variable; the cost of being wrong is
   diffuse and hard to attribute.
3. **The `??_7` co-condition now written into `WAVE11.md` is a rule about a
   future promotion, not a property of this build.** Arming makes the tier the
   thing that satisfies OBJDATA's expiry. Leaving it shut keeps those two
   decisions independent for one more wave, at no measured cost.

**Flip `BANISHED_RT_INTERNAL_TABLE` to default-on when an integrator who did not
write the counter has read §4.1 and agrees.** Everything else is done.

### 4.4 What is NOT evidence here

`clearRuns=0 resetRuns=0` in every run. Those two rows (lane OBJINT's) live only
on the save/load path and no run this session saved or loaded. That is a
statement about **the runs**, not about those ports — which is why the counter
prints it on its own line naming `BANISHED_AUTOLOAD` rather than folding it into
the ctor/dtor line, where a reader could take one pair's zero through the other
pair's 122,030 and call it exercised.

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

6. **The DLL's `MemoryAllocator` singleton does not exist at `RuntimePortInit`,
   and calling `MemAlloc` there FAULTS.** Measured, at the cost of a run
   (`factdtor4`, exit=3, ACCESS_VIOLATION):
   `?_singleton@MemoryAllocator@System@@` at 0x1801fc718 reads NULL both at
   `RuntimePortInit` and again at the exit summary — early it is not yet
   constructed, late it has been torn down. The live window is **between engine
   startup and engine shutdown**, so anything that must allocate through the
   DLL belongs on the frame tick, not in either summary. Two consequences worth
   carrying forward:
   * A probe that allocates *in order to find out whether it may allocate* has
     already crashed. This one now snapshots first and allocates only after the
     snapshot succeeds.
   * The first version of this probe read that slot through
     `ResolveAddr(Module::Runtime, …)`, **which consults the port registry
     first** — the wrong door for an oracle's theirs-side read on principle,
     independently of whether anything happens to be registered there today.
     `ResolveRuntimeRaw` is the right one.

7. **A block-scope `extern` declaration inside a function in an anonymous
   namespace declares the symbol IN that anonymous namespace**, and links
   against something that does not exist. It fails at link time, so it is cheap
   — but the fix is a file-scope declaration, not a differently-spelled block
   one.

8. **`tools/game_dir_lock.sh` has three defects this lane hit**, all reported
   and being fixed at wave close: `line 293: [: : integer expression expected`
   on every queue poll; a `ticket_head()` that permanently skips a waiter whose
   ticket file is empty (which is what starved this lane through two 900s
   windows, and which reads *exactly* like being at the head of the queue); and
   `acquire` returning non-zero on timeout while a wrapping shell returns 0 —
   the documented pipe hazard one layer out, hit **three times** here. Echo the
   inner status explicitly; do not trust the wrapper's.
