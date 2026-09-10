# WAVE 19 lane CSTREAM — `IO::CompressedStream` and its two subclasses

Worktree `C:/code/GIT/wt-w19-cstream`, branch `wave19/cstream`, cut from
`wave19/base` @ `e06c162` (phase 0 already merged). **Every figure below was
measured on this tip**, debug build (`mingw32-make standalone-debug`), and every
game run went through `tools/game_dir_lock.sh run lane-CSTREAM … --` as its own
statement.

**Progress is reported in BODIES and in SLOTS THEREBY CLEARED, never in a
percentage.** `called` sits in both halves of the ceiling fraction, so a lane
that opens a door moves the printed ceiling without changing anything real
(`reference_the_ceiling_percentage_is_a_moving_target.md`).

**Sections 0-9 were REPORTED AND SEALED at `8b82c10`. Everything after them is
POST-REPORT TRANCHE 2**, added after the integrator re-tasked this lane; sealing
forbids altering, not annotating, so nothing above is edited and the tranche is
additive. Where the tranche corrects a sealed section it says so and names it.

---

## 0. Headline

**14 distinct bodies closed** — 12 written in `runtime/src/io/compressed_stream_write.cpp`
and 2 added as rows to the existing `runtime/src/object/deleting_dtor.cpp`
family. They cover **all 27 vtable slots of this lane's three rows** except the
three already in the tree, plus the lane's three CODE rows.

All three of this lane's vtable rows went **BLOCKED -> READY, 9/9, TRAP=0**, and
**20 trap slots were cleared** — 14 in my own rows plus one each in
`??_7Stream@` and `??_7MemoryStream@`, which are NOT mine, and four more that
phase 0c could not count at all. The identity moved by **+3**, not +12, and
**§7 explains exactly why** — nine of
the fourteen fill MINTED VTABLE SLOTS, and the census's `called` column does not
model a minted slot as a call. That is not a defect in the work and it is not a
defect in the census; it is a statement about what the identity measures, and it
is the single most important thing in this document for anyone scoping a DATA
lane off the identity alone.

```
before   ported >= called - unpromotable      83 >= 122 - 5 = 117     34 SHORT
after    ported >= called - unpromotable      86 >= 123 - 5 = 118     32 SHORT
```

---

## 1. My lane was RE-SCOPED AGAIN, upward, and phase 0c is not wrong — it is superseded

The plan prices this lane at `??_7CompressedStream@` **8 traps** +
`??_7CompressedMemoryStream@` **6 traps**, both `[INHERITED]`. Phase 0c
re-measured and said something better: those 14 slots are **8 distinct
functions**, and §6.5 concludes "lane CSTREAM should scope 8, not 14."

**Phase 0c measured `??_7CompressedFileStream@IO@@6B@` as UNMINTED, and
therefore recorded its trap count as UNMEASURED — not zero.** Phase 0b then
MINTED it (correction C7 records all five newly minted rows as
"measured and BLOCKED"), and phase 0b is in my base at `e06c162`. So on this tip
the row exists, is a full nine slots, and is **ATTENDED** (0c §5, `post=2`).

Re-measured for this lane from `standalone/rt_data_mint_tables.inc` — which is
what the readiness screen actually dispatches, not a name grep:

| row | VA | slots |
|---|---|---|
| `??_7CompressedStream@IO@@6B@` | `0x180155640` | 9 |
| `??_7CompressedMemoryStream@IO@@6B@` | `0x180155560` | 9 |
| `??_7CompressedFileStream@IO@@6B@` | `0x1801555f0` | 9 |

**27 slots, 14 distinct functions.** By slot index:

| slot | CompressedStream | CompressedMemoryStream | CompressedFileStream | distinct |
|---|---|---|---|---|
| 0 | `0x180023fd0` | `0x1800244a0` | `0x180024100` | **3** |
| 1 | `?GetSource@Stream@IO@@` | ″ | ″ | **1** |
| 2 | `?Write@CompressedStream@` | ″ | ″ | **1** |
| 3 | `?Read@CompressedStream@` | ″ | ″ | **1** |
| 4 | `?SetCurrentPos@CompressedStream@` | ″ | ″ | **1** |
| 5 | `?GetCurrentPos@CompressedStream@` | ″ | ″ | **1** |
| 6 | `?GetSize@CompressedStream@` | ″ | ″ | **1** |
| 7 | `?Close@CS` `0x1800aa8d0` | `?Close@CMS` `0x1800a9620` | `?Close@CFS` `0x1800a9490` | **3** |
| 8 | `?PreCommit@CS` `0x1800aa8c0` | `?PreCommit@CMS` `0x1800a9660` | `?PreCommit@CS` (shared) | **2** |

**Three of the fourteen were already in the tree** and grep found them before a
line was written (§2 of the plan, and Wave 18 hit already-ported dependencies
twice in two rows):

* `?Read@CompressedStream@` — `compressed_stream_read.cpp` (Wave 9 lane PKG2)
* `?Close@CompressedMemoryStream@` — `compressedstream.cpp` (Wave 4 lane B)
* slot 0 of CompressedMemoryStream (`0x1800244a0`) — **`deleting_dtor.cpp` row
  17**, and finding that is what turned two of my three slot-0s from two hand-
  written bodies into two TABLE ROWS.

**11 DATA bodies + 3 CODE rows = 14.**

This is the same shape correction C7 recorded of phase 0b: **the saving was
real AND it grew a lane's denominator.** I reported it to the integrator before
writing code rather than at close.

---

## 2. `?Commit` was a DOOR — the plan's §2 worked example, confirmed

`runtime/src/io/compressedstream.cpp` names `?Commit@CompressedStream@IO@@`
**eight times**. None of them is a body. Lines 327 and 429 reach it through
`RT_EXPORT_ONCE`, which is a door; the other six are comments.

`ResolveExport` falls through silently to the shipped impl for an unported
symbol (`reference_converting_a_door_is_not_satisfying_it.md`), so those two
call sites have been running **the DLL's compressor** on every save this tree
has written. They keep their spelling and now resolve to our body, registry-first,
with no source change there.

The same grep also cleared eleven other names before any were written: none of
the fourteen was already ported, and the addresses that *did* hit
(`0x1800244a0`, `0x1800a9490`, `0x1800aa8c0`, …) were all comments, type headers
or the deleting-dtor table — checked one by one, not by hit count.

---

## 3. Every body, per row and per distinct function

`runtime/src/io/compressed_stream_write.cpp` (12) and
`runtime/src/object/deleting_dtor.cpp` (2). All twelve are registered **BY NAME
AND BY ADDRESS**: a name door serves `ResolveExport`/`RT_EXPORT_ONCE`, an address
door serves `ResolveAddr` and the minted vtables, and they are different doors.

| # | function | retail VA | reg. name | reg. addr | slots cleared |
|---|---|---|---|---|---|
| 1 | `?GetSource@Stream@IO@@UEBA?AVString@System@@XZ` | `0x18000c6a0` | ✔ | ✔ | **5** (Stream, MemoryStream, CS, CMS, CFS) |
| 2 | `?Write@CompressedStream@IO@@UEAA_NHPEBX@Z` | `0x1800aa6c0` | ✔ | ✔ | 3 |
| 3 | `?SetCurrentPos@CompressedStream@IO@@UEAA_NH@Z` | `0x1800aa830` | ✔ | ✔ | 3 |
| 4 | `?GetCurrentPos@CompressedStream@IO@@UEBAIXZ` | `0x1800aa850` | ✔ | ✔ | 3 |
| 5 | `?GetSize@CompressedStream@IO@@UEBAIXZ` | `0x1800aa860` | ✔ | ✔ | 3 |
| 6 | `?PreCommit@CompressedStream@IO@@UEAAXXZ` | `0x1800aa8c0` | ✔ | ✔ | 2 (CS, CFS) |
| 7 | `?PreCommit@CompressedMemoryStream@IO@@UEAAXXZ` | `0x1800a9660` | ✔ | ✔ | 1 |
| 8 | `?Close@CompressedStream@IO@@UEAAXXZ` | `0x1800aa8d0` | ✔ | ✔ | 1 |
| 9 | `?Close@CompressedFileStream@IO@@UEAAXXZ` | `0x1800a9490` | ✔ | ✔ | 1 |
| 10 | `?Commit@CompressedStream@IO@@QEAAXXZ` | `0x1800a9ee0` | ✔ | ✔ | — (CODE row) |
| 11 | `?CompressWindow@CompressedStream@IO@@AEAAXH@Z` | `0x1800a9810` | ✔ | ✔ | — (CODE row) |
| 12 | `?Create@CompressedStream@IO@@QEAA_NAEAVStream@2@H@Z` | `0x1800aa4b0` | ✔ | ✔ | — (CODE row) |
| 13 | deleting dtor, `IO::CompressedStream` | `0x180023fd0` | n/a (unnamed) | ✔ | 1 |
| 14 | deleting dtor, `IO::CompressedFileStream` | `0x180024100` | n/a (unnamed) | ✔ | 1 |

**Slots cleared across my three rows: 24 of 27.** The other three were already
ported (§1).

`?GetSource@` is worth its own line: it is the **single cheapest action in IO's
whole DATA set** (0c §6.1) and it is the sole remaining trap in `??_7Stream@`
(7/1/1) and one of two in `??_7MemoryStream@` (8/6/2). **It is in two rows that
are NOT mine**, so lanes IOBASE and FILE inherit those closures. I told the
integrator I was claiming it before writing it, because 0c §6.1 assigns it to
IOBASE and porting it twice is a failure this project has had three times in one
wave.

### 3.1 Traps aimed at these signatures, and what each check actually returned

* **`?CompressWindow@` is `AEAA` = PRIVATE, not unexported.** This DLL exports
  its private members (`reference_aeaa_is_private_not_unexported.md` — three
  silent address doors survived review on that misreading). **Verified against
  the shipped export table**, not assumed: it is export
  `?CompressWindow@CompressedStream@IO@@AEAAXH@Z` at RVA `0xa9810`. Registered
  by NAME.
* **The two deleting-dtor thunks genuinely ARE unnamed.** Also verified, not
  inherited from `deleting_dtor.cpp`'s banner: `0x180023fd0` and `0x180024100`
  return `<UNNAMED>` from the export table, so address-only registration is
  correct for them and this is not the same trap one line up.
* **`?GetSource@` has a HIDDEN RETURN SLOT.** `MOV [RSP+0x10],RDX … MOV [RDX],RAX
  … MOV RAX,RDX` — RDX is the by-value `System::String` return slot and RCX is
  `this`, which is never read. An `fn_*` typedef that omits the slot is a wild
  write that still returns the right answer
  (`reference_hidden_retslot_wild_write.md`), so the slot is spelled as an
  explicit first parameter rather than left to the compiler's agreement. The
  probe pre-poisons the slot so **"never written" and "written null" are
  different outcomes**; without that poison the comparison would pass on a body
  that writes nothing.
* **`?Create@CompressedStream` takes `AEAVStream@2@`, a Stream REFERENCE.** It
  never dispatches on it — it stores it and returns. Phase 0a's abstract-base
  question does not arise here; the first dispatch on `_source` is in `?Commit`,
  by which time the object is a concrete stream.
* **Read as DISASSEMBLY, never Ghidra's C** (`reference_ghidra_c_output_vs_disassembly.md`).
  This mattered concretely: `?Commit` makes a **virtual call on `this`**
  (`MOV RAX,[RBX] ; CALL [RAX+0x40]`, slot 8 = `?PreCommit`), and Ghidra's C is
  documented to drop whole arguments at a mistyped vtable call. The slot index
  came off the byte offset `0x40`.
* **`get_function_metrics` ignores `address=`** and silently name-matches. Not
  used; every body came from `disassemble_function`.

### 3.2 A correction to `runtime/types/io/Stream.h`'s prose

Stream.h attributes the indexed walks that pin the `+0x30`/`+0x40`/`+0x50`
arrays to `?Close@CompressedStream@IO@@UEAAXXZ` **(0x1800aa8d0)**.

**That address contains no walk.** It is five instructions:
`CMP dword [RCX+0x48],0 / JLE out / CMP dword [RCX+8],1 / JZ 0x1800a9ee0 / RET`.

**Confirmed a second way, from the raw image rather than from Ghidra's function
boundaries**, because "Ghidra split the function" is exactly the explanation that
would make this correction wrong. `read_memory` at `0x1800aa8b6`:

```
1800aa8b6  f3 c3                    REP RET          <- ?GetSize ends
1800aa8c0  c2 00 00                 RET 0            <- ?PreCommit@CompressedStream, ENTIRE
1800aa8d0  83 79 48 00              CMP [RCX+0x48],0
           7e 0a                    JLE +10
           83 79 08 01              CMP [RCX+0x08],1
           0f 84 00 f6 ff ff        JZ  -0xA00  =  0x1800a9ee0  (?Commit)
1800aa8e0  f3 c3                    REP RET
```

Twelve bytes of body between two `0xcc` pads. There is no walk there and there
is no room for one.

The walks are real and the array layouts are right — they are in `?Release@`
(`0x1800aa100`) and `?Commit@` (`0x1800a9ee0`), both of which Close reaches. The
FACT stands; the CITED ADDRESS is wrong. Recorded in the source rather than
silently corrected, because a header comment is where the next lane looks. The
layout is independently re-confirmed by `?Commit`, transcribed line by line here.

### 3.3 Two stale live-prose fixes in `deleting_dtor.cpp`

Its `BANISHED_RT_DELDTOR=0` message read "all **37** deleting destructors stay
on the DLL" and its registration banner read "all **37** are unnamed" — both
left stale by Wave 10C and Wave 10D, which had already appended rows. Both are
now derived from `kSpecCount` (39 → **41**). The `static_assert` beside the table
is what caught my addition (`41 == 39` failed the build); it is the reason those
two literals were the only stale things and not the table itself.

---

## 4. Doors this lane OPENS — `called` will grow, and here is the set

A faithful port that reaches DLL internals makes them newly `called`
(`reference_porting_can_grow_the_cutover_surface.md`). All four were named in
the source banner **before** the census was re-run, and the census then found
exactly one of them (the other three were already `called` through other files):

| door | VA | why | status |
|---|---|---|---|
| `?Create@MemoryStream@IO@@QEAA_NH@Z` | `0x1800ae7f0` | `?PreCommit@CMS` tail-jumps into it | **NEW in `called`** |
| `?CloseFile@FileSystem@IO@@QEAAXAEAUFileInfo@12@@Z` | `0x1800accc0` | `?Close@CFS` tail-jumps into it | already called — **LANE FILE'S ROW**, closes itself when they register |
| `??0Compression@System@@QEAA@XZ` | `0x1800c41a0` | `?Create@CS` constructs one | already called (probe-only door in `compression.cpp`); this is its first **LIVE** caller |
| `?Compress@Compression@System@@QEAAHPEBXIPEAXI@Z` | `0x1800c4230` | `?CompressWindow` calls it | **NEW in `called`** — and **I MISSED IT**, see below |
| `??1CompressedFileStream@IO@@UEAA@XZ` | `0x1800a9330` | the new deleting-dtor row calls it | already called |

**The fifth row is a correction, and the instrument caught it rather than the
prose.** The source banner listed four doors; the whole-tree set-diff showed
**two** symbols entering `called`, and only one of them
(`?Create@MemoryStream@`) was on my list. `?Compress@Compression@System@@` is
named twice already in `compression.cpp`, but both are probe-only and the census
does not count those, so this file is its first LIVE caller in exactly the sense
the `??0Compression` row claims. Recorded as a correction rather than folded in
silently: "I listed the doors I opened" is a claim a reader will trust, and it
was 4 of 5.

### 4.1 Set-diff of `called but not ported`, measured both sides

Taken from the census's **"Called but not ported"** section specifically, on a
clean `git archive` of `HEAD~1` for the before side, run from inside each tree
(the scanners walk cwd, not the script — `reference_scanner_walks_cwd_not_script.md`).

IO alone (`--namespace IO`):

```
before: 39 rows      after: 37 rows

LEFT   (closed by this lane)          ENTERED (newly called, still unported)
  ?Commit@CompressedStream@IO@@         ?Create@MemoryStream@IO@@QEAA_NH@Z
  ?CompressWindow@CompressedStream@IO@@
  ?Create@CompressedStream@IO@@
```

**A namespace-filtered diff cannot see a door this lane opened into another
namespace**, and this lane opened two — which is the same shape as correction
C2's own retracted check, where a `--namespace IO` listing was used to answer a
question about `System` symbols it could never contain
(`reference_a_screen_that_cannot_name_its_subject.md`). So the diff was re-run
UNFILTERED, over the whole tree:

```
before: 160 rows     after: 159 rows

LEFT   (closed by this lane)          ENTERED (newly called, still unported)
  ?Commit@CompressedStream@IO@@         ?Compress@Compression@System@@QEAAHPEBXIPEAXI@Z
  ?CompressWindow@CompressedStream@IO@@ ?Create@MemoryStream@IO@@QEAA_NH@Z
  ?Create@CompressedStream@IO@@
```

Whole-tree totals agree with the per-symbol diff: `called` 1175 → **1177** (+2,
the two entering), `ported` 971 → **974** (+3, the three leaving). Three
independent routes close.

Exactly the lane's three CODE rows out, and two doors in — **one of which the
source banner failed to predict** (§4, `?Compress@`).

**A note on how nearly I got this wrong.** My first extraction grepped every
`@IO@@` name out of the WHOLE `--unported` report, which also contains the
"Ports with no caller in our tree" listing and several other sections. It read
143 → 150 and produced a set-diff showing nine of my registered bodies
"entering" the unported set — a screen that could not name its own subject,
returning a confident wrong answer (`reference_a_screen_that_cannot_name_its_subject.md`).
What caught it was the arithmetic not closing: 39 → 37 is `+1 called − 3
ported`, and nine entering is not.

---

## 5. Verification

### 5.1 The two deleting-dtor rows — `tools/rt_deleting_dtor_verify.py`

That tool re-derives BOTH constants (the `~Class` address and `sizeof`) for
every row out of the shipped DLL and compares them to the table. It is why these
are table rows and not hand-written bodies.

```
41 of 41 rows OK, problems: 0
  OK  0x180023fd0  size=0x68  dtor=0x1800aa650  IO::CompressedStream
  OK  0x180024100  size=0xb8  dtor=0x1800a9330  IO::CompressedFileStream
```

**Negative control, scored on applicable cases only.** `0x068` was changed to
`0x070` on the CompressedStream row and the tool was re-run: it reported
`BAD 0x180023fd0 size=0x70 … problems: 1`, then `problems: 0` again on restore.
The gate has **exactly one applicable row**, that row moved, by the mechanism
the control names, in the predicted direction — **1 of 1, GREEN**. The control
corrupts a value the tool actually compares, which is the property
`reference_control_must_pick_a_compared_subject.md` exists to enforce.

`sizeof(IO::CompressedFileStream) == 0xb8` has a **circular** witness and it is
named as such: `runtime/types/io/FileStream.h:55` derives 0xb8 *from this very
thunk*. The independent witness is the ctor `0x1800a93b0`, which places the
`BinaryFileStream` member at `+0x68`, and `BinaryFileStream` is `0x50`:
`0x68 + 0x50 = 0xb8`.

### 5.2 The twelve bodies — an ARTEFACT oracle, `compressed_stream_write_probe.cpp`

Plan §5 names this case exactly: **"an artefact-producing body is compared by
its ARTEFACT."** Two screens, **scored separately, each with its own
denominator** — a union denominator lets a blind screen hide behind a working
one (`reference_union_control_hides_a_blind_screen.md`).

* **SCREEN A — byte identity.** The same content written twice, once through our
  twelve bodies and once through the shipped ones, and the two produced images
  `memcmp`'d.
* **SCREEN B — retail reads what we wrote.** Our image *alone* handed to the
  DLL's `?Open@CompressedStream`, which validates
  `4 + n*8 + sum(block sizes) == source size` **exactly** — a total structural
  checker, not a heuristic — then to the DLL's `?Read`, and the recovered bytes
  compared to the original content. Screen B never looks at retail's image, so
  it is independent of screen A.

This is deliberately **not a round trip**. Writing with our code and reading it
back with our code is a SELF-CHECK: it takes the port's own verdict and never
consults the DLL, which is precisely why the ~250 `RT_DIFF_SHADOW` sites cannot
catch a shared misunderstanding of retail
(`reference_shadow_marker_is_not_an_oracle.md`).

**Independence of the two sides, by construction.** The ours side calls
`RtIo_CompressedStream_*` by C++ symbol only — no `ResolveAddr`, no
`ResolveExport`, no DLL vtable. A probe whose ours side reaches the body BY
ADDRESS compares retail with itself, and one was dark for five waves on exactly
that (`reference_registering_an_address_converts_a_selfcheck_into_an_oracle.md`).

**The vtable is load-bearing and is this file's own.** `?Commit` dispatches
`?PreCommit` virtually through `[RAX+0x40]`. Stamping the DLL's table would have
left `?PreCommit@CompressedMemoryStream` — the body that **sizes the output
buffer** — never entered, with every screen still green. The probe stamps a
nine-slot table it builds itself, all ours, and slot 8 is a **counting wrapper**
so that the dispatch landing on our body is MEASURED, not assumed.

Results: **§5.4.**

### 5.3 The positive control CRASHED, and that is a finding about the port

The obvious control is the realistic transcription defect: clamp the chunk
against `w->end` instead of `w->start + 0x8000`, which is what `?Read` — the
body one file over — genuinely does.

**That control was written, run, and it faulted.** ACCESS_VIOLATION, `WRITE of
0x1e3994c8000` (page-aligned) inside `ucrtbase`'s memcpy, symbolized off
`standalone_crash.log` to `WriteBuggy+0xc2` in the probe's own anonymous
namespace, three frames under `RtIo_CompressedStreamWriteProbe`.

It is disqualified **by rule, not by taste**: §6.4 requires a control to produce
a comparable divergence, **not UB**. A control that faults says nothing about
the screen, because the screen never ran — this one took the whole probe down
before a single denominator printed.

It is recorded rather than quietly swapped out, because the crash is itself a
statement about the body under test: **retail's clamp against the window
CAPACITY is load-bearing for memory safety, not merely for correctness.** A port
that clamped against `end` would be a heap overflow on the first write into a
fresh window, not a wrong number.

The shipped control corrupts the **payload** instead — it complements the first
byte of every piece written. It is artificial and this document says so. What it
still does correctly: it mutates a value **both screens compare**, it stays
inside the window buffer, and it is applicable to every non-empty case. What it
does **not** cover: a defect in block LAYOUT (a wrong `sourceOffset`, a wrong
directory length). Screen B's `?Open` would catch those structurally, but that
is an argument from reading `?Open`'s disassembly, **not a measurement**, and it
is labelled as such here and in the source.

### 5.4 Measured results

Run archived in `docs/runtime_port/wave19_cstream/run_gameplay/` — full
`standalone_debug.log`, `standalone_error.log`, a **0-byte** `standalone_crash.log`,
`exit_rc.txt` (`rc=0`) and `exe_md5.txt` (the md5 of my build and of the
deployed exe, identical, plus the `lane CSTREAM` marker count in the DEPLOYED
binary). Route: `BANISHED_AUTOSTART=1 BANISHED_RT_DIFF=1
BANISHED_RT_MINT_SHADOW=1 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120` —
the full gameplay route, **not** the AUTOQUIT one, so the exit-phase readiness
table exists.

#### The oracle

```
CONTROL (a wrong Write clamp must be caught): 12 of 12 applicable -- GREEN
SCREEN A (our bytes == retail's bytes):       12 of 12 cases
SCREEN B (retail's reader recovers our image):12 of 12 cases;
                                              retail's Open ACCEPTED 12,
                                              Read+compare AGREED 12
PURE READERS (ours == the DLL's):              4 of 4
EFFECTIVENESS: 42 blocks written by us, 33 COMPRESSED, 9 STORED; 0 skipped
```

**The control read non-zero on every applicable case before any green was
believed** — that is the precondition, and the probe is written to say "NO GREEN
BELOW IS TRUSTED" and print the numbers anyway if it had not.

**The effectiveness counters say the screens measured something rather than
nothing.** 42 blocks with **both** kinds present means the
`compressedSize > uncompressedSize` → STORED rule was genuinely exercised; a
corpus of only-compressible data would have left that branch dark and the probe
warns when either count is zero. 12 cases = 4 corpora × 3 window counts (1, 2 and
4 windows), which also means the eviction path was driven, not just the
single-window case.

#### Attendance — read `worked=`, not `calls=`

Driver counters:

```
Create=24  Write=282  Commit=24  PreCommit(base)=1  PreCommit(mem)=24
Close=24   GetSize=1  GetCurrentPos=1  SetCurrentPos=1  GetSource=1
```

`PreCommit(mem)=24` is the load-bearing one: **`?Commit`'s virtual dispatch
through `[RAX+0x40]` landed on OUR `?PreCommit@CompressedMemoryStream` 24 times.**
That counter exists because the alternative — stamping the DLL's vtable — would
have left that body unentered with every screen still green.

The RT_DIFF harness's own table, which sees the two bodies the driver cannot:

```
IO::CompressedStream::Create            calls=0  worked=24   divergences=0
IO::CompressedStream::Write             calls=0  worked=282  divergences=0
IO::CompressedStream::Close             calls=0  worked=24   divergences=0
IO::CompressedStream::Commit            calls=0  worked=24   divergences=0
IO::CompressedStream::CompressWindow    calls=0  worked=44   divergences=0
IO::CompressedMemoryStream::PreCommit   calls=0  worked=24   divergences=0
IO::CompressedStream::GetCurrentPos     calls=0  worked=1    divergences=0
IO::CompressedStream::GetSize           calls=0  worked=1    divergences=0
IO::CompressedStream::SetCurrentPos     calls=0  worked=1    divergences=0
IO::Stream::GetSource                   calls=0  worked=1    divergences=0
IO::CompressedStream::PreCommit         calls=0  worked=0    divergences=0
IO::CompressedFileStream::Close         -- NO ROW AT ALL --
```

**Three of those lines mean something other than what they look like, and all
three are traps this project has already paid for:**

* **`calls=0` on every row is BY CONSTRUCTION and says nothing.** These bodies
  carry `RT_DIFF_BEGIN`/`RT_DIFF_WORK` and no in-body oracle, so the harness
  never runs a shipped body alongside them; `calls=` counts that, and there is
  nothing to count. The attendance is `worked=`
  (`reference_uncalled_port_is_not_verified.md` is about a genuinely uncalled
  port; this is the other shape,
  `reference_calls_zero_can_mean_always_took_the_uncovered_branch.md`).
* **`?PreCommit@CompressedStream` reads `worked=0` and DID run.** It is a
  one-instruction no-op, so its body passes `RT_DIFF_WORK(false)` — truthfully.
  Its attendance is the driver's `PreCommit(base)=1`. A reader taking
  `calls=0 worked=0` as "never entered" would be wrong, which is why both
  numbers are printed side by side.
* **`?Close@CompressedFileStream` has NO ROW, and that IS the evidence it never
  ran.** `RT_DIFF_BEGIN` registers a site lazily on first entry, so an absent row
  is a stronger negative than a zero. It is the one body here with no attendance
  of any kind, and §8 says so.
* `CompressWindow worked=44` against `Commit worked=24`: Commit flushes every
  resident window that holds anything, so 44 flushes across 24 commits is the
  multi-window corpora doing what they were chosen to do.

#### The readiness screen — what the identity cannot see

`BANISHED_RT_MINT_SHADOW=1`, **exit phase**, DLL present, default flags. The
whole screen with its denominators:

```
vtables            : 62, 598 slots
ground truth       : 598 slot comparisons vs the live DLL, 0 MISMATCH
minted from OUR    : 282 slots, 0 MISMATCH
complete vtables   : 30 of 62
effectiveness      : 282 of 450 non-purecall slots carry our own
==== clean: 0 problem(s) ====        (printed at BOTH phase=init and phase=exit)
```

This lane's three rows, and the two rows it changed that are **not** its own:

| row | phase 0c @ `ebbec6e` | this run | |
|---|---|---|---|
| `??_7CompressedStream@IO@@6B@` | 9 / 1 / **TRAP=8** BLOCKED | 9 / 9 / **TRAP=0** | **READY** |
| `??_7CompressedMemoryStream@IO@@6B@` | 9 / 3 / **TRAP=6** BLOCKED | 9 / 9 / **TRAP=0** | **READY** |
| `??_7CompressedFileStream@IO@@6B@` | **unminted — UNMEASURED** | 9 / 9 / **TRAP=0** | **READY** |
| `??_7Stream@IO@@6B@` | 7 / 1 / **TRAP=1** BLOCKED | 7 / 2 / **TRAP=0** (5 purecall) | **READY** — not my row |
| `??_7MemoryStream@IO@@6B@` | 8 / 6 / **TRAP=2** BLOCKED | 8 / 7 / **TRAP=1** | still BLOCKED — not my row |

`??_7MemoryStream@`'s one remaining trap is **slot 0, `DLL 0x1800242a0`,
unexported** — the deleting-destructor thunk, which is `deleting_dtor.cpp`'s
pattern and one table row for whoever owns that class. It is **not** claimed
here.

**20 trap slots cleared: 14 in my own three rows, 1 in `??_7Stream@`, 1 in
`??_7MemoryStream@`** (the two `?GetSource@` closures), and 4 more inside my rows
that phase 0c could not count because `??_7CompressedFileStream@` was unminted.

**Independent corroboration, from an aggregate that was not used to build that
table.** Phase 0c reported `256` slots minted from our own code and `26 of 57`
complete vtables. This run reports **282** and **30 of 62**. `282 − 256 = 26`,
which is exactly this lane's 24 slots across its three rows plus the two
`?GetSource@` slots in `??_7Stream@` and `??_7MemoryStream@`; and `30 − 26 = 4`
is exactly the three rows that went READY plus `??_7Stream@`. Two aggregates and
the row table agree, and none of them was derived from the others.

**A control phase 0c published that this lane FALSIFIES, and it matters for
anyone re-using that reasoning.** Phase 0c's control C3 found the `init` and
`exit` readiness tables identical across all 57 rows and concluded "**the phase
boundary is not a control for this screen**". That was true then and is **false
now**: `RtDataMintInit()` runs inside `RuntimePortInit()` *before*
`RtIoPortInit()` registers this lane's twelve bodies, so an init-phase table
cannot see them. In this run both phases happen to agree — the registrations land
before the first report — but the reasoning behind C3's conclusion no longer
holds, and a lane registering later would be invisible at init.

I hit that in practice, in the way that costs a wrong conclusion: an earlier
read showed `?GetSource@` still trapping in three rows, and it came from a
**stale log another lane had already overwritten** while my run's data was gone
from the game directory. That is why this run archives its logs **inside the
locked section**, and why `exe_md5.txt` records the md5 of the deployed binary
rather than the built one.

#### One stale line in the archived log

The archived `[CSWrite] NOT COVERED HERE:` line names `?Close@CompressedStream`
as not covered. **That wording is wrong** — it predates routing both sides of the
oracle through `?Close` rather than straight to `?Commit`, and the `ATTENDANCE`
line in the same report reads `Close=24`. The source is corrected; **no measured
number changes**, and the archived log deliberately still carries the old text
rather than being regenerated to look tidy. A "not covered" claim sitting beside
a non-zero counter for the same body is believed because it sounds more
specific, which is the only reason this is worth a paragraph.

### 5.5 Run configuration, and what the route does and does not reach

`BANISHED_AUTOSTART=1 BANISHED_AUTOQUIT=1 BANISHED_RT_DIFF=1
BANISHED_RT_MINT_SHADOW=1`, DLL present, default flags otherwise.

`BANISHED_AUTOQUIT` **overrides autostart's New Game path**
(`reference_autoquit_overrides_autostart_newgame.md`), so this route is
**boot → menu → quit** and does NOT enter gameplay. That is deliberate and it is
sufficient for this lane: the probe is invoked from `FileSystem::LoadPackages`,
which is on the boot path — the first point where the heap is provably live —
and it builds its own subjects rather than waiting for a caller. It does mean
**no attendance claim in this document covers the gameplay, save or load
routes.**

The exe under test was verified inside the locked section
(`strings … | grep -c 'lane CSTREAM'`). That check exists because **the first
run of this lane measured a STALE exe**: another lane deployed over the game
directory between my build and my read of it, and the missing registrar line was
the only evidence.

---

## 6. What the write path STILL owes after this lane

**`?Write` reaches `?SetWindow`, whose `direction != 0 && blk >= headerSize`
GROW branch is NOT ported.** `compressed_stream_read.cpp` §4 delegates it to the
shipped body, counts it and announces it once as a named cutover blocker.
**Every write that starts a new block takes that branch.**

So this lane does **not** make the writer cutover-clean, and "the writer is
ported" would be false. What it does is close 14 bodies and 24 of 27 trap slots.
The grow path's status is exactly as `compressed_stream_read.cpp` left it — this
lane neither improves it nor worsens it, and the oracle in §5.2 can neither
blame nor clear it, because it is the DLL's on **both** sides by construction.

---

## 7. WHY THE IDENTITY MOVED BY 3 AND NOT BY 12 — read this before scoping a DATA lane

Twelve exports were registered by name and by address. `ported` moved **+3**.

`ported` in `tools/runtime_census.py` is `|called ∩ registered|`. Nine of my
twelve bodies are **vtable slots**: nothing in our tree calls them by name or by
address, because they are dispatched through a minted vtable at run time. The
census's `called` column does not model a minted slot as a call, so those nine
land in "Ports with no caller in our tree" and contribute **zero** to the
identity. The three that moved it are exactly the three the plan lists as CODE
rows.

**This is not a bug report about the census** — it counts what it says it counts,
and `standalone/rt_data_mint_tables.inc` is reported on its own line. It is a
statement about what the Wave 19 success criterion can and cannot see:

> **The identity is blind to DATA work.** A lane that closes every trap slot in
> every vtable it owns can move `ported >= called - unpromotable` by almost
> nothing, and a lane that ports three ordinary functions moves it by three.

The instrument that *can* see this work is the readiness screen
(`BANISHED_RT_MINT_SHADOW=1`), which is why §5.4 reports trap counts per row
alongside the identity. Scoring a DATA lane on the identity alone would report
this lane as having done a fifth of its work.

---

## 8. What this does NOT claim

* **It does not claim the writer is cutover-clean.** §6. The `?SetWindow` GROW
  branch is still the DLL's and every new block takes it.
* **It does not claim any row is PROMOTED.** Three rows are now READY with
  TRAP=0, and READY means "safe to stamp", **not** that anything dispatches it
  (`reference_a_green_readiness_screen_is_not_a_route.md`). Promotion is the
  integrator's call and **no row here was promoted**.
* **It does not claim `??_7Stream@IO@@6B@` or `??_7MemoryStream@IO@@6B@`.** They
  moved because `?GetSource@` is their slot 1 too. `??_7Stream@` is now READY;
  `??_7MemoryStream@` still carries one trap — **slot 0, `DLL 0x1800242a0`,
  unexported**, the deleting-destructor thunk, which is one `deleting_dtor.cpp`
  table row for whoever owns that class and is NOT claimed here.
* **It does not claim the oracle covers block LAYOUT, only that screen B would.**
  See §5.3: the shipped control is a payload corruption. Screen B's `?Open`
  validates `4 + n*8 + sum(block sizes) == size` exactly, which WOULD catch a
  wrong `sourceOffset` — but that is an argument from reading `?Open`'s
  disassembly, and no control demonstrated it.
* **It does not claim `?Close@CompressedFileStream` is verified.** It is ported
  and registered; the oracle does not reach it, because it needs a real file and
  lane FILE's `?CloseFile`. Its *body* is five branches transcribed from the
  disassembly, and that is all the evidence there is for it.
* **It does not claim `?PreCommit@CompressedStream` is verified beyond
  attendance.** It is one instruction (`RET`). There is nothing to compare. It is
  called so its counter is non-zero, and that is the whole claim.
* **It does not claim anything about the gameplay, save, load or mods routes.**
  §5.5: the measured route is boot → menu → quit. `BANISHED_AUTOQUIT` overrode
  the New Game path by design.
* **It does not claim the positive control covers block LAYOUT defects.** §5.3.
  The shipped control is a payload corruption; the structural argument rests on
  reading `?Open`'s disassembly, which is not a measurement.
* **It does not claim `?SetWindowCount` or `?Release` are verified.** They are
  ours on the ours side and the DLL's on the theirs side of the oracle, as they
  should be — but they are lane PKG2's rows and a divergence in them would be
  MISATTRIBUTED to this lane's bodies.
* **It does not re-verify phase 0a.** The abstract-base NO-OP question is
  untouched here.
* **It does not claim `(unexported @0x1801554a8)` is or is not a FileStream
  secondary table.** I passed lane FILE a second, different hypothesis with an
  in-tree witness (`Stream.h` names that address as the shared `IO::Encoding`
  vtable every concrete stream ctor stamps) and explicitly did not test either.
* **Trap-slot figures quoted from phase 0c are under DEFAULT FLAGS.** A run with
  any `BANISHED_RT_*=0` measures something else, correctly.

---

## 9. Files touched

```
runtime/src/io/compressed_stream_write.cpp        NEW   12 bodies + registrar
runtime/src/io/compressed_stream_write_probe.cpp  NEW   the artefact oracle
runtime/src/io/rt_io_port.cpp                     +1 registrar, BANISHED_RT_CSWRITE
runtime/src/io/filesystem_packages.cpp            +1 probe call site
runtime/src/object/deleting_dtor.cpp              +2 table rows, +2 bodies,
                                                  +2 registrations, 2 stale-prose fixes
runtime/src/object/deleting_dtor_internal.h       kSpecCount 39 -> 41
docs/runtime_port/WAVE19_LANE_CSTREAM.md          this file
```

`BANISHED_RT_CSWRITE` is a kill switch (`RtWaveEnabled` is default-ON), its own
switch and not folded into `BANISHED_RT_CSREAD`, because this is the half of the
class that **produces** bytes: an A/B that could only move the read and write
halves together could not attribute a corrupt save to either.

### 9.1 This lane changes SAVE-WRITING behaviour today, not only at cutover

`?Close@CompressedMemoryStream@` is already ported and already live
(`compressedstream.cpp:429`), and it reaches `?Commit` through `RT_EXPORT_ONCE`.
Until this lane, that door fell through to **the DLL's compressor**. It now
resolves to ours, and `code/Game/Game/gameupdateloop_serialize.cpp` is on the
game's real save path.

So `BANISHED_RT_CSWRITE=1` (the default) means **every save this build writes is
compressed by our `?Commit`, `?CompressWindow` and `?Write`.** That is the point
of the lane, and it is also its whole risk. The kill switch exists for exactly
this, and the oracle in §5 exists to make the default defensible rather than
hopeful. This file's own recommendation was to run with `BANISHED_RT_CSWRITE=0` until
§5.4 had numbers in it. **It now does** — control 12 of 12 GREEN, screen A 12 of
12, screen B 12 of 12 with retail's `?Open` accepting all twelve of our images
and its `?Read` recovering the original bytes from all twelve — so the default-ON
switch is defensible rather than hopeful. The switch stays, because the blast
radius has not changed.

---

# POST-REPORT TRANCHE 2 — re-tasked after §0-§9 were reported and sealed

Everything above §9 was reported at `8b82c10` and is unaltered. This tranche is
additive, and it exists because the integrator sent four corrections and one
mandatory action after that report. Merged `wave19/base` @ `adc679a` (which
brings lanes IOBASE, FILE, MINT and TRAPS tranche 2) and re-measured on the
merged tip.

## T2.1 `?GetSource@Stream@IO@@` — DROPPED, probe KEPT

**Both lanes ported it.** Mine is deleted: the body, the `RegisterRuntimeExport`
and the `RegisterRuntimeAddr`. Lane IOBASE's (`runtime/src/io/iobase_vtables.cpp`)
is the surviving one, and my three rows consume it with no registration here,
because `ResolveExport` and `ResolveAddr` are both registry-first.

The integrator's stated reason was process (IOBASE is sealed, I am live).
**IOBASE's own reason is better and is the one recorded here:** `??_7MemoryStream@`
and `??_7VisitorStream@` are PROMOTED and reach READY only through that body, so
reverting IOBASE's copy would open an interval in which two promoted rows carry
traps. A resolution that transiently breaks the tree loses to one that does not.

**The cheap end of the collision is that it would not have linked** — both bodies
used the same `extern "C" RtIo_Stream_GetSource` symbol. The expensive end is the
one that *does* link: two `RegisterRuntimeExport` calls on one name are a
SHADOWED binding where the later wins and the earlier line is dead code that
still reads like a port.

**The probe is kept and repointed at IOBASE's body**, declared with IOBASE's
signature (`void* (const void*, void**)` — the same ABI spelled through `void*`).
Kept because it is the only differential oracle either lane has for that body:
IOBASE names "no differential oracle in this lane" as its own largest gap, and a
body with no call door cannot be verified by a live run at all. On the merged tip
it is inside `PURE READERS: 4 of 4`, with the return slot **pre-poisoned** so
"never written" and "written null" are different outcomes — which is the actual
defect class for a hidden return slot.

### The verification tool, and the pipeline that hid its verdict

`tools/dispatch_slot_map.py --check` **cannot see this duplicate** — it scans
`RegisterDispatchSlot` (the exe's `.rdata` IAT) and is structurally blind to
`RegisterRuntimeExport`. I had quoted its green twice in §5/§9 of this document;
it was green about a different subject. `tools/dup_runtime_export.py` is the tool
for this subject.

```
scanned 1521 files under C:\code\GIT\wt-w19-cstream
1111 distinct RegisterRuntimeExport names,   7 duplicated
1148 distinct RegisterRuntimeAddr addresses, 7 duplicated
?GetSource@Stream@IO@@ / 0x18000c6a0 : ABSENT from both duplicate lists
compressed_stream_write.cpp          : ABSENT from both duplicate lists
```

The 14 duplicates are pre-existing and are Wave 20's, not this wave's (they
include `runtime/src/object/internal.cpp` registering the same four symbols twice
in one file).

**My first invocation of that tool reported `EXIT=0` and it was 3.** I had piped
it into `tail -30`, and `$?` after a pipeline is the LAST command's status — so
`tail`'s success was read as the checker's verdict. That is
`reference_a_piped_acquire_discards_the_refusal.md` exactly, one tool over from
where it was first learned. Re-run unpiped: **exit 3**, which is the correct
verdict for a tree with 14 pre-existing duplicates and is not about my removal.

## T2.2 Slot extents RE-DERIVED FROM THE BYTES — the sizing defect does not bite these three

The integrator's warning is that `kMintVtables` sizes rows by
`adjacent-export:<next symbol>`, which **cannot see an unexported table lying
between two exported ones**, and that the mint shadow is **structurally blind to
extent** (an over-sized row's surplus slots are read from the DLL at their real
addresses and compare equal, so `598 comparisons, 0 MISMATCH` was true and
silent). `??_7ReaderWriter@` went 36 → 31 → 26 on that defect.

**So I re-derived all three of my rows from `.rdata` directly**, by the
next-stamped-head rule rather than the next export. `read_memory` over
`0x180155500`–`0x1801556a0`, decoded as little-endian qwords:

```
0x180155560  0x1800244a0  0x18000c6a0  0x1800aa6c0  0x1800aa780  0x1800aa830
             0x1800aa850  0x1800aa860  0x1800a9620  0x1800a9660
0x1801555a8  0x0000000000000000        <-- NULL TERMINATOR  => 9 slots
0x1801555b0  ??_7MemoryStream@ begins

0x1801555f0  0x180024100  0x18000c6a0  0x1800aa6c0  0x1800aa780  0x1800aa830
             0x1800aa850  0x1800aa860  0x1800a9490  0x1800aa8c0
0x180155638  0x0000000000000000        <-- NULL TERMINATOR  => 9 slots
0x180155640  ??_7CompressedStream@ begins

0x180155640  0x180023fd0  0x18000c6a0  0x1800aa6c0  0x1800aa780  0x1800aa830
             0x1800aa850  0x1800aa860  0x1800aa8d0  0x1800aa8c0
0x180155688  "PackageFile\0"           <-- ASCII, NOT A POINTER => 9 slots
```

**All three are exactly 9, and the evidence is a terminator in the row's own
bytes, not an adjacency argument.** `??_7CompressedStream@`'s terminator is the
mint table's own `[tail-not-a-pointer]` annotation, independently reproduced
here. So the `adjacent-export` hazard is real and narrow — it does not reach
these rows — and I can say that from the bytes rather than from having been
lucky.

**Lane TRAPS's byte-identity claim is CONFIRMED, and it is larger than stated.**
`CompressedFileStream` and `CompressedStream` share slots **1–6 AND slot 8** —
seven identical addresses, not six. Only slot 0 (the class's own deleting-dtor
thunk) and slot 7 (`?Close@`) differ. That is why 12 bodies close 24 slots.

## T2.3 `CompressedCopyFileStream` — it IS my family, and it is NOT a wave row

Asked twice, so answered from three sides.

* **It is a real, exported 9-slot vtable at `0x180155510`** (`??_7CompressedCopyFileStream@IO@@6B@`,
  ordinal 1407 per `runtime/types/io/FileStream.h:184`), terminated by a null
  qword at `0x180155558`, with `??_7CompressedMemoryStream@` starting at
  `0x180155560`. Read from `.rdata`, same method as T2.2.
* **It is in my family.** Its Ghidra xrefs name `CompressedCopyFileStream`'s
  constructor and destructor as the stampers, and its slots are:

```
  0  0x1800245f0   its own deleting-dtor thunk        NOT PORTED
  1  0x18000c6a0   ?GetSource@Stream@IO@@             PORTED (lane IOBASE)
  2  0x1800a90e0   its own Write                      NOT PORTED
  3  0x1800a9080   its own Read                       NOT PORTED
  4  0x1800aa830   ?SetCurrentPos@CompressedStream@   PORTED (this lane)
  5  0x1800aa850   ?GetCurrentPos@CompressedStream@   PORTED (this lane)
  6  0x1800aa860   ?GetSize@CompressedStream@         PORTED (this lane)
  7  0x1800a9140   its own Close                      NOT PORTED
  8  0x1800aa8c0   ?PreCommit@CompressedStream@       PORTED (this lane)
```

* **It is NOT called by our tree, so it does NOT belong in the wave's row set.**
  It is absent from the census's called-but-unported listing, and a whole-tree
  grep for the mangled name and for `0x180155510` finds only documentation and
  one comment in `FileStream.h`. There is no call door.
* **It is UNMINTED**, and the way it appears is the trap phase 0b named: its only
  occurrence in `standalone/rt_data_mint_tables.inc` is as the
  `adjacent-export:` LABEL on `??_7BinaryFileStream@`'s row. A name grep would
  read that as a hit.

**The free part**: 5 of its 9 slots already hold ported bodies as a side effect
of this lane and IOBASE, so if it is ever minted it starts at 9/5/TRAP=4 rather
than 9/1/TRAP=8. It costs four bodies, all of them its own, none of them anyone's
row today. **Recorded, not claimed.**

## T2.4 READY IS NOT PROMOTABLE — stampers enumerated per row

The integrator's criterion: every stamp of these vtables is an in-DLL
RIP-relative `LEA`, not a door, so **a row is promotable exactly insofar as we
own the FUNCTION THAT STAMPS IT.** Applied to my three READY rows, from Ghidra
xrefs (`[DATA]` references, which are the `LEA` sites), one row per distinct
stamping function:

| row | distinct stampers | ours | theirs |
|---|---|---|---|
| `??_7CompressedStream@` | 9 | **2** — `??0CompressedStream@` (0x1800aa3d0), `??1CompressedStream@` (0x1800aa650) | `Save`, `IsCurrent`, `Load`, `BuildPackageFile`, `LoadMetaData`, `SaveMetaData`, and an inlined ctor copy at 0x180023e9b |
| `??_7CompressedMemoryStream@` | 7 | **3** — `??0CompressedMemoryStream@`, `??1CompressedMemoryStream@`, `?ReadPackfile@PackageFile@` | `BuildPackageFile`, `LoadMetaData`, `SaveMetaData`, an inlined ctor copy at 0x180024384 |
| `??_7CompressedFileStream@` | 6 | **0** | `??0CompressedFileStream@`, `??1CompressedFileStream@`, `CompressedCopyFileStream` ctor, `Save`, `IsCurrent`, `Load` |

**So all three rows are READY and none is a good promotion candidate**, and
`??_7CompressedFileStream@` is the worst of the three despite being 9/9/TRAP=0:
we own **zero** of its stamp sites. Promoting it would publish our table and
leave every stamp on retail's side.

This is the sharpest form of
`reference_a_green_readiness_screen_is_not_a_route.md` I have met: the readiness
screen ranks these three identically, and the stamper census separates them
0 / 2 / 3. **No row here is promoted, and this table is the reason.** I recommend
the integrator promote none of them in this wave.

## T2.5 Corrections adopted, and one figure I never used

* **`27 trap slots / 20 distinct functions` is RETRACTED to `24 / 17`.** It does
  not change this lane's count: **my 14 was never derived from it.** §1 says how
  it was derived — by decoding my three rows' slot arrays out of
  `standalone/rt_data_mint_tables.inc` on my own tip, precisely because the
  figure I was handed was `[INHERITED]`. T2.2 has now re-derived the same three
  rows a second time from `.rdata`. Two routes, same answer.
* **`ReaderWriter slots 31–33 close 6 slots across two rows` is RETRACTED to 3 in
  one row.** Not used here.
* **`??_7ReaderWriter@` is 26 slots.** No reasoning in this document leans on it.
* **`?GetSource@` is the sole trap in SIX rows, not four.** Confirmed
  independently here: `0x18000c6a0` appears at `.inc` slot index 1 of
  CompressedStream, CompressedMemoryStream, CompressedFileStream, MemoryStream,
  VisitorStream and Stream. It is IOBASE's body, not mine (T2.1).
* **The integrator's sharpening of §7 is CORRECT and I adopt it.** I wrote "the
  identity is blind to DATA work"; the precise statement is **"the identity sees
  a vtable row being CLAIMED, and is blind to the work that makes the claim
  true."** My own measurement supports the sharper version: I registered no
  `??_7` symbol at all, so my twelve bodies moved `ported` by three; IOBASE
  registered `??_7` symbols and those *did* move it. The blind half is the SLOT
  BODIES, which is exactly the half that makes a row promotable.
* **The census MECHANISM I gave in §7 was slightly wrong and the conclusion
  stands.** I wrote that caller-less registrations "land in Ports with no caller
  and contribute zero"; the integrator's structural read of `runtime_census.py`
  confirms the row set is built by iterating `called_*`, so such a registration
  **never becomes a row at all**. Zero identity movement is unchanged.
  The integrator also found the precedent: Wave 10's
  `UI::TabElement::UpdateContainers` was the first instance of this same
  blindness, fixed by folding detour targets in as called+ported+detoured with
  the justification *"the detour is what makes it run"* — and **the minted vtable
  is what makes these slots run**. That makes this a precedented census fix for
  Wave 20 rather than only a caveat.

## T2.6 The kill-switch hazard IOBASE named — asked, and it does NOT apply here

IOBASE reports that `BANISHED_RT_IOBASE=0` removes its bodies but **not** its
promotions, so that combination publishes a trap-carrying table and the switch is
therefore not a clean control for the promotion half.

**`BANISHED_RT_CSWRITE` does not have that shape.** `RtIoCompressedStreamWriteRegister()`
registers twelve bodies and promotes nothing — this lane promotes no row (T2.4
argues it should not). So `BANISHED_RT_CSWRITE=0` removes exactly the twelve
bodies, my three rows revert to BLOCKED with their traps intact, and no table is
published without its bodies. **It is a clean control.** Checked rather than
assumed: the registrar's whole body is `RegisterRuntimeExport`/`RegisterRuntimeAddr`
pairs and one `printf`.

## T2.7 Re-measured on the merged tip — everything holds

Run archived in `docs/runtime_port/wave19_cstream/run_postmerge/`. Same route as
§5.5, `rc=0`, **0-byte crash log**, deployed-exe md5 recorded inside the locked
section and equal to my build's.

```
CONTROL         12 of 12 applicable -- GREEN
SCREEN A        12 of 12 cases
SCREEN B        12 of 12 cases; retail's Open ACCEPTED 12, Read+compare AGREED 12
PURE READERS     4 of 4        (now including lane IOBASE's GetSource body)
EFFECTIVENESS   42 blocks, 33 COMPRESSED, 9 STORED, 0 skipped
ATTENDANCE      Create=24 Write=282 Commit=24 PreCommit(base)=1 PreCommit(mem)=24
                Close=24 GetSize=1 GetCurrentPos=1 SetCurrentPos=1 GetSource=1
```

Readiness, exit phase, whole screen: **62 vtables, 598 slots, 598 of 598 compared
against the live DLL 0 MISMATCH, 291 slots minted from our own code, 34 of 62
complete**, clean at both phases.

```
??_7CompressedStream@IO@@6B@        9 / 9 / TRAP=0   READY
??_7CompressedMemoryStream@IO@@6B@  9 / 9 / TRAP=0   READY
??_7CompressedFileStream@IO@@6B@    9 / 9 / TRAP=0   READY
```

`??_7MemoryStream@` (8/8/0, PROMOTED) and `??_7VisitorStream@` (7/7/0, PROMOTED)
are IOBASE's and are green through its GetSource, not mine. `??_7Stream@` is
7/2/TRAP=0 READY and **must stay UNPROMOTABLE(NO-OP)** per phase 0a — noted
because a lane reading only the readiness screen would reach for it.

Census on the merged tip: **IO 125 called / 108 ported** (three lanes' work
together). This lane's own contribution to the identity is unchanged at **+3**,
and T2.5 explains why that number is not the measure of it.

## T2.8 What THIS TRANCHE does not claim

* **It does not claim `CompressedCopyFileStream` is finished, minted or safe to
  mint.** Its four own bodies are unported and it has no call door. Five of nine
  slots being covered is a side effect, not a deliverable.
* **It does not claim any promotability verdict beyond the stamper counts.**
  Those counts are distinct stamping FUNCTIONS from Ghidra `[DATA]` xrefs. I did
  not verify that every such xref is a vptr store rather than some other use of
  the address, and one of them (`0x1801b204x`, in every row) is plainly a
  DATA table rather than a stamper and is excluded.
* **It does not re-verify lane IOBASE's `?GetSource@` body beyond what my probe
  compares** — one case, against the DLL, with a poisoned return slot. That is
  more than IOBASE has, and it is still one case.
* **It does not claim the mint shadow validates any slot COUNT.** T2.2 exists
  because it structurally cannot; my counts come from `.rdata` terminators.
* **It does not re-run the `?Write` control question.** The UB control stays
  disqualified and the payload control stays artificial, as §5.3 says.
