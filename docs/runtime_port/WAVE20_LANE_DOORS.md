# WAVE 20 LANE DOORS — the screenshot board reports again, and the remainder is not what the guide says

**Worktree** `C:/code/GIT/wave20/doors` · **branch** `wave20/doors`, cut from
`f874bf7` (lane PROMOTE merged). Every number below was measured **in this
worktree**, on the exe whose md5 the run section names. Evidence files are in
`docs/runtime_port/wave20_doors/`.

---

## 0. Why the board was dark, and what was actually broken

`runtime/src/io/file_stream_cutover_probe.cpp` guarded its verdict with a
hard-coded literal:

```c
const char* const kCtlMustBeUnported = "?Commit@CompressedStream@IO@@QEAAXXZ";
```

which **MUST read NOT-PORTED**. A lane ported it. The control read PORTED, the
screen printed `MEASURED NOTHING` and offered no verdict, and the last quoted
figure (**18 of 25 SHUT**) went on being cited from a document while nothing in
the tree could re-derive it.

**The screen was not broken.** It did exactly what a screen should do when its
control goes bad. What was broken is that the control's SUBJECT was a **free
parameter that decays as lanes do their job** — every symbol that qualifies as
"looks ported and is not" is a symbol some lane is trying to disqualify. Lane
PROMOTE hit this and deliberately left it alone; that call was right, and this
lane owns the re-choice.

---

## 1. The fix: derive the subject, and demonstrate it can still fail

WAVE19_CLOSED §7.1 — *remove the free parameter; failing that, print it beside
the verdict.* Both, in that order.

**REMOVED.** `RtEnumRuntimeExports()` (new, `standalone/resolve_standalone.cpp`)
walks Runtime-x64.dll's **own export directory**, consulting no registry and no
resolver. The negative control's subject is now **selected at run time**: the
first named export the port registry does not answer. There is no symbol in the
source, so there is nothing to go stale.

**PRINTED.** The name it happened to pick is printed beside the verdict with both
denominators, so the aim is visible even though nothing aims it.

**BOTH CONTROLS COME FROM THE ONE WALK**, with a real denominator each — the old
pair were one sample each:

```
CONTROL walk: 4926 named exports in Runtime-x64.dll, 4926 with an address
CONTROL positive (registry MUST be able to say YES): 1113 of 4926 answered   -> GREEN
CONTROL negative (registry MUST be able to say NO):  3813 of 4926 unanswered -> GREEN
CONTROL negative SUBJECT, derived not named: ??0?$InterpolatedState@M@Graphics@@QEAA@M@Z (0x1800153d0)
```

### 1.1 IT CAN STILL FAIL — the demonstration, red then green

A derived control's failure mode is not "the picked symbol turned out to be
ported" — it is **`candidates == 0`**, the registry answering YES to all 4,926
exports. That is the *same* hazard the literal guarded, reached from the other
side, and it is reachable: `BANISHED_RT_FILE_CUTOVER_CTLBREAK` makes this file's
copy of the lookup lie (it lies to the screen only; it hands out a non-function
sentinel and cannot be called).

| run | negative | positive | verdict | evidence |
|---|---|---|---|---|
| `CTLBREAK=yes` (always answers PORTED) | **0 of 4926 → RED** | 4926 of 4926 GREEN | `MEASURED NOTHING: the registry answered YES to every export, so every shut below would be a lie` | `wave20_doors/ctlbreak_yes.txt` |
| `CTLBREAK=no` (never answers PORTED) | 4926 of 4926 GREEN | **0 of 4926 → RED** | `MEASURED NOTHING: the registry answered NO to every export, so every OPEN below would be a false alarm` | `wave20_doors/ctlbreak_no.txt` |
| unset | 3813 GREEN | 1113 GREEN | board reported | `wave20_doors/ctlbreak_unset_menu.txt` |

Each control goes red **in its own direction**, and the break line prints above
the verdict saying the run is deliberately lying.

**The old named positive control (`?OpenFile@FileSystem@IO@@`) is kept but
DEMOTED to informative — it gates nothing.** It is a literal, and a literal in a
gate is what darkened this screen.

---

## 2. THE CALL-GRAPH WALK, done before estimating anything — four doors nobody listed

The board was 25 hand-built rows. `Video_ImageBuffer_SaveImageToFile` has **ten**
`RT_EXPORT_ONCE` sites and one `RT_DATA_SLOT`; **six were on the board and four
were not** — all four unconditional, in the function's first statement block:

```
?GetPathFilename@Filename@System@@QEBA?AV12@_N@Z    0x1800c6740   save_image_to_file.cpp:227
?GetPathDirectory@Filename@System@@QEBA?AV12@XZ     0x1800c6880   :229
??HSystem@@YA?AVString@0@AEBV10@0@Z                 0x1800c9440   :231
??HSystem@@YA?AVString@0@AEBV10@PEBD@Z              0x1800c92d0   :233
```

This is the **fourth** time this debt has come out deeper than stated (plan: four
exports → C2: six → Wave 19's disassembly walk: ten → this: fourteen direct). All
four are ported by other lanes, so the board grows **25 → 29 rows** and the open
set does not move. That is why it survived; it is not a reason to have left it
out.

**The transitive closure was re-walked and found NOTHING new.** The `%TEMP%`
mechanism the brief flagged — a write goes to `GetTempFileNameW`'s file and is
`MoveFileW`'d into place on close — is implemented in
`filesystem_createfile_win.cpp` with **direct Win32 calls**, not Runtime doors;
its only Runtime doors (`?SubString@`, `?Set@WideString@`, `?AsUTF8@`,
`?AdjustDelimiters@`) were already rows.

### 2.1 So the DIRECT half is no longer hand-built

`tools/screenshot_door_walk.py` re-derives the direct doors from the path source
and set-diffs them against the array. It states its blind spots **before** its
findings (transitive half not derived; literals only, with the non-literal count
printed; `PATH_SOURCES` is a free parameter it prints beside the verdict).

**Effectiveness control:** pointed at HEAD's board it reports **4 MISSING, exit
1**; on this tree **11 of 11 present, 0 missing, exit 0**.

### 2.2 Address provenance, also derived

Every row's `addr` was copied out of some lane's `RegisterRuntimeAddr` call — so
the addr column was trivially green for whoever copied it, row and registration
sharing one typo. The probe now asks the **DLL's export table** where each named
symbol actually lives:

```
address provenance 25 of 25 checkable rows match the DLL's own export table
(4 rows have no exported name to check against and are NOT scored)
```

This check found its own defect on its first run: the walk computed the Ghidra
address as `loaded - base + OptionalHeader.ImageBase` and printed
`0x7fff975453d0`. **The Windows loader rewrites the in-memory `ImageBase` to the
address it mapped the module at**, so that expression collapses to the loaded
address. Fixed to use the resolver's own `mi.delta`, which comes from the
module table's PREFERRED base.

---

## 3. THE BOARD, as a SET — 23 of 29 SHUT, 6 OPEN

Run: `BANISHED_AUTOSTART=1 BANISHED_RT_FILE_CUTOVER=1 BANISHED_PAUSEMENU=600
BANISHED_PAUSEMENU_QUIT=120`, exe md5 verified identical to this worktree's
build, rc=0, **0-byte crash log**. Full board: `wave20_doors/board_main_run.txt`.

```
name doors 20 of 25 answered by our registry, 5 OPEN
addr doors 23 of 29 answered by our registry, 6 OPEN
==== 23 of 29 rows SHUT, 6 OPEN ====
```

**THE OPEN SET, all six:**

| # | door | why open | whose |
|---|---|---|---|
| 1 | `?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z` (0x1800c7a50) | addr door registered **only on the DLL-absent path** — unscoreable here | allocator |
| 2 | `?MemFree@MemoryAllocator@System@@SAXPEAX@Z` (0x1800c7ad0) | same | allocator |
| 3 | `?_singleton@FileSystem@IO@@0PEAV12@EA` (0x1801fc6a8) | **tagged `RT_UNPROMOTABLE … HARMFUL`, expiry NONE, EVER** | nobody — permanent |
| 4 | `??_7Stream@IO@@6B@` (0x1801549a0) | minted, READY, **not promotable**: 22 of 50 stampers ours | port work |
| 5 | `??_7FileStream@IO@@6B@` (0x180154960) | minted, **not promotable**: 6 of 17 stampers ours | port work |
| 6 | `0x1801554a8` shared Encoding vtable (unexported) | minted, **3 of 5 slots still traps** (slots 0-2) | port work |

**`??_7BinaryFileStream@IO@@6B@` HAS MOVED** — it was open in Wave 19 for lack of
promotion, lane PROMOTE promoted it, and it reads `shut` here (`name=yes
addr=yes`), with attendance to match (`slotEntries=18910 viaMintBFS=18910
viaDllBFS=0`, `stamps=2847 == stampedMinted=2847`). Measured, not assumed.

### 3.1 Two rows this screen structurally CANNOT score

`RegisterAllocatorAddresses()` — the only registrar of 0x1800c7a50/0x1800c7ad0 —
is called **exclusively from `allocator.cpp`'s `if (!RuntimeDllPresent())` arm**.
A run with the DLL present therefore *cannot* see it, whatever
`BANISHED_RT_ALLOC` says. Measured directly (`wave20_doors/board_alloc_armed.txt`,
`BANISHED_RT_ALLOC=1`): the two rows go from `name=NO addr=NO` to **`name=yes
addr=NO`** — arming registers the exports and not the addresses — and the row
total does not move (still 23 of 29).

The board **still counts them OPEN** and names them on their own line. Re-scoring
rows it cannot check is how a board flatters itself; the honest form is "counted
OPEN, and here is why that is not evidence".

Their by-address callers (`rtcoll::MemAlloc`/`MemFree`, `collection/array.cpp`
:166/:176 and `application/message_callbacks.cpp`:104) have **no name fallback**,
so the address door is the only door they use — which is why it is registered on
the cutover path at all.

---

## 4. WHAT CANDIDATE B ACTUALLY NEEDS — the guide's split is wrong

> The guide (§2 Candidate B): *"3 open for lack of PROMOTION, not ports —
> **Candidate A closes these**; 2 are the allocator; 2 remain."*

**Only one of those three was promotable.** Measured against lane PROMOTE's
stamper census, enumerated from the binary:

* `??_7BinaryFileStream@` — **2 of 2 stampers ours. Promoted. Closed.** ✅
* `??_7Stream@` — **22 of 50** stampers ours, 25 retail, 3 unattributed.
* `??_7FileStream@` — **6 of 17**. Lane PROMOTE assessed it and it *"fails
  hard"*: the eleven retail stampers include `?Run@Stream@Audio@@` (×2),
  `?Close@`/`?Create@`/`?Open@TextFile@`, three unexported `FUN_1800b6…`,
  `?BuildPackageFile@PackageFile@`, `?AreResourcesEqual@PackageFile@`,
  `??0FileStream@` itself and `??0TextFileStream@IO@@QEAA@XZ`.

**Candidate A closes ONE of the three, and it is already closed.** The other two
need those stamper bodies ported — ordinary port work spanning **Audio, TextFile
and PackageFile**, none of it IO stream work.

**And one row can never close by promotion at all.**
`?_singleton@FileSystem@IO@@` is tagged `RT_UNPROMOTABLE … | NONE, EVER |
HARMFUL` (`reg_mint_promoted.cpp:1184`): minting our own storage splits the
brain against in-DLL RIP-relative readers. **So 29 of 29 is not reachable on this
board's own rules**, and any plan that says "close the screenshot hole" must say
what it means for that row.

**Concretely, closing what remains takes:**

1. **Nothing** for the two allocator rows *on this route* — they are cutover-path
   registrations. What they need is a **DLL-ABSENT run of this screen** to score
   them at all, which is a different harness, not a port.
2. **`0x1801554a8`**: three slot bodies (0x1800a81e0, 0x1800a81d0, 0x1800a81c0 —
   `WAVE9_LANE_ENC.md`), after which slots 0-4 are ours and the row is a normal
   promotion candidate. **The cheapest of the six by a wide margin.**
3. **`??_7FileStream@`**: 11 stamper bodies across three subsystems.
4. **`??_7Stream@`**: 28 stamper bodies. The largest single item on this board.
5. **`?_singleton@FileSystem@IO@@`**: a written verdict says never. It needs a
   decision, not a lane.

---

## 5. The stale tag (opportunistic item), and a convention nobody declared

`reg_mint_promoted.cpp:1201` tagged `??_7ReaderWriter@IO@@6B@ | 2/36 ported
[INHERITED], 30 purecall`. Corrected: **26 slots, 25 purecall** — Wave 19 C30
(two hidden tables), lane EXTENT's re-derivation this wave
(`wave20_extent_audit.txt`, CORROBORATED-EXACT), and **this tree's own
`kVSlots46`**, which is 26 entries of which 25 are `RT_MINT_PURECALL`. Three
sources, one of them the artefact the mint actually stamps.

**The numerator is written `?`, not guessed.** The `2` was counted over the
36-slot extent, so it may have lived in the ten slots that belong to other
tables. Expiry clause written in place: the next lane that counts ported slots on
this row writes the number. Nothing reads this field as a number
(`runtime_census.py` parses it as free text).

**`tools/unpromotable_slot_check.py`** (new) checks all 40 tags against the
minted rows. It cleared the other nineteen — and found an **undeclared
convention split**: the Object/Video tags' denominator is *non-purecall* slots
(`??_7Base@Object@@6B@ 2/2 (1 purecall)` on a 3-slot row), the two IO tags' is
the *whole table* (`??_7Stream@IO@@6B@ 1/7 …, 5 purecall` on 7 slots). Both are
self-consistent and they disagree by K. The tool accepts either and reports only
rows that fit neither — **stated blind spot: a row wrong by exactly K in the
other reading**. Declared in the source rather than normalised: rewriting
eighteen lanes' numbers to one convention edits eighteen written verdicts.

**Effectiveness control:** on HEAD it reports the ReaderWriter row STALE and
exits 1; on this tree 20 of 20 fit, exit 0.

---

## 6. WHAT I DO NOT CLAIM

* **Not that the screenshot works at cutover.** This board is a static registry
  query. A shut row means "our registry answers this door", never "the body
  behind it is right" and never "the path runs with the DLL absent".
* **Not that 29 is the whole door set.** The direct half is now derived; the
  **transitive half is still hand-built** and cannot discover a door nobody wrote
  down. Four were missing from a list that had been reviewed three times.
* **Not that the two allocator rows are a gap.** They are unscoreable on a
  DLL-present run. I did not run a DLL-absent boot; I have no measurement of
  them either way.
* **Not that `??_7Stream@`/`??_7FileStream@` stamper counts are mine.** They are
  lane PROMOTE's census, quoted with its own declared limit: it counts
  `ours-capable` — a registration is a door, not proof every caller uses it — and
  is an **upper bound in the optimistic direction**.
* **Not that the derived control is strong, only that it is unaimable and can
  fail.** It detects a registry that answers uniformly. A registry wrong about
  *some* symbols would pass it, exactly as the single literal would have.
* **Not that this run exercised the screenshot path.** `?CreatePath=0`,
  `?CreateFileW=0`, `?PlatformCreateFile=0`, `?Create@FileStream=0` — no BMP was
  written. The 2,847 `??0BinaryFileStream` / 9,455 `Read` entries are the
  resource **read** path. The door board does not depend on that; the attendance
  lines are printed unconditionally so nobody can read the board without them.
* **The ReaderWriter numerator is not measured.** Only its denominator and
  purecall count are.
