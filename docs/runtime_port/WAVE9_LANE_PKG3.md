# WAVE 9 LANE PKG3 — `?ReadPackfile@PackageFile@IO@@` lands

**Continues [WAVE9_LANE_PKG2.md](WAVE9_LANE_PKG2.md) and its section numbering.**
PKG2 §67-§90 is the decode; this is the transcription and its verification. The
per-instruction reasoning lives beside the code in
`runtime/src/io/package_file_read_win.cpp` (§91-§96 there), so it is not copied
twice; this file is the findings and the measurement.

Read PKG2 §87 (the two open reads), §76 (the over-read and what it means for the
oracle) and §89 (the three standing items) first — every one of them is answered
below.

---

## 91. The two reads §87 refused to guess, answered

**The entry record's field assignment** (0x1800b5fa4, four instructions). The
frame identity that reads them is `RBP = RSP + 0x100` — `LEA RBP,[RSP-0x1860]`
at 0x1800b46c0 followed by `SUB RSP,0x1960`. Every stack slot in the function
resolves through that one equation, and each one cross-checks against a field
whose offset is already pinned: `[RSP+0x40]` is the `CompressedMemoryStream`, so
`[RSP+0x5c]` is `+0x1c` position, `[RSP+0x70]` is `+0x30` windowData,
`[RBP-0x58]` is `+0x68` the embedded MemoryStream, `[RBP-0x78]` is `+0x48`
headerSize. Five agreements, no assumption.

```
1800b5fa4  MOV EAX,dword ptr [RBP + -0x10]   ; the FIRST of the two reads
1800b5fa7  MOV dword ptr [RBP + -0x18],EAX   ; temp + 0x08  == offset
1800b5faa  MOV EAX,dword ptr [RSP + 0x3c]    ; the SECOND
1800b5fae  MOV dword ptr [RBP + -0x14],EAX   ; temp + 0x0c  == size
```

The temp is `[RBP-0x20]`, so **the on-disk order is offset first, size second.**

§87 was right that this is a coin flip no green run detects — so it was settled
the other way as well, empirically. With the two transposed the oracle fires on
**4 of 4 calls** and prints `entry 0 95/4 vs 4/95`. See §99.

**The `n` at 0x1800b5fbe:**

```
1800b5fb1  MOV EDX,dword ptr [R13 + 0x8]     ; entries._size
1800b5fb5  INC EDX                           ; n = _size + 1
1800b5fb7  LEA R8,[RBP + -0x20]              ; fill = &temp
1800b5fbe  CALL 0x1800298f0
```

`_size + 1`, as predicted — and R13 is `this + 0xb0` (set at 0x1800b5b86), an
independent re-confirmation of the `entries` / `entryCount` pair at +0xB0/+0xB8.

---

## 92. Four corrections to the recorded decode. Three change what gets written.

**(a) The `+0xAC` read is ONE BYTE, not four.** §75's table lists eight fields as
"4 each" and `+0xac` is among them. At 0x1800b4b4b the count register is
`MOV ESI,0x1`; at all eight other sites it is `MOV ESI,0x4`. `+0xAC` is
`overridable`, a `uint8_t` in `runtime/types/io/FileSystem.h`. A four-byte read
there consumes three bytes belonging to `crc` (`+0xA8`) and desynchronises every
field and every entry after it.

This is the transcription's most consequential correction, and it came from
reading the count register at each of the nine sites rather than trusting a table
that had collapsed them into one row.

**(b) The uncompressed ELSE arm writes SEVEN fields, not four.** §86 records
0x1800b5b25 as `+0x78 = 1; +0x7c = 1; SetSize(+0x80,1); [0] = 0xff000000`. It is
also:

```
+0x68 = 0x010007ff   (dword)
+0x70 = 0x2710       (ONE QWORD store: priority = 10000 AND platform = 0)
+0xa8 = 0            (crc)
+0xac = 1            (overridable)
```

Three of those four restate what the constructor already wrote and would have
been invisible if omitted. `+0x68` does not: the constructor leaves it zero and
`?GetVersion@` returns it.

**(c) The two string paths do NOT merge at the same address.** PATH A's three
exits `JMP 0x1800b5b6c`; PATH B's three `JMP 0x1800b5b73`, one instruction
later, skipping `MOV R15D,[RBP+0x18b0]`. PATH A's decode loop clobbers R15, so
the format version has to be reloaded before the entry loop's selector reads it;
on PATH B it is still live. Harmless in a C++ transcription — recorded rather
than smoothed over silently.

**(d) `[RBP+0x18a0]` is `this`'s incoming home slot,** not a second length temp
as §80's table reads. `MOV [RSP+0x8],RCX` at the function entry stores it there
(old RSP + 8 = RBP + 0x18a0 through the same frame equation), and PATH A groups 1
and 2 RELOAD `this` from it (0x1800b4fba, 0x1800b522a) after their decode loops,
because the loop burns R13 as a constant register. Group 3 then reuses the slot
as its own String temp (0x1800b54c2) — which is exactly why group 3 is last and
why nothing reloads `this` after it.

---

## 93. Two structural findings the decode did not have

**The two stack buffers OVERLAP, and the 0x800 cap IS the overlap point.**
`0x860 - 0x60 = 0x800`, and `0x800` is the cap the re-encode loop enforces
(`CMP ESI,0x800 ; JG`). They are one region: the decoded-output buffer runs from
its frame slot up to where the raw-read buffer begins, and the cap is what stops
the decoder writing over its own input. Modelling them as two independent arrays
compiles, runs, and silently changes what happens to any packfile that hits the
cap. The port uses one array with `buf860 = buf60 + 0x800`.

**The decoder `ReadPackfile` uses is encoding id 1, NOT 4.** Table entry 1
(0x1800c9ff1) is nine instructions — `*cp = p[0] | (p[1] << 8); return 2;`, with
`end` accepted and never read. Entry 4 (0x1800ca0e8) is a different,
surrogate-aware body, and entry 4 is the one `runtime/src/system/widestring.cpp`
reproduces. Reusing widestring's helper here because "it is the UTF-16 decoder"
would have been the fourth by-resemblance defect this lane has caught (after
§51's alignment, §74's `AdjustDelimiters` and 4c-ii part 1's `kSpec_StringExt`).

Both bodies this function uses — decode id 1 and encode id 2 (UTF-8) — are
transcribed from the `.rdata` jump tables at 0x1800ca1b8 and 0x1800c9d40 rather
than written from spec, which is a small improvement on the two existing copies
in the tree, both of which had to disclose that they were spec-written.

---

## 94. Ported

| Export | Retail | Ord | Size |
|---|---|---|---|
| `?ReadPackfile@PackageFile@IO@@QEAA_NAEBVFilename@System@@@Z` | 0x1800b46b0 | 3461 | 1,864 instructions |

Name and ordinal printed out of Runtime-x64.dll's own export directory with
`pefile`, not transcribed from a decompile. Both doors registered
(`RegisterRuntimeExport` + `RegisterRuntimeAddr`).

Kill switch **`BANISHED_RT_PKG3=0`**. Its own switch, because the body is
all-or-nothing and cannot be half-registered: a boot regression has to be
removable without disturbing PKG2's five rungs of callees underneath it.

`runtime/src/io/package_file_read_win.cpp` is `_win` for exactly one Win32 call:
the `SetFilePointer` at 0x1800b47e3, taken only on the loose-file branch. A
`_posix` twin needs that one seam.

The 28 inlined copies of `?Read@CompressedStream@IO@@` collapse to 28 calls on
the ported body, which is §68's substitution — proved there two ways, and used
here unchanged.

---

## 95. The oracle, and §76's exclusion made precise

`ReadPackfile` is not pure, but it IS replayable: everything it produces lands in
`*this`, and `this` can be a second scratch object. The oracle constructs a
scratch `PackageFile` with our own byte-proven constructor (§5) and runs the
SHIPPED body on it with the same filename, then compares the return value, the
nine scalar fields, `+0x90`, both arrays by SIZE **and by CONTENT**, the
`FileInfo`, the three metadata Strings by text, and **every entry's offset, size
and name**.

It then tears the scratch down by hand — three Strings, one String per entry,
three array blocks, the two `FileInfo` names, and the scratch's own `CreateFileW`
handle — because `~PackageFile` is unported (`package_file.cpp` §4) and a diff
run mounts every package in the install.

Exclusions, each with its justification:

* `winHandle` — two opens, two HANDLEs. Compared for validity, not by value.
* `_unk04` — never written by `?OpenFile@`; comparing memory neither side wrote
  is `reference_oracle_must_not_compare_unwritten_bytes.md`.
* the two `FileInfo` Filenames — different String objects, compared by text.

**And §76's, which is the one that matters.** §76 says retail decodes `2 * len`
bytes out of a buffer it filled with `len`, so ours and theirs read different
stale stack and a string may differ legitimately. "Compare only up to the
terminator" is not sufficient as stated: the divergence MOVES the terminator, so
it changes the whole String, not its tail.

So the port DECIDES it per string, from data both sides read identically — which
means the decision itself cannot diverge. A string is **deterministic** iff a
terminator appears inside the region actually read: a zero 16-bit unit fully
within `[0, len)` on the UTF-16 arm, a zero byte within `[0, len)` on the raw
arm. Deterministic strings are compared strictly; non-deterministic ones are
counted and excluded, and the count is printed whether it is zero or not.

**On this corpus the count is ZERO — `nondet=0` over 23,963 strings — so the
exclusion was never in force and the reported result carries no string
carve-out at all.**

---

## 96. The census is INSIDE the body

Every arm increments its own counter in the function, not a proxy observed
outside it. That is §65's correction, which the lane's effectiveness control got
wrong twice and reported PASS both times.

```
[PKG3] ReadPackfile "...\WinData\patch1.pkg" -> 1 | calls=4 early=0
       | compressed=4 plain=0 | v2block=4 | meta: utf16=0 raw=4
       | names: utf16=0 raw=23933 | entries=23933
       | strings=23963 nondet=0 | commit=0
```

(cumulative; one line per package. `BANISHED_PKG3_CENSUS=1`, or automatic under
the diff harness.)

---

## 97. Result

Run `final`, lockstep under `tools/game_dir_lock.sh` as `lane-PKG3`, logs copied
out before release. Identity gate OK — `21 / rung3 4 / readpackfile 1`, all three
counts derived from THIS build's source by `tools/pkg3_run.sh`, all three read
back out of the log the run itself wrote. Package set:
`data0.pkg data1.pkg patch0.pkg patch1.pkg`.

```
[RtDiff]   IO::PackageFile::ReadPackfile     calls=4   worked=4   divergences=0
```

Four packages, **23,933 entries**, every offset / size / name compared, plus all
metadata, both arrays' contents and the FileInfo. Read the `calls=` column: it is
4, not 0 — the site executed and the comparison ran.

**Re-measured from the committed tree.** Between the `final` run and the
commit, `git stash push/pop` (used to measure the census on both sides, §97)
touched the two source files' mtimes -- which is exactly the shape of
`reference_lane_edits_after_report_invalidate_the_measurement.md`, where a wave's
closing numbers were taken on an exe older than its sources. Rather than assert
the content was unchanged, the exe was REBUILT from the committed tree and the
run repeated: run `confirm`, identity gate OK, `calls=4 worked=4 divergences=0`,
identical census line. The reported numbers are from a build of what is committed.

The only divergence anywhere in that run is
`Object::Factory::Startup [probe, not diffable]`, which is not this lane's and
was already flagged in §5.

**Run `play`** (`BANISHED_AUTOSTART=1`, no diff harness, so the port runs alone):
boots, auto-drives menu → New Game → Start, builds and simulates a world (wild
animal herds forming and thinning, game settings persisted) and self-quits
through the pause menu. **Zero-byte crash log. `[RtResolve] 0 misses`.** That is
the end-to-end proof: every resource the world build reads comes out of those
four packages through the index this function builds.

### Accounting

`tools/runtime_census.py` measured on BOTH sides of the change (the lane's own
tree stashed and restored), not carried forward:

| | before | after |
|---|---|---|
| IO ported / called | 74 / 95 (77.9%) | **75 / 95 (78.9%)** |
| TOTAL ported / called | 740 / 998 (74.1%) | **741 / 998 (74.2%)** |
| `unsatisfied_export_scan.py` | 131 (128 outside a diff guard) | **130 (127)** |

* `tools/port_bypass_audit.py` — 0 bypasses on either door (address door 298
  sites, name door 469).
* `tools/abi_retslot_audit.py` — 0 suspicious of 48.
* `tools/export_name_audit.py` — the single name it flags is a doc-comment
  artifact in `runtime/funcs/rt_resolve_cache.cpp`, pre-existing, not this lane's.

### The five registered-but-uncalled addresses ALL CLOSED

On the run where this body first executed:

| address | what | calls |
|---|---|---|
| 0x180029510 | `Array<int32>::SetSize` (icon pixels) | 4 |
| 0x180029630 | `Array<bool>::SetSize` (`this+0x98`) | 4 |
| 0x180024d70 | `Array<CompressedStream::Header>::SetSize` | 8 |
| 0x180024c10 | `Array<CompressedStream::Window>::SetSize` | 14 |
| 0x1800298f0 | `Array<Entry>::SetSize(fill)` | 5,848 |

§84 named 0x180029630 as the diagnostic — unreached would have meant the
compressed flag was one-sided. It IS reached, so that particular inference does
not fire. **But the flag is one-sided anyway, in the other direction** (§98).
The diagnostic was sound; the corpus simply sits on the side that exercises it.

---

## 98. COVERAGE — read this BEFORE reading `divergences=0`

| conditional | arm taken | arm NOT taken |
|---|---|---|
| header sign bit — `compressed` (§72) | **set, 4 of 4** | **the uncompressed default at 0x1800b5b25 — UNTESTED** |
| format version, metadata strings (§81) | **`> 2` → PATH B raw, 4** | **`<= 2` → PATH A UTF-16 — UNTESTED** |
| format version, per-entry names (§86) | **`> 2` → raw, 23,933** | **`<= 2` → UTF-16 — UNTESTED** |
| `formatVersion >= 2` block (`+0x90`/`+0x98`) | **entered, 4** | `< 2` — UNTESTED |
| `_fileInfo.handle == -1` early-out | — | **UNTESTED** (every package opened) |
| `Commit` (§70) | — | **taken 0 times — §70's "provably dead" is now MEASURED, not argued** |

Every shipped `.pkg` came out of the same `Tools-x64.exe`: all four carry the
compressed sign bit and all four carry a format version greater than 2. §81
predicted exactly this for the version check and it is confirmed. §72's
compressed flag turns out to be one-sided too.

**The concrete consequence, stated plainly: the entire UTF-16 half of this
function has never executed.** `Decode1`, `Encode2`, the decode/re-encode loop,
§76's over-read, and both of the arms that use them (metadata PATH A and the
per-entry UTF-16 arm) are transcribed from disassembly and from the two
jump-table bodies and are **UNVERIFIED BY ANY RUN**. So are the uncompressed
default arm and the `handle == -1` early-out.

Closing them needs a version-1-or-2 or an uncompressed packfile, and no corpus on
this machine has one. `Tools-x64.exe` may be able to produce one; that is the
next lever, and it is a corpus problem, not a port problem.

`.pkm` mod packages were absent from `WinData` for these runs (see the `pkg set`
line in `rt_run_*_stdout.txt`), so `LoadPackages`' second loop was empty. That is
also a corpus fact — a `.pkm` takes the identical code path.

---

## 99. Two negative controls, and both fired

An oracle that has never failed is not known to work.

**Control 1 — the §91 coin flip itself.** `entry.offset` and `entry.size`
swapped, rebuilt, rerun through the same gate:

```
divergences=4 of calls=4
DIVERGENCE ... call #1  ours=entry 0 95/4 vs 4/95
```

So the field pair §87 refused to guess is settled twice over: by the four
instructions, and by a run that goes red the moment they are transposed. Had the
guess gone the other way, this is the run that would have caught it — which is
also the measure of what §87's refusal was worth.

**Control 2 — the text path.** `author` and `description` swapped in PATH B:

```
divergences=4 of calls=4
ours=metadata text: "game data"/"Main Game Data"/"Shining Rock Software, LLC"
```

against a shipped body whose name / author / description read `"game data"` /
`"Shining Rock Software, LLC"` / `"Main Game Data"`. The String comparison is
live, and the assignment order is confirmed independently of the disassembly.

Both controls were reverted and §97's measurement retaken on the reverted build,
with the identity gate passing on that build.

---

## 100. One defect this lane introduced, caught by an instrument

The first version of the registration read
`RegisterRuntimeExport(kReadPackfile, &ours)` — a named constant.
`tools/unsatisfied_export_scan.py` and `tools/runtime_census.py` both match
`RegisterRuntimeExport("` followed by a STRING LITERAL, so a registered,
running, oracle-verified port was invisible to both, and the scan duly reported
`?ReadPackfile@` as still unsatisfied on a build where it was live.

That is `reference_census_cannot_see_nonliteral_registrations.md` exactly — the
defect that cost six LANDED Wave 3 ports their visibility. The name is now spelt
out as a literal with the reason in a comment beside it, and `tools/pkg3_run.sh`
derives its expected count with an ANCHORED grep (`^ *RegisterRuntimeExport\(`)
so that comment is not itself counted as a second registration and does not fail
a good run.

Caught by an instrument, not by review. Third time this wave.

---

## 101. Next predicted fault site

`?ReadPackfile@` no longer stops `LoadPackages`. The loop completes, all four
packages mount, the path index builds, and a full new game builds and simulates
on top of it. **The lane's ladder is finished.**

What is still unported in this cluster and named in `package_file.cpp` §4 —
`??1PackageFile@` and its five unported callees, the deleting destructor,
`?Serialize@`, and the ToolKit write half — is a teardown and save-path concern.
None of it is on the path `LoadPackages` takes, so none of it is a boot blocker.

---

## Files

| file | what |
|---|---|
| `runtime/src/io/package_file_read_win.cpp` | the port, the codec pair, the census and the oracle (§91-§96 in its header) |
| `runtime/src/io/rt_io_port.cpp` | `BANISHED_RT_PKG3` switch and the registration call |
| `tools/pkg3_run.sh` | deploy + run + capture under the game lock, with the three-banner identity gate |
| `docs/runtime_port/wave9_lane_pkg3/` | `final_*` (§97), `play_*` (the gameplay run), `negctl*_error.log` (§99) |
