# WAVE 11 — the Object namespace to its ceiling

**Dispatched 2026-09-09 from `wave10/base` @ `fa4fa86`.**
Goal stated by the user: drive the **Object** namespace to 100% of `called`, or
prove its ceiling.

## The scoping correction this wave opens with

`WAVE11_KICKOFF.md` framed the wave as **the object creation path — 202
`Create*Object` exports, zero ported**. That framing is not wrong about the
cutover, but it is **not the goal the user asked for, and it does not move the
Object census at all**: those 202 exports are `called = 0`. The census
denominator is the set our tree *calls*, so porting a factory nobody calls
changes no percentage. Its payoff is the `??_7Dialog` promotion, which is one
row in **UI**, not in Object.

The Object number the user asked about:

```
namespace   total   called   ported   covered
Object        191       56       32    57.1%
```

**24 rows are the whole gap.** They are enumerated below, measured with
`tools/runtime_census.py --namespace Object --unported` on `fa4fa86`.

## The decomposition — 24 rows, three classes

Counted, not asserted. 10 + 5 + 9 = 24.

### Class A — minted DATA rows, none promoted (10)

Every one of these is already generated into
`standalone/rt_data_mint_tables.inc`; **not one appears in
`standalone/reg_mint_promoted.cpp`.** So the work is *promotion under the Wave
10 MINTSEL protocol*, not authoring.

| row | kind |
|---|---|
| `??_7Base@Object@@6B@` | vtable |
| `??_7CompiledExternal@Object@@6B@` | vtable |
| `??_7External@Object@@6B@` | vtable |
| `??_7Internal@Object@@6B@` | vtable |
| `?_firstTypeInfo@BaseTypeInfo@Object@@2PEAV12@EA` | chain head (see retraction below) |
| `?_typeInfo@External@Object@@0VBaseTypeInfo@2@A` | BaseTypeInfo |
| `?_typeInfo@ExternalList@Object@@0VBaseTypeInfo@2@A` | BaseTypeInfo |
| `?_singleton@Factory@Object@@0PEAV12@EA` | storage |
| `?_singleton@ExternalFactory@Object@@0PEAV12@EA` | storage |
| `?_singleton@InternalFactory@Object@@0PEAV12@EA` | storage |

The three `_singleton@` rows are **Wave 15 phase 2 subjects** and inherit its
doctrine verbatim (`WAVE15_CLOSED.md` §"Phase 2"): UNKNOWN-OWNER counts as
unported; a promotion must register the **address as well as the name**;
`ExternalFactory@Object` is explicitly flagged **NOT cheap tier** (127 of 151
references are unknown-owner).

#### RETRACTED — the `_firstTypeInfo` "split brain" was backwards

This brief, inheriting the line from `WAVE11_KICKOFF.md`, told lane OBJDATA that
`Factory::Startup` resolving `?_firstTypeInfo@` **by name** "walks the DLL's
chain, not the minted one", and called that a split brain to be repaired.

**That is the wrong way round. Resolving by name is what PREVENTS the split.**
Every `?_typeInfo@` static's constructor head-inserts itself into that one slot,
and `Object::Factory::Startup` walks that one chain — so a by-name resolve gives
**one merged chain** of both our minted nodes and the DLL's. *Promoting* the row
is what would break it into our half and theirs, with retail's `Factory::Startup`
walking the wrong one.

Confirmed by a route independent of the lane that reported it:
`tools/rt_mint_gen.py` §"CLOSURE 2: THE REFLECTION CHAIN IS NOT A SUBSET"
(Wave 9) states the head-insert mechanism and **measured** it — 415 nodes / 413
parentNames / 0 unresolved with the DLL loaded.

The error is kept here rather than deleted: it conflated *the slot belongs to the
DLL* with *the contents belong to the DLL*, and it would have justified exactly
the promotion that breaks the chain.

### Class B — lifecycle (5)

`??0Internal`, `??1Internal` (`UEAA`, virtual), `??1Factory`,
`??1ExternalFactory`, `??1InternalFactory`.

### Class C — real bodies (9)

`ExternalFactory`: `FindOrLoad`, `GetCompiledPath`, `PushPath`, `PopPath`,
`SerializeReferenceTable`.
`External`: `Load`, `SaveText`.
`InternalFactory`: `ClearPersistState`, `ResetPersistState`.

## Lanes

| lane | worktree | branch | subject | rows |
|---|---|---|---|---|
| OBJDATA | `C:/code/GIT/wt-wave11-objdata` | `wave11/objdata` | Class A, mint promotion | 10 |
| OBJLIFE | `C:/code/GIT/wt-wave11-objlife` | `wave11/objlife` | Class B, lifecycle | 5 |
| OBJEXT | `C:/code/GIT/wt-wave11-objext` | `wave11/objext` | Class C, External/ExternalFactory | 7 |
| OBJINT | `C:/code/GIT/wt-wave11-objint` | `wave11/objint` | Class C, InternalFactory + the Dialog-factory probe | 2 + probe |

## Standing rules for every lane

Inherited doctrine, restated because each has cost this project a wave:

1. **Grep for the port before writing it.** Three times in one wave a lane ported
   what the tree already had. Check both the mangled name *and* the address.
2. **Port the CHECK before the code.** A written recipe from a good lane can be
   wrong; a dtor that only frees the block instead of destroying every element
   is a leak under a false byte-identity claim.
3. **`calls=0` is four different findings.** Read the oracle's `calls=` column,
   and if it is zero say *which* of the four causes you established.
4. **A green readiness screen is not a route.** SAFE-to-stamp ≠ anything
   dispatches it. Only an attendance counter separates them.
5. **Do not "fix" a faithful crash with a null check.** Find the upstream null.
6. **The game directory is a mutex.** `tools/game_dir_lock.sh acquire <lane>` —
   **never piped**, a pipeline returns the last command's status and a timeout
   then reads as a grant.
7. **Report the set-diff, not the count.** A closure count moving the right
   direction by the wrong amount reads as success.
8. **Stay in your own worktree.** Never `git checkout`/`switch` a branch.

## Corrections earned mid-wave (advisor, from lane OBJDATA's close)

1. **`runtime_census.py` resolves its scan roots SCRIPT-relative** —
   `REPO = dirname(dirname(abspath(__file__)))`, line 40. Running the main
   checkout's copy from inside a lane worktree measures the **main checkout** and
   returns the baseline, i.e. "my change did nothing". Measure a lane branch with
   *that worktree's own copy* of the script. This is the **opposite** of the
   standing memory about the runtime-port source scanners walking cwd; the two
   tools differ and both failure modes print a confident wrong number.
2. **The reference-ownership table is necessary and NOT sufficient — read the
   `minted=` column first.** `?_singleton@InternalFactory@Object@@` scores as the
   cheap SaveLoad shape on the Wave 15 reference screen (6 refs, 5
   ported+registered, 1 unported reader provably dead) and is still HARMFUL to
   promote: `DLL=WRITTEN minted=zero`, because the factory is constructed every
   boot by an **in-DLL rel32 from `Engine::Startup`**. Our ported ctor never
   runs, minted storage stays zero, and promotion hands every reader a null.
   This amends `WAVE15_CLOSED.md` §"Phase 2", which lists
   `InternalFactory@Object` as a lead cheap candidate on the reference count.
3. **An attendance counter sampled from a static destructor is a false negative
   made of timing.** OBJDATA's `[ObjTI] ATTENDANCE` first printed after port-
   registry teardown, so every row read `unregistered(DLL)` and the armed half of
   the A/B was indistinguishable from the control — it would have been written up
   as "the row changes nothing". Its own positive control caught it. Any
   attendance instrument must be sampled before teardown and must carry a control
   that is required to read nonzero.

## The scoping error that produced three wrong lane premises — ONE cause, not three

**This wave's briefs mis-scoped three separate row assignments, and the cause is
a single misreading by the advisor.**

`runtime_census.py --unported` lists what is *called but not ported*. That means
**"no UNCONDITIONAL registration exists"** — because this project's own rule is
that a gated registration is not a port. The brief read it as **"no body
exists"**, and dispatched lanes to author code the tree already had.

Measured instances, all confirmed by the advisor from the source before adoption:

| rows | what the census said | what is actually there |
|---|---|---|
| `??0Internal`, `??1Internal` (OBJLIFE) | unported | ported since Wave 3, registered by **name and address**, `internal.cpp:1020-1026`, inside `if (table)` |
| `?ClearPersistState@InternalFactory`, `?ResetPersistState@InternalFactory` (OBJINT) | unported | same block, `internal.cpp:1028-1036`; the file's own banner names them **"STILL UNEXERCISED"** |
| the four `??_7…@Object` vtables (OBJDATA) | minted, "so promotion not authoring" | the rows were generated but the **slots were empty** — seven `?Type@`/`?TypeName@` bodies had to be written |

The gate is `BANISHED_RT_INTERNAL_TABLE`, with `|| noDll` so it auto-arms at
cutover.

**The rule this yields:** a census `unported` row has at least four distinct
meanings — no body; a body behind a gate; a body never registered; a row whose
*storage* is never written (see `minted=`, above). They imply completely
different work, and only the last two are "porting". **Grep for a gated
registration before assigning authoring work**, i.e. for `RtOptIn(`, `if (table)`,
`BANISHED_RT_*` near the registration block — not merely for the symbol.

This is the same family as the standing lesson that a body PORTED but never
REGISTERED is indistinguishable from an ABSENT one to a callee census. Here the
indistinguishable thing is a body ported, registered, and *gated*.

## Lane collision created and resolved

`??0Internal`/`??1Internal` (OBJLIFE) and both `InternalFactory` persist-state
rows (OBJINT) live in **the same `if (table)` block, behind the same env var**.
Resolved in OBJLIFE's favour — it was already in the file with an attendance
counter. OBJINT keeps the *exercise* verdict on its two rows and routes any
instrument it needs through OBJLIFE's version. Arming the env var at runtime is
not a conflict; only source edits are.

## Shared-resource note: the game directory serialises the wave

Four lanes, one game directory, several boot runs each. OBJLIFE lost a 900s
blocking acquire to the queue. Standing instruction issued: **acquire once and
batch every run under a single hold**, never acquire/release per run.

Two real defects in `tools/game_dir_lock.sh`, found by OBJLIFE, deliberately NOT
fixed mid-wave (changing shared tooling under three running lanes is worse than
the bugs):

1. `line 293: [: : integer expression expected` on every queue poll — cosmetic.
2. **Blocking `acquire` returns non-zero on timeout, but a wrapping shell
   function or `if` returns 0.** This is the documented pipe hazard *one layer
   out* — same class as the incident where a piped acquire made a timeout read as
   a grant, and it has the same consequence: a lane deploys over another mid-run
   believing it holds the lock.

## ADVISOR ADJUDICATION — the OBJLIFE/OBJDATA vtable deadlock

Lane OBJLIFE raised a cross-lane merge hazard neither lane could see from its own
side. It is real, it is **latent rather than live**, and the two lanes' exit
conditions are **mutually triggering** — which is the part that needed an
advisor and not a lane.

### The hazard

`internal.cpp`'s ported `??0Internal`/`??1Internal` stamp the vptr via
`VtblInternal()` / `VtblBase()`, which reach `??_7Internal@Object@@6B@` /
`??_7Base@Object@@6B@` through **`ResolveExport`, which consults the port
registry FIRST**. So if those minted rows are ever promoted *while the TABLE
tier is armed and the DLL is present*, objects built by our ctor carry the
minted table and objects built by the DLL's in-DLL `rel32` callers carry
`0x180154720` — **two vtables for one class**, PLAN.md §4.3 risk 3. It is the
exact condition `internal.cpp`'s own header bullet denies ("There is ONE vtable,
and it is THEIRS"): that bullet is *conditionally* true and its condition is
invisible at the site where it is asserted.

### Verified state — the hazard is NOT live on this wave's merge

Checked by the advisor on `wave11/objdata`, not taken from either lane's report:

```
git grep 'RegisterRuntimeExport("??_7(Internal|Base|External|CompiledExternal)@Object'  ->  ZERO
```

All five occurrences of those symbols in `reg_mint_promoted.cpp` are **comments**
(an `RT_UNPROMOTABLE` tag and a readiness note). OBJDATA promoted two
`?_typeInfo@` rows and no `??_7@Object` row at all. **Trigger requires both
halves and only one exists, so arming the TABLE tier is safe on this merge and
stays safe.**

### The deadlock, and the amendment that breaks it

The two lanes' conditions form a cycle:

* **OBJDATA** left `??_7Internal@Object@@6B@` READY with
  `EXPIRY: the moment [ObjTI] ATTENDANCE prints entries>0`.
* The **only** thing that makes that counter move is our ported `??0Internal`
  running — which requires **OBJLIFE's TABLE tier armed**.
* And arming + promoting is precisely the hazard.

So a future lane reading OBJDATA's expiry literally, after OBJLIFE arms, walks
straight into the split vptr. **OBJDATA's expiry is INCOMPLETE: it names a
necessary condition and reads as a sufficient one.** Same family as this
project's standing lesson that a prescription gets read as a completion.

**AMENDED, and binding on any future lane:**

> `??_7Internal@Object@@6B@` and `??_7Base@Object@@6B@` may be promoted only when
> BOTH hold: (a) `[ObjTI] ATTENDANCE … entries>0`, **and** (b) `VtblInternal()` /
> `VtblBase()` pin to the RAW DLL symbol while the DLL is present, falling back
> to the registry only when it is absent. Attendance alone is NOT sufficient.

### Decisions

1. **The hazard does not justify holding back rows 1-2.** OBJLIFE's instinct was
   right and its reasoning was right; the premise ("the neighbouring lane lands a
   promotion") is false on the actual merge. The arming decision stands or falls
   on its own run evidence, which is what the lock window is for.
2. **The raw-pinning fix is NOT to be made mid-wave.** OBJLIFE was correct to
   flag rather than do it: it decides what cutover sees. It is the wave's top
   follow-up, and it needs its own verification against the cutover boot gate.
3. Condition (b) above is recorded here because `reg_mint_promoted.cpp`'s tag
   lives on OBJDATA's branch and OBJDATA has stopped work.

## `game_dir_lock.sh` — root cause of the queue-poll error, and the fix to make at close

Confirmed by reading the source, not inferred from the message. `pick_next()`:

```sh
asked=$(cat "$f" 2>/dev/null || echo 0)
if [ -z "$best_at" ] || [ "$asked" -lt "$best_at" ]; then
```

`|| echo 0` fires only when `cat` **fails**. A ticket file that exists but is
**empty** makes `cat` succeed with empty output, so `asked=""` and the numeric
comparison throws `line 293: [: : integer expression expected`. Harmless to the
ordering (the branch is skipped) but it prints on every poll and trains lanes to
ignore the script's stderr — which is where a real refusal would appear.

Fix at close: `asked=$(cat "$f" 2>/dev/null); [ -n "$asked" ] || asked=0`, plus a
digits-only guard. Same treatment for the `age` line above it.

The second defect is the dangerous one and is unchanged: **blocking `acquire`
returns non-zero on timeout, but a wrapping shell function or `if` returns 0.**
That is the documented pipe hazard one layer out, with the same consequence — a
lane deploys over another mid-run believing it holds the lock.

### The field that saved this wave a window

`status` prints `holder activity: reimpl-x64.exe processes=N`. With **`N=0`** and
the lock held 591s, the holder was demonstrably sitting on the directory through
*analysis*, not a run. That is what let the advisor intervene on evidence rather
than on suspicion. **Keep that field, and read it before arbitrating a queue.**

## Advisor error: arbitrating a queue that already had an order

The advisor told OBJEXT it was first in the queue, ranked by "closest to done",
**without checking that the lock's own queue already had an order**. OBJLIFE was
at the head with ~630s invested against a 900s timeout and needed a ~30s run;
honouring the advisor's order would have discarded that wait and probably cost it
a second full window, for no gain. Retracted to both lanes within minutes.

**Rule: a shared resource with its own queue does not need an external
scheduler — read the queue before overriding it.** Rank by "closest to done" only
to break a genuine tie.

## Integration pre-check (advisor, done during the lock wait — needs no lock)

### The merge is one file, and the conflict is textual only

`git merge-tree` on all three pairs, non-destructive:

| pair | conflicts |
|---|---|
| objdata + objlife | `runtime/src/object/rt_object_port.cpp` (`standalone/main.cpp` auto-merges) |
| objdata + objext | `runtime/src/object/rt_object_port.cpp` |
| objlife + objext | `runtime/src/object/rt_object_port.cpp` |

Every lane adds a **purely additive, disjoint block** to that file — its own
`extern …Register()` declaration and its own switch. **No two lanes register the
same symbol**, checked by extracting the added `RegisterRuntime*` calls per
branch. So the resolution is "take all three blocks", and the standing hazard
that a duplicate registration is last-binding-wins and silently makes one body
dead code **does not apply here**. Confirm again on the real merge, since two
lanes are still moving.

### The gate semantics that make these rows real ports — checked, not assumed

All three lanes wrap their registrations in `RtWaveEnabled("BANISHED_RT_*")`.
Given that this wave's own headline finding is that **a gated registration is not
a port**, that had to be verified rather than trusted. `runtime/funcs/rt_port.h:114`:

```cpp
inline bool RtWaveEnabled(const char* envName) {
    const char* e = std::getenv(envName);
    return !(e && (std::strcmp(e, "0") == 0 || std::strcmp(e, "false") == 0));
}
```

**Default-ON kill-switch.** So the two gate helpers mean opposite things and the
distinction decides whether a row counts:

| helper | default | counts as a port? | used by |
|---|---|---|---|
| `RtWaveEnabled("X")` | **ON** (kill-switch, `X=0` disables) | **YES** | all three lanes' new work |
| `RtOptIn("X")` | **OFF** (opt-in) | **NO** | `BANISHED_RT_INTERNAL_TABLE`, the tier-2 block |

This is why the tier-2 rows (`??0Internal`, `??1Internal`, `ClearPersistState`,
`ResetPersistState`) read unported while the lanes' new rows legitimately move
the census. **Read which helper wraps a registration before believing either a
census gain or a "this is already ported" claim.**

## ADVISOR ERROR — "SA" is a calling convention, not a return shape

Lane OBJEXT corrected the brief and it is right. The brief said:

> `?GetCompiledPath@` and `?Load@` are `SA` — statics. … **Get the ABI right for
> the two by-value-return rows.**

**`?Load@` is not a by-value return.** The two facts live in different parts of
the mangled name and the brief conflated them:

```
?Load@External@Object@@ SA PEAV12@ AEBVFilename@System@@ PEAV12@ @Z
                        ^^ static   ^^^^^^^ RETURN External*  -> RAX, NO hidden slot

?GetCompiledPath@ExternalFactory@Object@@ SA ?AVFilename@System@@ AEBV34@ @Z
                                          ^^ static ^^^^^^^^^^^^^ RETURN BY VALUE -> hidden slot
```

**The rule:** the letters after the class list (`SA`, `QEAA`, `AEAA`, `UEAA`)
give **linkage and calling convention only**. The return shape is the token that
follows them: `?AV…` / `?AU…` is a **by-value class return and takes a hidden
first parameter**; `PEAV…` is a **pointer return in RAX and takes none**.
Applying the by-value rule to `?Load@` would insert a phantom first parameter —
the mirror image of the standing hazard where an `fn_*` typedef *omits* the
hidden slot and becomes a wild write that still returns the right answer.

Of the seven OBJEXT rows exactly **one** (`GetCompiledPath`) has a hidden slot.

Second finding from the same lane, worth carrying: `FindOrLoad` calls the
**private** `?Find@…@AEAA…` (`0x1800be0e0`), not the public `Find` that
`external.cpp` ports — and `external_find.cpp` already registers that private
name. So `called` grew by **one**, not three. Reading `AEAA` as "unexported,
reach it by address" would have opened an address door **onto our own ported
body** — a fresh instance of the standing lesson that
**`AEAA` means private, not unexported**, this time with the trap pointing at us.

## CORRECTION — the queue-poll error is NOT harmless; it is the starvation itself

This document said of `pick_next()`/`ticket_head()`'s empty-ticket bug:

> Harmless to the ordering (the branch is skipped) but it prints on every poll…

**That is wrong, and it cost lane OBJLIFE two 900s windows.** Line 293:

```sh
asked=$(cat "$f" 2>/dev/null || echo 0)          # 292 -- `|| echo 0` fires only if cat FAILS
if [ -z "$best_at" ] || [ "$asked" -lt "$best_at" ]; then   # 293 -- throws when asked=""
```

`ticket_head()` returns **the waiter with the earliest `asked` epoch**. When a
ticket file exists but is **empty**, `cat` succeeds with empty output, the
numeric test throws, and the `if` is false — so **that waiter is never selected
as head.** Skipping the branch is not a cosmetic skip: it is the selection.
A lane with an empty ticket can never reach the head of the queue while any
other lane holds a valid ticket, no matter how long it has waited.

**This exactly reproduces what OBJLIFE observed**, and the transition is the
signature:

```
[gamelock] waiting for 'objint'  (780s/900s)   <- head EMPTY (own ticket skipped) -> falls through
[gamelock] queued behind 'objext' (810s/900s)  <- objext queues with a VALID ticket -> becomes head
```

While OBJLIFE was alone, its skipped ticket left `head` empty, `[ -n "$head" ]`
was false, and it fell through to the "waiting for <owner>" branch — which reads
exactly like being at the head. The moment a lane with a readable ticket queued,
that lane became head and OBJLIFE was displaced **permanently**, not transiently.

### OBJEXT is exonerated

The reorder was **not** a lane jumping the queue, and not a lane acting on the
advisor's earlier bad ranking. OBJLIFE flagged this itself — *"my evidence points
at the script rather than at OBJEXT… worth checking before attributing it to a
lane"* — and it was right to. The advisor's retraction had nothing to revert.
**Attributing a scheduling anomaly to a peer before reading the scheduler is its
own failure**, and the lane that stood to gain from the accusation is the one
that prevented it.

### A second, independent unfairness in the same function

`ticket_drop` on timeout removes the ticket; the replacement `acquire` calls
`ticket_take`, which writes a **fresh** `asked = now()`. So **a timed-out waiter
returns to the BACK of the queue**, having already waited the longest. That is
the precise failure the ticketing system was introduced to fix (see the header
comment: lane UI3 lost a slot after waiting an hour) — reintroduced through the
timeout path.

### The fix, for the wave close

```sh
asked=$(cat "$f" 2>/dev/null); case "$asked" in ''|*[!0-9]*) asked=0 ;; esac
```

plus the same digits-guard on `age`, and a timeout path that preserves the
original `asked` epoch rather than minting a new one. **A queue that silently
starves its longest waiter is worse than no queue**, because the waiter reads its
own starvation as patience.

## A knowledge-loss defect: locally re-declared prototypes strip the caveats

Lane OBJEXT derived, from the disassembly, that
`?GetPathFilename@Filename@System@@QEBA?AV12@_N@Z` (`0x1800c6740`) **does not
mean "strip the extension"**: the backward scan for `'.'` strips it
unconditionally, and the flag is read in exactly one place — when no `'.'` is
found at index >= 1, a true flag yields the **EMPTY** filename rather than the
whole leaf. The derivation is strong: **one fact predicts all five of its control
ratios exactly** (6/6, 6/6, 4/6, 1/6, 5/6), and it is corroborated by lane RRES's
`filenameFromStack` counter, which exists precisely to handle that empty return.

**And the tree already knew.** `runtime/src/system/filename.cpp:282-288` — the
port of that very function — documents it in almost the same words, with the
parameter named `stripExt` and the caveat attached.

So a lane spent a cycle re-deriving a fact already written down at the callee's
own port. The mechanism is the local re-declaration:

```cpp
// external_path_win.cpp, BEFORE — "Declared locally, house style"
Filename* RtSystem_Filename_GetPathFilename(const Filename* self, Filename* ret,
                                            bool stripExtension);   // 0x1800c6740
```

The house style of re-declaring a callee locally (to avoid dragging in a header)
**carries the signature and drops everything that makes it usable** — here a
retail-flavoured parameter name with no caveat, sitting two call sites away from
code whose behaviour depends entirely on the caveat. It did not merely fail to
help; it actively misled, because `stripExtension` is a confident-sounding wrong
name.

**Fixed at source** (the advisor's to fix — no lane owns this file and no lane
branch touches it): the parameter is now `emptyIfNoExt`, with the semantics, the
reason the `filenameFromStack` fallback exists, and a pointer to
`filename.cpp:282-288`. It also says explicitly not to copy retail's own name
back in.

**The general rule: before deriving a callee's semantics from disassembly, grep
for OUR port of that callee.** A local re-declaration is a signature, not a
source of truth, and this tree has many of them by design. Anywhere a local
re-declaration exists, the caveat has to be copied with it or pointed at.

## ADVISOR VERDICT on `BANISHED_RT_INTERNAL_TABLE` — the evidence is sufficient; the TIMING is not

Lane OBJLIFE held the gate shut and named its own condition: *"flip it when
someone who didn't build the counter has read §4.1."* The advisor did not build
the counter and has read §4.1. **Verdict: the evidence supports arming. Do not
arm it inside this wave.**

### The evidence, assessed independently

Three things make this stronger than a normal green run:

1. **The oracle is predictive, not a self-check.** `calls=126764 divergences=0`
   and `calls=126758 divergences=0` — ours predicts, theirs runs, compare. This
   project's ~250 `RT_DIFF_SHADOW` sites are self-checks that never consult the
   DLL; this is not one of those. Traffic is a third above Wave 3's 93,387 and
   still zero.
2. **The attendance counter discriminates THREE ways, not two.** Baseline
   `ctorRuns=0 … not registered`; shadow `ctorRuns=0 … REGISTERED BUT NEVER
   ENTERED`; armed `ctorRuns=122030`. The middle reading is the one that matters
   — it separates *registered* from *entered*, which a two-cell A/B cannot, and
   it is the exact distinction that made an earlier attendance counter in this
   wave lie from a static destructor.
3. **An undesigned cross-check.** Ctor and dtor differ by exactly **6** in the
   shadow run (126764/126758) *and* in the armed run (122030/122024) — two
   different tiers, different totals, the same residue of six live objects. A
   coincidence nobody engineered agreeing across independent runs is worth more
   than a designed check, because nothing was tuned to make it pass.

`clearRuns=0 resetRuns=0` is correctly labelled a statement about **the runs**,
not about lane OBJINT's two ports, and printed on its own line naming
`BANISHED_AUTOLOAD` so the zero cannot be read through the 122,030.

### Why not now

Arming moves **~122,000 calls per run onto the busiest path in the engine**, for
every lane, at merge time. Bundling that with a three-lane merge confounds the
wave's integration measurement: any change in behaviour or timing afterwards has
two candidate causes and no way to separate them.

**Arm it as the FIRST isolated change after the wave's integration measurement**,
with its own before/after, and measure it against the perf baseline
(`make sa-stress`) rather than a functional run alone — 122k calls/run through
our bodies instead of the DLL's is exactly the shape that has breached the perf
gate before (Wave 2, two `ResolveExportRaw` calls in a hot ctor).

The lane's own reason for holding — *this lane wrote the instrument that measures
it* — is sound and is the right instinct. It is now satisfied; what replaces it
is a sequencing constraint, not a doubt about the evidence.

## Two reusable facts this lane paid a run each for

1. **The DLL's `MemoryAllocator` singleton does not exist at `RuntimePortInit`**
   (a `MemAlloc` there faults) **and is null again by the exit summary.** Its live
   window is between engine startup and shutdown, so any probe touching it must
   run from the frame tick. This is the same family as the attendance counter
   that lied from a static destructor: **the port registry and the DLL's
   singletons have narrower lifetimes than the process, and an instrument placed
   outside that window reports a confident wrong answer rather than an error.**
2. **`_allocatedBytes` is not a pure function of what was freed** — it records
   what the allocator *carved*, which moves with bin state. A control fired on
   the *theirs* side in a cell where both sides tear down identical subjects.
   **Read blocks as the verdict and bytes as corroboration.** The lane's
   independently computed `expected=` column turned this from an open question
   into a one-minute diagnosis, which is the argument for carrying an expected
   value beside every measured one.

Also confirmed the hard way, again: **`ResolveAddr(Module::Runtime, …)` consults
the port registry FIRST**, so it is the wrong door for an oracle's theirs-side
read *on principle*, regardless of what happens to be registered at the time.

## A CLEAN MERGE IS NOT A CORRECT MERGE — caught before the real integration

`git merge-tree` said the only conflicting file was `rt_object_port.cpp`, and it
was right. **The damage was in a file that merged with no conflict at all.**

Trial-merging OBJDATA + OBJLIFE + OBJINT into a scratch worktree produced a tree
that **builds cleanly** and in which lane OBJINT's headline verdict has been
silently reverted:

```
wave11/objint      internal.cpp:  if (shadow) … | if (!shadow) { ClearPersistState } | if (table) { … }
trial merge        internal.cpp:  if (shadow) … |            (BLOCK GONE)            | if (table) { ClearPersistState }
                                  grep -c "if (!shadow)"  ->  0
```

OBJINT moved `?ClearPersistState@InternalFactory@Object@@` **out of** the opt-in
`if (table)` tier and into `if (!shadow)` — that move *is* the lane's verdict
(VERIFIED, UNGATED, default-on) and is the one row it added to the census. Lane
OBJLIFE edited the same region to add its attendance counters. Git reconciled the
two without a conflict marker and **kept OBJLIFE's version of the region,
discarding OBJINT's deletion and its new block.** The row goes back to being
gated, which by this project's own rule means *not ported*.

Nothing about the result announces this. It compiles, the census still moves
(from the other lanes), and no marker was ever written.

**The rule for every multi-lane integration from here: after merging, GREP FOR
EACH LANE'S HEADLINE VERDICT and confirm it survived.** Conflict-freedom is a
statement about text, not about meaning. Specifically check:

* a registration a lane deliberately MOVED between guards — the move is a delete
  plus an add, and a neighbour editing the same region can silently undo it;
* the invariant OBJINT stated: **`ClearPersistState` must be registered exactly
  once and OUTSIDE the tier.** A duplicate is last-binding-wins and would
  silently make one body dead code; a survivor inside the tier is a lost row.

**Credit where it is due: this was found because the lane DECLARED the collision
rather than leaving the merge to discover it.** OBJINT triaged its own three
`internal.cpp` edits by whether each could move (registration: no, it is the
verdict; one `printf`: yes, drop it; three comment fixes: re-apply on OBJLIFE's
text) before any merge was attempted. Without that declaration the trial merge
would have read as a success.

The preview census on the trial merge — **Object 37/58 = 63.8%** — is therefore
**one row LOW**; with OBJINT's ungating restored it should read 38/58.

## THE CREATION PATH IS ANSWERED — and it is not a factory-porting problem

Lane OBJINT closed the question `WAVE11_KICKOFF.md` opened the wave with, and the
answer inverts the plan.

### The measurement

A linear scan for every direct encoding — `E8`/`E9 rel32`, any REX
`LEA reg,[rip+d32]`, `MOVABS reg,imm64` — against all 202 `?Create*Object@`
exports, over **both** shipped code sections:

| image | `.text` bytes | direct references to any of the 202 |
|---|---|---|
| `Runtime-x64.dll` | 1,349,120 | **0** |
| `Application-x64.exe` | 1,372,672 | **0** |

The 202 are reachable **only through a `.data` function pointer** — and that
pointer is the `BaseTypeInfo` factory slot: the qword holding `?CreateNewObject@`
sits **exactly 8 bytes before** the one holding `?CreateInPlaceObject@`, i.e.
**`+0x20 factoryFn` and `+0x28 inPlaceFactoryFn`, node start = ref − 0x20.**

**Verified independently by the advisor** with `pefile` against the shipped DLL,
by a different route than the lane's scan: 103 `?CreateNewObject@` exports, 99
`?CreateInPlaceObject@` (202 total), 99 classes exporting both, **15 of 15
spot-checked pairs adjacent at +8, zero failures**, every node landing in the
`0x1801f9xxx`-`0x1801faxxx` typeinfo region.

The 206-vs-202 reference gap resolves cleanly and is itself corroboration: **201
factories have exactly one `.data` reference; `?CreateInPlaceObject@Internal@Object@@`
has five** — shared by the five typeinfo nodes whose classes define no in-place
factory and inherit `Internal`'s. An anomaly that explains itself is the
strongest sign the model is right.

### The conclusion that reframes the wave

> **To intercept a factory you must own the minted `?_typeInfo@` row, because the
> pointer lives in that row. Porting the 202 bodies is neither necessary nor
> sufficient; owning the rows is both.**

So the kickoff's "202 factory exports, zero ported" is a true statement about a
**non-problem**. The creation path is not a factory-porting wave — it is a
**typeinfo-row promotion** wave, i.e. lane OBJDATA's subject, gated by the
`minted=` question (does our storage ever get WRITTEN?) rather than by any amount
of body-porting.

### Two exceptions, and why they change nothing

Exactly **two** of the 202 have an import-table door, confirmed by the advisor
with `tools/dispatch_slot_map.py`:

```
0x140151688 -> ?CreateInPlaceObject@DialogController@UI@@…
0x140152a88 -> ?CreateInPlaceObject@Internal@Object@@…
```

`UI::Dialog`'s is not among them. And `DialogController`'s door does **not**
revive its vtable row: that verdict was never about creation — its single
consumer stamps the pointer and then calls `??1Internal@Object@@`, whose first
statement overwrites the same qword unread.

### Why two instruments were needed

The direct-encoding scan and the import-table read are **complementary, not
redundant**, and neither alone gives the right picture: the scan alone says "202
doorless" (wrong — two have doors); the import table alone says nothing about the
other 200. A door through the IAT is an indirect call and by construction holds
no direct reference to the DLL address, which is exactly why a byte scan cannot
see it.

### Correction to the advisor's own ruling

The integration ruling attributed the dropped `printf`'s liveness reading to
**OBJLIFE's** §1.5. It is **OBJINT's** — `WAVE11_LANE_OBJINT.md` §1.5. The record
that must survive the dropped instrument is:

```
[RtPort] FIRST LIVE CALL into our ?ClearPersistState@InternalFactory@Object@@
         (default-on since Wave 11 lane OBJINT)
```

40s `BANISHED_AUTOLOAD` run, `rc=0`, empty crash log. That reading is *why* the
instrument can be dropped without losing the evidence that the ungated body is
not dead code.

---

# WAVE 11 CLOSED — 2026-09-09, merge `0bd9328`

## The numbers, measured on the merge

| | before | after |
|---|---|---|
| **Object** | 32/56 = **57.1%** | 44/61 = **72.1%**, ceiling **88.5%** |
| TOTAL | 80.0% | 80.7% |

**Read the set-diff, not the percentage.** The percentage is partly instrument
inflation — lane OBJEXT's probe pulled rows into the call graph, lifting
numerator and denominator together, **and the lane flagged this itself** rather
than banking it.

* **17 rows LEFT the unported set** — **10 by being PORTED**, **7 by being TAGGED
  provably unpromotable** (3 abstract `??_7` vtables, `?_firstTypeInfo@`, 3
  `?_singleton@` rows). Those two are different achievements and must not be
  added together.
* **3 rows ENTERED**: `?_typeInfo@CompiledExternal@`, `?_typeInfo@Internal@`
  (named by OBJDATA's new accessors) and `?Add@ExternalFactory@` (opened by
  OBJEXT).

## Was the goal met?

The goal was **100% of `called`, or its ceiling**. Neither: **72.1% against a
proven ceiling of 88.5%.** The gap is 10 rows, and every one has a named
mechanism — the `Set<ReferenceEntry>` AVL family, the by-value `IO` stream
classes, `ResetPersistState`'s realloc-on-the-load-leg, and the two ARM-READY
`Internal` rows behind a sequencing decision rather than a doubt.

## Integration verified on the merge, not summed from reports

Clean build; deployed under the game-dir lock (acquired **unpiped**);
`BANISHED_AUTOSTART` run driving menu → New Game → Start; 151s uptime; zero
asserts; **0-byte crash log**. Each lane's headline verdict was **grepped for
after merging** — a trial merge had already reverted one lane's verdict silently,
with no conflict marker, in a tree that still built.

## What the wave actually produced

The census points are the smaller half. The larger half:

1. **The creation path is answered** — a typeinfo-row problem, not a
   factory-porting one. Retires the kickoff's framing.
2. **Five instruments that would have lied**, each caught by its own author
   before publishing a number: an attendance counter in a static destructor; a
   probe scheduled off a function never called; a control that double-freed; a
   control that caught via a stale mechanism it did not name; and two shadow
   sites that had never executed in eight waves.
3. **A clean merge is not a correct merge** — now an integration gate.
4. **A census `unported` row has four meanings**, only two of which are porting.

## Follow-ups, in priority order

1. **Arm `BANISHED_RT_INTERNAL_TABLE`** as the first isolated change after this
   merge, measured against `make sa-stress`. Evidence is sufficient (advisor
   verdict above); the deferral was sequencing.
2. **The `IO` stream family** (`CompressedFileStream`, `BinaryFileStream`,
   `TextFileStream`, `VisitorStream`) — one root cause behind three held-back
   rows plus `Add`. Scoping it as "port `External::Load`" would scope the symptom.
3. **`Set<ReferenceEntry>` AVL family** (0x10-element, node 0x30).
4. **Fix `tools/game_dir_lock.sh`** — the empty-ticket starvation (line 293), the
   timeout path that sends the longest waiter to the back, and the exit-code trap
   that reported success three times in one lane.
5. **Typeinfo-row promotion**, gated by the `minted=` test — the real successor to
   the "object creation path" wave.

## Follow-up 1 CLOSED — `BANISHED_RT_INTERNAL_TABLE` armed (`wave11f/arm`)

Measured by the integrator on the merge:

| | before | after |
|---|---|---|
| **Object** | 44/61 = 72.1% | **47/61 = 77.0%**, ceiling 88.5% |
| TOTAL | 80.7% | **81.0%** |

Set-diff: **3 rows left, ZERO entered** — `??0Internal`, `??1Internal`,
`?ResetPersistState@InternalFactory@`. Both `standalone-debug` and
`standalone-perf` build clean at the merge.

**Perf: the effect is inside the noise.** 6 interleaved `sa-stress` runs under
one lock hold — ARMED 0.18856 sim ms/iter (spread 0.00467), KILLED 0.19033
(spread 0.00511). Difference of means −0.93%, **2.7× smaller than the within-arm
spread**, ranges fully overlapping. That **bounds** the cost well inside the 5%
gate; it does not measure it, and −0.93% is not a speed-up. The lane said so in
those words rather than quoting a point estimate.

The gate is `table = noDll || (!tableKilled && (tableForced || !shadow))`. A
naive default-on would have made `BANISHED_RT_INTERNAL_SHADOW=1` hit the
mutual-exclusion refusal and **silently delete the oracle that produced the
evidence for arming** — so an *explicit* arm still conflicts loudly while the
*default* arm yields. Kill switch verified by running it, not by reading it.

It also ran the live tier-2 load run that `ResetPersistState`'s own comment still
owed (`clearRuns=1 resetRuns=1`, clean exit), which closes lane OBJINT's
VERIFIED-BUT-UNARMED holdback.

### Two pre-existing defects it found, both confirmed by the integrator

1. **`make standalone-perf` did not build at `bd0f184`.** Six files call
   `std::printf`, which `log_strip.h`'s own comment says cannot survive the perf
   build's `printf` macro. The integrator reproduced this in an **untouched main
   checkout**: 20 errors, all in `log_strip.h`. **Consequence: `sa-stress` has
   been unrunnable since those files landed, so any recent "no perf regression"
   claim was not measured on the perf exe.** Fixed here mechanically.
2. **`runtime_census.py`'s `GATED_OFF_BLOCKS` refused to count any registration
   inside a block gated on a variable named `"table"` in `internal.cpp`.** That
   list exists to stop an UNARMED row counting as a port — left alone it would
   have refused to count an **ARMED** one, and the lane's first measurement came
   back empty for exactly that reason. **The empty answer was the tool's, not the
   change's.** Integrator check on blast radius: `GATED_OFF_BLOCKS` is keyed by
   FILE, and the only other `if (table)` in the tree (`runtime/src/ui/dialog.cpp:2171`)
   is a local pointer, not a gate, and registers nothing — so removing `"table"`
   affects exactly `internal.cpp`. `"shadow"`/`"checkPtr"` correctly stay.

## Follow-up 2 CLOSED — and it makes a Wave 11 safety statement STALE

Measured by the integrator on the merge:

| | before | after |
|---|---|---|
| **Object** | 47/61 = 77.0% | **50/61 = 82.0%**, ceiling 88.5% — **4 rows from the ceiling** |
| IO | 110/83 = 75.5% | 120/83 = **69.2%** (`ported` unchanged; the predicted fall) |
| TOTAL | 81.0% | 80.5% |

Set-diff: the **three subject rows left**; **11 entered** — ten exported IO names
and **one address, `0x18002c6a0`** (`Array<ExternalArrayElement>::SetSize`,
unexported in both builds), the only genuine new address door. The IO fall was
**pre-registered by the lane before the run** and is the honest price of
resolving by name. Both build configurations clean.

### ⚠ THE 104 FACTORY ABORT TRAPS ARE NOW REACHABLE — this changes follow-up 5

Wave 11 lane OBJINT recorded *"Abort traps: none become reachable"*, correctly at
the time: nothing in our tree called a `BaseTypeInfo` factory pointer. **That
statement is now stale, and this merge is what made it stale.**

Our newly ported `?Load@External@Object@@` is not the "construct, Open, dispatch
+0x28" body the brief described — the lane found it **under-scoped** — it is a
full deserialisation driver that looks up a type id and **creates the object
through `BaseTypeInfo::factoryFn`**:

```
runtime/src/object/external_load.cpp:351    reinterpret_cast<FactoryFn>(info->factoryFn)()
                                            created=2847 per boot
```

And the mint's own rule (`standalone/rt_data_mint.cpp:394-395`) is:

```cpp
if (f) { bti->factoryFn = f; }                      // ported
else   { bti->factoryFn = (void*)t.factoryTrap; }   // ABORT TRAP
        RtMintTrapDie("minted BaseTypeInfo::factoryFn is not ported")
```

**So promoting any minted `?_typeInfo@` row whose factory is not ported now hands
our own `Load` an abort trap, 2 847 times a boot.** Before this merge that was
unreachable by construction; it is now one promotion away.

**Binding on follow-up 5:** a typeinfo row may be promoted only if its
`factoryFn`/`inPlaceFactoryFn` is **ported**, or that type is **provably never
constructed through `External::Load`**. Read `tiFactoriesTrapped` — the mint
already counts this — and treat a non-zero count on a promoted row as a blocker,
not a warning. This is the risk the Wave 11 kickoff called *"a live risk, not a
footnote"*; it has now materialised.

Corroboration worth noting: `rt_data_mint.cpp:147` carries
`static_assert(offsetof(RtObject::BaseTypeInfo, factoryFn) == 0x20)` — the mint
independently agrees with lane OBJINT's `+0x20`/`+0x28` derivation.

### Four defects in the restart-orphaned draft, all fixed

1. **`DumpAddCounters` was never `atexit`-scheduled**, so the file's own *stated*
   positive control could not print — another instrument that would have been
   silently inert. Fixed, and it immediately produced the `visited=1799
   skipped=3221` split.
2. **A comment inverted the `GetPath` ABI** (claimed retslot-first; the binary
   says `this`-first). **The code was right and only the prose was wrong** — the
   dangerous direction, because the next reader trusts the prose.
3. *"Retail's inlined `~VisitorStream` makes ONE vtable stamp"* — it makes two.
4. **The perf build was broken again** by `std::printf` vs `log_strip.h`'s macro
   — **a recurrence one lane after follow-up 1 fixed it**, and this time 4 of the
   6 offending files were the lane's own. The fix has no guard, so it will recur
   again. See the follow-up list.

### Two scoping corrections to the endorsed decomposition

* **`Load` was UNDER-scoped** by the brief and by the decomposition I endorsed.
* **`IO::Serializer` is a 16-byte facade `{Stream*, Impl*}`**, not the
  vtable-carrying object — getting that wrong would have mis-shaped every
  serializer call.
* **`SaveText` can never be verified against a real object in the shipped DLL**:
  retail's TEXT `Encoding` has **both** Serializer factory slots stubbed to
  `XOR EAX,EAX; RET` while the BINARY one's are real. Established by **running
  the shipped export**, not by inspection.

## Follow-up 3 CLOSED — `SerializeReferenceTable` + the `Set<ReferenceEntry>` family

| | before | after |
|---|---|---|
| **Object** | 50/61 = 82.0% | **51/61 = 83.6%**, ceiling 88.5% — **3 rows from the ceiling** |
| TOTAL | 80.5% | 80.6% |

`ported` +2 (the export and the address `0x18002cb90`); nothing entered `called`
unported. Both builds clean with the new log-strip gate active.

**The three rows left are ALL follow-up #5's subject**: `??_7Internal@Object@@6B@`,
`?_typeInfo@CompiledExternal@Object@@`, `?_typeInfo@Internal@Object@@`. So the
88.5% ceiling is reachable by promotion alone — subject to the abort-trap
precondition recorded under follow-up 2.

### Two corrections, and the first is the integrator's

1. **The brief named two missing bodies; only one was missing.** `0x18002ebf0`
   (destroy-tree) had been ported *and registered* since Wave 11 lane OBJLIFE as
   `ReferenceSubtreeFree` (`factory_dtors.cpp:381`, registered at `:499`). The
   integrator passed OBJEXT's §5.1 addresses into the brief **without grepping
   either**. That is the **fourth** instance in this series of a brief sending a
   lane at code the tree already had, and the root cause is the same every time:
   **treating a prior lane's prose as a port-state statement.** Post-merge check:
   exactly one binding each for `0x18002ebf0` and `0x18002cb90` — necessary
   because `RegisterRuntimeAddr` is last-binding-wins and a duplicate would have
   silently made one body dead code.

2. **The AVL family is what the READ path needs.** The WRITE path never inserts —
   it walks `_resources` in order and emits. **So the half of the function whose
   failure mode is a silent save desync was never blocked by the Set family at
   all**, and had been portable and oracle-able since Wave 9 lane S existed.
   Generalisable: *a holdback's stated blocker can cover one direction of a
   two-direction function and read as covering both.*

### Method worth reusing

* **Mechanism-pure controls.** `wrong slot` caught on *identical values*
  (`uint(+0x98)=5683` vs `int(+0xa0)=5683`); `wrong value` caught on *identical
  slots* (`ours=2 theirs=1`). Neither could have been caught by the other's
  mechanism — the "catch via the mechanism it names" rule demonstrated, not
  asserted.
* **Adversarial population against a signed comparator.** The comparator is a
  signed `JL`/`JGE` on a `uint32` field while the live caller only ever inserts
  `1..N`. Populating for that is the difference between a verification and a
  uniform population that verifies nothing.
* **Set-diff measured from a scratch worktree at the base.** `git stash` measures
  nothing once the work is committed, and the lane's first attempt produced
  exactly that empty diff before it noticed.
* **Pre-registration scored honestly**: direction and magnitude held, the
  *mechanism* was wrong (it predicted the exported name would enter `called`; it
  was the unexported address). Scoring a prediction partly wrong when the
  headline matched is worth more than a clean-looking hit.

## Follow-up 4 CLOSED — the game-dir lock (`wave11f/lock`)

All three defects reproduced against the pre-fix code before being fixed, and
**scored separately** rather than as an aggregate.

| defect | verdict |
|---|---|
| empty-ticket starvation in `ticket_head` | **real, fixed** — digits-guard on both the `cat` and `date -r` reads. Reproduction on the OLD file returned the wrong lane. This cost Wave 11 two 900s windows |
| timeout sends the longest waiter to the BACK | **real, fixed** — `ticket_drop` removed from both TIMEOUT branches; liveness still keyed on `mtime`/`ticket_touch`, plus a 20-min `ticket_sweep_garbage` so abandoned tickets cannot accumulate |
| exit-code trap | **already >half-mitigated by Wave 10** — the residue was naming/discoverability, closed with an `assert-held` alias and a usage header leading with a copy-pasteable gating idiom |

**Verified by the integrator, not taken on report:** selftest 4/4 per defect; a
full cycle on a scratch `BANISHED_GAME_DIR` returning 0/0/1/0/1; and a smoke test
on the **real** game directory, since follow-up 5 depends on this tool.

### A false finding the integrator nearly acted on

The lane reported `tools/objext_run.sh:28` *"calls acquire with NO check at all"*.
**It checks, correctly, on the very next line** (`rc=$?` … `exit 9`), and did so
at the lane's own base commit — the branch never touched the file. The mechanism
is a **one-line window**: the line was found by grepping for `acquire` and judged
without reading line 29.

This is the known class where **a checker with too small a window manufactures
confident wrong findings**, and its false positives cluster on exactly the code
that *mentions* the thing being looked for. **The integrator nearly edited a
shared script on the strength of it**; the only thing that prevented it was
reading the surrounding lines before touching anything. No edit was made.

Two things the lane did right that made this recoverable: it **stayed in file
scope** and flagged the caller instead of editing outside its subject — had it
edited, the false finding would have landed as a change; and it reported defect 3
as **already mostly fixed** rather than claiming a fresh save, which is a
correction to the brief that is easy to quietly not make.
