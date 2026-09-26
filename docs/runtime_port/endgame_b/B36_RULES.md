# B.3 step 6 — `UI` becomes real C++ classes: the rule sheet

Written 2026-09-25 by block 0 (the question pass) on main `dc647869`. This is the sheet the
conversion agents are handed. It answers `NEXT_SESSION.md` §2b's five questions from the RETAIL
`Runtime-x64.dll` (15290 functions, image base 0x180000000) and from the tree, and it
supersedes `UI_INVENTORY.md` wherever the two disagree — the inventory is a map, this is the
evidence. §13 lists what is still NOT VERIFIED; nothing outside §13 was taken on trust.

Standing rules from `B34B_RULES.md` §2d, `B35_RULES.md` §8 (B5-1..17) and `NEXT_SESSION.md` §2d
apply unchanged and are not repeated except where UI instantiates them. `ENDGAME.md` §B.1/§B.2
decisions apply unchanged: retail layout kept, slots in retail order, **no virtual destructor**
(slot 0 is `DeletingDtor(unsigned)`), no real ctor on a polymorphic class, doors delegate with a
QUALIFIED non-virtual call.

## 0. How this sheet was measured

| fact | instrument |
|---|---|
| vtable addresses, slot count, each slot's target + retail name | `read_memory` over `.rdata` via `b36_session/ghidra/UiAppendix.java`, one qword at a time, terminating on the next `??_7`/`??_R` symbol, a zero qword or a non-code target. NOT decompiled C. |
| `sizeof` of every polymorphic class | the `MOV EDX,imm` in its own slot-0 vector deleting destructor, read as DISASSEMBLY |
| base class | `?_ParentTypeName@X@UI@@CAPEBDXZ`'s returned string literal, read byte-wise; ALL 68 classes carry one; cross-checked against shared slot targets (every `inherited-from` marker in `B36_SLOTS.md` §1 agrees with the literal) |
| secondary vptrs | every `??_7` LEA inside every `??0`/`??1` of the 68 classes, with the following store instruction (`B36_SLOTS.md` §7) |
| who stamps a vtable | Ghidra `getReferencesTo` on the vtable address, resolved to the containing function's mangled name (§5 of the slots file) |
| by-value returns | every UI export whose mangled return type is `?AV`/`?AU` (class by value → hidden retslot); `?AW4` (enum, returned in EAX) excluded |
| our tree | `Grep`/`Glob` scoped to the worktree, build dirs excluded; three read-only subagent reports in `b36_session/` (file map, POD tree audit, exe subclass enumeration) |

**The raw data is `B36_SLOTS.md`** (and `b36_session/ui_appendix_raw.txt`, the unabridged script
output). Copy from it rather than from the tables here.

---

## 1. Q1 — THE CLASS LIST, SETTLED

**68 polymorphic classes + 5 non-polymorphic = 73 classes carry `@UI@@` exports** (section 6 of
the slots file buckets 1,766 exports; the two "classes" it lists beyond 73 are two
`Achievement::*` getters whose RETURN type is a UI class, not UI classes).

- 68 with a `??_7X@UI@@6B@` vtable AND a `?_typeInfo@X@UI@@` record AND a `_ParentTypeName`:
  28 `*Description` (incl. the base `ElementDescription`), 28 `*Element` (incl. `Element`),
  6 dialog-family (`Dialog`, `DialogController`, `DialogControllerConfig`, `DialogInstance`,
  `ComboDialog`, `ToolTipDialog`), 6 resources (`CharacterSet`, `Cursor`, `Font`,
  `SpriteCapture`, `SpriteSheet`, `StringTable`).
- 5 non-polymorphic: `UserInterface` (52 exports), `SpriteContext` (23), `Sprite` (12),
  `String` (8 — spelled `UIString` in the tree, `UI::String` in retail), `Element::UpdateState`
  (2: a ctor and `operator=`). Nested value types with NO exports of their own (`DialogTimer`,
  `ClipRegion`, `ClipRect`, `SpriteDraw`, `FontGlyph`, `FontLayout`, `SpriteInfo`,
  `CaptureEntry`, `Event`, `DocumentItem`, …) are layout-only and belong to their holder's block.
- **`ENDGAME.md`'s "53"** is reconciled as 27 leaf Description/Element pairs (54) minus the one
  pair that shares a file with its parent in the original source tree — it is a stale
  approximation and is superseded by 68 + 5. Nothing in this step depends on it.

## 2. Q2 — PER CLASS, FROM RETAIL BYTES

### 2.1 The hierarchy, proved by 68 `_ParentTypeName` literals (every edge below is a read string)

```
Object::Base (3 slots)
├─ Object::Internal (4 slots: 0 DeletingDtor, 1 TypeName, 2 Type, 3 Persist)   [Internal.h:500, B.3.3]
│  ├─ UI::Element (12 slots, 0xc0)           ABSTRACT — slot 4 ComputeSize is _purecall
│  │  ├─ UI::BorderElement 0x150 · ButtonElement 0xd8 · ClipRegionElement 0xd8 · ContainerElement 0xd8
│  │  │  DocumentTextElement 0x1c8 · GraphElement 0x100 · GroupElement 0xc0 · ImageElement 0xe0
│  │  │  LayoutElement 0xd0 · ListElement 0x100 · NumberElement 0xe8 · ProgressElement 0x130
│  │  │  RibbonElement 0xd8 · ScrollElement 0x178 · SliderElement 0x130 · SpinnerElement 0xc8
│  │  │  TabElement 0x108 · TableElement 0xf0 · TextElement 0x100        (all 12 slots)
│  │  ├─ UI::EditElement 0xf8 (13 slots: adds 12 FinalizeText)
│  │  │  └─ UI::NumberEditElement 0x100 (13)        _ParentTypeName "EditElement"
│  │  ├─ UI::ButtonElement ─┬─ UI::CheckElement 0xe0     "ButtonElement"
│  │  │                     └─ UI::ComboElement 0x128    "ButtonElement"
│  │  ├─ UI::GroupElement ──┬─ UI::PageElement 0xc8      "GroupElement"
│  │  │                     └─ UI::RadioElement 0xd8     "GroupElement"
│  │  ├─ UI::ImageElement ─── UI::AnimatedImageElement 0x100  "ImageElement"
│  │  └─ UI::TextElement ──── UI::LabelElement 0x130     "TextElement"
│  ├─ UI::DialogInstance (10 slots, 0xb8)   "Object::Internal"
│  │  ├─ UI::ComboDialog 0xd0 (10)          "DialogInstance"
│  │  └─ UI::ToolTipDialog 0xc0 (10)        "UI::DialogInstance"  ← QUALIFIED literal, same namespace
│  └─ UI::DialogController (11 slots, 0x30) "Object::Internal"
└─ Object::External (8 slots)
   ├─ UI::ElementDescription (9 slots, 0x68) ABSTRACT — 6 Reconstruct, 7 Destruct, 8 CreateInstance are _purecall
   │  ├─ 22 leaves directly: AnimatedImage→ImageDescription, Border, Button, ClipRegion, Container,
   │  │  DocumentText, Edit, Graph, Group, Image, Layout, List, Number, NumberEdit, Progress,
   │  │  Ribbon, Scroll, Slider, Spinner, Tab, Table, Text  (all 10 slots)
   │  ├─ UI::ButtonDescription 0xa0 ─┬─ CheckDescription 0xa8   "ButtonDescription"
   │  │                              └─ ComboDescription 0xb8   "ButtonDescription"
   │  ├─ UI::GroupDescription 0x78 ──┬─ PageDescription 0x80    "GroupDescription"
   │  │                              └─ RadioDescription 0x80   "GroupDescription"
   │  ├─ UI::ImageDescription 0x80 ─── AnimatedImageDescription 0x98  "ImageDescription"
   │  └─ UI::TextDescription 0x90 ──── LabelDescription 0xb8    "TextDescription"
   ├─ UI::Dialog 0x88 (9) · CharacterSet 0x28 (9) · Cursor 0x30 (9) · Font 0x90 (9)
   │  SpriteCapture 0x58 (9) · SpriteSheet 0x58 (9) · StringTable 0x30 (9)     all "Object::External"
   └─ UI::DialogControllerConfig 0x20 (8)  "Object::External"  ABSTRACT: only slots 0,1,2 are its own;
                                            3,5,6,7 stay _purecall. A base for EXE config classes.

UI::{UserInterface 0x128, SpriteContext, Sprite 0x10, String 0x10, Element::UpdateState}
   — NON-POLYMORPHIC. No ??_7, no _typeInfo. Roots.
```

**Contradictions of `UI_INVENTORY.md`:** the leaf pairs are NOT "27 pairs each deriving from the
base pair" — **ten** leaves derive from an INTERMEDIATE leaf (Check/Combo from Button,
Page/Radio from Group, AnimatedImage from Image, Label from Text, NumberEdit from Edit), on BOTH
the Description and the Element side. A block that converts `CheckElement` needs `ButtonElement`
converted first. The parent literal of `ToolTipDialog` is `"UI::DialogInstance"` — namespace-
qualified although the parent is in the SAME namespace — so B5-15's "pattern, not a guarantee"
is now a measured exception: copy the literal, never derive it.

### 2.2 Slot lists — the four shapes

All 68 slot lists are in `B36_SLOTS.md` §1. The shapes:

| shape | classes | slots |
|---|---|---|
| Internal + Element | `Element` and its 27 subclasses | 0 DeletingDtor · 1 TypeName · 2 Type · 3 Persist(IO::Stream&) · **4 ComputeSize (PURE on Element)** · 5 ComputeLayout(int,int,int,int) · 6 Draw(SpriteContext&, uint8) · 7 Update(const Time&, UserInterface*, InternalPtr<DialogInstance>, const UpdateState&) · 8 Enable(bool) · 9 Contains(int,int,UserInterface*) · 10 SetColor(uint) · **11 GetBorder → InternalPtr<Element> BY VALUE (RETSLOT)** · [12 FinalizeText on EditElement/NumberEditElement only] |
| External + Description | `ElementDescription` and its 27 subclasses | 0..7 as `Object::External` (3 GetFileVersion, 4 Compile inherited from External on ALL 28, 5 Serialize, 6 Reconstruct, 7 Destruct) · **8 CreateInstance(const Dialog*) → InternalPtr<Element> BY VALUE (RETSLOT; PURE on the base)** · **9 Construct — leaves only** |
| External resources | `Dialog`, `CharacterSet`, `Cursor`, `Font`, `SpriteCapture`, `SpriteSheet`, `StringTable` | 0..7 as External (`Font` and `SpriteSheet` OVERRIDE slot 4 Compile; the other five inherit External's) · 8 Construct |
| Internal + dialog | `DialogInstance` (10) | 3 Persist · 4 OnOpened · 5 OnClosed · 6 OnUpdateLayout · 7 OnUpdate · 8 OnElementEvent · 9 OnSerializeSettings |
| Internal + controller | `DialogController` (11) | 3 Persist (inherited from Internal) · **4 OnCreated** · 5 OnOpened · 6 OnClosed · 7 OnUpdateLayout · 8 OnUpdate · 9 OnElementEvent · 10 OnSerializeSettings |
| `DialogControllerConfig` (8) | | 0,1,2 own; 3,5,6,7 `_purecall`; 4 External::Compile |

**RULE B6-1 (slot-order traps read from bytes).**
1. **`Construct` is at slot 9 on every leaf Description, behind `CreateInstance` (8)** — and
   `ElementDescription` itself has NO `Construct` (9 slots). On the seven External resources
   `Construct` is slot 8. Copying the resource shape onto a Description swaps `CreateInstance`
   and `Construct` (B5-16's `ParticleSystem` trap, 27 times over).
2. **`DialogInstance` and `DialogController` number the same-named virtuals DIFFERENTLY**:
   `OnOpened` is slot 4 on DialogInstance and slot 5 on DialogController (Controller has
   `OnCreated` at 4). `code/Game/Dialog/dialog_controller_vtable.cpp`'s `kBaseSlots` (4..10) is
   the Controller numbering; `mod_dialog_vtables.cpp`'s comments (+0x20 OnOpened …) are the
   Instance numbering. Never copy one class's slot comments onto the other.
3. `Element`'s slot 4 `ComputeSize` is PURE; every one of the 27 subclasses either overrides it
   or inherits an override (e.g. `PageElement` inherits `GroupElement::ComputeSize`). So
   `Element` is ABSTRACT in C++ exactly as in retail.
4. `ElementDescription` is ABSTRACT: slots 6, 7, 8 are `_purecall` — it is never constructed as a
   leaf; its slot 5 `Serialize` (0x1801170e0, `elementdescription_serialize.cpp`) is REAL and every
   leaf's Serialize calls it (qualified — B6-6).
5. `Font::Compile` (0x1801176c0) and `SpriteSheet::Compile` (0x18011d720) OVERRIDE External's
   slot 4; the other 26 Externals in UI inherit `?Compile@External@Object@@` (0x1800066e0).
   A Description block that declares `Compile` anywhere is wrong; a `Font`/`SpriteSheet` block
   that does not is wrong.
6. `EditElement` introduces slot 12 `FinalizeText`; `NumberEditElement` overrides it and
   INHERITS 4..7 from `EditElement`. `virtual`, not `override`, on `EditElement`'s slot 12.

### 2.3 `sizeof` — 68 read, 25 header agreements, ONE disagreement, 42 undocumented

Every size is in `B36_SLOTS.md` §1 (`sizeof=`). Checked against every
`static_assert(sizeof(X) == …)` under `runtime/types/ui/` and `engine/types/`:

- **25 agree** (all of `DescriptionsBatch1.h`'s ten, `Dialog` 0x88, `DialogController` 0x30,
  `DialogInstance` 0xb8 in BOTH trees, `ComboDialog` 0xd0, `Element` 0xc0, `ElementDescription`
  0x68, `Font` 0x90, `GroupDescription` 0x78, `ImageElement` 0xe0, `LabelElement` 0x130,
  `NumberElement` 0xe8, `ProgressElement` 0x130, `SliderElement` 0x130, `SpriteCapture` 0x58,
  `SpriteSheet` 0x58).
- **ONE disagrees: `EditElement` is 0xf8 in retail** (`MOV EDX,0xf8` in its deleting dtor
  0x18003fed0); `runtime/types/ui/NumberEditElement.h:32` asserts `sizeof(EditElement) == 0xE8`
  and pads `NumberEditElement` from there to `_min` at +0xF8. The pad arithmetic happens to land
  `_min` correctly (0xF8 = retail `sizeof(EditElement)`), so **`NumberEditElement` adds `_min`/`_max`
  AT +0xF8 and is 0x100 — but `EditElement`'s POD is 0x10 bytes SHORT: bytes +0xe8..+0xf7 are
  EditElement's, not a gap.** `EditElement.h:34` admits its size is "not asserted to be a
  specific retail size". The Edit block must pin those 16 bytes from `??0EditElement` and replace
  the 0xE8 assert with 0xF8.
- **42 classes have NO size assert anywhere** and 38 of them have no `struct` declaration at all
  (18 leaf Elements, 16 leaf Descriptions, `CharacterSet`, `Cursor`, `DialogControllerConfig`,
  `StringTable`, `ToolTipDialog`; `ComboElement`, `NumberEditElement`, `RadioElement`,
  `SpinnerElement` have a struct but no size assert). Their SIZE is now known (§1 of the slots
  file); their FIELDS are not. See §13 item 1 and RULE B6-8: the ported bodies in
  `runtime/src/ui/*.cpp` carry the offsets as file-local constants; the block MOVES those under
  the retail name and asserts the retail size — it does not re-derive them, and it does not
  invent names for bytes no body reads.

### 2.4 The NON-polymorphic classes

| class | sizeof | evidence | real ctor/dtor? |
|---|---|---|---|
| `UI::UserInterface` | 0x128 | `UserInterface.h:276` ✓; `gameupdateloop_load.cpp:1011` allocates exactly 0x128 | NO — constructed on the LOADER thread through `??0UserInterface@UI@@QEAA@XZ` (§5); the singleton-shaped root, like `Video::Device`. Holds `LinkedList<InternalPtr<DialogInstance>>` and `Array<…>` by value. |
| `UI::SpriteContext` | **≥ 0x88 INFERRED** (`SpriteContext.h:262`) | no deleting dtor, no `MemAlloc` of it found this pass; `??0SpriteContext@UI@@QEAA@AEAVContext@Video@@@Z` 0x18011d1e0 | NO until the size is pinned (§13 item 2). Its holder is the thing to find: `UserInterface::Draw` (0x180121890) hands `Video::Context&` to `SpriteContext::Begin` — read whether it constructs one on the STACK (then the frame size pins it). |
| `UI::Sprite` | 0x10 | `Sprite.h` ✓; three ctors + `operator=`/`==`/`!=` exported; returned BY VALUE by `GetSprite@ImageElement`, `GetSprite@LabelElement`, `GetBackgroundSprite@Dialog` (retslot) | **YES** — a value type held by value in `ImageElement`/`LabelElement`/`Dialog` and passed by `const Sprite&` to `SpriteContext::AddSprite` ×4. Real ctors/assign, trivially copyable, `friend` not needed (public). Retail spells it `UI::Sprite`; the tree's bare `struct Sprite` moves under `UI::`. |
| `UI::String` (tree: `UIString`) | 0x10 | `String.h` ✓; `Element::_toolTip` holds it by value at +0x48 | **YES** — same as `Sprite`: real ctors (`(ExternalPtr<StringTable>, const System::String&)`, `(ExternalPtr<StringTable>, int)`, default), `operator=`, `operator System::String()` (RETSLOT: `??BString@UI@@QEBA?AV0System@@XZ`). NOT refcounted (Element.h banner). Retail name is `UI::String`; the tree's `::UIString` becomes an alias of `UI::String` (B6-4). |
| `UI::Element::UpdateState` | tree: unknown | `??0UpdateState@Element@UI@@QEAA@XZ` 0x180115600, `??4` 0x180035880; passed `const UpdateState&` to `Element::Update` (slot 7) | value type; pin from the ctor's stores in the Element block |

### 2.5 Hidden return slots — 73 exports, TWO OF THEM THROUGH A VTABLE SLOT (the inventory said none)

`B36_SLOTS.md` §4: 73 `?AV`/`?AU` UI exports (three `?AW4` enums excluded). **The inventory's
"none through a vtable slot" is WRONG**:

| slot | class | export | returns |
|---|---|---|---|
| 11 | `UI::Element` (and every subclass inherits it — 28 (class, slot) pairs) | `?GetBorder@Element@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@XZ` 0x180035630 | `InternalPtr<Element>` |
| 8 | the 27 leaf `*Description` (PURE on the base) | `?CreateInstance@XDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z` | `InternalPtr<Element>` |

**RULE B6-2 (the two retslot slots).** Per B5-12's COORDINATOR AMENDMENT (GCC ms_abi passes the
hidden return pointer BEFORE `this`; MSVC after): slot 11 and slot 8 are declared with the
out-parameter in RETAIL order,
`Object::InternalPtr<Element>* GetBorder(Object::InternalPtr<Element>* ret) const` and
`Object::InternalPtr<Element>* CreateInstance(Object::InternalPtr<Element>* ret, const Dialog* dialog) const`,
on EVERY class that declares or overrides them. Today's doors already have that shape
(`RtUi_Element_GetBorder(const Element*, void** ret)` in `element_vtable.cpp:381`;
`CreateInstanceFn = void** (*)(const void*, void** ret, void* dialog)` in `comboelement.cpp:106`).
A unit test pins both, modelled on `tests/unit/graphics_geometry_abi_test.cpp` PART C.

The 55 non-slot retslot exports (`B36_SLOTS.md` §4) — `Element::Child` ×2, `FindElement` ×2,
`FindElementType` ×2, `GetToolTipDialog`, `GetDescription`, `GetToolTip` (UI::String),
`GetToolTipString` (System::String); `DialogInstance::GetRoot` ×2, `GetParent`, `GetController`,
`FindElement`, `FindElementType`; `Dialog::CreateElements`, `CreateElement`, `GetTemplate`,
`GetReference`, `GetBackgroundSprite`; `DialogController::GetContainer`; `UserInterface::
GetFirstDialog` (a `LinkedList::Iterator` struct), `GetFirstContainedDialog`, `SetClipRegion` ×2
(`ClipRegion` struct); `SpriteContext::{Get,GetScreen,Set×2}Scissor` (`Video::Viewport`);
`StringTable::GetText` ×2, `SpriteSheet::GetSpriteNameFromIndex`, `DocumentTextElement::GetLink`,
`ComboElement::GetItemText`, `TextElement::GetText` (System::String); the `ExternalPtr<T>`
getters (`Sprite/Font/Cursor::GetSpriteSheet`, `SpriteSheet::GetMaterialInstance`,
`String::GetStringTable`); `ImageElement::GetSprite`, `LabelElement::GetSprite` (UI::Sprite) —
are NON-virtual: the door keeps the explicit `(this, ret, …)` signature and placement-news the
member's prvalue (B5-12's general sentence). `fn_Element_Child` (90 call sites) and
`fn_DialogInstance_FindElement` (79) and `fn_FindElement` (73) are the caller batches' heaviest
retslot doors: convert only in the "door body moved to the call site" shape.

### 2.6 Multiple inheritance — NONE

`B36_SLOTS.md` §7: 364 `??_7` LEAs inside the 68 classes' ctors/dtors, **364 followed by
`MOV qword ptr [reg],RAX` at offset 0 of `this`**. No stamp at a non-zero offset, no second vptr,
no this-adjusting thunk in any UI table. Every UI class is single-inheritance. (The exe's
`ControllerComponent` pattern starts in `code/Game`, not here — the exe-side dialog controllers
are `DialogController` subclasses and the exe's UI classes are enumerated in §3.)

---

## 3. Q3 — THE EXE-SIDE SUBCLASSES (the constraint on abstract bases)

Report: `b36_session/EXE_SUBCLASSES.md` (read-only enumeration, file:line per row). Summary and
the rules it yields are in §3.1–§3.3 below; the report is authoritative for the per-class rows.

### 3.1 What the exe builds on top of UI bases (measured, `EXE_SUBCLASSES.md`)

| Runtime base | exe subclasses | hand-built tables | how the base is constructed |
|---|---|---|---|
| `UI::DialogInstance` | **20** (the "22" banner in `engine/types/ui/DialogInstance.h:5` counts two `Object::External` tutorial dialogs) | 21 + 1 table for the BASE itself (`tool_base_toolui.cpp:118`, slots 1..9 copied from the minted 0x180156d70) | `fn_DialogInstance_Ctor` door, then the vptr is overwritten; slot-0 bodies call `fn_DialogInstance_Dtor` (20 sites) |
| `UI::DialogController` | 39 in `kMasks` | 38 (`SelectResourceUI` is an abstract exe intermediate with no table) | NO exe subclass runs the base ctor or dtor today (0 callers of either door); `Apply()` fills inherited slots 3..10 by DLL address |
| `UI::DialogControllerConfig` | 4 (`ControlUI/ResourceLimitUI/StatusIconUI/UpgradeUIConfig`) | 4 (9-slot) | `fn_External_External` — pins the class as External + no fields of its own below +0x20 |
| `UI::Element` | 2 (`FrustumOutlineElement`, `CaptureBoxElement`) | 1 (CaptureBox keeps the abstract base table: a latent `_purecall`) | `fn_Element_Ctor` |
| `UI::ElementDescription` | 2 (`CaptureBoxDescription`, `FrustumOutlineDescription`) | 2 (11-slot) | `fn_ElementDescription_Ctor` |
| `UI::SpriteSheet` | 0 | 1 base-table MIRROR (`thumb_vtables.cpp:69-77`, slots 1..8 read off the first constructed object) | `??0SpriteSheet` by name |

Bare `UI::DialogInstance` objects ARE constructed (`tool_base_toolui.cpp:220-231`: `MemAlloc(0xB8)`
+ the ctor door + a no-behaviour table; retail's `ToolTipDialog` adds nothing to it), and bare
`SpriteSheet`s too — through the DOORS, never as C++ objects. So the C++ verdict is unchanged:
every one of these bases keeps slot 0 `= 0` with its `deleting_dtor.cpp` row, and no real ctor.

### 3.2 Where the QUALIFIED-CALL rule bites

A slot body of a hand-built exe subclass that today calls a base member through a door
(`fn_DialogInstance_FindElement(self, …)`, `fn_Element_SetFlagsOnOff`, a `ResolveAddr(Module::Exe,
0x14015…)` slot, a hand-inlined `(*(void***)p)[N]`) converts to
`reinterpret_cast<UI::DialogInstance*>(self)->UI::DialogInstance::Member(…)` — QUALIFIED when the
member is VIRTUAL on the base (`OnOpened`, `OnClosed`, `OnUpdateLayout`, `OnUpdate`,
`OnElementEvent`, `OnSerializeSettings`, `Persist`; `Element`'s 4..11; `DialogController`'s 4..10).
An unqualified call dispatches through the hand-built table back into the same slot body: the
report's §3 lists every such site per file. Non-virtual members (`FindElement`, `SetFlagsOnOff`,
`GetRoot`, …) need no qualification.

### 3.3 Exe code that COPIES Runtime vtable slots (four sites — all read the MINTED tables)

`tool_base_toolui.cpp:142-146` (by ADDRESS 0x180156d70, slots 1..9), `frustumoutlineelement.cpp:
248-258` (24 slots copied through a freshly constructed object), `thumb_vtables.cpp:56-77` (slots
1..8 off the first `SpriteSheet` object, and it calls slot 0 as a scalar deleting dtor),
`dialog_controller_vtable.cpp:44-51` (seven `DialogController` bodies by DLL address). Every one
reads a table the MINT owns and that step 6 does NOT retire (§7), and every object they read from
was constructed through a ctor door and so wears the minted table. **They keep working unchanged
in step 6.** They are the reason the mint rows retire at the END of B.3 and not per class, and
the reason member identity must not move: the closing pass that retires the rows must convert
these four sites first (step 8 work; listed in `EXE_SUBCLASSES.md` §4).

Thirty sites need the QUALIFIED call after conversion (report §3): 20 `fn_DialogInstance_Dtor`
slot-0 bodies, `fn_DialogInstance_OnUpdate` ×2 (`mainmenudialog.cpp:936`, `newgamedialog.cpp:
1062`), `OnUpdateLayout` (`mainmenudialog.cpp:915`), `OnElementEvent` (`moddialog.cpp:818`),
`Element::ComputeLayout`/`Draw` (`frustumoutlineelement.cpp:80,191`),
`ElementDescription::Serialize`/`GetFileVersion` (`captureboxdescription.cpp:46,77`,
`frustumoutlinedescription.cpp:49,82`). Hand-inlined `(*(void***)self)[N]` onto a UI base
virtual inside the 20 dialog files: none (the regex hits are exe `.rdata` dispatch slots).

Step-8 notes the report raises, NOT step 6's: no `DialogController` subclass runs `~DialogController`
today (making the 38 real subclasses would start running it); `Dialogs.h:23` and `Panels.h:22`
both claim 0x14015b070 (AchievementDialog "secondary vtable" vs AttendanceUI's table — a
multiple-inheritance hint on the EXE side to settle in Ghidra before AchievementDialog/ModDialog
become single-base subclasses); `::TutorialSetupDialog` is defined twice with different members
(`engine/types/ui/Dialogs.h:468` vs `engine/types/TutorialSetups.h:90`, both compiled — a live ODR
violation in the exe tree).

---

## 4. Q4 — THE SECOND POD TREE AND THE NAMESPACE SPELLINGS

Report: `b36_session/POD_TREE_AUDIT.md`. Measured:
- The two engine base PODs (`engine/types/ui/DialogController.h` 0x30, `DialogInstance.h` 0xB8)
  agree with the runtime headers on EVERY offset and size; every field NAME differs, and the
  engine `DialogInstance` POD makes six initialisation claims the runtime ctor refutes (+0x10/
  +0x18/+0x28/+0xA8 are ctor params, +0x30 is `_dataSource`, +0xB0 is `_userFlags`, not padding).
  The engine tree also supplies the size proof `runtime/types/ui/DialogInstance.h` says it lacks:
  all 38 `Panels.h` subclasses start their own fields at +0x30 and all 20 `Dialogs.h`
  DialogInstance subclasses at +0xB8 — `sizeof` is EXACT (and the deleting dtors say the same).
- NO TU includes both trees' definition of one class (runtime UI headers leave `runtime/` only via
  `standalone/mouse_inject.cpp:128-130`). Four engine headers have ZERO includers:
  `engine/types/ui/Elements.h`, `engine/types/Descriptions.h`, `engine/types/DialogLoopT.h`,
  `engine/types/Dialogs.h` (the non-ui one, which duplicates 12 global names with different
  members). `code/Game/Dialog/ui_dialogs.h` (24 TUs) and `ui_panels.h` (42 TUs) are the only live
  doors to the engine-side dialog/panel PODs.
- Inside `runtime/types/ui` three definitions of `Sprite` exist (`Sprite.h` bare, `ImageElement.h`
  `UI::Sprite`, `runtime/types/save/SaveLoad.h` `RtSave::Sprite` — same offsets, different names)
  and TWO of `FontGlyph` (`Font.h` vs `SpriteContext.h`, DIFFERENT field meanings). Both collide the
  moment `RtUI` folds into `UI`.
- FIVE bare-struct headers, not four: `ImageElement.h`'s own `struct ImageElement` is bare too.
  Blast radius of moving the bare names is `runtime/src/ui` ONLY (`SpriteSheet`/`SpriteInfo` ≤16
  TUs + 8 headers + the forward declaration in `runtime/types/common.h:24`; `Sprite` 9 TUs + 5
  headers; `UIString` 3 TUs; `LabelElement` 2-3; `UiStringTable` 3); `code/Game` and `standalone`
  have ZERO code uses of any bare name.
- `LabelElement.h` pins TWENTY `static_assert`s on WRONG field names (its own header says so at
  l.16-27; `TextElement.h`'s `TextElementOffsets` has the right ones at the same offsets).
- `DialogLoaderLoopT.h`/`DialogLoopT.h` are exe-side PODs of `Engine::DialogLoaderLoopT`/
  `DialogLoopT`, field-identical to `runtime/types/engine/UpdateLoop.h:202,225`, which hold
  `UI::UserInterface` BY VALUE at +0x40 / +0x60. `UI::SpriteContext` is a STACK object in
  `runtime/src/save/saveload_draw.cpp:734` (constructed inline, field by field). `UI::Sprite` is
  held by value in `runtime/types/save/SaveLoad.h:210,322`.

**RULE B6-4 (one definition per layout).**
1. `runtime/types/ui/<Class>.h` owns the ONE definition of every Runtime UI class. Where a POD
   occupies the retail name (`UI::Element`, `UI::ImageElement`, … — 15 headers spell `namespace UI`),
   B4b-1 applies: the POD moves to `RtUI::` in the same commit the class takes the name, OR — where
   class and POD are field-identical — the POD becomes an ALIAS of the class (step 5's `Context`/
   `Device` precedent) so other blocks' files compile untouched.
2. The five bare-struct headers and `StringTable.h` take their retail namespace: `UI::Sprite`,
   `UI::SpriteSheet`, `UI::String`, `UI::LabelElement`, `UI::ImageElement`, `UI::StringTable`, with
   `using UIString = UI::String;`, `using Sprite = UI::Sprite;`, `using SpriteSheet = UI::SpriteSheet;`
   at global scope for the transition. `ImageElement.h`'s second `UI::Sprite` is DELETED (include
   `Sprite.h`); `RtSave::Sprite` becomes an alias of `UI::Sprite`; `SpriteContext.h`'s `FontGlyph`
   is RENAMED (it is not `Font.h`'s `FontGlyph`) in the block that owns both files (D).
3. `engine/types/ui/DialogInstance.h` and `DialogController.h` become one-line aliases of the
   runtime classes (`using DialogInstance = UI::DialogInstance;` under their present namespace)
   in the Dialog block's commit, and the four dead headers are deleted in the same commit.
   `Dialogs.h`, `Panels.h` (exe-phase subclasses, step 8) stay; where a struct spells its Runtime
   base as an embedded POD/blob it stays THIS step (the audit proves the sizes agree, so the
   layout is not at risk).
4. `RtUI::*Description` PODs in `DescriptionsBatch1.h` (ten classes across four leaf clusters):
   the leaf clusters DO NOT EDIT this header (a shared file edited by five parallel blocks is the
   "two blocks appended at the same spot" conflict). Each cluster authors
   `runtime/types/ui/<X>Description.h` with the real class; the closing pass turns the ten PODs
   into aliases.
5. `UserInterface` and `SpriteContext` are held BY VALUE (above) → B5-4 shape: concrete, a
   protected `= default` constructor, `friend` the holders (`Engine::DialogLoopT`,
   `Engine::DialogLoaderLoopT` — spelled as the `RtEngine::` PODs they are today — for
   `UserInterface`; the `saveload_draw.cpp` TU for `SpriteContext`, via a public trivial ctor,
   since the object is built field by field there), and `static_assert(std::is_trivially_copyable)`.
   `Sprite` and `String` are public value types (B6-10).

## 5. Q5 — THREADING

- **The loader thread constructs UI.** `code/Game/Game/gameupdateloop_load.cpp:1009-1013`
  (`GameUpdateLoop_Load`, the `Engine::Loader` thread per B35_RULES §5.2) allocates 0x128 and
  calls `??0UserInterface@UI@@QEAA@XZ`; the same thread's resource loads construct every UI
  `External` (`Dialog`, `SpriteSheet`, `Font`, `StringTable`, `Cursor`, `CharacterSet`,
  `SpriteCapture`, all 28 Descriptions) through the factory's `Construct`/`CreateNewObject`
  slots while the MAIN thread draws the menu through the previous `UserInterface`.
- `runtime/src/ui` takes NO lock of its own (grep `CriticalSection|std::mutex|std::atomic`: 0).
  `InternalFactory::AddObject` (every `Element`/`DialogInstance`/`DialogController` ctor) and the
  `MemoryAllocator` critical section are the only synchronisation, both outside UI.

**RULE B6-5.** No static initialiser, lazy singleton or non-atomic first-call cache in any UI
class (B5-7 verbatim). A `static X* s_fn` resolved on first call inside a member body is what
the class blocks are REMOVING, not adding; where a member must resolve a door of another
namespace, resolve it at global scope in the class's `.cpp` at registration time.

---

## 6. Q6 — THE CALLER SURFACE (from `UI_INVENTORY.md` §3, units verified there)

| binding | call sites | where |
|---|---|---|
| (a) `engine_funcs` globals bound to a `@UI@@` export — 118 globals, names NOT `fn_UI_*` (only `UserInterface`'s are) | **1,434** | all `code/Game`; `Element`'s four doors = 398 |
| (b) `ResolveExport("…@UI@@…")` | 155 | engine 114 (the globals' initialisers), standalone 25, code/Game 16 |
| (c) `RT_EXPORT_ONCE("…@UI@@…")` | 184 | all inside `runtime/src/ui` (class blocks) |
| (d) `ResolveAddr(Module::Runtime, 0x18…)` onto a UI address | 5 of 51 | — |
| (e) exe dispatch slots: 205 exe import slots resolve to a UI export; `ResolveAddr(Module::Exe, 0x14015…)` hits | 49 | `engine_funcs.cpp` 45 (the same globals), `reg_element_typeids.cpp` 17, `address_registry.cpp` 17, `reg_unreg_lane2.cpp` 14, then `code/Game/Dialog` |
| (f) hand-inlined `(*(void***)p)[N]` in `code/Game/Dialog` | **417** in 39 files | `tradeui.cpp` 150, `townhallui.cpp` 95 |

**RULE B6-6.** UI callers are dominated by spelling (a) — the opposite of Video/Graphics. The
caller batches therefore START from `engine/funcs/engine_funcs.cpp`'s 118 initialisers (find
each global BY ITS INITIALISER STRING, map it to exactly one member, THE DOOR DECIDES THE
OVERLOAD — `Child` has a const and a non-const export, `GetRoot` too, `SetScissor` ×2,
`SetClipRegion` ×2, `GetText` ×2), then sweep spelling (e) through `tools/dispatch_slot_map.py`,
then (f) by proved receiver. The `engine_funcs` globals are deleted by the CLOSING owner, not by
a batch.

---

## 7. Deliberately NOT converted in step 6

| left alone | why |
|---|---|
| the 68 `??_7X@UI@@6B@` mint rows and the 67+1 `RT_DELDTOR` rows | retire at the END of B.3 (B35_RULES §11) |
| ctor/dtor DOORS (`??0`/`??1`/`??_G`/`??2`/`??3` names; `fn_DialogInstance_Ctor` 21 sites, `fn_DialogInstance_Dtor` 21, `fn_UIString_Ctor` 63, `fn_Sprite_Ctor` 26 …) for POLYMORPHIC classes | B4b-2; the exe subclasses construct their bases through them (B6-3). **EXCEPTION: `UI::Sprite` and `UI::String` are value types with REAL ctors (§2.4) — their ctor doors DO convert to C++ construction, in the caller batches, after the value-type block lands.** |
| the two `?_typeInfo@` rows the manifest tags (`??_7Dialog@UI@@6B@` WITHHELD, `??_7DialogController@UI@@6B@` REQUIRED) and every other `_typeInfo` row | end of B.3; no `DEFINE_TYPE` is introduced in step 6 — the literals are RECORDED per class from `B36_SLOTS.md` §3 (B5-15) |
| `System::MemoryAllocator` doors (`operator new/delete`, `CreateNewObject`, `CreateInPlaceObject` on all 68) | deferred past step 4 on purpose |
| the exe-phase UI subclasses (`Dialogs.h`, `Panels.h`, `Elements.h`, `Descriptions.h`, the 39 controllers, the ~21 dialogs) | step 8; step 6 gives them REAL BASES to derive from later |
| `dialog_controller_vtable.cpp::Apply` and `ToolUI_InitChildDialogVtable`'s slot copies | they read the minted tables, which survive the step (§3.3) |

---

## 8. The rules, collected

| # | rule | § |
|---|---|---|
| B6-1 | Slot-order traps: `Construct` is slot 9 on leaf Descriptions (8 on the seven resources, absent on the base); DialogInstance and DialogController number `OnOpened…` one apart; `Element` slot 4 and `ElementDescription` 6/7/8 are PURE; only `Font`/`SpriteSheet` override `Compile`; `EditElement` introduces slot 12 | 2.2 |
| B6-2 | Slots 11 (`GetBorder`) and 8 (`CreateInstance`) return `InternalPtr<Element>` BY VALUE: declare `T* M(T* ret, …)` in retail order on every class that has them; unit-test both | 2.5 |
| B6-3 | Every base an exe class hand-builds on is abstract and keeps its ctor door; qualified calls inside exe slot bodies | 3 |
| B6-4 | One definition per layout: runtime header owns it; PODs become aliases or move to `RtUI::`; the five bare structs take `UI::`; `engine/types/ui/{DialogInstance,DialogController}.h` die | 4 |
| B6-5 | No static init / lazy cache in UI classes (loader thread constructs them) | 5 |
| B6-6 | Callers by BINDING starting from `engine_funcs.cpp`'s 118 initialisers; the door decides the overload | 6 |
| B6-7 | Type-name literals copied from `B36_SLOTS.md` §3, never derived (`ToolTipDialog` → `"UI::DialogInstance"`) | 2.1 |
| B6-8 | 42 undocumented layouts: MOVE the ported bodies' file-local offsets under the retail name, assert the retail `sizeof` and `alignof`, name only bytes a body reads; `EditElement` is 0xf8 not 0xe8 | 2.3 |
| B6-9 | Intermediate leaves convert BEFORE their children: Button before Check/Combo, Group before Page/Radio, Image before AnimatedImage, Text before Label, Edit before NumberEdit — on both sides | 2.1 |
| B6-10 | `UI::Sprite` and `UI::String` are REAL value types (ctors, assign, trivially copyable, `Element::_toolTip`/`ImageElement`/`LabelElement`/`Dialog` hold them by value); every other UI class keeps no real ctor | 2.4 |

## 9. The blocks (file sets from `B36_FILEMAP.md`; 93 of 100 files are disjoint, 7 cross-cutting)

Waves, because the leaves derive from the bases and `UserInterface` calls everything:

| wave | block | classes | owned files | notes |
|---|---|---|---|---|
| 1 | **A** | `Element` + `Element::UpdateState`, `ElementDescription` | `runtime/types/ui/{Element,ElementDescription}.h`; `runtime/src/ui/{element.cpp, element_vtable.cpp, elementdescription_serialize.cpp, element_common.h, description_common.h, ui_operator_delete.cpp, element_sprite_debug.cpp/.h}` | the two abstract bases; `dialog.cpp`'s six ElementDescription bodies and the one-line strays in `tooltipdialog.cpp`/`widgets_layout.cpp`/`element.cpp` are the FRAGMENT pass's |
| 1 | **B** | `DialogInstance`, `DialogController`, `DialogControllerConfig`, `Dialog`, `ComboDialog`, `ToolTipDialog` | `runtime/types/ui/{Dialog,DialogInstance}.h` (+ new `DialogController.h`, `DialogControllerConfig.h`, `ToolTipDialog.h` if it splits them); `runtime/src/ui/{dialog.cpp, dialog_controller_base.cpp, dialog_create.cpp, dialog_ctor_dtor.cpp, dialog_factory.cpp, dialog_settings.cpp, dialog_vtable.cpp, dialogcontrollerconfig.cpp, combodialog.cpp, combodialog_ctor.cpp, tooltipdialog.cpp}`; `engine/types/ui/{DialogInstance,DialogController}.h` (→ aliases); deletes the four dead engine headers | B6-1.2 (Instance/Controller slot numbering), B6-3; `spritesheet_new.cpp`'s two dialog `operator new/delete` rows stay doors |
| 1 | **D** | `Sprite`, `String`, `SpriteSheet`, `SpriteCapture`, `SpriteContext`, `Font`, `CharacterSet`, `StringTable`, `Cursor` | `runtime/types/ui/{Sprite,String,SpriteSheet,SpriteCapture,SpriteContext,StringTable,Font}.h` (+ new `CharacterSet.h`, `Cursor.h`); `runtime/src/ui/{sprite.cpp, sprite_debug.cpp/.h, spritesheet_new.cpp, spritesheet_vtable.cpp, spritecapture.cpp, spritecapture_class.cpp, spritecontext.cpp, spritecontext_add.cpp, spritecontext_flush.cpp, stringtable_factory.cpp, stringtable_lifecycle.cpp, font.cpp, characterset.cpp, cursor.cpp}`; `runtime/types/common.h:24`'s forward declaration; `runtime/types/save/SaveLoad.h`'s `RtSave::Sprite` (alias) | B6-4.2 (the namespace fold + the `Sprite`×3 / `FontGlyph`×2 collisions), B6-10, `SpriteContext` size (§13.2) |
| 2 | **C1** | `Button`, `Check`, `Combo`, `Border`, `ClipRegion` (both sides) | the ten pair files | B6-9: Button before Check/Combo |
| 2 | **C2** | `Group`, `Page`, `Radio`, `Image`, `AnimatedImage` | the ten pair files; `ImageElement.h` (bare → `UI::`, its `Sprite` copy deleted) | Group before Page/Radio, Image before AnimatedImage |
| 2 | **C3** | `Text`, `Label`, `Edit`, `NumberEdit`, `Number` | the ten pair files + `textelement_cachetext.cpp`; `LabelElement.h` (bare → `UI::`, the twenty wrong names), `EditElement.h` (0xf8), `NumberEditElement.h`, `NumberElement.h`, `TextElement.h` | Text before Label, Edit before NumberEdit |
| 2 | **C4** | `Container`, `Layout`, `List`, `Scroll`, `Table` | the ten pair files | — |
| 2 | **C5** | `DocumentText`, `Graph`, `Progress`, `Ribbon`, `Slider`, `Spinner`, `Tab` | the fourteen pair files + `documenttext.cpp` + `tabelement_addtab.cpp` | — |
| 2 | **E** | `UserInterface` | `runtime/types/ui/UserInterface.h`; `runtime/src/ui/{userinterface.cpp, userinterface_close.cpp, userinterface_dragstart.cpp, userinterface_draw.cpp, userinterface_frame.cpp, userinterface_hover.cpp, userinterface_showdialog.cpp, userinterface_standalone.cpp}` | B6-4.5 (by-value holder), B6-5 |
| 3 | **fragments** | bodies of converted classes living in another block's file | `dialog.cpp` (ElementDescription ×6), `element.cpp` (ContainerElement), `tooltipdialog.cpp` (Element), `userinterface_hover.cpp` (DialogInstance), `widgets_layout.cpp` (Element + 8 leaves), `widgets_input.cpp` (8 leaves), `element_bodies_b5.cpp` (9 leaves), `ui_factory_wave28.cpp` (34 classes), `spritesheet_new.cpp`, `DescriptionsBatch1.h` (ten PODs → aliases) | one owner per side |
| 4 | **callers** | round 1 by directory (`code/Game/Dialog` ×3, `tradeui.cpp`+`townhallui.cpp`, rest of `code/`, `engine`+`standalone`, `runtime/src` outside `ui`), round 2 | | B6-6 |
| 5 | **close** | `engine_funcs` zero-caller sweep by BINDING, dead caches, numbers, the step-close gate with the five dialog routes | | |

Not a block's: `standalone/{reg_element_typeids,reg_unreg_lane2,address_registry}.cpp`,
`runtime/src/ui/{factory_ui1.cpp(+.inc), rt_ui_port.cpp, description_dtors.cpp}`,
`runtime/src/object/deleting_dtor*.cpp`, `standalone/reg_mint_promoted.cpp`,
`runtime/src/save/{saveload,saveload_draw}.cpp` (cross-namespace CALLERS: round 1).

## 10. Coordinator's dated corrections from the blocks

> **RULE B6-11 (block A, 2026-09-25) — an `InternalPtr<T>` ARGUMENT is passed by HIDDEN POINTER.**
> A retail signature that takes `Object::InternalPtr<T>` BY VALUE receives the ADDRESS of the
> caller's temporary in the argument register: `Element::Update`'s caller at 0x1801162c6 is
> `MOV RAX,[R12] / LEA R9,[RBP+0x50] / MOV [RBP+0x50],RAX / CALL [RAX+0x38]`. GCC would pass the
> trivially-copyable 8-byte class in the register itself. Every such parameter — virtual or not —
> is declared `Object::InternalPtr<T>* p` in retail order (`Element::Update`, `DialogInstance::
> OnElementEvent` slot 8 / `DialogController` slot 9 / `ComboDialog` slot 8, `UserInterface::
> AddDialog`, `CloseDialog`, `BringDialogToFront`, `IsDialogOpen`, `ShowDialog`, `BeginToolTip`,
> `EndToolTip`, `BeginDragDrop`, `PostEvent`, `Dialog::CreateElement`'s `ExternalPtr`…) and pinned
> by a raw retail-order call in the unit test. This is the argument-side twin of B5-12's amendment
> (return-side: GCC puts the retslot BEFORE `this`). `ExternalPtr<T>` arguments were measured as
> bare register pointers in step 5 (`EdgeMesh::SetMaterials`); re-read the prologue per member
> rather than assuming either way.

> **CORRECTION to B6-4.1 (block A) and B6-4.3 (block B), 2026-09-25 — an ABSTRACT class cannot be
> an alias target for an EMBEDDED POD.** Seven leaf element headers embed `Element base;` by value
> and 60 `engine/types/ui/{Dialogs,Panels}.h` structs embed `DialogInstance base;` /
> `DialogController base;`; a member of abstract type is ill-formed. So the old `UI::Element`
> struct moved to `RtUI::Element` (cross-asserted field for field against the class), and the
> engine headers became one-line aliases of the `RtUI::DialogInstance` / `RtUI::DialogController`
> PODs, not of the classes. The alias-onto-the-class shape stays right for PODs nobody embeds.

> **CLOSED (block A): §13 item 3 — `Element::UpdateState` is 0x30.** `??4UpdateState` (0x180035880)
> copies six qwords; the ctor (0x180115600) leaves +0x0a/+0x0b, +0x14 and +0x21..+0x2f unwritten.
> **CLOSED (block D): §13 item 2 — `sizeof(SpriteContext)` is 0x88** (block D commit `c11f740b`).

> **FOUND (block A):** `ElementDescription` +0x40 is the ALIGNMENT, not an element type: the ctor
> seeds it with 3 and `CreateInstance` (0x180115ad0) copies it into `Element::_alignment` (+0x30).
> The header records this; the field is not renamed this step. **Name-hiding:** the non-virtual
> `ElementDescription::CreateInstance(InternalPtr<Element>)` is hidden by every leaf's slot-8
> override — qualify it from a leaf body.

> **FOUND (block B):** `ToolTipDialog`'s 8 bytes past `DialogInstance` are two dwords at +0xb8/+0xbc
> (copied by `??0ToolTipDialog(const&)` 0x18004d180, written by nothing else read). The ported
> `ComboDialog::OnUpdate` called the root's `Contains` with `this` only; retail 0x180111c50 passes
> `(mx, my, _ui)` — fixed as a faithful correction. `Dialog::GetController` returns
> `Dialog::ElementController`, aliased to the 0x20 config entry from the export shapes (inferred).
> `combodialog` +0x10 is `_ui`, +0xb8 is `_list` (old comments said otherwise). `DialogTimer`'s
> `_remaining`/`_period` names are swapped per dialog.cpp's own notes; unrenamed.

> **CORRECTION to B6-4.5 and §2.4 (block D, 2026-09-25) — `SpriteContext` is NOT trivially
> copyable.** Retail exports a deep copy constructor (`??0SpriteContext@UI@@QEAA@AEBV01@@Z`
> 0x1800321c0, last store at +0x80) and a destructor (0x18011d250) that frees +0x10, +0x40, +0x58
> and +0x68 — four `Collection::Array` members. So the class is concrete (a stack object in
> `saveload_draw.cpp:734`) with a public trivial default ctor but no `is_trivially_copyable`
> assert; `sizeof == 0x88` (ctor stores through +0x70, `Begin` writes +0x80, copy ctor ends at
> +0x80). +0x50 is `RtUI::FontLayout _layout`; `_clipRegion` (+0x78) is `const Video::Viewport*`.
> `SpriteContext.h`'s old `FontGlyph` was the entry type of `Font::Layout` (removed;
> `FontLayoutEntry::_codepoint` is `_glyph`); `Font` +0x54 is `_baseLine`; `Font`'s 16-byte glyph
> row is retail's `UI::Font::Kerning` (`GetKern` 0x180031dd0 returns a pointer to it). The old
> banner's "AddSpriteDraw/Flush/Begin are not exported" was wrong — all three are exports.
> Block D's aliases: `::Sprite`, `::UIString`, `::SpriteSheet`, `::SpriteInfo`, `::StringTable`,
> `::StringTableTextEntry`, `::UiStringTable`, `RtSave::Sprite` → the `UI::` classes;
> `runtime/src/save/saveload.cpp` and `saveload_update.cpp` needed an 11-line member rename
> (`.sheet/.index` → `.spriteSheet/.spriteIndex`).

> **B6-11 SETTLED PER TYPE (block E, 2026-09-25, from the prologues of eleven `UserInterface`
> members).** `InternalPtr<T>` by value → HIDDEN POINTER (`AddDialog` `MOV RCX,[RDX]`,
> `BringDialogToFront`, `CloseDialog`, `IsDialogOpen`, `ShowDialog`, `EndToolTip`, `BeginToolTip`,
> `BeginDragDrop`, `PostEvent` all dereference the argument register). `ExternalPtr<T>` by value →
> BARE POINTER in the register (`SetCursor` stores RDX at +0xd0 then reads `[RDX+0x20]`; `SetSound`
> stores R8), confirming step 5. Any 16-byte class by value → HIDDEN POINTER (`UI::Sprite` in
> `SetFullscreenFadeSprite`: `MOV RAX,[RDX]`; `Video::Viewport` in `BeginSpriteCapture`:
> `MOV EAX,[R8]`). So: 8-byte trivially-copyable-in-MSVC's-eyes classes (`ExternalPtr`) travel in
> the register; `InternalPtr` (8 bytes but with a non-trivial copy in retail) and every ≥16-byte
> class travel by hidden pointer. `UI::String` by value (`TextElement::SetText`'s `VString@2@`) is
> therefore a hidden pointer too.

> **CORRECTION to B6-4.5 (block E): `UserInterface` is NOT trivially copyable** — three
> `Collection::Array<T>` members have a user-declared destructor (`Camera.h` records the same for
> `RendererDrawGroup`). The header asserts `!is_trivially_copyable` and says why; the class is
> concrete with a protected `= default` ctor and the two `RtEngine::` loop PODs as friends; the
> old POD already held the retail name with identical fields, so it simply became the class (no
> `RtUI::UserInterface`). Its alignof is 8 (the old banner's "4" was `ShowDialog`'s allocation
> alignment). `UI::DialogInstance::EndHover` is defined in `userinterface_hover.cpp` (block E).
> Block C1: `mingw-w64` GCC emits typeinfo as a COMDAT in every TU that uses it, so a leaf's
> `TypeName` can be defined while the base's `TypeName` is still a door — block A's key-function
> worry does not bite on this toolchain. `?CreateInstance@ButtonDescription@UI@@QEBAX…` (0x1801108d0)
> and `?UpdateText@ComboElement@UI@@` (0x180112180) are EXPORTED (old banners said otherwise);
> `??0CheckDescription` (0x1801115e0), `??0ComboDescription` (0x180112320), `??0ClipRegionDescription`
> (0x1800cc140) exist and are unregistered. Block C4: `layoutdescription.cpp`'s banner had the
> `LayoutDescription::Entry` name offsets permuted (+0x08 own name, +0x10/+0x18 relative-to; the
> body was right); `ListElement` +0xc0 is a `UI::Sprite _highlight`.

## 11–12. (reserved for the fragment passes and caller rounds)

## 13. NOT VERIFIED — read this before citing anything above

Read from RETAIL BYTES this pass: all 68 vtables' slot lists; all 68 `sizeof`; all 136
`_TypeName`/`_ParentTypeName` literals; the five Object parent vtables; every function
referencing each of the 68 vtable addresses; the 76 by-value-returning exports; every `??_7`
store in every UI ctor/dtor. Read from the TREE this pass: every `static_assert(sizeof)` under
`runtime/types/ui` and `engine/types`; the mint rows; the three subagent reports.

1. **42 layouts are undocumented** (§2.3). Sizes verified, fields not. Each block closes its own.
2. **`sizeof(SpriteContext)` is 0x88 or 0x90.** No deleting dtor. `UserInterface::Draw` (0x180121890) keeps it at `[RBP-0x80]` with the next distinct local at `RBP+0x14`, so 0x88 ≤ sizeof ≤ 0x94; its ctor's last store is the qword at +0x70 (`0x18011d1e0`), `Begin` writes +0x80. Block D pins it from `??1SpriteContext` (0x18011d250) or the `saveload_draw.cpp:734` stack frame.
3. **`Element::UpdateState`'s size and fields** — from `??0UpdateState` 0x180115600, not read here.
4. **The bodies behind the 68 slot-0 addresses** were not disassembled beyond the `MOV EDX,imm`;
   `deleting_dtor.cpp` carries 67 of them as `RT_DELDTOR` rows; the 68th, `StringTable`'s, is an
   inline body in `stringtable_factory.cpp` (`RtUiStringTable_ScalarDelete`) — `B36_FILEMAP.md` §6.
5. **The inventory's caller counts (§6)** are the inventory's greps, not re-measured here.
6. **I did not build, run or gate anything.**
7. **No Ghidra renames or annotations were made.** One read-only Java script
   (`b36_session/ghidra/UiAppendix.java`); `switch_program` was left on `Runtime-x64.dll`.
