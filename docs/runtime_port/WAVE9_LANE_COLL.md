# Wave 9 — lane COLL: the `Collection::Set` / `Collection::Map` insert side

Branch `wave9/lane-coll`, cut from integration tip `ba7bc60`.

The lane this project has wanted for three waves: the keyed entry points of the
header-only `Collection::Set` / `Collection::Map` templates — the descents, the
node allocations, the pair constructions and the wrappers — plus the recursive
subtree free, the packfile introsort, two element copy-assigns and the CRT
`_stricmp` they all compare with.

---

## 1. Headline

| | |
|---|---|
| Bodies written | **20** |
| Addresses registered | **29** (24 + 3 + 2 across three files) |
| Unregistered address doors | **42 → 34**, set-diff exactly the 8 targets |
| Lockstep probe (in-boot) | **115 steps, 0 mismatches, 0 parent-linkage faults, 0 gaps, 4/4 negative controls CAUGHT** |
| Standalone sort oracle | **784,000 cases, 0 permutation differences** |
| Standalone `_stricmp` oracle | **6,000,000 comparisons, 0 differences** |
| `Object::Factory::Startup` ours-vs-shipped | **theirs.count=415 ours.count=415, 0 mismatches** on the real 416-type population |
| Whole-run diff harness | **483 sites, 17,096,197 calls, 1 divergence** — the Factory self-check line, explained below and NOT a defect |
| `port_bypass_audit.py` | 0 bypasses on both doors |
| Census | 1003 called, 753 ported, **75.1%** |

Measured exe `943a8053d3a1226b2f796681492c0dc7`. A rebuild from the committed
tree after the run reproduces that md5 byte for byte, so the measurement is of
the committed source (`reference_lane_edits_after_report_invalidate_the_measurement.md`).
The run was gated on an identity line the build emits into its own log —
`[RtPort]   Collection keyed Set/Map + sort (lane COLL): 24 + 3 + 2 addresses
registered` — not on the on-disk md5, which says which exe is on disk and not
which one ran.

---

## 2. The doors, before and after

`tools/addr_door_audit.py`, run on the committed tree.

```
BEFORE   by-address call sites 284   distinct 201   registered 1125   UNREGISTERED 42
AFTER    by-address call sites 297   distinct 205   registered 1154   UNREGISTERED 34
```

Set-diff of the UNREGISTERED column, `before \ after`, is **exactly eight and
exactly the targets**, with nothing added:

```
0x180008340   the recursive subtree free          filesystem_packages.cpp:646
0x180025a80   the packfile-index BST insert       filesystem_packages.cpp:824
0x1800264a0   the packfile sort                   filesystem_packages.cpp:767
0x18002a090   Map<String,String>::Insert          persistence.cpp:59 | persistence_registry_win.cpp:266
                                                  | configuration.cpp:268 | engine_funcs.cpp:1723
0x180030140   Set<Pair<u32,TypeRecord>>::Insert   type_system.cpp:529, :639
0x180050d30   Array<T16>::operator=               audio.cpp:1114
0x180051650   EffectSlot copy-assign              array.cpp:427
0x18007eefc   the CRT _stricmp                    filesystem_openfile_win.cpp:192 | filesystem_packages.cpp:438
```

**The denominator moved and it is not a trick.** `distinct addresses` went
201 → 205 and `call sites` 284 → 297 because the new bodies themselves call
eleven addresses by literal (`0x1800c7a50`, `0x1800c7ad0`, `0x180009350`,
`0x1800093e0`, `0x1800067d0`, `0x180051d80`, `0x18002a310`, `0x180025e10`,
`0x180050d30`, `0x18007eefc`, `0x1801fba28`). Every one of them is registered,
so none appears in the unregistered column — but they are spelled as literals at
the call site on purpose, so the census can see the edges this lane CREATED and
not only the ones it retired
(`reference_census_cannot_see_nonliteral_registrations.md`, whose failure mode
is exactly a lane hiding its new rows behind a helper).

---

## 3. Closure, counted BEFORE any body was written

Lane PICK's rule: porting the visible rows of a census can trade N doors for
more than N. Counted transitively from `disassemble_function(filter_mnemonics="CALL")`
on every body, before a line was written.

| Group | Entry | Closure |
|---|---|---|
| A | `0x180030140` Set<Pair<u32,TypeRecord>>::Insert | 0x180030380, 0x180008440, 0x1800087c0, 0x180030540, 0x1800307b0, 0x1800308e0, 0x180009350, 0x1800093e0 (**9 bodies**) |
| B | `0x18002a090` Map<String,String>::Insert | 0x18002a170, 0x18000f970, 0x18000fb00, + 0x18002a310/590/6c0 (**7 bodies, 3 reused**) |
| D | `0x180025a80` Set<Pair<Filename,u64>>::Insert | 0x180025c70, 0x180008520, 0x180008840, + 0x180025e10/026080/0261b0 (**7 bodies, 3 reused**) |
| E | `0x180008340` recursive subtree free | LEAF apart from two already-registered doors (**1 body**) |
| C | `0x1800264a0` packfile sort | **LEAF — no CALL instructions at all** (**1 body**) |
| F | `0x180051650` EffectSlot copy-assign | 0x180050d30 (**2 bodies**) |
| G | `0x18007eefc` `_stricmp` | 0x18007eec0 `__ascii_stricmp` (**2 bodies**); the locale arm declined, §7 |

**Closure size 20 written + 6 reused registrations. It CLOSES: no new
unregistered door was created.** The one open dependency is `0x18007eefc`,
which was already an unregistered door with two callers — and it is now ported
too (§7), so that hole closes as well.

---

## 4. The finding that made groups B and D cheap — and how it was CHECKED

The Map<String,String> tree and the packfile Filename tree turn out to have the
**same node header** as the message-callback Set lane VID2 ported: element 0x10,
parent +0x10, left +0x18, right +0x20, balance +0x28, height +0x2a, sizeof 0x30.
Their retraces and rotations are separate compilations of one template.

"Separate compilations of one template" is a *claim*, and this tree's Array/Set
family has produced four near-misses that all passed green runs and differed
only in an alignment immediate, a shift, or a trailing call. So it was
**measured against the shipped bytes**, not asserted:

| family | addresses | result |
|---|---|---|
| rotate-left | `0x18000b040`, `0x18002a590`, `0x180026080` | **BYTE-IDENTICAL** over the whole 0x12b-byte extent |
| rotate-right | `0x18000b170`, `0x18002a6c0`, `0x1800261b0` | **BYTE-IDENTICAL** over the whole 0x12b-byte extent |
| retrace | `0x18000adc0` vs `0x18002a310` / `0x180025e10` | **NOT byte-identical.** First difference at **+0x23**: `MOV R8,RAX` vs `MOV RDX,RAX`. The two later bodies carry the node in RDX where `0x18000adc0` carries it in R8, and `0x18000adc0` RE-READS `*slot` before the `balance > 1` test where the other two use the register they already hold. Nothing writes `*slot` between those two points, so the three are semantically one. `0x18002a310` and `0x180025e10` ARE byte-identical to each other. |

Lane VID2's `rtsetavl::RotateLeft` / `RotateRight` / `Retrace` are therefore
registered at the six sibling addresses. **These are six DISTINCT addresses
bound to three existing bodies — the opposite of a duplicate registration,
which is last-binding-wins and silently makes one body dead code.** Checked
across the whole tree: none of the six is registered anywhere else.

The generalisable half: **a byte comparison against the shipped image is cheap
and it is the only thing that turns "these look like the same template" into
evidence.** It also told me where NOT to reuse: I was one step from assuming the
three retraces were identical, and they are not.

---

## 5. `0x1800264a0` — the packfile sort, which lane PKG had declined

`filesystem_packages.cpp:753` declined this deliberately and its reasoning was
correct:

> *"A tie-heavy population is exactly where an unstable sort's exact swap
> sequence decides the answer, and that answer decides which package owns a
> colliding path in LOOP 3. A 'clean' std::sort here would compile, run, and
> silently pick different winners. With no run and no oracle available in this
> lane, transcribing it would be an unverified guess at a precedence rule."*

Every clause stands. What changed is the last one.

### What the body actually is

Transcribed instruction by instruction, not written as "an introsort":

* **The order is DESCENDING.** `CMOVG RDX,R8` at `0x1800264fc` moves the tracked
  slot when the TRACKED element's key is GREATER, so the small-range pass tracks
  the MINIMUM and swaps it to the HIGH end, then shrinks the high bound.
* The `n <= 8` path is a **selection** sort, not an insertion sort, and its scan
  start `lo+1` is **hoisted** out of the outer loop (`0x1800264d6`) rather than
  recomputed as `hi` shrinks.
* **THE TIE RULE IS ONE INSTRUCTION.** `CMOVG`, not `CMOVGE`: the tracked
  position moves only on a STRICT `>`, so among equal keys the **earliest
  position wins**. That single letter is the whole of "which package owns a
  colliding path".
* The comparator is the whole of `CMP dword ptr [RCX+0x70],EAX` — SIGNED 32-bit,
  one field, no tiebreak, no secondary key, no pointer comparison. On equality it
  does nothing at all; position is decided entirely by the swap sequence.
* Median-of-3 in retail's exact order; two partition scans; **two** equal-run
  walks; a pivot-slot fixup when the pivot is the element swapped out of `j`
  (and no matching fixup for a pivot at `i` — retail has none); a 62-entry
  pending-range stack with **no bounds check**, safe only because the split
  pushes the LARGER half and iterates on the smaller.

### The oracle, and the division of labour worth keeping

**The shipped body is a LEAF** — `filter_mnemonics="CALL"` returns nothing — so
it needs no allocator, no engine and no boot, and a bare `LoadLibrary` harness
drives it directly. And a sort is a pure permutation of a caller-owned buffer,
so it **replays**: both implementations get the same input in two copies and the
resulting **permutation** is compared element for element via an injected id —
never the sortedness, which every wrong sort also satisfies (lane PICK's lossy-
transform rule).

```
cases = 784,000      permutationDiffs = 0
key domains 1 (every element a tie -- the LIVE shape), 2, 3, 5, 17, 1000, full random
sizes 1..300, every value across the n<=8 boundary
112,000 of the cases were ALL-TIES

effectiveness, counted INSIDE the body:
  smallPasses 12,161,086   smallSwaps 12,161,086   medianSwaps 4,575,503
  partitions   3,793,610   partSwaps  22,980,048   pivotFixups 1,522,946
  eqRunA      11,014,778   eqRunB     16,483,836   pushes=pops  3,368,077

negative control -- a STABLE sort by the same key (correctly descending,
differing only in tie permutation, i.e. exactly the defect the comment feared):
  CAUGHT in 7,571 of 7,800 cases
```

> **The live boot call verifies that the REGISTRATION is reached; the harness
> verifies the BODY.**

That division is not a formality here, it is the whole coverage argument. The
live population is four packages that all carry the same `+0x70` (`0x2710`,
seeded by the constructor), and four all-equal elements never leave the `n<=8`
path — so on the live call alone `partitions`, `pivotFixups` and both equal-run
counters read **0** and a green run would be vacuous.

---

## 6. `0x180030140` — the row `Object::Factory::Startup` could not close

`type_system.cpp` was ported in Wave 3 but its row did **not** close at cutover:
the body resolved `0x180030140` through the port-aware door, found nothing, and
printed a diagnostic and returned. With the DLL absent the type tree was simply
not built.

It closes here, and it is verified against the shipped body on the real
population by the probe that already existed in `type_system.cpp` (armed with
`BANISHED_RT_FACTORY_STARTUP_PROBE=1`), which runs the **shipped**
`Factory::Startup` on one synthetic `FactorySet` and ours on another and
compares the trees node by node:

```
chain=416  nodes=415  theirs.count=415  ours.count=415
withParent=413  bogusParent=0  withChildren=41  childEntries=413
grownArrays=28  mismatches=0
negative controls: capacity-perturbation detected=YES, child-perturbation detected=YES
VERDICT: 0 mismatches
```

**The 416-vs-415 shortfall is explained and is not a lost insert.**
`ReportKeyCollisions()` names it:

```
key 0x52DCE80B  AutoSaveDialog  AutoSaveDialog  SAME NAME -- this type is registered TWICE
```

So the duplicate arm of the ported insert correctly returns the existing node
without incrementing the count, and retail's identical loop into its identical
Set produces the same 415. This settles the open question WAVE9.md left when the
broken version of that self-check reported `ours=nodes=415 theirs=nodes=416` and
lane UIC read it as a lost insert: it never was one.

**This is also the one divergence in the whole-run diff summary.** `[RtDiff]
==== 483 sites, 17,096,197 calls, 1 divergences in 1 sites ====`, and the site is
`Object::Factory::Startup [probe, not diffable]` printing
`ours=chain=416 nodes=415 badCrc=0` against its constructed expectation
`nodes==chain==416`. That expectation is what is wrong, not the port — it
assumes no duplicate registrations. Left as-is rather than "fixed" to match,
because the line is doing its job: it is what surfaced `AutoSaveDialog`.

---

## 7. `0x18007eefc` — `_stricmp`, taken on the integrator's request

Now a **fourth** caller (`filesystem_openfile_win.cpp:192`,
`filesystem_packages.cpp:438`, `filesystem_find_win.cpp`, and this lane's
packfile descent), and a null at cutover for all of them.

`runtime/src/crt/msvc_stricmp.cpp` ports `_stricmp` (`0x18007eefc`) and
`__ascii_stricmp` (`0x18007eec0`).

**The locale arm is DECLINED, and that it is unreachable is a measurement.**
Retail's first instruction pair is `CMP dword ptr [0x1801fba28],0 / JNZ` into
`_stricmp_l`. Every reference to that flag was enumerated: 23 of them, and
exactly **one is a WRITE** — `_wsetlocale` at `0x180083052`. `_wsetlocale`
(entry `0x180082f74`) has exactly one reference in the whole 15,290-function
binary and it is a **DATA** reference at `0x18020aba0`, a CRT dispatch slot, not
a call site. Nothing in Runtime-x64.dll ever calls setlocale. Confirmed live as
well: `[RtCrt] _stricmp locale flag [0x1801fba28] = 0`.

The arm **aborts loudly** rather than falling through to the ASCII path. A
silent fallback would make a broken premise indistinguishable from a correct
one, which is the shape of half the defects this wave found.

### Oracle — again a leaf, again unbounded

```
3,000,000 randomised pairs x 2 bodies = 6,000,000 comparisons, 0 differences
outcome spread  lt=858,597  eq=840,403  gt=1,301,000
1,529,089 pairs containing a byte >= 0x80;  687,207 pure case-variant pairs
```

### A CORRECTED CLAIM, reported because it did NOT fire

The received comment said `std::strcasecmp` would be wrong here. Two negative
controls were run and **one missed**:

* **vs the host CRT's `_stricmp`** (MinGW, default "C" locale): **0
  sign-differing cases in 400,000 high-byte pairs.** So "strcasecmp would be
  wrong" is *not unconditionally true* — in the C locale the host's lowering is
  also 'A'..'Z' only and the two agree. The real hazard is that it becomes wrong
  the moment anything selects a locale, i.e. it makes the packfile index depend
  on process-global CRT state no call site controls. Still a good reason to
  port, but a **different** reason from the one recorded, and the source comment
  now says so.
* **vs a SIGNED-byte lowering comparison** — which is exactly what this lane's
  `StrCmpSigned` (the Map<String,String> comparator) does: **169,849
  sign-differing cases.** *That* half of the original comment is exactly right
  and is now quantified. The two comparators must never be shared, and the two
  trees that use them must never share a descent even though their node headers
  are identical.

---

## 8. The lockstep probe, and the two defects it found in ITSELF

`runtime/src/collection/avl_trees_probe.cpp`, `BANISHED_RT_AVLTREES_PROBE=1`.
Two private trees per section, ours through the ported bodies and theirs through
`ResolveRuntimeRaw`, **compared after every insert** on a fingerprint that
carries shape (pre-order, side-of-parent, balance, height, nulls emitted) plus a
separate parent-linkage walk. AVL defects cancel; a final comparison would
report green.

Final in-boot run:

```
[AvlColl] S1 type-tree:      steps=53 depthAfter24Ascending=5 (a spine would be 24) rootMoved=6 finalCount=52
[AvlColl] S2 string-map:     depth=6  rootMoved=3 finalCount=28
[AvlColl] S3 packfile-index: depth=5  rootMoved=2 finalCount=16
[AvlColl] S3b subtree-free:  visited=15 expected=15  (NOT a lockstep replay)
[AvlColl] S4 sort:           cases=13 permutationDiffs=0
[AvlColl] S4 effectiveness:  smallPasses=62 smallSwaps=62 medianSwaps=16
[AvlColl] S4                 partitions=18 partSwaps=51 pivotFixups=6 eqRunA=53 eqRunB=59 pushes=14 pops=14
[AvlColl] S4 negative-controls: tie-rule=CAUGHT stable-sort=CAUGHT
[AvlColl] S5 element-assigns: comparisons=13 bad=0
[AvlColl] steps=115 mismatches=0 parent-linkage-bad=0 gaps=0
[AvlColl]   50-node: rotL=32 rotR=17 preRotL=4 preRotR=3 retraceIters=253
[AvlColl]   descent: left=161 right=257 equal=6 rootInsert=5 nullSearchKey=5 nullNodeKey=0
[AvlColl]   strcmp : lt=102 eq=2 gt=40      stricmp: lt=53 eq=8 gt=30
[AvlColl]   ptrArray: realloc=41 treble=0 self=0 | free: nodes=15 stringsReleased=15
[AvlColl] negative-controls: signed-descent=CAUGHT  first-byte-strcmp=CAUGHT
[AvlColl] VERDICT: PASS (mismatches=0 gaps=0)
```

Every effectiveness counter is a branch taken **inside** a ported body, not a
proxy observed from outside it. `ptrAssignTreble=0` is expected and is not a
gap: that arm is dead in retail too, because the destructor has already zeroed
`_size` before `old` is read.

### Defect 1 in the probe — found by CRASHING, and the crash proved the port right

The first run died at `Runtime-x64.dll + 0xC6D00`, `READ of 0x8`, four frames
deep inside the **shipped** `0x180008340`. My probe's strings were static
buffers; `Release` drives the refcount to zero and then frees `str-8`, so a
static buffer handed to anything that releases is a free of a non-heap pointer.

**The crash was in RETAIL's body, on RETAIL's tree, and that is exactly how you
tell a bad corpus from a bad port** — a port defect cannot make the shipped body
fault on a tree the shipped body built. Fixed by giving the probe real
allocations from `MemoryAllocator::MemAlloc`, laid out as `System::String` lays
one out, one fresh block per call (never reused: `Filename`'s copy ctor
normalises `/` to `\` **in place through the shared buffer**, so two sides
sharing a buffer would have the second see the first's normalisation — lane
FIND's `dirAlias` rule in a different costume). The free path is now genuinely
exercised: `freeNodes=15 stringsReleased=15`.

### Defect 2 in the probe — a negative control that COULD NOT FIRE

The first version of the tie-rule control used `n=12` and compared a
small-range-ONLY `>=` variant against the FULL shipped sort. Twelve elements go
down the big-range path, and on an all-equal input both that path and the `>=`
variant happen to be the **identity permutation** — so the control could not
fire on any input, and it duly reported `*** MISSED ***`.

That is WAVE9.md's *"a control that cannot go to zero is not measuring
coverage"*, and the only reason it was caught is that the MISSED was read
instead of explained away. `n=8` keeps the whole range on the small path, where
`>` and `>=` genuinely disagree, and the control now fires.

**Both of the probe's own defects failed in the direction that looks like a
result.** One produced a crash that could have been read as a port defect; the
other produced a MISSED that could have been waved through as "the corpus is
small". Neither was a port defect.

---

## 9. Source comments corrected at their source (PLAN §6 rule 8)

| File | What was wrong |
|---|---|
| `runtime/types/object/TypeInfo.h` | `balance` at +0x48 was `int32_t` and marked **"GUESS, NOT MEASURED"**, with the node's size unknown and a "do not allocate one of these" warning. Measured: sizeof **0x50** (`0x180008440`), and +0x48 is **two 16-bit fields** — a signed balance at +0x48 and a height at +0x4a (`0x180030540`); `0x1800087c0` zeroes both with one dword store, which is why an int32 view looked right for as long as nobody rebalanced. |
| `runtime/types/io/FileSystem.h` | `PackfileIndexNode::_unused10` — *"touched by none of the three walks"* — **is the AVL parent pointer** (`0x180008840` writes it, `0x180025e10` reads it). True of the three READ walks lane PKG had; false of the tree. The mirror of `reference_never_written_needs_by_whom.md`. The node also carries balance/height at +0x28/+0x2a and is 0x30 bytes; it is a full AVL node, not a plain BST node. Both now declared with static_asserts. |
| `runtime/src/collection/array.cpp` | *"0x180051650 calls Audio's 0x180050d30 and is gated with the Wave 8 Audio holdbacks"* — **0x180050d30 is not Audio-specific at all.** It is a plain `Collection::Array<16-byte POD>::operator=`: the same body lane UIW ported at `0x18001cdf0`, with stride 0x10, **ALIGN 4** (not 8, not 0x10) and dtor `0x180051d80`, which that same file already registers. Both ported; the row and audio.cpp:1114's separate row both close. |
| `runtime/src/object/type_system.cpp` | The "DEPENDENCY THIS PORT DOES **NOT** CLOSE" block. It does now. |
| `runtime/src/io/filesystem_packages.cpp` | Four "UNPORTED"/"deliberately not ported" entries in §4 and at the call sites, plus the "these three callees are not reached on iteration 12's boot" note. |
| `runtime/src/crt/msvc_stricmp.cpp` | Carries the corrected `strcasecmp` claim from §7 rather than repeating the received one. |

---

## 10. Declined, with reasons

* **`_stricmp_l` (`0x18007ef44`) and its subsystem** — `_LocaleUpdate`
  (`0x18007ee00`), `_tolower_l` (`0x180081d0c`). Measured unreachable (§7); the
  arm aborts loudly if the premise ever breaks. Porting a CRT locale subsystem
  to satisfy a branch with no reachable call site would be work spent on
  something no evidence says runs.
* **`errno` / `_invalid_parameter_noinfo` on `_stricmp`'s null path.** The
  observable contract — the `0x7FFFFFFF` return — is reproduced; the errno write
  is not, because after cutover that errno is our CRT's and no caller in the tree
  reads it. Stated in the source rather than silently dropped.
* **`0x180004220`**, the 0x38-byte SpriteSheet element copy-assign, is the last
  row of `array.cpp`'s non-trivial tier still on the DLL. Out of scope; named.
* **`0x180026b50`** (`array.cpp`, attempt 17's wall) is not this lane's and is
  still listed unregistered by the audit at the time of writing.

## 11. Honest limits

* **S3b (the recursive free) is NOT a lockstep replay and is labelled as such.**
  It frees everything it touches, so there is no second run. What is checked is
  that ours and theirs, run on two trees the section above already proved
  identical, visit the same number of nodes and leave both roots unwound
  (`visited=15 expected=15`). That is a weaker claim than the other three and it
  is written as one.
* **The in-boot S4 corpus is small** (13 cases). It exists to prove the
  registration is reached and to keep the effectiveness counters honest; the
  784,000-case harness is the oracle for the body. `pivotFixups` was 0 until a
  65-element pseudo-random case was added specifically to reach that branch.
* **`nullNodeKey=0`** — the arm where a node's key is null while the search key
  is not never ran. Both trees' keys come from real strings. The sibling arm
  (`nullSearchKey=5`) did run. Recorded as a coverage gap, not claimed as
  verified.
* **The `coll3` log was overwritten by the `coll4` run** (`rt_run.sh` truncates
  the shared logs). The `[AvlColl]` block in §8 is transcribed from that run's
  output; the `[FactoryStartupProbe]` block in §6 is from `coll4`. Both ran the
  same exe, `943a8053d3a1226b2f796681492c0dc7`.
