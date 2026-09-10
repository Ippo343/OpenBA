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
because **`[RtMint] promoted 31 minted DATA row(s)` out of 62 minted** — READY is
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
  *not promoted*, so it is an open door. 31 of 62 minted rows are promoted.
* **Lane PERSIST / whoever next**: `?SetObject@TextFileStream@IO@@` is newly in
  `called` and unported. It is row 7's only remaining door.
* **The integrator**: the census cannot see 4 of this lane's 12 bodies (§2.1).
  A count of rows closed will understate it; the set-diff will not.
* **Every lane**: `tools/lane_file_run.sh` generalises trivially. §6.4 cost this
  lane one whole run.

**Two commits: `45d2baa` (the ports), `4025da5` (the instruments and evidence).**
