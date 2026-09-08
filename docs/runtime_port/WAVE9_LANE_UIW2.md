# Wave 9 lane UIW2 — widget setters and the ElementDescription pair

Branch `wave9/lane-uiw2`, cut from `fd30180`. Date 2026-09-08.

Assignment: five symbols.

| # | Symbol | Retail | Ord | Verdict | Verified? (§9) |
|---|---|---|---|---|---|
| 1 | `?SetParameter@SliderElement@UI@@QEAAXHAEBVString@System@@@Z` | `0x1800d6ca0` | 3936 | **PORTED** | **VERIFIED** 1202/1202, 0 div |
| 2 | `?SetProgressTextParameter@ProgressElement@UI@@QEAAXHAEBVString@System@@@Z` | `0x180119b30` | 3946 | **PORTED** | **UNVERIFIED**, route known |
| 3 | `?SetCurrentItem@ComboElement@UI@@QEAAXH@Z` | `0x180112540` | 3853 | **PORTED** | verified, THIN (4/4) |
| 4 | `?CreateInstance@ElementDescription@UI@@QEBAXV?$InternalPtr@VElement@UI@@@Object@@@Z` | `0x180115ad0` | 2004 | **DECLINED** — §5 | n/a |
| 5 | `?Serialize@ElementDescription@UI@@UEAA_NAEAVStream@IO@@@Z` | `0x180116b00` | 3687 | **DECLINED** — §6 | n/a |

Plus three dependencies ported to keep the closure positive (§2c, §8):

| — | Symbol | Retail | Ord | Verdict | Verified? (§9) |
|---|---|---|---|---|---|
| — | `ComboElement::UpdateText` (private, **unexported**) | `0x180112180` | — | **PORTED**, address-only | verified, THIN (4/4) |
| — | `?CacheText@NumberElement@UI@@AEAAXXZ` | `0x1801189f0` | 1679 | **PORTED**, name + address | **UNVERIFIED** — §9c |
| — | `??9String@System@@QEBA_NAEBV01@@Z` | `0x18003ffe0` | 1290 | **PORTED**, name + address | **weakly verified** (1 real compare) |

**§9 is the verification record and it contains a prediction this document got
WRONG.** Read it before quoting anything here as done.

All five addresses and ordinals are `pefile` output against the shipped export
directory of `C:\GOG Games\Banished\Runtime-x64.dll`, not Ghidra's, and were
checked **before** any body was written
(`reference_resolveexport_phantom_names.md`). All five are present.

---

## 1. The closure set-diff — the number that decides whether this lane helped

> **READ SECTION 8d FIRST FOR THE AUTHORITATIVE NUMBERS.** The table in this
> section was produced by the PRE-FIX `addr_door_audit.py` against base
> `fd30180`, before the integrator fixed that tool to stop matching
> `ResolveAddr` text inside COMMENTS (its denominators moved 295->270 sites
> and 198->192 distinct on an unchanged tree). It is kept as recorded rather
> than retconned, because the phantom-name finding in 1a is a fact about how
> this lane was measured. Section 8d re-measures parts 1 AND 2 with the fixed
> tools against the current integration base `f5dbeb8`.

Measured on two trees identical except for this lane's 564 inserted lines: a
copy of `fd30180`'s `runtime/ code/ engine/ standalone/ dx11/ tools/` with and
without the change. **Both halves were re-measured on the clean `fd30180`
trees** after an earlier pair of numbers turned out to have been taken against a
shared checkout carrying other lanes' uncommitted work — the numbers below are
the ones to quote.

| Audit | base (`fd30180`) | after | delta |
|---|---|---|---|
| `unsatisfied_export_scan.py` | 848 doors, **129** unsatisfied, 126 outside a diff guard | 850 doors, **126** unsatisfied, 123 outside | **−3 unsatisfied**, denominator **+2** |
| `addr_door_audit.py` | 295 call sites, 198 distinct, **16 UNREGISTERED**, 1186 registered | 295, 198, **16**, 1190 | **±0 unregistered**, +4 registered |
| `port_bypass_audit.py` | 334 address sites / 0 bypass; 477 raw-door sites / 0 bypass | **byte-identical output** | 0 |
| `rawdoor_audit.py` | 607 sites, **0 UNGUARDED-CALL**, 66 DEFERRED | **byte-identical** | 0 |
| `detour_target_audit.py` | clean | clean | 0 |
| `abi_retslot_audit.py` | 0 suspicious of 48 | 0 of 48 | 0 |
| `export_name_audit.py` | 670 names, 1 not in the DLL (pre-existing) | 670, 1 | 0 |
| `resolve_cache_audit.py` | **442 FROZEN** | **442 FROZEN** | 0 |
| `runtime_census.py` | UI 153 called / 118 ported (77.1%); TOTAL 1003 / 771 (76.9%) | UI 155 / 121 (78.1%); TOTAL 1005 / 774 (77.0%) | +3 ported, +2 called |

**The SET, not the count:**

```
CLOSED (left the unsatisfied list)
  ?SetCurrentItem@ComboElement@UI@@QEAAXH@Z
  ?SetParameter@SliderElement@UI@@QEAAXHAEBVString@System@@@Z
  ?SetProgressTextParameter@ProgressElement@UI@@QEAAXHAEBVString@System@@@Z

OPENED (entered the unsatisfied list)
  <none>
```

Three closed, none opened, and **no new by-address call site** — 295 and 198 are
unchanged, so nothing moved into the hardest bucket. That property does not come
for free: `reference_porting_can_grow_the_cutover_surface.md` records
`KeyMap::RegisterInput` closing one row and opening two.

**Every denominator was diffed and none shrank.** 848→850 and 1186→1190 both
GREW, which is the honest direction: this lane resolves two more names
(`?_typeInfo@TextElement@`, `?_typeInfo@LabelElement@`, both already carried by
lane C2's mint) and registers four more addresses.

`resolve_cache_audit.py` is unchanged at **442 FROZEN**, which is the specific
check that this lane did not add to WAVE9.md's "caches a null forever on the
first call" class of ~22: every new dependency accessor uses the retry form
(`RT_EXPORT_ONCE` / `RT_DATA_SLOT`), not a frozen function-local static.

### 1a. THE SET-DIFF CAUGHT A PHANTOM NAME THAT THE COUNT WOULD HAVE HIDDEN

The first version of this lane went **129 → 127**, i.e. −2 where −3 was expected.
A lane reading only the headline would have recorded "went down, good". The
entered/left comparison named the culprit immediately:

```
OPENED: ?FindElementType@Element@UI@@QEBA?AV?$InternalPtr@VElement@UI@@@Object@@I@Z
```

The dependency accessor had been written `QEBA` (const member) on the assumption
that a finder is const. **It is not, and that name is not in the export table.**
It would have resolved to NULL in silence, the three ports would still have
registered, `runtime_census.py` would still have counted them, and
`ComboElement::UpdateText` would have called through a null pointer the first
time a combo box had to search for its label. The real symbol is
`?FindElementType@Element@UI@@QEAA?AV…` — the exact string
`runtime/src/ui/element.cpp:152` already uses for the same function.

This is `reference_resolveexport_phantom_names.md` for at least the fourth time
in this project, and the lesson worth carrying is not "check manglings" (everyone
already knows that) but: **the instrument that caught it was the SET-DIFF, and
the instrument that would have missed it was the COUNT.** −2 and −3 look the same
from a distance.

---

## 2. What was ported, and why each was cheap

All three ports live in `runtime/src/ui/widgets_input.cpp` — Wave 7 lane D's own
file, which is where these three symbols were held back — and register through
the existing `RtUiWidgetsInRegister()`. `rt_ui_port.cpp` says in its own header
that it is owned by no lane and must not be edited to add one; it was not.

Kill switch: the existing `BANISHED_RT_UIWIDGETSIN=0`.

### 2a. Three stale blockers, re-checked rather than inherited

`reference_holdback_blockers_are_stale_check_them_first.md`, and it held again —
three of five stated blockers had dissolved:

| Stated blocker (Wave 7 lane D §3 / lane UIF rows 16, 18, 19) | Status 2026-09-08 |
|---|---|
| "the target object belongs to a class this lane does not own or model" | **STALE.** `runtime/types/ui/TextElement.h` has existed since Wave 7 lane E, derived from `TextElement::SetParameter`'s own call site. |
| "`String::operator=`'s exact refcount sequence was not independently confirmed" | **STALE.** Ported and diff-verified in Wave 2 (`runtime/src/system/string.cpp:360`), registered under name and address. |
| "`UserInterface::CloseDialog` — one of this wave's six phantom names, owned by lane C and not yet fixed" | **HALF STALE, and the distinction matters.** The *phantom* is fixed: Wave 7 lane C established the real by-value mangling and `engine_funcs.cpp:1607` resolves it correctly. The *function* is still unported. "Unported" and "phantom" are different claims, and only the second was blocking. |
| "a private `UpdateText` helper this lane did not resolve" | **RESOLVED AND PORTED.** `0x180112180`. |
| "a refcount swap has no safe replay oracle regardless" | **STILL TRUE.** §4. |

### 2b. `SliderElement::SetParameter` / `ProgressElement::SetProgressTextParameter`

Same program, one branch apart. Both resolve `_children._data[0]`, cast it
through `IsDerivedFrom` against `?_typeInfo@TextElement@UI@@`, range-check the
index against the child's own parameter count, then run seven identical
instructions: dirty-walk from the CHILD up the parent chain,
`String::operator=(&child->_params[index], str)`, `TextElement::CacheText(child)`.

**RETAIL HARD-FAULTS IN `ProgressElement` WHERE `SliderElement` RETURNS, and the
port reproduces it.**

```
Slider   0x1800d6ccd  TEST AL,AL   / JZ done    <- cast failed, bail
         0x1800d6cd1  TEST RBX,RBX / JZ done    <- and bail on a null child
Progress 0x180119b62  XOR EDI,EDI
         0x180119b66  CMOVNZ RDI,RBX            <- child = ok ? first : NULL
         0x180119b6c  TEST ESI,ESI / JS done    <- only a NEGATIVE index exits
         0x180119b6e  CMP ESI,dword ptr [RDI+0xd0]   <- READS THROUGH IT ANYWAY
```

With a first child that is not a `TextElement` and a non-negative index,
`SetProgressTextParameter` reads address `0xd0` and dies. The `CMOVNZ` is a
cast, not a null check, and retail then *asserts* the cast succeeded by
dereferencing. The port therefore contains a deliberate load through a
possibly-null pointer, spelled as byte arithmetic on a `uint8_t*` so the
compiler emits the load rather than concluding the pointer cannot be null and
deleting the branch around it. **Do not "fix" this with a guard** — the
guard-masking-a-fault inversion is exactly what WAVE9.md's Finding 3 had to
un-learn.

### 2c. `ComboElement::SetCurrentItem` + `ComboElement::UpdateText`

`UpdateText` was ported because it is the **cheaper** half: `FindElementType`
(ported, Wave 7 lane A), `IsDerivedFrom` (ported, Wave 3), `String::operator=`
(ported, Wave 2) and `TextElement::CacheText` (an existing door). Porting it
opens nothing. Leaving it unported would have meant `SetCurrentItem` calling
`0x180112180` by address — one brand-new row in the bucket with no export name
to work from.

Two things in `UpdateText` look like transcription slips and are not:

1. **`_children._data[4]`, not `[0]`.** `0x18011219b` / `0x1801121a7` are
   `MOV RBX,[RCX+0xa0]` then `MOV RBX,[RBX+0x20]`; `0x20 / 8 = 4`. Every other
   wrapped-widget lookup in the file is `children[0]`. Ghidra's C prints both
   loads as one expression — `reference_ghidra_c_output_vs_disassembly.md`.
2. **Neither `_strings` nor `_sprites` is bounds-checked against its own size.**
   Retail guards only the item count at `+0xf0` and, for the sprite, the
   sprite's own validity (`index >= 0 && sheet != null`, which is
   `UI::Sprite::IsValid()` taken apart). A `_currentItem` of `-1` — which
   `RemoveAllItems` sets — would index `[-1]`. Not a check retail performs, so
   not one this port performs.

`SetCurrentItem`'s out-of-range branch resets `_currentItem` to **0, not −1**
(`0x1801125a8`). Only `RemoveAllItems` leaves −1. Getting that backwards would
put the combo silently on the wrong item, so it is called out in the source.

The body allocates nothing and frees nothing. After handing `_openDialog` to
`CloseDialog` it simply zeroes the field with no release and no destructor —
`Object::InternalPtr` is a handle, and the callee only reads the slot.

---

## 3. A finding worth more than the three ports: `?_typeInfo@` HAS A NAME

The type ids these bodies read are **exported DATA**:

```
0x1801faee8  ?_typeInfo@TextElement@UI@@0VBaseTypeInfo@Object@@A        ord 4922
0x1801fa040  ?_typeInfo@LabelElement@UI@@0VBaseTypeInfo@Object@@A       ord 4877
0x1801fa190  ?_typeInfo@NumberEditElement@UI@@0VBaseTypeInfo@Object@@A  ord 4886
```

All three are already carried by lane C2's mint. So a **named** read costs
nothing at cutover, while an **address** read books a row in the unregistered
-address bucket.

`widgets_input.cpp`'s existing `NumberEditElementTypeId()` reads `0x1801fa190`
**by address**, with a comment explaining the choice — written before anyone
checked whether the slot had a name. It does. **This lane did not change that
live accessor** (retuning another lane's working call site is not this lane's
call), but it is filed here as a one-line fix at source per PLAN §6 step 8, and
the new accessors next to it use the named form so the next person copying the
idiom copies the right one.

Generalisation worth checking in another lane: **the `?_typeInfo@` reads
scattered through `code/` and `runtime/` are a mixed population of named and
by-address forms for the same class of symbol**, and every by-address one is a
row the mint cannot help.

---

## 4. Oracles — what is here, what is not, and what each is worth

**There is no replay oracle on any of the four bodies, and that is a positive
decision, not a gap.** Every one ends in `System::String::operator=` on a slot of
a CHILD object reached through a pointer a byte-for-byte scratch copy would
*share* (this file's own Wave 7 "shared-child caveat"). A replay would run the
refcount acquire/release twice on one live string; restoring the slot's raw
pointer to undo our write can make retail's subsequent access a use-after-free
rather than a diagnostic. This is the identical argument
`widgets_layout.cpp`'s `TextElement::SetParameter` makes for the identical tail,
and lane M's Finding 1 says a body of this shape must not be prefix-replayed.

What is here instead:

* **`RT_DIFF_WORK`** — the DID-WORK counter WAVE9.md asks for beside `calls=`.
  Every body counts calls that **reached the assignment** separately from calls
  that entered. A large `calls=` reading as coverage is the same trap as
  `calls=0` reading as clean, one level up: 12,886 of `SetValueInt`'s 12,896
  calls took an early-out.
* **`RT_DIFF_SHADOW` on a falsifiable post-condition** — after
  `String::operator=(dst, src)` our ported body guarantees
  `dst->data == src->data`. That predicate **fails** if the slot address is
  computed wrong (wrong base, wrong stride, wrong index), which is the whole of
  what this lane could get wrong that a run would not otherwise notice. It is
  **not** a second spelling of the code above it.
* **`RtUiW2SelfTest()`** — a positive AND a negative control for that
  arithmetic, run once per diff-enabled process, printing `ALIVE` or
  `*** BROKEN — its silence means NOTHING ***`. The negative control recomputes
  the slot at a 4-byte stride (the single most plausible way to get it wrong)
  and the test **requires that it disagree**; it is taken at index 3 because at
  index 0 the two computations coincide and the control could not fire. It runs
  on a synthetic buffer, so it needs no `String`, no allocator and no DLL.

**What the shadow predicate is silent about, stated rather than glossed:** the
dirty-flag parent walk, `CacheText`'s effects, the combo's child search, the
16-byte sprite copy, and anything past the two offsets it names. It is a control
for the arithmetic, **not an oracle for the port**, and it must not close a gate
row on its own — the same sentence lane M had to write about the released-pointer
detector, for the same reason.

---

## 5. DECLINED — `?CreateInstance@ElementDescription@` (0x180115ad0)

**Measured blocker, not an inherited one.** Lane UIF row 10 declined it on "five
unexported internals, depth 5". Two of those five have since become ours
(`0x180009460`, `0x180039440`, both registered in
`runtime/src/collection/array.cpp` by lanes UIW and UIC). The remaining blocker
is larger than the row said, and re-reading the disassembly is what found it:

**`CreateInstance` CONSTRUCTS AND DESTROYS A COMPLETE `UI::Dialog` ON ITS OWN
STACK FRAME.** At `0x180115c70` it stamps `LEA R12,[0x1801562a0]`, and pefile
says `0x1801562a0` is **`??_7Dialog@UI@@6B@`** (ord 1417) — not
`??_7ElementDescription@UI@@6B@` (`0x1801561c0`). The frame is built by stamping
`??_7Base@Object@@6B@`, then `??_7External@Object@@6B@`, then that Dialog vtable,
zeroing ~0x80 bytes of members, and it is torn down at
`0x180115d3a`-`0x180115d93` through four unexported destructors plus a
refcounted `String` release.

Still unported and needed:

| Address | What it is |
|---|---|
| `0x180039360` | member destructor of the stack Dialog |
| `0x180039280` | member destructor of the stack Dialog |
| `0x180006b80` | `Object::External` sub-object teardown |
| `0x180002470` | `StringTable::GetIndexFromName` |
| `[childDesc->vtbl + 0x40]` | a virtual dispatch on an `ElementDescription`-family vtable whose slot 8 is unidentified — the same call lane UIF declined `?CreateElement@Dialog@` on |

Porting it today would close one named export and open **four** address rows plus
an unidentified vtable slot. That is the wrong direction by §1's own measure.

**And the blast radius is the one the brief flagged.** `CreateInstance` writes
eleven fields of a live `UI::Element` and sits on the `UIDescription` parse path
of every `.rsc`. The previously-closed defect where `UIDescription` slot 8 gave
no type id **doubled every loaded entity's roster** and made `FindComponent`
return the empty duplicate — invisible in a screenshot, visible only as
highlights at the wrong position. An unoracled port here is the single worst
place in this codebase to land one.

**Recipe for whoever takes it:** the two Dialog member destructors are prime
`J1_ENTRY` candidates — `array.cpp`'s J1 family established that this pool is
one body parameterised by element size, and `tools/uic_bytediff.py` decides it in
one run against the reference at `0x180033960`. Settle those two first; the
vtable-slot-8 question is shared with `?CreateElement@Dialog@` and should be
answered once for both.

---

## 6. DECLINED — `?Serialize@ElementDescription@` (0x180116b00)

**The stated blocker from Wave 7 lane B was TWO reasons and ONE OF THEM HAS
EXPIRED.** Recorded that way because the difference is the useful part.

> 1. **No oracle.** It constructs an `IO::Serializer` … a shadow replica would
>    have to model the whole `IO::Serializer` state machine.

**EXPIRED.** Wave 9 lane S built exactly that oracle:
`runtime/src/io/save_trace.{h,cpp}`, `BANISHED_RT_SAVE_TRACE=1`, which replays
both bodies into two independent synthetic sinks and compares the **emission
sequence** rather than the bytes. Its first subject was `Audio::Scene::Serialize`
— 32,772 emissions, 0 diverged. And this function is the *easiest possible*
subject for it: on the write path it is a **pure read** of the description with
no mutation at all, so it needs no prefix cap.

The emission sequence, read off the listing, is short and completely determined:

| # | Field | Encoding `ReaderWriter` slot (RETAIL offsets) |
|---|---|---|
| 1-6 | `_x _y _width _height _minWidth _minHeight` | `+0xa0` (int) ×6 |
| 7-9 | `_elementType` `_flags` `_id` | `+0x98` (uint) ×3 |
| 10 | `_tabOrder` | `+0xa0` (int) |
| 11 | `_childDesc` (+0x48) | `CALL 0x180038f90` |
| 12 | `_userData` (+0x50) | `CALL 0x180076610` |
| 13 | `_stringTable` (+0x58) | `CALL 0x1800042a0` |
| 14 | `_textName` (+0x60) | `CALL 0x1800c9930` — `String::Serialize`, **ported** |

Note the ORDER: `_id` (+0x3c) is emitted **ninth**, after `_flags` (+0x44), and
`_tabOrder` (+0x38) is emitted **last of the scalars** — the emission order is
not the field order, and a port that walks the struct top-to-bottom desyncs every
save. The `+0x98` / `+0xa0` slots above are the **RETAIL** offsets; the profile
build's `IO::Encoding` vtable puts uint/int at `+0xb0`/`+0xb8`. That warning has
been in `runtime/types/object/Internal.h` since Wave 4 and a lane still tripped
on it.

The `Serializer` is a 16-byte stack local: `{Stream* at +0x00, ReaderWriter* at
+0x08}`. It is built by reading `stream+0x10` (the `Encoding*`) and calling
encoding vtable `+0x18` when `stream+0x08 == 0` (reading) or `+0x20` otherwise
(writing); it is destroyed by `(*rw)[0](rw, 1)`. The function always returns
`true`.

> 2. It lands on the `UIDescription` parse path of every `.rsc` in the game.

**STILL TRUE**, and now the only reason. Registering this name does more than
redirect our own `fn_*` pointers: `standalone/address_registry.cpp:1109` binds
exe PE-import slot `0x1401518c8` to it by name, and `RuntimePortInit()` runs
before `AddressRegistryInit()`, so the exe's own `.rdata` slot re-points at our
body too.

**The measured blocker is the three callees, and it is smaller than it looks.**
`0x180038f90`, `0x180076610`, `0x1800042a0` are all unported
`Object::ExternalPtr<T>::Serialize` instantiations that mutate the global
`ExternalFactory` (`?_singleton@ExternalFactory@Object@@` at `0x1801fc6c8`,
confirmed by pefile) and do refcounted `String` work. But **`0x1800042a0` and
`0x180076610` are 215 instructions each with byte-identical prologues**, while
`0x180038f90` is ~128 — i.e. the first two are almost certainly ONE body
parameterised by a type id, and the J1 method in `array.cpp` decides that in one
`tools/uic_bytediff.py` run.

**So the recipe is:** byte-diff `0x1800042a0` against `0x180076610`; if they are
one body, porting it plus `0x180038f90` closes all three callees, and lane S's
oracle then verifies `Serialize` itself against the shipped DLL **while the DLL
is still there**. That is a well-shaped next lane. It is not this lane's, because
porting `Serialize` first — before its callees — would close one named export
and open three address rows onto the `.rsc` parse path with no oracle, which is
§5's mistake with a worse blast radius.

**A correction to `runtime/types/ui/ElementDescription.h` that falls out of the
above and should be applied by whoever takes this row:** the header types `+0x50`
as `uint64_t _userData`. `Serialize` hands it to a 215-instruction
`ExternalPtr<T>::Serialize`, not to a scalar slot, so `+0x50` is an
**`ExternalPtr<T>`** (8 bytes, which is why `CreateInstance`'s plain qword copy
at `0x180115c04` still looks right). Same for `+0x58`, typed `void*
_stringTable`, which is an `ExternalPtr<StringTable>`. The offsets are correct;
the TYPES are under-specified, and a port that treats `+0x50` as a plain integer
will emit the wrong thing.

---

## 7. HONEST LIMITS

> ⚠ **LIMIT 1 IS SUPERSEDED — SEE SECTION 9.** The integrator ran the lane after
> this section was written. Four cells exist now, four of the six bodies have
> real `calls=`/`worked=` figures, and one is fully verified at 1202/1202.
> Limit 1 is left standing rather than rewritten, because the reason it existed
> — a lane that cannot run its own ports has not verified them — is exactly what
> §9 had to be written to discharge, and because its "what the integrator should
> do" list is what produced §9's numbers. **The limits that did NOT expire, and
> the one prediction in this document that turned out to be WRONG, are in §9.**

1. **NO RUN HAPPENED, AND SO `calls=` IS UNMEASURED FOR ALL FOUR BODIES.** This
   is the largest limit and it is not a small one: the brief's rule 5 says a port
   with no caller reports clean, and **these four have exactly the
   `divergences=0, calls=?` status that rule warns about.** Nothing here has
   executed. The build is proven only as far as a clean
   `g++ -std=c++17 -fsyntax-only -Wall -Wextra` of the translation unit (exit 0,
   no warnings); no link, no deploy, no game-directory lock, no
   `BANISHED_RT_DIFF` summary, no `[UIW2 self-test]` line has ever been printed.
   **Every claim in §1 is a SOURCE-level claim measured by the static audits, and
   every claim in §4 about the shadow predicate and the self-test is a claim
   about code that has not yet run.**

   The cause is environmental and is reported rather than worked around: this
   session's git worktree isolation reassigned itself repeatedly mid-task —
   `lane-addr`, `lane-mod9`, `w9-addr-089e6eb4`, a nested `engio-089e6eb4`,
   `lane-uid-089e6eb4` — with other lanes' branches and uncommitted edits
   appearing in the directory under this session between consecutive tool calls.
   One early edit landed in a directory another lane was using and **was detected
   and reverted** (`runtime/src/ui/widgets_input.cpp` restored byte-for-byte;
   that directory's `git status` afterwards showed only the other lane's own
   files). The dedicated worktree this branch now lives in was created explicitly
   to stop that. Running the game additionally needs the shared game-directory
   lock while ~15 lanes are live, and a run taken from an ambiguous tree is
   exactly the "complete-but-wrong measurement" the brief warns about — a wrong
   `calls=` would have been worse than an absent one.

   **What the integrator should do before merging:** build, then run at least the
   menu cell with `BANISHED_RT_DIFF=1` and read three things — the
   `[UIW2 self-test]` line (must say `ALIVE`), the `calls=` and did-work columns
   for the four site names, and the shadow verdicts. **`SetCurrentItem` and
   `UpdateText` are drivable unattended** with
   `BANISHED_AUTOMENU_BUTTON=7 BANISHED_AUTOMENU_CLICKS=…` (the Mods dialog has
   combo boxes) and the in-game half with `BANISHED_PAUSEMENU_*`. If `calls=0` on
   all four, the ports are **UNVERIFIED, not clean**, and should be reported as
   such.

2. **The self-test's power is narrow and is not the port's verification.** It
   proves `TextParamSlot`/`TextParamCount` address what
   `runtime/types/ui/TextElement.h` says they address. It cannot detect a wrong
   value in that header. If `kParamArrayData`/`kParamCount` are wrong, the test
   passes and the port is wrong.

   > **CORRECTION, made when this lane re-based onto `f5dbeb8`.** An earlier
   > draft of this limit said the `+0xD0` disagreement between `TextElement.h`
   > (int32 `size`) and `NumberElement.h` (an owning refcounted `String` data
   > pointer) was **an open contradiction this lane did not resolve**. That was
   > written against `fd30180` and it is **stale**: `NumberElement.h` on the
   > integration branch already carries lane N's resolution, from the
   > disassembly — **`UI::NumberElement` is not a `UI::TextElement`.** Its ctor
   > (0x180118C70) calls the `UI::Element` base ctor at 0x180115E50, not
   > TextElement's, and it has its own private `CacheText` at 0x1801189F0
   > distinct from `TextElement::CacheText` at 0x18011EC10. Two different
   > classes, one offset, both headers right.
   >
   > Part 2 of this lane (§8) is an **independent confirmation** of that
   > verdict, arrived at before the header was read: `CacheText@NumberElement`
   > decrements `[p-4]`, frees `p-8` and stores a new pointer at
   > `NumberElement+0xD0` — behaviour that is only coherent if that qword is an
   > owning `String`. This lane's three §2b/§2c subjects are `TextElement`s (the
   > type-id cast proves it at runtime), so `TextElement.h`'s call-site-derived
   > table is the applicable one for them. **The limit that remains is the
   > narrow one:** the self-test cannot detect a wrong value in the header it
   > checks against.

3. **The `ProgressElement` null-deref is untested against a real failing case.**
   It is proven from the listing that retail dereferences a possibly-null child;
   it is not proven that our compiler emits the load in every optimisation
   configuration. The byte-arithmetic spelling is a mitigation, not a guarantee,
   and only a disassembly of the built object file would settle it.

4. **`0x180112180` (`UpdateText`) shows in `runtime_census.py` as a port with no
   caller in our tree**, taking that list from 178 to 179 addresses. That is
   correct and expected — its only caller is `SetCurrentItem`, which calls the
   C++ function directly rather than through `ResolveAddr`. The address
   registration exists so the OTHER door reaches our body, per the register-twice
   convention. It is **not** evidence that the body is dead, and it is **not**
   evidence that the body works.

5. **The `?CacheText@TextElement@` door is still null at cutover.** These ports
   do not create that problem and do not fix it: `widgets_layout.cpp:102` already
   had it, and this lane deliberately spells the same mangled name so the two
   share one row. Closing it needs `?SetParameterCount@TextElement@`
   (`0x18011e8e0`) and is the best next move in this cluster —
   **`CacheText` blocks three ported bodies now, not two.**

6. **`?CloseDialog@UserInterface@` is likewise still null at cutover** and
   `SetCurrentItem` will call through it. Reusing `engine_funcs.cpp`'s door keeps
   the row count flat, but the row is real and belongs to lane UIF row 4.

7. **The static audits carry their own documented blind spots and this lane did
   not re-derive them.** `unsatisfied_export_scan.py` says in its own output that
   2 resolves go through a non-literal wrapper and are invisible to it;
   `addr_door_audit.py` reports 77 non-literal call sites and 11 non-literal
   registrations invisible to both halves. A clean set-diff means "no change of
   the shape these tools can see" — the same sentence
   `port_bypass_audit.py`'s gate row carries.

---

## 8. PART 2 — two unregistered ADDRESS doors, closed by porting

Added after the integrator's triage of the 16 remaining unregistered address
doors named three in this cluster. **Two of the three were in this lane's own
file**, at what were then lines 220 and 232 of `runtime/src/ui/widgets_input.cpp`.

| Address | Symbol | Ord | Verdict |
|---|---|---|---|
| `0x1801189f0` | `?CacheText@NumberElement@UI@@AEAAXXZ` | 1679 | **PORTED**, registered name + address |
| `0x18003ffe0` | `??9String@System@@QEBA_NAEBV01@@Z` | 1290 | **PORTED**, registered name + address, not counted in the UI tally |
| `0x1800da130` | `?AddTab@TabElement@UI@@QEAA?AV?$InternalPtr@…@Z` | 1589 | **DECLINED** — §8c |

Both taken doors were verified as **live code, not prose**, before anything was
written — the integrator's warning that `addr_door_audit.py` had been matching
`ResolveAddr` text inside comments applies to exactly this kind of hand-off.
Both sit inside function bodies (`static … = ResolveAddr(Module::Runtime, …)`),
and both are now **deleted**, not converted.

### 8a. The reason they were address doors is a COMMENT THAT WAS WRONG

The old source text said `0x1801189f0` was "an unexported private helper", and
that for `0x18003ffe0` "neither showed up as an exported mangled name in this
lane's pefile check". Re-run against the shipped export directory, 2026-09-08,
**all three are exported** — the ordinals in the table above are that query's
output.

`AEAA` means **private, not unexported**. This DLL exports its private members;
that is precisely why `?CacheText@TextElement@UI@@AEAAXXZ`, twelve lines away in
the same file, resolves by name and works. **A private method and an unexported
one look identical in a decompiler and are not the same thing in an export
table**, and a comment recording the confusion is what kept two symbols in the
worst-failing door class for two waves.

This is the same defect as §3's `?_typeInfo@` finding, arriving from a different
direction: **an address door on a symbol that has a perfectly good name,
protected from review by a comment asserting that it does not.** Worth a sweep
of its own — the pattern is a `ResolveAddr` whose justification is the words
"unexported" or "not exported", and this file alone had three (the two here plus
`NumberEditElementTypeId()`, §3).

Neither door was **converted** to a name door. A converted door on an unported
symbol only changes which list it prints in
(`reference_converting_a_door_is_not_satisfying_it.md`). Both are ported, so the
door closes rather than moves.

### 8b. What the two bodies are, and the one REAL oracle in this lane

**`System::String::operator!=` (0x18003ffe0)** is retail's `operator==` inlined
and negated, twice over (`SETZ / TEST / SETZ`):

```
lenA = a ? a[-2] : 0                        0x18003ffe6-0x18003fff5
lenB = b ? b[-2] : 0                        0x18003fff7-0x180040005
if (lenA != lenB)       equal = false       0x18004000b / 0x18004000d
else if (!a || lenA==0) equal = true        0x18004001a, 0x18004001f
else                    equal = StrCompare(a,b) == 0     0x180040024
return !equal
```

The "both empty" shortcut tests only `a`, never `b`. That asymmetry is retail's
and is preserved: it is reachable only when the two lengths already agree, so a
null `b` there necessarily has length 0 and the answer is the same — but the
instruction that would test `b` does not exist, and a port that adds it is not
this function.

**It carries the only true differential oracle in this lane.** The function is
PURE — it reads two Strings, allocates nothing, mutates nothing — so the shipped
body can be called on the same two arguments and the two booleans compared with
`RT_DIFF_SCALAR`. Everything else this lane touched ends in a refcount swap and
could not be diffed at all (§4). Its `RT_DIFF_WORK` counts only calls that
**reached the byte compare** (equal non-zero lengths); every other path is
decided by the lengths alone, so a run where that counter stays 0 has exercised
none of `StrCompare`, which is the half that can be wrong.

**`NumberElement::CacheText` (0x1801189f0)** formats the live half of the tagged
union into the owning refcounted `String` at `+0xD0`:

```
mode == 0 : Format(&tmp, _showSign ? "%+i"   : "%i",   _intValue)
mode != 0 : Format(&tmp, _showSign ? "%+.2f" : "%.2f", (double)_floatValue)
_cachedText = tmp;      // retail's INLINED String::operator=
release(tmp)
```

The four literals were read out of the DLL's `.rdata` with `read_memory`
(`0x180159144` `"%+i"`, `0x180154848` `"%i"`, `0x180159148` `"%+.2f"`,
`0x180159150` `"%.2f"`) and match the table `NumberElement.h` already carries.
They are transcribed as C string literals: a function's own string constants are
part of the function, and resolving them would have opened **four new `.data`
doors to close one code door** — precisely
`reference_porting_can_grow_the_cutover_surface.md`.

Two ABI details that are easy to get wrong here:

* The float branch passes a **double**. Retail's `CVTPS2PD XMM2,XMM2` then
  `MOVQ R8,XMM2` sets both the integer register and the matching XMM for
  argument 3, which is what the Microsoft x64 ABI requires of a variadic float
  and is invisible if only the `MOVQ` is read. Passing a `float` would not
  reproduce it.
* `?Format@String@System@@SA?AV12@PEBDZZ` (0x1800c9730) is **our** ported body,
  so this is an ordinary C++ call and carries none of the cross-toolchain
  MSVC-variadic risk Wave 7 lane D held `NumberEditElement::SetValue` back for.

**ZERO NEW DOORS in either body.** `RtString_StrCompare`,
`RtString_ReleaseInline`, `RtSystem_String_Assign` and `RtSystem_String_Format`
are all reached as plain C++ calls into our own ported code.

**The `CacheText` oracle is a REDIRECTED REPLAY, and it is real.** The body swaps
a refcounted pointer, so a naive replay would run the acquire/release twice on
one live string — the trap declined everywhere else in this lane. The way out is
`dialog.cpp`'s `?StopTimer@` pattern: copy the object, **repoint the copy's
owning field at null** so the shipped body allocates its own buffer and can never
touch ours, run both, and compare the two formatted **payloads** (not the
pointers — the two sides allocate separately by construction, so comparing
addresses would fail 100% of calls and read as a catastrophic port). The
scratch's string is released immediately after, so the diff leaks nothing.

**A trap this oracle had to step around, recorded because it is a live defect in
a shared header.** The scratch must be **0xE8** bytes — `CreateNewObject`
(0x18004a910) calls `Allocate(_singleton, 0xE8, 8)` — but `sizeof(NumberElement)`
is still **0xE0**, because `NumberElement.h` documents the 0xE8 size in prose and
**its struct was never widened**. `NumberElement.h` also records what a 0xE0
replay buffer did the last time: the DLL's `CacheText` read `+0xE0`, the
`_showSign` byte, off the end of the stack object and chose its format string
from garbage. That is *this* function. The port therefore uses an asserted
`kNumberElementTrueSize = 0xE8` rather than `sizeof`, and does **not** widen the
shared struct — adding trailing padding to a binary-compatible type mid-wave is a
change every other lane would inherit without asking for it. **The header should
be widened, by whoever owns it, with a `static_assert(sizeof(...) == 0xE8)`.**

### 8c. DECLINED — `?AddTab@TabElement@UI@@` (0x1800da130)

143 instructions, a 0xC0-byte frame, seven callee-saved registers, and it
**returns `InternalPtr<Element>` by value** — so there is a hidden return slot
between `this` and its first visible argument, and it takes two more by-value
class parameters (`ExternalPtr<ElementDescription>` ×2) that MSVC passes
indirectly. It allocates. Its door is `engine/funcs/engine_funcs.cpp:2971`, not
this lane's file.

Declined on scope, not on difficulty: it is a different file, a different
argument-shape hazard family, and it wants the `abi_retslot_audit` treatment on
its own rather than as a rider. **It is exported (ord 1589)**, so whoever owns
`engine_funcs.cpp` can at minimum turn that silent address door into a NAMED one
— which converts a null call at cutover into a visible `[RtUnresolved]` row. That
is a strictly smaller change than porting it and is worth doing even if the port
waits.

### 8d. Closure, PART 1 AND PART 2 TOGETHER, on the CURRENT integration base

**Re-measured from scratch against `f5dbeb8`** — which matters twice over: it is
the base the integrator asked for, and it carries the **FIXED**
`addr_door_audit.py` (the one that no longer counts `ResolveAddr` text inside
comments; its denominators moved 295→270 sites and 198→192 distinct on the base
alone). **All numbers below are the fixed tool's.** The §1 table was produced by
the PRE-FIX tool against `fd30180` and is left as it was recorded rather than
retconned.

| Audit (from `f5dbeb8/tools/`) | base `f5dbeb8` | after (parts 1+2) | delta |
|---|---|---|---|
| `addr_door_audit.py` | 270 sites, 192 distinct, **15 UNREGISTERED**, 1187 registered | 268, 190, **13**, 1193 | **−2 UNREGISTERED**, −2 sites, −2 distinct, +6 registered |
| `unsatisfied_export_scan.py` | 848 doors, 128 unsat, **125 outside a diff guard** | 852, 125, **122** | **−3 outside-guard**; CLOSED 3, OPENED 0 |
| `port_bypass_audit.py` | 334 address / **0**, 480 name / **0** | 332 / **0**, 482 / **0** | 0 bypasses both doors |
| `rawdoor_audit.py` | 610 sites, **0 UNGUARDED-CALL** | 612, **0** | +2 sites, both guarded oracles |
| `abi_retslot_audit.py` | 0 suspicious of 48 | **byte-identical** | 0 |
| `detour_target_audit.py` | clean | **byte-identical** | 0 |
| `export_name_audit.py` | 670 names, 1 not in the DLL (pre-existing) | **byte-identical** | 0 |
| `resolve_cache_audit.py` | 442 FROZEN | **byte-identical** | 0 |
| `runtime_census.py` | UI 153/118 77.1%; System 97/73 75.3%; TOTAL 1003/772 **77.0%** | UI 154/121 78.6%; System 96/73 76.0%; TOTAL 1003/775 **77.3%** | +3 ported |

**ONE DENOMINATOR SHRANK AND IT IS NOT A TOOL BUG — but it is named rather than
buried,** because a shrinking denominator has faked a clean pass in this project
twice. `runtime_census.py`'s System `called` went **97 → 96**. Cause: `called`
counts symbols our tree resolves through a door, and this lane **deleted** the
only live door for `??9String@System@@` (its `ResolveAddr`). The symbol left the
work list because the work was done, which is the one legitimate reason a
denominator may shrink. `addr_door_audit`'s 270→268 and 192→190 are the same
event seen from the other side.

**Both new bodies land in the census's "Ports with no caller in our tree" list**
(86 exports/178 addresses → 88/181). That is intended and is not evidence of dead
code: their callers are the two forwarders in the same translation unit, calling
them as C++. What the census means by "no caller" is "no *door* in our tree
resolves them" — which is exactly the state deleting the doors was meant to
produce. It is still `reference_uncalled_port_is_not_verified.md` territory for
the purpose of §7 limit 1: **neither body has executed.**

`??9String@System@@` is registered under both doors but **not counted** in the UI
tally — it is a `System` symbol living temporarily in a UI file. Leaving it
unregistered was tried first and rejected **on the measurement**: without the
registration, the oracle's own `RT_ORIGINAL_ONCE` spelling appears in
`unsatisfied_export_scan.py` as an unsatisfied name (harmless — inside a diff
guard, compiled out at cutover — but it is still a ported body reachable through
neither door, which is how `?RemoveChildren@Element@UI@@` became the tree's only
address-door bypass). **For whoever ports `System::String` next: grep for that
registration before writing a second body, and MOVE it rather than adding a
second binding** — a duplicate registration is last-binding-wins and silently
makes one of the two dead code.

---

## 9. THE RUNS — what actually executed, and one prediction that was WRONG

§7 limit 1 said this lane had never run and that `calls=` was unmeasured. The
integrator ran it. This section replaces the guesswork with numbers, records the
one claim in this document that the runs falsified, and hands on a finding about
the exe that a `calls=0` chase turned up by accident.

### 9a. Final standing

Diff harness on, DLL present, integration tips `442eea6` and `bfb3863`.

| Body | calls | worked | divergences | Status |
|---|---:|---:|---:|---|
| `UI::SliderElement::SetParameter` | **1202** | **1202** | 0 | **VERIFIED** |
| `UI::ComboElement::SetCurrentItem` | 4 | 4 | 0 | verified, THIN |
| `UI::ComboElement::UpdateText` | 4 | 4 | 0 | verified, THIN |
| `System::String::operator!=` | 4 | **1** | 0 | **weakly verified** — one real comparison |
| `UI::ProgressElement::SetProgressTextParameter` | — | — | — | **UNVERIFIED**, route known |
| `UI::NumberElement::CacheText` | — | — | — | **UNVERIFIED**, open question at its caller — §9c |

`[UIW2 self-test] ALIVE` in every cell, with the 4-byte-stride negative control
reported as FIRED. That line is what made the rest of the table legible: every
other `calls=0` in the wave is ambiguous between *not exercised*, *not
registered*, *raw resolve missed* and *wrong build*; with the self-test present,
these are ambiguous between exactly one of those.

**The 1202 came from a harness this lane did not have to write.**
`SliderElement::SetParameter` has **two call sites in the entire tree**, both in
`pastureui.cpp` (`:217`, `:222`, in `PastureUI_Slot7`), so no front-end route
reaches it and it was on its way to being recorded as permanently unverified.
`standalone/tool_harness.cpp:1220-1330` is a purpose-built PastureUI driver —
`BANISHED_TOOL_HARNESS=pasturefull` plus `BANISHED_PASTUREPANEL_SELECT=<idx>` —
and one cell took the body from UNVERIFIED to the best-covered thing in the lane.
`worked=1202/1202` because those two calls are unconditional and back to back.
**Before deciding a body has no route, grep the harness directory.**

### 9b. `String::operator!=` — why `worked=` is the only column worth reading

Two cells, and they are the argument for the did-work counter in one line:

```
gameplay cell   calls=2  worked=0   <- divergences=0 is worth NOTHING here
pasture cell    calls=4  worked=1   <- one real comparison
```

`worked` counts only calls that reached `StrCompare`, which needs **equal,
non-zero lengths**; every other path is decided by the lengths alone. So the
first cell entered the function twice and never executed the half that can be
wrong. **A `divergences=0` on that line reads as a pass and is not one.**

The `calls=4 worked=1` split is fully accounted for rather than assumed: the only
consumer is `RtUi_EditElement_SetText`, which calls this once at its top, and the
driver is `newgamedialog.cpp:229` — the random town-name generation in
`NewGameDialog_Slot4`, reached on every autostart cell. The first `SetText` is
empty → name, so `lenA == 0` early-outs before `StrCompare`; the rest differ in
length; one pair happened to match. **To raise it, a `SetText` is needed where
old and new have the same non-zero length** — `mapparamsdialog.cpp:339`'s
re-randomise path over an existing name is the cheapest.

### 9c. ⛔ A PREDICTION IN THIS DOCUMENT WAS WRONG, AND IT WAS AN UNCHECKED ASSERTION

When the integrator asked why `NumberElement::CacheText` never declared, this
lane answered that it *"should be well exercised; it's on both `SetValue`
paths."* **That was wrong, and it was wrong in a way this document has no excuse
for: it was asserted without checking that our port is on the status bar's
path.** The lane pre-committed to three outcomes before the evidence arrived and
this is the one that obliges the miss to be recorded, so it is recorded.

What the runs actually show: `UI::NumberElement::SetValueInt` **never declared a
site**, in either a 720-frame gameplay cell or the pasture cell — and
`RtDiffRegister` declares on first entry, before any early-out, so absence means
never entered, not entered-and-silent. `CacheText` is reached only when a
`SetValue` changes a value, so its absence is inherited from a caller that never
ran.

**The reason is now known, and it is not in this lane.** See §9d. Knowing the
reason does not retire the miss: the assertion was made before the check.

**What remains genuinely open for `CacheText`:** its **float** branch has never
executed anywhere. That is where `_showSign` selects `"%+.2f"` vs `"%.2f"` and
where the variadic `double` argument lives — the two things most likely to be
wrong, and the ones no integer call touches. `BANISHED_DEBUG_PANEL` is the probe:
`debugui.cpp`'s `numFPS` plus the two `SetValueFloat` sites (`numFrameMS`,
`numTotalMS`) change every frame by construction, so they cannot all early-out.

### 9d. AN EXE-SIDE GAP FOUND BY ACCIDENT: `StatusBarUI` IS DEAD CODE

Chasing the `SetValueInt` zero turned up something larger than the question.

**`code/Game/Dialog/statusbarui.cpp` is fully reimplemented and nothing
registers it.** The file documents the retail vtable at exe `0x14015d810` and the
three slots it fills —

```
+0x28 (idx5)  0x14007a3d0  element discovery   -> StatusBarUI_Draw
+0x38 (idx7)  0x1400798d0  full stats refresh  -> StatusBarUI_Startup
+0x48 (idx9)  0x14007a3a0  status-icon click   -> StatusBarUI_OnRemove
```

— and there is **no `RegisterExeAddr` and no vtable array for any of them**.
`StatusBarUI_Startup`, the full stats refresh, owns **all 15**
`fn_NumberElement_SetValueInt` call sites (year, temperature, wood, stone, iron,
tools, firewood, food, herbs). It never executes, on any route. That accounts for
both zeros without implicating this lane's ports at all.

**It is the same shape as the half-registered `Array<8>::SetSize` lane UID found
earlier the same day, one level up:** there a *body* existed with no
registration; here an entire *dialog* does.

**How it was checked matters.** A grep for the class-name string `StatusBarUI`
finds nothing, but a grep for a class-name string is precisely the blindness this
project has paid for repeatedly — so it was re-checked **by symbol**:
`StatusBarUI_Startup`, `StatusBarUI_Draw`, `StatusBarUI_OnRemove`, `W_Refresh`,
`W_OnUpdate`, `W_Setup` — **six symbols, zero references outside their own
file**. Same answer, arrived at a way that could have disagreed.

### 9e. A FALSIFIABLE PREDICTION, left standing so it can fail

`BANISHED_AUTOTUTORIAL=1` with the diff harness has not been run. **This lane
predicts `SetValueInt` will show `calls=0` there too**, because the same
unregistered `statusbarui.cpp` owns the status bar on that route.

**If instead it shows a large `calls=`, this prediction is wrong and something
other than `statusbarui.cpp` drives `SetValueInt`** — which is the thing to find,
and which matters beyond this lane: `runtime/types/ui/NumberElement.h` records
lane M measuring **12,896** calls at that site *"on the tutorial routes"*, and
WAVE9.md's column is a **max across the matrix**. Reproducing that figure is what
keeps lane M's Finding 1 standing; failing to reproduce it anywhere would put a
question mark over the measurement Finding 1 was derived from.

Recorded here rather than in a message so that whoever runs it can mark this
paragraph right or wrong. Comparing a max-over-cells against a single cell and
calling the difference a regression was a mistake this lane and the integrator
both nearly made, and the guard against it is that the prediction names the cell.
