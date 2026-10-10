# Wave 9 lane TYPE — attempt 20's wall: the reflection chain was a SUBSET

**Branch** `wave9/lane-type` · **base** `e608c6c` · **worktree**
`C:\code\GIT\untitled_rev_eng_project\.claude\worktrees\lane-type-089e6eb4` · 2026-09-08

---

## The one-line answer

`UI::DialogControllerConfig`. Four exe types derive from it, the DLL exports its
`?_typeInfo@` at `0x1801f9e80`, and **the mint did not mint it** — so at cutover
the reflection chain lacked the parent those four name, and
`FactoryStartupRun`'s faithful `LEA RBP,[RCX+8]` on a null `RCX` produced
`(TypeRecord*)8` and faulted reading `0x28`, exactly as
`runtime/src/object/type_system.cpp`'s own banner predicted.

The port was correct. The mint was short by 58 rows.

---

## 1. What the fault was, named

`runtime/src/object/type_system.cpp` now carries two things that were not there.

**A survey** (`BANISHED_RT_TYPE_SURVEY=1`), a read-only pre-pass between
Factory::Startup's two passes. It resolves every `parentName` the way pass 2
will and reports **all** the misses — the one thing the faithful fault cannot
tell you, because the fault stops at the first. It writes nothing and is
compiled behind a `getenv` test, so pass 2 is bit-identical with the var unset.

**A name on the fault itself** (unconditional, one line, on the path that is
about to die). It is **not a guard**: the `LEA` and the deref below it are
unchanged and still fault. It only replaces `READ of 0x0000000000000028` with
the type, the parentName and both CRCs.

First cutover run, before any fix:

```
[RtTypeSurvey] MISSING PARENT: type "UpgradeUIConfig"       (id 0x7242EE81) names parent "DialogControllerConfig" (id 0xB77E0EB9)
[RtTypeSurvey] MISSING PARENT: type "ControlUIConfig"       (id 0xC1E2EF37) names parent "DialogControllerConfig" (id 0xB77E0EB9)
[RtTypeSurvey] MISSING PARENT: type "StatusIconUIConfig"    (id 0xCAF7FF58) names parent "DialogControllerConfig" (id 0xB77E0EB9)
[RtTypeSurvey] MISSING PARENT: type "ResourceLimitUIConfig" (id 0xE2CAA0F9) names parent "DialogControllerConfig" (id 0xB77E0EB9)
[RtTypeSurvey build LANETYPE-A1] 357 nodes in tree, 355 with a parentName, 4 parentName(s) UNRESOLVED
[RtPort] Factory::Startup: type "UpgradeUIConfig" (id 0x7242EE81) names parent "DialogControllerConfig" ... faults at 0x28 -- as retail does.
```

The four are `code/Game/Dialog/{upgrade,control,statusicon,resourcelimit}uiconfig.cpp`,
each `REGISTER_TYPE(..., "UI::DialogControllerConfig", ...)`.

## 2. Why it is absent at cutover and present with the DLL — MEASURED

The same survey, same build, **Runtime-x64.dll present**, mint off:

```
[RtTypeSurvey build LANETYPE-A1] 415 nodes in tree, 413 with a parentName, 0 parentName(s) UNRESOLVED
```

| configuration | nodes | with parentName | unresolved |
|---|---|---|---|
| DLL present (baseline) | **415** | 413 | **0** |
| cutover, 46 minted BaseTypeInfo | **357** | 355 | **4** |
| difference | **58** | 58 | 4 |

And `Runtime-x64.dll` exports **104** `?_typeInfo@` DATA symbols. The mint carried
**46**. `104 − 46 = 58`, and `415 − 357 = 58`. The gap is an identity, not a
coincidence: the exe contributes the same 311 types in both configurations
(`415 − 104 = 357 − 46 = 311`), and the entire deficit is Runtime BaseTypeInfo
statics that the mint never minted.

### The blind spot, and it is the third instance of one memory

`tools/rt_mint_gen.py` had two population rules, and both answer the same
question — *which symbols does something in this tree READ*:

1. `scan_source_for_data_symbols` — every mangled DATA name appearing as a
   **literal** in `code/ runtime/ standalone/ engine/ dx11/`.
2. `CLOSURE: a minted vtable DRAGS IN ITS OWN ?_typeInfo@` — added when ten
   `BaseTypeInfo` came out null because only the minted vtables' own slots read
   them (`reference_mint_population_rule_blind_to_its_own_output.md`).

A `?_typeInfo@` earns its place in the chain **by existing**, not by being read.
Every one is a static whose constructor head-inserts it into `?_firstTypeInfo@`,
and Factory::Startup then resolves each entry's `parentName` **by name against
that one chain**. `UI::DialogControllerConfig` is named as a parent only by a
`REGISTER_TYPE` **string**, never by its mangled symbol, and no minted vtable
reads it — so rule 1 and rule 2 were both blind to it *by construction*. Same
subsystem, same shape, third time.

## 3. The fix — CLOSURE 2, in the generator, not by hand

`tools/rt_mint_gen.py`, immediately after the vtable closure: **every exported
`?_typeInfo@` DATA symbol is minted**, because the DLL's reflection chain is
exactly its BaseTypeInfo statics and any subset is a chain retail does not have.
It refuses (into `errors`) if such a symbol lives in `.text`.

Adding the one `DialogControllerConfig` row by hand was the obvious move and the
wrong one, for the reason the vtable-closure comment already gives: the gap is
derivable, so it is derived. Any `?_typeInfo@` the DLL grows brings itself along.

**What a minted row dereferences, and why registering it is safe.** Nothing new.
The generator reads `name / parentName / fileExtension / factoryFn /
inPlaceFactoryFn` straight out of the DLL's own `.data` image (the same
`cstr_at` / `by_rva` path the other 46 use); only `next` is ours, and
`MintTypeInfos()` builds it parents-first. An unported `factoryFn` gets the
identical trap stub every other minted row's does — a type nothing constructs
never calls it. `RegisterMintedData()` publishes storage the mint already laid
out and constructs nothing, and it is off unless `BANISHED_RT_MINT=1`, i.e. the
cutover configuration. All 104 pass the generator's existing
"NOT built by any CRT initialiser" check.

Generator output: `chain closure: +58`, `typeinfo 104`,
`crt init 106 initialisers; 108 ... CONSTRUCTED at load (3 storage + 104 BaseTypeInfo + 1 vtable)`.
That last breakdown is new — the line previously printed `108` against
`3 + 104 = 107` with nothing accounting for the difference. It is one vtable,
written by a CRT initialiser. No defect; the line just did not say so.

## 4. Result — prediction recorded before the run, and met

**Predicted:** `104 BaseTypeInfo`; survey reports `415 / 413 / 0`, identical to
the DLL-present baseline; boot leaves Factory::Startup and hits a different wall.

**Measured** (`[RtTypeSurvey build LANETYPE-A2]`, that run's own log):

```
[RtMint] minted 54 vtables (491 slots: ...), 104 BaseTypeInfo, 38 storage symbols
[RtTypeSurvey build LANETYPE-A2] 415 nodes in tree, 413 with a parentName, 0 parentName(s) UNRESOLVED
```

Three for three. **The Factory::Startup wall is closed.** The boot now dies
further along in `Engine::Startup` — `engine_lifecycle.cpp:638`:

```
[RtUnresolved] ?PlatformStartup@Application@1@AEAA_NXZ  (asked by 00007ff71b275925)
[RtResolve] MISS "?PlatformStartup@Application@1@AEAA_NXZ" -> NULL ... this symbol still needs minting.
Fault: EXEC of 0000000000000000
```

That is an unported export and a different lane's row, not a defect in anything
this lane touched. (The crash dump labels it `VCALL-of-0` and prints `this[+0]`;
it is a plain `RT_EXPORT_ONCE` function-pointer call on a NULL resolve, and the
dump's virtual-call heuristic simply guessed wrong. Read the `[RtUnresolved]`
line, not the `VCALL` label.)

## 5. Controls

* **Effectiveness control** — the survey prints `withParent`, and appends
  `(NO PARENTS RESOLVED -- this survey proved nothing)` when it is zero. It
  measured 355 and 413 on the two runs, so the arm that can be wrong ran
  hundreds of times, not zero.
* **Negative control, and it FIRED** — the DLL-present baseline is the same code
  on the same build reporting `0 unresolved` out of `413` resolved. A survey
  that could only ever say "4 missing" would be worthless; this one says 0 when
  0 is true and 4 when 4 is true.
* **Build identity** — the survey line carries `build LANETYPE-A1` / `-A2`, and
  every number above is read from that run's own copied log, not from
  "I rebuilt".
* **Door closure, predicted then set-diffed.** Predicted **no** movement: this
  lane adds no `ResolveAddr` literal and satisfies no export by name. Measured
  by running `tools/addr_door_audit.py --csv` and
  `tools/unsatisfied_export_scan.py` in this tree **and** in a pristine
  `e608c6c` worktree: both outputs `diff`-identical (16 door rows, 120 export
  lines, empty set-diff both ways).

## 6. HONEST LIMITS

* **The 58 new rows are proved as chain MEMBERS, not as usable types.** What is
  measured is that the tree is now 415 nodes with 0 unresolved parents, matching
  retail's shape exactly. Nothing here constructs an object of any of the 58, so
  their `factoryFn` / `inPlaceFactoryFn` are unexercised; the first one that is
  called will hit its trap stub and say which. That is a *later* wall, and it is
  named when it happens rather than silent.
* **Node identity is not compared, only the count and the parent resolution.**
  The 415 at cutover and the 415 with the DLL were not compared name-by-name.
  The Startup probe (`BANISHED_RT_FACTORY_STARTUP_PROBE=1`, in the same file,
  written by lane OBJ2) does exactly that comparison and **still has never been
  executed** — this lane did not run it either. Doing so is the obvious next
  strengthening and is the honest gap in this row.
* **The survey does not prove reachability of pass 2's growth arm.** It only
  resolves parents; the `capacity = n*3` rule and the copy-then-free path are
  untouched by anything measured here.
* **Two cutover runs, not a soak.** One before the fix, one after, plus one
  DLL-present baseline. The numbers are deterministic (a static chain and a CRC),
  so a soak would add nothing, but nothing here is a statistical claim.
* **`?PlatformStartup@Application@1@AEAA_NXZ` is measured only as the next
  failure**, not diagnosed. This lane did not look at what it needs.
* **`addr_door_audit.py` reports 16 rows in this tree**, not the ~129 figure
  quoted elsewhere in the wave; the set-diff above is between two runs of the
  same tool on two trees, which is the comparison that is sound regardless of
  what the absolute number means.

## 7. Files

* `runtime/src/object/type_system.cpp` — the survey and the fault's name. No
  behaviour change with `BANISHED_RT_TYPE_SURVEY` unset.
* `tools/rt_mint_gen.py` — `CLOSURE 2`, plus the `+ N vtable` term that
  explains the `crt init` line's off-by-one.
* `standalone/rt_data_mint_tables.inc` — regenerated; +58 `BaseTypeInfo`.
* `tools/lane_type_run.sh` — the cutover runner. Moves `Runtime-x64.dll` aside
  and **restores it from a `trap`** on every exit path, then verifies it is back
  and prints its size.
* `tools/lane_type_run_dll.sh` — the DLL-present baseline runner.
