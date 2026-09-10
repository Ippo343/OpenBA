# Wave 11 follow-up 3 — lane AVL

**Worktree** `C:/code/GIT/wt-w11f-avl` · **branch** `wave11f/avl`, cut from `6761773`.

Subject: `?SerializeReferenceTable@ExternalFactory@Object@@QEAAXAEAVStream@IO@@@Z`
(retail `0x1800bcb60`–`0x1800bcf38`, 292 instructions), held back since **Wave 3**
and re-held by lane OBJEXT §5.1 on one stated mechanism: it needs the
`Collection::Set<ReferenceEntry>` AVL family — the 0x10-byte-element
instantiation, node size 0x30.

**Closed. Object 50/61 → 51/61, 82.0% → 83.6%.** The remaining gap to the 88.5%
ceiling is now entirely follow-up #5's subject.

---

## 1. The headline correction to the brief

The brief named **two** missing bodies: the insert `0x18002cb90` and the
destroy-whole-tree `0x18002ebf0`.

**Only one was missing.** `0x18002ebf0` has been ported *and registered* since
Wave 11 lane OBJLIFE, as `RtObjFactoryDtor::ReferenceSubtreeFree` in
`runtime/src/object/factory_dtors.cpp:381`, registered at
`factory_dtors.cpp:499`. One grep of the address across `runtime/ code/
engine/ standalone/` found it in under a minute.

This matters beyond the saved work. `RegisterRuntimeAddr` is
**last-binding-wins**: a second registration of `0x18002ebf0` from this lane
would have silently made one of the two bodies dead code, and nothing in the
build, the boot or the census would have said which one was running. That is
`reference_grep_for_the_port_before_writing_it.md` for the fourth time.

**The second correction is bigger and is about scope.** The brief and OBJEXT
§5.1 both frame the AVL family as what the *function* needs. Reading the
disassembly says otherwise:

| | needs the Set family? | needs the teardown? |
|---|---|---|
| **WRITE path** (`0x1800bcdf4`–`0x1800bcf11`) | **NO** — it never inserts. It walks `_resources` in order and emits. | yes |
| **READ path** (`0x1800bcbe5`–`0x1800bcdef`) | yes, one call per row to `0x18002cb90` | yes |

So the half of this function that decides the *save layout* — the half whose
failure mode is a silent save desync — was never blocked by the AVL family at
all. It was blocked by nobody having read it.

---

## 2. Per-row verdicts

| row | verdict | oracle |
|---|---|---|
| `?SerializeReferenceTable@ExternalFactory@Object@@QEAAXAEAVStream@IO@@@Z` `0x1800bcb60` | **PORTED + VERIFIED (write path), PORTED + VERIFIED BY A/B (read path)** | §4.1 and §4.3 |
| `Set<ReferenceEntry>::Insert` `0x18002cb90` | **PORTED + VERIFIED** | lockstep synthetic-subject probe, §4.2 |
| `Set<ReferenceEntry>::InsertCore` `0x18002d180` | **PORTED + VERIFIED (transitively)** | same probe — the shipped `Insert` reaches it as an in-DLL near call |
| `Set<ReferenceEntry>::Retrace` `0x18002d4b0` | **PORTED + VERIFIED (transitively)** | same |
| `Set<ReferenceEntry>::RotateLeft` `0x18002dac0` | **PORTED + VERIFIED (transitively)** | same, `rotL=12` |
| `Set<ReferenceEntry>::RotateRight` `0x18002dbf0` | **PORTED + VERIFIED (transitively)** | same, `rotR=14` |
| `Set<ReferenceEntry>::AllocNode` `0x18002e5d0` | **PORTED + VERIFIED (transitively)** | same |
| `Set<ReferenceEntry>::PairCtor` `0x18002e570` | **PORTED + VERIFIED (transitively)** | same |

Eight bodies. **Seven written by this lane**; the eighth (`0x18002ebf0`) was
already in the tree, §1.

"Transitively" is stated, not implied: this lane's probe drives **one** entry
point, `0x18002cb90`. The shipped body reaches the other six as in-DLL `rel32`
calls that nothing of ours can intercept, so driving the shipped insert
exercises the shipped six as one unit against our seven.

---

## 3. The destroy/free shape, established from the disassembly

The brief's specific warning — that lane OBJLIFE found the three Object subtree
frees genuinely differ, that writing one as "free the block" leaks and writing
another as a symmetric release double-frees — is about `0x18002ebf0`, and it had
already been acted on. Both readings were **re-checked against the disassembly
for this lane** and both stand:

| body | node | children | RELEASES | ported in |
|---|---|---|---|---|
| `0x18002ebf0` `_references` | 0x30 | `+0x18` / `+0x20` | **`+0x08`** — the `Filename` VALUE | `factory_dtors.cpp` |
| `0x180008340` (sibling) | 0x30 | `+0x18` / `+0x20` | **`+0x00`** — the key, and explicitly NOT `+0x08` | `avl_trees.cpp` |
| `0x18002eea0` `_resources` | 0x38 | `+0x20` / `+0x28` | `+0x00` only | `factory_dtors.cpp` |

Same node size, same child offsets, **different released slot**, because in one
instantiation the `String` is the key and in the other it is the value. A
"tidied" symmetric release in either is a heap corruption.

**Nothing this lane wrote destroys a tree.** The seven new bodies are the insert
side. The only release any of them performs is `Insert`'s own
(`0x18002cb90 @ 0x18002cbda`): it drops the reference its 0x10-byte stack
temporary took, **on every path including the equal-key path where nothing was
inserted**. Skipping it there leaks one `Filename` reference per repeated id.
The probe's `equalHit=4` is what makes that path non-hypothetical.

---

## 4. Oracles

### 4.1 The write path — full lane-S emission replay, `stopAfter = 0`

`docs/runtime_port/WAVE8_5_PREFIX_MEASUREMENT.md` had already measured the
prefix stop `WAVE8_5.md` proposed for this subject: **2 emissions out of
`3 + 3N`**, ~0.012%, and the two are the codec factory and the entry count — a
header check wearing an oracle's clothes. The same measurement found the
premise wrong in our favour: the write path's mutations are **idempotent and
value-identical** under replay, so the FULL compare is safe. This lane took that
recommendation as written.

```
[RtSaveTrace] pass Object::ExternalFactory::SerializeReferenceTable:
              17052 emissions identical
```

`17052 = 3 + 3 × 5683`, which is the measurement's formula at the run's actual
`N`. Ours and theirs each emit into their own recording sink; the adapter
recovers the `Stream*` from `Serializer+0x00`, because **this subject takes a
`Stream&` and lane S's first subject took a `Serializer&`**.

**Two negative controls, scored separately, each shown to catch via the
mechanism it names.** Both were injected as a one-line edit, built, run, and
reverted:

| control | mechanism it claims to test | result |
|---|---|---|
| **A** — emit the entry count through `kScalarUInt` (+0x98) instead of `kScalarInt` (+0xa0) | **slot identity** | **CAUGHT 1/1**: `DIVERGE at emission 1: ours uint(+0x98)=5683 theirs int(+0xa0)=5683`. Same value on both sides — the catch is *purely* the slot. |
| **B** — start the id counter at 2 instead of 1 | **emitted values** | **CAUGHT 1/1**: `DIVERGE at emission 2: ours uint(+0x98)=2 theirs uint(+0x98)=1`. Same slot on both sides — the catch is *purely* the value. |

The two controls discriminate: neither could have been caught by the mechanism
the other names.

### 4.2 The Set family — lockstep synthetic-subject probe

Two private `Set`s, one script, ours through the ported bodies and theirs
through `ResolveRuntimeRaw(0x18002cb90)` — **raw**, because `ResolveAddr`
consults the port registry first and would have compared ours with ours.
Compared after **every** step on shape, balance, height, parent linkage and the
element payload, because AVL defects cancel and a final-state comparison would
miss a rotation that is wrong twice.

```
[RefSet] LOCKSTEP PROBE: 35 steps, 0 shape/payload mismatches, 0 parent-linkage failures
[RefSet]   rootInsert=1 descLeft=14 descRight=16 equalHit=4 cmpLt=74 cmpEq=4 cmpGt=64
[RefSet]   rotL=12 rotR=14 preRotL=3 preRotR=3 retraceIters=124 allocFail=0
[RefSet]   NEGATIVE CONTROLS, scored separately:
           balance 3/3  height 3/3  id 3/3  path 3/3  parent 3/3
[RefSet]   VERDICT: the seven ported bodies agree with the shipped 0x18002cb90
           family at every step.
```

Every rebalance case fired, including both double rotations. Five controls, each
run three times, each scored on its own, each reversible.

**The population is adversarial on purpose.** The live caller only ever inserts
`1..N` ascending, which exercises one rotation case and never the equal-key
path — a population that cannot fail. The script forces ascending, descending,
both zig-zags, repeats, and the ids `0 / 0x7fffffff / 0x80000000 / 0xffffffff`.
That last group is not decoration: **the comparator is `JL`/`JGE`, a SIGNED
32-bit compare, on a field declared `uint32`**, so a port that used unsigned `<`
would order `0x80000000` differently. Nothing the live caller can produce
reaches that, which is exactly why the probe must.

### 4.3 The read path — an A/B against retail, on the same save

The emission oracle drives the write path (the sink is always in the writing
direction), so it says nothing about the read path. The read path consumes the
stream and cannot be replayed. It *can* be A/B'd, because its whole product is a
table that a second, independently-ported function queries 5,683 times
immediately afterwards:

| run | `refTable` resolved to | `[PreloadRefs]` |
|---|---|---|
| ours (`BANISHED_RT_REFTABLE` unset) | `0x7ff7259e19e4` — **exe** | `preloaded 5682/5683 referenced externals (0 not found)` |
| theirs (`BANISHED_RT_REFTABLE=0`) | `0x7fff975ecb60` — **DLL** | `preloaded 5682/5683 referenced externals (0 not found)` |

The kill switch demonstrably moves the resolved pointer between the exe and the
DLL, so this is a real A/B and not a self-check
(`reference_shadow_marker_is_not_an_oracle.md`).

Our read path's own counters on that run:

```
[RefTable] calls=2 save=1 load=1 entries=11366 teardownNonEmpty=0
           resFound=5678 resMissed=5 nullSearchKey=0 nullNodeKey=0
           nullEncoding=0 nullRw=0
```

`entries=11366 = 5683 write + 5683 read`. `resFound=5678` is the inlined
`_stricmp` descent over `_resources` hitting an already-loaded resource;
`resMissed=5` are the five paths in the save that were not loaded at that
moment. The game then ran in-world with an empty crash log.

**What this A/B cannot say**, stated because a partial oracle that reads like a
full one is worse than none: it compares an *outcome over 5,683 queries*, not
the two trees node for node. Two tables that differ only in a path
`FindOrLoad` also fails to resolve would look identical to it. It does not cover
the `nullSearchKey` / `nullNodeKey` arms (both read 0 — the live population has
no null paths), and it does not cover a non-empty `_references` at entry
(`teardownNonEmpty=0`, because this was the session's first load).

### 4.4 MUTATION A — the check the measurement asked for

The `_references` teardown **emits nothing**, so a port that dropped it or freed
the wrong slot would sail through a perfectly clean 17,052-emission trace. The
measurement document asked whoever ported this to cover it with an explicit
post-condition instead of trusting the trace, and
`external_reftable_probe.cpp:RtObjectExternalRefTableNoteTeardown` is it: it
counts the tree on entry and asserts root and count are zero afterwards. No
failure line appeared on any run.

**What that check can and cannot say**: it can say the body called a teardown
with the right set pointer and zeroed the header. It **cannot** say the teardown
released the right slot — that is `0x18002ebf0`'s own property, verified by
`factory_dtors_probe.cpp`. This lane does not re-verify it and does not claim to.

---

## 5. Census — set-diff first, percentages second

Run from inside this worktree with the project venv's interpreter, against a
scratch worktree at the base commit `6761773` for the "before" side (a `git
stash` measures nothing once the work is committed — the first attempt did
exactly that and produced an empty diff that looked like "your change did
nothing").

```
 Object              191       61       50 ...  82.0%   88.5%     <- 6761773
 Object              191       61       51 ...  83.6%   88.5%     <- this branch
 internal                     158      124 ...  78.5%
 internal                     159      125 ...  78.6%
 TOTAL              4632     1163      936 ...  80.5%
 TOTAL              4632     1164      938 ...  80.6%
```

**Set-diff, which is the number to trust:**

* `ported` **+2**: the export `?SerializeReferenceTable@…` (Object) and the
  address `0x18002cb90` (internal).
* `called` **+1**: `0x18002cb90`. **Nothing entered `called` unported.**
* six new caller-less **address** rows: `0x18002d180`, `0x18002d4b0`,
  `0x18002dac0`, `0x18002dbf0`, `0x18002e570`, `0x18002e5d0`.

**Against the pre-registration.** Before running it I predicted "`ported` +1
(the export); `called` gains at most the subject's own mangled name, and no new
*unported* names". Two of the three were right and one was wrong in a way worth
recording: the growth in `called` was **not** the subject's exported name (the
probe's `ResolveExportRaw` literal did not move the census) but the *unexported
address* `0x18002cb90`, which the read path reaches through the registry. The
predicted direction and magnitude held; the predicted mechanism did not.

**The six caller-less address rows are a census artefact, not six dead ports.**
Our `Insert` calls `InsertCore`, which calls `Retrace`, which calls the
rotations, all by **C++ symbol** — the census can only see a caller that goes
through a resolve door. This is the known blind spot, and it is why the
lockstep probe's transitive coverage claim in §2 is the evidence for those six,
not the census column.

---

## 6. Two things this lane fixed that were not its subject

1. **A stale registration banner.** `external.cpp` printed
   *"Clean/FindOrLoad/SerializeReferenceTable/SaveText deliberately NOT
   ported"* on every boot. All four had been ported — three of them by earlier
   waves — and a banner like that is precisely how a lane gets sent to write
   what the tree already has. Corrected to name where each of the four now
   lives.

2. **An instrument that could only print at exit.** The first clean-exit run was
   made with `BANISHED_AUTOQUIT`, which **overrides** `BANISHED_AUTOLOAD`'s
   path: the driver quit at the menu, the load never happened, and both new
   instruments printed a plausible **zero** on a run that looked healthy
   (`reference_autoquit_overrides_autostart_newgame.md`, hit exactly as
   written). The summary now also prints **from inside the body**, so a run that
   has to be killed can still be read; and the exit-summary text says in as many
   words that a zero there is not a pass.

---

## 7. What remains

* **`ExternalFactory::_references` has no ported `Find` and no ported `Erase`.**
  Deliberate. `GetPathForReferenceId` (`0x1800bf1d0`) inlines its own descent
  and is already ported in `external.cpp`; nothing in the DLL erases a single
  reference entry — the table is torn down wholesale and rebuilt. Porting an
  `Erase` with no caller would be a body no oracle in this tree can reach.
* **The read path's two null-key arms are untested** (`nullSearchKey=0`,
  `nullNodeKey=0` on the live run, and the lockstep probe drives the *reference*
  tree, not the inlined `_resources` descent). They are transcribed from the
  flags and their asymmetry is documented at the call site; they are not
  verified.
* **A non-empty `_references` at entry is untested** (`teardownNonEmpty=0`). It
  needs a second load in one session.
* **No `save` route in the harness.** The write path was verified with the
  emission oracle, which does not need one; but the real save-to-disk path
  through this body has not been exercised end to end. `BANISHED_AUTOLOAD` is
  the only save/load route the unattended harness has.

## 8. Switches

```
BANISHED_RT_REFSET=0     leave the seven Set<ReferenceEntry> address doors on the DLL
BANISHED_RT_REFTABLE=0   leave SerializeReferenceTable on the DLL
BANISHED_RT_SAVE_TRACE=1 arm the write-path emission replay (§4.1)
```

Both nested inside `BANISHED_RT_EXTERNAL`. Two switches rather than one because
the halves carry different risk, and `BANISHED_RT_REFTABLE=0` with the Set
family left ours is the A/B that separates "the container is wrong" from "the
driver is wrong".
