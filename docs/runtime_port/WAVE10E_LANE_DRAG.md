# Wave 10E — Lane DRAG — `BeginDragDrop`, the last genuine port in the UI namespace

> ## ✅ CLOSED — **UI 184 / 186 = 98.9%** (from 183 / 186 = 98.4%)
> One function ported, **differentially verified against the shipped DLL on
> every field it writes**, with a positive control that was scored and CAUGHT.
> The census set diff is exactly one row, `ported 0 -> 1`: **nothing entered the
> census and nothing left it.**

Branch `wave10e/lane-drag`, cut from `wave10/base` @ `c22d952` (verified with
`git merge-base --is-ancestor` before a line was written).

| | |
|---|---|
| subject | `?BeginDragDrop@UserInterface@UI@@QEAAXV?$InternalPtr@VDialogInstance@UI@@@Object@@V?$InternalPtr@VElement@UI@@@4@HHIPEAX@Z` |
| retail | `0x18011f5f0`, **15 instructions**, 63 bytes, no branches |
| port | `runtime/src/ui/userinterface_dragstart.cpp` |
| switch | `BANISHED_RT_UIDRAGSTART` (default-on kill switch) |
| verdict | **PORTED AND DIFFERENTIALLY VERIFIED** — see §4 |

---

## 1. It was absent, and that was established before writing anything

`reference_grep_for_the_port_before_writing_it.md`, and its sharper twin
`reference_half_registered_port_reads_as_absent.md`: a body ported but never
REGISTERED is indistinguishable from an absent one, and a duplicate registration
is last-binding-wins, which silently makes one body dead code.

Searched by **address** (`18011f5f0`) and by the **bare identifier**
(`BeginDragDrop` — the spelling that also finds split string literals, which is
how lane UIF6 nearly missed `BeginToolTip`) across `code/`, `runtime/`,
`engine/`, `standalone/`. **Thirteen hits, all in `standalone/mouse_inject.cpp`,
every one a CONSUMER** — the typedef, the `ResolveExport` call site, and prose.
No definition, no `RegisterRuntimeExport`, no `RegisterRuntimeAddr`.

The census agreed independently, before the file existed:

```
export,?BeginDragDrop@UserInterface@UI@@...,UI,0,0,standalone/mouse_inject.cpp
                                               ^ported=0  ^detoured=0
```

---

## 2. The listing was RE-DERIVED, and lane MOUSE's count was wrong

The brief warned that Wave 10D shipped two lane docs whose instruction counts
were wrong in 4/4 and 2/2 checked cells while every byte size beside them was
correct, and that MOUSE's own published count for this function was **11**.

**It is 15.** Counted with an instrument and not by eye — `objdump` over the
shipped `Runtime-x64.dll`, `grep -c` on the address column, run twice, both
times 15. The body is `0x18011f5f0..0x18011f62e` inclusive, **63 bytes**, and
`0x18011f62f` begins `INT3` padding, so the tail `JMP` is the last instruction
and there is no second basic block.

**Everything else in WAVE10D_LANE_MOUSE.md §6 was right** — the field map, the
read-only element, the tail-JMP target, the `SUB R9D,[RAX+0x68]` fault address.
The analysis was sound and only the number summarising it was wrong, which is
exactly the shape the brief predicted.

```
18011f5f0:  48 8b 02              mov    (%rdx),%rax          ; *dlgPtr
18011f5f3:  8b 54 24 28           mov    0x28(%rsp),%edx      ; arg5 y
18011f5f7:  48 89 41 60           mov    %rax,0x60(%rcx)      ; _dragDropDialog
18011f5fb:  8b 44 24 30           mov    0x30(%rsp),%eax      ; arg6 id
18011f5ff:  89 41 70              mov    %eax,0x70(%rcx)      ; _dragDropId
18011f602:  48 8b 44 24 38        mov    0x38(%rsp),%rax      ; arg7 data
18011f607:  48 89 41 78           mov    %rax,0x78(%rcx)      ; _dragDropData
18011f60b:  49 8b 00              mov    (%r8),%rax           ; *elemPtr
18011f60e:  44 2b 48 68           sub    0x68(%rax),%r9d      ; x -= _layoutLeft
18011f612:  44 89 89 80 00 00 00  mov    %r9d,0x80(%rcx)      ; grab offset X
18011f619:  49 8b 00              mov    (%r8),%rax           ; *elemPtr AGAIN
18011f61c:  2b 50 6c              sub    0x6c(%rax),%edx      ; y -= _layoutTop
18011f61f:  89 91 84 00 00 00     mov    %edx,0x84(%rcx)      ; grab offset Y
18011f625:  ba 02 00 00 00        mov    $0x2,%edx            ; sound id 2
18011f62a:  e9 a1 2d 00 00        jmp    0x1801223d0          ; tail: Play(this,2)
```

### Four things a tidy rewrite would get wrong, all kept

1. **The store order is load-bearing.** `+0x60`, `+0x70` and `+0x78` all commit
   *before* the first `*elemPtr` deref. A null element therefore faults at
   `0x18011f60e` with dialog, id and data **already written** — which is
   precisely the crash lane MOUSE hit and correctly refused to paper over.
   There is no null check in this port (CLAUDE.md), and the order is preserved
   so a future null leaves the same partial state retail leaves.
2. **`*elemPtr` is reloaded** (`0x18011f60b` and `0x18011f619`). Nothing writes
   it in between, so the two reads cannot disagree — but "it cannot differ" is
   an argument about the shipped code, not a licence to change its shape.
3. **The arithmetic is 32-bit.** `SUB 0x68(%rax),%r9d` is a 32-bit signed
   subtract, **not** a MOVSXD-widened one. Read off the disassembly, not off
   Ghidra's C (`reference_ghidra_c_output_vs_disassembly.md` records MOVSXD
   printed as a zero extension as a live Wave 6 defect class).
4. **`+0x80` is two int32s here, not a pointer** — §3.

### The tail call pulled in nothing, as predicted

`?Play@UserInterface@UI@@QEAAXH@Z` (`0x1801223d0`) is **already ported and
registered** — `runtime/src/ui/userinterface.cpp:814` (body) and `:2111-2112`
(both doors). **Confirmed, not assumed.** Its census row was `ported=1` before
this lane and `ported=1` after, with an unchanged caller list.

It is reached through `RT_EXPORT_ONCE` on the same `kPlay` spelling
`userinterface_frame.cpp:370` already uses, rather than by a direct C++ call to
`RtUi_UserInterface_Play`. That keeps the kill switches honest: if Play's own
switch is ever off, this body reaches the DLL's Play exactly as retail's rel32
JMP does.

> **A census blind spot, recorded rather than worked around.** `kPlay` is a
> named constant, so `tools/runtime_census.py` cannot see this call site
> (`reference_census_cannot_see_nonliteral_registrations.md`). Neither can it
> see `userinterface_frame.cpp`'s — that file is likewise absent from the Play
> row's caller list. It is harmless *here* only because the row is already
> `called` by literal-spelled sites and already ported. The **registration** in
> this lane's file is spelled as a literal for exactly this reason.

---

## 3. `_dragDropUserData` is two int32s at its writer — header comment, not a retype

`UserInterface.h` declares `void* _dragDropUserData` at `+0x080`, and for its
two existing call sites that is right: `userinterface_hover.cpp:291` and
`userinterface_frame.cpp:539` both **clear the whole qword**. But the *writer* —
this function — fills `+0x80` and `+0x84` as a pair of int32 **grab offsets**
(where inside the dragged element the cursor took hold).

The two readings are consistent: the pair covers all eight bytes, so a qword
clear zeroes both halves. **The shared header was not retyped** — a `void*` is
what the two clearing sites need, and retyping a field across a lane boundary
mid-wave is how a shared struct corrupts silently. The dual use is documented at
the field, and the port writes through an explicit int32 view of its address.

---

## 4. Verification — a real oracle, not an entry counter

Lane MOUSE's injector (`BANISHED_MOUSE_INJECT`) resolves `BeginDragDrop`
**by name**, and `ResolveExport` consults the port registry first. So the moment
this port registers, the injector's own call lands in our body — and disarming
`BANISHED_RT_UIDRAGSTART` sends the identical call to the DLL instead. **That is
an ours-vs-theirs A/B on one env var, with everything else byte-identical.**

Driver: `tools/drag_run.sh`, run under `tools/game_dir_lock.sh` (never piped —
`reference_a_piped_acquire_discards_the_refusal.md`). Logs read from the
`--snapshot` copy in `lane_drag_logs/`.

### 4.1 The field oracle

Added by this lane at `standalone/mouse_inject.cpp` (after lane MOUSE
published), **outside** the switch, so it reads the same five fields back
whichever implementation just wrote them. Neither side is asked for its own
verdict; both are asked for their bytes — so this is not a shadow self-check
(`reference_shadow_marker_is_not_an_oracle.md`).

### 4.2 The first result was DEGENERATE, and that is the interesting part

The first A/B agreed on all five fields — and read `grabX=20 grabY=20`.
`FindTarget` always aims at the element **centre**, and the toolbar button is
40x40, so the grab offset is `(width/2, height/2)` = `(20,20)`. **A square
subject makes the oracle blind to the one defect it exists to catch:**
transposing the `+0x80` and `+0x84` stores turns `(20,20)` into `(20,20)`.

That is `reference_uniform_population_is_not_a_verification.md`, caught before
it was reported rather than after. Reporting that first run as a pass would have
claimed coverage of a defect class the instrument could not see.

The fix was to break the symmetry deliberately: `BANISHED_MOUSE_DRAGSKEW=dx,dy`
shifts the drag's aim point by an asymmetric delta, identically in every cell —
it changes what the oracle can **see**, not what either side **computes**.

### 4.3 The scored result

`BANISHED_MOUSE_DRAGSKEW=3,-7`, target `#time`, seed 12345, otherwise identical
to lane MOUSE's `all_det` cell.

| cell | who ran `BeginDragDrop` | `dlgMatchesArg` | `id` | `dataIsNull` | **grabX** | **grabY** |
|---|---|---|---|---|---|---|
| `skew_on` | **OURS** | 1 | `0x4D4F5553` | 1 | **23** | **13** |
| `skew_off` | **THEIRS (the DLL)** | 1 | `0x4D4F5553` | 1 | **23** | **13** |
| `skew_swap` | OURS, stores transposed | 1 | `0x4D4F5553` | 1 | **13** | **23** |

* **`skew_on` == `skew_off` on every field the function writes.** `+0x60`,
  `+0x70`, `+0x78`, `+0x80`, `+0x84`.
* **The arithmetic checks out independently:** `argX=2138, layoutLeft=2115 ->
  23`; `argY=1341, layoutTop=1328 -> 13`.
* **`23 != 13`,** so the comparison had something to compare.
* **POSITIVE CONTROL CAUGHT.** `skew_swap` (`BANISHED_DRAG_CONTROL=swap`, which
  deliberately transposes the two stores in our body) **diverges** from
  `skew_off`. The oracle can see the defect it was built for. Without this the
  agreement above would be an assertion, not a measurement.

### 4.4 Entry count, read as a RATIO against an anchor

Absolutes scale with the injector script, so a predicted absolute turns a script
change into a false divergence. The anchor is the injector's own `dragStarted`
counter — the calls it made, which the switch does not affect.

| cell | `BeginDragDrop entries` | `dragStarted` (anchor) | ratio |
|---|---|---|---|
| port armed | **1** | 1 | **1.00** |
| port disarmed | **0** | 1 | **0.00** |

`nonZeroGrabOffset=1` is the effectiveness control
(`reference_probe_needs_an_effectiveness_control.md`): it says the `*elemPtr`
deref did real work — the element had a non-zero layout origin — rather than
subtracting zero.

**The kill-switch control is what makes the counter an instrument.** The
injector called `BeginDragDrop` exactly as often in both cells (`dragStarted=1`),
but our counter went `1 -> 0`, and the UI registration count went `206 -> 205`.
So `entries` tracks *this body*, not the injector's willingness to call
something.

### 4.5 The downstream A/B

Every downstream row read **identically** across the switch, in both the square
and the skewed pairs — these are the observable consequences of the drag our
body starts, produced by code the switch does not touch:

```
IsInsideClipRegion  entries=0 inside=0
EndDragDrop         entries=1 destroyedElement=0
EndHover            entries=2 pushedEvent0x11=2
BeginToolTip        entries=2 rejected=0 closedPrev=0 nonEmptyString=0 allocFailed=0
SCREENS  S1=240  S2=240  S3=160  S4=129  S5=129
```

Every cell: `rc=0`, `[Shutdown] done`, **0-byte crash log**.

### 4.6 What is NOT verified, stated plainly

* **The tail `Play(this, 2)` side effect is not observed.** No counter reports
  the audio-scene queue. What *is* established: the body is straight-line with
  no early exit, it ran to completion (the `+0x84` store is visible in the field
  dump and the function returned), and a null `play` would have died as
  `EXEC-of-0` in the VEH handler against a 0-byte crash log. So the call was
  made with a non-null resolved pointer. Whether the *sound* matches is Play's
  own row, which is separately ported and separately owned.
* **One drag per run.** The route produces a single entry; this is a coverage
  breadth limit, not a correctness one, and it is the same limit lane MOUSE
  reported.

---

## 5. MOUSE's open question — a bounded answer, and it is sharper than before

MOUSE could not reach `EndDragDrop`'s destroy arm because "`BeginDragDrop` never
sets `_dragDropElement`; whatever API does is unidentified."

Having the full listing settles the first half **from the code rather than from
a crash**: all fifteen instructions are accounted for above and **not one
touches `+0x68`**. The element argument is strictly read-only — two loads, two
field reads, no store.

A bounded scan then went further. Disassembling the **entire UserInterface
implementation range** (`0x18011f400`–`0x180122800`) and filtering for stores to
`+0x68` on any base register (stack frames excluded) gives **five store
instructions at four logical sites, and every one of them writes ZERO**:

| site | what it is |
|---|---|
| `0x180120e25` | the constructor's zero-fill (`+0x00`..`+0xc8`, `rax = 0`) |
| `0x180120b5a`, `0x180120b5e` | `EndDragDrop`'s teardown (duplicated tail) |
| `0x18012182b`, `0x18012182f` | `UserInterface::Update`'s **inline** end-drag block (`r13` is the zero register throughout; same duplicated-tail shape) |

**So no `UserInterface` member ever stores a non-null element into `+0x68`.**
That upgrades MOUSE's statement from "BeginDragDrop does not set it" to "nothing
in this class sets it", which means the destroy arm's unreachability is a
property of the class, not a gap in the injector.

Both readers destroy through **vtable slot 0 with flag 1** (the *deleting*
destructor), so whoever does set it is transferring **ownership** of a heap
element to the UI — the setter must write `UI+0x68` from outside the class.

**The lane stopped here, as instructed.** Identifying that external writer would
require a whole-DLL scan for stores to `+0x68` across every object type, which
is not scopeable as a by-product.

---

## 6. Build, and the symmetric hazard actually firing

* **Debug** green, 30,755,426 bytes.
* **Release** green from a **genuine `make clean` first**, 8,165,362 bytes.
* **The symmetric hazard the brief named fired, and was caught by the size
  check.** Running `standalone-debug` straight after `standalone` printed no
  `Built` line and left the **release** binary (8,165,362) sitting at the debug
  path. Only `make clean` then `standalone-debug` produced a real 30 MB debug
  binary. Checking the byte size is what made this visible.
* Four `-Wunused-variable` warnings the field dump introduced in **release
  only** — `standalone/log_strip.h` `#define`s `printf` away, so its consumers
  vanish — are silenced with `(void)` casts rather than left to accumulate.
* The measured binary and the final tree differ only by those four `(void)`
  casts, which are provably inert; the debug binary is byte-identical in size
  (30,755,426) before and after.

---

## 7. Census SET diff — where and when it was measured

Both snapshots taken with **this worktree's own copy** of
`tools/runtime_census.py` (it roots its scan at `REPO`, derived from the script
path, so it measures this checkout and not the cwd —
`reference_scanner_walks_cwd_not_script.md`).

* **before**: on `wave10/base` @ `c22d952`, *before any file was created*.
* **after**: on the finished source, before commit.
* Archived: `docs/runtime_port/wave10e_lane_drag/census_{before,after}.csv`.

The **set** diff, not the count (`reference_diff_the_set_not_the_count.md`):

```
rows before=1107  after=1107

ENTERED the census:  (none)
LEFT the census:     (none)
ported flag CHANGED:
  export ?BeginDragDrop@UserInterface@UI@@...  0 -> 1
detour flag CHANGED: (none)
```

| namespace | before | after |
|---|---|---|
| **UI** | 183 / 186 = **98.4%** | 184 / 186 = **98.9%** |
| TOTAL | 888 / 1107 = 80.2% | 889 / 1107 = **80.3%** |

**Nothing new entered the census** — the one callee this port introduces (`Play`)
was already both `called` and `ported`, so the port cost nothing on the cutover
surface. That is what the brief predicted, and it was confirmed rather than
assumed.

---

## 8. Files

| file | what |
|---|---|
| `runtime/src/ui/userinterface_dragstart.cpp` | **new** — the port, both doors, entry + effectiveness counters, the `swap` positive control |
| `runtime/src/ui/rt_ui_port.cpp` | wired `RtUiDragStartRegister()` in, with the rationale for a separate switch |
| `runtime/types/ui/UserInterface.h` | comment only — the dual reading of `+0x080`. **No retype.** |
| `standalone/mouse_inject.cpp` | the `DRAGFIELDS` oracle and `BANISHED_MOUSE_DRAGSKEW` |
| `tools/drag_run.sh` | **new** — the cells, modelled on `tools/mouse_run.sh` |

### Switches

| var | effect |
|---|---|
| `BANISHED_RT_UIDRAGSTART=0` | leave `BeginDragDrop` on the DLL (default-on kill switch) |
| `BANISHED_MOUSE_DRAGSKEW=dx,dy` | shift the drag aim point, to break a square subject's symmetry |
| `BANISHED_DRAG_CONTROL=swap` | **positive control only** — transpose the two grab offsets. Never in a subject cell. |
