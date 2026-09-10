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
