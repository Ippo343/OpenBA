# WAVE 19 — PLAN: the IO namespace to its ceiling

**Written 2026-09-10 by the integrator at `1cc90a2`, immediately after Wave 18
closed. Not yet dispatched.**

Every number below was **measured for this document on this tip**, not carried
over. Wave 18's plan took **six corrections from its own lanes**, all from
repeating prose instead of running a query, so each fact here says how it was
checked. Where a fact is *inherited and not re-verified*, it is labelled
**[INHERITED]** and re-verifying it is a lane's first job.

Read alongside: [WAVE18_CLOSED.md](WAVE18_CLOSED.md) (§4 instruments, §7 limits),
[WAVE18_KICKOFF.md](WAVE18_KICKOFF.md) §4 (doctrine, binding here too),
[WAVE17_LANE_CEIL.md](WAVE17_LANE_CEIL.md) (the triage this builds on).

## 0. Why IO, and what "done" is

Chosen over Graphics on three grounds, of which the second is the decisive one:

1. **Bigger real gap.** IO 36 rows vs Graphics 25.
2. **It carries debt with a deadline that we created.** Wave 18 ported
   `?SaveImageToFile@ImageBuffer@Video@@` and moved the screenshot cutover hole
   from *no implementation* to *an implementation with four unresolved IO deps*.
   Until those four land, that body still dies at cutover. **Graphics has no
   equivalent obligation.**
3. **Highest ceiling of the three at 97.5%** — nearly all of IO's gap is ordinary
   portable work rather than rows that will turn out unpromotable.

**Success criterion is the IDENTITY, never a percentage** (Wave 18 §4):

```
ported >= called - unpromotable      today:  83 >= 122 - 3 = 119     36 SHORT
```

`called` sits in **both halves** of the ceiling fraction, so any lane opening a
door moves the percentage without changing anything real. **Report the identity
plus a set-diff of what entered `called`. Never a percentage delta.** Expect
`called` to grow — the four screenshot deps already did that to IO in Wave 18.

## 1. The 36 rows, named and partitioned

`tools/runtime_census.py --namespace IO --unported`, minus the 3 UNPROMOTABLE
(HARMFUL) rows. Arithmetic checked: **14 + 9 + 13 = 36.**

### DATA — 13 vtables + 1 typeinfo (14)

`??_7{BinaryFileStream, BinaryReader, BinaryWriter, CompressedFileStream,
CompressedMemoryStream, CompressedStream, FileStream, MemoryStream, PackageFile,
ReaderWriter, Stream, TextFileStream, VisitorStream}@IO@@6B@`
· `?_typeInfo@PackageFile@IO@@0VBaseTypeInfo@Object@@A`

### ctor / dtor (9)

`??0BinaryFileStream` · `??0TextFileStream` · `??0VisitorStream` ·
`??1BinaryFileStream` · `??1FileStream` · `??1FileSystem` · `??1Modification` ·
`??1PackageFile` · `??1Persistence`

### CODE (13)

`?BeginDelete@Persistence` · `?BeginGetSaveGameInfo@Persistence` ·
`?BeginLoad@Persistence` · `?GetSaveCount@Persistence` ·
`?GetSaveFilename@Persistence` · `?GetSaveInfo@Persistence` ·
`?CloseFile@FileSystem` · `?CreatePath@FileSystem` · `?Create@FileStream` ·
`?Commit@CompressedStream` · `?CompressWindow@CompressedStream` ·
`?Create@CompressedStream` · `?SetupFileSystem@Modification`

## 2. IO IS NOT GREENFIELD — the single most important scoping fact

`runtime/src/io/` already holds **35 files**, **20 kill switches** in
`rt_io_port.cpp` (`BANISHED_RT_{IO, FILESYSTEM, PERSISTENCE, PKG, PKG2, PKG3,
MEMSTREAM, ENCODING, BINWRITER, CSOPEN, CSREAD, CSRELEASE, SERIALFAM, MOD, FIND,
OPENFILE, FSDIRS, FSSTARTUP, PERSSTARTUP, REGREAD}`), and **nine probe files**.
Layouts are pinned in `runtime/types/io/` — `FileStream.h` covers
`IO::FileStream` and `IO::BinaryFileStream` completely (`sizeof 0x50`, filename
`+0x18`, FileInfo `+0x20`, Win32 handle `+0x38`, cursor `+0x2c`), and
`Persistence.h`, `Serializer.h` and `Stream.h` are equally detailed.

**Wave 18 proved twice what this is worth:** row 7 and `PlatformSerialize` were
both "a day" that came in well under, *because nothing re-derived a layout*, and
in each case an Array helper turned out **already ported by another lane**, found
by grepping the address before writing a line.

**So every lane's first hour is grep, not disassembly.** Grep the mangled name,
the retail address, `RtOptIn(` / `RtWaveEnabled(` / `if (table)` gates, AND
`standalone/` plus the `BANISHED_*` list for an existing driver. A body ported but
unregistered is indistinguishable from an absent one to the census.

**A worked example of what that grep must survive**: `?Commit@CompressedStream@IO@@`
looks ported — `runtime/src/io/compressedstream.cpp` names it eight times. It is
**not**: line 327 reaches it through `RT_EXPORT_ONCE`, a *door*, and the
surrounding comment says retail inlines the guard and calls the exported body.
Neighbour, not duplicate.

## 3. Phase 0 — three cheap cells that change the worklist. Run these FIRST.

Each is well under a lane and each either **removes rows** or **converts them from
port work to something cheaper.** Doing them after the porting lanes wastes the
saving.

### 0a. The abstract-base NO-OP question — could delete 2 rows and raise the ceiling
`??_7ReaderWriter@IO@@6B@` is **30 of 36 slots purecall** and `??_7Stream@IO@@6B@`
is **5 of 7** [INHERITED, CEIL §3.4]. An abstract base admits no leaf instance, so
the table can only ever be the transient `+0x00` — lane OBJDATA's exact proof for
`??_7Base@Object@@6B@`. `encoding.cpp` states in a **comment** that both Encoding
constructors stamp `??_7ReaderWriter@` first and then overwrite it.

**CEIL deliberately refused to tag these**, because the claim rests on a comment
and `reference_a_written_recipe_can_be_wrong.md` is precisely a good lane's
written recipe being wrong. **Settle it from the shipped bytes** — read the two
Encoding constructors as disassembly. If it holds, two rows leave permanently and
IO's ceiling rises to ~95.8%. One afternoon.

### 0b. The five unminted IO vtables — a harvest gap, not port work
**VERIFIED for this document.** `??_7BinaryFileStream@` (`0x1801554d0`),
`??_7CompressedFileStream@` (`0x1801555f0`), `??_7FileStream@` (`0x180154960`),
`??_7TextFileStream@` (`0x180155980`), `??_7VisitorStream@` (`0x180155a20`) each
appear **zero times by VA** in `standalone/rt_data_mint_tables.inc`.

**Reproduce the trap before trusting any grep here**: searching by *name* returns
4, 2, 2, 0, 0 hits — those are `adjacent-export:` labels on **other** rows. **The
name grep looks like a hit and is not one.** This is a `tools/rt_mint_gen.py`
harvest gap; fixing the generator is cheaper than five hand-written rows, and two
of the five are ATTENDED today.

### 0c. Re-verify the trap counts before scoping any DATA lane
Every `slots / ported / TRAP` figure in CEIL §3.4 is **[INHERITED]** from a
readiness run on `3da1503`. Wave 18 has since merged four lanes. `BANISHED_RT_MINT_SHADOW=1`
re-runs that screen. **A trap-carrying row is unfinished, not unpromotable** —
CEIL predicted 40 NO-OPs and measured 12 because it asked whether promotion would
*matter* before asking whether it was *possible*.

Cheapest rows if the counts hold: `??_7BinaryReader@` **1 trap of 26**,
`??_7Stream@` **1 of 7**.

## 4. Lane split — by cluster, because the clusters share files

| lane | rows | subject |
|---|---|---|
| **FILE** | 11 | The screenshot debt + the FileStream family |
| **PERSIST** | 7 | `IO::Persistence` — save enumeration and load kickoff |
| **CSTREAM** | 6 | `IO::CompressedStream` and its two subclasses |
| **IOBASE** | 12 | The abstract bases, PackageFile, Modification, VisitorStream |

**11 + 7 + 6 + 12 = 36.**

### Lane FILE (11) — the highest-priority lane
`?CreatePath@FileSystem` · `?Create@FileStream` · `?CloseFile@FileSystem` ·
`??0BinaryFileStream` · `??1BinaryFileStream` · `??1FileStream` ·
`??0TextFileStream` · `??1FileSystem` · `??_7FileStream` ·
`??_7BinaryFileStream` · `??_7TextFileStream`

**The first four are exactly the four exports `SaveImageToFile` depends on**
(verified present in the unported set for this document). **This lane closes the
cutover hole Wave 18 opened** — that is its headline, not its row count. It should
finish by re-running Wave 18 lane VRES's BMP oracle **with the DLL absent**, which
is the only test that proves the hole is shut.

### Lane PERSIST (7)
`??1Persistence` · `?BeginDelete` · `?BeginGetSaveGameInfo` · `?BeginLoad` ·
`?GetSaveCount` · `?GetSaveFilename` · `?GetSaveInfo`

`persistence.cpp`, `persistence_startup.cpp` and `persistence_registry_win.cpp`
already exist and `Persistence.h` is pinned — grep hard before scoping. The three
`Begin*` methods are **asynchronous**: expect a stored completion callback, which
is the root cause that held back all six Wave 8 audio rows
(`reference_audio_holdbacks_share_a_callback_root_cause.md`). **Read that before
estimating this lane.** `BANISHED_AUTOLOAD` and `BANISHED_SAVE_TEST` drive real
routes here.

### Lane CSTREAM (6)
`?Commit` · `?CompressWindow` · `?Create@CompressedStream` ·
`??_7CompressedStream` (**8 traps** [INHERITED]) · `??_7CompressedFileStream` ·
`??_7CompressedMemoryStream` (**6 traps** [INHERITED])

Four `compressed_stream_*.cpp` files and two probes already exist. `?Commit` is
the real compression body and is reached today through `RT_EXPORT_ONCE` — see §2.

### Lane IOBASE (12)
`??_7Stream` · `??_7ReaderWriter` · `??_7BinaryReader` · `??_7BinaryWriter` ·
`??_7MemoryStream` · `??_7VisitorStream` · `??_7PackageFile` ·
`?_typeInfo@PackageFile` · `??1PackageFile` · `??0VisitorStream` ·
`??1Modification` · `?SetupFileSystem@Modification`

**Depends on phase 0a** — if `??_7ReaderWriter@` and `??_7Stream@` are NO-OPs this
lane sheds two rows before it starts. `??_7BinaryWriter@` is **26/26, TRAP=0**
[INHERITED] — the only fully complete row in CEIL's whole subject, blocked purely
on attendance; `[Encoding] ATTENDANCE` corroborates it never ran on four routes.
**"No route driven" is not "no route exists"** — Wave 18's `BANISHED_FORCE_DEVICE_LOST`
was sitting in the tree the whole time.

## 5. What would make this wave fail

* **Treating the 36 as a work list without phase 0.** Up to 7 of them may not be
  port work at all.
* **Believing an [INHERITED] number.** Every trap count predates four merges.
* **Skipping the grep.** IO has 35 files; Wave 18 hit already-ported dependencies
  twice in two rows.
* **Scoping against the percentage.** It will move for reasons unrelated to
  progress, and `called` will grow as lanes open doors.
* **Declaring "no oracle" or "no route".** Wave 18 refuted that four times
  (`BANISHED_FORCE_DEVICE_LOST`, `tool_harness.cpp`, a write-path Serialize
  replay, and a file-level BMP byte comparison). **A write-path `Serialize` is
  replayable; an artefact-producing body is compared by its artefact.**

## 6. Instruments — Wave 18's hardest-won section, and it applies directly

Six instruments in Wave 18 printed a verdict without stating what they examined,
and **three of the fixes landed in tools this wave will use**: `abi_retslot_audit`
(`fa85957`), `dispatch_slot_map --check` (`636b521`, was checking 63 of 110 while
reporting itself clean), `unsatisfied_export_scan` (`42bd07a`, was inflating
111 → 88 and **manufacturing** work).

1. **Quote a green WITH its denominator.** `N suspicious` alone is quotable out of
   context; `N of M, K unaudited` is not. Run an instrument **in the tree you are
   measuring** — several derive their root from the script path.
2. **Every screen needs a positive control that MUST read non-zero**, verified to
   print before any zero is trusted.
3. **Score each control separately, on its APPLICABLE cases only.** A *union*
   denominator overstates a blind control; an *inapplicable-case* denominator
   understates a working one. Both happened in Wave 18.
4. **A control must produce a comparable divergence, not UB**, and must catch via
   the mechanism it names.
5. **Read `calls=`, not just `divergences=`.** `calls=0` has four causes that
   invert the conclusion, and `calls=0` with `worked>0` means the body ran every
   time and always took the uncovered branch.
6. **Prefer an instrument that can say "I MEASURED NOTHING"** over one that can
   only say "no difference found". From outside they are indistinguishable, and
   this project has repeatedly shipped the second while believing the first.
7. **A control written by the same hand as the thing it checks will forgive
   exactly the flaw that hand introduced.** The integrator proved this in Wave 18
   by certifying an additivity property with a checker tuned to pass it.

## 7. Standing doctrine — see WAVE18_KICKOFF.md §4 in full

Condensed, with the ones that bit hardest last wave:

* **Prose is a hypothesis; a grep is the answer.** A stated blocker is a claim
  with a **timestamp** — four for four were stale in Wave 18, and one was a lane's
  own earlier judgement.
* **`AEAA` means PRIVATE, not unexported.** This DLL exports its private members.
* **Register by name AND address.** `ResolveAddr(Module::Runtime, …)` is
  registry-first, not "the DLL"; `ResolveExport` falls through silently.
* **Read the disassembly, not Ghidra's C.**
* **Do not "fix" a faithful crash with a null check** — find the upstream null.
* **`game_dir_lock.sh acquire <lane>` gets a statement of its OWN** — no pipe, no
  `;`, no trailing echo. **Prefer the self-releasing `run` form.** Compile and fix
  **without** the lock; acquire only for a batch you intend to measure.
* **No inline shell quoting of commit text** — heredoc, double-quoted `-m`,
  backticks all rewrite it. Write + `git commit -F`. Same for `git merge -F`.
* **Private worktree per lane**; report path, branch and commit in every update.
* **Stop editing once you report**, and if re-tasked, land it as a clearly-marked
  post-report tranche. **Sealing forbids altering, not annotating** (§6.16).
* **Commit early and often.**

## 8. Not in this wave, but decide it consciously

**The cutover boot wall is now `?PlatformConfigure@Engine@1@` (`0x1800a1670`) —
an ENGINE row, not IO or Graphics.** Wave 18 worked because its census and cutover
targets happened to be superset/subset. **They have now diverged**, and this plan
serves the census objective only.

`PlatformConfigure` is unusually cheap for a wall: CLAUDE.md documents it as the
Win32 launcher dialog, and `HarnessGuardUnattendedBoot()` already has a seam on
it. Engine sits at 33 called / 23 ported. **A one-lane Engine cell could run
beside this wave** — the lock is the only shared resource, and lane GATE showed a
boot-gate lane needs it only in short batches.

Deferred and named so they are not rediscovered:
* `0x180053f00` (`Array<Array<uint8>>::SetSize`, 107 instructions) — **Collection**
  lane work, opened by Wave 18's `PlatformSerialize`.
* Three Wave 18 rows are **PORTED, NOT VERIFIED** with written expiries
  (`??1Configuration@Video@@`, `AdjustDisplay`, seven `??_7ImageBuffer` slots).
  None is IO's to close.

## Appendix A — the 36 rows, EXACT mangled names

Generated verbatim from `tools/runtime_census.py --namespace IO --unported` on
`1cc90a2`, so a lane can grep these directly. **The body text above abbreviates
signatures for readability — do NOT grep those.** A partial mangled name silently
misses, and this project requires the exact name AND the retail address before a
line is written.

A note on counting, because it bit while writing this: piping the census through
`grep` appends a `Binary file (standard input) matches` line, which made the set
read as **37**. The real count is **36**, and it agrees with the identity
(`119 - 83`) and with the lane split (`11+7+6+12`). Three independent routes.

```
??_7BinaryFileStream@IO@@6B@
??_7BinaryReader@IO@@6B@
??_7BinaryWriter@IO@@6B@
??_7CompressedFileStream@IO@@6B@
??_7CompressedMemoryStream@IO@@6B@
??_7CompressedStream@IO@@6B@
??_7FileStream@IO@@6B@
??_7MemoryStream@IO@@6B@
??_7PackageFile@IO@@6B@
??_7ReaderWriter@IO@@6B@
??_7Stream@IO@@6B@
??_7TextFileStream@IO@@6B@
??_7VisitorStream@IO@@6B@
??0BinaryFileStream@IO@@QEAA@XZ
??0TextFileStream@IO@@QEAA@AEBVString@System@@0H@Z
??0VisitorStream@IO@@QEAA@XZ
??1BinaryFileStream@IO@@UEAA@XZ
??1FileStream@IO@@UEAA@XZ
??1FileSystem@IO@@QEAA@XZ
??1Modification@IO@@QEAA@XZ
??1PackageFile@IO@@UEAA@XZ
??1Persistence@IO@@QEAA@XZ
?_typeInfo@PackageFile@IO@@0VBaseTypeInfo@Object@@A
?BeginDelete@Persistence@IO@@QEAA_NAEBVString@System@@@Z
?BeginGetSaveGameInfo@Persistence@IO@@QEAA_NAEBVString@System@@H@Z
?BeginLoad@Persistence@IO@@QEAA_NAEBVString@System@@HPEBUSaveBlock@12@@Z
?CloseFile@FileSystem@IO@@QEAAXAEAUFileInfo@12@@Z
?Commit@CompressedStream@IO@@QEAAXXZ
?CompressWindow@CompressedStream@IO@@AEAAXH@Z
?Create@CompressedStream@IO@@QEAA_NAEAVStream@2@H@Z
?Create@FileStream@IO@@IEAA_NAEBVFilename@System@@@Z
?CreatePath@FileSystem@IO@@QEBA_NAEBVFilename@System@@@Z
?GetSaveCount@Persistence@IO@@QEBAHXZ
?GetSaveFilename@Persistence@IO@@AEBA?AVFilename@System@@AEBVString@4@@Z
?GetSaveInfo@Persistence@IO@@QEBA?AUSaveGameInfo@12@H@Z
?SetupFileSystem@Modification@IO@@QEAAXXZ
```
