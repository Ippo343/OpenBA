# Wave 7 — Lane A — `UI::Element` + `UI::ContainerElement`

Branch `wave/upgrade-path-complete`. Files owned by this lane:
`runtime/src/ui/element.cpp`, `runtime/types/ui/Element.h`, this document, and
the three `engine/funcs/engine_funcs.h` declarations this lane's functions
disproved (§6).

**Result: 21 of 23 ported, 2 held back with written reasons.**
`RtUiElementRegister()` returns **21**. `sizeof(UI::Element) == 0xC0` (192),
alignment 8, twelve virtual slots. Compiling `element.cpp` with the wave's exact
command line at `-O0`, `-O2` and `-O3` is **warning-clean**;
`runtime/types/ui/Element.h` compiles standalone and `static_assert`s all 31
field offsets plus the size and the alignment. `tools/export_name_audit.py` is
at **3 pre-existing phantoms, none in this lane**;
`tools/abi_retslot_audit.py` reports **0 suspicious of 48**. One real defect
was found and **fixed at its source**: `fn_Element_SetPadding_t` declared its
middle two parameters in the wrong order (§6).

Not run by this lane, by instruction: the game, `make`, `tools/rt_run.sh`,
`sa-run`, `git commit`. Every runtime number below is therefore a *prediction of
what the oracle will report*, not a measurement — see §7.

---

## 1. The function table

Retail addresses and ordinals were read out of the export directory of
`C:\GOG Games\Banished\Runtime-x64.dll` with a pefile-style reader **before any
body was written** (`reference_resolveexport_phantom_names.md`: a name the DLL
does not contain resolves NULL in silence, the port still registers, and the
census still counts it). **All 23 names exist in the shipped DLL** — the six
Wave 7 phantoms are all in lanes B, C and D.

| Export | Retail | Ord | Ported | Oracle | Diff site name |
|---|---|---|---|---|---|
| `??0Element@UI@@QEAA@XZ` | 0x180115e50 | 171 | yes | build | `UI::Element::ctor` |
| `??1Element@UI@@UEAA@XZ` | 0x1801161a0 | 584 | **no** | — | — |
| `?AddChild@Element@UI@@QEAAXV?$InternalPtr@VElement@UI@@@Object@@@Z` | 0x180115df0 | 1537 | yes | shadow | `UI::Element::AddChild` |
| `?Child@Element@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@H@Z` | 0x180035540 | 1693 | yes | replay | `UI::Element::Child` |
| `?ChildCount@Element@UI@@QEBAHXZ` | 0x180035530 | 1695 | yes | replay | `UI::Element::ChildCount` |
| `?ComputeLayout@Element@UI@@UEAAXHHHH@Z` | 0x1801168d0 | 1763 | yes | thunk | `UI::Element::ComputeLayout[thunk]` |
| `?Draw@Element@UI@@UEBAXAEAVSpriteContext@2@E@Z` | 0x1801168f0 | 2243 | yes | thunk | `UI::Element::Draw[thunk]` |
| `?FindElement@Element@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@AEBVString@System@@@Z` | 0x180115650 | 2347 | yes | replay | `UI::Element::FindElement` |
| `?FindElementType@Element@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@I@Z` | 0x180115760 | 2349 | yes | replay | `UI::Element::FindElementType` |
| `?GetAlignedPosition@Element@UI@@QEAAXHHHHHHAEAH000_N1@Z` | 0x180115ff0 | 2442 | yes | outputs | `UI::Element::GetAlignedPosition` |
| `?GetPaddedSize@Element@UI@@QEAAXAEAH0@Z` | 0x180115f10 | 2830 | yes | outputs | `UI::Element::GetPaddedSize` |
| `?GetUserData@Element@UI@@QEAAPEAVBase@Object@@XZ` | 0x180115550 | 2997 | yes | replay | `UI::Element::GetUserData` |
| `?Hide@Element@UI@@QEAAX_N@Z` | 0x180035830 | 3056 | yes | replay | `UI::Element::Hide` |
| `?RemoveChildren@Element@UI@@QEAAXXZ` | 0x180115570 | 3552 | **no** | — | — |
| `?SetElementId@Element@UI@@QEAAXH@Z` | 0x180115820 | 3875 | yes | replay | `UI::Element::SetElementId` |
| `?SetFlagsOnOff@Element@UI@@QEAAXII@Z` | 0x180116160 | 3883 | yes | replay | `UI::Element::SetFlagsOnOff` |
| `?SetName@Element@UI@@QEAAXAEBVString@System@@@Z` | 0x1801154f0 | 3927 | yes | shadow | `UI::Element::SetName` |
| `?SetNeedsSizeCalc@Element@UI@@QEAAXXZ` | 0x180035470 | 3928 | yes | replay | `UI::Element::SetNeedsSizeCalc` |
| `?SetPadding@Element@UI@@QEAAXHHHH@Z` | 0x180115480 | 3935 | yes | replay | `UI::Element::SetPadding` |
| `?SetToolTip@Element@UI@@QEAAXVString@2@@Z` | 0x180035740 | 3989 | yes | replay | `UI::Element::SetToolTip` |
| `?SetToolTipString@Element@UI@@QEAAXAEBVString@System@@@Z` | 0x1800356e0 | 3990 | yes | shadow | `UI::Element::SetToolTipString` |
| `?SetUserData@Element@UI@@QEAAXPEAVBase@Object@@@Z` | 0x180115560 | 3998 | yes | replay | `UI::Element::SetUserData` |
| `?SetOffsets@ContainerElement@UI@@QEAAXHH@Z` | 0x1800cc1e0 | 3932 | yes | shadow | `UI::ContainerElement::SetOffsets` |

Every one of the 21 is registered **twice**, under its mangled name and under
its retail address, even though today every live call site in our tree uses the
name form (§5). The address registration costs nothing and closes the Wave 5
lane D failure mode where one live object is split between two implementations
because a later call site used the other door.

---

## 2. The vtable question, which is the one that matters for this class

`Element` is polymorphic with a deep derived hierarchy, so PLAN §4.3 risk 3
("port classes whole") is live here in a way it was not for Wave 6's
non-polymorphic `Video::Context`. It is defused, not ignored, by four specific
facts — each checked against the shipped binary rather than assumed:

**(1) `??_7Element@UI@@6B@` is an exported data symbol at 0x180156160, and that
is the exact address the shipped ctor stamps.** `Element::Element`'s second
instruction pair is `LEA RAX,[0x180156160] / MOV [RBX],RAX`. The ported ctor
resolves that symbol **by name** and stamps the same pointer, so an Element our
ctor built and an Element the DLL's ctor built carry the identical vtable, and a
virtual call from either side lands on the same twelve slots. This is Wave 6's
`??_7Geometry@Graphics@@6B@` lever, and unlike that one it was *verified in the
ctor* rather than inferred from the export existing.

**(2) The table is twelve slots, and that came out of the binary.** The next
exported vtable, `??_7ElementDescription@UI@@6B@`, is at 0x1801561c0 — exactly
0x60 bytes later. This is the discipline
`reference_vtable_slot_count_from_the_name_string.md` was written for: a table
sized to the base class's slot count mis-dispatches every derived virtual, and
in the tutorial wave that made slot 8 call the ctor and auto-pass 61 goals.

| slot | off | method | retail |
|---|---|---|---|
| 0 | 0x00 | scalar deleting dtor, called as `fn(this, 1)` | 0x180035c10 |
| 1 | 0x08 | `TypeName()` | 0x180035440 |
| 2 | 0x10 | `Type()` | 0x180035420 |
| 3 | 0x18 | `Persist(IO::Stream&)` | 0x180116870 |
| 4 | 0x20 | `ComputeSize()` — **`_purecall` in `Element`** | 0x18007ec28 |
| 5 | 0x28 | `ComputeLayout(x,y,w,h)` | 0x1801168d0 |
| 6 | 0x30 | `Draw(SpriteContext&, byte)` | 0x1801168f0 |
| 7 | 0x38 | `Update(...)` | 0x180116270 |
| 8 | 0x40 | `Enable(bool)` | 0x180116910 |
| 9 | 0x48 | `Contains(x, y, UserInterface*)` | 0x1801169e0 |
| 10 | 0x50 | `SetColor(uint)` | 0x180035620 |
| 11 | 0x58 | `GetBorder()` | 0x180035630 |

`??_7ContainerElement@UI@@6B@` (0x1801564b0) is 0x60 bytes too
(`??_7ContainerDescription@UI@@6B@` follows at 0x180156510), so
`ContainerElement` adds no virtuals.

**(3) No method in this lane is registered as a vtable slot.** Registering
redirects OUR direct callers only; the DLL's own internal near-CALLs and its
widgets' vtables still reach the DLL's bodies. So through the rest of Wave 7
there really are two implementations operating on one live `Element` — the
hazard WAVE7.md §1 names. It is safe because every ported body is a pure
function of the object's own fields; there is no cached derived state for the
two halves to disagree about.

**(4) The two functions that would have made this dangerous are held back.**
See §3.

---

## 3. The two hold-backs

### `??1Element@UI@@UEAA@XZ` (dtor, 0x1801161a0)

Two independent reasons, either sufficient.

**(a) Nothing in our tree calls it.** `fn_Element_Dtor`
(`engine/funcs/engine_funcs.h`) is assigned in `EngineFuncsInit` and never
invoked; a grep across `code/`, `engine/`, `standalone/`, `runtime/` and `dx11/`
finds the declaration, the assignment, and no call. A registered port on a path
nothing executes reports `calls=0 divergences=0` forever, which *reads* as a
pass — `reference_uncalled_port_is_not_verified.md` is exactly this trap, and a
wrong `Quaternion::operator*` survived the whole of Wave 1 that way.

**(b) It is the most destructive function in the class and has no oracle.** The
reversed body:

```
this->_vtable = ??_7Element@UI@@6B@        # re-stamp, the standard base-dtor step
RemoveChildren(this)                       # 0x180115570 — destroys every child
if (this->_border) { border->vtable[0](border, 1); this->_border = 0 }
Array_dtor(&this->_children)               # 0x180037240 — MemFree the buffer
release(this->_toolTipString)              # refcount at [-4], free at [-8]
release(this->_name)
~Object::Internal(this)                    # 0x1800bfdb0 — unregisters the handle
```

Replaying any of that destroys the objects a second time.

### `?RemoveChildren@Element@UI@@QEAAXXZ` (0x180115570)

Fully reversed and deliberately not landed:

```
for (i = 0; i < this->_children._size; ++i) {     # count re-read every iteration
    Element** data = this->_children._data;       # buffer re-read every iteration
    if (data[i]) { data[i]->vtable[0](data[i], 1); data[i] = nullptr; }
}
Array_SetSize(&this->_children, 0)                # 0x1800764c0 — frees the buffer
```

Slot 0 called as `fn(child, 1)` is the **scalar deleting destructor**: it frees
the child. There is no oracle for that. A replay destroys every child twice, and
a shadow predicate can only observe that the array ended up empty — which is
true whether or not the children were correctly destroyed, and is therefore the
kind of self-satisfied check that lets a defect ship.

The cost is real and is stated rather than hidden: `fn_Element_RemoveChildren`
has nine live call sites under `code/Game/Dialog/`, and all nine keep going to
the DLL. That is the right trade. An unverified port that frees objects is a
worse outcome than a hold-back (the WAVE2 allocator verdict: an unmeasured port
is not a port).

---

## 4. Layout recovered — `runtime/types/ui/Element.h`

`sizeof(UI::Element) == 0xC0` (192), alignment 8. **Proven twice, not
inferred:**

1. `UI::ContainerDescription::CreateInstance` (0x1800ccb90) allocates BOTH the
   `ContainerElement` and its `ClipRegionElement` with
   `MemoryAllocator::Allocate(_singleton, 0xd8, 8)`, calls `Element::Element` on
   each, and then writes the derived fields at +0xC0 … +0xD4.
2. `?GetWidth@ContainerElement@UI@@QEBAHXZ` (0x180039ee0) is one instruction,
   `MOV EAX,[RCX+0xC0]`, and `?GetHeight@` (0x180039ef0) reads +0xC4 — the first
   field a derived class owns is at +0xC0.

Corroboration: `Element::Element`'s last store is the qword at +0xB8, and
`runtime/types/ui/ImageElement.h` had independently recorded "UI::Element
(0xC0)" before this lane existed.

Inheritance is `Object::Base` → `Object::Internal` → `UI::Element`.
`Element::Element` calls `??0Internal@Object@@QEAA@XZ` (0x1800bfd10) first,
which stamps `??_7Base@Object@@6B@` (0x180154788) then `??_7Internal@Object@@6B@`
(0x180154720) over it — both exported, both addresses matching the
disassembly — writes the handle pair, and **registers the object in
`?_singleton@InternalFactory@Object@@0PEAV12@EA` (0x1801fc6d8)**.

### The offsets

A one-line exported accessor is the strongest kind of evidence there is: it
gives the offset AND the field's real engine name. Where one exists it is cited.

| off | field | proven by | retail |
|---|---|---|---|
| 0x00 | `_vtable` | ctor's stamp of `??_7Element@UI@@6B@` | 0x180115e50 |
| 0x08 | `_handleIndex` | `??0Internal@Object@@` | 0x1800bfd10 |
| 0x0c | `_handleId` | `??0Internal@Object@@` | 0x1800bfd10 |
| 0x10 | `_name` (System::String) | `?SetName@` | 0x1801154f0 |
| 0x18 | `_leftPad` | `?GetLeftPad@` | 0x1800355e0 |
| 0x1c | `_rightPad` | `?GetRightPad@` | 0x180035600 |
| 0x20 | `_topPad` | `?GetTopPad@` | 0x1800355f0 |
| 0x24 | `_bottomPad` | `?GetBottomPad@` | 0x180035610 |
| 0x28 | `_minWidth` | `?SetMinSize@` | 0x1801154a0 |
| 0x2c | `_minHeight` | `?SetMinSize@` | 0x1801154a0 |
| 0x30 | `_alignment` | `?SetAlignment@`; ctor default 3 | 0x180115750 |
| 0x34 | `_flags` | `?GetElementFlags@` | 0x1800354d0 |
| 0x38 | `_color` | `?SetColor@` (slot 10); ctor default 0xFFFFFFFF | 0x180035620 |
| 0x3c | `_mouseCursorIndex` | `?GetMouseCursorIndex@` | 0x180115740 |
| 0x40 | `_toolTipDialog` (`ExternalPtr<Dialog>`, 8 bytes) | `?GetToolTipDialog@` | 0x180035670 |
| 0x48 | `_toolTip` (`UI::String`, 16 bytes) | `?SetToolTip@` | 0x180035740 |
| 0x58 | `_toolTipString` (System::String) | `?SetToolTipString@` | 0x1800356e0 |
| 0x60 | `_layoutWidth` | `?GetLayoutWidth@` | 0x1800355c0 |
| 0x64 | `_layoutHeight` | `?GetLayoutHeight@` | 0x1800355d0 |
| 0x68 | `_layoutLeft` | `?GetLayoutLeft@` | 0x180035580 |
| 0x6c | `_layoutTop` | `?GetLayoutTop@` | 0x1800355a0 |
| 0x70 | `_layoutRight` | `?GetLayoutRight@` | 0x180035590 |
| 0x74 | `_layoutBottom` | `?GetLayoutBottom@` | 0x1800355b0 |
| 0x78 | `_unknown78` | ctor sets −1; **no accessor names it** | 0x180115ebb |
| 0x80 | `_userData` | `?GetUserData@` / `?SetUserData@` | 0x180115550 |
| 0x88 | `_elementId` | `?GetElementId@`; ctor −1 | 0x180115830 |
| 0x90 | `_dialogParent` (`InternalPtr<DialogInstance>`) | `?SetDialogParent@` | 0x180115630 |
| 0x98 | `_parent` | `?SetParent@` | 0x1801154d0 |
| 0xa0 | `_children` (`Array<Element*>`, 16 bytes) | `?ChildCount@` (+0xa8) / `?Child@` (+0xa0) | 0x180035530 |
| 0xb0 | `_border` (`InternalPtr<Element>`) | `?GetBorder@` (slot 11) | 0x180035630 |
| 0xb8 | `_description` (`ExternalPtr<ElementDescription>`) | `?GetDescription@` | 0x1800357d0 |

**Nothing here is a guess.** `_unknown78` is named honestly: the store at
0x180115ebb is real and no exported accessor claims it, so it is neither invented
nor silently omitted.

**Independent cross-check of +0x48.** `code/Game/Dialog/buildui.cpp:313` reads
`*(uint64_t*)(labelStorage + 0x48)` as the element's tooltip **string table** and
feeds it to `fn_UIString_Ctor` — written months before this lane and agreeing
exactly that +0x48 is `UI::String::_table`.

### `UI::String` is 16 bytes and is NOT refcounted

`?SetToolTip@` copies exactly two qwords from its by-value `String@2` argument
and never touches a refcount, unlike `?SetToolTipString@`, whose argument is a
`System::String` and which does the full release/acquire. That single fact is
what makes `SetToolTip` replayable and `SetToolTipString` not. `?IsToolTipValid@`
(0x180035640) splits the fields for us: it tests `+0x48 != 0` as a pointer and
`(int)+0x50 >= 0` as a signed index. Confirmed again by the `fn_UIString_Ctor`
call sites, which all pass a `uint8_t[16]`.

**Lane A and lane F derived this layout independently and agreed** — lane A from
`?SetToolTip@` / `?IsToolTipValid@` above, lane F from the three `UI::String`
constructors (0x18011da20/40/60) and `?IsValid@` (0x180035060) — both landing on
`{table +0x00, index +0x08, pad0C +0x0C}` at 0x10 bytes. That agreement is the
cross-check worth keeping; the duplicate is not. Lane A's local `UI::UiString`
has been **deleted**, and `Element.h` now includes lane F's
`runtime/types/ui/String.h` and uses `::UIString` for `_toolTip`. One source of
truth, and the field's type is now owned by the lane that owns the type.

One asymmetry this lane can add to it: `pad0C` (Element +0x54) is never written
by `Element::Element`, but it **is** written by `?SetToolTip@`, whose second
store is a full qword at +0x50. So it must be excluded from a comparison against
a freshly constructed object (it is, in the ctor's build oracle) and must NOT be
excluded from `SetToolTip`'s replay (it is not).

### Bytes NEITHER ctor writes — and why that matters

`Element::Element` **does not write +0x54, +0x60, +0x64, +0x7c or +0x8c.** They
keep whatever the allocator returned. `_layoutWidth`/`_layoutHeight` (+0x60/+0x64)
are filled by virtual slot 4, not by construction.

This is not trivia: comparing bytes neither side wrote is the mistake that read
as a catastrophic port defect and cost a day in Wave 6
(`reference_oracle_must_not_compare_unwritten_bytes.md`). The ctor's oracle (§5)
masks exactly those five ranges, plus the handle pair, and nothing else.

### `_flags` (+0x34)

Every bit named by the exported one-line predicate that reads it or the setter
that writes it. Bits not listed are **not** guessed.

| bit | mask | meaning | proven by |
|---|---|---|---|
| 0 | 0x00000001 | **hidden** (not "visible") | `?IsVisible@` 0x1800354e0 is `NOT EAX; AND EAX,1` |
| 1 | 0x00000002 | no hit test | `?Contains@` 0x1801169e0 tests mask 0x3 |
| 2 | 0x00000004 | align override A | member `?GetAlignedPosition@` 0x180115ff0 |
| 3 | 0x00000008 | align override B | member `?GetAlignedPosition@` |
| 4/5 | 0x00000030 | size from screen height / width | `?GetPaddedSize@` 0x180115f10 |
| 18 | 0x00040000 | halve alpha | `?ComputeColor@` 0x180035490 |
| 16,19 | 0x00090000 | tooltip valid (set together) | `?SetToolTip@` 0x180035740 |
| 22 | 0x00400000 | was activated | `?WasActivated@` 0x1800354f0 |
| 23 | 0x00800000 | disabled | `?IsDisabled@` 0x180035520, `?Enable@` 0x180116910 |
| 24 | 0x01000000 | system-disabled | `?IsSystemDisabled@` 0x180035510 reads BYTE [+0x37] |
| 27 | 0x08000000 | needs size calc | `?NeedsSizeCalc@` 0x180035460 |

`?IsEnabled@` (0x180035500) is `TEST [RCX+0x34],0x1800000; SETZ` — enabled means
**neither** bit 23 nor bit 24.

---

## 5. Four oracles, argued per function

Almost nothing in this lane is pure, so "replay" was not assumed to apply. Each
function got its own argument.

### (a) Replay — twelve functions

`Child`, `ChildCount`, `GetUserData`, `FindElement`, `FindElementType`,
`SetUserData`, `SetElementId`, `SetPadding`, `SetToolTip`, `SetNeedsSizeCalc`,
`SetFlagsOnOff`, `Hide`.

The five read-only ones compare the returned value or the 8-byte
`InternalPtr` out-slot directly. The mutators snapshot the live 0xC0-byte
Element into a private scratch **before** our body runs, run ours on the real
object, run the DLL's on the copy, and `memcmp` all 0xC0 bytes.
`RtDiffReportBytes` names the first differing offset, and §4's table turns that
offset straight into a field name.

For the mutators this is a **full** oracle: it catches a wrong offset, a missed
flag bit, an early-out taken when retail would not have taken it, and a stray
write into padding.

**The one gap, stated plainly.** `SetNeedsSizeCalc`, `SetFlagsOnOff` and `Hide`
walk `_parent` to the root and OR flag bit 27 into every ancestor. The scratch
copy's `_parent` is the REAL parent, so the replayed original walks the real
chain and ORs bit 27 a second time. That is safe for a specific reason, not a
hopeful one: **our body has already set exactly those bits on exactly those
objects, so the replay's writes are a bitwise-OR of a bit that is already 1** —
idempotent, not a second, different mutation. If a future edit makes the walk do
anything other than OR a set bit, this argument dies with it, and the comment in
the source says so.

One shared file-scope scratch is used rather than an 0xC0 stack frame per
function. A depth counter makes that safe even if a replay ever nests. It does
not today: `FindElement` / `FindElementType` recurse through a private
non-diffing helper (which also stops the oracle from doubling the work at every
level of a deep dialog tree), and no other ported body calls another ported body.

### (b) Build — the ctor

A ctor cannot be replayed onto a copy of the object it just built: the second
run would register a **second handle** in `Object::InternalFactory::_singleton`
pointing at our scratch buffer, and that entry would dangle the moment the
buffer is reused. That is a real leak into shared state, not a theoretical one.

So instead the DLL **builds its own object** in the scratch, the two are
compared byte for byte, and the scratch is then **destroyed with the DLL's own
dtor** so its handle is unregistered again. On a freshly constructed Element that
dtor is a sequence of no-ops — no name, no tooltip string, no border, and
`Array_SetSize(0)` early-outs because `_size` is already 0 — so it cleans up the
handle and nothing else.

Excluded from the comparison, with the reason for each:

| range | why |
|---|---|
| +0x08 … +0x0f | handle index / id — necessarily different for two distinct live objects; that they differ is *correct* |
| +0x54, +0x60 … +0x67, +0x7c, +0x8c | never written by either ctor (§4) |

That leaves every one of the 27 field stores compared against the shipped ctor's,
including the three literals (`_alignment = 3`, `_flags = 0x08000000`,
`_color = 0xFFFFFFFF`) and the vtable pointer.

### (c) Thunk — `ComputeLayout` and `Draw`

Both are five-instruction forwarders that load `this->_border` and tail-jump
through THAT object's vtable. A forwarder cannot be replayed — running `Draw`
twice submits the geometry twice — but it has exactly two things that can be
wrong: **the field offset it loads and the slot it jumps to**, and getting
either wrong calls a different method with this method's arguments.

So at registration time the port decodes the shipped bodies out of the loaded
image and reports, through an ordinary diff site, whether the DLL's field
displacement and jump slot are the ones the port uses. Both shapes were verified
byte-for-byte against the on-disk image before the decoder was written:

```
ComputeLayout 0x1801168d0: 48 8b 89 b0 00 00 00  48 85 c9  74 07  4c 8b 11  49 ff 62 28  f3 c3
Draw          0x1801168f0: 48 8b 89 b0 00 00 00  48 85 c9  74 07  48 8b 01  48 ff 60 30  f3 c3
```

**A trap this lane hit and fixed, worth recording.** The first version located
those bytes with `ResolveAddr(Module::Runtime, 0x1801168d0)`. That is wrong:
this lane calls `RegisterRuntimeAddr` on both forwarder addresses, and
`ResolveAddr(Module::Runtime, …)` consults the port registry **first**
(`standalone/resolve_standalone.cpp:325`) — so it would have handed back OUR
function and the decoder would have reported "unrecognised thunk shape" against
our own prologue. It now uses `RtOriginal` (= `ResolveExportRaw` =
`GetProcAddress`), which is the only lookup guaranteed to reach the DLL. Any
future lane that reads shipped bytes at an address it also registers has the
same bug waiting.

### (d) Shadow — four functions

`SetName`, `SetToolTipString`, `AddChild`, `ContainerElement::SetOffsets`. Each
predicate names **every** term it checks, so a failure message says which one
broke — `reference_shadow_predicate_must_name_its_failing_term.md`, where 16
"divergences" turned out to be a predicate whose message was a term short.

**`SetName` / `SetToolTipString`.** No replay is possible: the scratch copy holds
the OLD string pointer, so the replayed original would decrement that string's
refcount a **second** time — and if our body already dropped it to zero and
freed it, the replay writes into freed memory. Refcounts are global state; PLAN
§4.3 risk 2 calls this the shadow case. The predicate checks that the field
holds the incoming body pointer, that the incoming string's refcount is ≥ 1, and
that the previous string was released exactly once (freed, or left with a
refcount ≥ 1).

**`AddChild`.** The array grow at 0x180076900 allocates; replaying it would push
the same child twice and reallocate the buffer. The predicate checks
`_size == old + 1`, `_data[old] == child`, `child->_parent == this`, and that bit
27 is set.

**`SetOffsets`.** Ours writes at most four locations, all inside objects somebody
else owns, so a replay onto a scratch `this` would still walk the real children
and write to them a second time. The predicate runs the original against the
real object **after** ours and asserts that every location ours wrote still holds
the value ours put there. That catches a swapped x/y, a wrong constant and a
wrong target object. It does **not** catch a write the original makes at an
offset ours never touched — see §7.

---

## 6. The defect this lane found, and two wrong names it fixed

**Defects found by Wave 7 lane A: two, plus two misleading declarations.** The
second, a missing-include bug in `runtime/types/ui/String.h`, belongs to lane F
and is written up under "Still for the wave lead" below. The other three are in
`engine/funcs/engine_funcs.h`, all three describe lane-A functions, and **all
three are FIXED in place** rather than routed — PLAN §6 step 8: fix it
where it is written, state what it IS, and keep one line recording what it was
believed to be so the next reader recognises the old claim if they meet it
elsewhere.

### (1) DEFECT — `fn_Element_SetPadding_t` had two parameters in the wrong order

```c
// before
typedef void (*fn_Element_SetPadding_t)(void* self, int left, int top, int right, int bottom);
//                                                            ^^^^^^^^^^^^^^^^^^
// after
typedef void (*fn_Element_SetPadding_t)(void* self, int left, int right, int top, int bottom);
```

Retail's `?SetPadding@` (0x180115480) stores `EDX->+0x18`, `R8D->+0x1c`,
`R9D->+0x20`, `[RSP+0x28]->+0x24`, and the four one-line exported accessors
settle which offset is which field: `?GetLeftPad@` (0x1800355e0) reads +0x18,
**`?GetRightPad@` (0x180035600) reads +0x1c**, **`?GetTopPad@` (0x1800355f0)
reads +0x20**, `?GetBottomPad@` (0x180035610) reads +0x24. The real order is
**(left, right, top, bottom)**. Corroborated independently by `?GetPaddedSize@`
(0x180115f10), which computes `width = _layoutWidth + [+0x1c] + [+0x18]` and
`height = _layoutHeight + [+0x24] + [+0x20]` — right and left, then bottom and
top.

**Latent when found, and that did not lower its priority.** A grep across
`code/`, `engine/`, `standalone/`, `runtime/` and `dx11/` finds no caller of
`fn_Element_SetPadding` outside `engine_funcs.cpp`, so nothing needed fixing
alongside it and no behaviour changed. But a wrong comment is worse than no
comment because it is believed: this one would have silently swapped top and
right padding for the first caller that appeared, and a UI padding bug is
exactly the kind that gets blamed on a description file for a week.

The typedef now carries the four accessor addresses, the `GetPaddedSize`
corroboration, and one line recording the old `(left, top, right, bottom)` claim.

### (2) `fn_Element_SetToolTip_t`'s argument was called `sprite`; it is a `UI::String`

Renamed to `uiString`, with the evidence. The mangled name says it:
`?SetToolTip@Element@UI@@QEAAXVString@2@@Z`, where `String@2` back-references
`UI`. So does every call site — `buildui.cpp:316`, `professionui.cpp:493`,
`toolbardialog.cpp:1465` and `townhallui.cpp:456` all build one with
`fn_UIString_Ctor` into a `uint8_t[16]` and pass that. Behaviourally harmless
(the parameter is `void*`), but the name sent the next reader looking for a
sprite sheet, and the new comment also records the thing that actually matters
about the type: `UI::String` is a POD and is NOT refcounted, unlike the
`System::String` that `fn_Element_SetToolTipString` takes.

### (3) `fn_Element_SetFlagsOnOff_t`'s parameters were named `hideFlags, showFlags`

Renamed to `setFlags, clearFlags`. The body is
`_flags = (_flags | setFlags) & ~clearFlags` — bits to SET and bits to CLEAR.
The old names read correctly only by accident: flag **bit 0 means HIDDEN**
(`?IsVisible@` 0x1800354e0 is `NOT EAX; AND EAX,1`), so the ubiquitous `(1,0)`
call sets the hidden bit and `(0,1)` clears it, and every other bit inverts that
intuition. Twenty-odd call sites under `code/Game/Dialog/` carry `// hide` /
`// show` comments that depend on the reader already knowing bit 0 is inverted;
the typedef now says so, and points at `UI::ElementFlag` in
`runtime/types/ui/Element.h` for the rest of the bit map. Retail also runs the
parent dirty walk only when bit 0 changed — no other bit triggers it — which is
now recorded there too.

**Verified after the edits:** `engine_funcs.cpp` and all nine
`code/Game/Dialog/` files that call either function still compile clean with the
wave's exact command line (the only warnings are pre-existing
`-Wunused-parameter` in unrelated `*_Slot9` bodies). No call site needed a
change: `SetPadding` has none, and the other two edits are parameter names only.

### Still for the wave lead

**(0) `runtime/types/ui/String.h` (lane F) does not compile if it is included
FIRST in a translation unit.** It uses `int32_t`, `size_t` and `offsetof`
without including `<cstdint>` or `<cstddef>`. Found by lane A while deleting its
duplicate `UiString`: a two-line probe that includes `String.h` and then
`Element.h` fails with eleven errors, the first being `'int32_t' does not name a
type`, after which `UIString` silently becomes an 8-byte struct with no `index`
and no `pad0C` and every `static_assert` in both headers fires.

It is latent today and PRE-EXISTS this lane: the only other includer,
`runtime/src/ui/sprite.cpp:112`, happens to pull the standard headers in first.
Lane A's `Element.h` is also unaffected in both directions, because it includes
`<cstdint>` and `<cstddef>` before including `String.h` (verified: `Element.h`
alone compiles clean, and so do all nine `runtime/src/ui/*.cpp`). But
`Element.h` has just made `String.h` a header two lanes depend on, so the next
file to include it first will hit this. Not fixed here because lane F owns the
file. The fix is two lines at the top:

```c
#include <cstdint>
#include <cstddef>
```

**(a) `docs/runtime_port/wave7_lanes.txt` — the `callers:` line for four lane-A
rows says only `standalone/address_registry.cpp`, which understates them.**
`?Draw@Element@UI@@…` and `??0Element@UI@@…` also have live consumers in
`code/Game/Component/frustumoutlineelement.cpp` and `captureboxelement.cpp` via
`fn_Element_Ctor` / `fn_Element_Draw` / `fn_Element_ComputeLayout`; the dispatch
slots at 0x1401518d8…0x1401518f8 are how they get there. Conversely
`??1Element@UI@@…` has **no** consumer at all — `fn_Element_Dtor` is assigned and
never called — and that is not visible from the line as written. Worth annotating,
because "registered in address_registry" and "actually invoked" are different
facts and this lane's hold-back decision turned on the difference.

**(b) `docs/runtime_port/WAVE7.md` scope line "A | `UI::Element` +
`ContainerElement` | 23".** Accurate as a call-site count; worth annotating that
one of the 23 (`??1Element`) has no live caller, so the achievable denominator
for this lane is 22, of which 21 landed.

**(c) A census artefact this lane creates, so nobody chases it.**
`element.cpp` resolves three names the census now counts as newly "called" and
unported UI functions: `??_7Element@UI@@6B@` (a **data** symbol, not a function
— pure artefact), `?GetAlignedPosition@Element@UI@@SAX…` (the static 17-argument
overload, which the member deliberately delegates to and which is **not** in this
lane's 23), and `?_typeInfo@ClipRegionElement@…` / `?_typeInfo@ScrollElement@…`
(data symbols). They are correct resolutions, not gaps.

---

## 7. What this lane could NOT verify

Stated plainly, because a prediction is not a measurement.

1. **No run happened.** Per the lane brief, this lane did not build, deploy or
   run anything. `RtUiElementRegister()` returning 21, the warning-clean compile
   at three optimisation levels, and the oracle designs are all source-level
   facts; **zero diffed calls have executed.** Until the control agent runs
   `BANISHED_RT_DIFF=1`, every row in §1 is "expected clean", not "clean". The
   kill switch is `BANISHED_RT_UIELEMENT=0`.

2. **The `GetPaddedSize` / `GetAlignedPosition` oracle compares OUTPUTS ONLY,
   and it is the weakest oracle in the lane.** Both functions call virtual slot 4
   (`ComputeSize`) before reading `_layoutWidth`/`_layoutHeight`. A derived
   `ComputeSize` walks the REAL children, so it cannot be replayed onto a scratch
   copy of `this`. The original is therefore run against the **real** object into
   a second set of output ints, and only the outputs are compared. That is sound
   only because `ComputeSize` is idempotent — its implementations early-out on
   flag bit 27, which our call has already cleared, and even an unconditional one
   is a pure function of an unchanged child tree. If some widget's `ComputeSize`
   turns out to have a side effect that is *not* idempotent, this oracle will
   report a false divergence (the safe direction) and the diffed run will have
   perturbed that widget's layout by one extra recompute.

3. **The `SetOffsets` shadow cannot see a write at an offset ours never
   touched.** It proves that the four locations ours wrote survive the original
   unchanged, which covers a swapped x/y, a wrong constant and a wrong target
   object. A wrong *offset* — ours writing +0xd0 where retail writes +0xd8 —
   would leave ours' value undisturbed at ours' address and pass. The offsets
   were read from the disassembly (`MOV [R9+0xd0]`, `[R9+0xd4]`,
   `MOVSS [RBX+0x160]`) and cross-checked against `ContainerDescription::
   CreateInstance`, which initialises the same ClipRegionElement fields, but this
   oracle is not what proves them.

4. **The thunk decoder has never decoded a live byte.** It was written against
   the two bodies read out of the on-disk image and handles both encodings they
   use (`4c 8b 11 / 49 ff 62 xx` and `48 8b 01 / 48 ff 60 xx`). An unrecognised
   shape is reported as a divergence rather than silently passing, which is the
   right failure direction, but it is an untested path.

5. **`?RemoveChildren@` and `??1Element@` stay the DLL's**, so nine
   `fn_Element_RemoveChildren` call sites under `code/Game/Dialog/` and the whole
   Element teardown path are unported by design. Any Wave 7 divergence that
   surfaces during dialog teardown is therefore NOT this lane.

6. **`?IsDerivedFrom@Base@Object@@`, `?StrCompare@String@System@@` and
   `?MemFree@MemoryAllocator@System@@` are resolved through `ResolveExport`**, so
   this lane's bodies use whichever implementation is registered. `IsDerivedFrom`
   and `StrCompare` are Wave 3 / Wave 2 territory; `MemFree` is the Wave 2
   allocator, which shipped **opt-in with a NO-GO verdict** (`WAVE2_ALLOCATOR.md`)
   and is therefore the DLL's unless somebody arms it. If a later wave gets one
   of those wrong, `SetOffsets` / `FindElement` / `SetName` are where it will
   surface — correctly, but it will look like a lane-A regression. This follows
   the precedent Wave 6 lane A set with `Math::Matrix::Inverse`.

7. **`Collection::Array<Element*>::SetSize` (0x180076900) is called, not
   ported.** It is a header-only template instantiation with no exported symbol,
   it allocates and frees through `MemoryAllocator` under the allocator's own
   critical section, and `EnterCriticalSection` cannot appear in a non-`_win`
   file (PLAN §4.1 decision 1). This lane calls the shipped instantiation by
   address — the exact one `AddChild` itself calls, since a different `Array<T>`
   gets a different copy of the template. It is safe to use
   `ResolveAddr(Module::Runtime, …)` there **only because nothing registers that
   address**; see the trap in §5(c).

8. **`_unknown78` (+0x78) is unnamed.** The ctor sets it to −1 and no exported
   accessor reads it; a scan of `Element::Update` (0x180116270) found no access
   either. It is carried faithfully and compared by every replay, so a divergence
   there would be caught — but the next reader will have to reverse whichever
   widget writes it.

9. **Nothing here was measured for performance.** Every oracle sits inside
   `if (rt_diff_on__)` and costs nothing in a normal run. The ported bodies add
   one indirect call versus retail's inline code for the two forwarders and
   nothing measurable elsewhere; all resolves are cached in function-local
   statics precisely because `RtOriginal` is `GetProcAddress`
   (`reference_rtoriginal_is_getprocaddress.md` — two of those in `System::Time`
   cost 5.6 points of sim time and were invisible to both profilers). The PLAN 5%
   gate is deferred (PLAN §5) and this lane did not touch it.

---

## 8. Notable faithfulness details

Things a "tidied" port would silently drop, each preserved and commented in the
source.

- **`ContainerElement::SetOffsets` derefs a null unguarded, and retail means
  it.** It does not null-check `children[0]` before calling `IsDerivedFrom` on
  it, and it does not null-check the *result* before storing through it: when
  child 0 is not a `ClipRegionElement`, `R9` is 0 and `MOV [R9+0xd0],EDX` writes
  to address 0xd0. That is exactly the fault `standalone/main.cpp:518`'s crash
  handler already recognises as "IsDerivedFrom null-this at Runtime+0xC0390" —
  0x1800c0390 **is** `?IsDerivedFrom@Base@Object@@QEBA_NI@Z`. CLAUDE.md's rule
  applies: the deref asserts the invariant that a ContainerElement's first child
  IS its clip region, and a guard would skip the behaviour rather than restore
  it.
- **`SetOffsets` re-loads `_children._data` before each of the three child
  reads** rather than caching it. Three separate loads at 0x1800cc1f8,
  0x1800cc232 and 0x1800cc25d. Reproduced.
- **`Child` does not bounds-check and SIGN-extends the index** (`MOVSXD RCX,R8D`).
  A negative index reads before the buffer. Reproduced — and this is precisely
  the instruction `reference_ghidra_c_output_vs_disassembly.md` warns is printed
  as a zero extension in Ghidra's C.
- **`SetPadding` does NOT run the dirty walk**, although the neighbouring
  `SetMinSize` (0x1801154a0) does. Not an omission on our part; the walk is
  simply not in the shipped body.
- **`SetToolTip`'s dirty walk is unreachable.** Both branches end with
  `if ((flags ^ old) & 1) walk`, but neither `|= 0x90000` nor `&= ~0x80000` can
  change bit 0. Kept, because it is in the shipped code and because the replay
  would report its disappearance as a divergence the day someone changes the
  masks.
- **`SetToolTip` evaluates `_toolTipDialog != 0` BEFORE the 16-byte copy**
  (`CMP [RCX+0x40],0` is the first instruction), so the branch below uses the
  dialog pointer as it was on entry.
- **`SetToolTip` writes +0x54.** Its second store is a full qword at +0x50, so it
  copies the `UI::String` tail padding that the ctor deliberately leaves alone.
- **`FindElement` never matches an EMPTY search string**, not even against an
  unnamed element: the `length(query) == 0` test jumps straight to the child
  scan.
- **`FindElementType` uses EXACT type equality, not `IsDerivedFrom`.** A subclass
  of the requested type is not a hit.
- **`SetName`'s release path uses the FIELD as its scratch**: `this->_name -= 8`
  and then `MemFree(this->_name)`, leaving a freed pointer in the field for two
  instructions before it is overwritten. Reproduced, because the object is
  observable from a signal handler and tidying it would hide that the field is
  briefly dangling.
- **`GetPaddedSize` re-reads `self->_flags` for its second test.** Retail keeps
  `LEA RDX,[RBX+0x34]` alive across the root walk, which clobbers `RBX`.
  Reproduced as a second load rather than reusing the cached value.
- **`GetPaddedSize` adds in retail's order and width**: `layout + right + left`
  and `layout + bottom + top`, in 32-bit, so an overflowing pad wraps the same
  way. Its minimum-size floor is applied to the **padded** value, not the layout
  value, and both screen-size conversions are `CVTTSS2SI` — truncation toward
  zero, not rounding.
- **`GetPaddedSize`'s screen override walks to the ROOT element** and reads the
  root's `_dialogParent`, not this element's.
- **`Hide`'s mask is built branchlessly**: `hide ? 0xFFFFFFFF : ~1`, so the AND
  is a no-op on the hide path. Reproduced as written rather than as
  `if (hide) flags |= 1; else flags &= ~1;`, which is equivalent here but not
  obviously so.
- **The dirty walk is a do/while, not a while.** The OR happens before the null
  test, so it always runs at least once on `self` itself.
- **`AddChild` grows the array to `_size + 1` and lets the shipped helper choose
  the capacity** (which triples on a +1 growth from a non-empty array). Not
  reimplemented.
- **The ctor leaves +0x54, +0x60, +0x64, +0x7c and +0x8c untouched**, exactly as
  retail does. Writing them would be a divergence.

---

## 9. Ghidra

Everything in this lane was read with `program="Runtime-x64.dll"` on every call.
`switch_program` was never invoked. No function was renamed and no type was
created, so no other lane's view of the shared project changed. Export
addresses, ordinals, vtable bounds and the two forwarders' opcode bytes were
read directly out of `C:\GOG Games\Banished\Runtime-x64.dll` with a
pefile-style reader rather than taken from Ghidra, so they are the shipped
values.
