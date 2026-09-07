# Wave 7 — Lane E — Layout / content widgets

Branch `wave/upgrade-path-complete`. Files owned by this lane:
`runtime/src/ui/widgets_layout.cpp`, `runtime/types/ui/{TableElement,TextElement,
DocumentTextElement,TabElement,PageElement,GraphElement}.h` (new), plus corrections
to the pre-existing `runtime/types/ui/{ImageElement,LabelElement}.h`, and this
document.

**Result: 20 of 24 ported, 4 held back with written reasons.**
`RtUiWidgetsLayRegister()` returns **20**. `-fsyntax-only` with the wave's exact
command line is warning-clean (`-Wall -Wextra`, verified below). All 24 mangled
names were checked against the shipped export table with a pefile query
**before any body was written** — none of this lane's six WAVE7.md phantoms
apply here (all six belong to lanes B/C/D).

**Revision note**: this doc's first version claimed 20 replay oracles that
were shown on review to be vacuous — see §4 for the finding and the fix.
9 of the 20 ported mutators now have a genuinely falsifiable restore-based
oracle; the other 5 (plus `TableElement::SetChild`'s two overloads, counted
once) are registered but explicitly marked NO ORACLE in the source, because a
safe fix would require freeing/refcounting machinery this lane judged out of
scope. The four true getters were unaffected — they were always independent
replays and are correctly done.

Not run by this lane, by instruction: the game, `make`, `tools/rt_run.sh`,
`sa-run`, `git commit`. Every runtime number below is a *prediction of what the
oracle will report*, not a measurement — see §7.

---

## 1. The function table

| Export | Retail | Ported | Oracle |
|---|---|---|---|
| `?GetColumnCount@TableElement@UI@@QEBAHXZ` | 0x180079460 | yes | true replay |
| `?GetRowCount@TableElement@UI@@QEBAHXZ` | 0x180079450 | yes | true replay |
| `?SetChild@TableElement@UI@@…H@Z` (1-int overload) | 0x18011daa0 | yes | **NO ORACLE** — see §4a |
| `?SetChild@TableElement@UI@@…HH@Z` (2-int overload) | 0x18011db50 | yes | delegates to the row above |
| `?SetSize@TableElement@UI@@QEAAXHH@Z` | 0x18011db90 | yes | restore-based, own fields only — see §4 |
| `?GetSprite@ImageElement@UI@@QEBA?AVSprite@2@XZ` | 0x1801182a0 | yes | true replay |
| `?SetRotation@ImageElement@UI@@QEAAXM@Z` | 0x1801182c0 | yes | restore-based (falsifiable) — see §4 |
| `?SetSprite@ImageElement@UI@@QEAAXVSprite@2@@Z` | 0x180118600 | yes | restore-based (falsifiable) — see §4 |
| `?SetWidthHeight@ImageElement@UI@@QEAAXHH@Z` | 0x180118260 | yes | restore-based (falsifiable) — see §4 |
| `?_typeInfo@ImageElement@UI@@0VBaseTypeInfo@Object@@A` | 0x1801fac48 | **no** | — |
| `?GetSprite@LabelElement@UI@@QEBA?AVSprite@2@XZ` | 0x1800429d0 | yes | true replay |
| `?SetSprite@LabelElement@UI@@QEAAXVSprite@2@@Z` | 0x180042980 | yes | restore-based (falsifiable) — see §4 |
| `?SetParameter@TextElement@UI@@…@Z` | 0x18011e9b0 | yes | **NO ORACLE** — see §4a |
| `?SetText@TextElement@UI@@QEAAXVString@2@@Z` | 0x180042070 | yes | **NO ORACLE** — see §4a |
| `?GetLink@DocumentTextElement@UI@@…@Z` | 0x18003c0c0 | yes | true replay + refcount release |
| `?SetText@DocumentTextElement@UI@@…AEBVString@System@@@Z` | 0x18003c010 | yes | **NO ORACLE** — see §4a |
| `?SetText@DocumentTextElement@UI@@…VString@2@@Z` | 0x18003bfd0 | yes | restore-based (falsifiable) — see §4 |
| `?AddTab@TabElement@UI@@…@Z` | 0x1800da130 | **no** | — |
| `?SetActiveTab@TabElement@UI@@QEAAXH@Z` | 0x18004c890 | yes | restore-based (falsifiable) — see §4 |
| `?GetActivePage@PageElement@UI@@QEBAHXZ` | 0x1800d5100 | yes | true replay |
| `?SetActivePage@PageElement@UI@@QEAAXH@Z` | 0x1800d5110 | yes | restore-based, own field only — see §4 |
| `?AddGraph@GraphElement@UI@@QEAAXI@Z` | 0x180040620 | **no** | — |
| `?AddGraphPoint@GraphElement@UI@@QEAAXHH@Z` | 0x180040690 | **no** | — |
| `?RemoveGraphs@GraphElement@UI@@QEAAXXZ` | 0x180040610 | yes | weak invariant only (`count==0`), not comparative — see §4 |

Every retail address above was read out of the export directory with
`tools/pe_exports.py` and matches `wave7_lanes.txt` exactly (script output kept
in the session transcript, not reproduced here for space).

---

## 2. The four hold-backs

### `?_typeInfo@ImageElement@UI@@0VBaseTypeInfo@Object@@A`

A **data** export (storage class `0` in the mangling = static data member, not
a function), same shape as every DATA-export holdback in Waves 3–6: registering
it before cutover has no defined meaning in this port's seam
(`RegisterRuntimeExport` binds a function pointer). Its one caller,
`code/Game/Player/tool_base_createbar.cpp`, was checked: it resolves the symbol
today via `ResolveExport` and uses it purely as an opaque type-id key for a
lookup table, never dereferencing its contents — so the exe side is unaffected
either way and there is nothing for this lane to fix there.

### `?AddTab@TabElement@UI@@…@Z`

Reversed enough to know it is not a leaf accessor: it builds a temporary
`ElementDescription`, wires a `Dialog::_vftable_`-tagged local, constructs a new
child `Element` through `ElementDescription::CreateInstance`-shaped virtual
calls (`(**(code**)(*param_3 + 0x40))(...)`), appends it to the shared Element/
ContainerElement children array (`runtime/types/ui/TabElement.h`'s
`kChildrenData`/`kChildrenCount`, the SAME field `PageElement::SetActivePage`
below reads), bumps a running tab count at `this+0xc0`, and tears down five
local RAII-shaped temporaries (`FUN_180009460`/`180039440`/`180039360`/`180039280`)
plus a manual refcount release on a `System::String`. This is squarely the kind
of "port classes whole" territory PLAN §4.3 risk 3 warns about, and it mutates
by appending — not idempotent, so the replay trick this lane leans on
elsewhere (§4) does not apply. Porting it with confidence needs
`ElementDescription::CreateInstance` (lane B territory) reversed first. Held
back rather than guessed.

### `?AddGraph@GraphElement@UI@@QEAAXI@Z` and `?AddGraphPoint@GraphElement@UI@@QEAAXHH@Z`

Both are genuine appends (WAVE7.md names them explicitly as replay-invalid), and
this lane agrees with that call: `AddGraph` grows the `this+0xf0` graph-series
array by one element via the non-exported `FUN_1800417d0` and writes a color
into the new element; `AddGraphPoint` grows a *nested* `Array<int32_t>` living
inside one series (`(*(this+0xf0))[graphIndex] + 0x08`) via the same
`Array<uint32_t>::SetSize` instantiation at 0x180023830 that
`runtime/src/graphics/scene.cpp`'s `RtArrayU32SetSize` already calls for
`Graphics::Scene::RemoveObject` — independent confirmation the address is
right. A byte-faithful port of the top-level orchestration is not the hard
part; verifying it is. Calling both ours and retail would append twice, and a
shadow byte-compare would need the full `GraphSeries` (0x18-byte) struct
reversed, which this lane did not do (only its first 4 bytes, the color, and
its `+0x08` nested array are proven). Rather than register an append with no
oracle at all — the exact WAVE2 allocator trap ("an unmeasured port is not a
port") — both are held back. The reversed top-level shape, for whoever lands
this once `GraphSeries` is fully reversed:

```
AddGraph(color):
    count = this._graphs.size
    this._graphs.SetSize(count + 1)   # via FUN_1800417d0, default-fills the new element
    this._graphs[count].color = color # first 4 bytes of the 0x18-byte element

AddGraphPoint(graphIndex, value):
    points = this._graphs[graphIndex] + 0x08      # nested Array<int32_t>
    points.SetSize(points.size + 1, &value)         # via the shared 0x180023830 SetSize
```

---

## 3. Layout — offsets against `UI::Element`, confirmed mid-wave

This lane's recon started before lane A had published `runtime/types/ui/
Element.h` (`docs/runtime_port/WAVE7.md`, "the cross-lane layout contract"), so
none of its eight classes were modeled as C++ structs inheriting `UI::Element`
at first — every header instead declared **raw byte offsets from `this`**,
each proven directly against a disassembled retail address, to avoid guessing
at a layout lane A owns (PLAN §4.3 risk 3, the two-layouts-in-play hazard).

**`Element.h` landed while this lane was still working, and independently
confirms every inference this lane had already made from three of its own
bodies** (`TableElement::SetChild` 0x18011daa0, `TabElement::AddTab`
0x1800da130, `PageElement::SetActivePage` 0x1800d5110): `sizeof(UI::Element) ==
0xC0`, `Element::_flags` (the dirty-flags word) at `+0x34` with the bit this
lane calls "dirty" being exactly `kElementFlagNeedsSizeCalc`, `Element::_parent`
at `+0x98`, and `Element::_children` — a `Collection::Array<Element*>` — at
`+0xA0`, which is the SAME array `TableElement` treats as its cell grid,
`TabElement::AddTab` appends tabs into, and `PageElement::SetActivePage`
iterates to find each page's `Element*`. A second independent confirmation:
this lane's own reversal of `PageElement::SetActivePage`'s per-child bit-1
toggle ("hidden means not the active page") turns out to be exactly
`UI::kElementFlagHidden` (bit 0, "hidden, not visible") from `Element.h`'s own
`_flags` enum — a bit this lane found from a completely different function
than the ones lane A used to name it.

**Headers were updated once `Element.h` landed** (`TableElement.h`,
`TextElement.h`, `DocumentTextElement.h`, `TabElement.h`, `PageElement.h`) to
express the shared offsets as `offsetof(UI::Element, _flags)` /
`offsetof(UI::Element, _parent)` / `offsetof(UI::Element, _children)` and
`UI::kElementFlagNeedsSizeCalc` / `UI::kElementFlagHidden`, rather than a
second copy of the magic numbers next to lane A's. Each widget class's OWN new
fields (TableElement's `rowCount`/`colCount`/row-and-column-size arrays,
TextElement's parameter array and text value, PageElement's `activePage`, …)
remain this lane's own literals, still starting at `+0xC0` as `Element.h`'s
`sizeof(Element) == 0xC0` requires.

**Two per-offset findings that correct the pre-existing `ImageElement.h`/
`LabelElement.h`** (PLAN §6 step 8 — applied at the source, see the diffs to
those files):

1. `ImageElement.h`'s `+0xD0` field was commented "zeroed in ctor (image
   mode/flags?)". `ImageElement::SetRotation` (0x1801182c0) proves it is a
   **plain `float` rotation in degrees**, written with **no dirty-flag walk**
   — the one setter in this lane's whole scope that skips it. Corrected in
   place; the field is renamed `rotation`.
2. `LabelElement.h`'s "TextElement base (0xC0..0xFF)" comment block invented a
   layout (text string at `+0xC0`/`+0xC8`, font at `+0xD0`, color at `+0xE0`,
   …) that **disagrees with `TextElement::SetParameter` (0x18011e9b0) and
   `TextElement::SetText` (0x180042070)**: the actual text value is written at
   `+0xD8`/`+0xE0` (16 bytes), the parameter array's data pointer is at `+0xC8`
   with its count at `+0xD0`, and the old table's `+0xE0` "color" claim
   collides with the second half of the real text field. The block is kept in
   the file crossed out (marked WRONG) rather than deleted, so the next reader
   sees what was disproved and why, with the corrected layout immediately
   below it and full evidence in `runtime/types/ui/TextElement.h`.

`LabelElement.h`'s own fields at `+0x100` and above (its Sprite, alignment,
etc.) were **not** touched — this lane's two LabelElement functions
(`GetSprite`/`SetSprite`) only read `+0x100`/`+0x108`, which the decompile
confirms exactly matches the existing header, so nothing there needed
correcting.

---

## 4. The oracle, reworked after review

**This section originally described a "call ours, then call retail for real on
the same already-mutated object, then compare" design and argued it was sound
because every mutator in scope is idempotent.** That argument was correct about
idempotence and *wrong about what the resulting comparison could detect*: the
review below is preserved because the mistake is worth a future lane not
repeating, not because the design stood.

### The defect: a comparison that cannot fail

Take the original `ImageElement::SetWidthHeight`, verbatim:

```
1. ours writes w,h through OUR offset constants (+0xD4/+0xD8)
2. retail runs FOR REAL on the same live object, which ALREADY holds w,h
3. compare the live field (now written by BOTH, in sequence) against w
```

Walk every way this could be wrong: wrong offset — we wrote `w` through the
wrong constant, retail then writes `w` through the TRUE offset, step 3 reads
back through our wrong constant and finds the value retail just put there;
passes. Wrong value — retail overwrites it with the right one; passes. Missed
side effect — retail performs it, step 3 never looks; passes. **The assertion
can only fail if neither side writes at all.** It is a self-consistency check
reading through the same constant it wrote through, with retail's real call in
between silently repairing any damage first — and it is *worse* than merely
uninformative, because `RtDiffSummary()`'s `divergences=0` reads as a clean
result to anyone who has not read the code.

**A second, sharper failure mode compounds the first.** `SetWidthHeight` and
`SetSprite` (both classes) take an early-out on "value did not change" before
touching the dirty-flag chain — the part of each function most likely to have
a wrong bit or a wrong walk. Because ours had *already* written the field
before retail's real call ran, retail's own guard saw "no change" and never
executed its own walk *at all* — so the riskiest code path was not merely
weakly checked, it was **never entered by either side during the diff call**.
This is lane C's `EnableCursor` finding, hit independently in this lane
without recognising it at the time: our own prior write satisfies the second
implementation's guard and the interesting branch goes untested while the
comparison reports success.

**Third**, calling retail's real implementation from inside every diffed call
means `BANISHED_RT_DIFF=1` mutates each object twice per call — a diff run is
then measuring a different execution than the shipping one, for the specific
functions that matter most to get right.

### The fix: restore pristine state before retail's real call, where that is safe

The corrected pattern, applied to every mutator whose full mutation footprint
is proven self-contained bytes on `self` with no free or refcount step in it
(`ImageElement::SetRotation/SetSprite/SetWidthHeight`, `LabelElement::
SetSprite`, `DocumentTextElement::SetText[UI::String]`, `TabElement::
SetActiveTab`, `PageElement::SetActivePage`'s own field, `TableElement::
SetSize`'s own `rowCount`/`colCount`):

```
1. capture the PRISTINE footprint bytes, before ours runs
2. run ours for real on the live object (this is what stays live afterward)
3. capture what ours produced
4. write the PRISTINE bytes back — undoing step 2's field writes only
5. call retail for real: it now sees the true pre-call state, so its own
   "did this change" guard fires for real and its own dirty-walk / helper
   call actually executes
6. capture what retail produced, compare against step 3's capture
7. write step 3's (OUR) result back, regardless of the comparison's outcome
   — the port's own answer is what the game keeps running on
```

This is genuinely falsifiable: a wrong offset means step 3 and step 6 read
different memory than intended, and now nothing has quietly repaired that
before the compare. And it exercises the risky branch for real, because
retail is handed the true pre-call state rather than a state ours already
normalized. Traced against the three failure modes the old design could not
catch (wrong offset, wrong value, missing dirty walk), all three now diverge —
wrong offset: `oursW` reads back the wrong-constant write, the restore puts
`pristineW` back through the SAME wrong constant, retail then writes the TRUE
offset and leaves the wrong constant holding `pristineW`, so the compare is
`w` vs `pristineW` — a real mismatch. Wrong value: compare is `v` vs `w`
directly. Missing dirty walk: `oursDirty == pristineDirty` while retail's
walk, now firing for real, changes the live word — caught, and this is the
one that matters most, since it was the one blind spot the old design could
not see even in principle.

**Two residuals of this fix, stated as scope rather than left implicit:**

1. **The restore only restores `self`'s own dirty-flags word, not the parent
   chain `WalkDirty` climbs.** So after ours' walk, every ancestor already has
   the bit set; retail's walk (now firing for real against the restored
   `self`) re-sets the SAME bit on the SAME ancestors — a harmless no-op, since
   OR is idempotent, and not a false pass. But it means this oracle verifies
   the dirty walk's effect **on `self` only**; a divergence in HOW FAR up the
   chain the walk goes (a bug that stops one ancestor early, or climbs through
   the wrong `_parent` pointer) would not be seen, because both ours' and
   retail's version of that walk already ran to completion on the real
   ancestors before either snapshot was taken.
2. **A diff run now walks the parent chain twice per diffed call** (once for
   ours, once for retail's real re-run) instead of once. Harmless for the same
   idempotence reason, but it means `BANISHED_RT_DIFF=1`'s behaviour is not
   byte-identical to a normal run's — worth remembering if a future divergence
   ever looks timing- or ordering-dependent rather than value-dependent.

### 4a. Where the fix is NOT safe: registered, but marked NO ORACLE

The function is instead marked NO ORACLE in the source and left registered
(option (b), the WAVE2 allocator precedent — a holdback with a reason is a
success, not a failure):

* `TableElement::SetChild` (both overloads) — if the previous cell occupant
  was actually released (its scalar-deleting destructor called), restoring
  the cell slot to point at it and letting retail dereference `*puVar4`'s
  vtable is a use-after-free, not a diagnostic. There is no way to restore
  "the state before a delete" without the object still existing.
* `TextElement::SetParameter`, `TextElement::SetText`,
  `DocumentTextElement::SetText[String const&]` — each does a real
  refcount acquire/release on a shared `System::String`. If the OLD string's
  refcount hit zero and was freed by our own call, restoring the raw pointer
  field to point at it and letting retail's `String::operator=` touch it is
  the same use-after-free hazard.

Both hazards are the SAME shape: **the safe direction for "undo" is restoring
a plain value, never resurrecting something that was freed.** Every function
in the safe list above only resets POD fields (ints, floats, or a
non-refcounted 16-byte value type — `UI::String`/`UI::Sprite`, proven
non-refcounted in `runtime/types/ui/Element.h`'s own `UiString` evidence and
this lane's original `Sprite.h` reading). Every function on the holdback list
does or can free something. A proper fix for the holdback list needs a
disposable owned object built and torn down by the diff itself — judged out of
scope for this pass; see WAVE7_LANE_E.md's per-function comments for exactly
what would need to exist.

`TableElement::SetSize` sits in between: its own `rowCount`/`colCount` fields
are safely restorable (plain ints), but its two `Array<int32_t>::SetSize`
calls and its `RemoveChildren` call are literally invocations of retail's own
code (resolved by address, not reimplemented — §5), so restoring
`rowCount`/`colCount` does not change what those calls do, and there is no
second, independent implementation of the array/child logic for a live compare
to be checking. The fix therefore restores and re-verifies only the two scalar
fields, and the doc says plainly that the array/RemoveChildren correctness is
a source-level fact (we call retail's own functions) rather than a measured
one.

`GraphElement::RemoveGraphs` is a similar shape one step further: `ours` calls
retail's own teardown helper directly, so there is no reimplementation to
diff against at all. Its check (`count == 0` after both calls) is kept but now
explicitly labeled a weak invariant, not a comparison — it cannot detect a
wrong array-header offset the way a true replay would, because reading
garbage that happens to already be zero would still pass.

`DocumentTextElement::GetLink` and every `Get*` in this lane's scope needed no
rework: they were always a true, independent replay (retail runs into a
SEPARATE scratch output, never touching the live object), which is exactly
what makes them immune to both defects above — there is no live object for
either side's write to be "repaired" by the other, and no early-out for a
prior write to have already satisfied.

### 4b. `UI::Sprite`/`UI::String` byte-copy widths: 16, not 12 — because these are copies, not constructions

Lane F published `runtime/types/ui/Sprite.h` mid-rework with
`kSpriteWrittenBytes = 0x0C`: `UI::Sprite` is 16 bytes, but none of its three
constructors write the last 4 (the pad at `+0x0C`). The reviewer initially
asked this lane to use that constant in every Sprite-shaped byte comparison —
then corrected that instruction before this lane acted on it, because lane A
had hit the identical question for `UI::String` and shown the constant does
NOT generalize: it bounds what a *constructor* writes, not what every function
that touches the type writes. `Element::SetToolTip` copies a full 16 bytes
into its `UiString` field (a qword MOV that lands on the exact byte the ctors
skip), so narrowing there would have dropped a real write.

**The rule, as corrected: name the writer before narrowing a comparison.**
This lane has five functions that copy a 16-byte Sprite-shaped or
`UI::String`-shaped value — `ImageElement::GetSprite`/`SetSprite`,
`LabelElement::GetSprite`/`SetSprite`, `DocumentTextElement::SetText[UI::String]`
— and checked every one at the *instruction* level rather than assume the
constant applied or didn't:

| Function | Retail | Instructions |
|---|---|---|
| `ImageElement::GetSprite` | 0x1801182a0 | `MOV RAX,[RCX+0xC0]`/`[RDX],RAX` then `+0xC8`/`+0x8` — two unconditional 8-byte MOV pairs |
| `ImageElement::SetSprite` | 0x180118600 | `MOV RAX,[RDX]`/`[R8],RAX` then `+0x8`/`+0x8` — same shape |
| `LabelElement::GetSprite` | 0x1800429d0 | same two-qword-pair shape at `+0x100`/`+0x108` |
| `LabelElement::SetSprite` | 0x180042980 | same two-qword-pair shape at `+0x100`/`+0x108` |
| `DocumentTextElement::SetText[UI::String]` | 0x18003bfd0 | `MOV RAX,[RDX]`/`[RCX+0xC0],RAX` then `+0x8`/`+0xC8` — same shape |

**None of these five is a constructor call.** Every one is retail copying an
existing 16-byte value from one location to another, unconditionally, in two
8-byte MOVs with no narrower variant anywhere in the instruction stream. So
16 is the width retail itself moves, and 16 is what this port's writes and
comparisons use throughout — `kSpriteWrittenBytes` does not apply to any of
them, and using it would have made this port write (and compare) 4 bytes
fewer than retail's own copies do, which is a real infidelity, not a
conservative choice. Each site carries a one-line comment in
`widgets_layout.cpp` citing this evidence so a future reader does not have to
re-derive it. This lane's earlier written claim ("only copies 16 raw bytes,
doesn't need `sizeof(UI::Sprite)` to be right") was answered correctly by
instinct but for the wrong reason — the right reason is the instruction-level
copy width, which happens to equal `sizeof`, not "raw bytes don't need types."

### `DocumentTextElement::SetText(String const&)` — a genuine retail
under-determinism, not a port defect

Retail's own codegen writes the reset value at `this+0xC8` as
`CONCAT44(uStack_c, 0xffffffff)` — `uStack_c` is an **uninitialized stack
local**, never assigned before this store. Retail's own high dword at
`+0xCC` is therefore whatever garbage happened to be on the stack that call,
not a deterministic value. This port writes only the low dword (`-1`, the
real index reset) and leaves `+0xCC` untouched; the diff comparison
deliberately excludes it
(`reference_oracle_must_not_compare_unwritten_bytes.md` — comparing a byte
neither side reliably writes reads as a catastrophic divergence and is not
one).

---

## 5. What lane E calls through, and why

Three internal Runtime helpers are genuine DLL exports (mangled-name resolved,
no reimplementation risk): `?RemoveChildren@Element@UI@@QEAAXXZ`,
`?CacheText@TextElement@UI@@AEAAXXZ` (private but exported — it owns
`String::FormatParamList` and refcount management this lane has no reason to
duplicate), and `?UpdateContainers@TabElement@UI@@AEAAXXZ` (private but
exported — visibility recomputation, not reversed by this lane).

Three are **not** exported at all — `Collection::Array<int32_t>::SetSize`
(0x18002fea0), `Collection::Array<InternalPtr<Element>>::SetSize`
(0x1800764c0), and the graph-array destroy-and-free helper (0x1800416d0) — so
they are resolved **by address** via a locally forward-declared `ResolveAddr`/
`Module`, the identical idiom `runtime/src/graphics/scene.cpp` already
established for the same reason (template instantiations carry no mangled
export). One of the two Array<T>::SetSize instantiations this lane calls
(0x180023830, used only in the *held-back* `AddGraphPoint`) is independently
corroborated by that same file's `RtArrayU32SetSize`.

---

## 6. Corrections applied at the source (PLAN §6 step 8)

* `runtime/types/ui/ImageElement.h`: `+0xD0` field renamed `rotation`,
  comment corrected (§3.1 above).
* `runtime/types/ui/LabelElement.h`: the "TextElement base (0xC0..0xFF)"
  comment block is marked disproved in place, with the corrected layout and
  full evidence added immediately below it (§3.2 above).

No other lane's files were touched.

---

## 6a. Does any of this lane's eight classes override `Element` vtable slot +0x48 (`Contains`)?

Asked by lane C: `UserInterface::GetFirstContainedDialog` calls this virtual, so
under a replay oracle it runs twice per diffed call, and a mutating override
anywhere in the widget hierarchy would misattribute a divergence to lane C's
file. Answered by reading all eight vtables directly out of the shipped DLL
(`??_7<Class>@UI@@6B@`, +0x48, 96 bytes each) rather than by inspecting source:

| Class | vtable | +0x48 (Contains) | Overrides? |
|---|---|---|---|
| `TableElement` | 0x180159260 | 0x1801169e0 | **No** — same as `Element::Contains` |
| `ImageElement` | 0x1801560b0 | 0x1801169e0 | **No** |
| `LabelElement` | 0x180156ad0 | 0x1801169e0 | **No** |
| `TextElement` | 0x180156b80 | 0x1801169e0 | **No** |
| `DocumentTextElement` | 0x1801566f0 | 0x1801169e0 | **No** |
| `TabElement` | 0x180157790 | 0x1801169e0 | **No** |
| `PageElement` | 0x180157080 | 0x1801169e0 | **No** |
| `GraphElement` | 0x1801569d0 | 0x1801169e0 | **No** |

All eight resolve to the identical address `0x1801169e0` — `Element::Contains`
itself. **None of this lane's classes overrides the hit test.** This closes
the hazard lane C flagged: a replay of `GetFirstContainedDialog` cannot
mis-mutate through any widget in this lane's scope, because there is nothing
here but the base implementation to call.

(Incidental, not asked, noted for whoever needs it later: while reading these
tables, `LabelElement` DOES override slot +0x50 `SetColor` — 0x180042970
instead of `Element::SetColor`'s 0x180035620 — and `DocumentTextElement`/
`TabElement`/`GraphElement`/`TableElement` override slot +0x38 `Update` with
their own bodies. Neither is in this lane's 24-function scope.)

---

## 7. What this lane could NOT verify

1. **No run happened.** Per instruction, this lane did not build, deploy, or
   run anything, including `-fsyntax-only`'s companion steps of actually
   linking or executing. `RtUiWidgetsLayRegister()` returning 20 and every
   oracle argument above are source-level design; **zero diffed calls have
   executed.** The kill switch is `BANISHED_RT_UIWIDGETSLAY=0`.
2. **`-fsyntax-only` was run**, with the exact `SA_CXXFLAGS`/`SA_INCLUDES` from
   the Makefile (`-std=c++17 -fno-strict-aliasing -Wall -Wextra
   -Wno-cast-function-type -MMD -MP -include standalone/env_cache.h`, `-I.
   -Iengine/types -Iengine/funcs -Icode/Game -Iruntime/types -Iruntime/funcs
   -Idx11/types -Idx11/funcs -Istandalone`), against
   `runtime/src/ui/widgets_layout.cpp` and each new/changed header standalone.
   All are warning-clean. This proves the code parses and type-checks; it does
   not prove any offset is correct beyond the disassembly evidence cited.
3. **The Element layout in §3 is now taken from lane A's published
   `Element.h`, not just this lane's own inference** — see §3 for the two
   independent confirmations found along the way. Everything in this lane's
   own headers still cites its own retail address as primary evidence, so a
   future change to `Element.h` cannot silently invalidate this lane's proofs
   the way it would have if this lane had only trusted the shared offsets.
4. **The restore-based fix in §4 was reasoned through, not exercised.** No
   diffed call has run, so "retail's own guard now sees the true pre-call
   state" is a claim about what the restored bytes will cause retail to do,
   not an observation of it happening. The five NO-ORACLE holdbacks in §4a are
   registered but genuinely unverified beyond normal play — that is the
   honest state of this lane's mutators until `BANISHED_RT_DIFF=1` runs for
   real.
5. **Nothing here was measured for performance.** Every diff site is inside
   `if (rt_diff_on__)`; a normal run pays only the direct call. The four
   `RT_ORIGINAL_ONCE`/address-resolved helper calls in `TableElement::SetSize`
   run unconditionally (they are not diff-only — they're needed to produce the
   correct result), each paying one cached-after-first-call indirection, which
   this lane believes is in the same cost class as retail's own calls to the
   same functions and did not attempt to measure.
