# WAVE 23 LANE TAIL — the long tail: 18 rows across five namespaces

Branch `wave23/tail`, cut from `wave23/base` at `a60351e3`. Private worktree.
**Every figure in this document is BRANCH-LOCAL** (kickoff rule 13): measured in this
lane's own worktree, which contains no other Wave 23 lane's work. The integrator must
re-measure on the merged tip — Wave 21 published from lane reports and had to correct
itself, and in Wave 22 the merged System figure matched neither lane that moved it.

---

## 0. HEADLINE

**Six of the eighteen rows closed, across four of the five namespaces. Zero DATA rows
promoted, and that is the finding, not a shortfall.** All five `?_singleton@` rows were
decided individually on their own xrefs and all five came out HARMFUL-with-the-DLL and
**REQUIRED at cutover** — the route-scope inversion, four rows at once.

The lane's most transferable result is not a port. It is that **the census CSV cannot be
used to ask "is this function ported?"** — it is keyed on `called`, so a body that is
registered but never called by our tree reads as absent. That mis-classified **seven** of
the readers this lane's DATA verdicts depend on. §6 has it.

Branch-local census movement:

| | base `a60351e3` | this branch | delta |
|---|---|---|---|
| TOTAL called | 1314 | **1314** | **0** |
| TOTAL ported | 1137 | **1143** | **+6** |
| TOTAL % | 86.5% | **87.0%** | +0.5 |
| gap rows | 177 | **171** | −6 |
| of which a function to reimplement | 42 (24%) | **36 (21%)** | −6 |

`called` did NOT move. That is deliberate and is checked, not assumed — see §1.3.

| namespace | gap before | gap after | rows closed |
|---|---|---|---|
| Audio | 7 | **5** | `?Stop@Device@Audio@@`, `??1Configuration@Audio@@` |
| UI | 5 | **3** | `?EndAddingSprites@SpriteCapture@UI@@`, `?Set@SpriteSheet@UI@@` |
| Input | 4 | **3** | `??1Configuration@Input@@` |
| Save | 1 | **0** (100.0%) | `?Get@SaveLoad@Save@@` |
| Application | 1 | **1** | none — refused, with evidence (§5) |

Build: `mingw32-make standalone-debug` clean, `reimpl-x64.exe` produced.
`tools/dispatch_slot_map.py --check`: **0 mismatches, 0 double registrations.**
`tools/port_bypass_audit.py`: 32 findings, **none in any file this lane touched**.
`tools/export_name_audit.py`: 3 phantoms, the same 3 CLAUDE.md already records.

**NO GAME-DIRECTORY RUN WAS OBTAINED.** §7 says exactly what that does and does not cost.

---

## TASK 0 — the grep-before-you-port sweep (kickoff rule 2)

All eighteen names were grepped across `runtime/`, `engine/`, `code/` and `standalone/`
before a line was written. **Result: no hidden bodies.** Every hit was a *call site* or a
*comment*, never an unregistered implementation. Cross-checked two ways: the census CSV
reports `ported=0` for all eighteen, and a targeted search for the naming convention a
body would have used (`RtAudio_Device_Stop`, `RtUI_SpriteSheet_*`, `RtSave_SaveLoad_Get`,
…) found nothing.

So the `?StrCompare@String@System@@` shape — a correct body unregistered for twenty waves
— **did not recur in this set**. Stating the negative because rule 2 asks for the check,
not for a finding.

---

## 1. AUDIO — 7 rows, 2 closed, 2 DATA refused, 3 held back

Denominator: the 7 Audio rows named in the kickoff. Re-derived on this tip at kickoff;
the board matched §0 of `WAVE23_KICKOFF.md` exactly.

### 1.1 `?Stop@Device@Audio@@QEAAXPEAUAudioChannel@12@@Z` — **PORTED** (0x1800DE200, ord 4105)

The stated Wave 8 blocker on the Audio Device family was **stale, and re-checking it was
the whole job** (rule 3). Wave 8.5 held this cluster back because "the device IS the
subject, so no synthetic Scene helps". That is an *oracle* verdict. Wave 9 lane IOA had
already acted on the distinction and ported `Device::Update` and
`Device::FreeChannelPlatform` into `runtime/src/audio/audio_device.cpp` — so one of
`Stop`'s two callees was **already ported nine waves before this lane looked**.

The other callee decided the row:

> **`?StopPlatform@Device@Audio@@AEAAXH@Z` (0x1800DEA20) IS EXPORTED.**
> `AEAA` is the PRIVATE mangling, and this DLL exports its private members
> (`reference_aeaa_is_private_not_unexported.md`). Confirmed with `pefile` against the
> shipped export directory, not inferred from the mangling.

**A trap avoided, and it is worth more than the row.** Reaching `StopPlatform` with
`RT_EXPORT_ONCE("?StopPlatform@…")` would have been correct, idiomatic — and would have
**added a new called-but-unported row in the same namespace as the one it closed**
(`reference_porting_can_grow_the_cutover_surface.md`). Audio's gap would have read 7 before
and 7 after, and the lane would have reported a closure that netted nothing. So
`StopPlatform` is **ported too**, in the same file, from the same layout constants.

That this worked is *measured*, not argued: **TOTAL `called` is 1314 on both sides of this
branch.** A `called` column that had moved would have been the symptom.

`Stop`'s body is `StopPlatform` → `FreeChannelPlatform` → a five-instruction reap epilogue
that is **byte-for-byte the one `Device::Update` runs** at 0x1800DE560..0x1800DE57B. Lane
IOA had already written that epilogue out in this file's banner as its cross-check for
`Update`; the two are now written to read identically.

Three faithfulness points, recorded at the call sites:
* `StopPlatform` calls the voice's vtable slot **+0xA0**, where `FreeChannelPlatform`
  calls **+0x90** on the *same object*. Not a transcription slip — it is the reason lane
  IOA used this function as its third witness. +0xA0 stops, +0x90 releases; `Stop` does both.
* `StopPlatform`'s closing `OR dword ptr [RAX],0x2` is **unguarded** and sits *outside*
  the `if (voice)`. Left unguarded per CLAUDE.md — a guard would skip the reap, not
  restore it.
* The index is a signed 64-bit divide by 0x28 truncated to `int` **exactly once**, then
  re-sign-extended. Reproduced rather than simplified.

**ORACLE: none, and none is manufactured.** The body ends in a stored completion callback
and a COM stop/release; replaying the DLL's copy fires the callback twice and releases a
released object — a second side effect, not a comparison. This is the Wave 8 root cause
and it is *real*; what it blocks is verification, not the port. `RT_DIFF_WORK` carries what
`calls=` cannot (rule 8).

### 1.2 `??1Configuration@Audio@@QEAA@XZ` — **PORTED** (0x1800DE0D0, ord 554)

The entire retail body is two instructions: `MOV qword ptr [0x1801fe758],0 ; RET`. It
destroys nothing — the caller frees (`engine_lifecycle.cpp` does `dtor(p); MemFree(p)`).

`QEAA` is the **non-virtual** dtor spelling and is the one the export table carries; the
`UEAA` virtual spelling is absent and would resolve NULL silently
(`reference_phantom_dtor_mangling_qeaa_vs_ueaa.md`). Checked with `pefile` before writing.

The port's value is not the two instructions. **The store now goes through `RT_DATA_SLOT`**,
which converts retail's un-interceptable RIP-relative store into a door. With the DATA row
unpromoted the door *is* the DLL slot, so it is byte-identical today; if the row is ever
promoted (§1.4: at cutover it must be) this writer moves with it instead of being stranded.

### 1.3 What `called` proves, stated once

`?StopPlatform@` is now registered under both doors and is **not** in `called`. It
therefore contributes 0 to `ported` as well — the +6 in §0 is exactly the six board rows,
with no credit taken for the bonus body. That is the honest accounting and it is why the
two columns are quoted side by side.

### 1.4 The two Audio DATA rows — **BOTH REFUSED, both route-scoped** (Task 2)

Method for all five DATA rows in this lane (§3, §5 use it too), stated so it can be
re-derived:

1. **Route A** — Ghidra `get_xrefs_to` on the slot, taking its READ/WRITE tags.
2. **Route B** — a decoder-independent scan of every disp32 window in `.text` whose
   RIP-relative target is the slot, at imm widths 0/1/2/4. The imm widths are not
   decoration: `MOV qword ptr [rip+d],0` measures RIP *past* the imm32, and that exact
   encoding made the same scan under-report by one in `reg_mint_promoted.cpp`'s SaveLoad
   section. A second route is only worth running if it *can* disagree.
3. **Classification** — by grepping `RegisterRuntimeExport` in `runtime/`, **not** by the
   census CSV. See §6 for why that distinction decided the outcome.

**Routes A and B agree exactly on all five slots**: 9 / 8 / 24 / 17 / 26 references.

| row | refs | W | R | readers ported | verdict DLL-present | verdict cutover |
|---|---|---|---|---|---|---|
| `?_singleton@Configuration@Audio@@` (0x1801FE758) | 9 | 7 | 2 | **0 of 2** | **HARMFUL** | **REQUIRED** |
| `?_singleton@Device@Audio@@` (0x1801FE760) | 8 | 1 | 7 | 3 of 7 | **HARMFUL** | **REQUIRED** |

* **Configuration@Audio**: both readers are unported —
  `?Get@Configuration@Audio@@` has no body anywhere in the tree, and
  `?Update@Scene@Audio@@` is held back this very wave (§1.5) **and runs every frame**.
  Promoting would strand a live per-frame reader on a slot our ctor/dtor no longer write.
* **Device@Audio**: `PauseEffect`, `StopEffect` and `StopAll` are ported; **four are not**
  (`??1SoundBuffer@Audio@@`, `?Run@Stream@Audio@@`, `?Get@Device@Audio@@`,
  `?Update@Scene@Audio@@`). A half-ported reader set is still a split brain.

**These two were decided separately and their numbers differ (2 readers vs 7, 7 writers vs
1).** They are not one answer applied twice — the rule Wave 22 lane ERRCFG paid for.

**THE INVERSION.** Both rows are tagged `HARMFUL (DLL-present) / REQUIRED (cutover)`.
With no DLL, the unported readers *do not exist*, and nothing else supplies the storage —
the `?_singleton@FileSystem@IO@@` shape. Both previously carried **no tag at all**, which
is how each successive lane rediscovered them; they are now in
`standalone/reg_mint_promoted.cpp` with `[SCOPE:]` and a named `[ADDRESSEE:]`.

### 1.5 The three Audio rows held back

* **`?Update@Scene@Audio@@`** (0x1800DF6C0, ~150 lines). Wave 8's stated blocker — that a
  replay diverges by construction because the body draws from `System::Random` — **still
  stands, re-checked**. But it is an ORACLE blocker, and lane IOA's precedent in this same
  file (port without an oracle, add `RT_DIFF_WORK`) shows it need not block the port. What
  it needs is a snapshot/restore of the `Random` state: bounded, real, not a missing tool.
  It is left because it is the largest body in the set and this lane preferred to close
  five small rows correctly. **[ADDRESSEE: the Wave 24 kickoff writer — this is Audio's
  last BODY row and it unblocks BOTH Audio DATA rows' reader sets.]**
* **`??0Stream@Audio@@` / `??1Stream@Audio@@`** (0x1800A0E30 / 0x1800A0D90). `Stream` is a
  `System::Thread` subclass. The dtor is portable *except* for `CALL 0x18000C4A0` on the
  member at `+0x30`, which is **not exported** (`pefile`, checked) — an in-DLL call with no
  door, dead memory at cutover. Porting the pair requires porting that helper first.
  **[ADDRESSEE: the Wave 24 kickoff writer.]**

---

## 2. UI — 5 rows, 2 closed, 1 deferred to lane ALLOC, 2 structurally unpromotable

Denominator: the 5 UI rows named in the kickoff.

### 2.1 `?EndAddingSprites@SpriteCapture@UI@@` — **PORTED** (0x1800D8A60, ord 2304)

**A genuinely pure function**: no calls, no allocation, no virtuals, nothing outside the
object. Two passes over `_entries` — a bounding box over non-scissor entries only, then a
rebase over *every* entry including the scissors.

**The degenerate case is reproduced, not repaired.** With zero entries — or with entries
that are all scissors, which pass 1 skips — the seeds survive and retail stores
`_width = _height = −65535`. Clamping would hide retail's behaviour from whatever reads
`_width` next, not restore it.

The whole layout was already in `runtime/types/ui/SpriteCapture.h` with `static_assert`s;
this lane asserted no new offsets.

**Verifiable by input sweep** rather than by waiting for a route
(`project_pure_functions_verify_by_sweep.md`). **The sweep was NOT run** — it needs no game
directory, so this is a gap in this lane's evidence and not a blocked one. §7.2.

### 2.2 `?Set@SpriteSheet@UI@@QEAAXHHAEBVString@System@@0_N@Z` — **PORTED** (0x1800034F0, ord 3819)

Five parameters, two of which arrive on the stack. **The argument mapping was read off the
prologue, not deduced from the mangling** — retail shifts the frame by 0x28 before reading
`[RSP+0x50]` and `[RSP+0x58]`. The assignment order is *not* source order: `+0x40` is
assigned from the 4th parameter **first**.

Two things in the String member-assign idiom look like transcription errors and are not,
both reproduced and both commented in place:
* an `else this->field = nullptr;` branch that is a **dead store** in every
  single-threaded reading — overwritten two instructions later;
* the free goes **through the member** (`ADD qword ptr [this+0x40],-8`, then that member
  loaded back as `MemFree`'s argument), leaving the member transiently pointing at the
  allocation base.

Only callee: `?MemFree@MemoryAllocator@System@@SAXPEAX@Z` (0x1800C7AD0, confirmed by
`pefile`).

**BONUS, and deliberately not acted on.** This function *names* three fields that
`runtime/types/ui/SpriteSheet.h` carries as `m_field48` / `m_field4C` / `m_field50`
"unknown int / unknown int / unknown byte": they are precisely this setter's two ints and
its bool. **The header was NOT renamed.** One setter is not enough evidence for a *name* —
`reference_a_writer_beats_readers_agreeing_on_a_sign.md` cuts the other way here, and this
lane has no reader to corroborate against. Recorded so a future lane starts from it.
**[ADDRESSEE: the Wave 24 kickoff writer — hand this to whichever lane next reads
SpriteSheet; it needs one reader, not a new disassembly.]**

### 2.3 `??2SpriteSheet@UI@@SAPEAX_K@Z` — **HELD BACK, and handed to lane ALLOC**

`operator new`. The whole body is: `EnterCriticalSection(alloc+0x640)`,
`?Allocate@MemoryAllocator@System@@QEAAPEAX_KH@Z(alloc, size, 8)`,
`LeaveCriticalSection`. Both callees are satisfiable; **this is not a technical blocker.**

It is held back because it is **squarely lane ALLOC's subject** and `WAVE23_KICKOFF.md`
§1a puts the allocator out of bounds while ALLOC is still running against the same repo.
Porting a UI `operator new` that routes through the allocator singleton, while another
lane is actively changing how that singleton is reached, is how two correct lanes produce
one broken tip.

**[ADDRESSEE: lane ALLOC (Wave 22), via the integrator.]** Two concrete items:
1. This row is ready the moment the allocator door is settled. Body shape above; the
   singleton is at `0x1801FC718`, offset `+0x640` is the critical section.
2. **A door question this lane deliberately did not answer.** `RtUi_SpriteSheetAssignString`
   frees through `RT_ORIGINAL_ONCE` — the *raw* door — to match `ReleaseStringRef` and the
   other String sites in `runtime/src/ui/sprite.cpp`, so that every String block in that
   file is allocated and freed through the **same** door. `rt_port.h`'s standing advice is
   the opposite (`RT_EXPORT_ONCE` for a dependency). **That tension is real and the whole
   file has it**, not just this lane's two call sites. Mixing doors inside one refcounted
   block is the hazard; which door the file should settle on is ALLOC's call, not TAIL's.

### 2.4 `??_7Dialog@UI@@6B@` and `??_7DialogController@UI@@6B@` — **UNPROMOTABLE, and one tag corrected**

Rule 4 applies in full and both were re-checked rather than inherited.

* **`??_7DialogController@UI@@6B@`** — permanently unpromotable. Wave 10E lane FINAL gives
  four independent proofs, including one that does not depend on reachability at all: the
  single consumer's stamp is **overwritten, unread, by the first statement of
  `??1Internal@Object@@`**. Nothing here to revisit; no route should be hunted.

* **`??_7Dialog@UI@@6B@` — ITS EXPIRY READS AS FIRED AND HAS NOT.** This is the item lane
  OBJ handed to this lane, and OBJ was right to hand it over rather than edit a tag
  outside its own rows. The tag read:

  > `… | either Dialog factory ported+registered`

  Both factories now **have bodies and `RegisterRuntimeExport` calls** in
  `runtime/src/ui/dialog_factory.cpp`. **A grep says the expiry fired. It has not**, for
  two independent reasons:
  1. **The registration is behind `BANISHED_RT_DLGFACTORY` and is opt-in.** A gated
     registration is not a port: the code exists, the grep finds it, and it never runs
     unless armed. `runtime_census.py` does not count it and neither should a tag. This is
     the same shape Wave 22 lane ALLOC hit from the other direction, and **nothing in the
     tree detects a gated port** — only a person grepping the gate.
  2. **The expiry was wrong when it was written.** Wave 11 lane OBJINT was *the experiment
     that tested it* and found the factory is **necessary but not sufficient**: the
     creator must also hold a `BaseTypeInfo` whose `factoryFn` is *our* pointer, and that
     minted node must be in the AVL `Factory::Startup` walks — blocked by the
     `?_firstTypeInfo` split brain.

  The tag is rewritten to name **all three** conditions, with `[SCOPE:]` and
  **[ADDRESSEE: the Wave 24 kickoff writer — do not re-scope a lane onto this row until
  the `_firstTypeInfo` split brain is closed]**.

* **"Unpromotable" is not "nothing to do"** (lane OBJ's second point, applied here). Both
  UI vtables are resolved by name from live code — `??_7Dialog@UI@@6B@` from four files,
  `??_7DialogController@UI@@6B@` from `dialog.cpp`'s dtor. **With no DLL those name doors
  resolve `nullptr`.** So both rows, though never *promotable*, must be **published** at
  cutover. This lane did **not** do that publishing work and does not claim to have
  scoped it. **[ADDRESSEE: the lane that first boots without Runtime-x64.dll.]**

---

## 3. INPUT — 4 rows, 1 closed, 2 DATA refused, 1 held back

Denominator: the 4 Input rows named in the kickoff.

### 3.1 `??1Configuration@Input@@QEAA@XZ` — **PORTED** (0x1800E0E10, ord 555)

`MOV qword ptr [0x1801fe768],0 ; RET`. The exact twin of the Audio dtor in §1.2, down to
the encoding, differing only in which slot it nulls.

**They were still read separately.** A two-instruction body is precisely the shape where
"same as the other one" gets assumed instead of checked, and this lane's brief forbids
letting one namespace's answer stand in for another's. Both were disassembled at their own
addresses and both manglings checked independently with `pefile`.

Registered from `RtInputRegister()` and **not** from `RtInputConfigStartupRegister()`,
which is gated behind `BANISHED_RT_INPUTCFGSTART` — a gated registration is not a port
(§2.4), and putting it there would have produced exactly the defect this lane just
corrected in someone else's tag.

### 3.2 `?PlatformShutdown@Device@Input@@AEAAXXZ` — **HELD BACK** (0x1800E1B50, ord 3395)

Structure fully recovered: it walks the red-black tree at `Application::_singleton + 0x18`
three times (keys `7`, `8`, `0x20A`), erasing a message handler each time, then Unacquires
(`vtbl+0x40`) and Releases (`vtbl+0x10`) three DirectInput COM objects at `+0x11E8`,
`+0x11E0`, `+0x11D8`.

**Blocker, stated precisely:** the erase is `0x18000ABA0` — a full red-black-tree erase
with rebalancing, **not exported** (`pefile`), and it calls two *further* unexported
helpers (`0x180009C30`, `0x18000ADC0`). That is three unported, un-doored in-DLL bodies,
all dead memory at cutover. This is a real dependency chain, not an oracle complaint.
**[ADDRESSEE: the Wave 24 kickoff writer — this row is gated on an RB-tree erase port,
which is a `Collection`-namespace job and is likely shared with other callers; scope it
there, not in Input.]**

### 3.3 The two Input DATA rows — **BOTH REFUSED**

Same method as §1.4; Routes A and B agree.

| row | refs | W | R (sites/distinct) | readers ported | DLL-present | cutover |
|---|---|---|---|---|---|---|
| `?_singleton@Configuration@Input@@` (0x1801FE768) | 24 | 7 | 16 / 13 | 4 of 13 | **HARMFUL** | **REQUIRED** |
| `?_singleton@Device@Input@@` (0x1801FE770) | 17 | 1 | 16 / 14 | 4 of 14 | **HARMFUL** | **REQUIRED** |

* **Configuration@Input** has the strongest refusal of the five: **three of its unported
  readers are unexported DLL internals** (`FUN_18000DB80`, `FUN_18000DD60`,
  `FUN_1800A1DC0`). Those have no door and can *never* be ported by name, so this row
  cannot be promoted by any amount of ordinary port work while the DLL is present.
* **Device@Input** is the row that most rewards being decided on its own xrefs:
  `?Pressed@KeyMap@Input@@` and `?GetValue@KeyMap@Input@@` are **ported**, while their
  siblings `?Released@KeyMap@Input@@` and `?GetNormalizedValue@KeyMap@Input@@` are **not**.
  A verdict reached by looking at the *class* rather than the *slot* would have been wrong
  in either direction.

Both are newly tagged in `reg_mint_promoted.cpp` with scope and addressee; neither carried
a tag before.

---

## 4. SAVE — 1 row, closed; namespace now 100.0%

### `?Get@SaveLoad@Save@@SAPEAV12@XZ` — **PORTED** (0x180030D40, ord 2425)

Two instructions: `MOV RAX,[0x1801FC6F0] ; RET`.

**This row exists because our own diagnostic created it, and that is said plainly rather
than dressed up.** `standalone/rt_orphan_probe.cpp` names it as a string literal, which put
it in the census's `called` column; that file says so itself. Nothing in any shipped binary
calls it. **Re-derived independently rather than inherited** (rule 3):

* Runtime-x64.dll: `get_xrefs_to(0x180030D40)` returns exactly two references —
  `0x1801B3028 [DATA]` (a `.pdata` unwind record) and the export directory. **No call site.**
* Application-x64.exe (899 imports), VideoDX11-x64.dll (163), VideoDX9-x64.dll (146):
  absent from all three import tables. Checked with `pefile` in this lane.

**So why port it at all?** One reason, and it is not cosmetic:
`?_singleton@SaveLoad@Save@@` **is promoted** (census `ported=1`), so our storage is live
and the DLL's `.data` copy is stale. Retail's `Get` is an un-interceptable RIP-relative
load of that stale copy — the very fact `reg_mint_promoted.cpp` leans on to argue the
promotion is safe. Reading through `RT_DATA_SLOT` makes **our** `Get` return the live
pointer, so the symbol is correct at cutover instead of resolving null.

**THE ORACLE WAS CHECKED, NOT ASSUMED.** `ProbeSaveLoadSingleton` uses retail's `Get` as
its oracle. That is exactly the shape that went dark for five waves in Wave 5's
`Goal::Serialize` probe, where the ours-side reached the body **by address** and so
compared retail with itself. It survives here because the probe resolves `Get` with
**`ResolveExportRaw`**, which skips the port registry by construction — verified by reading
`rt_orphan_probe.cpp:2145`, not by assuming the macro's name. The **address** door is the
one that could have broken it; it does not, because the probe never resolves
`0x180030D40` by address. Both doors were checked against that file before either was added.

---

## 5. APPLICATION — 1 row, REFUSED; and its existing tag corrected

### `?_singleton@Application@1@0PEAV11@EA` (0x1801FC678) — **NOT PROMOTED**

26 references, both routes agreeing: **2 WRITE, 24 READ** across 12 distinct readers. Five
readers are ported; **seven are not**, including `?WindowProc@Application@1@`'s siblings
`?Update@LoaderLoop@Engine@@` and `?Update@DialogLoaderLoopT@Engine@@`,
`?PlatformGetKeyboardState@Device@Input@@`, `?PlatformGetMouseState@Device@Input@@`, and
`?PlatformShutdown@Device@Input@@` — **this lane's own held-back row from §3.2**.

The existing refusal is correct and is **re-confirmed, DLL-present**. But its tag read:

> `… | NONE, EVER | HARMFUL`
> *"EXPIRY: none. A future lane that promotes this introduces a defect."*

**That is unqualified, and it is wrong at cutover.** With no DLL, all 24 in-DLL readers
cease to exist and nothing else supplies the storage: the row is not merely promotable
there, it is **required**. This is the same inversion lane OBJ found in four of its own ten
rows, and the `?_singleton@FileSystem@IO@@` precedent
(`reference_unpromotable_is_route_scoped_not_permanent.md`) is the named case. Writing
"NONE, EVER" filed a cutover task as a permanent refusal.

Tag corrected in place to `HARMFUL (DLL-present) / REQUIRED (cutover)` with
`[SCOPE: the HARMFUL verdict is DLL-PRESENT ONLY]` and
**[ADDRESSEE: the lane that first boots without Runtime-x64.dll]**.

---

## 6. THE FINDING THAT OUTLIVES THE LANE

> **The census CSV cannot answer "is this function ported?", and it fails SILENTLY in the
> direction that makes refusals look better-supported than they are.**

`tools/runtime_census.py --csv` emits one row per symbol **our tree calls**. A body that is
written and `RegisterRuntimeExport`-ed but that our tree never *calls* is simply not in the
file. Ask it "is X ported?" and an absent row is indistinguishable from `ported=0`.

This lane's first pass at the DATA verdicts classified readers against that CSV and
**mis-reported seven of them as having no body at all**:

`?PauseEffect@Scene@Audio@@`, `?StopEffect@Scene@Audio@@`,
`?OnFocusMessage@Device@Input@@`, `?OnLostFocusMessage@Device@Input@@`,
`?OnMouseWheelMessage@Device@Input@@`, `?WindowProc@Application@1@`,
`?Get@Application@1@`.

The fix was to classify by grepping `RegisterRuntimeExport` in `runtime/` instead.

**Why this was nearly invisible.** Every one of the seven errors inflated the
*unported-reader* count, i.e. pushed towards **HARMFUL** — which is the conclusion all five
rows reached anyway. A wrong instrument agreeing with the right answer leaves no symptom.
It was caught only because the count for `?_singleton@Device@Audio@@` was checked against a
direct grep of `audio.cpp`, where `PauseEffect` and `StopEffect` are plainly registered.
**The verdicts in §1.4, §3.3 and §5 are the corrected ones**, and they still hold — but
they hold on 4-of-7, 4-of-13, 4-of-14 and 5-of-12, not on the CSV's inflated figures.

Two secondary blind spots, recorded because a screen that cannot name its subject
manufactures work:

* **The gate detector is file-scoped.** `regcheck.py` flags a registration whose *file*
  mentions `RtWaveEnabled`/`BANISHED_*`. It flagged `?Get@Application@1@` in
  `rt_application_port.cpp`; reading the actual line showed that registration is **ungated**
  (the gates belong to other blocks in the same file). A file-level gate screen
  over-reports and must be followed by reading the line.
* **The disp32 scan attributes a hit to the nearest preceding export**, which is a *lower
  bound* on function identity, not the function. For hits deep inside unexported internals
  it produced nonsense like `??4Modification@IO@@…+0x1BCC`. Those are exactly the sites
  Ghidra calls `FUN_18000F6F0` etc. **Both routes agree they are not exported functions**,
  which is all the verdict needs — but the scan's own attribution for them is wrong and is
  reported here rather than quoted as a finding. Per lane OBJ's warning, the unclassifiable
  bucket was read, not just the totals.

---

## 7. WHAT THIS LANE DID **NOT** CHECK (rule 10)

### 7.1 No game-directory run — stated plainly, with the actual holder

`tools/game_dir_lock.sh acquire --timeout 30` was run **bare, never piped** (a pipeline
reports the last command's status, so a timeout would read as a grant). It returned:

```
[gamelock] DENIED -- TIMEOUT after 30s; queued behind 'VIDVT'.
ACQUIRE_EXIT=1
```

**No run was obtained.** One correction to the coordinator's briefing: the holder at the
time of this attempt was lane **VIDVT**, not lane ALLOC.

**What that costs, precisely.** The six ported bodies are compiled and registered but
**none has been observed executing**. `RT_DIFF_WORK` sites are in place and will
distinguish "entered" from "did real work" on the first run anyone gets — but this lane has
**zero attendance data**, and a green build is not a route
(`reference_a_green_readiness_screen_is_not_a_route.md`). **No claim is made that any of
these six bodies has run.**

The absence of a run is **not** an implied verdict on the ports and must not be read as
one; equally, no verification is claimed from its absence.

### 7.2 Checks that needed no game directory and were still not run

* **The `EndAddingSprites` input sweep.** It is a pure function and the sweep is the right
  oracle (§2.1). It needs only the shipped export and a driver. **Not built, not run.**
  This is the single largest gap in this lane's evidence and it is not excused by §7.1.
  **[ADDRESSEE: the Wave 24 kickoff writer — a sweep harness for this one body is cheap and
  would convert the lane's only pure port from unverified to measured.]**
* **A negative control (rule 9).** None was run, because this lane built no probe to
  control — the DATA work is static analysis, and the six ports have no oracle harness
  attached. Rule 9 asks for a control on a *probe*; there is no probe here. Said explicitly
  rather than left as an unexplained absence.
* **No detour was installed** for any of the six, so the DLL's own internal callers still
  reach the DLL's copies. Correct and intended for this wave; noted because it bounds what
  "ported" means for these rows.

### 7.3 Scope not examined

* The **three Audio holdbacks** (§1.5) and **`?PlatformShutdown@Device@Input@@`** (§3.2)
  were analysed to the point of a stated blocker and no further; no attempt was made to
  port the RB-tree erase or `0x18000C4A0`.
* **Cutover publishing** of the two UI vtable name doors (§2.4) was identified but not
  scoped or costed.
* The `SpriteSheet` field names (§2.2) were derived but deliberately **not** applied.

---

## 8. FILES TOUCHED

| file | change |
|---|---|
| `runtime/src/audio/audio_device.cpp` | + `Device::Stop`, + `Device::StopPlatform`, +5 platform offsets, 2 registrations |
| `runtime/src/audio/audio.cpp` | + `Configuration::~Configuration`, 1 registration |
| `runtime/src/input/input.cpp` | + `Configuration::~Configuration`, 1 registration |
| `runtime/src/ui/spritecapture.cpp` | + `SpriteCapture::EndAddingSprites`, 1 registration |
| `runtime/src/ui/sprite.cpp` | + `SpriteSheet::Set` + its assign helper, 1 registration |
| `runtime/src/save/saveload.cpp` | + `SaveLoad::Get`, 1 registration |
| `standalone/reg_mint_promoted.cpp` | 2 tags corrected (`??_7Dialog@UI@@6B@`, `?_singleton@Application@1@`), 4 tags added |

**No file owned by lane ALLOC was touched** (`allocator.cpp`, `allocator_win.cpp`,
`rt_system_port.cpp`, `tools/runtime_census.py`) — verified against `WAVE23_KICKOFF.md` §1a.

## 9. RESIDUALS, EVERY ONE WITH A NAMED ADDRESSEE (rule 11)

| # | item | addressee |
|---|---|---|
| 1 | `??2SpriteSheet@UI@@` ready to port; **and** the raw-vs-port-aware `MemFree` door question, which is file-wide in `runtime/src/ui/sprite.cpp`, not lane-local | **lane ALLOC (Wave 22), via the integrator** |
| 2 | `?Update@Scene@Audio@@` — needs a `System::Random` snapshot/restore; unblocks both Audio DATA reader sets | **the Wave 24 kickoff writer** |
| 3 | `??0Stream@Audio@@` / `??1Stream@Audio@@` — gated on unexported `0x18000C4A0` | **the Wave 24 kickoff writer** |
| 4 | `?PlatformShutdown@Device@Input@@` — gated on an RB-tree erase port; scope it in `Collection`, not Input | **the Wave 24 kickoff writer** |
| 5 | All five `?_singleton@` rows must be **minted** at cutover (the inversion) | **the lane that first boots without Runtime-x64.dll** |
| 6 | Both UI vtable **name doors** resolve `nullptr` at cutover and need publishing despite being unpromotable | **the lane that first boots without Runtime-x64.dll** |
| 7 | `??_7Dialog@UI@@6B@` — do not re-scope a lane onto it until the `_firstTypeInfo` split brain is closed | **the Wave 24 kickoff writer** |
| 8 | `EndAddingSprites` input sweep — cheap, needs no game directory, would convert this lane's only pure port to measured | **the Wave 24 kickoff writer** |
| 9 | `SpriteSheet` `m_field48/4C/50` are `Set`'s two ints and its bool — needs one reader to corroborate before renaming the header | **the Wave 24 kickoff writer** |
| 10 | **Nothing in the tree detects a gated registration.** A grep for `RegisterRuntimeExport` finds code that never runs. This bit lane OBJ, Wave 22 ALLOC, and this lane's tag correction | **the Wave 24 kickoff writer — this wants a checker, not another lane rediscovering it** |
