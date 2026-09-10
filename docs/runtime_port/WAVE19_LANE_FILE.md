# WAVE 19 — lane FILE: the FileStream family and the screenshot cutover hole

Worktree `C:/code/GIT/wt-w19-file`, branch `wave19/file`, cut from `wave19/base`
@ `20aa588`. Every figure below was **measured on this branch**, debug build
(`mingw32-make standalone-debug`), under
`tools/game_dir_lock.sh run lane-FILE … -- …` in its own statement.

Evidence: `docs/runtime_port/wave19_lane_file/` — `main_debug.log`,
`main_error.log` (the `BANISHED_RT_STRICT` residue census), `ctrl_debug.log`
(the kill-switch control).

---

## 0. Headline

**All 8 rows ported, registered by NAME and by ADDRESS, and all 8 have left the
unported set.** Three DEFERRED vtable rows were not started, as instructed.

**But the deliverable was the screenshot cutover hole, and it is NOT shut.**
It is *narrower by ten bodies* and the remainder is now enumerated rather than
unknown: **18 of 25 doors on that path are answered by our registry, 7 are
OPEN**, and none of the 7 is lane FILE's to close. §5 names each one.

**The brief's acceptance test could not be executed as written, and that is
reported rather than worked around.** "Re-run VRES's BMP oracle with the DLL
absent" asks for a *comparison* whose second side is the shipped body. With the
DLL absent there is no second side. §4 is what was run instead, in two halves,
and §4.3 is the uncomfortable part: **the BMP oracle passes IDENTICALLY with
this lane's ports switched off**, so its green attributes nothing to this lane.
The attendance counts do.

---

## 1. The rows

Retail addresses and ordinals printed out of Runtime-x64.dll's export directory
with `pefile`. **Every mangled name in both source files was run against that
directory before a body was written: 21 checked, 0 phantoms** (§6.1).

| # | symbol | retail | ord | ported | reg name | reg addr | left unported set | attendance (main run) |
|---|---|---|---|---|---|---|---|---|
| 1 | `?CreatePath@FileSystem@IO@@QEBA_NAEBVFilename@System@@@Z` | 0x1800ae430 | 2130 | yes | yes | yes | **yes** | **6** (30 dirs attempted) |
| 2 | `?Create@FileStream@IO@@IEAA_NAEBVFilename@System@@@Z` | 0x1800aa8f0 | 1876 | yes | yes | yes | **yes** | **3** (true=3) |
| 3 | `?CloseFile@FileSystem@IO@@QEAAXAEAUFileInfo@12@@Z` | 0x1800accc0 | 1726 | yes | yes | yes | **yes** | **2854** |
| 4 | `??0BinaryFileStream@IO@@QEAA@XZ` | 0x1800a8200 | 55 | yes | yes | yes | **yes** | **2850** |
| 5 | `??1BinaryFileStream@IO@@UEAA@XZ` | 0x180023c80 | 532 | yes | yes | yes | **yes** | **3** |
| 6 | `??1FileStream@IO@@UEAA@XZ` | 0x1800aaab0 | 593 | yes | yes | yes | **yes** | **2850** |
| 7 | `??0TextFileStream@IO@@QEAA@AEBVString@System@@0H@Z` | 0x1800ba340 | 441 | yes | yes | yes | **yes** | **0** — see §4.4 |
| 8 | `??1FileSystem@IO@@QEAA@XZ` | 0x1800ac6d0 | 594 | yes | yes | yes | **yes** | **0** — see §4.4 |

**DEFERRED, not started, exactly as instructed:** `??_7FileStream@IO@@6B@`,
`??_7BinaryFileStream@IO@@6B@`, `??_7TextFileStream@IO@@6B@`. All three are
lane MINT's. They are also three of the seven OPEN doors in §5 — this lane's
port *depends* on them and cannot close them.

### 1.1 Four more bodies, NOT on the row list, without which the hole moves one link and stays open

Walking the call graph out of rows 2 and 3 found three unported bodies between
them and Win32, plus one unexported destructor under row 6:

| symbol | retail | ord | why it had to be in this lane |
|---|---|---|---|
| `?CreateFileW@FileSystem@IO@@QEAA?AUFileInfo@12@AEBVFilename@System@@@Z` | 0x1800acc50 | — | `?Create@FileStream@` calls it |
| `?PlatformCreateFile@FileSystem@IO@@AEAA_NAEAUFileInfo@12@AEBVFilename@System@@@Z` | 0x1800ade60 | 3357 | `?CreateFileW@` calls it; it is where CreateFileW is actually issued |
| `?PlatformCloseFile@FileSystem@IO@@AEAAXAEAUFileInfo@12@@Z` | 0x1800ae020 | — | `?CloseFile@` calls it; it is where the file is renamed into place |
| `~FileInfo` (unexported) | 0x18000c180 | — | `??1FileStream@` calls it |

All four ported and registered. Porting only the eight named rows would have
left `ResolveExport` falling through to the DLL at each of these, and a
DLL-absent screenshot would have died one link further down than before.

**Two of the three are `AEAA` — PRIVATE, not unexported.** `?PlatformCreateFile@`
is ordinal 3357.

---

## 2. The identity, and a set-diff of `called` — never a percentage delta

`tools/runtime_census.py --namespace IO`, run **in this worktree** (the scanner
walks `os.walk("runtime")` relative to the current directory).

```
baseline  ebbec6e   IO   122 called   83 ported   3 unpromotable
                         83 >= 122 - 3 = 119                    36 SHORT

this tip            IO   123 called   91 ported   3 unpromotable
                         91 >= 123 - 3 = 120                    29 SHORT
```

**`called` grew by 1. The set-diff, which is the only honest way to report it:**

* **ENTERED `called`: exactly one —**
  `?SetObject@TextFileStream@IO@@QEAAXAEBVString@System@@0H@Z`.
  Row 7's constructor tail-calls it (0x1800ba45e) and this lane reaches it
  through `RT_EXPORT_ONCE`. **Deferred work surfaced, not invented work**: the
  TextFileStream constructor needs it whoever writes it.
* **LEFT the unported set: exactly the 8 rows in §1.** Nothing else.

Two independent routes agree on the +1: the unported set gained exactly
`?SetObject@`, and `called` moved by exactly 1. If a second symbol had entered
`called` *and* been ported in the same change it would be invisible in the first
route; the arithmetic (`ported +8` from the 8 rows, `called +1`) rules that out.

**Reconciliation on the extraction, because it did not match at first.** The
census header says `Called but not ported (32)`; my symbol regex parsed **29**.
The 3 it missed are the `?_singleton@{FileSystem,Modification,Persistence}` rows,
which print with an `UNPROMOTABLE` tag in a different line shape. `29 + 3 = 32`,
the screen's own total, so the extraction covers **32 of 32 rows, 0 unaudited**.
(Also: piping the census through `grep` appends `Binary file (standard input)
matches` — the trap WAVE19_IO.md Appendix A warns about. `grep -a` avoids it.)

### 2.1 THE CENSUS UNDERSTATES THIS LANE BY FOUR BODIES, and here is why

The four bodies in §1.1 are ported and registered, and **none of them appears in
either census column.** They are in the census's *"Ports with no caller in our
tree"* section instead. `called = distinct functions our tree RESOLVES today`,
and our own code calls these four DIRECTLY rather than through a door — which is
correct (a door on an already-ported symbol is only a way to reach the shipped
copy by accident) and which makes them invisible to a door-scanning census.

That is `reference_half_registered_port_reads_as_absent.md` inverted: there, a
ported-but-unregistered body read as absent; here, a ported-AND-registered body
reads as absent because nothing resolves it. **12 bodies landed; the census can
see 8.**

---

## 3. What the disassembly settled that a signature or a decompile summary gets wrong

Read as disassembly, never as Ghidra's C. Recorded in full in the two source
file headers; the four that changed the port are:

1. **A write goes to `%TEMP%` first and is RENAMED into place on close.**
   `?PlatformCreateFile@` calls `GetTempPathW` / `GetTempFileNameW` and opens
   *that*, storing the temp path in `FileInfo::_name0` (+0x20) and the caller's
   path in `_name1` (+0x28). `?PlatformCloseFile@` then `DeleteFileW`s the
   destination and `MoveFileW`s the temp over it. **Nothing appears at the
   caller's path until CLOSE** — which is why row 6 (`~FileStream`, the thing
   that closes it) and rows 1–3 had to be one lane.
   * Which field is which is settled by `MoveFileW`'s argument order at
     0x1800ae0d7/0da, not guessed: two same-typed adjacent `String` fields are a
     coin flip, and `FileStream.h` records losing exactly that coin flip once on
     TextFileStream's `objectName`/`objectType` pair.
   * The rename is skipped when `_name0` has **zero LENGTH**, not when it is
     null: 0x1800ae065 reads `[RAX-0x8]`, the block's length, not the refcount
     at -4.
2. **`?CreateFileW@FileSystem@` returns a `FileInfo` BY VALUE** — hidden return
   slot in RDX, `Filename*` in R8, three arguments in the typedef, not two.
   Omitting it writes 0x30 bytes of the caller's frame and still returns the
   right answer (`reference_hidden_retslot_wild_write.md`).
3. **`?PlatformCreateFile@`'s failure test catches TWO handle values, not one.**
   `DEC RAX ; CMP RAX,-0x3 ; JA` is unsigned and fails on `INVALID_HANDLE_VALUE`
   **or NULL**. A port testing only the former treats a null handle as success.
4. **`?CreatePath@` returns `true` unconditionally** (`MOV AL,0x1` on the only
   exit, 0x1800ae637); every `CreateDirectoryW` result is discarded. Propagating
   the Win32 status would diverge on every path that already exists.

**No layout was re-derived.** `runtime/types/io/FileStream.h` has pinned this
family since Wave 11 lane IOSTREAM and `FileSystem.h` pins `FileInfo` and
`FileSystem`. Zero hours went into layout work, exactly as WAVE19_IO.md §2
predicted.

---

## 4. Verification

### 4.1 Why the brief's test was not run as written

The brief: *"re-run Wave 18 lane VRES's BMP oracle WITH THE DLL ABSENT. That is
the only test that proves the hole is shut."*

VRES's cases 9–11 are a **comparison**: our `SaveImageToFile` writes one BMP, the
shipped body at 0x1800f2840 writes another from the same synthetic pixels, and
the two FILES are byte-compared. With the DLL absent the shipped body does not
exist, so the comparison has no second side. `standalone/resolve.h` says the same
thing about the residue census in its own words: *"Strict mode observes; it does
not simulate the DLL being gone. Deleting the DLL is what does that."* — and a
full DLL-absent boot is a whole-tree operation that currently dies at
`?PlatformConfigure@Engine@1@`, which is not this lane's wall to move.

So the single test was split into the two questions it was standing in for, and
both were run.

### 4.2 Half A — is our body right? VRES's oracle, DLL present, through our ports

```
[VresProbe] mode=none  cases=11 mismatches=0 | work: freed=12 refcount--=4
            reached0=2 survived=2 bmp-bytes-compared=238
[VresProbe] VERDICT: ours matches the shipped bodies (positive run mismatches=0)
```

`cases=11 mismatches=0`, **238 of 238 BMP bytes compared** across the three
padding shapes (5×3 one pad byte per row, 4×2 none, 1×1 single row) — the same
figures Wave 18 recorded, now with all six of its doors resolving to our bodies
instead of the DLL. Its effectiveness counters are non-zero (`freed=12`,
`refcount--=4`, `reached0=2`), so the run is not the "proves nothing" shape its
own guard prints. `RC=0`, **0-byte crash log**.

### 4.3 …AND THAT GREEN ATTRIBUTES NOTHING TO THIS LANE. Read §4.4 with it.

The kill-switch control (`BANISHED_RT_FILESTREAM=0 BANISHED_RT_CREATEFILE=0`)
produces a **byte-identical oracle line**:

| | main | control (both switches 0) |
|---|---|---|
| `[VresProbe]` | `cases=11 mismatches=0 … bmp-bytes-compared=238` | `cases=11 mismatches=0 … bmp-bytes-compared=238` |

**The BMP oracle is INSENSITIVE to this lane's ports.** That is expected — with
the switches off, our `SaveImageToFile` reaches the DLL's IO through the same
doors and writes the same file — but it means the green in §4.2 cannot on its
own distinguish "our IO ports work" from "the DLL's IO works". Quoted alone it
would be a green board over an untested subject. §4.4 is what makes it
attributable.

### 4.4 Half B — did our bodies run, and is anything still asking the DLL?

**Attendance, main run** (`[FileCutover] attendance`, counters in the ported
bodies themselves, printed unconditionally):

```
??0BinaryFileStream=2850  ??1BinaryFileStream=3  ??1FileStream=2850
??0TextFileStream=0  ?Create@FileStream=3 (true=3)  ~FileInfo=2850
?CreatePath=6 (dirs=30)  ?CreateFileW=3  ?PlatformCreateFile=3 (ok=3 fail=0)
?CloseFile=2854  ?PlatformCloseFile=2854 (renamed=3)  ??1FileSystem=0
```

* **The write path ran exactly 3 times** — `?Create@FileStream=3 (true=3)`,
  `?CreateFileW=3`, `?PlatformCreateFile=3 (ok=3)`, `renamed=3` — matching the
  oracle's three SaveImage cases one for one. **The BMPs of §4.2 were written
  THROUGH these bodies.** That is the attribution §4.3 says the oracle lacks.
* **The read path ran 2850 times.** `??0BinaryFileStream`, `??1FileStream` and
  `~FileInfo` at 2850 and `?CloseFile`/`?PlatformCloseFile` at 2854: every
  resource the game loads on the autostart route now goes through these bodies,
  and the run boots, builds a world and quits with a **0-byte crash log**.
* **`??0TextFileStream=0` and `??1FileSystem=0` are honest zeros**, reported as
  such rather than folded into a total. Rows 7 and 8 are UNATTENDED on this
  route (`Object::External::SaveText` is the only TextFileStream caller and no
  autostart route saves text; `~FileSystem` runs only on a full engine
  shutdown this route does not reach). **They are ported and registered, not
  verified.** See §7.
* `??1BinaryFileStream=3` against `??1FileStream=2850` is not a discrepancy: only
  the three screenshot streams are destroyed through the subclass thunk; the
  other 2847 reach `~FileStream` from `CompressedFileStream`'s inlined chain.

**Effectiveness, not just calls.** `?CreatePath=6 (dirs=30)` and
`?PlatformCloseFile=2854 (renamed=3)` are branch counters: 30 directory
creations were actually attempted and the rename arm ran 3 times. `calls=N
worked=0` — the body ran every time and always took the arm nothing covers — is
the shape these exist to make visible
(`reference_calls_zero_can_mean_always_took_the_uncovered_branch.md`).

### 4.5 The door screen and its two controls

`runtime/src/io/file_stream_cutover_probe.cpp`, armed with
`BANISHED_RT_FILE_CUTOVER=1`, asks the port registry directly
(`RtPortLookupExport` / `RtPortLookupAddr` — the residue-free queries) about
**25 hand-enumerated doors** on the screenshot path.

```
CONTROL positive (must read PORTED)      ?OpenFile@FileSystem@IO@@…  : PORTED     -> GREEN
CONTROL negative (must read NOT-PORTED)  ?Commit@CompressedStream@IO@@…: not ported -> GREEN
name doors 15 of 21 answered by our registry, 6 OPEN
addr doors 18 of 25 answered by our registry, 7 OPEN
==== 18 of 25 rows SHUT, 7 OPEN ====
```

Both controls are scored on their **own applicable case, 1 of 1 each**, never on
a union. The positive control (`?OpenFile@`, registered since Wave 9 lane PKG2)
catches a lookup stuck at "no"; the negative control (`?Commit@
CompressedStream@` — WAVE19_IO.md §2's worked example of a door that *looks*
ported and is not) catches one stuck at "yes". With only one of them the screen
could not tell "nothing is ported" from "the lookup always says no". **Both, or
no verdict**: the screen prints `MEASURED NOTHING` and offers no verdict if
either fails or the list is empty.

### 4.6 The kill-switch control, scored on applicable cases only

| | main | control | moved |
|---|---|---|---|
| rows SHUT | **18** of 25 | **8** of 25 | **10** |
| name doors answered | 15 of 21 | 6 of 21 | 9 |
| addr doors answered | 18 of 25 | 8 of 25 | 10 |
| every attendance counter | 2850 / 2854 / 3 / 6 / … | **all 0** | — |
| `[VresProbe]` | `11 / 0 / 238` | `11 / 0 / 238` | **nothing** |

**10 of 10 applicable rows moved, GREEN**: this lane owns exactly 10 code rows
(the 8 named + `?CreateFileW@`, `?PlatformCreateFile@`, `?PlatformCloseFile@`,
`~FileInfo` — 12 bodies, of which 9 have exported names and 10 have addresses).
The 15 inapplicable rows did not move; that is a **specificity** result and is
scored separately, not folded into the control's denominator. The zero-attendance
line printed too, verbatim: `*** NO BODY IN THIS LANE WAS ENTERED IN THIS RUN.***`

### 4.7 An independent second route: the `BANISHED_RT_STRICT` residue census

The main run also recorded 166 `[RtResidue]` / 48 `[RtRawResidue]` lines — every
request that reached the DLL. Of the symbols on the screenshot path, exactly
these appear, and they are **the same set the door screen called OPEN**:

```
[RtResidue] 1  ??_7BinaryFileStream@IO@@6B@      [RtResidue] 16 ?MemAlloc@MemoryAllocator@System@@…
[RtResidue] 2  ??_7FileStream@IO@@6B@            [RtResidue] 16 ?MemFree@MemoryAllocator@System@@…
[RtResidue] 5  ??_7Stream@IO@@6B@                [RtResidue] 13 ?_singleton@FileSystem@IO@@…
```

A static source-side screen and a live runtime census, built from opposite ends,
naming the same set. §6.2 records the one place they *disagreed*, and which one
was wrong.

---

## 5. The 7 OPEN doors — the remainder, named, and none of it lane FILE's

| door | why it is open | whose |
|---|---|---|
| `??_7FileStream@IO@@6B@` (0x180154960) | not minted — WAVE19_IO §0b's harvest gap | **lane MINT** (DEFERRED to it by my brief) |
| `??_7BinaryFileStream@IO@@6B@` (0x1801554d0) | same | **lane MINT** |
| `??_7Stream@IO@@6B@` (0x1801549a0) | **minted and READY, but not PROMOTED** — see below | mint/promotion |
| 0x1801554a8, the shared Encoding vtable | unexported, minted, not promoted | mint/promotion |
| `?_singleton@FileSystem@IO@@0PEAV12@EA` | minted storage, not promoted | mint/promotion |
| `?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z` | the allocator port is behind `BANISHED_RT_ALLOC`, off in a default run | allocator lane |
| `?MemFree@MemoryAllocator@System@@SAXPEAX@Z` | same | allocator lane |

**`??_7Stream@` is the one worth pausing on, because a readiness screen says the
opposite.** Phase 0c reports it `slots=7 ported=1 TRAP=1 purecall=5`, and this
run's own readiness table says `READY … TRAP=0`. It is still an OPEN door,
because only **5 of 62 minted VTABLES** are registered on a default run [CORRECTED — see §T11] — READY is
not PROMOTED, and only a promoted row is registered.
`reference_a_green_readiness_screen_is_not_a_route.md` is exactly this, and it
would have been easy to read the READY line as a shut door.

`??_7TextFileStream@` (0x180155980) and 0x1801559d0 are on row 7's path, not the
screenshot's, and are open for the same reason.

---

## 6. Two defects this lane found in its own work, by running something

### 6.1 A phantom vtable name, caught before it shipped

The first draft stamped the text Encoding through `??_7TextEncoding@IO@@6B@` — a
name this lane **invented**. 0x1801559d0 has **no export at all**, and a
`ResolveExport` name the DLL does not contain returns NULL silently
(`reference_resolveexport_phantom_names.md`). It was caught by running every
mangled name in both files against the shipped export directory with `pefile`
*before* writing the bodies: **21 checked, 0 phantoms** after the fix. Both
unexported tables now go in by ADDRESS, which
`runtime/src/io/compressedstream.cpp:524` had already concluded about 0x1801554a8.

### 6.2 The door screen named the wrong allocator symbol, and the residue census corrected it

The door list originally carried `?Allocate@MemoryAllocator@System@@QEAAPEAX_KH@Z`
(0x1800c6e60) — the member retail *inlines* inside `??0BinaryFileStream`. Our
port calls `RtSystem::RtMemAlloc`, which resolves the **static** locking pair
`?MemAlloc@` / `?MemFree@` (`allocator.cpp:684` records that the static IS
`lockEnter + Allocate + leave`, so the transcription is faithful — but the DOOR
is a different symbol from the one in the disassembly).

`BANISHED_RT_STRICT` named `?MemAlloc@` and not `?Allocate@`. Two instruments
looking at the same path from opposite ends, and **the live one corrected the
static one's subject**. Fixed and re-measured; the numbers in §4.5 are from
after the fix.

### 6.3 …and one in the harness: a control that switched off its own reporter

The screen was originally armed from inside `RtIoFileStreamRegister()`. The
obvious control — re-run with both kill switches at 0 — therefore printed
**nothing at all**, because the switch removed the reporter along with its
subject, and a run with no `[FileCutover]` block reads like a clean run. The arm
now lives in `RtIoPortInit()`, outside both switches. **Found by running the
control, not by inspecting it.**

### 6.4 And one in the measurement procedure, which invalidated a whole run

The first measured run launched `reimpl-x64.exe` directly from bash. It
**detached and returned immediately**, so `game_dir_lock.sh run` released the
lock while the run was in flight; another lane deployed over the exe inside that
window. **The measurement was taken against a binary 16,599 bytes different from
this lane's build and was discarded.**

It was caught by the missing `[RtPort] … lane FILE` registration line and by
comparing sizes — *nothing in the run itself announced it*. `tools/rt_run.sh`
already existed and already solves this (it refuses to launch over a live
process and waits for exit); it was not grepped for first
(`reference_grep_the_harness_before_declaring_no_route.md`, in its harness form).
`tools/lane_file_run.sh` now wraps it and **md5-compares the deployed exe against
the build before AND after every measured run**, refusing to report if either
differs. Every number in this document comes from a run that printed
`exe md5 unchanged across the run -- measurement stands`.

---

## 7. What this does NOT claim

* **NOT that the screenshot works with the DLL absent.** No DLL-absent run was
  performed; §4.1 says why one could not be, and 7 doors on that path are still
  OPEN (§5). The claim is narrower and exact: *every door on this path that
  lane FILE could close is closed, and the remainder is enumerated.*
* **NOT that the BMP oracle validates this lane.** §4.3: it passes identically
  with both switches off. Only the attendance counts make its green
  attributable, and they must be quoted together.
* **NOT that rows 7 and 8 are verified.** `??0TextFileStream@` and
  `??1FileSystem@` read **attendance 0** on the only route driven. They are
  transcribed, compile-checked, registered, and **never executed**. `calls=0`
  here means *no caller on this route*, which is the benign one of its four
  causes — but it is still an unverified body, and "no route driven" is not "no
  route exists": `Object::External::SaveText` drives row 7 and a save-text
  harness would reach it. **This lane did not look for one.** Recorded as
  PORTED, NOT VERIFIED.
* **NOT that the door list is complete.** It is HAND-BUILT from
  `save_image_to_file.cpp`'s `RT_EXPORT_ONCE` sites and the disassembly of each
  callee. It cannot discover a door nobody wrote down. §4.7's residue census is
  the independent check on that and found no path symbol the list lacked — but
  the residue can only name what the run reached.
* **NOT a claim about `~FileSystem`'s element destructor.** `~FileSystem` calls
  a POD array free (0x1800097f0) that frees the block and does **not** release
  the `PackageFile*` elements — `ClosePackages` owns those. It is transcribed as
  a file-local helper and **deliberately not registered**, because it is a
  Collection row, not IO's. Nothing else in the tree reaches that address
  (grepped), so no door is left open by it — but it is also unexercised
  (`??1FileSystem=0`).
* **NOT a percentage delta.** §2 gives the identity and a set-diff. `called` sits
  in both halves of the ceiling fraction and moves for reasons unrelated to
  progress.
* **Attendance figures are from ONE route** (`BANISHED_AUTOSTART=1
  BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120`) under **default flags**.
  Quote them as that, never as "the tree's".
* **NOT any statement about the three DEFERRED vtable rows.** Not started.

---

## 8. Handover

* **Lane MINT**: `??_7FileStream@`, `??_7BinaryFileStream@`, `??_7TextFileStream@`
  are three of the seven doors keeping the screenshot path open. This lane's
  bodies stamp all three by NAME and will get a null vptr at cutover without
  them. 0x1801559d0 (the text Encoding table) is unexported and unminted and is
  a fourth.
* **Whoever owns promotion**: `??_7Stream@IO@@6B@` is READY with TRAP=0 and is
  *not promoted*, so it is an open door. Only 5 of 62 minted vtables are
  promoted [CORRECTED — see §T11].
* **Lane PERSIST / whoever next**: `?SetObject@TextFileStream@IO@@` is newly in
  `called` and unported. It is row 7's only remaining door.
* **The integrator**: the census cannot see 4 of this lane's 12 bodies (§2.1).
  A count of rows closed will understate it; the set-diff will not.
* **Every lane**: `tools/lane_file_run.sh` generalises trivially. §6.4 cost this
  lane one whole run.

**Two commits: `45d2baa` (the ports), `4025da5` (the instruments and evidence).**

---
---

# POST-REPORT TRANCHE — the two FileStream DATA rows

**Added 2026-09-10, after §0–§8 were reported and sealed. Nothing above this
line is altered, reworded or renumbered.** Deleting everything from this rule
down reconstructs the report as delivered, which is the audit property that
distinguishes a marked addendum from a silent rewrite (WAVE19_IO.md §7, §6.16).

Re-tasked by the integrator once phase 0b regenerated the mint: the three DATA
rows this lane had been told to DEFER were un-deferred. This lane took **two**
and handed back the third. Branch merged `wave19/base` @ **`ee3d557`** (note the
tip had moved past the `e06c162` the re-tasking named).

## T0. Headline

**Both rows cleared: `??_7FileStream@IO@@6B@` and `??_7BinaryFileStream@IO@@6B@`
go from `0 ported / 8 TRAP` to `8 ported / TRAP=0`, READY** — measured by
`BANISHED_RT_MINT_SHADOW`, with a kill-switch control that puts them back.

**Ten bodies, all VERIFIED against the shipped ones**, `cases=2 mismatches=0
comparisons=90 file-bytes-compared=11`, both negative controls fired and caught.

**And on the first measured run all ten read attendance ZERO while the readiness
screen said READY.** That is not a contradiction, it is §T3 — and it is why
these bodies needed an oracle rather than an attendance count.

## T1. What landed

| body | retail | ord | door | file |
|---|---|---|---|---|
| `?GetSource@FileStream@IO@@UEBA?AVString@System@@XZ` | 0x1800aab50 | 2920 | name+addr | `file_stream.cpp` |
| `?Write@FileStream@IO@@UEAA_NHPEBX@Z` | 0x1800aab90 | 4549 | name+addr | `file_stream_win.cpp` |
| `?Read@FileStream@IO@@UEAA_NHPEAX@Z` | 0x1800aabf0 | 3450 | name+addr | `file_stream.cpp` |
| `?SetCurrentPos@FileStream@IO@@UEAA_NH@Z` | 0x1800aac10 | 3856 | name+addr | `file_stream_win.cpp` |
| `?GetCurrentPos@FileStream@IO@@UEBAIXZ` | 0x1800aac50 | 2534 | name+addr | `file_stream.cpp` |
| `?GetSize@FileStream@IO@@UEBAIXZ` | 0x1800aac60 | 2904 | name+addr | `file_stream.cpp` |
| `?Open@FileStream@IO@@UEAA_NAEBVFilename@System@@@Z` | 0x1800aac70 | 3304 | name+addr | `file_stream.cpp` |
| FileStream deleting-dtor thunk | 0x18000c090 | — | **addr only** | `file_stream.cpp` |
| BinaryFileStream deleting-dtor thunk | 0x180023d80 | — | **addr only** | `file_stream.cpp` |
| `??0BinaryFileStream@IO@@QEAA@AEBV01@@Z` (copy ctor) | 0x180023ca0 | — | name+addr | `file_stream.cpp` |

Kill switch `BANISHED_RT_FSSLOTS`. Oracle `BANISHED_RT_FSSLOTS_PROBE=1`
(`neg1`, `neg2` for controls). Commits `5cb83ea`, `2ab06ae`, and the oracle.

**Sixteen trap slots are nine functions.** `??_7FileStream@` and
`??_7BinaryFileStream@` have BYTE-IDENTICAL slots 1..7 — re-derived by reading
both tables out of the image, not inherited:

```
0x1801554d0: 180023d80 1800aab50 1800aab90 1800aabf0 1800aac10 1800aac50 1800aac60 1800aac70
0x180154960: 18000c090 <the same seven>
```

The two rows differ ONLY in slot 0. Scope a DATA row by FUNCTION, not by slot.

## T2. The grep the integrator assigned — answered

MINT reported the eight base slots UNREGISTERED and was explicit that
**unregistered is not unported**. Checked two ways, because an address grep
cannot see a name-only registration: by ADDRESS (all nine), and by EXACT MANGLED
NAME (all seven exported, taken from the shipped export directory).

**Genuinely UNPORTED, not merely unregistered.** Every non-mint-table hit is a
comment in `runtime/types/io/FileStream.h`, plus `file_stream_probe.cpp`, which
resolves them through `RtResolveExportForProbe` — raw doors, deliberately.

**Byproduct worth more than the answer**: `file_stream_probe.cpp` already exists,
drives these classes, and carries five negative controls. **Its limit, stated so
it is not mistaken for an oracle for this tranche: both its sides go through raw
doors, so it compares retail with itself.** It needs an ours-side.

## T3. THE CENTRAL RESULT: READY, TRAP=0, AND ATTENDANCE ZERO

The first measured run after the tranche:

```
[RtMintShadow]   READY  ??_7FileStream@IO@@6B@        slots=8 ported=8 TRAP=0 purecall=0
[RtMintShadow]   READY  ??_7BinaryFileStream@IO@@6B@  slots=8 ported=8 TRAP=0 purecall=0
[FileCutover] slots  GetSource=0  Write=0  Read=0  SetCurrentPos=0  GetCurrentPos=0
                     GetSize=0  Open=0  deletingDtor=0  copyCtor=0
```

**Ten registered bodies at TRAP=0 and not one of them entered.** The cause is
measured, not guessed, and this lane's own cutover board prints it two lines
above: the name doors for both tables read **NO**. The tables are MINTED but
**NOT PROMOTED** (only 5 of 62 minted vtables are, [CORRECTED — see §T11]), so
`RT_DATA_SLOT("??_7FileStream@IO@@6B@")` inside our own constructors falls
through to the DLL, our objects get RETAIL's table, and retail's slot bodies run.

**A green readiness screen is not a route**
(`reference_a_green_readiness_screen_is_not_a_route.md`), and this is as clean an
instance as the project has produced: the screen's READY verdict and the
attendance counters are both correct and they answer different questions.
READY means *safe to stamp*. Attendance means *something dispatched it*.

Promotion is not this lane's gate to open. So the bodies were verified directly
instead (§T4) — which is the same disposition as "no route driven is not no
route exists", one step further: **no route AVAILABLE is still not no oracle.**

## T4. The oracle — `runtime/src/io/file_stream_slots_probe.cpp`

Two `BinaryFileStream`s, **both constructed and `Create`d through RETAIL's
bodies** so the constructor is not a second variable, then driven through an
identical operation sequence — one side entirely through OUR ten bodies, one
entirely through the shipped ones via `ResolveRuntimeRaw`. Every return value
compared pairwise, and **the two files byte-compared at the end**, the same
artefact trick Wave 18 used for the BMPs.

Neither side uses `ResolveExport`, which is registry-first and would hand both
sides ours — a probe whose ours-side reaches the body by address compares retail
with itself.

**Result, with its denominators:**

```
[FSSlots] cases=2 mismatches=0 comparisons=90 file-bytes-compared=11 controlFired=0
[FSSlots] VERDICT: ours matches the shipped bodies on every comparison above.
[FileCutover] slots  GetSource=1 (nonNull=1)  Write=2 (full=2)  Read=1
                     SetCurrentPos=1 (win32=1)  GetCurrentPos=3  GetSize=1
                     Open=1 (true=1)  deletingDtor=2  arrayArmAbort=0  copyCtor=1
```

**Every one of the ten bodies is now non-zero** except the array arm, which is a
declared abort and is deliberately not driven. The 11 file bytes are the whole
artefact: `Write("hello world")`, `SetCurrentPos(6)`, `Write("THERE")` →
`hello THERE`, 11 bytes, read back through `Open` + `Read` and compared.

**Controls, each scored on its own applicable case, 1 of 1, never on a union:**

| control | what it perturbs | applicable case | result |
|---|---|---|---|
| `neg1` | seeks to 7 instead of 6 on OUR side only | case 1 | **CAUGHT**, 4 mismatches |
| `neg2` | drops the copy ctor's third vtable store | case 2 | **CAUGHT**, 1 mismatch |

Both produce a COMPARABLE DIVERGENCE — a different file, a different byte —
never undefined behaviour, and both `controlFired=1` printed before any verdict
was trusted.

### T4.1 The probe was wrong twice before it was right, both caught by its own guards

1. **`GetSource` compared two values that DIFFER BY CONSTRUCTION.** Subject A's
   filename is `…fsslots_ours.bin` and B's is `…fsslots_theirs.bin`, so a value
   comparison reported a mismatch on a correct port every run. **The evidence
   pointed at the probe because the "defect" was exactly the difference the
   subject was built to have** — the same shape as Wave 18 case 9. It now
   compares the BEHAVIOUR (returns this stream's own buffer, AddRef'd not
   copied) plus an ABSOLUTE assertion, because two sides that were both wrong
   the same way would agree.
2. **The effectiveness guard printed before the control verdict**, so a control
   that was caught *so decisively that the two files ended up different sizes*
   printed `THIS RUN PROVES NOTHING` and nothing else — which reads as a FAILED
   control when it is the opposite. Reordered; the qualifier now follows the
   verdict instead of replacing it.

## T5. Two briefed stamper counts were wrong, and one changes a verdict

The promotion criterion supplied with the re-tasking is right and is the
sharpest tool in this wave: **every stamp of these tables is an in-DLL
RIP-relative LEA, not a door, so a row is promotable exactly insofar as we own
the function that stamps it.** The two counts attached to it are not.
`get_xrefs_to` is cheap and neither had been run.

### `??_7BinaryFileStream@` — briefed ONE stamper, measured TWO

`get_xrefs_to 0x1801554d0` returns exactly two code sites: `0x1800a8270`
(`??0BinaryFileStream@IO@@QEAA@XZ`, ported in the sealed report) and
**`0x180023cfe`, inside `??0BinaryFileStream@IO@@QEAA@AEBV01@@Z` (0x180023ca0),
the COPY CONSTRUCTOR** — unported, and not listed as called by the census.

**Acting on "port that ctor and you own every stamp" would have declared the row
fully owned while a second stamper sat in the DLL putting retail's table into
every copied object.** The copy constructor is now ported, registered and
oracle-verified (control `neg2` targets it), so the claim is true — because it
was checked, not because it was told.

### `??_7FileStream@` — briefed NINE stampers, measured about SEVENTEEN

Distinct containing functions at `get_xrefs_to 0x180154960` include
`??0FileStream`, `~FileStream`, both `??0TextFileStream` overloads, the
BinaryFileStream default and copy ctors, `SaveText`, `SaveImageToFile`,
`TextFileStream::{Close,Create,Open}`, **`Engine::Run` at two sites**, and three
unexported bodies (`FUN_1800b6aa0`, `FUN_1800b6e00`, `FUN_1800b65b0`). **Five are
already ours.**

The CONCLUSION survives — Engine::Run and the three unexported bodies are not
ours, so **TRAP=0 on this row is NOT a promotion** — but the number it rested on
does not. Recorded because a measured number sitting beside an asserted one
lends the asserted one credibility.

## T6. The kill-switch control, and an aggregate that agrees from outside

| | main | control (`BANISHED_RT_FSSLOTS=0`) |
|---|---|---|
| `??_7FileStream@` | READY 8 / 8 / TRAP=0 | **BLOCKED 8 / 0 / TRAP=8** |
| `??_7BinaryFileStream@` | READY 8 / 8 / TRAP=0 | **BLOCKED 8 / 0 / TRAP=8** |
| `??_7TextFileStream@` | BLOCKED 20 / 2 / TRAP=18 | BLOCKED 20 / 0 / TRAP=20 |
| mint aggregate | 598 slots, **275 ported**, 175 unported-trap | 598 slots, **257 ported**, 193 unported-trap |
| every slot counter | non-zero (§T4) | **all 0** |

**The aggregate reconciles exactly against the row table, from a number that was
not used to build it:** `275 − 257 = 18` and `193 − 175 = 18`. My two rows are
16 of those; the other 2 are `??_7TextFileStream@`'s slots 1 and 7, which are
FileStream's `GetSource` and `Open` — the same shared bodies, showing up in a
third row. **8 + 8 + 2 = 18.**

Scored on applicable cases only: **2 of 2 rows moved, GREEN**, by the mechanism
the switch names. The other 59 minted rows did not move — a specificity result,
scored separately, not part of the control's denominator.

## T7. Facts the disassembly settled

* **Reads go through `FileSystem`, writes do not.** `?Read@` is six instructions
  and a tail-jump into `?ReadFile@FileSystem@IO@@`; `?Write@` calls `WriteFile`
  directly and hand-advances `FileInfo::cursor`. That asymmetry is why only one
  of the two needed a `_win` file — reading a PACKED file must consult the
  package index, and `FileSystem::ReadFile` is where that lives.
* **`?Write@` returns `written == len`, not the `WriteFile` BOOL**, which is
  discarded. A short write returns false *and* has already advanced the cursor.
* **`?SetCurrentPos@` writes the cursor BEFORE its branch** (0x1800aac1c), so it
  moves on the packed path that never touches Win32 and on a failed seek. Its
  discriminator `CMP [RCX+0x20],-0x3 ; JA` is **UNSIGNED** — read as signed it
  inverts and every packed read would `SetFilePointer` a handle that is not one.
* **`?GetCurrentPos@`/`?GetSize@` return UNSIGNED** (`UEBAIXZ`) while the fields
  are modelled `int32_t`; **`?SetCurrentPos@` returns `_N`, a bool**, not void.
  A one-instruction body cannot show a return type — the export directory is the
  only place these are visible.
* **`?Open@` is the read twin of `?Create@`**: same five steps, three
  differences — no `CreatePath`, `direction = 0` instead of 1, and
  `FileSystem::OpenFile` instead of `CreateFileW`. **A port written by analogy
  from `Create` would add the `CreatePath` call**, which would create
  directories on the way to a file it is about to read.
* **The copy constructor copies `Stream::encoding` as a RAW POINTER**
  (0x180023cc6). Two objects share one heap Encoding and both destructors free
  it — **a double-free waiting for the second destructor**, and a plausible
  reason nothing calls it. Reproduced exactly rather than deep-copied: an
  unguarded hazard in a faithful body is retail's statement, not ours to repair.
  The hazard is recorded at the body.

## T8. The one declared divergence

The `flags & 2` (delete[]) arm of **both** deleting-destructor thunks is a
**NAMED ABORT**. Retail calls the CRT's `eh_vector_destructor_iterator` at
`0x18007ec98`, unexported and unported; `binary_writer.cpp` faced this and took
the same disposition rather than open an address door onto a CRT helper for a
path no caller takes.

**Its limit, because it is weaker than the precedent's.** BinaryWriter's only
producer is a `MemAlloc(0x30,4)` with no array cookie, so that lane could PROVE
the arm unreachable. This lane can only say it **found no array producer** —
every FileStream it can see is stack-constructed or embedded in a
`CompressedFileStream`. The abort message says what would have had to happen.

## T9. What this tranche does NOT claim

* **NOT that either row is PROMOTED.** They are minted and at TRAP=0. `[RtMint]
  promoted 31` is a MIXED count; only **5 of 62 vtables** are promoted
  [CORRECTED — see §T11] — **minted is not promoted**, and until promotion nothing
  in play dispatches these bodies (§T3). `??_7BinaryFileStream@` is now fully
  owned (both stampers ours) and is *ready for* promotion; that is a different
  statement from being promoted, and the decision is not this lane's.
* **NOT that `??_7FileStream@` becomes promotable.** §T5 — twelve of its
  seventeen stampers are not ours, including `Engine::Run`.
* **NOT that these bodies run in play.** Every non-zero slot count in §T4 was
  produced BY THE ORACLE, not by gameplay. On the gameplay route they are still
  zero, and will be until promotion. The oracle proves the bodies are RIGHT; it
  proves nothing about whether anything reaches them.
* **NOT that the array arm works.** It is a declared abort and was deliberately
  not driven; `arrayArmAbort=0` on every run means it was never reached, which
  is the expected and desired reading.
* **NOT `??_7TextFileStream@`.** Handed back. It is minted at 20 slots and is
  really **10** — an unexported table sits at 0x1801559d0, exactly ten slots in,
  and `adjacent-export:` cannot see it. Three routes agree: the constructor
  NAMES the address (`??0TextFileStream` at 0x1800ba431 is
  `LEA RAX,[0x1801559d0]`), the byte dump shows ten coherent pointers and then a
  different class's descending group, and `FileStream.h` has said 10 since Wave
  11. **A second instance of the ReaderWriter 36-vs-31 defect**, reported to lane
  MINT with the general pattern: every concrete IO stream ctor heap-allocates an
  8-byte Encoding and stamps an UNEXPORTED vtable into it, and those tables live
  in .rdata interleaved with the exported class tables. Its 2 ported slots in
  §T6 are FileStream's shared bodies, not work on that row.
* **NOT a change to §0–§8.** Rows 7 and 8 of the sealed report
  (`??0TextFileStream@`, `??1FileSystem@`) are still **PORTED, NOT VERIFIED**,
  attendance 0 on every run here too. This tranche did not close that.
* **The screenshot board did not move.** Still **18 of 25 SHUT, 7 OPEN** — these
  ten bodies are not on the screenshot path, and three of the seven open doors
  are the very tables this tranche cleared, which are open for lack of
  PROMOTION rather than for lack of ports.

## T10. Handover, updated

* **Whoever owns promotion**: `??_7BinaryFileStream@` is now the strongest
  promotion candidate in IO — TRAP=0, **both** stampers ours, bodies
  oracle-verified. Promoting it is what turns §T4's green into a live route.
  `??_7FileStream@` is TRAP=0 but should NOT be promoted on that basis alone.
* **Lane MINT**: `??_7TextFileStream@` is over-sized by 10 (§T9), and the
  interleaved-unexported-Encoding pattern will over-size other rows in this
  family. Bounding a table by the next STAMPED address rather than the next
  export fixes the class, not the instance.
* **Whoever takes `??_7TextFileStream@`**: it is 10 slots and **8 bodies** — its
  slots 1 and 7 are FileStream's and already done.
* **Lane CSTREAM**: `??_7CompressedFileStream@` is BLOCKED 9 / 1 / TRAP=8 and
  its slot 7 trap is `?Close@CompressedFileStream@`, which you are porting. Its
  slots 1..6 are CompressedStream's, not FileStream's, so this tranche does not
  help it.

**Tranche commits on top of the sealed report: `5cb83ea` (slot bodies),
`2ab06ae` (the copy constructor), plus the oracle and this addendum.**
Evidence: `docs/runtime_port/wave19_lane_file/tranche_main_{debug,error}.log`,
`tranche_neg1.txt`, `tranche_neg2.txt`.

## T11. CORRECTION — "31 of 62 promoted" was MY error, and it is the same defect I was asked to diagnose

The integrator asked whether the `31 of 62` figure came from the readiness
screen with its denominator attached, or whether a lane had to derive it —
"because if a lane has to derive it, that is an instrument defect."

**A lane had to derive it, the instrument defect is real, and I fell into it.**
The screen prints two lines with DIFFERENT SUBJECTS:

```
[RtMint] promoted 31 minted DATA row(s) -- live on every run
[RtMint] minted 62 vtables (598 slots: ...), 104 BaseTypeInfo, 39 storage symbols
```

`promoted 31` carries **no denominator and does not name its kinds**. I paired
it with the `62` on the adjacent line. **"DATA row" spans three kinds**, and
counting them in `standalone/reg_mint_promoted.cpp` gives:

| kind | promoted | minted |
|---|---|---|
| `??_7` vtables | **5** | 62 |
| `?_typeInfo@` | 25 | 104 |
| `?_singleton@` / storage | ~1 | 39 |

**The correct statement is `5 of 62` minted VTABLES, not 31 of 62.** The five
are `??_7{SpriteSheet,Element,DialogInstance,ToolTipDialog}@UI@@6B@` and
`??_7ImageBuffer@Video@@6B@`. Wherever this report said 31 of 62 it now says 5
of 62 and points here; the wrong figure also went to the integrator in two
messages and is corrected there.

**And the corrected number makes the point far stronger.** The same run prints

```
[RtMint] 28 of 62 vtables are COMPLETE (every non-purecall slot is our port)
```

so **28 vtables are minted and complete, and 5 are registered. Twenty-three
complete tables are sitting unregistered**, my two among them. That is the
systemic figure, and no single line of the screen's output states it.

**Why it slipped**: `promoted 31` reads as a count of the thing the surrounding
lines are about, which is vtables. This is `reference_a_screen_that_cannot_name_its_subject.md`
in its quietest form — the line is not wrong, it simply does not say what it
counted, and the adjacent line supplies a plausible denominator for a different
population. The fix is one word: `promoted 31 minted DATA row(s) (5 vtables,
25 typeInfo, 1 storage)`.

## T12. THE `??_7Stream@IO@@6B@` QUESTION — confirmed, with a refinement that inverts the conclusion

The integrator asked whether a row can be **permanently unpromotable (NO-OP)
AND still an open cutover door**, and asked for the mechanism rather than the
inference.

**Confirmed. And the refinement matters: the two facts are not merely
compatible, they are about different objects.**

### The mechanism, from source

**Eight sites in six files resolve `??_7Stream@IO@@6B@`, and every one of them
is a WRITE of the address into an object's vptr. Not one dispatches through the
table:**

```
runtime/src/io/compressedstream.cpp:166        static const void* v = ResolveExport("??_7Stream@IO@@6B@");
runtime/src/io/file_stream.cpp:242,364,919     self->base.vptr = RT_DATA_SLOT(...)
runtime/src/object/external_add.cpp:366        vs.base.vptr    = RT_DATA_SLOT(...)
runtime/src/object/external_load.cpp:476       cfs.base.base.vptr = RT_DATA_SLOT(...)
runtime/src/object/external_savetext_win.cpp:373  ts.base.base.vptr = RT_DATA_SLOT(...)
runtime/src/save/saveload_meta.cpp:611,649     ... = RT_VTABLE_ONCE("??_7Stream@IO@@6B@")
runtime/src/ui/userinterface.cpp:1621          return RT_DATA_SLOT(...)
```

Corroborated live rather than only from source: the strict run records
`[RtResidue] 5 … ??_7Stream@IO@@6B@` — five resolves that actually reached the
DLL.

So phase 0a's finding (**nothing ever DISPATCHES through this table**) and this
lane's OPEN verdict (**several things need its ADDRESS to stamp**) are both true
and independent. A destructor that stamps `??_7Stream@` on the way down is
reproducing retail's store; it does not care what is in the table.

### The refinement: it is REGISTRATION that satisfies the door, not promotion of CONTENT

`RtDataMintInit` (`standalone/rt_data_mint.cpp:909`) separates two things that
the word "promote" runs together:

* **Minting** lays out our storage — a copy of the table with our ported slots
  substituted and traps elsewhere. Every row is minted.
* **Registering** publishes that storage under the symbol and address.
  `RegisterMintedData()` does it for ALL rows but is gated on
  `BANISHED_RT_MINT`; the default path calls `RtMintRegisterPromotedRows()`,
  which publishes only the measured-safe subset.

An unpromoted row's storage EXISTS and is not published, so `RT_DATA_SLOT` misses
the registry and falls through to the DLL. **With the DLL gone, all eight sites
above stamp NULL.**

**So `UNPROMOTABLE(NO-OP)` correctly says "no PORT work remains" and must NOT be
read as "satisfied at cutover".** The integrator's inference is right.

**And the inversion, which is the part worth acting on:** the reason promotion is
a measured, one-row-at-a-time decision is the risk that a *wrong slot* gets
dispatched. **For a proven NO-OP row that risk does not exist** — nothing
dispatches through it, so the storage's contents are irrelevant and could hold
retail's pointers verbatim. **A NO-OP row is therefore the SAFEST possible
promotion, not one to skip.** Phase 0a's proof is exactly the warrant that makes
registering it cheap.

The same shape covers 3 of this lane's 7 open doors: `??_7Stream@`,
`??_7FileStream@`, `??_7BinaryFileStream@` are open for lack of REGISTRATION,
not for lack of ports.

**What this lane did NOT verify**: that promoting `??_7Stream@` is safe, or that
phase 0a's NO-OP proof is sound. Neither is this lane's; the answer above is the
mechanism only. Note also that the door screen alone proves only that the symbol
is ABSENT FROM THE REGISTRY — it is a registry query, not a live resolve. The
residue census is what proves something actually asks.

## T13. ROW 7 CLOSED — and the harness was in the tree

The integrator asked whether a save-text harness exists or whether this lane
simply did not look. **The honest first answer was "did not look". The second
answer, after looking, is that it exists** —
`runtime/src/object/external_savetext_probe.cpp`, armed by
**`BANISHED_RT_SAVETEXT_PROBE=1`** from `external_add.cpp:277`, with three
negative controls. `reference_grep_the_harness_before_declaring_no_route.md`
paid out again, against this lane.

Measured on that route:

```
[SaveTextProbe] VERDICT ctl=0 checked=13 failed=0 bytes=42 serCalls=1/1  PASS
[ExtSaveText] calls=1 typeFound=1 createFailed=0 serialized=1 serializeTrue=1
[FileCutover] attendance  ??0TextFileStream=1
```

**Row 7 (`??0TextFileStream@`) is EXERCISED and sits on an oracle-verified
path**: 13 checks, 0 failures, 42 bytes byte-compared. Attendance 1 is the
oracle's OURS pass — the THEIRS pass calls the shipped constructor in-DLL, which
is not a door and does not reach our counter, so 1 is the expected figure and
not a shortfall.

**Precisely what this does and does not claim.** It is not a dedicated oracle on
the constructor: it is an ENCLOSING oracle whose ours-side runs through our
constructor and whose 42 compared bytes are what the resulting stream wrote.
That is materially stronger than attendance alone and materially weaker than the
slot oracle in §T4. Row 7 is recorded **PORTED, EXERCISED, COVERED BY AN
ENCLOSING ORACLE** — not "verified" in §T4's sense.

**A fact from that probe worth carrying wave-wide**: the retail TEXT encoding's
two serializer factories are STUBS — `0x1801559d0+0x18 -> 0x1800b9a80` and
`+0x20 -> 0x1800b9a70`, both `XOR EAX,EAX ; RET`. **Text I/O is stripped from
the shipped build** (it is a Tools-x64.exe path), which is why `??0TextFileStream@`
can never be attended on any gameplay route in this DLL, and why the probe's
subject has to be synthetic. That is a fact about retail, established by running
retail.

## T14. ROW 8 — a written expiry, from the callee, not from a guess

`??1FileSystem@IO@@QEAA@XZ` reads attendance **0 on every run in this document**,
including the SaveText route. Before writing a holdback I read the caller, as
lane PERSIST's retired `??1Persistence@` blocker requires.

**Its sole caller in this tree is `RtEngine_Engine_Shutdown`**
(`runtime/src/engine/engine_lifecycle.cpp:1005`, inside the teardown chain that
also destroys Factory, InternalFactory, ExternalFactory, Modification and
Persistence). And that file **already records, with evidence, that its own
`calls=` is structurally zero** (`engine_lifecycle.cpp:183`):

> `Engine::Shutdown`'s `calls=` is ZERO and will stay zero. `game_main.cpp`
> resolves `?Shutdown@Engine@1@QEAAXXZ` only to satisfy its own null guard
> (line 426) and then calls its own `ShutdownEngineStaged()` instead (line 924).
> Nothing in this project calls the real one.

So row 8's blocker is **not "no driver found"** — it is that its only caller is
deliberately never called, by this project's own design. That is a stronger and
more useful statement than a failed search, and it came from the callee.

**WRITTEN EXPIRY, in the Wave 18 form:**

* **What would verify it**: any run in which `RtEngine_Engine_Shutdown` actually
  executes. Its five sibling destructor calls would be exercised at the same
  time, so this expiry is shared with `??1Modification@` and `??1Persistence@`.
* **What would have to change for that to be possible**: `game_main.cpp:924`
  would have to call the real `Engine::Shutdown` instead of
  `ShutdownEngineStaged()`. That is a deliberate harness decision, not an
  accident, so this expiry is a REQUEST TO ANOTHER LANE, not a waiting game.
* **The cheaper route that does not need any of that, and which this lane did
  NOT build**: a direct two-subject oracle in the shape of §T4's `CaseCopyCtor`
  — build two identical `IO::FileSystem` images, run ours on one and the shipped
  body on the other, compare the bytes and the freed blocks. It is possible
  today. **Its weakness is stated rather than discovered later**: the subject
  would be hand-built, so it would test our construction of a FileSystem as much
  as retail's destruction of one, and the packfile-index tree at +0x40 is the
  part most likely to be built wrong. That is why it is offered as a scoped
  follow-up rather than done in a hurry at the end of a lane.

Row 8 stays **PORTED, NOT VERIFIED**, with the above as its expiry.
