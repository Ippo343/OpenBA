# WAVE 23 LANE IOPORT — all 16 IO rows

Branch `wave23/ioport`, cut from `wave23/base` at `a60351e3`. Private worktree
`C:\code\GIT\untitled_rev_eng_project\.claude\worktrees\agent-a3de6559ccbdf65d1`.

Every binary read below is against **retail** `C:\GOG Games\Banished\Runtime-x64.dll`.
The profile build was not opened for anything in this report.

---

## 0. HEADLINE — MY BRIEF'S ONE NAMED OPEN ITEM WAS STALE, AND IT MEANT THE OPPOSITE

I was named addressee for this, quoted verbatim from my brief:

> `??_7ReaderWriter@IO@@6B@` currently produces an `UNMATCHED` line in
> `tools/unpromotable_slot_check.py` with an **inherited `?` numerator**
> (`?/26 ported [NUMERATOR STILL INHERITED], 25 purecall [RE-DERIVED]`). This
> pre-dates Wave 23 and is a known open item.

**Three things in that paragraph are wrong, and the third is the useful one.**

1. The numerator is not `?`. It has read `1/26` since Wave 21 lane ENC, which was
   the assigned addressee and discharged it. `reg_mint_promoted.cpp:1321` carries
   the re-derived value plus a `[CONV: M=TABLE]` declaration.
2. It is not an open item. It is a **closed** one.
3. **The `UNMATCHED` line is the checker working exactly as designed, and its
   wording cannot say so.** The freeze is on the `(symbol, EXACT field text)`
   PAIR, and the tool's own comment says editing a field "is permitted and is
   precisely what makes that tag require a declaration". So a lane that correctly
   re-derives an inherited numerator *necessarily* produces an `UNMATCHED` line.
   The message printed for that — `UNMATCHED (tag deleted or its field edited)` —
   is identical to the message for a silent deletion.

That is a diagnostic field structurally incapable of expressing the answer it is
read for. The cost is measured, not hypothetical: a correct discharge in Wave 21
was still being briefed **two waves later** as an open defect, and I was
dispatched to fix something that was already fixed.

I proved the mechanism rather than arguing it: I made a second, independently
correct tag edit (§2.2) and **it produced a second `UNMATCHED` line**. Then I
fixed the instrument (§4).

---

## 1. THE BOARD, RE-DERIVED ON MY OWN TIP (rule 1)

```
tools/runtime_census.py            # branch-local, wave23/ioport at a60351e3
IO   407 total   216 called   200 ported   92.6%   ceiling 97.7%
IO gap 16 |  BODY 3   DTOR 1   VTABLE 8   DATA 4
```

**The kickoff's IO line re-derives EXACTLY** — 16 = 3 BODY + 1 DTOR + 8 VTABLE +
4 DATA, and all 16 symbols match by name. No correction to report. All 16
addresses were then pinned from the export directory with `pefile` rather than
carried from any doc.

**After this lane (branch-local, rule 13):**

```
IO   407 total   216 called   202 ported   93.5%   ceiling 97.7%
IO gap 14 |  BODY 1   DTOR 1   VTABLE 8   DATA 4
TOTAL      1314 called  1139 ported  86.7%   (from 1137 / 86.5%)
```

The integrator must re-measure on the merged tip. Wave 21 published from lane
reports and had to correct itself; Wave 22's merged System figure matched neither
lane that moved it.

---

## 2. SECTION A — THE FOUR FUNCTION ROWS

### 2.0 Rule 2 discharged first

Grepped `runtime/ standalone/ engine/ code/` for all four mangled names before
writing anything. All four were genuinely absent — only `RT_EXPORT_ONCE` /
`ResolveExport` call sites reaching the DLL, no bodies. No repeat of the
`?StrCompare@` twenty-wave rediscovery.

### 2.1 PORTED — `?Create@MemoryStream@IO@@QEAA_NH@Z` (0x1800ae7f0, ord 1880)

26 instructions. Full retail disassembly read; five stores:

```
1800ae852  MOV [RBP+0x18],RBX   _data      = MemAlloc(size, 4)
1800ae856  MOV [RBP+0x28],ESI   _size      = size
1800ae859  MOV [RBP+0x24],0     _cursor    = 0
1800ae860  MOV [RBP+0x8],1      _direction = 1   <-- WRITING
1800ae867  MOV [RBP+0x20],1     _ownsData  = 1
1800ae86b  MOV AL,1             return true, UNCONDITIONALLY
```

**A tree claim re-derived, not inherited.** `compressed_stream_probe.cpp:143`
already said this is `?Open@MemoryStream@IO@@QEAA_NH@Z`'s body "except
`MOV dword ptr [RBP+0x8],1` at 0x1800ae860". I checked it against the bytes:
**true, byte for byte**, including the cited address.

Ported in `runtime/src/io/compressedstream.cpp` beside the existing `Open` port,
registered by name AND address. The unconditional `true` on allocation failure is
retail's and is reproduced unguarded (CLAUDE.md's rule — a guard would skip the
behaviour, not restore it).

**Oracle: FULL, by replay into a scratch object** — the same instrument the
adjacent `Open` port already uses. Compares `_size`, `_cursor`, `_direction`,
`_ownsData` and the return value; deliberately does NOT compare `_data`, because
two independent allocations cannot share an address and comparing it would fail
100% of calls for a reason that is not a defect.

**Why the oracle is not vacuous here:** `_direction` is the single field that
distinguishes `Create` from `Open`, and it IS in the compared set. If this body
had been written as a copy of `Open`, the shadow fires on every call. That is the
exact defect `compressed_stream_probe.cpp` records as having cost two runs.

### 2.2 PORTED — `?SetObject@TextFileStream@IO@@QEAAXAEBVString@System@@0H@Z` (0x1800ba280, ord 3930)

42 instructions. **The argument order is the whole risk in this row**, and this
project has had this exact pair backwards before (`runtime/types/io/FileStream.h`
records carrying `objectName`/`objectType` swapped until retail settled it). So I
derived it from the REGISTERS, not from Ghidra's parameter names:

```
1800ba29e  MOV RDI,RDX   arg1 ... 1800ba313  MOV [RBX+0x60],RAX   objectType
1800ba29b  MOV RSI,R8    arg2 ... 1800ba2d2  MOV [RBX+0x58],RAX   objectName
1800ba298  MOV EBP,R9D   arg3 ... 1800ba2de  MOV [RBX+0x50],EBP   objectVersion
```

`SetObject(a, b, v)` sets **objectTYPE = a, objectNAME = b** — the reverse of the
reading order the layout suggests. Corroborated two further ways: the already-ported
`RtIo_TextFileStream_Ctor` passes `(self, type, name, version)`, and `?GetObjectName@`
returns +0x58 while `?GetObjectType@` returns +0x60.

The version store sits BETWEEN the two string assignments. The self-assignment
guard at `1800ba2a4` is reproduced: it skips the whole release-and-addref when the
incoming String holds the same `char*`. Dropping it frees the buffer outright
whenever the field holds the last reference.

**This row has an immediate live caller**: `RtIo_TextFileStream_Ctor` already
calls this symbol through `RT_EXPORT_ONCE`, so registering it converts an
existing call site with no edit at the call site.

**Oracle: A/B on SYNTHETIC subjects, and why it is not a replay is the point.**
Running theirs after ours is **vacuous** — ours has already stored the incoming
pointers, so theirs takes the self-assignment guard on both strings and does
nothing, and the comparison passes regardless of what this body did. Running
theirs on a scratch whose fields ALIAS the live strings is worse: both bodies
decrement the same refcount, double-releasing a live string. So each armed call
builds independent TextFileStream-shaped buffers and freshly allocated refcounted
strings.

Compared: all three written fields **and the refcount of every string involved** —
the refcounts are where a dropped addref or a doubled release actually shows up.

Branch coverage, stated honestly:

| case | branch | covered |
|---|---|---|
| (a) old == nullptr | fresh assign | YES |
| (b) old != new, refcount > 1 | release-without-free, 1800ba2c7 | YES |
| (c) old == new | the self-assignment guard, 1800ba2a7 | YES |
| (d) refcount reaches 0 | release-WITH-free, 1800ba2b7 | **NO — static byte reading only** |

(d) is not driven because it means handing the DLL a string it will `MemFree`, at
which point the two subjects need separately-allocated-but-equal strings and the
compared pointers legitimately differ.

**Negative control:** `BANISHED_RT_TFS_SETOBJ_CONTROL=1` writes `nullptr` to
`objectType`. +0x60 **is** in the compared set, so the control corrupts something
the probe actually reads. It is also not the mirror failure: +0x60 must be EQUAL
across both subjects, because both are handed the same incoming pointer.

### 2.3 SCOPED, NOT PORTED — `?SetupFileSystem@Modification@IO@@QEAAXXZ` (0x1800aebb0, ord 4030)

Wave 20 §8 item 7, unscoped for two waves. **Here is the scope.**

**Extent — both bounds, and they nearly coincide:**

| method | bound | value |
|---|---|---|
| `.pdata` entry | LOWER (MSVC chains fragments) | 0x1800aebb0..0x1800af5e9 = **2617 bytes** |
| adjacent export | UPPER | next export 0x1800af5f0 = **2624 bytes** |

~572 instructions, 33 call sites, 16 distinct callees.

**It is NOT dependency-blocked, which is the finding.** 13 of the 16 callees are
already ported and registered. The 3 that are not — `0x1800beec0`, `0x180028cd0`
(an Array grow), `0x180027380` (a tree insert) — are reached through
registry-first doors and **fall through to the DLL**, exactly like the
`deleting_dtor.cpp` family's unported callees. "Blocked on callees" would have
been the wrong holdback.

**What it actually does** (from the decompilation): walks every loaded `External`
recording which packfile each came from into a tree; `ClosePackages`; rebuilds the
package list from the `Modification` array; `LoadPackages`; then walks the
Externals again and, for each whose packfile id CHANGED, either **ForceUnloads**
it or queues it for reload.

**The three real blockers, in order:**

1. **SIZE/SHAPE.** Two nested loops over `ExternalFactory::FindNext`, an inlined
   tree find+insert+erase, an inlined `strcmp`, an inlined Array grow, and ~10
   inlined String releases. This is a dedicated pass, not a slot-sweep item.
2. **NO USABLE VERIFICATION ROUTE.** Its only caller is the mod-remount path
   (`runtime/src/engine/loops.cpp:698`, guarded by a mod-change event). CLAUDE.md
   records that route as **currently broken** — `ModDialog_ToggleMod_Impl+0x43`
   ACCESS_VIOLATION, reproduced independently by two parties. Its own call site
   already carries `RT_DIFF_SHADOW(true, "no oracle: mounts the mod filesystem
   and wakes the loader thread")`.
3. **BLAST RADIUS.** This is the function that decides `ForceUnload`. A wrong port
   silently unloads mods, and CLAUDE.md separately records that mod/package state
   is delicate (a save made with mods mounted stalls or crashes on load once the
   `.pkm` are moved aside).

**ADDRESSEE: the Wave 24 lane that owns the `ModDialog_ToggleMod_Impl` crash.**
This row should be scheduled *after* that fix, not before, because blocker 2
dissolves the moment the mods route works. I am explicitly NOT handing this to
"the next lane to touch IO" — that is a condition, not an addressee.

### 2.4 SCOPED, NOT PORTED — `??1PackageFile@IO@@UEAA@XZ` (0x1800b4540, ord 639)

**Mangling re-derived, not inherited:** the export at 0x1800b4540 is `UEAA` —
virtual. The non-virtual `QEAA` spelling resolves NULL silently.

**Extent:** `.pdata` LOWER 318 bytes, adjacent-export UPPER 336 bytes. ~87
instructions. Materially smaller than §2.3 and mechanical.

**Body** (read, not recipe-followed — the warning in my brief is that a prescribed
dtor port once swapped a body that destroys every element for one that destroys
only the first):

```
vptr = &PackageFile::_vftable_
FUN_18000c180(this+0xc0)      ~FileInfo        [PORTED, registered by address]
FUN_1800269a0(this+0xb0)                       [UNPORTED -> falls through]
FUN_1800112a0(this+0x98)      Array<u8> dtor   [PORTED]
FUN_1800111a0(this+0x80)      Array<i32> dtor  [PORTED]
String release x5 at +0x60, +0x58, +0x50, +0x48, +0x40   (FIVE, separately inlined)
ListCleanup(this+0x30) ; ListCleanup(this+0x20)
Object::External::~External(this)
```

**Not blocked on callees** — 3 of 4 helpers ported, the 4th falls through.

**Why I did not port it:** it has **no oracle**. A destructor cannot be replayed
(running theirs after ours double-frees), and the two-scratch A/B the
`MemoryStream` dtor uses needs a constructible subject — `PackageFile` is 0xc8+
bytes with a `FileInfo`, two lists and five strings, so a fabricated subject is a
fake rather than a synthetic one. With the game directory unavailable (§5) I
could not have validated it either way. Shipping an unverifiable dtor to move one
census row is the trade this project has repeatedly paid for.

**Its real value is NOT the census row** — see §3.2: it is **one of the two retail
stampers of `??_7PackageFile@IO@@6B@`**, which is the narrowest stamper gap in
the whole IO vtable set.

**ADDRESSEE: Wave 24 lane OBJ or whoever next takes `??_7PackageFile@IO@@6B@`.**
Porting this dtor plus `??0PackageFile@IO@@QEAA@AEBV01@@Z` (the copy ctor, the
other retail stamper) drives that row's retail stamper count 2 -> 0. That is a
two-body, fully-specified task and it is the only IO vtable within reach of
promotion.

---

## 3. SECTION B — THE EIGHT VTABLES

### 3.1 Extent, readiness and promotability are three questions (rule 4)

**Slot state** — `tools/runtime_census.py` PROMOTION BACKLOG, and
`tools/vtable_dump.py` against retail for the two I re-derived by hand:

| row | addr | ours/slots | state |
|---|---|---|---|
| `??_7CompressedFileStream@IO@@6B@` | 0x1801555f0 | 9/9 | COMPLETE |
| `??_7CompressedMemoryStream@IO@@6B@` | 0x180155560 | 9/9 | COMPLETE |
| `??_7CompressedStream@IO@@6B@` | 0x180155640 | 9/9 | COMPLETE |
| `??_7FileStream@IO@@6B@` | 0x180154960 | 8/8 | COMPLETE |
| `??_7ReaderWriter@IO@@6B@` | 0x1801553b0 | 1/1 | COMPLETE |
| `??_7Stream@IO@@6B@` | 0x1801549a0 | 2/2 | COMPLETE |
| `??_7PackageFile@IO@@6B@` | 0x180155700 | 5/9 | incomplete |
| `??_7TextFileStream@IO@@6B@` | 0x180155980 | 8/10 | incomplete |

**SIX OF EIGHT ARE ALREADY SLOT-COMPLETE.** "IO VTABLE 8" is not eight tables of
slots to fill; it is six promotion questions and two slot questions. A namespace
gap decomposed once more.

### 3.2 The stamper census, RE-DERIVED on this tip (rule 3)

`tools/mint_stamper_census.py`, all controls green (decoder 3/3, positive
`??0BinaryFileStream@IO@@QEAA@XZ` REGISTERED, negative synthetic not-registered).

| row | stampers | ours-capable | RETAIL | unattributed |
|---|---|---|---|---|
| `??_7CompressedFileStream@IO@@6B@` | 8 | 1 | **7** | 0 |
| `??_7CompressedMemoryStream@IO@@6B@` | 7 | 5 | **2** | 0 |
| `??_7CompressedStream@IO@@6B@` | 9 | 5 | **4** | 0 |
| `??_7FileStream@IO@@6B@` | 17 | 6 | **11** | 0 |
| `??_7PackageFile@IO@@6B@` | 4 | 2 | **2** | 0 |
| `??_7ReaderWriter@IO@@6B@` | 20 | 7 | **10** | 3 |
| `??_7Stream@IO@@6B@` | 50 | 22 | **25** | 3 |
| `??_7TextFileStream@IO@@6B@` | 8 | 2 | **6** | 0 |

**Wave 20's table re-derives TRUE.** For the five rows it printed
(FileStream 17/6/11, CompressedStream 9/5/4, CompressedMemoryStream 7/5/2,
CompressedFileStream 8/1/7, ReaderWriter 20/7/10/3, Stream 50/22/25/3) my numbers
agree **exactly**. A measured section held, again.

It also supplies the **two rows Wave 20's table omitted**: PackageFile 4/2/2 and
TextFileStream 8/2/6.

**VERDICT: ZERO of my 8 are promotable. Every one has retail stampers > 0.** This
is a measurement, not an analogy — I ran the tool rather than reasoning from
BinaryFileStream's precedent.

**Route-scoping (rule 5):** these verdicts are **DLL-PRESENT**. At cutover the
"retail" column is not a stamper we lost to; it is a function that must itself be
ported or the object is never constructed at all. The two states are different
and the table above is the DLL-present one.

**The narrowest gap is `??_7PackageFile@IO@@6B@`: 2 retail stampers, and one of
them is `??1PackageFile@IO@@UEAA@XZ` — my own §2.4 DTOR row.** The other is
`??0PackageFile@IO@@QEAA@AEBV01@@Z`. That is a cluster, not two unrelated rows,
and it is the finding I would most want carried forward. It still needs the 4
missing slots as well.

### 3.3 `??_7ReaderWriter@IO@@6B@` — the numerator, re-derived independently

I did not take Wave 21's word for it. `tools/vtable_dump.py 0x1801553b0 26`
against retail: **25 of 26 slots are `_purecall`; slot 0 is one concrete
function, 0x180023bd0.** That address is `deleting_dtor.cpp` row 42, registered by
address, and `tools/rt_deleting_dtor_verify.py` re-reads the DLL and confirms it
(`OK 0x180023bd0 size=0x20 dtor=0x1800b9150 IO::ReaderWriter`, 0 problems, 43/43).

**Numerator = 1/26. Confirmed independently. The candidate denominator is 1, not 26.**
Disposition: **already discharged by Wave 21 lane ENC; nothing to do but say so**,
plus the instrument fix in §4 so it stops reading as open.

### 3.4 `??_7Stream@IO@@6B@` — A SECOND STALE NUMERATOR, FOUND AND FIXED

Checking ReaderWriter's neighbours turned up the same defect one line below.
The tag read `1/7 ported [INHERITED], 5 purecall [MEASURED]` while
`runtime_census.py` printed `2 2 COMPLETE`.

`tools/vtable_dump.py 0x1801549a0` against retail: 5 purecall, **two** concrete
slots — slot 0 `0x18000c610` (the deleting-dtor thunk, `deleting_dtor.cpp`,
registered by address) and slot 1 `0x18000c6a0` `?GetSource@Stream@IO@@`
(`iobase_vtables.cpp:522`, registered by address). **Both are ours.** The `1` was
stale by one: the `GetSource` port closed the second slot and nobody updated the
tag.

Corrected to `2/7 [RE-DERIVED]` with `[CONV: M=TABLE]`, a `[SCOPE: DLL-PRESENT]`
and a named addressee. The expiry now says what actually blocks it — **stamper
ownership (25 retail), not slots** — because the row is COMPLETE-and-unpromotable,
which is a different state from incomplete and the old tag could not express it.

---

## 4. THE INSTRUMENT FIX — `tools/unpromotable_slot_check.py`

My §3.4 edit was correct and produced a **second** `UNMATCHED` line, proving §0's
mechanism rather than asserting it. A freeze pair stops matching for two opposite
reasons and the tool named only one:

- **DISCHARGED** — the field was edited on purpose and the symbol still carries a
  live tag that DECLARES a convention. The freeze working as designed. Good news.
- **VANISHED** — no live tag declares the symbol. Deleted or silently edited.
  The case actually worth looking at.

Both printed as `UNMATCHED (tag deleted or its field edited)`. Now split, with
the historical text preserved on a `was:` line so nothing is erased.

**Controls, both arms scored separately** (a union score would hide a dead
branch):

```
CONTROL 4pos : 2 freeze pair(s) routed to DISCHARGED, all with a live declared tag -> GREEN
CONTROL 4neg : synthetic never-declared symbol would route to VANISHED -> GREEN
```

4neg is driven by a synthetic name that is not in the file, so the arm is live
even in a run where nothing real has vanished — a screen looking for an absence
has to be handed a shape.

**Checker output after the lane (exit 0):**

```
CONTROL 1pos/1neg/2neg/2pos/3neg/3pos : GREEN
FREEZE       : 20 grandfathered pairs, 18 matched by a live tag
               DISCHARGED (field re-derived; live tag declares a convention): ??_7ReaderWriter@IO@@6B@
                 was: ?/26 ported [NUMERATOR STILL INHERITED], 25 purecall [RE-DERIVED]
               DISCHARGED (field re-derived; live tag declares a convention): ??_7Stream@IO@@6B@
                 was: 1/7 ported [INHERITED], 5 purecall [MEASURED]
CONTROL 4pos : 2 freeze pair(s) routed to DISCHARGED -> GREEN
CONTROL 4neg : synthetic never-declared symbol would route to VANISHED -> GREEN
==== CHECK 1: 20 of 20 scoreable tags fit, 0 STALE ====
==== CHECK 2: 20 numeric tags -- 2 declared, 18 grandfathered, 0 UNDECLARED, 0 malformed ====
==== CHECK 3: 41 tags with an expiry -- 2 scoped, 2 addressed, 39 grandfathered, 0 MISSING ====
```

`tools/runtime_census.py` was **not** edited — lane ALLOC owns it (§1a).

---

## 5. SECTION C — THE FOUR DATA ROWS

Rule 6 says ask who READS the row in the DLL, and rule 6's second half says decide
each on its own xrefs rather than by analogy — which is how Wave 22 lane ERRCFG
refuted the general rule for 3 of its 5 rows.

### 5.1 A wrong instrument, caught by its own control

My first reader scan reported **2** distinct readers for
`?_singleton@FileSystem@IO@@`. That was wrong by 20x, for two reasons I am
recording because the first is a trap anyone repeating this will hit:

1. **`objdump` on this toolchain emits AT&T, not Intel.** My WRITE regex was
   written for Intel (`mov QWORD PTR [rip+..],reg`) and matched nothing, so every
   non-LEA site fell through to the READ branch — and the tool then reported
   "0 writers" for all four rows, a confident wrong answer that looked plausible
   beside a plausible reader list.
2. **No attribution control.** v1 mis-attributed silently: the read at
   `0x1800aebe1` belongs to `?SetupFileSystem@Modification@IO@@` and did not
   appear under it.

v2 asserts that exact `(site, owner, kind)` triple before printing any count:

```
CONTROL(attribution): 0x1800aebe1 -> ?SetupFileSystem@Modification@IO@@QEAAXXZ / READ
                      [expected ?SetupFileSystem@Modification@IO@@QEAAXXZ / READ] -> GREEN
```

In AT&T the rule is positional: rip operand first = READ, last = WRITE.

**Attribution is "nearest preceding export" and is an APPROXIMATION** — an
unexported function between two exports is credited to the earlier export. This
bites visibly once, in §5.5.

The port-status scan carries its own control too (1147 registered symbols found;
two known positives GREEN, one synthetic negative GREEN), because an empty
`registered` set would have reported "nothing is ported" and read as a
catastrophic finding rather than a broken scan.

### 5.2 The measurement

The number that decides a DATA row is **UNPORTED pure readers**: an unported DLL
reader reaches the static as a hard-coded RVA with no door, stays correct by
construction while we are unpromoted, and is **stranded** by promotion.

| row | addr | sites | pure readers | ported | **UNPORTED** | writers |
|---|---|---|---|---|---|---|
| `?_singleton@FileSystem@IO@@` | 0x1801fc6a8 | 91 | 41 | 19 | **22** | 2 |
| `?_singleton@Persistence@IO@@` | 0x1801fc6b8 | 24 | 21 | 17 | **4** | 1 |
| `?_singleton@Modification@IO@@` | 0x1801fc6b0 | 11 | 6 | 4 | **2** | 5 |
| `?_typeInfo@PackageFile@IO@@` | 0x1801f9ac8 | 3 | 2 | 2 | **0** | 0 (+1 LEA) |

Write side cross-checked independently against `tools/rt_mint_writer_audit.py`,
which reaches the same writer sets and classifies all three singletons category
**(b) a PORTED writer** (Modification's `??0DialogLoopT@Engine@@` writer is the
one unported writer across the three).

### 5.3 `?_singleton@FileSystem@IO@@` — the route-scoped row, now with a number

Recorded as "HARMFUL with the DLL present but REQUIRED at cutover". **Re-derived
TRUE, and the vagueness is now quantified: promoting it today strands 22 unported
DLL readers** — `?Get@FileSystem@IO@@`, the whole `PackageFile` read/write
cluster, `?Open@TextFile@IO@@`, `?LoadResources@UpdateLoop@Engine@@`,
`?Serialize@PackageFile@IO@@`, and `?SetupFileSystem@Modification@IO@@` itself
(my §2.3 row — it reads this singleton at `0x1800aebe1`).

At cutover those 22 are either ours or unreachable, and the row flips to REQUIRED.
Two states, one spelling, exactly as the route-scope lesson says. **Verdict: DO
NOT PROMOTE while the DLL is present. Unchanged, and now defensible with a count.**

### 5.4 `?_singleton@Persistence@IO@@` (4 unported readers) and `?_singleton@Modification@IO@@` (2)

Both **NO** on their own xrefs, not by analogy — but both are *close*, and the
blockers are small named functions rather than a wall:

- Persistence: `?Get@Persistence@IO@@SAPEAV12@XZ`, `?PlatformConfigure@Engine@1@`,
  `?ReadAchievements@AchievementManager@Achievement@@`,
  `?SetAchievementMet@AchievementManager@Achievement@@`.
- Modification: `?Get@Modification@IO@@SAPEAV12@XZ`,
  `?LoadResources@UpdateLoop@Engine@@`.

Two of these are already other lanes' Wave 23 subjects: `?PlatformConfigure@Engine@1@`
and `?LoadResources@UpdateLoop@Engine@@` both belong to **lane ENGINE**, and the
two `AchievementManager` readers belong to **lane ACHIEVE**. The three `?Get@X@`
singleton accessors are trivial (a single `MOV RAX,[rip+d32]; RET` shape).

**If lanes ENGINE and ACHIEVE land their rows, Modification drops to 1 unported
reader and Persistence to 2.** That is a real cross-lane dependency and I am
flagging it rather than assuming it.

### 5.5 `?_typeInfo@PackageFile@IO@@` — THE ONE ROW THAT PASSES RULE 6

**Zero unported readers.** Only two functions read it —
`?Type@PackageFile@IO@@UEBAIXZ` and `?TypeId@PackageFile@IO@@SAIXZ` — and I
verified both are registered at `runtime/src/io/package_file.cpp:846` and `:850`.
Promoting it strands nobody.

Write side is handled: it is already a minted row
(`rt_data_mint_tables.inc:2077`, storage `kTypeInfoStorage[65]`), so
`MintTypeInfos()` constructs it — category (a), not the "zero forever" hazard.

The third site is a LEA attributed to `??0Scene@Picking@@QEAA@XZ`, which is
**the nearest-preceding-export approximation of §5.1 showing its seam**: a
`PackageFile` typeinfo is not taken by a picking-scene constructor. The real LEA
is in an unexported CRT-init thunk between exports. I am reporting the artefact
rather than the tidy story.

**I did NOT promote it, and the reason is not the xrefs — it is §6.** Type/TypeId
run on every `External` type check, `MintTypeInfos()` ordering versus first read
is a boot-time question, and I could not get a run to validate it. Promoting a
static on a hot type-check path on static evidence alone is the trade this
project has paid for before.

**ADDRESSEE: Wave 24 lane OBJ** (which owns the Object-namespace DATA rows and
will already be reasoning about `BaseTypeInfo` construction ordering), or any
lane that holds the game-directory lock long enough for one armed boot. The
evidence is complete; only the run is missing.

**Net: ZERO DATA promotions this lane**, on the same measured reasoning that took
Wave 22 lanes TIMER and ERRCFG to zero — but with one row (§5.5) carried to
*ready-pending-a-run* rather than refused.

---

## 6. VERIFICATION — AND THE RUN I DID NOT GET

Per §1b, plainly: **I did not get a game-directory run.**

I checked the harness before saying so (§1b forbids "unverifiable" without it).
A route exists and is named: `BANISHED_RT_DIFF=1` arms both new shadow oracles,
and `RtIo_TextFileStream_Ctor` — already ported and registered — calls
`SetObject` on every TextFileStream construction, with
`RtIoProbeRunTextFileStreamProbe()` reachable from `runtime/src/object/external_add.cpp:291`.
So these two rows are **verifiable, not unverifiable**; the instrument is written
and wired, only the run is missing.

The lock was acquired **bare, never piped** (a pipeline reports the last command's
status, so a timeout reads as a grant):

```
$ bash tools/game_dir_lock.sh acquire IOPORT 240
[gamelock] queued behind 'VIDVT' (0s/240s)... ... (210s/240s)...
[gamelock] DENIED -- TIMEOUT after 240s; queued behind 'VIDVT'.
ACQUIRE_EXIT=1
```

Held first by lane ENGINE, then by lane VIDVT. Ten lanes, one directory. I report
`calls=0` for both new oracles because **they have not been run at all**, which is
a different statement from "ran and measured nothing".

**What WAS verified without a run:**

- `mingw32-make standalone-debug`: **clean** (two compile errors found and fixed
  during the lane — a `RT_ORIGINAL_ONCE` typedef name and a missing `RT_DIFF_BEGIN`
  scope for the oracle's shadow site).
- `tools/rt_deleting_dtor_verify.py`: 43/43 rows OK, **0 problems** (re-reads the
  DLL; independent of the source's own count).
- `tools/unpromotable_slot_check.py`: **exit 0**, all controls green (§4).
- `tools/mint_stamper_census.py`: all controls green, agrees with Wave 20 exactly.
- Every address in this report read from retail with `pefile` / `objdump` /
  Ghidra MCP against `Runtime-x64.dll`, never carried from a doc.

---

## 7. DENOMINATORS

- "IO 216 called / 200 -> 202 ported", "gap 16 -> 14": `tools/runtime_census.py`
  on **this branch only**. Branch-local (rule 13).
- "ours/slots" per `??_7` row: the census's mint-vtable backlog fold,
  **M=NONPURECALL** (purecall slots excluded from the denominator by the tool).
  The `RT_UNPROMOTABLE` tags use **M=TABLE** (purecall included) — that is why
  `??_7Stream@` reads `2 2` in one place and `2/7` in the other. Both are declared.
- "stampers / ours-capable / retail": `tools/mint_stamper_census.py`. `ours-capable`
  is an **UPPER bound** — a registration is a door, not proof every caller uses it.
- "pure readers": distinct nearest-preceding-export owners of a rip-relative READ
  that never also WRITE the row, from a full `objdump -d` of retail .text.
  Attribution is an approximation (§5.1).
- "ported": literal `RegisterRuntimeExport("<mangled>")` scan of
  `runtime/ standalone/ engine/ code/ dx11/` **with comments stripped**.
- Extents: `.pdata` is a **LOWER** bound, adjacent-export a **UPPER** bound. Both
  quoted for §2.3 and §2.4; neither presented as exact.

---

## 8. WHAT I DID NOT CHECK

1. **No live run of anything** (§6). Both new oracles are `calls=0` because they
   were never executed. No attendance figure in this report.
2. **`??_7PackageFile@IO@@6B@`'s 4 missing slots** were not identified
   individually. I measured 5/9 and the stamper split; I did not enumerate which
   four slots are unported or size them.
3. **`??_7TextFileStream@IO@@6B@`'s 2 missing slots** — re-confirmed as Wave 21
   lane ENC left them (slots 2 `Write`, 462 bytes; 8 `Open`, 666 bytes; NOT
   ATTEMPTED, a size judgement). I did not re-read them.
4. **Case (d) of the SetObject oracle** (release-with-free) — static reading only,
   see §2.2.
5. **The `_data` pointer** in the Create oracle — deliberately uncompared, §2.1.
6. **Whether `MintTypeInfos()` runs before the first `?Type@PackageFile@IO@@`
   read** — the one fact §5.5 still needs, and it needs a boot.
7. **The exe side.** Every reader/writer count here is Runtime-x64.dll only. I did
   not sweep `Application-x64.exe` for readers of the four DATA rows, so the
   "22 unported readers" figure for FileSystem is a **lower** bound on the true
   strand count.
8. **The 3 unattributed stampers** on `ReaderWriter` and `Stream` — the census
   prints them; I did not chase what they are.
9. **Whether other Wave 23 lanes are editing `reg_mint_promoted.cpp` or
   `unpromotable_slot_check.py`** concurrently. Both are shared, append-mostly
   files; a conflict there is a rebase, not a logic bug, but I did not coordinate.
10. **`?SetupFileSystem@`'s 3 unported callees** were confirmed unregistered but
    not sized or scoped individually.

---

## 9. RESIDUALS, EACH WITH A NAMED ADDRESSEE (rule 11)

| # | item | addressee |
|---|---|---|
| R1 | `?SetupFileSystem@Modification@IO@@` — scoped (§2.3), blocked on the mods-route crash, not on callees | **the Wave 24 lane that fixes `ModDialog_ToggleMod_Impl+0x43`**; schedule this row after it |
| R2 | `??1PackageFile@` + `??0PackageFile@...@AEBV01@@Z` — the two retail stampers of `??_7PackageFile@IO@@6B@` (§2.4, §3.2) | **Wave 24 lane OBJ**, or whoever next takes `??_7PackageFile@IO@@6B@` |
| R3 | `?_typeInfo@PackageFile@IO@@` — zero unported readers, mint-constructed, **ready pending one armed boot** (§5.5) | **Wave 24 lane OBJ**, or any lane holding the game-dir lock |
| R4 | Run `BANISHED_RT_DIFF=1` against the two new oracles (§6) | **the Wave 23 integrator**, on the merged tip where one run covers all lanes |
| R5 | `?Get@FileSystem@IO@@` / `?Get@Modification@IO@@` / `?Get@Persistence@IO@@` — three trivial singleton accessors, each removing one unported reader (§5.4) | **Wave 24 lane IOPORT** if one is cut; otherwise the integrator may fold them in |
| R6 | Cross-lane: lane ENGINE's `?PlatformConfigure@` + `?LoadResources@UpdateLoop@` and lane ACHIEVE's two readers each shrink §5.4's counts | **the Wave 23 integrator** — re-measure §5.2 on the merged tip before anyone re-refuses those two rows |

**Merged nothing.**
