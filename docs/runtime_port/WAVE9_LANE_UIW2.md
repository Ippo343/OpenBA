# Wave 9 lane UIW2 — widget setters and the ElementDescription pair

Branch `wave9/lane-uiw2`, cut from `fd30180`. Date 2026-09-08.

Assignment: five symbols.

| # | Symbol | Retail | Ord | Verdict |
|---|---|---|---|---|
| 1 | `?SetParameter@SliderElement@UI@@QEAAXHAEBVString@System@@@Z` | `0x1800d6ca0` | 3936 | **PORTED** |
| 2 | `?SetProgressTextParameter@ProgressElement@UI@@QEAAXHAEBVString@System@@@Z` | `0x180119b30` | 3946 | **PORTED** |
| 3 | `?SetCurrentItem@ComboElement@UI@@QEAAXH@Z` | `0x180112540` | 3853 | **PORTED** |
| 4 | `?CreateInstance@ElementDescription@UI@@QEBAXV?$InternalPtr@VElement@UI@@@Object@@@Z` | `0x180115ad0` | 2004 | **DECLINED** — §5 |
| 5 | `?Serialize@ElementDescription@UI@@UEAA_NAEAVStream@IO@@@Z` | `0x180116b00` | 3687 | **DECLINED** — §6 |

Plus one dependency ported to keep the closure positive:

| — | `ComboElement::UpdateText` (private, **unexported**) | `0x180112180` | — | **PORTED**, address-only |

All five addresses and ordinals are `pefile` output against the shipped export
directory of `C:\GOG Games\Banished\Runtime-x64.dll`, not Ghidra's, and were
checked **before** any body was written
(`reference_resolveexport_phantom_names.md`). All five are present.

---

## 1. The closure set-diff — the number that decides whether this lane helped

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
   passes and the port is wrong — and **there is an open contradiction on exactly
   these offsets**: `TextElement.h` and `NumberElement.h` both carry lane M's
   note that probing `NumberElement::SetValueInt` showed `+0xD0` holding a
   refcounted `String` data pointer rather than an int32 size. Both readings
   cannot be right for the same offset. This lane's three subjects are
   `TextElement`s (the type-id cast proves it at runtime), so the call-site
   derivation is the applicable one — but **the contradiction is unresolved and
   this lane did not resolve it.**

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
