# Wave 7 — Lane D — Input/value widgets

Branch `wave/upgrade-path-complete`. Files owned by this lane:
`runtime/src/ui/widgets_input.cpp`, `runtime/types/ui/{SpinnerElement,ComboElement,
NumberEditElement,NumberElement,EditElement,SliderElement,RadioElement,
ProgressElement}.h`, this document, and the `EditElement::SetText` correction
in `engine/funcs/engine_funcs.{h,cpp}`.

**Result: 18 of 27 ported, 9 held back with written reasons, 1 phantom name
resolved (a real functional-gap fix, not a port).**
`RtUiWidgetsInRegister()` returns **18**. `-fsyntax-only` with the wave's exact
command line is warning-clean for `widgets_input.cpp` and for the corrected
`engine/funcs/engine_funcs.cpp`. `tools/export_name_audit.py` drops from 4 to
**3** pre-existing phantoms, **none in this lane's scope** — the fourth was
`EditElement::SetText`'s by-value overload, fixed at the source (§5).

Not run by this lane, by instruction: the game, `make`, `tools/rt_run.sh`,
`sa-run`, `git commit`. Every runtime number below is a *prediction of what
the oracle will report*, not a measurement — see §7.

---

## 1. The function table

All 27 mangled names (including the two that turned out not to be real
functions, and the two `IsDerivedFrom`/`ToInteger` dependencies) were checked
against the export directory of `C:\GOG Games\Banished\Runtime-x64.dll` with
`tools/pe_exports.py` before any body was written.

| Export | Retail | Ported | Oracle | Diff site |
|---|---|---|---|---|
| `?GetMaxValue@SpinnerElement@UI@@QEBAHXZ` | 0x1800d7cb0 | yes | replay | `UI::SpinnerElement::GetMaxValue` |
| `?GetValue@SpinnerElement@UI@@QEBAHXZ` | 0x1800d7cf0 | yes | replay | `UI::SpinnerElement::GetValue` |
| `?IsActive@SpinnerElement@UI@@QEBA_NXZ` | 0x1800d7c30 | yes | replay | `UI::SpinnerElement::IsActive` |
| `?SetElementId@SpinnerElement@UI@@QEAAXH@Z` | 0x1800d7e10 | yes | prefix replay | `UI::SpinnerElement::SetElementId` |
| `?SetIncrement@SpinnerElement@UI@@QEAAXH@Z` | 0x1800d7d30 | yes | prefix replay | `UI::SpinnerElement::SetIncrement` |
| `?SetMinMax@SpinnerElement@UI@@QEAAXHH@Z` | 0x1800d7d40 | yes | prefix replay | `UI::SpinnerElement::SetMinMax` |
| `?SetValue@SpinnerElement@UI@@QEAAXH@Z` | 0x1800d7dc0 | yes | prefix replay | `UI::SpinnerElement::SetValue` |
| `?AddItem@ComboElement@UI@@QEAAHAEBVSprite@2@AEBVString@System@@PEAX@Z` | 0x1800750b0 | **no** | — | — |
| `?GetCurrentItem@ComboElement@UI@@QEBAHXZ` | 0x180075160 | yes | replay | `UI::ComboElement::GetCurrentItem` |
| `?GetItemCount@ComboElement@UI@@QEBAHXZ` | 0x180075180 | yes | replay | `UI::ComboElement::GetItemCount` |
| `?GetItemData@ComboElement@UI@@QEBAPEAXH@Z` | 0x180075170 | yes | replay | `UI::ComboElement::GetItemData` |
| `?RemoveAllItems@ComboElement@UI@@QEAAXXZ` | 0x180075120 | **no** | — | — |
| `?SetCurrentItem@ComboElement@UI@@QEAAXH@Z` | 0x180112540 | **no** | — | — |
| `?GetValue@NumberEditElement@UI@@QEBAHXZ` | 0x1800d4b50 | yes | replay | `UI::NumberEditElement::GetValue` |
| `?SetMinMax@NumberEditElement@UI@@QEAAXHH@Z` | 0x1800d4b70 | yes | prefix replay | `UI::NumberEditElement::SetMinMax` |
| `?SetValue@NumberEditElement@UI@@QEAAXH@Z` | 0x1800d4bb0 | **no** | — | — |
| `?SetValue@NumberElement@UI@@QEAAXH@Z` | 0x180118d50 | yes | prefix replay | `UI::NumberElement::SetValueInt` |
| `?SetValue@NumberElement@UI@@QEAAXM@Z` | 0x180118db0 | yes | prefix replay | `UI::NumberElement::SetValueFloat` |
| `?SetText@EditElement@UI@@QEAAXAEBVString@System@@@Z` | 0x1800d0db0 | yes | **none (see §5)** | `UI::EditElement::SetText` |
| `?SetText@EditElement@UI@@QEAAXVString@2@@Z` | *** NOT IN EXPORT TABLE *** | n/a | — | resolved as a bug fix, §5 |
| `?SetParameter@SliderElement@UI@@QEAAXHAEBVString@System@@@Z` | 0x1800d6ca0 | **no** | — | — |
| `?FindCheckBoxes@RadioElement@UI@@AEAAXV?$InternalPtr@VElement@UI@@@Object@@@Z` | 0x1800d5750 | **no** | — | — |
| `?FindCheckBoxes@RadioElement@UI@@QEAAXXZ` | 0x180047c00 | **no** | — | — |
| `?SetActiveRadio@RadioElement@UI@@QEAAXH@Z` | 0x1800d56f0 | yes | prefix replay | `UI::RadioElement::SetActiveRadio` |
| `?SetCurrentValue@ProgressElement@UI@@QEAAXM@Z` | 0x180077ed0 | yes | prefix replay | `UI::ProgressElement::SetCurrentValue` |
| `?SetMaximumValue@ProgressElement@UI@@QEAAXM@Z` | 0x180077ec0 | yes | prefix replay | `UI::ProgressElement::SetMaximumValue` |
| `?SetProgressTextParameter@ProgressElement@UI@@QEAAXHAEBVString@System@@@Z` | 0x180119b30 | **no** | — | — |

18 ported / 27 rows; one row (the by-value `EditElement::SetText`) is not a
real function at all. Every ported row is registered **twice**, under its
mangled name and under its retail address (`RegisterRuntimeAddr`), following
the Wave 6 lane A precedent — defensive against a future address-form call
site splitting one live object between two implementations.

---

## 2. The oracle, argued per shape

### (a) Replay — the seven pure getters

`GetMaxValue`, `GetValue` (×2, Spinner and NumberEditElement), `IsActive`,
`GetCurrentItem`, `GetItemCount`, `GetItemData`. None of these writes
anything. Call ours, call the DLL's own body on the SAME unmodified object,
`RT_DIFF_SCALAR` the two results. No snapshot needed — this is the plainest
oracle in the whole port.

### (b) Prefix replay — the ten mutators

`SpinnerElement::{SetElementId,SetIncrement,SetMinMax,SetValue}`,
`NumberEditElement::SetMinMax`, `NumberElement::{SetValueInt,SetValueFloat}`,
`RadioElement::SetActiveRadio`, `ProgressElement::{SetMaximumValue,
SetCurrentValue}`.

None of this lane's eight classes has a decompiled ctor/dtor, so **none of
them has a proven `sizeof`** the way `Video::Context` did in Wave 6 (that
lane had the literal allocation-size argument to cite). Rather than invent a
size, each header only asserts the offsets it can prove, and the oracle
compares `sizeof(OurStruct)` bytes — the compiler-computed size of exactly
the fields this lane named, plus whatever explicit padding bridges them.

The argument that this is still a valid oracle, not a guess dressed up as
one: the scratch copy is a **`memcpy` of the live, currently-correct object**,
taken *before* our body runs — not a freshly-constructed guess. Both bodies
then run starting from **byte-identical state**. Any divergence anywhere in
that `sizeof(OurStruct)` range is therefore a genuine behavioural difference
between the two implementations, regardless of how many of those bytes this
lane has individually named — exactly the argument `WAVE6_LANE_A.md` §4 makes
for `Video::Context`, just over a smaller, proven-lower-bound window instead
of a proven-exact one. What it does NOT catch: a divergence past the last
byte this lane's struct covers (the true retail object is presumably larger
for at least `ComboElement`, `RadioElement` and `ProgressElement`, which have
proven fields well past their struct's last member). Stated once here rather
than on every function.

**The shared-child caveat**, same shape as Wave 6 lane A's `AddVertex` note:
where a mutator reaches a CHILD object through a shared pointer copied
byte-for-byte into the scratch (Spinner's wrapped `NumberEditElement`, Radio's
checkbox array), replaying the DLL's original on the scratch touches the
**real, live child a second time** (the pointer value is identical; there is
no separate scratch child). Every function ported this way was checked to be
**idempotent under a repeated identical call** before being ported this way:

- `SpinnerElement::SetElementId` / `SetIncrement`: unconditional overwrites
  with the same input — trivially idempotent.
- `SpinnerElement::SetMinMax` / `SetValue`, `NumberEditElement::SetMinMax`:
  delegate the "commit as text" step to the REAL `NumberEditElement::SetValue`
  (§3, "delegated, not reimplemented"); calling a deterministic clamp-and-
  format function twice with the same final inputs reproduces the same string.
- `NumberElement::SetValueInt` / `SetValueFloat`: each has an explicit
  early-out guard (`if (mode == X && value == current) return`), so a second
  call with the same value is a no-op on the SECOND invocation specifically
  because the FIRST one already made the state match the guard.
- `RadioElement::SetActiveRadio`: same shape — the guard at the very top
  compares the incoming index against `_activeIndex`, so a scratch copy that
  still holds the OLD `_activeIndex` (the snapshot was taken *before* ours
  ran) does re-execute the child-clearing loop on replay, but every byte it
  writes to a child (`box[0xd8] = 0`) is either already 0 or is being set to
  0 again — a no-op either way.

**NOT done this way**: anything that swaps a REFCOUNTED pointer. See §5.

### (c) No oracle — `EditElement::SetText`

See §5.

---

## 3. The nine hold-backs

### `ComboElement::AddItem`, `RemoveAllItems`, `SetCurrentItem`

**Allocation and private-helper risk, the same shape as Wave 6 lane A's
`Video::Context` ctor/dtor holdback.** `AddItem` grows THREE parallel
`Collection::Array<T>` (strings, sprites, user data — `ComboElement.h`) via
three private, unexported growth helpers (`FUN_180034860`,
`FUN_18004bfc0`, `FUN_180024fe0`) that are instantiations of
`Collection::Array<T>::SetSize` and therefore reach
`System::MemoryAllocator::Allocate` under its own critical section.
`RemoveAllItems` is the mirror: it frees all three buffers (`ListCleanup`,
`FUN_180037310`, `FUN_180024aa0`) including per-element `System::String`
refcount teardown. `SetCurrentItem` additionally calls
`UserInterface::CloseDialog` — **one of this wave's six phantom names**,
owned by lane C and not yet fixed at the time this lane's recon ran — and a
private, unexported `UpdateText` helper this lane did not resolve. All three
are reversed in full (§6 below has the growth/free shapes) for whoever picks
them up once the allocator question and lane C's `CloseDialog` are settled.

### `NumberEditElement::SetValue`

**Not held back for allocation — held back for calling convention risk.**
Its body calls `System::String::Format((String*)local_res8, "%i", uVar2,
in_R9, 0xfffffffffffffffe)` — a **variadic** MSVC export. Calling a
variadic MSVC function from MinGW-compiled code across the two toolchains'
differing argument-promotion and register-shadow-space conventions is exactly
the kind of ABI-fragile call this project has been burned by before
(`reference_hidden_retslot_wild_write.md`, `reference_hidden_retval_and_int_pointer_args.md`
— both about getting an MSVC call shape subtly wrong and having it "work" by
accident). Reimplementing the refcount teardown of the temporary formatted
string afterward compounds the risk. **The function is NOT needed to port
everything else in this lane**: `SpinnerElement::SetMinMax`/`SetValue` and
`NumberEditElement::SetMinMax` all need "commit this int as the displayed
text", and all three get it by calling the REAL `NumberEditElement::SetValue`
export through `ResolveExportRaw` — an ordinary, non-variadic call site from
our side, carrying none of the risk above. Held back as a body; used
constantly as a dependency.

### `SliderElement::SetParameter`, `ProgressElement::SetProgressTextParameter`

Both resolve a CHILD element through the same "first child" pattern
`SpinnerElement` uses, then write into an indexed slot of that child's OWN
parameter array (`Collection::Array<System::String>` at the child's own
+0xc8/+0xd0/+0xd4 — a *different* class from anything in this lane's eight,
almost certainly `TextElement` itself, which is lane E's scope) via
`System::String::operator=` (refcounted assignment — the exact shared-state
risk `EditElement::SetText` hits, §5) and then call `TextElement::CacheText`
(0x18011ec10 — confirmed by `get_xrefs_to` to be the SHARED base
implementation these two use, as opposed to `NumberElement`'s own
0x1801189f0, which this lane DID resolve and use for `NumberElement`'s
setters). Held back because (a) the target object belongs to a class this
lane does not own or model, (b) `String::operator=`'s exact refcount sequence
was not independently confirmed the way `EditElement::SetText`'s manual
refcount dance was, and (c) per §5, a refcount swap has no safe replay oracle
regardless.

### `RadioElement::FindCheckBoxes` (both overloads)

The public zero-arg overload (0x180047c00) is a two-line forwarder to the
private one (0x1800d5750, which takes an `Object::InternalPtr<UI::Element>` —
passed as a pointer-to-pointer at the raw ABI level, matching the
`_children._data[0]` double-indirection pattern seen throughout this lane).
The private one recurses over the ENTIRE element tree, and for every child
whose dynamic type equals a cached checkbox type id
(`(**(code**)(*child+0x10))() == typeId` — a VIRTUAL call through the
child's own vtable, not `IsDerivedFrom`, note the difference from every other
type check in this lane) appends it to `RadioElement::_checkBoxes` via the
same private, unexported, allocating growth helper (`FUN_180048fe0`)
`ComboElement::AddItem` uses its three of. Held back for the same allocator
reason as `ComboElement::AddItem`, plus the added complexity of an
`InternalPtr` parameter and a recursive tree walk this lane did not fully
verify against `Element::_children`'s exact iteration order.

---

## 4. Layout notes worth restating here

**The "first child" pattern is not a dedicated field.** `SpinnerElement`,
`SliderElement` and `ProgressElement` (for the widgets this lane ported and
held back alike) resolve their wrapped value/label widget as
`Element::_children._data[0]` — the ordinary, INHERITED Element child array
(Element.h, lane A) — cast via `IsDerivedFrom`, not through a
`SpinnerElement`-owned pointer. This was the single biggest layout surprise
of this lane's recon: the existing comment in
`engine/funcs/engine_funcs.h:606-609` ("SetMinMax dereferences +0xa0 which is
NULL on SliderElements") reads, before this, like a `SpinnerElement`-private
field that happens to be unpopulated on a sibling class; it is actually
`Element::_children._data` being null because a `SliderElement` has no
children — the exact same field, the exact same offset, on every `*Element`
in the hierarchy. Worth flagging for lane A/E: no header change is being
requested, since `Element.h` already models `_children` correctly and this
lane's headers reuse it as-is — just noting that this fact generalises to any
future widget with a similar "wraps its first child" shape.

**`Element::_elementId` (+0x88) is shared, not shadowed.**
`SpinnerElement::SetElementId` writes it on BOTH the spinner and its child —
two independent instances of the SAME inherited field, not two different
fields with the same name. No dedicated `SpinnerElement` field was needed to
model this.

**`Collection::Array<T>`'s T-independent size (Wave 2's `Collection.h`) is
what makes `ComboElement.h` and `RadioElement.h` possible without knowing
`sizeof(System::String)` or `sizeof(UI::Sprite)` inside this lane**: the
container itself is always `{data, size, capacity}` at +0x00/+0x08/+0x0c
regardless of the element type, so `Array<void*>` reproduces the CONTAINER
layout exactly even where the element type used is not the real one (noted
in each header at the point it matters).

---

## 5. The phantom name: `EditElement::SetText`, resolved

`?SetText@EditElement@UI@@QEAAXVString@2@@Z` (a by-VALUE overload) is **not
in the shipped export table** — confirmed by the same pefile query every
wave has run before writing a body. Before this lane, `engine/funcs/
engine_funcs.cpp` resolved it, got NULL, and fell back to a **hardcoded
address, 0x180042070** — which decompiles to `UI::TextElement::SetText(UI::
String)`, a **different class's** by-value `SetText`, taking a 16-byte
`UI::String` (sheet+index) and writing to `this+0xd8`/`+0xe0`
(`TextElement`'s own cached-display-text field), then calling
`TextElement::CacheText`.

Called on an `EditElement` (which has a DEDICATED `System::String` field at
`+0xc8`, gated by the read-only-ish flag at `+0xe0` — this lane's own
`EditElement.h`), that fallback function:

- wrote 16 raw bytes from a `System::String` (only the low 8 are meaningful;
  the rest is caller-stack garbage) into `TextElement`'s OWN base field at
  `+0xd8`/`+0xe0` — which, on an `EditElement`, is **NOT** the `+0xe0`
  active/read-only flag this lane's own `EditElement::SetText` reads
  (`+0xd8`/`+0xe0` here are `TextElement`'s field pair, a DIFFERENT
  interpretation of that byte range than `EditElement`'s single-byte flag at
  the same nominal offset — the two classes are siblings, not one extending
  the other's layout at that point, so "the same offset" does not mean "the
  same field"),
- never touched the field the edit box actually displays (`+0xc8`),
- never respected the read-only-style guard `EditElement::SetText` itself
  checks,
- never managed the `System::String` refcount the way `EditElement::SetText`
  does,
- and called the WRONG `CacheText` for the object's real type.

The one live caller, `code/Game/Dialog/spritecaptureui.cpp:100`, has
therefore never actually set that edit box's default text since this call
site was written — a silent no-op dressed as a working call, in the exact
shape `reference_resolveexport_phantom_names.md` warns about.

**Scoping this precisely, per the standing "a phantom is not automatically
dead" check lane C's closeout asked every lane to re-run**: this call site is
`SpriteCaptureUI::Serialize` — the file's own header comment says it plainly,
"Dev tool for capturing UI element sprites." It is NOT a general player-facing
edit box; the earlier framing in this section ("can the player type into an
edit box and have it stick?", taken from the wave brief's phrasing of the
task) overstated the blast radius. The concrete, verified fact is narrower
and still real: this ONE dev-tool call site's default capture-path text has
never been set, and — unlike lane C's three phantom-adjacent names, which
all turned out to resolve to the SAME correct function through their address
fallback and were therefore benign — this fallback resolves to a DIFFERENTLY
BEHAVED function (wrong offsets, wrong CacheText, no refcount management, no
read-only guard), independently confirmed by decompiling both 0x180042070 and
0x1800d0db0 side by side (§ above). Applying lane C's test honestly changes
the audience (a modder using the sprite-capture tool, not a general player)
but not the verdict: this one is a real defect, not a masked non-issue.
Other `EditElement::SetText`-shaped call sites in the tree already went
through `fn_EditElement_SetText_SysStr`, which always resolved the correct
by-reference export and was never affected.

**Fix applied at the source** (`engine/funcs/engine_funcs.cpp`, `PLAN.md` §6
step 8): `fn_EditElement_SetText` is retargeted to resolve the REAL, exported,
by-reference `?SetText@EditElement@UI@@QEAAXAEBVString@System@@@Z`
(0x1800d0db0) — the identical raw ABI (`self`, pointer-to-String either way),
so `spritecaptureui.cpp:100` needs no change and now actually works. The
header comment above `fn_EditElement_SetText_t` was rewritten to record the
whole story so nobody re-derives it.

**This lane ALSO ports the real `EditElement::SetText` as a Wave 7
`RegisterRuntimeExport` (§1), with an important caveat: it carries NO REPLAY
ORACLE.** Unlike this lane's other mutators, `SetText` swaps a REFCOUNTED
shared string: it decrements the OLD value's refcount (freeing the buffer via
`MemFree` if that hits zero) and increments the NEW one's. Replaying the
DLL's original body a second time on a byte-copy — this lane's usual
technique — would decrement/increment those SAME shared refcounts a second
time, which is wrong regardless of whether the first decrement already freed
the buffer (a use-after-free if it did). This is PLAN §4.3 risk 2's territory
exactly, and is why `WAVE7.md` allows "a written NO-ORACLE holdback for
anything that allocates, frees, or reshapes a tree." Rather than hold the
port back entirely — the phantom-name bug is a real, currently-live
functional gap, and the resolution-layer fix above already delivers the
actual bug fix on its own — this lane ALSO registers a from-scratch
reimplementation for completeness, with correctness resting on matching the
decompile instruction-for-instruction (recorded above) rather than on any
cross-implementation check. If a future lane wants a verified version, the
honest path is a SHADOW oracle that inspects the refcount arithmetic's
inputs/outputs without re-running the original.

---

## 6. Reversed for the next lane (holdbacks' bodies, not landed)

**`ComboElement::AddItem`** (0x1800750b0): appends to all three parallel
arrays in lockstep —
`Array<String>::SetSize(&_strings, _strings._size+1, &newString)`,
`Array<Sprite>::SetSize(&_sprites, _sprites._size+1, &newSprite)`,
`Array<void*>::SetSize(&_userData, _userData._size+1, &newUserData)` — and
returns `_strings._size - 1` (the just-appended index) as the new item's
index.

**`ComboElement::RemoveAllItems`** (0x180075120): `ListCleanup(&_strings)`
(presumably per-element `String` release + `Array::SetSize(0)`),
`Array<Sprite>::Clear(&_sprites)`, `Array<void*>::Clear(&_userData)`, then
`_currentItem = -1`.

**`ComboElement::SetCurrentItem`** (0x180112540): if `_openDialog != 0`,
calls `UserInterface::CloseDialog(_openDialog->vtable_or_field+0x10,
&_openDialog)` and zeroes `_openDialog`; then, if the new index differs from
`_currentItem`, either sets it (when `0 <= index < _strings._size`) or resets
it to 0 — **not -1** — when out of range; either branch ends by calling the
private `UpdateText(this)` (retail address not resolved by this lane).

**`RadioElement::FindCheckBoxes`** (private, 0x1800d5750): for the passed-in
element, calls its type-id VIRTUAL function (vtable slot 2, per `Element.h`'s
"Type()") and compares against a cached checkbox type id; if equal, appends
it to `_checkBoxes` via `FUN_180048fe0` (the same growth shape as
`ComboElement`'s arrays). Then recurses into every child of the passed
element (`Element::_children`, iterating `_data[0.._size)`), skipping null
entries. The public overload (0x180047c00) just calls this with `this`
itself as the starting element.

---

## 7. What this lane could NOT verify

1. **No run happened.** Per the lane brief, no build/deploy/run of any kind.
   `RtUiWidgetsInRegister()` returning 18 and the oracle design are
   source-level facts; **zero diffed calls have executed.** Until
   `BANISHED_RT_DIFF=1` runs, every row in §1 is "expected clean". Kill
   switch: `BANISHED_RT_UIWIDGETSIN=0`.
2. **The `NumberEditElementTypeId()` global (0x1801fa190) was read as a
   plain `.data` value, never observed populated at runtime.** Retail reads
   it unconditionally with no lazy-init branch, so it must already be set by
   the time any `SpinnerElement` method is reachable — but this lane did not
   watch it happen.
3. **`EditElement::SetText`'s refcount arithmetic was not exercised.** No
   oracle, per §5 — correctness rests entirely on the decompile transcription
   above being right, not on any runtime check.
4. **The nine held-back functions' bodies (§6) are read from Ghidra's C only
   where noted, not fully cross-checked against the raw disassembly** the way
   `reference_ghidra_c_output_vs_disassembly.md` recommends for a LANDED
   port — acceptable for a holdback record, not for a port, which is exactly
   why they are holdbacks.
5. **Nothing here was measured for performance.** All oracle code is inside
   `if (rt_diff_on__)`; a normal run pays only for the `RT_DIFF_BEGIN` site
   registration check.
6. **The true `sizeof` of all eight classes remains unproven.** Every prefix
   replay's window is a lower bound (§2b). A retail-only divergence past that
   window would be invisible to this lane's oracle.

---

---

## 8. Ghidra

Everything in this lane was read with `program="Runtime-x64.dll"` on every
call. No function was renamed and no other lane's view of the shared project
changed. Two disambiguations worth recording for future lanes:
`search_functions_enhanced(name_pattern=...)` treats `?`/`@` specially and
returns garbage for a mangled-name substring search — plain short names
(`"CacheText"`, `"ToInteger"`) work; mangled names do not. `get_xrefs_to` on
both `CacheText` candidates (0x1801189f0, 0x18011ec10) is what separated
`NumberElement`'s own override from the shared `TextElement` one — a bare
address match without checking callers would have picked either
indiscriminately.

---

## 9. Owed answer — does any of this lane's classes override `Element` vtable slot +0x48 (`Contains`)?

**No. All eight classes use Element's own `Contains` (0x1801169e0), unmodified.**

Asked because lane C's `UserInterface::GetFirstContainedDialog` port calls
that virtual, so under lane C's replay oracle it runs twice per diffed call;
if any widget this lane owns overrode it with a mutating body, the
divergence would surface as an apparent bug in lane C's file when the real
cause is here — the "RemoveMesh"-shaped attribution trap.

Checked directly against the shipped binary rather than inferred: each
class's exported vtable (`??_7<Class>@UI@@6B@`) was read with
`read_memory` and slot 9 (byte offset 0x48, `Element.h`'s `Contains`) decoded
as a little-endian qword and compared against `Element`'s own value.

| Class | Vtable | Slot +0x48 value | Overridden? |
|---|---|---|---|
| SpinnerElement | 0x180157660 | 0x1801169e0 | no |
| ComboElement | 0x180159030 | 0x1801169e0 | no |
| NumberEditElement | 0x180156f70 | 0x1801169e0 | no |
| NumberElement | 0x1801574d0 | 0x1801169e0 | no |
| EditElement | 0x180156810 | 0x1801169e0 | no |
| SliderElement | 0x180157580 | 0x1801169e0 | no |
| RadioElement | 0x1801573c0 | 0x1801169e0 | no |
| ProgressElement | 0x180159180 | 0x1801169e0 | no |

Every one of the eight reads exactly `0x1801169e0` — `Element::Contains`
itself — at that slot. (Slots 4-8, ComputeSize/ComputeLayout/Draw/Update/
Enable, DO vary per class, as expected for real per-widget behaviour; slots
0-2 vary too, being TypeName/Type/the scalar dtor. Slots 3, 10 and 11
— Persist, SetColor, GetBorder — are unmodified across all eight as well,
for what it's worth.) So this lane closes the hazard cleanly: nothing it
owns can make lane C's double-`Contains`-call assumption wrong.

**Standing-rule compliance check, also owed**: "register any port of a
phantom-adjacent name under both the export name and the address" — already
true for `EditElement::SetText` before this note was received (§1:
`RegisterRuntimeExport` under the real by-reference mangled name, PLUS
`RegisterRuntimeAddr(0x1800d0db0, ...)`). Confirmed against
`standalone/resolve_standalone.cpp`: `ResolveExport` checks the Wave-7
registry (`RtExportReg`) before falling through to `GetProcAddress`, and
`RuntimePortInit()` runs before `EngineFuncsInit()`
(`standalone/main.cpp:1614` then `:1631`) — so `engine_funcs.cpp`'s own
`fn_EditElement_SetText` / `fn_EditElement_SetText_SysStr` resolves, through
its now-corrected `ResolveExport` call, straight to this lane's
`RtUi_EditElement_SetText` once the port is registered, with no separate
wiring needed.
