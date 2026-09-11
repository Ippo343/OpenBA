# WAVE 21 LANE SHOT — what "close the screenshot hole" MEANS, and the transitive door set

Branch `wave21/shot`, cut from `wave21/base` at `018426f2`. Items 5 and 6 of
WAVE20_CLOSED §8. **This lane's deliverable is a decision, not a port. No port was
written.**

Evidence, all regenerable: `docs/runtime_port/wave21_shot/`.

---

## HEADLINE — three corrections, in decreasing order of consequence

1. **The price of the last two vtable rows is 14 bodies, not 39.** "11 and 28 stamper
   bodies" both re-derive exactly (§1.3) — but `??_7Stream@IO@@6B@`, the row that costs
   28, **already carries a written `RT_UNPROMOTABLE … | NONE, EVER` NO-OP verdict**
   (`standalone/reg_mint_promoted.cpp:1203`) and this tree **deliberately declines to
   promote it** (`reg_mint_promoted.cpp:923-930`, in as many words). Porting its 28
   stampers would not change that verdict. §8 item 5 called both rows "port work"; one of
   them is a row we have already decided not to move.
2. **29 of 29 is unreachable for a DIFFERENT reason than Wave 20 gave, and the number of
   permanently-open rows is TWO, not one.** The `NONE EVER / HARMFUL` tag on
   `?_singleton@FileSystem@IO@@` is **CONFIRMED from the binary** (§2) — and its expiry is
   **wrong as written**. It is harmful *while the DLL is present* and **required at
   cutover**, which is the `cutoverOnlyAddr` shape the allocator pair already uses. It is
   not permanently out of reach; it is out of reach *on one route*.
   `??_7Stream@IO@@6B@` is the row that is genuinely permanently open.
3. **"29 of 29 SHUT" is not a coherent goal at all**, because the board is scored on ONE
   route and three of its rows want opposite answers on the two routes. The definition in
   §4 is per-route.

---

## 1. THE RE-DERIVED NUMBERS, each with its denominator

### 1.1 The walker, its subject and its denominator

```
python tools/screenshot_door_walk.py                 -> exit 0
python tools/screenshot_door_walk.py --transitive    -> exit 1
```

* **The 29 is `kDoorCount`** — the number of ROWS in `const Door kDoors[]` in
  `runtime/src/io/file_stream_cutover_probe.cpp`. Re-derived today: **29 rows**. Not the
  same as the number of *names*: **4 rows are address-only** (`name == nullptr`), so the
  board parses to **25 mangled names and 29 rows**. The old tool printed only the name
  count (25) and the old fixed-tool row count was off by one (it counted the array's own
  opening brace); both are fixed and both numbers now print side by side.
* **The population is "doors on `?SaveImageToFile@ImageBuffer@Video@@`'s path"**, and it
  is a HAND-BUILT population for 18 of its 29 rows. It still has 29 members; it is not
  the right 29 (§3).
* The direct half re-derives green and unchanged: **11 of 11 derived direct doors are on
  the board, 0 missing**, 0 non-literal door macros in the subject file.

### 1.2 The open set, re-derived statically (I did NOT re-run the live board)

`docs/runtime_port/wave21_shot/board_static.txt` — a literal
`RegisterRuntimeExport`/`RegisterRuntimeAddr` scan of `runtime/ engine/ code/ standalone/
dx11/` (1521 files / 26,113,438 bytes; positive and negative controls green), joined onto
the 29 rows:

```
==== STATIC: 25 of 29 rows shut, 4 OPEN ====
  OPEN  ?_singleton@FileSystem@IO@@0PEAV12@EA   0x1801fc6a8
  OPEN  ??_7Stream@IO@@6B@                      0x1801549a0
  OPEN  ??_7FileStream@IO@@6B@                  0x180154960
  OPEN  0x1801554a8 (unexported encoding vtable)
```

Wave 20's LIVE board read **23 of 29 shut, 6 OPEN**. The two-row difference is **exactly
the allocator pair** (`?MemAlloc@`/`?MemFree@`), whose registration this static scan can
see in the text but whose `if (!RuntimeDllPresent())` guard it cannot. **So the static
screen is an UPPER bound on shut rows — the optimistic, unsafe direction — and I say so
rather than quoting 25/29 as an improvement.** The two instruments agree exactly once
that one known guard is accounted for, which is the useful result: **4 rows are open on
every route; 2 more are open only on the DLL-present route, by construction.**

### 1.3 The "11 and 28 stamper bodies" — counted myself

`tools/mint_stamper_census.py --rows '??_7Stream@IO@@6B@,??_7FileStream@IO@@6B@,??_7BinaryFileStream@IO@@6B@'`
(decoder control 3/3, ownership controls both green; full output in
`wave21_shot/stamper_census.txt`):

| row | stampers | ours-capable | retail | unattributed |
|---|---|---|---|---|
| `??_7BinaryFileStream@IO@@6B@` | 2 | 2 | 0 | 0 |
| `??_7FileStream@IO@@6B@` | 17 | 6 | **11** | 0 |
| `??_7Stream@IO@@6B@` | 50 | 22 | **25** | **3** |

* **11 re-derives EXACTLY.**
* **28 re-derives as 25 + 3**, and the 3 are UNATTRIBUTED by this channel — stamp sites
  outside every `.pdata` range. **Ghidra names all three** (§1.4), so the price is 28
  nameable bodies. An earlier draft of this section said *"you cannot port a function you
  cannot name"*; that is **RETRACTED** — the silence was a limit of `.pdata` attribution,
  not a fact about the binary. This is the `reference_grep_cannot_tell_internal_from_unported_export`
  shape, caught by a second channel rather than by review.
* `ours-capable` is an **upper bound** (a registration is a door, not a guarantee every
  caller uses it), so 11 and 25 are **lower bounds** on the remaining bodies.

### 1.4 A SECOND, INDEPENDENT CHANNEL — Ghidra's xref graph

Everything in §1.3 and §2 above comes from one channel: `objdump` text plus `pefile`
`.pdata`/export attribution. Re-asked of Ghidra
(`wave21_shot/ghidra_corroboration.txt`; `Runtime-x64.dll` verified from each
`switch_program` result, because that instance is shared by seven lanes):

| subject | objdump + pefile | Ghidra xrefs | |
|---|---|---|---|
| `0x1801fc6a8` refs | 91 = 89 R + 2 W | 89 `[READ]` + 2 `[WRITE]` | **agrees exactly** |
| `??_7FileStream@` stampers | 17 | 17 distinct functions | **agrees exactly** |
| `??_7Stream@` stampers | 50 (3 unattributed) | 50 distinct functions, **all 3 named** | **agrees exactly** |

The three `.pdata`-less stampers are `??0Stream@IO@@QEAA@XZ` (`0x1800b99d0`),
`??1Stream@IO@@UEAA@XZ` (`0x1800b9a40`) and `??0Stream@IO@@QEAA@AEBV01@@Z`
(`0x18000c6d0`) — frameless leaf functions, which is exactly the class
`mint_extent_audit.py`'s own docstring warns it cannot attribute.

**What Ghidra does NOT corroborate:** the `ours-capable` / `retail` split. That comes from
our source-side registration scan and Ghidra has no view of our registrations, so 6/11 and
22/25 rest on one channel only.

**One reference shape NEITHER channel counts as a door**, surfaced by Ghidra and recorded
rather than acted on: each of the three addresses also carries a `[DATA]` reference from
`.rdata` (`0x1801b5570`, `0x1801b20b0`, `0x1801b21cc`) — a data-held pointer to the slot,
not an instruction. A door inventory bounded by MECHANISM
(`reference_count_the_doors_not_the_calls.md`) should account for it; I did not.

**A Ghidra silence is not a negative** — and no conclusion here rests on one. Every query
returned a non-empty list, and the one place an empty answer would have mattered (the
three unattributed stampers) is the place Ghidra spoke.

---

## 2. `?_singleton@FileSystem@IO@@` — the tag CONFIRMED from the binary, its EXPIRY REFUTED

Checked from `Runtime-x64.dll` and from the call sites, not from the doc that asserts it.
`wave21_shot/filesystem_singleton_refs.txt`; channel is an `objdump` scan of the whole
image joined onto `.pdata`-bounded function attribution and the registration scan.

```
refs 91 = 89 reads + 2 writes          (WAVE17_LANE_CEIL.md §3.2 says "85 / 2")
WRITE sites: 2 functions, 2 of 2 ours-capable
             ??0FileSystem@IO@@QEAA@XZ           0x1800ac818
             ?Startup@Engine@1@QEAAIPEBDII_N@Z   0x1800dba3e
READ  sites: 44 distinct functions, 18 of 44 ours-capable -> 26 STILL RETAIL
```

* The **write count re-derives exactly (2)**. The read count is **89, not 85** — and
  **two independent channels now say 89** (§1.4), so CEIL's figure is 4 low rather than
  mine being 4 high. I did not reconcile CEIL's method. The direction of the verdict does
  not turn on it. Ghidra's function names for the readers match my `.pdata` attribution
  name for name, including the three `FUN_1800b6…` privates.
* **HARMFUL is CONFIRMED, and on the stated mechanism.** Promote our minted storage today
  and 18 ported reader bodies read OUR slot while **26 reader functions still living in
  the DLL** read the DLL's. That is the split brain the tag names, measured rather than
  assumed. Among the 26 are `?SetupFileSystem@Modification@IO@@`, the whole `PackageFile`
  cluster, `TextFile::Open/Create/Close` and three unexported `FUN_1800b6…`.
* **The expiry `NONE, EVER` is WRONG.** Decomposed (per
  `reference_a_check_is_unaimable_only_without_a_free_parameter.md`): the claim
  *"promoting can never help"* rests on the premise *"there will always be in-DLL readers
  of this slot"*. **At cutover there is no DLL and therefore no in-DLL reader.** The two
  writers are both ours; our own readers reach it through `RT_DATA_SLOT`, which on a miss
  returns NULL with a named refusal (`rt_resolve_cache.cpp:347-357`) — so at cutover an
  unpromoted slot is a null `FileSystem*` handed to eleven of our own bodies.
  `runtime/src/io/filesystem.cpp:339-350` states the same expiry correctly and in prose —
  *"required until FileSystem's OWNER … is ported"* — which is a condition, not "ever".
* **The mechanism to satisfy it already exists and is already used**: the allocator pair
  registers its addresses only from `allocator.cpp`'s `if (!RuntimeDllPresent())` arm
  (`allocator.cpp:1343-1372`), and the board carries a `cutoverOnlyAddr` flag for exactly
  this. `?_singleton@FileSystem@IO@@` is the same shape and needs the same treatment: a
  cutover-only registration, ~10 lines, **a decision and not a port**.

**So the answer to item 5's "if it is genuinely harmful to mint, then 29/29 is not a goal
and saying so is the result" is: it IS genuinely harmful to mint TODAY, and that is a
route-scoped fact, not a permanent one.** It is the wrong row to hang the impossibility
claim on.

---

## 3. Item 6 — the transitive half, now derived

`tools/screenshot_door_walk.py --transitive` (this lane wrote it). It indexes every
function definition in `runtime/ engine/ code/ standalone/ dx11/` by brace matching over
comment-stripped text, maps every mangled symbol the tree registers to its implementing
function, and walks our own call graph from the root symbol, following **both** door
literals **and** ordinary direct calls.

```
POPULATION : 1521 files / 26113438 bytes ; 10577 function definitions indexed ;
             2196 mangled symbols mapped to an impl
ROOT IMPL  : Video_ImageBuffer_SaveImageToFile
REACHED    : 156 of our own function bodies, 74 entered through an AMBIGUOUS name hop
==== transitive: 31 name doors derived, 20 on the board, 11 NOT ON BOARD
     (4 of those on an UNAMBIGUOUS path); board has 29 rows / 25 names,
     5 of which the walk does not reach ====
ADDRESS COLUMN: 6 of 17 derived address doors are on the board's address column
```

### 3.1 The positive control, in both directions

A screen whose output is read as "here is the set" is an absence screen the moment anyone
reads a name as missing, and then a dead walker prints the same thing as a clean one
(`reference_absence_screen_must_be_handed_a_shape.md`). So the walk is re-run twice with a
**synthetic door literal spliced into a named function's text** and must score both ways:

```
CONTROL t-pos : injected into RtIo_FileSystem_CreatePath          -> FOUND      GREEN
CONTROL t-neg : injected into RtVideo_Device_EnumerateVideoModes  -> not found  GREEN
```

`RtIo_FileSystem_CreatePath` is at depth ≥ 2 (it is reached through the `?CreatePath@`
door, not defined in the root file), so the positive control exercises the transitive
machinery and not just the direct scan. If either control fails the tool prints
`TRANSITIVE MEASURED NOTHING` and withholds the set; it does not print a verdict.

### 3.2 The SET DIFF, not the count

**11 name doors the hand-built board does not list.** Wave 20's "four short" was the
DIRECT half; the transitive half is **eleven** short. Registration status checked with the
same scan:

| door | reached via | on the board? | registered? |
|---|---|---|---|
| `??0Filename@System@@QEAA@AEBV01@@Z` | `RtIo_FileInfo_CopyCtor` | no | shut |
| `??0Filename@System@@QEAA@AEBVString@1@@Z` | `MakePath` | no | shut |
| `?GetSaveDirectory@FileSystem@IO@@QEBA?AVFilename@System@@XZ` | `MakePath` | no | shut |
| `?Set@String@System@@QEAAXPEBD@Z` | `RtIo_FileSystem_OpenFile` | no | shut |
| `?StrAppend@String@System@@SAPEADPEADHPEBD@Z` | `RtSystem_String_StrAppend` † | no | shut |
| `?SetText@Error@System@@SAXPEBD_N@Z` | `FatalOutOfMemory` † | no | shut |
| `?Display@Error@System@@SAXXZ` | `FatalOutOfMemory` † | no | **OPEN** |
| `?_errorString@Error@System@@0PADA` | `ErrorStringBuffer` † | no | **OPEN** |
| `?_stopApplication@Error@System@@0_NA` | `StopApplicationSlot` † | no | **OPEN** |
| `?_singleton@ExternalFactory@Object@@0PEAV12@EA` | `SingletonSlot` † | no | **OPEN** |
| `?_singleton@MemoryAllocator@System@@0PEAV12@EA` | `SingletonSlot` † | no | **OPEN** |

† reached through an ambiguous-name hop — see §3.4. All five OPEN rows carry the dagger,
so **the five that would change the board's verdict are exactly the five I am least sure
belong on it.** That is stated, not smoothed: the honest reading is *up to five*, and a
lane that wants them on the board should resolve the ambiguity first. Four of the five are
`?_singleton@`/static-storage rows already tagged `UNPROMOTABLE … HARMFUL` elsewhere, so
adding them would add *permanently open* rows.

**And 5 board names the walk does NOT derive as doors** — every one because our source
reaches that symbol by an **ordinary C++ call**, not through a door:

```
??1FileStream@IO@@UEAA@XZ            <- RtIo_BinaryFileStream_Dtor calls RtIo_FileStream_Dtor directly
??4Filename@System@@QEAAAEAV01@AEBV01@@Z
?CreateFileW@FileSystem@IO@@…
?PlatformCreateFile@FileSystem@IO@@…
?PlatformCloseFile@FileSystem@IO@@…
```

**At cutover nothing has to resolve those**, so calling them "doors on the path" flatters
the board's denominator by 5. They are still worth listing as *path members*; they are not
doors. The board merges the two kinds and that is a defect of the board, not of the walk.

### 3.3 Which doors the walker still CANNOT see — count the doors, not the calls

Enumerated by mechanism, per `reference_count_the_doors_not_the_calls.md`, and printed by
the tool on every run rather than asserted here:

| door | read by the walk? | why |
|---|---|---|
| `RT_EXPORT_ONCE`, `RT_DATA_SLOT`, `RT_ORIGINAL_ONCE`, `RT_VTABLE_ONCE` | yes, literal names only | |
| `ResolveExport` / `ResolveExportRaw` | yes, literal names only | |
| `RtOriginal<T>("…")` | yes, literal names only | |
| `ResolveAddr(Module::Runtime, 0x…)` / `ResolveRuntimeRaw(0x…)` | yes, **as ADDRESSES** | not a mangled name — reported in its own bucket, **never merged into the name set**; 17 found, 6 of them on the board's address column |
| `ResolveOrdinal` / `ResolveOrdinalRaw` | counted, 0 found | an ordinal has no name and can never be set-diffed against a name-keyed board |
| any of the above spelled through a **named constant** | **NO** — 4 such sites counted and printed | `reference_literal_grep_blind_to_variables.md` |
| a door in a body we have **not ported** | **NO** | there is no source to scan — the result is a LOWER bound |

**The 11 address doors not on the board's address column are a real second gap**, and 8 of
them come from one place: `RunFileStreamSlotsProbe`, a diagnostic invoked unconditionally
from `RtIo_FileSystem_CloseFile` after 2000 closes
(`filesystem_createfile_win.cpp:540-548`). I traced that reach rather than assuming it was
a name collision. **The walker has no notion of a guard** — an env-gated or
diagnostic-only door counts like a production one — and that is now in its docstring.

### 3.4 The over-reach the tool declares about itself

The function index is keyed on the bare identifier, so a name with several definitions is
followed into all of them. Dropping such hops would convert an over-reach into an
under-reach, which is worse for a screen read as "the door set", so the walk follows them
and **taints** the node instead; the taint propagates and every door found only in a
tainted node prints `[AMBIGUOUS HOP]`. 74 of 156 reached bodies are tainted. The verdict
line reports both `11 NOT ON BOARD` and `4 of those on an UNAMBIGUOUS path`.

---

## 4. THE DEFINITION — what counts as "the screenshot hole is closed"

The board conflates two kinds of row and two routes, and that is the whole reason "29 of
29" reads as a goal.

* A **CODE** row shuts when a ported body is registered. Same answer on both routes.
* A **DATA** row (a vtable, a singleton) shuts when our minted storage is **promoted over
  the DLL's**. On the DLL-present route promotion can be *harmful*; on the cutover route
  the same promotion can be *required*. **Three rows want opposite answers on the two
  routes** — the allocator pair and `?_singleton@FileSystem@IO@@`.

> ### CLOSED means: a **DLL-ABSENT** run of `BANISHED_RT_FILE_CUTOVER=1` reports every row
> SHUT except `??_7Stream@IO@@6B@`, with non-zero attendance on the four lane-FILE
> counter blocks — and on the **DLL-PRESENT** route the same board reports every row SHUT
> except three named cutover-only rows (`?MemAlloc@`, `?MemFree@`,
> `?_singleton@FileSystem@IO@@`) and `??_7Stream@IO@@6B@`.

Two routes, two targets, one board. The board already has the vocabulary for the
route-scoped rows (`cutoverOnlyAddr`); it needs one more flag for the permanently-open
one.

### 4.1 What is permanently out of reach, and why — ONE row

**`??_7Stream@IO@@6B@`.** It carries `RT_UNPROMOTABLE … | NONE, EVER` with class NO-OP
(`reg_mint_promoted.cpp:1203`, from `WAVE19_PHASE0A_NOOP.md §3.2`): the table is abstract
(5 of 7 slots purecall), every reference to its VA in the whole DLL is an in-DLL
RIP-relative LEA, no shipped image imports it, and **nothing dispatches while it is
installed** — 58 stamp windows containing zero calls through the stamped object. The tree
already acts on this: `reg_mint_promoted.cpp:923-930` records the row as **READY and still
UNPROMOTABLE(NO-OP), deliberately not promoted.** A null vptr there at cutover is
provably not dispatched, so the row is safe to leave open and pointless to close.

**Its expiry, written here because the tag's own is `NONE, EVER` and this lane is its
addressee if that turns out wrong:** the verdict lapses the moment any lane observes a
dispatch through a `??_7Stream@`-stamped object **in one of OUR bodies at cutover** —
Phase 0a measured retail's windows, and the claim that ours mirror them is inherited from
`WAVE19_PHASE0A_NOOP.md`, not re-measured by me.

**`?_singleton@FileSystem@IO@@` is NOT in this category**, and Wave 20 put it there. It is
route-scoped: harmful today, required at cutover, satisfiable by a cutover-only
registration.

### 4.2 The reachable target, stated as an IDENTITY and not a percentage

A percentage here has the board's row count in both halves, and §3.2 shows the row count
is about to move by up to +11. Quoting "X%" or even "28 of 29" would be the
`reference_the_ceiling_percentage_is_a_moving_target.md` trap with a different
denominator. So:

> **TARGET: the open set is `{ ??_7Stream@IO@@6B@ }` on the cutover route, and
> `{ ??_7Stream@IO@@6B@, ?MemAlloc@, ?MemFree@, ?_singleton@FileSystem@IO@@ }` on the
> DLL-present route. Score the lane on that SET.**

The work between here and there, priced:

| # | item | price | kind |
|---|---|---|---|
| 1 | `0x1801554a8` encoding vtable | **3 slot bodies** (`0x1800a81e0/81d0/81c0`) | port — **owned by lane ENC this wave** |
| 2 | `??_7FileStream@IO@@6B@` | **11 stamper bodies** across Audio / TextFile / PackageFile | port |
| 3 | `?_singleton@FileSystem@IO@@` | **a cutover-only registration, ~10 lines** | decision, not a port |
| 4 | the allocator pair | **nothing** — already correct; they need a DLL-ABSENT RUN to be scored at all | harness |
| 5 | `??_7Stream@IO@@6B@` | **nothing — deliberately left open** | decision |

**14 bodies, one ten-line registration and one harness run.** Wave 20 §8 item 5's "11 and
28 … which is port work" priced it at 39 bodies and named the wrong row as the
impossibility.

### 4.3 Should this hole be closed at all?

**Yes, but item 1 and item 4 are the whole of the cheap half, and item 4 is the one that
would change what anybody knows.** Nothing on this board has ever been scored on the route
it exists to test: **no DLL-absent run of this screen has been made.** Six of the 29 rows
(the four address-only ones and the allocator pair) are scored today only by a screen that
cannot see the arm that registers them. **A lane that runs the board DLL-absent learns more
than a lane that ports item 2's eleven bodies**, and it is a harness change, not port work.

---

## 5. WHAT I DID NOT CHECK

1. **I did not run the game.** No live board, no attendance, no `BANISHED_RT_FILE_CUTOVER=1`
   run, no DLL-absent run. §1.2's open set is a STATIC re-derivation and is an upper bound
   on shut rows. The 23/29 live figure is quoted from Wave 20 and was not re-measured.
2. **I did not build.** `mingw32-make standalone-debug` was not run; the only source change
   in this branch is `tools/screenshot_door_walk.py`, which is not compiled.
3. **I did not reconcile 89 reads against CEIL's 85.** Two channels say 89 and the write
   count (2) matches exactly, so I left the 4-ref difference unexplained rather than
   guessing at CEIL's channel.
3a. **I did not account for the `[DATA]` references** Ghidra reports from `.rdata` to all
   three subject addresses (§1.4). A data-held pointer is a reference shape no instrument
   in this lane counts.
3b. **Ghidra does not corroborate the `ours-capable` / `retail` split** (6/11, 22/25).
   That rests on the source-side registration scan alone.
4. **I did not verify that our own stamp windows mirror retail's** for `??_7Stream@`. §4.1's
   permanence rests on `WAVE19_PHASE0A_NOOP.md §3.2`, inherited.
5. **I did not resolve the ambiguity on the 7 tainted transitive doors**, including all 5
   of the OPEN ones. Up to five new open rows, or as few as zero.
6. **I did not measure how many class-body method definitions the function index skips.**
   The indexer only opens a function at a non-function nesting level; the skip count is
   unknown, and it is an UNDER-reach.
7. **I did not check the 4 non-literal door macros** the transitive scan counted and could
   not read. They are counted, not named.
8. **I did not enumerate the stampers of the encoding vtable at `0x1801554a8`.** Item 1's
   "3 slot bodies" is inherited from `WAVE20_LANE_DOORS.md §4`/`WAVE9_LANE_ENC.md`; lane
   ENC owns it this wave and the census backlog row (ours 2 / slots 5) is the check.
9. **I did not verify that the 11 `??_7FileStream@` retail stampers are all genuinely
   needed at cutover.** Some may be on paths our build never takes; `ours-capable` being an
   upper bound makes 11 a lower bound on the bodies, not a bound on the *useful* bodies.
10. **I did not walk the DLL's OWN call graph from `?SaveImageToFile@` in Ghidra.** That
    is a different and complementary question — *what does RETAIL call on this path*,
    including in-DLL `rel32` edges our source can never show — and it would bound the door
    set from the other side. My walk is over OUR source only and is a LOWER bound. This is
    the single largest thing a follow-up could add, and it is a day's work, not a
    re-derivation: the closure has to be cut off somewhere and where to cut it is a free
    parameter with no obvious value.
11. **I did not grep the `BANISHED_*` list for a driver for the DLL-absent route** beyond
    reading `allocator.cpp`'s arming path and `RtSystemAllocatorSetCutoverArmer`. §4.3's
    "harness change" is an estimate, not a measured scope.
