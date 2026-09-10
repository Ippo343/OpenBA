# WAVE 18 — lane VRES: the Video resource lifecycle

Worktree `C:/code/GIT/wt-w18-vres`, branch `wave18/vres`, cut from `d12e8be`.
Every number below was measured on this branch, debug build, DLL present, under
`tools/game_dir_lock.sh acquire vres` — held for the whole run batch, released
promptly, `assert-held vres` gating every deploy, never piped.

## 0. Headline

**6 of 7 rows ported, registered by NAME and by ADDRESS, and verified against
the shipped bodies. The 7th is BLOCKED with a named expiry condition.**

| # | symbol | retail | ported | reg name | reg addr | verdict |
|---|---|---|---|---|---|---|
| 1 | `??0RenderBuffer@Video@@QEAA@XZ` | 0x1800f3ee0 | yes | yes | yes | **VERIFIED** (live, calls=1) |
| 2 | `??1RenderBuffer@Video@@QEAA@XZ` | 0x1800f4020 | yes | yes | yes | **VERIFIED** (live, calls=1) |
| 3 | `?Bind@RenderBuffer@Video@@QEBAX…` | 0x1800f4300 | yes | yes | yes | **VERIFIED** (live, calls=5568) |
| 4 | `?Recreate@ImageBuffer@Video@@QEAAXXZ` | 0x1800f33f0 | yes | yes | yes | **VERIFIED by synthesised oracle** (live calls=0, no route) |
| 5 | `??1Configuration@Video@@QEAA@XZ` | 0x1800f6f80 | yes | yes | yes | **PORTED, NOT VERIFIED** (live calls=0, no route) |
| 6 | `??1Device@Video@@QEAA@XZ` | 0x1800f7290 | yes | yes | yes | **VERIFIED by replay** (live calls=0, no route) |
| 7 | `?SaveImageToFile@ImageBuffer@Video@@SA_N…` | 0x1800f2840 | no | — | — | **BLOCKED** — §6 |

Files: `runtime/src/video/video_resources.cpp` (six bodies),
`video_resources_win.cpp` (the one Win32 call), `video_resources_probe.cpp` (the
oracle). Kill switch `BANISHED_RT_VIDRES=0`. Probe `BANISHED_RT_VRES_PROBE=1`
(or `=neg` for the negative controls).

Every address and ordinal was read out of the shipped export directory with
pefile **before** any body was written
(`reference_resolveexport_phantom_names.md`). Every body was transcribed from
`disassemble_function` at that address, never from Ghidra's decompiled C
(`reference_ghidra_c_output_vs_disassembly.md`).

**Confirmed absent before writing.** Grep of all seven mangled names and all
seven retail addresses across `code/ engine/ runtime/ standalone/ dx11/` found
only address-table comments, two `RT_EXPORT_ONCE` call sites in
`engine_lifecycle.cpp`, and DX11's import table. Nothing was ported and nothing
was gated-but-ported.

## 1. The oracle

`runtime/src/video/video_resources_probe.cpp` — synthetic-subject replay against
the SHIPPED bodies, reached through `ResolveRuntimeRaw`, never `ResolveAddr`
(`reference_resolveaddr_consults_port_registry_first.md`: this lane registers
0x1800f7290, so `ResolveAddr` would have compared ours with ours and always
agreed). It self-schedules from the first `?Bind@RenderBuffer@`, not from init,
because it allocates ~40 blocks and `RtVideoPortInit()` runs when the DLL's
`MemoryAllocator` is constructed but not started.

Eight cases:

* **1–3** drive the three element-destroy helpers `??1Device` reaches —
  0x180010cf0 (per `DisplayMode`), 0x180010da0 (per `Adapter`), 0x180010f00 (per
  `GraphicsInterface`). **That is where 100% of this lane's parameterization
  lives** — every stride, every offset, and the nesting order.
* **4–5** drive `??1Device` end to end with all three arrays empty, covering the
  four steps the helpers do not: the `deviceData` slot-0 call, the null store,
  the singleton CMOV (**both** arms: case 4 is `type != 0`, case 5 is
  `type == 0`) and `DeleteCriticalSection`.
* **6–8** drive `?Recreate@ImageBuffer` against a fake `DeviceData` and a fake
  companion with recording stubs, one case per flag arm.

**Final run: `cases=8 mismatches=0`**, effectiveness `freed=12 refcount--=4
reached0=2 survived=2`, `RC=0`, 0-byte crash log. Both arms of the String
release are driven (one name at refcount 1 that must be freed, one at 2 that
must survive), which is the asymmetry a raw memcmp cannot see.

### 1.1 Controls, scored separately

| control | mechanism it names | caught |
|---|---|---|
| LEAK | skip the nested recursion — the "free the block instead of destroying every element" defect | **2 of 2** applicable cases |
| NOSTR | skip the String release (Body B slot) | **2 of 2** |
| SHIFT | Body A's 16-byte header clear lands 8 bytes early (the offset a spec row is transcribed at) | **3 of 3** |
| NOZERO | drop the `slot+8 = 0` half of the clear (the width of the clear) | **3 of 3** |

A control is scored **only on the cases where its wrong body actually runs**:
case 1 runs no Adapter or Interface walker, so LEAK and NOSTR are not applicable
to it and are not counted there
(`reference_control_must_pick_a_compared_subject.md`).

**Kill-switch A/B.** With `BANISHED_RT_VIDRES=0` every per-row counter is absent
and the run is clean (`RC=0`, empty crash log). The quantity varies with the
thing the gate gates, so it is a gate and not a decoration
(`reference_the_gate_was_never_a_control.md`).

### 1.2 The probe refuted itself three times before it was trusted

Recorded because two of the three produced **confident wrong answers**, not
errors.

1. **Two nested memcmps were use-after-free.** Each level frees the level
   below's buffer, so by the time the comparator read the nested `DisplayMode`
   and `Adapter` buffers they held the allocator's free-list links. It printed
   two MISMATCH lines that were pure artefacts, then dereferenced a link as a
   `System::String` and took the process down inside `CompareBuffers`. Both
   memcmps are gone; the nesting is covered by the level that drives it directly
   plus the delta below.
2. **Their replacement first measured BYTES.** `GetAllocatedSize` made the
   positive run fail by 24 bytes on one case and −16 on another while every
   compared byte matched: `Free` decrements `_allocatedBytes` by the **block's**
   size, which depends on splitting at whatever address the request happened to
   get, so two identically-sized subjects need not account for the same bytes.
   Switched to `_allocCount` (+0x638), decremented exactly once per `Free` and
   therefore symmetric by construction. **The switch is legitimate because LEAK
   still separates under it, by a larger margin than the noise the byte metric
   carried — not because it went green.**
3. **The third negative control produced UB, not a divergence.** It walked
   Adapters at stride 0x18 instead of 0x38, handed the allocator a pointer made
   of the probe's own seed pattern, and faulted inside
   `MemoryAllocator::Free`. A control that crashes proves nothing about the
   comparator and takes the remaining controls down with it. SHIFT replaced it —
   and SHIFT's own first spelling shifted *forward*, ran eight bytes past the
   element buffer for the two 0x18-stride specs and corrupted the allocator's
   next tag, which the probe also caught, as a WRITE fault inside `MemAlloc` two
   cases later. It shifts backward now.

## 2. What each destructor DESTROYS vs merely FREES

Asked of every dtor in this lane, and answered from the disassembly before any
code was written.

### `??1Device@Video@@` — three arrays, three different helpers

| array | helper | what it does |
|---|---|---|
| `+0x40 Array<GraphicsInterface>` | 0x1800109d0 | **DESTROYS EVERY ELEMENT**, two levels deep |
| `+0x30 Array<bool>` | 0x180009560 | **FREES THE BLOCK ONLY** — correct, `bool` has no destructor |
| `+0x20 Array<DisplayMode>` | 0x180010830 | **DESTROYS EVERY ELEMENT** |

The +0x40 walk, per `GraphicsInterface` (stride 0x18): recurse into 0x180010da0
over the element's `Adapter` array, free the `Adapter` buffer and clear its
16-byte header, release the `System::String` at +0x00. Per `Adapter`
(stride 0x38): free the multisample buffer at +0x18 and clear its header,
recurse into 0x180010cf0 over the `DisplayMode` array to free each mode's
`RefreshRate` buffer, free the mode buffer and clear its header, release the
adapter's name String. Writing this as "free the block" would leak, per Device,
one String and one Array per interface, plus one String and three Arrays per
adapter, plus one Array per mode. **That is exactly the defect
`reference_a_written_recipe_can_be_wrong.md` is about, and it is what the LEAK
control exists to exclude.**

**How it was checked:** the LEAK control, which produces `blocks released:
ours=5 theirs=9` on case 2 and `ours=3 theirs=21` on case 3. The correct body
produces equal counts on both.

**The two arrays the brief warned about are not conflated.** `??1Device` never
reads `dllSlots` at +0xB0 — no `FreeLibrary`, no clear. It destroys the +0x40
`Array<GraphicsInterface>` and ignores the +0xB0 plugin-slot array entirely.
(`ShutdownEngineStaged` step 7 is what frees those modules.)

**The singleton store is a CMOV, not an `if`.** `CMP dword [RBX],ECX / CMOVZ
RAX,RCX` rewrites `?_singleton@Device@Video@@` **either way** — with null when
`type == 0`, with its own current value otherwise. A tidier `if (type == 0)
*slot = nullptr;` would skip a write retail makes. Both arms are driven by cases
4 and 5.

### `??1Configuration@Video@@` — destroys nothing, frees nothing

Two instructions: `MOV qword ptr [0x1801fe7a0],0x0 / RET`. It does not read
`this`, takes no branch, and clears the exported singleton unconditionally — even
for a Configuration that was never the singleton. `Engine::Shutdown` does the
`MemFree` separately.

### `??1RenderBuffer@Video@@` — one instruction, destroys nothing itself

`JMP 0x1800f4400`. The linker folded `~RenderBuffer` into `PlatformDestruct`; it
is a tail-call alias and nothing else. What the destination does — unlink from
the circular `_firstRenderBuffer` list and release the companion at +0x28,
**without** nulling +0x28 (buffers.cpp:1330 calls that "the ImageBuffer
asymmetry") — has been ported and oracle-verified since Wave 9 lane GFX2. This
row adds no memory operation of its own.

## 3. Measured attendance, per row

`BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120
BANISHED_RT_DIFF=1`:

```
Video::RenderBuffer::RenderBuffer                       calls=1
Video::RenderBuffer::~RenderBuffer                      calls=1
Video::RenderBuffer::Bind [-> RenderBufferData slot 2]  calls=5568
Video::ImageBuffer::Recreate                            calls=0
Video::Configuration::~Configuration                    calls=0
Video::Device::~Device                                  calls=0   (2 with the probe on; both are the probe's)
```

`calls=0` has four causes and they invert the conclusion. Here the cause is
**structural and named**, not unknown:

* **`??1Device` and `??1Configuration`.** Their only caller in the tree is
  `RtEngine_Engine_Shutdown` phase 2 (`engine_lifecycle.cpp:963,967`) — and that
  function **never runs**: the tree already prints
  `[RtEngineLife] Engine::Shutdown ran 0 times -- STRUCTURAL … game_main.cpp
  calls ShutdownEngineStaged() instead`, and `ShutdownEngineStaged`
  (`code/Game/Game/game_main.cpp:206`) stops at step 13 with `(14+ deliberately
  omitted)`. Steps 14+ are the destroy-and-free phase. So these two rows are
  registered-but-uncalled on every route that exists today, and the census will
  correctly list them under "ports with no caller".
* **`?Recreate@ImageBuffer`.** Its only caller is
  `saveload_update.cpp`'s `OnVideoRestart`, the device-lost recovery path, which
  no harness route drives.

**"No gameplay route" is not "no oracle"**
(`reference_no_route_is_not_no_oracle.md`). `??1Device` is verified by cases 1–5
and `?Recreate` by cases 6–8, both against the shipped body.
`??1Configuration` is the one row this lane records as **PORTED, NOT VERIFIED**:
its entire body is a single store to a slot the probe would have to redirect to
observe, so an oracle for it would be testing the redirection. It is two
instructions and is registered by name and by address; if that is not good
enough for the wave, say so and it gets a case.

## 4. Corrections to the brief and to the tree

Lanes correcting the brief is the norm here, so these are stated plainly.

1. **The brief's caller list is right, but `dx11/src/contextdata.cpp` does not
   name this function's address.** It lists `?Bind@RenderBuffer@Video@@` at
   `0x180013448`, which in Runtime-x64.dll is a displacement inside an unrelated
   copy-assign body. **It is not a defect**: both DLLs prefer image base
   0x180000000, and that constant is VideoDX11's own IAT slot for the import,
   which is what `Dx11ResolveRuntimeImport` wants. The Runtime function is
   **0x1800f4300** and that is what this lane registers. Worth recording because
   the address looks like a Runtime address and reads like a bug.

2. **`array_destroy.cpp`'s destroy-helper cluster is FOURTEEN addresses, not
   eleven.** Reached from `??1Device`, three more appear, and all three are
   already covered by the bodies and specs that file exports:
   * `0x180010cf0` == BODY A, offs {0x08}, stride 0x18 — **a second address for
     the `kD_InnerArr_18` row**; the banner lists only `0x1800419b0`.
   * `0x180010da0` == a nested per-`Adapter` destroy: A{0x18}, recurse, A{0x08},
     B{0x00}.
   * `0x180010f00` == a nested per-`GraphicsInterface` destroy: recurse, A{0x08},
     B{0x00}.

   Both nested ones are the shape lane D2 named once as "the eleventh,
   0x18005c550, a RECURSIVE destroy". This lane **composes** them out of
   `rtcoll::FreeInnerArraysCore` / `ReleaseStringsCore` / `ArrayDtorImpl` rather
   than re-deriving, so it inherits lane D2's verification instead of writing a
   fourth copy. They are deliberately **not** registered at their own addresses,
   for the reason that file gives for the other eight: nothing in this tree
   resolves them, and a registration with no caller dilutes the census's
   dead-port signal.

3. **`runtime/types/video/Device.h` says "that list is why this lane does not
   port the ctor or the dtor" for `RenderBuffer`.** That sentence is **stale**:
   Wave 9 lane GFX2 ported `PlatformConstruct`/`PlatformDestruct`, which is where
   the whole list handling lives, so both rows are now three lines and one line
   respectively.

4. **The `?SaveImageToFile@` holdback note is wrong in one claim and stale in
   another** — §6.

## 5. Nothing in CEIL's 18 UNPROMOTABLE rows was touched

None of this lane's seven appears in CEIL §3.1 or §3.2. No `??_7`, no
`?_typeInfo@`, no `?_singleton@` row was registered: `??1Configuration` and
`??1Device` **write** the two Video singleton slots (through `RT_DATA_SLOT`, the
mint-aware door, which is what makes the write visible to every other reader —
`reference_port_must_reproduce_side_effects.md`), but neither slot is
registered, so both stay UNPROMOTABLE-HARMFUL exactly as CEIL classified them.

## 6. Row 7 — `?SaveImageToFile@ImageBuffer@Video@@SA_N…` (0x1800f2840, ord 3609)

**Class: BLOCKED.** Not NO-OP and not HARMFUL — promoting it would matter and
would work; it is unported for scope and dependency reasons, both nameable.

`runtime/src/video/buffers.cpp:1415` holds it back with three reasons. **Two of
them do not survive a re-check, which is why this section exists rather than a
one-line "inherited".**

* Reason 1, *"file I/O side effect — not diffable"*: **not a real blocker.** Two
  subjects, two paths, byte-compare the two files. That is the same
  synthetic-subject shape this lane used for the other six rows.
* Reason 2, *"Wave 4 (IO) and System::String territory"*: **half stale.** The
  `System::Filename` / `System::String` / allocator half is ported —
  `0x1800c6740`, `0x1800c6880`, `0x1800c9440`, `0x1800c92d0`, `0x1800c6970` and
  `0x1800c7ad0` are all registered today in `filename.cpp`, `string.cpp`,
  `setupdir_helpers.cpp` and `allocator.cpp`. The IO half is not.
* Reason 3, *"leaving it on the DLL costs nothing beyond the census … 
  w2_takescreenshot.cpp calls a different, already-ported capture path"*:
  **FALSE.** `code/Game/Game/w2_takescreenshot.cpp:165` resolves this exact
  mangled name through `ResolveExport` and **line 270 calls it**:
  `s_SaveImageToFile(&path, width, height, pixels);`. The screenshot path goes
  through this export and nothing else. At cutover, with no DLL, that call has
  no implementation. `reference_grep_cannot_tell_internal_from_unported_export.md`
  is the shape: a claim about a callee that a grep of the actual call site
  refutes in seconds.

**What actually blocks it.** Six callees are unresolved in this tree, and — per
`reference_grep_cannot_tell_internal_from_unported_export.md` — the export table
was run rather than a source grep, so this is a scopeable list and not a wall:

| address | name | kind |
|---|---|---|
| 0x1800ae430 | `?CreatePath@FileSystem@IO@@QEBA_NAEBVFilename@System@@@Z` | **named export**, unported |
| 0x1800a8200 | `??0BinaryFileStream@IO@@QEAA@XZ` | **named export**, unported |
| 0x1800aa8f0 | `?Create@FileStream@IO@@IEAA_NAEBVFilename@System@@@Z` | **named export**, unported |
| 0x1800accc0 | `?CloseFile@FileSystem@IO@@QEAAXAEAUFileInfo@12@@Z` | **named export**, unported |
| 0x1800881b0 | — | in-DLL internal |
| 0x180053da0 | — | in-DLL internal |

Four of the six are named exports and land squarely in **IO, whose gap of 34
rows is the next wave's declared subject** (KICKOFF §0). The body itself is
~1300 bytes and 37 calls: `Filename` path splitting, refcounted `String`
concatenation, `FileSystem::CreatePath`, `BinaryFileStream`/`FileStream::Create`,
a hand-assembled 24-bit BMP header, and a `WriteFile` loop writing rows
back-to-front.

**Expiry condition, concrete:** port it **with the IO wave**, once those four
`IO::FileSystem` / `IO::FileStream` exports land — at which point it is one
transcription with no unported dependency, and the oracle is
run-ours-to-path-A, run-theirs-to-path-B, `memcmp` the two files.

**This is a scope call and it is flagged, not absorbed.** It was reported to the
advisor when it was made. Porting it here would have added four IO name doors
that the IO wave is about to own, growing the cutover surface
(`reference_porting_can_grow_the_cutover_surface.md`) for a row that runs once
per user-initiated screenshot, while adding no coverage to the Video resource
lifecycle this lane was scoped around.

## 7. What is NOT covered, stated rather than left to a green board

* **The outer `~Array` bodies with a non-empty array.** An outer body's last act
  is to free the element buffer, so afterwards there is nothing to compare.
  `array_destroy_probe.cpp` declines the same seven bodies for the same reason.
  Covered by composition: each is `helper(this, _data, _size)` then
  `ArrayDtorImpl`, the helpers are replayed here and `ArrayDtorImpl` is lane Q's
  J1, replayed by `array_probe.cpp`.
* **The `deviceData != null` arm of `??1Device`.** It is a virtual dispatch to
  slot 0 with the delete flag set — it destroys and frees a real `DeviceData`,
  and a synthetic one would need a real DX11 companion and a real swap chain.
  Cases 4–5 drive the null arm only. The arm is one `if` and a vtable index,
  both read straight off the listing.
* **The ORDER in which blocks are freed.** The slots do not alias, so no
  comparator on the element buffer can see it — only the allocator can.
  Reproduced from the disassembly; checkable, if at all, with
  `BANISHED_REDZONE=watch`.
* **`??1Configuration@Video@@`** — see §3.
* **`?Recreate@ImageBuffer`'s negative control.** Cases 6–8 have no wrong-body
  control of their own. What stands in for one is the per-case effectiveness
  check that **the shipped body took the slot the case is about**: three cases
  observe three different slots (3, 2, 1) through the same trace instrument, so
  the instrument demonstrably discriminates, and bit 4's precedence over bit 5
  is measured rather than assumed. That is weaker than a wrong-body control and
  is counted as such.

## 8. Build and hygiene

`standalone-debug` and `standalone-perf` both build clean; `make check-log-strip`
exits 0 (it caught eight qualified `std::printf` in the probe, now unqualified).

Worktree `C:/code/GIT/wt-w18-vres`, branch `wave18/vres`.

---
---

# POST-REPORT WORK — everything below this line came AFTER the measurement

**Everything above this line is the lane's report as it stood at `024a06a`, and
it is SEALED.** It is the closed measurement for rows 1–6 and the advisor quotes
it as such. Nothing above has been renumbered, reflowed or re-verdicted; the
file above this line is byte-identical to `024a06a`.

Everything below is work the advisor **explicitly re-tasked after that report**.
It is kept here, separately, so the boundary between "what was measured" and
"what came after" stays visible in the artefact rather than only in the git log.
A lane that keeps improving after it reports invalidates the integrator's
measurement; an explicit re-task is legitimate *only* if that boundary survives.

**A DISCLOSURE, because this rule was broken once already.** The advisor's
boundary message and this lane's first row-7 commit (`047149f`) crossed in
flight. That commit edited the sealed sections in place — §0's headline and row
table, §1's case list, §1.2's "three times" → "five times", §3's attendance
table, §6, §8. The next commit restored the file above to its exact `024a06a`
content and moved all of it down here. The breach was in the write-up only; the
code at `047149f` was unaffected and stands.

## Tranche 1 — row 7, `?SaveImageToFile@ImageBuffer@Video@@` (re-tasked)

The sealed report records this row **BLOCKED**, with a written expiry condition
and a recommendation to defer it to the IO wave. **The advisor overrode that.**
The deferral case is not withdrawn — §6 above still states it, and one part of
it survives the port and is restated in §T1.2 below, because a reader could
otherwise infer the opposite from the row being ported.

**Result: PORTED, registered by NAME and by ADDRESS, VERIFIED by file
byte-compare.** `runtime/src/video/save_image_to_file.cpp`, kill switch
`BANISHED_RT_VIDSAVEIMG` — separate from `BANISHED_RT_VIDRES` because this is
the only row with unported IO dependencies, so an A/B that needs the sealed six
on and this one off has a switch for it. The one Win32 call is
`RtVideoWriteFileRaw` in `video_resources_win.cpp`.

| symbol | retail | ord | ported | reg name | reg addr | verdict |
|---|---|---|---|---|---|---|
| `?SaveImageToFile@ImageBuffer@Video@@SA_NAEBVFilename@System@@HHPEAI@Z` | 0x1800f2840 | 3609 | yes | yes | yes | **VERIFIED by file byte-compare** (live `calls=0`, no route) |

**It was not a wall.** The estimate in the sealed report was "a day of careful
transcription"; it took well under one.

### T1.1 The oracle — cases 9–11

Ours writes one BMP, the shipped body writes another from the **same** synthetic
pixels, and the two FILES are byte-compared. Three sizes, chosen for their
padding, because a run of only the middle one would never exercise padding and
padding written per-row instead of once is a real defect:

| case | rowBytes | data bytes/row | padding |
|---|---|---|---|
| 5 × 3 | 16 | 15 | ONE pad byte per row |
| 4 × 2 | 12 | 12 | none at all |
| 1 × 1 | 4 | 3 | one pad byte, single row |

**All three identical** — 102, 78 and 58 bytes, each exactly
`0x36 + rowBytes * height`. Confirmed a **second, independent way**: the same
three file pairs were compared outside the process with `cmp`, which reported
IDENTICAL for all three. That covers both headers, the bottom-up row ORDER, the
32bpp→24bpp packing and the zero padding at the end of every row.

Final run on the combined probe: **`cases=11 mismatches=0`,
`bmp-bytes-compared=238`**, all four negative controls GREEN, `RC=0`, 0-byte
crash log.

**Its control, and it is weaker than the others — counted as such.** Cases 9–11
have no wrong-body control. What stands in for one is the effectiveness guard,
which refused two separate green boards by reporting NO SUBJECT rather than NO
DIFFERENCE (§T1.5), plus the exact-size assertion, which requires each file to
be precisely `0x36 + rowBytes * height` and so cannot be satisfied by two
identically-truncated files.

### T1.2 PORTING IT DID NOT CLOSE THE SCREENSHOT'S CUTOVER HOLE

Stated here at the advisor's request, and it is the strongest part of the
deferral case the override set aside — it survives the port.

The ported body reaches four IO functions nothing in this tree has ported,
through `RT_EXPORT_ONCE` — and `ResolveExport` falls through to the raw DLL
implementation for an unported symbol. With the DLL absent those four resolve to
null and the screenshot path dies exactly as it did before this file existed.
**Porting moved the hole from "no implementation" to "an implementation with
four unresolved dependencies"; it did not close it.** Do not read a ported row 7
as a working screenshot at cutover.

The four, named here and in the file's banner so the **Wave 19 IO brief inherits
them rather than rediscovering them**:

| address | ord | symbol |
|---|---|---|
| 0x1800ae430 | 2130 | `?CreatePath@FileSystem@IO@@QEBA_NAEBVFilename@System@@@Z` |
| 0x1800a8200 | 55 | `??0BinaryFileStream@IO@@QEAA@XZ` |
| 0x1800aa8f0 | 1876 | `?Create@FileStream@IO@@IEAA_NAEBVFilename@System@@@Z` |
| 0x1800accc0 | 1726 | `?CloseFile@FileSystem@IO@@QEAAXAEAUFileInfo@12@@Z` |

plus `??1BinaryFileStream@IO@@UEAA@XZ` (0x180023c80, ord 532). The census's
`called` column grows by these; that is **deferred work being surfaced, not
invented work** — the screenshot path needs those four no matter who calls them.

### T1.3 Why it was cheap, and the uncomfortable half of that

The expensive part was already done. `runtime/types/io/FileStream.h` has pinned
`IO::FileStream` and `IO::BinaryFileStream` **completely** since Wave 11 lane
IOSTREAM — sizeof 0x50, `filename` at +0x18, `FileInfo` at +0x20, the Win32
handle at +0x38, the byte cursor at +0x2c. Nothing in this port re-derived a
layout.

So the sealed report's deferral was wrong on a second count beyond the census
trade the advisor named: it was wrong that the work was large. **And the stale
blocker inherited on that pass was this lane's OWN scoping judgement from its
first pass, not someone else's note** — the same failure as
`reference_holdback_blockers_are_stale_check_them_first.md`, one step closer to
home. Re-checking a blocker applies to the ones you wrote yourself.

**The `buffers.cpp:1415` note refuted itself**, exactly as the advisor said: its
opening line names `w2_takescreenshot.cpp` as the caller and its closing
paragraph says that file calls something else. Both halves sat in one comment
block for four waves. "Grep the call site" is the cheap lesson; **"the note
already disagreed with itself" is the cheaper one.**

### T1.4 Two facts the disassembly settles that a signature would get wrong

* **It SUBSTITUTES the extension, it does not append one.**
  `?GetPathFilename@Filename@System@@QEBA?AV12@_N@Z` with the bool SET returns
  the base name with its extension **removed**, and EMPTY when there is no `.`
  at all (0x1800c67a5). So the target is `directory + basename + ".bmp"`:
  `…\Screenshot0.bmp` → unchanged (the real caller), `…\shot.png` → `…\shot.bmp`
  (extension REPLACED), `…\shot` → a file literally named `.bmp`, with a `true`
  return. Found by the oracle, on both sides.
* **Four callees return a class BY VALUE, so each has a hidden return slot**,
  and its register differs between the members (RCX = `this`, RDX = the slot)
  and the free `System::operator+` overloads (RCX = the slot). Omitting it from
  a typedef is a wild write that still returns the right answer
  (`reference_hidden_retslot_wild_write.md`).

**The destructor was ported by calling the exported one, after checking.**
Retail INLINES `~BinaryFileStream` into this function's epilogue; the port calls
`??1BinaryFileStream@IO@@UEAA@XZ` instead — and the two were compared
instruction by instruction **before** the substitution, because assuming an
exported body matches an inlined one is how a prior lane nearly shipped a leak
under a byte-identity claim. Same six steps, same order, same offsets.

### T1.5 The probe refuted itself twice more — five times in total

The sealed §1.2 records three. These two are the same shape and both were caught
by the effectiveness guard reporting **NO SUBJECT** rather than NO DIFFERENCE:

4. **Case 9's first subject had no file extension, and the port was right.**
   Handed extensionless paths, both sides wrote a file literally named `.bmp`,
   both to the SAME name, and the readback found nothing. **The two sides
   AGREED — same return value, same output name — which is how the evidence
   pointed at the probe rather than at the body.** The guard printed "this case
   PROVES NOTHING" instead of a green comparison of two files that did not
   exist. The behaviour is now recorded in `save_image_to_file.cpp`.
5. **Then the readback path appended a second `.bmp`** and read
   `…_ours.bmp.bmp`. Same guard, same way.

Both are one-line bugs in the instrument, and **both would have read as port
defects** if the guard had not been there to say the comparison had no subject.

### T1.6 Attendance

`?SaveImageToFile@ImageBuffer` reads `calls=0` on the gameplay+pause-menu route.
The cause is named, not unknown: its only caller is
`w2_takescreenshot.cpp:270`, which runs on a user-initiated screenshot, and no
autostart route takes one. Verified by cases 9–11 against the shipped body
instead.

## Addendum to the SEALED §1.2 — filed here rather than inline

The advisor asked that the comparator's **structural blindness** be stated next
to the LEAK control's `ours=5 theirs=9` and `ours=3 theirs=21`, because those
numbers are what a reader will otherwise take as the whole proof. Those numbers
live in the sealed §1.2, and editing them in place is the thing the boundary
forbids — so the statement is filed here and says where it belongs. If the
integrator would rather have it inline, that is a one-line marked edit to sealed
material and it is the advisor's call, not this lane's.

**A memory comparator STRUCTURALLY CANNOT see this leak.** Each of the three
destroy levels frees the buffer the level below it walked, so a body that
skipped the recursion entirely leaves the outer buffer **byte-identical**. This
is the same shape as an oracle comparing bytes neither side writes
(`reference_oracle_must_not_compare_unwritten_bytes.md`): the comparison is
green and the thing it was supposed to test was never expressible in it.

So `5 vs 9` and `3 vs 21` are evidence that **the allocator-count instrument
works**. They are **not** evidence that a byte comparison would have caught
anything — it could not have, at any seeding, on any subject. A reader who takes
the LEAK numbers as the whole proof has the wrong model of what caught it.

And the generalisable half of the three self-refutations is the second one:
`_allocCount` was chosen over `GetAllocatedSize` because it is **symmetric by
construction** — decremented exactly once per `Free`, independent of the block
size the allocator happened to hand out — not because it looked more correct.
A metric that is right by construction beats one that is merely right on the
runs you have taken.
