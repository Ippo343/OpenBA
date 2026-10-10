# Wave 10, lane UIF — the three UI rows the allocator NO-GO was holding

**VERDICT: all three CLOSED, each with a real differential oracle, and each oracle
demonstrated to catch a planted defect.**

| Census row | Retail | Ord | Ported | Oracle | Negative control |
|---|---|---|---|---|---|
| `??1UserInterface@UI@@QEAA@XZ` | 0x180120b90 | 705 | yes | twin synthetic (object bytes + ordered dtor-dispatch log) | CAUGHT |
| `?SaveAllSettings@UserInterface@UI@@QEAAXXZ` | 0x1801204b0 | 3608 | yes | live buffer-replay **and** twin synthetic (settings buffers + ordered call log) | CAUGHT |
| `?CreateInstance@ElementDescription@UI@@QEBAXV?$InternalPtr@VElement@UI@@@Object@@@Z` | 0x180115ad0 | 2004 | yes | live replay on the Element **and** twin synthetic (3 cases) | CAUGHT |

Branch `wave10/lane-uif`, worktree
`C:\code\GIT\untitled_rev_eng_project\.claude\worktrees\agent-a17f7f0e1e24f988d`,
cut from `wave10/base` @ `94ca43a` (the lane-ALLOC merge).
All runs under `tools/game_dir_lock.sh acquire lane-UIF`, debug exe
**md5 `93e2ef9cefa1ac943f98252445f070a1`**, `Runtime-x64.dll` present throughout.
Every mangled name was checked against the shipped export directory with `pefile`
**before** a body was written — no phantoms
(`reference_resolveexport_phantom_names.md`).

---

## 1. Which runs are trustworthy — the clobber question, answered by md5

The coordinator flagged a window (~23:00–23:01) in which lane UI4 overwrote the
deployed exe. **Every number in this document was re-taken afterwards**, and each
run's launch is bracketed by an md5 of the deployed binary against this lane's own
build. All of them read `93e2ef9c…` before AND after:

| Run | Time | Config | Result |
|---|---|---|---|
| `uif_armed_R2` | 23:03:30 → 23:04:30 | `RT_DIFF=1 RT_ALLOC=1 SAVE_TEST=900` | rc=0, crash log **0 bytes**, all sites 0 divergences |
| `uif_unarmed_R2` | 23:04:38 → 23:05:38 | `RT_DIFF=1`, **no** `RT_ALLOC` | rc=0, crash log 0 bytes, `RtAlloc` lines = 0, identical verdicts |
| `uif_neg1_R2` | 23:05:53 | `UIF_NEGCTRL=1` | dtor twin **divergences=1**, other two 0 |
| `uif_neg2_R2` | 23:06:38 | `UIF_NEGCTRL=2` | CreateInstance twin **divergences=4**, other two 0 |
| `uif_neg3_R2` | 23:07:22 | `UIF_NEGCTRL=3` | SaveAllSettings twin **divergences=3**, other two 0 |
| `uif_redzone_shim` | 23:10 | `RT_DIFF=1 REDZONE=1` | rc=0, crash log 0 bytes, **0 redzone violations** |

The pre-clobber runs (22:45–22:58) reached the same verdicts; they are discarded
rather than caveated, as instructed.

## 2. Which conclusions depend on `BANISHED_RT_ALLOC=1` — **none of them**

This is the question the brief asked to be answered explicitly, and the answer is
sharper than "it also works unarmed".

* Lane ALLOC's gate opening is what made these three bodies **writable**: all three
  free heap blocks, and the standing objection was "our body allocates, the DLL's
  unported code frees". With `BANISHED_RT_ALLOC=1` proven to arm with the DLL
  present there is demonstrably one heap, so the objection is retired *as an
  argument*.
* But none of the three bodies **depends** on the flag at run time. They free
  through whichever `MemFree` is live, and the blocks they free came from the same
  one. `uif_unarmed_R2` is byte-for-byte the same set of verdicts with
  `grep -c RtAlloc standalone_debug.log` = **0**.
* Lane ALLOC's one still-standing caveat — *"do NOT arm `Shutdown` /
  `~MemoryAllocator` into lane N's teardown crash"* — is about the **allocator's
  own** teardown pair. `~UserInterface` is a *client* of the allocator. Nothing
  here arms `Shutdown`.

## 3. `??1UserInterface@UI@@QEAA@XZ` — 0x180120b90

Four phases, transcribed from the disassembly (not the decompiled C):

1. per `_removedDialogs[i]`: find its node in `_dialogs`, unlink head/tail/prev/next,
   `--_count`, **free the node**, then re-read `_removedDialogs._data` and run the
   dialog's deleting destructor unless its flag bit 5 is set;
2. walk what is left of `_dialogs` and destroy each remaining dialog (nodes NOT freed);
3. free `_events` (+0x48), `_addDialogs` (+0x28), `_removedDialogs` (+0x18) in that
   order, zeroing pointer and size/capacity as two QWORDs each;
4. tail-JMP to `Collection::LinkedList<8>::Clear` (0x1800325f0), which frees the
   surviving nodes.

### Two traps only the disassembly shows

* **The flag word is read BEFORE the null test**, in both loops:
  `180120c7c MOV ECX,[RAX+0x20]` precedes `180120c8c TEST RAX,RAX`. Reproduced in
  that order. CLAUDE.md's rule applies — a guard would not restore the behaviour,
  it would skip it.
* **`_removedDialogs._data` is RE-READ after the node free** (`180120c74`). A body
  that caches it across the free is wrong in exactly the way a reallocating
  allocator exposes.

### The oracle: twin synthetic

A destructor cannot be replayed. It can be run twice on two subjects that are
identical *by construction*, and unlike the constructor's twin this one needs an
object **graph**: five 0x28-byte synthetic dialogs out of the real allocator, each
with a one-entry fake vtable whose slot 0 is a **recorder** rather than a
destructor, a 4-node `_dialogs` list and a `_removedDialogs` array of
`[d1, d0, d4, d3]`. Population chosen to hit every arm exactly once —
d1 middle, d0 head, d4 absent-and-bit-5-set, d3 tail, d2 left for phase 2 — so the
expected log is exactly `d1(1) d0(1) d3(1) d2(1) `, and **that string is asserted,
not merely compared**, so "both sides did nothing" cannot read green.

Compared: all 0x128 object bytes (both sides start from the same 0x5A poison, so
the ~200 bytes neither body touches are equal by construction) plus the log.
**Not** compared: the *identity* of the pointers handed to `MemFree` — each side
frees its own blocks at different addresses. That question is answered by §7.

## 4. `?SaveAllSettings@UserInterface@UI@@QEAAXXZ` — 0x1801204b0

Per dialog with a non-null buffer at +0xa8: build an `IO::MemoryStream` on the
stack (inlined), allocate an 8-byte `IO::Encoding` and stamp the shared unexported
vtable 0x1801554a8, point the stream at the dialog's own settings buffer, write the
`0xdeadbeef` magic and advance the cursor to 4, create a `BinaryWriter` through
encoding slot +0x20, call the dialog's `+0x48`, serialize the controller count
(+0xa0) and then per controller its id (+0x10 → +0x98) plus a throw-away
per-controller writer around its `+0x50`; finally the inlined `~MemoryStream`.

**Every IO primitive it needs was already recovered and ported** —
`runtime/types/io/Stream.h` pins `IO::MemoryStream` field for field and my
disassembly reproduced every offset independently; `runtime/src/io/encoding.cpp`
already ports `CreateReader`/`CreateWriter`. This row was never an IO problem.

### `DialogInstance` +0xa8/+0xb0 are NOT what the header calls them

`runtime/types/ui/DialogInstance.h` names them `_userData` / `_userFlags`, correctly,
*from the constructor*, where they are parameters 5 and 6. **This caller reads them
as `(settingsBuffer, byteCount)`** — `1801205b5 MOV RDX,[R14+0xa8]` becomes
`MemoryStream::data`, `1801205ae MOV EAX,[R14+0xb0]` becomes its size, and
`1801205d4` writes a magic *into* it. Both readings are of the same two words. This
lane did **not** rename lane B's fields; it reads them at their offsets and records
the disagreement here, which is the honest form.

### Two oracles, because the live one has no population

The live oracle is a **buffer replay**: snapshot every dialog's settings buffer,
run ours, copy the result out, restore the snapshot, run the shipped body, compare.
Sound for the reason `reference_write_path_serialize_is_replayable.md` records.

It is live and it reports **`calls=0 worked=0`** — measured on a run that *really
saved* (`[SaveTest] frame 900: invoking retail autosave handler` …
`frame 961: operation 4 -> 0`). The site IS registered, so the body ran; `worked=0`
says not one dialog in the live list had a settings buffer, so the loop was skipped
every time. That is
`reference_calls_zero_can_mean_always_took_the_uncovered_branch.md`, and it is why
`worked=` is reported at this site at all.

So the interesting half is **built**: two identical synthetic `UserInterface`s, each
with two dialogs that DO have 256-byte settings buffers and two controllers apiece,
recorders on `+0x48` / `+0x10` / `+0x50`. Everything else is real — the encoding
allocation, both `CreateWriter` calls, real `BinaryWriter` `ScalarInt`/`ScalarUInt`
writes into the buffer, the throw-away deletes, the inlined `~MemoryStream`.
Compared: the ordered log (asserted equal to `D0 C0 C1 D1 C2 C3 `), **both settings
buffers byte for byte**, and an effectiveness control asserting
`buffer[0] magic == 0xdeadbeef` so that "both sides left the 0xC3 fill alone" cannot
read green.

## 5. `?CreateInstance@ElementDescription@UI@@QEBAX…@Z` — 0x180115ad0

`InternalPtr<T>` is an 8-byte class with a non-trivial copy ctor, so MSVC passes it
**by address** even though the name spells it by value — the body dereferences RDX
eleven times. The second parameter is `void** elemPtr`, not `void* elem`.

### Three facts only the disassembly shows

* **(a) The inlined `UI::Dialog` temporary IS the exported ctor/dtor, instruction
  for instruction.** `??0Dialog@UI@@QEAA@XZ` (0x180112db0) and
  `??1Dialog@UI@@UEAA@XZ` (0x180112e40) were both disassembled and compared against
  the inline. The ctor matches in every store, differing only in that the inline
  writes +0x20/+0x28 as one 16-byte `MOVDQA` instead of two QWORDs; the dtor matches
  the whole teardown including the tail-JMP to `~External`. So the port
  **transcribes** the ctor (constant stores only — no new callee) and **calls** the
  exported dtor.
* **(b) `elem->_toolTip._index` is written as a QWORD over an uninitialised upper
  half.** Retail stores the int32 index to a stack slot and reads **eight** bytes
  back (`MOV [RBP+0x1f],EAX` … `MOV RAX,[RBP+0x1f]` … `MOV [RCX+0x50],RAX`), so
  element +0x54..+0x57 receives whatever was on retail's stack. That is
  indeterminate and cannot be reproduced. **The port writes the DWORD only and both
  oracles exclude +0x54..+0x57.** This is the one place the body is deliberately not
  byte-faithful, and it is stated rather than hidden.
* **(c) The flags cascade is DEAD CODE in the shipped binary.** The guard is
  `EDX = flags; EAX = EDX|0x90000; [+0x34] = EAX; EDX ^= EAX; TEST DL,1; JZ`.
  `0x90000` touches bits 16 and 19 only, so the low byte of the XOR is always zero
  and the parent walk up the +0x98 chain can never execute. Transcribed anyway —
  "provably unreachable in this build" is not a claim a port should bake in.

### It has no live caller, and that was measured

A `RT_DIFF=1 AUTOSTART=1` run registers 542 diff sites and
`UI::ElementDescription::CreateInstance` was **not among them** — the body was never
entered. The reason is structural: the only caller in the tree is
`code/Game/Component/frustumoutlinedescription.cpp:116` (through exe PE-import slot
0x1401518d0), and a `FrustumOutlineDescription` is not instantiated on any route the
harness can drive; Ghidra reports **no xrefs at all** to 0x180115ad0, every other
caller being a virtual dispatch inside the DLL.

So both oracles ship:

* a **live replay** on the Element, skipped when `_childDesc != nullptr` (slot 8 of
  a concrete description *creates* an element; replaying it would build a second
  one and report two fresh addresses as a "divergence"). The skipped/replayed split
  is printed on every report as `nochild=N child=M table=K`. String refcounts are
  compensated — a reference is taken on the old buffer before either body runs and
  one dropped from the new one afterwards — because both bodies assign against the
  *same* two `System::String`s.
* a **twin synthetic** with three cases: (0) no child / no table, (1) a fake child
  description whose vtable slot 8 hands back a sentinel, (2) a synthetic
  `UI::StringTable` with one matching entry so **all four** cascade guard conditions
  pass. The effectiveness control asserts `cases=3 border=1 cascade=1 mismatched=0`,
  so a green byte compare over three cases that all took the same arm cannot pass.

**Not covered, stated:** the String assign's *freeing* arm (at refcount 1 the first
body frees the buffer and the second would decrement freed memory; giving each side
its own buffer makes the post-state differ by pointer value and manufactures a
divergence — `reference_control_must_pick_a_compared_subject.md`), and the case
where `GetPath()` yields a non-empty path. The *release* arm IS covered.

### Wider blast radius

`standalone/address_registry.cpp:1114` binds exe PE-import slot 0x1401518d0 with
`RegisterDispatchSlot(slot, ResolveExport(name))`, and `RuntimePortInit()` runs
before `AddressRegistryInit()`. Registering this row therefore re-points the exe's
own `.rdata` slot at our body, not merely our `fn_*` pointers. That is exactly the
row the census asked for (it is *sourced* from address_registry.cpp) — and it is why
this one gets a replay oracle rather than a shape check.

## 6. The negative controls — 3 for 3, each caught by its own oracle only

`BANISHED_UIF_NEGCTRL=<n>` plants one defect. A green oracle that has never failed
is an untested instrument.

| n | Planted defect | Expected to be caught by | Result |
|---|---|---|---|
| 1 | `~UserInterface` destroys the bit-5 dialog it must skip | dtor twin's **ordered log** | `divergences=1`, other two sites 0 |
| 2 | `CreateInstance` drops the `_id`/`_tabOrder` **crossover** | CreateInstance twin's **byte compare** | `divergences=4` (and the live replay site 2), other two sites 0 |
| 3 | `SaveAllSettings` skips the cursor advance past the magic | SaveAllSettings twin's **buffer compare** | `divergences=3`, other two sites 0 |

Each control is scored **separately**, and the isolation matters as much as the
catch: in every run the two innocent oracles stayed at 0, so the union number cannot
be hiding a blind screen (`reference_union_control_hides_a_blind_screen.md`). Each
subject is chosen *inside* the compared extent
(`reference_control_must_pick_a_compared_subject.md`).

## 7. Heap integrity — and one instrument that is broken independently of this lane

The twin oracles' *freed-pointer identity* is not comparable between sides, so the
"did we hand `MemFree` the right block?" question needs a different instrument.

* **`BANISHED_REDZONE=1` (the shim): PASS.** `uif_redzone_shim`, rc=0, zero-byte
  crash log, `grep -cEi "REDZONE.*(violation|corrupt)"` = **0**, and all three twin
  oracles green in the same run. The twins allocate and free ~40 blocks through the
  live allocator and the live `~UserInterface` runs; a wrong pointer would corrupt
  the free list.
* **`BANISHED_REDZONE=watch` (the 1 ms watchdog): BROKEN ON THIS TREE, not by this
  lane.** It crashed twice, both times inside the watchdog's own free-list walk. The
  discriminating run is the important one: with `BANISHED_RT_DIFF` **and**
  `BANISHED_RT_ALLOC` **both unset** — so not one of this lane's oracles allocates
  anything and the ports are merely registered — `watch` still died
  (`EXEC of 00000001e39a3c70`, module UNKNOWN, mid-run). It is therefore not
  attributable to these ports. **Recorded as an open item for whoever owns the
  redzone harness; this lane did not chase it.** Note that lane ALLOC's §5c
  DLL-present pass condition rests on that same instrument.

## 8. Census — the SET diff, not the count

`tools/runtime_census.py`, before/after on the identical tree
(`reference_diff_the_set_not_the_count.md`, and
`reference_scanner_walks_cwd_not_script.md` — both runs from this worktree root):

```
UI    before:  total 1625   called 164   ported 135   82.3%
UI    after :  total 1625   called 167   ported 139   83.2%
```

`ported +4` for three ports, and `called +3`, is explained entirely rather than
assumed:

```
PORTED ENTERED  ??1UserInterface@UI@@QEAA@XZ
PORTED ENTERED  ?CreateInstance@ElementDescription@UI@@QEBAXV?$InternalPtr@VElement@UI@@@Object@@@Z
PORTED ENTERED  ?SaveAllSettings@UserInterface@UI@@QEAAXXZ
PORTED ENTERED  ?GetIndexFromName@StringTable@UI@@QEBAHAEBVString@System@@@Z    <-- see below
CALLED ENTERED  ??1Dialog@UI@@UEAA@XZ          (ported=0)
CALLED ENTERED  ??_7Dialog@UI@@6B@             (ported=0, and NOT in the mint table)
CALLED ENTERED  ?GetIndexFromName@StringTable@UI@@QEBAHAEBVString@System@@@Z    (ported=1)
```

* The fourth `ported` entrant is **not a fourth port**. `?GetIndexFromName@StringTable@`
  has been registered since Wave 7 (`runtime/src/ui/sprite.cpp:787`) but had **no
  caller**, so the census — whose denominator is `called` — could not count it. My
  body calling it made an existing port visible. This is
  `reference_half_registered_port_reads_as_absent.md` seen from the other side.
* **This lane grows the cutover surface by two UI rows**, both belonging to
  `UI::Dialog`, which no lane owns this wave:
  `??1Dialog@UI@@UEAA@XZ` and the data row `??_7Dialog@UI@@6B@`
  (`reference_porting_can_grow_the_cutover_surface.md` — said here rather than left
  for a later census to discover). Transcribing the inlined `~Dialog` instead would
  have swapped one export row for two unexported address doors (0x180039360,
  0x180039280) and would still have needed the vtable, so it is not a better trade.

Net unported UI **export** rows: 29 → **28**. The three lane rows are gone from
`--unported --namespace UI`; what remains under `UserInterface` is
`CloseDialog` / `Draw` / `ShowDialog` / `Update` (lane UI1), `?Serialize@ElementDescription@`
(lane EXTPTR) and `??0DialogInstance@` (lane UI3) — all outside this lane's fence,
all untouched.

## 9. Scope fence — nothing outside it was edited

No lane UI1 / UI3 / EXTPTR body was changed. Files touched:

* `runtime/src/ui/userinterface.cpp` — the two `UserInterface` ports and their oracles
* `runtime/src/ui/dialog.cpp` — the `CreateInstance` port and its oracle
* `runtime/src/ui/dialog_settings_probe.cpp` — **build fix only**, see below

**`DialogControllerConfigEntry` field-name defect (lane UI3): NOT AFFECTED.** No
body or oracle in this lane reads those two fields, by name or by offset; the only
`_description` in the diff is `UI::Element::_description` at +0xb8, a different
struct, written by raw offset.

**Build fix requested by lane PERFFIX, applied:** the 3 remaining `std::printf(`
call sites in `runtime/src/ui/` (`dialog.cpp` ×1, `dialog_settings_probe.cpp` ×2)
are now unqualified, since `standalone/log_strip.h` cannot expand a qualified call.
`standalone-perf` still fails on this branch — the other ~267 sites live in files
this lane does not own and are fixed on `wave10/lane-perffix`.

## 10. One reusable lesson, paid for in a crash

**An oracle that needs the HEAP cannot run from a registration function.** The first
version of the dtor twin was called from `RtUiManagerRegister()` and the process died
there every time:

```
Exception 0xC0000005   Fault: WRITE of 0x0000000000000648
RCX=0000000000000640    [REDZONE-DUMP] no allocator singleton
```

`RuntimePortInit()` runs long before `Engine::Startup` creates
`System::MemoryAllocator::_singleton`, so retail's inlined
`EnterCriticalSection(_singleton + 0x640)` writes to address 0x648. Both twin
oracles are now triggered from the **first live constructor** of their class
(`??0UserInterface` and `??0ElementDescription`), which is the earliest point that
is guaranteed to be past allocator startup. The `WRITE of 0x648` / `RCX=0x640`
signature is worth recognising on sight: it is *always* a null allocator singleton.
