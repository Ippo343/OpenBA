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
