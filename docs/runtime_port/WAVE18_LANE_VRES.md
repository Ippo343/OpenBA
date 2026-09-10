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
