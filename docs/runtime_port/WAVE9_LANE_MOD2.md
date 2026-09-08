# Wave 9 — Lane MOD, second pass: `IO::Modification::RefreshList`, the live wall

Produced 2026-09-08 on `wave9/lane-mod`, cut from `fd30180`.

Attempt 19 of the DLL-less boot died on a **null call**, not a null store:

```
[RtResolve] MISS "?RefreshList@Modification@IO@@QEAAXXZ" -> NULL
  ... call through RAX=0, RCX=RDX=the Modification object,
      return address Engine::Startup+0x338
```

Of the 27 symbols still unsatisfied on that boot path, this was the one the boot
**reached**. Everything else was resolved-and-stored. This lane ports it.

---

## 1. Verdict per row

| # | address | symbol / role | instrs | verdict |
|---|---------|---------------|--------|---------|
| 1 | `0x1800af5f0` | `?RefreshList@Modification@IO@@QEAAXXZ` | 934 | **PORTED + BOTH DOORS.** Live on both, diffed, 0 divergences. |
| 2 | `0x180028610` | `Array<Mod>::SetSize(int, const Mod&)` | 101 | **PORTED + ADDRESS DOOR.** Live — the only append in RefreshList. |
| 3 | `0x1800274c0` | `Set<Filename>` insert fix-up (AVL retrace) | 71 | **PORTED + ADDRESS DOOR.** Live, once per inserted path. |
| 4 | `0x180027f50` | `Set<Filename>` rotate-right | 43 | **PORTED + ADDRESS DOOR.** Reached from #3. |
| 5 | `0x180027e20` | `Set<Filename>` rotate-left | 43 | **PORTED + ADDRESS DOOR.** Reached from #3. |
| 6 | `0x180025530` | `?GetIconData@PackageFile@IO@@` | 20 | **PORTED + BOTH DOORS.** Live. |
| 7 | `0x180025580` | `?GetPreviewData@PackageFile@IO@@` | 20 | **PORTED + BOTH DOORS.** Live. |
| 8 | `0x18007f034` | the registry-string `strtol` wrapper | 3 | **PORTED + ADDRESS DOOR.** Live. |
| — | `0x1800b4540` | `??1PackageFile@IO@@UEAA@XZ` (ord 639) | 87 | **DECLINED — lane PKG2's row.** §6 |
| — | `0x1800accc0` | `?CloseFile@FileSystem@IO@@` (ord 1726) | 56 | **DECLINED — lane PKG2's row, and it OPENS a door.** §6 |
| — | `0x180028760` | the `Array<Mod>` introsort | 422 | **DECLINED — opens two doors, needs its own probe.** §6 |

**8 ported, 3 declined with a reason measured today.**

RefreshList is **934 instructions**, not the "~450" the first lane-MOD pass
estimated from a decompile's line count. The difference is what MSVC inlined
into the one body: the whole `Set<Filename>` insert, the same tree's LOOKUP and
in-order walk in the override pass, three `Collection::Array` grow-and-copy
loops, the `PackageFile` constructor, and eleven copies of the refcounted-String
release idiom.

---

## 2. The blocker dissolved — re-derived, not inherited

`WAVE9_LANE_MOD.md` §6 held RefreshList back on `IO::PackageFile`, and that was
right when written: attempt 14 ended with
`[RtResolve] MISS "??0PackageFile@IO@@QEAA@XZ" -> NULL`. Re-checked from
scratch today (`reference_holdback_blockers_are_stale_check_them_first.md`):

| dependency | state today | evidence |
|---|---|---|
| `??0PackageFile@IO@@QEAA@XZ` | **ported, both doors** | `package_file.cpp:838` + `:840` (lane PKG2) |
| `?ReadPackfile@PackageFile@IO@@…` | **ported, both doors** | `package_file_read_win.cpp:999` + `:1001` (lane PKG3) |
| `?GetName@` / `?GetAuthor@` / `?GetDescription@` | ported | `package_file.cpp:867-877` |
| `?GetFileName@` | ported | `filesystem_packages.cpp:580-582` |
| `?FindFiles@FileSystem@IO@@` | ported | `filesystem_find_win.cpp:771-772` (lane FIND) |
| `?GetPackDirectory@` | ported | `filesystem_packages.cpp:973-974` |
| `Map_Find` `0x18002ace0` | ported | `configuration.cpp:310` |

The last row also **corrects the first pass's §5 by half**. That section left
`0x18002ace0` grouped with the *insert* (`0x18002a090`) as "a Collection lane's
work". The **insert** is still unported; the **find** has been ported since
Wave 8 and is what RefreshList needs. Anyone reading §5 as "the map is
unported" would have re-derived this holdback and kept it.

---

## 3. Measurements

All runs: DLL present, `BANISHED_RT_DIFF=1`, debug build, under
`tools/game_dir_lock.sh` as `lane-MOD`. Scripts: `tools/mod2_run.sh`,
`tools/mod2_run2.sh`, `tools/mod2_run3.sh`. Raw logs in
`docs/runtime_port/wave9_lane_mod2/`.

### THE POPULATION — why zero mods would have been a green lie

`WinData/*.pkm` is normally **empty** in the shared game directory (the
achievement lanes need the mods out; `WinData/_mods_off/` is where they live).
`RefreshList`'s entire body is a loop over `WinData/*.pkm`. With none mounted,
`FindFiles` returns zero entries, LOOP 1 never executes, and the oracle compares
**two empty photographs and prints `divergences=0`** — a green line meaning
"never ran", the exact shape of
`reference_uniform_population_is_not_a_verification.md`.

So the scripts **mount** `betterFields.pkm` and `DebugMenu.pkm` for the duration
of their runs, move them back afterwards under the same lock, and print the set
they ran against. **The integrator was told separately** (message at 14:45): the
lock does not cover `WinData`, so any lane whose run overlapped these windows
saw two extra packages and three extra `ReadPackfile` calls. `WinData/*.pkm` is
back to zero, verified after every script.

**Two mods is the minimum, not a nicety.** The override pass (LOOP 2) is a
double loop over ordered pairs `i != j`. At n=1 its body never executes and the
`files` array it exists to populate is empty for every mod no matter what the
code does — every term that pass could get wrong is unstateable.

### Run A — the mods menu (the BY-NAME door)

`BANISHED_AUTOMENU_BUTTON=7 BANISHED_AUTOMENU_CLICKS=1001,5001,d0,4001,101,m6 BANISHED_MODAL_ANSWER=1`

```
[RtDiff]   IO::Modification::RefreshList    calls=1   worked=1   divergences=0
[ModDialog] opened + AddDialog'd (dlg=… mods=2)
standalone_crash.log 0 bytes, rc=0
```

`worked=1` is the effectiveness control (`RT_DIFF_WORK(self->count > 0)`,
counted INSIDE the subject), and the shadow's own message carries the
population: `ours=mods=2 all terms equal`. A run reporting `divergences=0` with
`worked=0` is the "never ran" case above, which is why the column is printed.

### Run B — the BY-ADDRESS door

Run A reached RefreshList once, from `Engine::Startup`, which resolves it **by
name**. The other door is `engine/funcs/engine_funcs.cpp`'s
`fn_Modification_RefreshList`, resolved from the raw address `0x1800af5f0`, and
`code/Game/Dialog/moddialog.cpp:747` calls it on the mods dialog's **Cancel**
(element id 100). Run A clicked 101 (Ok), whose Apply path with
`BANISHED_MODAL_ANSWER=1` takes the `SaveList` branch and never calls
RefreshList — so run A exercised the by-name door only, and a port registered on
only one of the two doors would still have printed a clean `calls=1`.

`BANISHED_AUTOMENU_CLICKS=1001,5001,d0,4001,100,m6`

```
[RtDiff]   IO::Modification::RefreshList    calls=2   worked=2   divergences=0
standalone_crash.log 0 bytes, rc=0
```

Two calls: `Engine::Startup` by name, then the dialog's Cancel by address.

### The negative controls — and the one that had to be rewritten

`BANISHED_RT_MOD_NEGCTL=<n>` perturbs **our** photograph after it is taken and
before the compare. Every one FIRED, and each named the term it broke:

| n | perturbation | term reported | verdict |
|---|---|---|---|
| 1 | flip `mods[0].enabled` | `TERM=enabled index=0` | **fired** |
| 2 | flip a bit in the LAST KEY of set 0's in-order walk | `TERM=set.inorder index=0` | **fired** |
| 3 | `mods[0].order += 1` | `TERM=order index=0` | **fired** |
| 4 | drop a key from set 0's walk | `TERM=set.walkLength index=0` | **fired** |

Each control run also reported `calls=1 worked=1 divergences=1` — one call, one
that did work, one divergence, so nothing fired by accident.

**Control 2 had to be rewritten, and that is the finding.** The first draft had
three controls and its only *set* control was today's #4 — it **drops a key**,
which fires on the `set.walkLength` length check *before the key walk ever
runs*. That left `set.inorder` — the term the entire AVL port rests on — with no
control at all: a retrace that produced a differently ordered tree over the same
keys would have gone unreported and nothing in the run would have said so. Two
lessons in one defect: a control that fires is not automatically a control of
the thing you think it is
(`reference_probe_needs_an_effectiveness_control.md`,
`reference_recheck_must_match_its_own_proof.md`).

### The control run — the same harness on a build WITHOUT this lane

`tools/mod2_run3.sh` moved `modification_refresh.cpp` aside, checked out
`fd30180`'s `modification.cpp`, rebuilt, and ran run A's harness. The gate is
the *absence* of the identity line — `IDENTITY AS EXPECTED (present=0)` — which
is the only proof a control really was a control.

| line | control (lane absent) | run A / B (lane present) |
|---|---|---|
| `IO::Modification::RefreshList` | *(no such site)* | `calls=1..2  worked=1..2  divergences=0` |
| `IO::PackageFile::ReadPackfile` | `calls=4 worked=4 divergences=0` | `calls=6..7`, **2-3 `+0x90` divergences** — §7 |
| `Object::Factory::Startup [probe]` | `chain=416 nodes=415` **DIVERGENCE** | identical **DIVERGENCE** |
| `standalone_crash.log` | 0 bytes, rc=0 | 0 bytes, rc=0 |

Both non-lane divergences are accounted for. `Object::Factory::Startup` is
**pre-existing and identical in the control** — not this lane's, and not caused
by the mounted mods either, since the control ran with the same package set. The
`ReadPackfile` lines are §7 and are an oracle defect, not a port defect.

Note what the control also establishes: `ReadPackfile calls=4` without this lane
and 6-7 with it. The extra calls are RefreshList's, one per `.pkm`, and they are
the reason §7 surfaced now and not in lane PKG3's own runs.

---

## 4. The oracle, and exactly what it can and cannot fail on

RefreshList destroys and rebuilds `this->mods` from scratch: it frees every
record, re-scans the pack directory, re-reads every `.pkm` header and re-reads
the registry. Nothing about it can be restored-and-replayed byte-wise, and a
byte compare of the result is meaningless because the two runs allocate
different heap blocks.

It **is** replayable in the sense that matters, for a reason visible in its
first instruction: `CALL 0x180010b80` destroys the existing array
**unconditionally**, before anything else. The body is therefore idempotent over
the pair (pack directory, registry) — which is what the game itself relies on
when the mods dialog's Ok calls it a second time. So:

```
run OURS   -> photograph the resulting list BY VALUE
run THEIRS on the same object (it re-destroys and rebuilds)
photograph again -> compare the two photographs
```

Retail's run is the one that **lands**, the same property `SaveList`'s oracle
has: a diffing run's live behaviour is retail's behaviour.

**By value, never by pointer** — every String and every Set node in the second
run is a different heap block, and the second run FREES the first's, so a
photograph that only pointed at them would compare freed memory
(`reference_oracle_must_snapshot_aliased_inputs.md`). Seventeen terms per mod:
`name`, `author`, `description`, `file` (all by `strcmp` on owned copies),
`enabled`, `order`, `version`, `userVersion`, `iconWidth`, `iconHeight`, `crc`,
`icon.size`, `preview.size`, `files.size`, `set.count`, the set's **in-order key
sequence**, and the `files` array element by element. The verdict and the
message come from the same walk, so they cannot disagree
(`reference_shadow_predicate_must_name_its_failing_term.md`).

**WHAT IT CANNOT CATCH,** stated rather than papered over: a wrong rotation that
still leaves a valid BST over the same keys with the same count — a differently
*shaped* but equally correct tree. The in-order walk catches a tree that lost,
duplicated or misordered a node; it does not see shape. Shape is unobservable
through this class's own API (nothing exports the tree), so a probe that could
see it would be testing our model of the layout rather than the behaviour.

---

## 5. Four things a careful reimplementation would have got wrong

**`_stricmp`, not `strcmp`.** `0x18007eefc` is `_stricmp`
(`runtime/src/crt/msvc_stricmp.cpp`). Every ordering decision in the set —
insert, lookup, and both arms of each — is a case-INSENSITIVE compare, and the
population is filenames, where case differences are the norm. A `strcmp` here
builds a different tree on the same input, and the in-order walk is the term
that would have caught it.

**The two null arms are asymmetric and both are retail's.** In the insert's
first arm a null NODE key stops the leftward descent while a null SEARCH key
descends left; in the second, a null SEARCH key ends the walk **with no insert**
while a null NODE key descends right. That is four distinct behaviours out of
two `TEST ; JZ` pairs, and "treat null as the empty string" collapses them into
one.

**The retrace's parent is captured BEFORE the rotations.** `0x1800274e6`
(`MOV RSI,[RAX+8]`) reads `n->parent` at the top of each iteration and the
rotations change it; retracing from the *new* parent revisits the subtree just
fixed. The cursor slot is also the retrace's OUTPUT — it ends null.

**The `PackageFile` constructor leaves `+0x78`, `+0x7c` and `+0x90`
uninitialised**, and RefreshList's inlined copy of it leaves exactly the same
three. `package_file.cpp` already documents the first two; `+0x90` is the third,
and it is the one that surfaced a real oracle defect in another lane — §7.

The stack `Mod` is likewise only *partly* zeroed (`0x1800af790..0x1800af7d7`):
`+0x24`, `+0x28`, `+0x2c` and the padding at `+0x51..+0x57` are left, because
every path that appends the record writes all three first and the two early-out
paths destroy it without reading them. This port uses raw storage and writes
only what retail writes, rather than `Mod m{}`.

---

## 6. What is declined, and the closure arithmetic

RefreshList's call closure is 22 distinct addresses; 14 were registered before
this lane started. This file closes 5 of the remaining 8, plus the two rotations
the retrace needs and RefreshList itself. **Three remain:**

| address | why declined |
|---|---|
| `0x1800b4540` `??1PackageFile` | **NOT THIS LANE'S ROW.** `package_file.cpp:925` already names it as lane PKG2's next wall, "(5 unported callees)". A duplicate `RegisterRuntimeAddr` is last-binding-wins and silently makes one of the two bodies dead code (`reference_grep_for_the_port_before_writing_it.md`). |
| `0x1800accc0` `?CloseFile@FileSystem@IO@@` | Lane PKG2's own document claims it (`WAVE9_LANE_PKG2.md:944`, "the teardown half"), and it **opens a door rather than closing one**: it calls `0x1800ae020`, which nothing in the tree registers. |
| `0x180028760` the `Array<Mod>` introsort | 422 instructions, and it opens **two** unregistered doors: `0x180028fc0` (its small-range insertion pass) and `0x1800c98a0` (`String::Compare`, its tie-break). Its comparator is `order` ascending with a `name` tie-break over a population where every record can tie — the "which package wins" shape lane COLL's `introsort.cpp` banner warns is decided by the **swap sequence**, not by sortedness. It needs the replayed-permutation probe on a tie-heavy corpus `introsort.cpp` built for `0x1800264a0`. That is a lane, not a footnote in this one. |

### THE SET-DIFF — and the +2 this lane must own

Both columns from the same tree and the same tool versions; "before" is this
worktree with `modification_refresh.cpp` moved aside.

```
                                          before    after    delta
tools/addr_door_audit.py
  by-address call sites                     295      312      +17
  distinct addresses                        198      212      +14
  registered addresses                     1186     1194       +8
  UNREGISTERED                               16       18       +2   <-- OWNED, see below
  non-literal call sites (blind spot)        77       79       +2

tools/port_bypass_audit.py
  ADDRESS door sites                        334      351      +17
  ... ADDR-REGISTERED                        62       74      +12
  ... UNPORTED                               93       94       +1
  reach an already-ported fn (BYPASSES)       0        0        0   <-- gate held
  NAME door raw-door sites                  477      478       +1
  are a ported fn on a live path                0        0        0   <-- gate held

tools/rawdoor_audit.py
  raw-door sites                            607      608       +1
  UNGUARDED-CALL                              0        0        0   <-- gate held
  DEFERRED-CALL                              66       66        0

tools/detour_target_audit.py                clean    clean            <-- gate held

tools/unsatisfied_export_scan.py
  distinct names through a door             848      848        0   <-- DENOMINATOR UNMOVED
  unsatisfied                               129      128       -1
  ... live (outside a diff guard)           126      125       -1

tools/runtime_census.py
  IO       called/ported                  93/75    97/79   80.6% -> 81.4%
  internal called/ported                 106/94   108/95   88.7% -> 88.0%
```

**`UNREGISTERED` went UP by 2, and this lane owns that.** The arithmetic is
exact: `RefreshList` itself was already a counted unregistered door and is now
registered (**-1**); the port introduces 14 newly-visible distinct addresses, of
which 11 are registered by this lane or another and **3 are the declined rows**
(**+3**). 16 - 1 + 3 = 18. The audit's `concentrations` list names the file:

```
    3  runtime/src/io/modification_refresh.cpp
```

and those three are `0x1800b4540`, `0x1800accc0`, `0x180028760` — grepped for
`RegisterRuntimeAddr(0x…)` across `runtime/`, `standalone/` and `code/`, no
match. This is `reference_porting_can_grow_the_cutover_surface.md`: a faithful
port that calls DLL internals by address grows the census. The three were always
in retail's call graph; what changed is that they are now COUNTED.

**And they are only counted because the doors were written with the literal at
the call site.** The first draft of this file routed all seventeen through a
two-line template — `RtAddr(cache, 0x1800b4540)` — which reads better and moved
`by-address call sites` by **zero** and `distinct addresses` by **zero**, while
pushing two more sites into the tool's own blind-spot counter. The three
unregistered doors were invisible to the instrument whose entire job is to find
unregistered by-address doors, and `UNREGISTERED` read 15 (a tidy **-1**) rather
than 18. That number was measured, seen to be too good, and traced. It is the
fourth recorded instance of
`reference_literal_grep_blind_to_variables.md`, and the reason every resolve in
this file is spelled out.

The `internal` census coverage falling 88.7% -> 88.0% is the same three
addresses appearing in the `called` column without a port.

### RE-MEASURED WITH THE FIXED `addr_door_audit.py`

Every number above came from the audit **as it stood at `fd30180`**, which had
no notion of a comment: it matched `ResolveAddr` / `RegisterRuntimeAddr` text
inside comment blocks and string literals and counted them as live call sites.
Lane ADDR fixed that (merge `a6c509f` on the integration branch, +80 lines), and
the team lead's instruction was to re-run and say which version produced which
numbers. Re-run from THIS worktree with `pwd` verified in the same command --
the tool's `ROOTS` are relative, so running one checkout's copy from another
checkout's directory measures the directory, not the checkout:

```
                                    old tool (fd30180)     fixed tool (a6c509f)
                                   before  after  delta    before  after  delta
by-address call sites                295    312    +17      270    287    +17
distinct addresses                   198    212    +14      192    206    +14
registered addresses                1186   1194     +8     1186   1194     +8
UNREGISTERED                          16     18     +2       15     17     +2
non-literal call sites (blind spot)   77     79     +2       15     15      0
```

**Every delta this lane is judged on is identical under both tools**, and the
fixed tool's "before" of 15 matches the figure the lead independently quotes for
the clean tree. What the fix changes is the ABSOLUTE level (25 of the old 295
"call sites" were prose) and the size of the tool's own blind spot, which
collapses 77 -> 15.

The last row is worth its own sentence. Under the old tool this lane appeared to
push **two more sites into the blind spot**; under the fixed tool it pushes
**none**, because the two "non-literal" sites it was blamed for were comment
text, not code. The literalisation described above is what makes the real number
zero -- every by-address door in this file is visible to the instrument.

**The `--csv` check the lead asked for passes.** `0x1800af5f0` no longer appears
in the unregistered list; the only rows this file contributes are the three
declined ones, at named lines:

```
0x1800b4540,1,runtime/src/io/modification_refresh.cpp:769
0x1800accc0,1,runtime/src/io/modification_refresh.cpp:781
0x180028760,1,runtime/src/io/modification_refresh.cpp:793
```

`engine/funcs/engine_funcs.cpp`'s concentration drops from 9 to 8 with the fixed
tool and from 10 to 9 with the old one -- the same one address leaving, which is
RefreshList's.

### TWO PREMISES HANDED TO THIS LANE, BOTH CHECKED

The lead asked that neither be built on unverified.

* **"`engine_funcs.cpp:3141` is a live address door, not prose."** TRUE.
  `git show fd30180:engine/funcs/engine_funcs.cpp` lines 3140-3141 read
  `fn_Modification_RefreshList = reinterpret_cast<...>(ResolveAddr(Module::Runtime, 0x1800af5f0));`
  It also survives the comment stripper, which is the independent second check.
* **"`RefreshList` may already have a ported body nobody registered."** FALSE
  here, checked before writing anything. `git grep -n RefreshList fd30180 --
  runtime/ standalone/ code/ engine/` returns comments, one typedef
  (`engine_funcs.h:2595`), the resolve site, and two call sites -- no body. The
  shortcut that turned another lane into a one-line change does not apply; this
  address genuinely had nothing behind it.

---

## 7. A DEFECT FOUND IN ANOTHER LANE'S ORACLE — `ReadPackfile` at `+0x90`

Every run that reaches RefreshList now carries 2-3 of these:

```
[RtDiff] DIVERGENCE IO::PackageFile::ReadPackfile  call #5
         ours=+0x90 -52017280 vs -842150451   theirs=shipped ReadPackfile
```

**It is the oracle, not the port, and not this lane's port either.**

* `-842150451` is `0xCDCDCDCD` — a debug fill, not a value any body computed.
* `package_file_read_win.cpp:634` writes `+0x90` **only inside**
  `if (formatVersion >= 2)`. Both mods are format < 2, so **neither body writes
  the field on this path.**
* The constructor does not write it either. Checked instruction by instruction
  against `0x1800b4440`: the store list runs `… +0x80, +0x88, +0x98, +0xa0 …`
  and skips `+0x90` entirely, exactly as it skips `+0x78`/`+0x7c` — which
  `package_file.cpp` already documents and already excludes from its own byte
  oracle.
* The tell is the asymmetry: `theirs` is **constantly** `0xCDCDCDCD` (PKG3's
  freshly constructed scratch) while `ours` differs on every run
  (`-52017280`, `981878656`, `-1832916784`) — because RefreshList's
  `PackageFile` lives on the **stack**, and `LoadPackages`' lives on the heap.
  That is why the control run's four `LoadPackages` calls are clean and only the
  RefreshList calls diverge.

This is `reference_oracle_must_not_compare_unwritten_bytes.md`, and PKG3's own
§95 already makes exactly this exclusion for `_fileInfo._unk04` — one field got
it and the other did not. The fix is one line at
`package_file_read_win.cpp:854`: guard the `+0x90` comparison on the same
`formatVersion >= 2` that guards the write (the census already counts it as
`armV2Block`).

**This lane did NOT edit PKG3's file** — a cross-lane edit mid-wave would
invalidate that lane's own measurement. The integrator has been told, so these
lines are not read as a `ReadPackfile` regression.

---

## 8. HONEST LIMITS

1. **`calls=1` and `calls=2` are small populations.** Both runs are clean and
   both doors are exercised, but 3 calls total across the two runs is not the
   tens of millions Wave 6-8 lanes report. RefreshList runs at boot and on a
   mods-dialog action; there is no path that calls it in a loop, so the number
   cannot be raised by running longer — only by more dialog interactions.

2. **Two mods, both format-version < 2.** The `formatVersion >= 2` arm of
   `ReadPackfile` is never taken with this corpus, so `+0x90` and the flags
   array at `+0x98` are never populated, and `preview.size` is 0 for both mods.
   The `preview.size` term therefore compared 0 against 0 and proves nothing;
   `icon.size` was non-zero and does.

3. **The AVL tree's SHAPE is not verified**, only its in-order sequence and its
   count — §4. Both rotations and the retrace are transcribed instruction by
   instruction and are exercised (the two mods carry enough files to force
   rebalancing), but a rotation bug that preserves ordering and count would pass.

4. **WHETHER LOOP 2's APPEND EVER FIRED IS NOT ESTABLISHED, and the report will
   not guess.** The override pass appends to `mods[i].files` only when two mods
   share a path. The pass certainly RAN -- its double loop is over 2x2 ordered
   pairs and both trees are non-empty -- and `files.size` and the `files`
   elements were compared against retail on every call. But nothing in the run
   prints that size, and whether `betterFields.pkm` and `DebugMenu.pkm` collide
   on any path was not measured. If they do not, both terms compared 0 against 0
   and prove nothing; if they do, they are the strongest terms in the shadow.
   An earlier draft of this section asserted the array was empty. That was an
   assumption written as a measurement, which is the failure this document
   spends section 3 avoiding, and it is recorded here rather than quietly fixed.
   **What settles it is one line**: add `files.size` (summed) to the shadow
   message next to `mods=%d`, exactly as `worked=` was added for the same
   reason, and read it off the next run. If it is 0, the corpus needs two mods
   that override the same file before this pass can be said to be verified.

5. **`Array<Mod>::SetSize` was exercised only in its append shape**
   (`SetSize(size+1, fill)`), which is RefreshList's only use. Its shrink branch
   (`oldSize > n`, the truncated-tail destructor loop) never executed and is
   transcribed but unverified.

6. **The three declined addresses are called by address from this body.** With
   the DLL present they reach the DLL. At cutover they resolve to null and the
   wall moves from `RefreshList` to whichever of the three is hit first — which
   will be `??1PackageFile` (`0x1800b4540`), on the first `.pkm` found, or
   immediately at `FileSystem::CloseFile` if the platform check rejects it. This
   lane advances the boot; it does not finish this path.

7. **The `PackageFile` constructor is CALLED, not inlined** — the one deliberate
   deviation. Justified by a store-by-store comparison of `0x1800b4440` against
   the inlined stream at `0x1800af830..0x1800af8e6` (identical, including the
   three fields neither writes), but it is a deviation and is listed here rather
   than only in the file banner.

---

## 9. Kill switch

`BANISHED_RT_MOD=0` disables **both** lane-MOD passes — the five original
exports and fourteen addresses, and this file's three exports and eight
addresses. The registration is called from `RtIoModificationRegister()` rather
than from `rt_io_port.cpp` deliberately: an A/B switch that disables half a
cluster is not a control. It sits under `BANISHED_RT_IO`.

---

## 10. `?SaveText@External@Object@@QEAA_NAEBVFilename@System@@0@Z` — DECLINED

`0x1800bb9d0`, ordinal 3614, **173 instructions**. The lane's second assignment.
Lane FNHOOK declined it and re-checked its blocker; the brief said to treat that
as real but **re-derive it independently**, because three "stale blockers" have
dissolved this wave. Re-derived, and it holds — for **two** reasons, either of
which is sufficient on its own.

### Reason 1 — `IO::TextFileStream` really is absent, and it is a whole class

Not a helper. `Runtime-x64.dll`'s export directory, read with `pefile`, carries
**sixteen** `TextFileStream` symbols:

```
0x18002b8b0  440 ??0TextFileStream@IO@@QEAA@AEBV01@@Z
0x1800ba340  441 ??0TextFileStream@IO@@QEAA@AEBVString@System@@0H@Z   <- SaveText calls this
0x1800ba530  442 ??0TextFileStream@IO@@QEAA@XZ
0x1800ba490  701 ??1TextFileStream@IO@@UEAA@XZ
0x18002b990 1236 ??4TextFileStream@IO@@QEAAAEAV01@AEBV01@@Z
0x180155980 1516 ??_7TextFileStream@IO@@6B@                            <- THE VTABLE
0x1800ba940 1884 ?Create@   0x1800bac30 2536 ?GetCurrentPos@
0x1800ba270 2821 ?GetObjectName@   0x1800ba260 2822 ?GetObjectType@
0x1800ba250 2823 ?GetObjectVersion@   0x1800ba8e0 2907 ?GetSize@
0x1800ba640 3308 ?Open@     0x1800ba900 3457 ?Read@
0x1800babd0 3858 ?SetCurrentPos@   0x1800ba280 3930 ?SetObject@
0x1800ba9f0 4556 ?Write@
```

Tree-wide, `TextFileStream` appears in **four comments and nothing else**
(`runtime/src/object/external.cpp:46`, `runtime/types/object/External.h:85`,
`:256`, `runtime/src/io/save_trace.h:10`). No type, no port, no registration —
FNHOOK's finding, reproduced by an independent grep.

**And the vtable is NOT minted.** `standalone/rt_data_mint_tables.inc` mentions
`??_7TextFileStream@IO@@6B@` exactly twice (lines 1285 and 1371) and in both
places as an *adjacency bound* for a different vtable:

```
{ "??_7Visitor@IO@@6B@", 0x1801558b0ull, 26, …, "adjacent-export:??_7TextFileStream@IO@@6B@" }
```

That is the mint generator using `0x180155980` to work out where `Visitor`'s
vtable ENDS. `TextFileStream`'s own vtable has no row. This matters more than
the missing bodies, and it is why "port the constructor and stub the rest" is
not an option: **SaveText stack-constructs the stream and then hands it to the
subject's own virtual `Serialize`** (`CALL qword ptr [RAX + 0x28]` at
`0x1800bbb12`), and that Serialize drives the stream through its virtuals —
`Write`, `GetCurrentPos`, `SetCurrentPos`. A port of SaveText alone builds an
object with a null vtable pointer and vcalls it. That is attempt 14 exactly:
`[RtResolve] MISS -> NULL`, then VCALL-of-0 on an all-zero object.

`SaveText`'s closure also includes `IO::FileStream::Create` (`0x1800aa8f0`),
which nothing in `runtime/`, `standalone/` or `code/` mentions either, and
`?CloseFile@FileSystem@IO@@` (`0x1800accc0`) — the same row §6 declines.

### Reason 2 — there is NO DRIVER, so a port would report `calls=0`

Independent of the dependency. `SaveText` has exactly **one** caller in the
whole tree:

```
code/Game/Dialog/spritecaptureui.cpp:219   fn_External_SaveText(pending, pathFilename, filenameRsc);
```

— `SpriteCaptureUI::OnCreate`, the sprite-capture **dev tool**, reached only
when a user drags a capture box in that dialog and the pending capture's
`saveFrameCount` reaches 1. No harness in this project opens that dialog:
`BANISHED_AUTOMENU_*` drives the front-end menu buttons and
`BANISHED_PAUSEMENU_*` the in-game menu, and neither has a token for it.

The audit agrees. `SaveText`'s only door site tree-wide is

```
?SaveText@External@Object@@QEAA_NAEBVFilename@System@@0@Z   engine/funcs/engine_funcs.cpp:3889
```

which is a **resolve-and-store at startup**, not a call. It is one of the
"merely resolved" symbols the brief distinguishes from the wall RefreshList
was, and porting it moves the DLL-less boot forward by nothing.

So a port would be verified by a green `calls=0` line — precisely
`reference_uncalled_port_is_not_verified.md`.

### What building it would cost, and what would make it honest

Stated so the next lane can price it rather than rediscover it:

* **~17 rows**: the 16 `TextFileStream` exports plus `IO::FileStream::Create`,
  and a **mint row for `??_7TextFileStream@IO@@6B@`** — which needs the vtable's
  slot count established first (the generator currently only knows the address
  as `Visitor`'s upper bound).
* **A synthetic subject, not a fake.** `reference_synthetic_subject_beats_a_fake.md`
  applies and the answer is favourable here: `Write`/`Read`/`SetCurrentPos` over
  a temp file are **replayable**, so the honest oracle is a probe that writes a
  known object through ours and through the shipped stream into two files and
  compares the bytes — a real differential, not a shadow. That is the same
  "a Serialize on the WRITE path IS replayable" argument
  `reference_write_path_serialize_is_replayable.md` records, and it means this
  class is *well* placed for verification once someone owns it.
* It is `IO::Stream`'s subtree, which is lane IOA/FS territory, not this lane's.

**RefreshList was the deliverable that mattered and it is done. SaveText is
declined on a blocker measured today, not inherited: the class is absent, its
vtable is unminted, and nothing in this project can call the function.**
