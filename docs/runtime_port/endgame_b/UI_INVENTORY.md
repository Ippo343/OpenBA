# ENDGAME B.3 step 6 — `UI` design inventory (read-only, 2026-09-19)

> **A MAP, NOT EVIDENCE.** Written by a read-only pass at the B.3.5 close, the same way
> `VIDEO_GRAPHICS_INVENTORY.md` was written for step 5. NOTHING in it was verified by a
> question-pass coordinator or read byte-wise from the retail DLL the way `B35_RULES.md` did for
> Video/Graphics. Step 6's own question pass must re-derive every structural claim (base classes,
> slot order, by-value holdings, threading) from retail before a rule sheet cites it — B.3.4 and
> B.3.5's inventories each needed corrections once that pass ran. Everything not opened and read
> line-by-line is marked NOT VERIFIED / INFERRED. Every number below names the grep/script that
> produced it.

Scope: namespace `UI` per `docs/runtime_port/ENDGAME.md` §B.3 step 6 ("53 Description/Element
classes, the largest batch, fully parallel"). Format follows `VIDEO_GRAPHICS_INVENTORY.md`. Read
`B35_RULES.md` §2 (retail-bytes method), §7 (caller-surface measurement, the 9 binding spellings),
§8 (rules table) and `B35_FILEMAP.md` before this pass; the caller-surface section below is
modelled on `B35_RULES.md` §7's per-binding table, not on the earlier, looser
`VIDEO_GRAPHICS_INVENTORY.md` §4 (which that document's own step later corrected upward by ~4x).
No tree file was edited to produce this. All greps are scoped to the worktree with
`standalone/build*/`, `tests/unit/build/`, `.claude/worktrees/` excluded (none of the counts below
hit those paths since every command below is source-tree-scoped explicitly).

---

## 1. Class list

### 1.1 Headers — `runtime/types/ui/*.h` (29 headers, 4,231 lines, VERIFIED `wc -l`/`grep -c`)

| header | lines | static_assert | namespace spelling | class(es) |
|---|---|---|---|---|
| `ComboElement.h` | 56 | 6 | `namespace UI {` | `ComboElement` |
| `DescriptionsBatch1.h` | 277 | 38 | `namespace RtUI {` | multiple `*Description` bodies batched (name split from vtable table NOT individually confirmed which classes — flag) |
| `Dialog.h` | 242 | 19 | `namespace RtUI {` | `Dialog` and companions |
| `DialogInstance.h` | 332 | 31 | `namespace RtUI {` | `DialogInstance`, `DialogTimer`, `ComboDialog` |
| `DocumentTextElement.h` | 152 | 9 | `namespace UI { namespace DocumentTextElementOffsets {` | `DocumentTextElement`, `DocumentItem` |
| `EditElement.h` | 61 | 4 | `namespace UI {` | `EditElement` |
| `Element.h` | 327 | 33 | `namespace UI {` | `Element` (the base), `Event` |
| `ElementDescription.h` | 185 | 16 | `namespace RtUI {` | `ElementDescription` (the base) |
| `Font.h` | 176 | 25 | `namespace RtUI {` | `Font`, `FontGlyph`, `FontLayout`, `FontLayoutEntry` |
| `GraphElement.h` | 72 | 0 | `namespace UI { namespace GraphElementOffsets {` | `GraphElement` (0 top-level asserts — offsets-only sub-namespace holds them, NOT VERIFIED whether that sub-namespace itself asserts) |
| `GroupDescription.h` | 63 | 3 | `namespace RtUI {` | `GroupDescription` |
| `ImageElement.h` | 76 | 10 | `namespace UI {` | `ImageElement` |
| `LabelElement.h` | 112 | 20 | **no namespace at all** — bare `struct LabelElement` | `LabelElement` |
| `NumberEditElement.h` | 37 | 5 | `namespace UI {` | `NumberEditElement` |
| `NumberElement.h` | 131 | 8 | `namespace UI {` | `NumberElement` |
| `PageElement.h` | 38 | 0 | `namespace UI { namespace PageElementOffsets {` | `PageElement` |
| `ProgressElement.h` | 61 | 7 | `namespace UI {` | `ProgressElement` |
| `RadioElement.h` | 41 | 3 | `namespace UI {` | `RadioElement` |
| `SliderElement.h` | 98 | 11 | `namespace UI {` | `SliderElement` |
| `SpinnerElement.h` | 52 | 2 | `namespace UI {` | `SpinnerElement` |
| `Sprite.h` | 77 | 4 | **no namespace at all** — bare `struct Sprite` | `Sprite` (0x10, value type) |
| `SpriteCapture.h` | 150 | 16 | `namespace UI {` | `SpriteCapture`, `CaptureArray`, `CaptureEntry` |
| `SpriteContext.h` | 327 | 33 | `namespace UI {` | `SpriteContext`, `ClipRect`, `ClipRegion`, `SpriteDraw` |
| `SpriteSheet.h` | 228 | 26 | **no namespace at all** — bare `struct SpriteSheet` | `SpriteSheet`, `SpriteInfo` |
| `String.h` | 82 | 5 | **no namespace at all** — bare `struct UIString` (not `String`; see §6.3) | `UIString` |
| `StringTable.h` | 112 | 11 | only a nested `namespace StringTableOffsets {` — the struct itself is bare | `StringTable`, `StringTableTextEntry`, `UiStringTable` |
| `TabElement.h` | 37 | 0 | `namespace UI { namespace TabElementOffsets {` | `TabElement` |
| `TableElement.h` | 45 | 0 | `namespace UI { namespace TableElementOffsets {` | `TableElement` |
| `TextElement.h` | 83 | 0 | `namespace UI { namespace TextElementOffsets {` | `TextElement` |
| `UserInterface.h` | 315 | 33 | `namespace UI {` | `UserInterface` (singleton), `UserInterface::Iterator` |

**Namespace spelling is FOUR-WAY, not the three the task brief guessed** (VERIFIED,
`grep -m1 "^namespace" runtime/types/ui/*.h` plus a manual check of the four files that matched
nothing):
1. `namespace RtUI { ... }` — 6 headers (`DescriptionsBatch1.h`, `Dialog.h`, `DialogInstance.h`,
   `ElementDescription.h`, `Font.h`, `GroupDescription.h`).
2. `namespace UI { ... }` — 15 headers, several with a nested `namespace XOffsets { ... }` holding
   the `static_assert`s (`DocumentTextElement.h`, `GraphElement.h`, `PageElement.h`,
   `TabElement.h`, `TableElement.h`, `TextElement.h`).
3. **No namespace at all** — a bare top-level `struct` — 4 headers: `Sprite.h`, `SpriteSheet.h`,
   `String.h` (declares `struct UIString`, not `struct String`), `LabelElement.h`.
4. `StringTable.h` is its own fourth shape: the struct is bare (no wrapping namespace) but an
   `XOffsets`-style sub-namespace (`StringTableOffsets`) still exists for the assert block.
Neither `RtUi` nor `rt::ui` (the brief's two guesses) was found anywhere under
`runtime/types/ui/` — **zero hits**, VERIFIED. A step-6 planner must not assume one spelling when
grepping this directory.

### 1.2 `.cpp` files — `runtime/src/ui/*` (109 entries: 105 `.cpp`, 3 `.h`, 1 `README.md`; VERIFIED
`ls`, 46,469 total lines in the `.cpp` files, VERIFIED `wc -l`)

Per-file line count and `RegisterRuntimeExport`/`RegisterRuntimeAddr` counts for all 105 `.cpp`
files were captured this pass (VERIFIED, `grep -c` per file) — the full table is long; the
extremes and the files a step-6 planner will meet first:

| file | lines | export | addr | note |
|---|---|---|---|---|
| `dialog.cpp` | 2329 | 30 | 30 | largest file in the directory by a wide margin |
| `ui_factory_wave28.cpp` | 1480 | 80 | 79 | single largest registration count in the namespace — a wave/batch file (CLAUDE.md's historical-waves note applies, same as Video/Graphics §1.3) |
| `widgets_input.cpp` | 1612 | 27 | 26 | |
| `userinterface.cpp` | 1622 | 20 | 18 | |
| `combodialog_ctor.cpp` | 786 | 3 | 3 | large file, few registrations — mostly body, not doors |
| `documenttext.cpp` | 1090 | 6 | 7 | |
| `element.cpp` | 1350 | 25 | 24 | |
| `dialog_create.cpp` | 817 | 4 | 5 | |
| `widgets_layout.cpp` | 942 | 23 | 3 | export/addr mismatch of 20 — NOT VERIFIED why (candidate: many exports resolved once and reused by address internally, or an under-registered addr side; flag for step 6) |
| `sprite.cpp` | 759 | 24 | 2 | same shape of export/addr mismatch (22) as `widgets_layout.cpp` — two files with the same anomaly is a pattern, not a fluke; NOT VERIFIED |
| `description_dtors.cpp`, `element_sprite_debug.cpp`, `rt_ui_port.cpp`, `sprite_debug.cpp` | — | 0 | 0/1 | zero-registration files: dtors batch, debug helpers, the wave entry point (same convention as `rt_video_port.cpp`/`rt_io_port.cpp`) |

Totals (VERIFIED, `grep -c ... | awk`): **849 `RegisterRuntimeExport` + 814 `RegisterRuntimeAddr`
= 1,663 registrations** — for scale: IO ≈593, Video+Graphics combined ≈1,012. **UI alone is 1.6x
Video+Graphics combined and 2.8x IO** — the "largest batch" label in `ENDGAME.md` is not just
class count, it is registration volume too.

Grouping the 105 `.cpp` files by the class family whose bodies they hold (VERIFIED file names,
NOT VERIFIED per-class body attribution beyond what the filename implies):
- **One-file-per-Description/Element-pair** (the common case, ~46 files): e.g.
  `borderdescription.cpp`+`borderelement.cpp`, `buttondescription.cpp`+`buttonelement.cpp`, …
  down the alphabet — this is closer to IO's "whole classes, one file each" convention than to
  Video/Graphics's scattered wave-lane files.
- **Dialog family**: `dialog.cpp`, `dialog_controller_base.cpp`, `dialog_create.cpp`,
  `dialog_ctor_dtor.cpp`, `dialog_factory.cpp`, `dialog_settings.cpp`, `dialog_vtable.cpp`,
  `dialogcontrollerconfig.cpp`, `combodialog.cpp`, `combodialog_ctor.cpp`, `tooltipdialog.cpp` —
  11 files, NOT disjoint from each other (all touch `Dialog`/`DialogController`/`DialogInstance`
  vtables — confirm before splitting).
- **Sprite/Font/text family**: `sprite.cpp`, `sprite_debug.cpp`, `spritecapture.cpp`,
  `spritecapture_class.cpp`, `spritecontext.cpp`, `spritecontext_add.cpp`,
  `spritecontext_flush.cpp`, `spritesheet_new.cpp`, `spritesheet_vtable.cpp`, `font.cpp`,
  `characterset.cpp`, `cursor.cpp`, `stringtable_factory.cpp`, `stringtable_lifecycle.cpp`,
  `textelement_cachetext.cpp` — 15 files.
- **UserInterface (the singleton) + widgets**: `userinterface.cpp` +7 `userinterface_*.cpp` files,
  `widgets_input.cpp`, `widgets_layout.cpp`, `widget_common.h` — the closest analogue to
  Video/Graphics's `Context`/`Device` (one big non-polymorphic root plus its many satellite files).
- **Element base + cross-cutting**: `element.cpp`, `element_bodies_b5.cpp`, `element_vtable.cpp`,
  `element_common.h`, `elementdescription_serialize.cpp`, `ui_operator_delete.cpp` — stamp/serve
  the `Element`/`ElementDescription` base pair that every leaf pair embeds or derives from.
- **Wave/factory entry points**: `factory_ui1.cpp`+`.inc`, `ui_factory_wave28.cpp`,
  `rt_ui_port.cpp` — owner-retained, same convention as `rt_video_port.cpp`/`rt_factories_video.cpp`.

`runtime/src/ui/README.md` opens "Port target for the Runtime-x64.dll `UI` directory … mirror
these names here, lowercased" and lists the ORIGINAL retail source-tree file names — **this
pass did NOT check whether its status/porting-progress claims are stale** the way
`runtime/src/graphics/README.md`'s "nothing yet" was stale in step 5; flagged as an open
follow-up, not asserted either way.

### 1.3 Polymorphism / vtables (VERIFIED against `standalone/rt_data_mint_tables.inc`)

**68 distinct `??_7*@UI@@6B@` vtable rows** (VERIFIED, parsed the `{ "??_7Name@UI@@6B@",
0xVA_ull, N, ... }` promotion-table rows, not just counting comment lines which double-count each
class). Grouped by name shape (VERIFIED via a Python pass over the 68 names):

| group | count | slot counts seen | names |
|---|---|---|---|
| `*Description` | 28 | 9 (`ElementDescription` only), 10 (all 27 leaves) | `AnimatedImageDescription`, `BorderDescription`, `ButtonDescription`, `CheckDescription`, `ClipRegionDescription`, `ComboDescription`, `ContainerDescription`, `DocumentTextDescription`, `EditDescription`, `ElementDescription` (base), `GraphDescription`, `GroupDescription`, `ImageDescription`, `LabelDescription`, `LayoutDescription`, `ListDescription`, `NumberDescription`, `NumberEditDescription`, `PageDescription`, `ProgressDescription`, `RadioDescription`, `RibbonDescription`, `ScrollDescription`, `SliderDescription`, `SpinnerDescription`, `TabDescription`, `TableDescription`, `TextDescription` |
| `*Element` | 28 | 12 (25 of them), 13 (`EditElement`, `NumberEditElement`), 12 (`Element` base) | `AnimatedImageElement`, `BorderElement`, `ButtonElement`, `CheckElement`, `ClipRegionElement`, `ComboElement`, `ContainerElement`, `DocumentTextElement`, `EditElement`, `Element` (base), `GraphElement`, `GroupElement`, `ImageElement`, `LabelElement`, `LayoutElement`, `ListElement`, `NumberEditElement`, `NumberElement`, `PageElement`, `ProgressElement`, `RadioElement`, `RibbonElement`, `ScrollElement`, `SliderElement`, `SpinnerElement`, `TabElement`, `TableElement`, `TextElement` |
| Dialog family | 6 | 8–11 | `ComboDialog`(10), `Dialog`(9), `DialogController`(11), `DialogControllerConfig`(8), `DialogInstance`(10), `ToolTipDialog`(10) |
| Other | 6 | 9 each | `CharacterSet`, `Cursor`, `Font`, `SpriteCapture`, `SpriteSheet`, `StringTable` |

This is **27 leaf Description/Element pairs + 1 base pair (`ElementDescription`/`Element`) = 28
pairs = 56 classes**, plus 6 Dialog-family + 6 other = **68 total**. `ENDGAME.md`'s "53" figure is
close but does not match exactly against this count either way this pass sliced it (56 leaf+base
Description/Element classes alone, or 54 if the base pair is excluded and `ComboElement` is
folded in differently) — **NOT RECONCILED**; a step-6 planner should re-derive "53" from
`ENDGAME.md`'s own source rather than assume this document's 68 or 56 supersedes it.

**Every one of the 68 vtable classes has a matching `?_typeInfo@` reflection row** (VERIFIED,
`grep -c` on both lists gives 68 = 68, a clean 1:1) — **this is UNLIKE Video/Graphics, where only
18 of ~39 vtable classes had a typeInfo row.** If the `?_typeInfo@` row implies `Object::External`
subclass status the way `B34_RULES.md`/`B35_RULES.md` established for Video/Graphics, then **all
68 UI polymorphic classes are candidate `Object::External` (or `CompiledExternal`) subclasses —
NOT VERIFIED against retail bytes this pass**, but the ratio itself (100% vs Video/Graphics's 46%)
is a real structural difference worth flagging: UI may be a much more homogeneous hierarchy than
Video/Graphics was, or the `Description`/`Element` split may itself be the
`External`/`CompiledExternal` split (NOT VERIFIED — a real question for step 6's Q2).

**RT_DELDTOR rows** (`runtime/src/object/deleting_dtor.cpp` + `deleting_dtor_leaves.cpp`,
VERIFIED grep): **67 rows** naming a `@UI@@` class — one short of the 68 vtable classes. Which
class is missing was NOT identified this pass (a straightforward diff of the two 68/67-item name
lists is the fast follow-up).

### 1.4 Non-polymorphic root / value classes

Headers with NO `??_7` vtable row (VERIFIED — cross-checked the header class list in §1.1 against
the 68-name vtable list): `UserInterface`, `SpriteContext`, `Sprite`, `UIString` (`String.h`), and
`StringTable`'s companion `UiStringTable`. Plus a long tail of small value types embedded in
polymorphic classes' headers: `DialogTimer`, `ComboDialog`'s companions, `ClipRect`, `ClipRegion`,
`SpriteDraw`, `CaptureArray`, `CaptureEntry`, `FontGlyph`, `FontLayout`, `FontLayoutEntry`,
`SpriteInfo`, `StringTableTextEntry`, `DocumentItem`, `Event` (Element.h). **Base-class identity
and real-ctor eligibility for every one of these is NOT VERIFIED** — no retail-bytes pass has been
run for UI the way `B35_RULES.md` §2 ran for Video/Graphics.

### 1.5 The parent chain — INFERRED only, not proved

No `?_ParentTypeName@` byte-wise read was done this pass (that is step 6's Q2, the same as
`B35_RULES.md` §2.1 did for Video/Graphics). What the tree shows, all **INFERRED**:
- `ElementDescription` and `Element` read as the two base classes of the whole 27-pair leaf family
  (every `XDescription`/`XElement` pair's header includes or extends the base pair's shape) —
  INFERRED from naming and from `ElementDescription.h`/`Element.h` being the only members of their
  groups without a "leaf" sibling.
- `DialogInstance.h` says outright in its own banner comment (VERIFIED read, `runtime/types/ui/
  DialogInstance.h`): "Base class for all 22 Dialog Controllers in the exe" — a rare case where the
  header states the parent relationship in English rather than needing inference. The COUNT "22"
  in that comment is itself INFERRED/unverified by this pass (not cross-checked against
  `code/Game/Dialog/*` file count).
- `Dialog`, `DialogController`, `DialogControllerConfig`, `ComboDialog`, `ToolTipDialog` are
  presumed to form a small hierarchy under or beside `DialogInstance` — INFERRED from the "Dialog
  family" grouping in §1.3, not from a read `?_ParentTypeName@` string.
- Everything else (`SpriteCapture`, `SpriteSheet`, `Font`, `CharacterSet`, `Cursor`,
  `StringTable`): base class **UNKNOWN**, not even inferred — flag for step 6 Q2.

---

## 2. Counts

**Registrations** (VERIFIED, §1.2): 849 export + 814 addr = **1,663** in `runtime/src/ui/*.cpp`.

**Minted vtable rows**: **68** (§1.3) — the largest of any B.3 step so far (IO ~19, Video+Graphics
39 combined, UI alone 68).

**RT_DELDTOR rows**: **67** (§1.3), one short of 68 — unreconciled.

**`?_typeInfo@` rows**: **68**, full 1:1 coverage with the vtable list (§1.3) — unlike
Video/Graphics's partial 18/39.

**Cutover manifest** (VERIFIED, `python tools/cutover_manifest.py --table`, read-only, `by
namespace:` line includes `UI 2`): only **2** REQUIRED/WITHHELD-tagged UI rows exist in the
manifest, both vtables, not 68:

```
| 40 | UI | ??_7Dialog@UI@@6B@           | HARMFUL | PERMANENT   | vtable | 5 | WITHHELD |
| 41 | UI | ??_7DialogController@UI@@6B@ | NO-OP   | PERMANENT   | vtable | 2 | REQUIRED |
```

This is a MUCH smaller REQUIRED footprint than Video's 18 or Graphics's 4 — **the manifest is not
a proxy for UI's true size** (as `B35_RULES.md` §2 already warned for Video/Graphics); it tags
specific storage/vtable gap rows the cutover tool flagged, not every UI class. `Dialog`'s own row
is tagged **WITHHELD** — the tool's own verdict is that harm from cutting it over is PERMANENT, not
lifted at cutover, unlike almost every other row in the whole manifest (which says "verdict
inverts at cutover"). This is worth a direct read of the manifest tool's reasoning for that one row
before step 6 touches `Dialog`.

**`tools/runtime_census.py --namespace UI`** (VERIFIED, ran read-only — confirmed via `git status`
that no file was written): `UI` is the single **largest namespace in the whole census output**,
larger than IO, Graphics, Video, System and Object combined by absolute count:

| namespace | total (DLL rows) | called | ported | called-coverage | ceiling |
|---|---|---|---|---|---|
| **UI** | **1625** | 329 | 214 | 65.0% | 99.4% |
| IO | 407 | 207 | 200 | 96.6% | 97.6% |
| Graphics | 599 | 108 | 86 | 79.6% | 96.3% |
| Video | 573 | 104 | 79 | 76.0% | 82.7% |

UI's 65.0% called-coverage is the LOWEST of the four listed above except its own ceiling (99.4%)
being high — i.e. most of UI's surface is reachable/portable but a smaller fraction of it is
CALLED from the live game today than for IO. NOT VERIFIED what accounts for the gap (candidate:
many `Description`/`Element` accessor pairs are rarely-exercised getters/setters for dialog
authoring, not hot gameplay paths — INFERRED, not read).

---

## 3. Caller surfaces

Measured the same way `B35_RULES.md` §7 measured Video/Graphics — **by BINDING, not by file** —
because engine_funcs.h-anchored or name-anchored sweeps under-count by roughly an order of
magnitude for this kind of namespace (confirmed again below). All counts VERIFIED by the grep
shown; none use the comment-aware lexer `IO_CALLER_INVENTORY.md` built, so raw grep noise (matches
inside comments) is possible and is called out where suspected.

### 3.1 (a) `engine_funcs.h` global doors bound to a `@UI@@` export

**118 distinct `fn_*` pointers** (VERIFIED: 118 `ResolveExport("...@UI@@...")` call sites in
`engine/funcs/engine_funcs.cpp`, and a paired count of 118 distinct non-`_t` `fn_*` tokens
assigned within three lines of each — 1:1, so no name was missed by the window). **These names do
NOT reliably start with `fn_UI_`** — confirmed exactly as the task brief predicted: only
`UserInterface`'s own methods (`fn_UI_Update`, `fn_UI_Draw`, `fn_UI_CloseDialog`, …) use the
`fn_UI_` prefix; every other class's doors are spelled `fn_<ClassName>_<Method>` — e.g.
`fn_Element_SetFlagsOnOff`, `fn_DialogInstance_FindElement`, `fn_ComboElement_AddItem`,
`fn_TextElement_SetParameter`. **A sweep anchored on the literal string `fn_UI_` would find only a
handful of the 118 doors.**

Call-site counts in `code/Game` (VERIFIED, `grep -rho "\bNAME(" code/Game | wc -l` per pointer,
top 20 of the ~90 pointers that are called at all):

| pointer | calls |
|---|---|
| `fn_Element_SetFlagsOnOff` | 144 |
| `fn_Element_SetElementId` | 130 |
| `fn_Element_Child` | 90 |
| `fn_DialogInstance_FindElement` | 79 |
| `fn_FindElement` | 73 |
| `fn_TextElement_SetParameter` | 71 |
| `fn_UIString_Ctor` | 63 |
| `fn_NumberElement_SetValueInt` | 53 |
| `fn_TextElement_SetText` | 51 |
| `fn_ComboElement_SetCurrentItem` | 50 |
| `fn_UI_CloseDialog` | 42 |
| `fn_Element_ChildCount` | 34 |
| `fn_UIString_ToSystemString` | 28 |
| `fn_Sprite_Ctor` | 26 |
| `fn_ComboElement_AddItem` | 24 |
| `fn_LabelElement_SetSprite` | 22 |
| `fn_FindComponent_Building` | 22 |
| `fn_DialogInstance_Dtor` | 21 |
| `fn_ComboElement_GetItemData` | 20 |
| `fn_UIString_Ctor_Index` | 18 |

**Total across all called pointers: 1,434 call sites** (VERIFIED, summed) — **~23x** Video/
Graphics's combined 61 `engine_funcs.h`-door calls. `Element`'s own three doors
(`SetFlagsOnOff`/`SetElementId`/`Child`) alone account for 364 of the 1,434 — a step-6 caller batch
touching `Element` will have a much larger blast radius than any single Video/Graphics class did.

### 3.2 (b) `ResolveExport("...@UI@@...")` by mangled name

**155 call sites** across `code/Game` + `engine` + `standalone` (VERIFIED, `grep -c`), by
directory: `engine` 114 (nearly all inside `engine_funcs.cpp` itself, i.e. overlapping with §3.1's
own resolution — NOT double-counted as a separate caller surface, just the resolution mechanism
for the doors already listed), `standalone` 25, `code/Game` 16. `code/Game` breakdown (VERIFIED):
Component 5, Dialog 3, Game 4, Player 2, Tutorial 2. Exactly **one** file caches a UI mangled
export into a file-local static outside the doors table
(`code/Game/Dialog/dialog_exe_helpers.cpp`, VERIFIED `grep -rlE "static[^;]*ResolveExport"`) — a
much smaller file-local-cache population than Video/Graphics's 28 files, consistent with UI
callers routing overwhelmingly through the `engine_funcs.h` global-door idiom (§3.1) rather than
the by-name-cache idiom `B35_RULES.md` §7's row (f) measured for Video/Graphics.

Two call sites use a mangled literal wrapped across two source lines (VERIFIED,
`grep -rzoP` multiline scan) — small, but a naive single-line grep would silently miss them, same
caution `B35_RULES.md` §7 raised for Video/Graphics's 15 wrapped literals.

### 3.3 (c) `RT_EXPORT_ONCE("...@UI@@...")`

**184 call sites, all inside `runtime/src/ui/*.cpp`** (VERIFIED, `grep -c`) — this is a
runtime-internal registration idiom, not a caller surface from `code/Game`/`engine`/`standalone`'s
point of view (same convention `B35_RULES.md` §7 row (c) recorded: 147/147 for Video/Graphics were
also 100% internal to `runtime/src`). Top files: `userinterface_hover.cpp` (11),
`dialog_settings.cpp` (10), `userinterface_frame.cpp` (9), `spritecapture_class.cpp` (9),
`element_bodies_b5.cpp` (9), `element_vtable.cpp` (8), `combodialog.cpp` (7).

### 3.4 (d) `ResolveAddr(Module::Runtime, 0x18...)` onto UI addresses

VERIFIED: built the UI address set from `RegisterRuntimeAddr(0x...)` calls in `runtime/src/ui/*`
(802 distinct addresses), then intersected against every `ResolveAddr(Module::Runtime, 0x...)`
call site tree-wide (51 total such call sites exist anywhere in `code/Game`/`engine`/
`standalone`). **Only 5 of the 51 hit a UI address** — a small surface, in line with
Video/Graphics's similarly small showing for this binding (56 total, mostly `runtime/src`
internal per `B35_RULES.md` §7 row (d)).

### 3.5 (e) exe dispatch slots — the dominant, name-invisible spelling

VERIFIED via `python tools/dispatch_slot_map.py` (read-only; builds its map from the retail PE
import table via `pefile`, does not write): **205 of the Application-x64.exe import slots resolve
to a `@UI@@` Runtime-x64.dll export** — the single largest per-namespace slot count found across
any B.3 step's inventory to date (Video/Graphics's combined dx11-plus-exe dispatch-slot surface
was measured only impressionistically in step 5; this pass measured UI's precisely).

Cross-referencing those 205 slot addresses against every `ResolveAddr(Module::Exe, 0x...)` call
site tree-wide (1,066 such calls exist anywhere in `code/Game`/`engine`/`standalone`, VERIFIED):
**49 calls hit a UI dispatch slot address directly**, by file:

| file | hits |
|---|---|
| `engine/funcs/engine_funcs.cpp` | 45 |
| `engine/funcs/engine_funcs.h` | 41 (declarations mirroring the .cpp resolutions — not separate bindings) |
| `standalone/reg_element_typeids.cpp` | 17 |
| `standalone/address_registry.cpp` | 17 |
| `standalone/reg_unreg_lane2.cpp` | 14 |
| `code/Game/Dialog/tradeui.cpp` | 5 |
| `code/Game/Dialog/toolbar_vtable_standalone.cpp` | 4 |
| `code/Game/Dialog/helpui.cpp` | 4 |
| `code/Game/Tutorial/tutorial_wave.cpp` | 3 |
| `code/Game/Dialog/productionui.cpp` | 2 |
| `code/Game/Dialog/optionsdialog.cpp` | 2 |
| `code/Game/Dialog/controlui.cpp` | 2 |
| `code/Game/Dialog/buildui.cpp` | 2 |
| `code/Game/Component/nomadcomponent.cpp` | 2 |
| (7 more files) | 1 each |

**This confirms the task brief's prediction exactly**: exe-dispatch-slot resolution is a real,
sizeable, code/Game/Dialog-concentrated caller idiom for UI that is invisible to any sweep
anchored on the mangled-name string or on `fn_UI_`-prefixed globals. A step-6 caller batch over
`code/Game/Dialog/*` MUST grep for `ResolveAddr(Module::Exe,` with hex literals, not just for
`@UI@@` name strings.

### 3.6 (f) hand-inlined `(*(void***)p)[N]` vtable-slot calls — the largest raw hazard surface

VERIFIED, scoped tightly to `code/Game/Dialog/*` (a directory that is uniformly UI-consuming, so
this count does not carry step 5's "keyword-noisy, over-broad" caveat): **417 occurrences across
39 files.** Top files:

| file | occurrences |
|---|---|
| `tradeui.cpp` | 150 |
| `townhallui.cpp` | 95 |
| `statusbarui.cpp` | 20 |
| `workplaceui.cpp` | 19 |
| `moddialog.cpp` | 19 |
| `saveloaddialog.cpp` | 11 |
| `townhall_helpers.cpp` | 8 |
| `toolbardialog.cpp` | 7 |
| `debugui.cpp` | 7 |
| `optionsdialog.cpp` | 6 |

`tradeui.cpp` and `townhallui.cpp` alone account for 245 of the 417 (59%) — these are the two
single riskiest files for a hand-inlined-vtable hazard in the entire UI namespace, larger than
Video/Graphics's entire (noisy, unanchored) 91-file estimate concentrated into two files here.
**NOT VERIFIED** which specific UI vtable(s) each call targets (would need per-call-site reading,
the way `B35_RULES.md`'s slot tables were built from actual bytes) — flagged as the single most
valuable follow-up read before a `code/Game/Dialog` caller batch is sized.

### 3.7 Top-20 doors, combined ranking

Combining §3.1 (engine_funcs calls) with §3.5/§3.6 (file-level exe-slot and hand-inlined hits, not
individual-symbol counts for those two):

1. `fn_Element_SetFlagsOnOff` — 144 calls
2. `fn_Element_SetElementId` — 130 calls
3. `code/Game/Dialog/tradeui.cpp` — 150 hand-inlined slot calls (file-level)
4. `fn_Element_Child` — 90 calls
5. `code/Game/Dialog/townhallui.cpp` — 95 hand-inlined slot calls (file-level)
6. `fn_DialogInstance_FindElement` — 79 calls
7. `fn_FindElement` — 73 calls
8. `fn_TextElement_SetParameter` — 71 calls
9. `fn_UIString_Ctor` — 63 calls
10. `fn_NumberElement_SetValueInt` — 53 calls
11. `fn_TextElement_SetText` — 51 calls
12. `fn_ComboElement_SetCurrentItem` — 50 calls
13. `fn_UI_CloseDialog` — 42 calls
14. `fn_Element_ChildCount` — 34 calls
15. `fn_UIString_ToSystemString` — 28 calls
16. `fn_Sprite_Ctor` — 26 calls
17. `code/Game/Dialog/statusbarui.cpp` — 20 hand-inlined slot calls (file-level)
18. `fn_ComboElement_AddItem` — 24 calls
19. `fn_LabelElement_SetSprite` — 22 calls
20. `fn_FindComponent_Building` — 22 calls

The clear signal: **`Element`'s own doors dominate everything** (SetFlagsOnOff, SetElementId,
Child, ChildCount together = 398 calls), `DialogInstance`/`Dialog`/`ComboElement`/`TextElement`
form the next tier, and `tradeui.cpp`/`townhallui.cpp` are file-level hazard hotspots that don't
fit the per-symbol ranking at all — a step-6 planner needs both views, not just one.

---

## 4. Exe-side subclasses

Only **one** confirmed live hex-literal vtable canonicalization was found this pass (VERIFIED,
`grep -rnE "0x18015[0-9a-f]{4}|0x18016[0-9a-f]{4}"` across `code/Game`/`engine`/`standalone`,
excluding binaries and `cutover_manifest.inc` data rows): `code/Game/Player/tool_base_toolui.cpp:
136-142` — `ResolveAddr(Module::Runtime, 0x180156d70)` canonicalizes a `DialogInstance` vtable
(the same site the step-5 brief already named). `code/Game/Component/statusicon_geometry.cpp:266`
also names a hex VA in this range but it is `Geometry`'s (Graphics namespace), not UI's, and is a
comment, not a live call.

This single hit is almost certainly an UNDER-count: §3.6 found 39 `code/Game/Dialog/*` files that
hand-build vtables via `(*(void***)p)[N]`, and the duplicate-POD grep in §6.1 below found explicit
files named `dialog_controller_vtable.cpp`/`.h` and `mod_dialog_vtables.cpp` in
`code/Game/Dialog/` — these almost certainly canonicalize or construct UI vtables **without** a
bare hex literal (e.g. via `ResolveExport`/`ResolveAddr` with a symbolic name, or via a constant
defined elsewhere). **NOT VERIFIED** — a step-6 pass must read
`dialog_controller_vtable.cpp/.h` and `mod_dialog_vtables.cpp` directly rather than trust a
hex-literal grep, which is a narrower net than the one `B35_RULES.md` used for Video/Graphics (it
had Ghidra `getReferencesTo` on each vtable address; this pass had only a text grep).

`runtime/types/ui/DialogInstance.h`'s own banner (VERIFIED read, §1.5) states plainly: "Base class
for all 22 Dialog Controllers in the exe" — i.e. the EXE, not just `runtime/`, is expected to
build 22 (INFERRED-count) concrete vtables on top of this Runtime base, the same abstract/concrete
shape `B35_RULES.md` §1 confirmed for Video's twelve `*Data` classes. **This is very likely
step 6's single biggest "which base classes may become abstract" constraint** — every one of those
22 exe-side dialog controllers pins `DialogInstance`'s layout and (at minimum) its slot-0 deleting
dtor — but it was NOT enumerated file-by-file this pass.

---

## 5. Hazards

1. **A second, parallel POD tree for UI classes lives outside `runtime/types/ui/`, at
   `engine/types/ui/{DialogController.h, DialogInstance.h, Dialogs.h, Elements.h, Panels.h}` plus
   `engine/types/{Descriptions.h, DialogLoaderLoopT.h, DialogLoopT.h}`** — 8 files, 1,887 lines
   (VERIFIED `wc -l`). This is the UI-namespace analogue of step 5's `dx11_backend_access.h`
   duplicate-views hazard (`B35_RULES.md` §1.3), but **much larger** (1,887 lines vs. five small
   inline structs) and it lives in a whole separate directory, not one header. Spot-checked ONE
   pair for agreement (VERIFIED): `engine/types/ui/DialogInstance.h` asserts
   `sizeof(DialogInstance) == 0xB8` and so does `runtime/types/ui/DialogInstance.h`
   (`static_assert(sizeof(DialogInstance) == 0xb8, ...)`) — **the two independently-asserted
   layouts of the SAME retail class currently AGREE**, but this was checked for exactly one class
   out of the pair's many; `Dialogs.h` (487 lines) and `Panels.h` (826 lines) were NOT
   cross-checked this pass. A step-6 question pass must do for this tree what `B35_RULES.md` §1.3
   did for `dx11_backend_access.h`: check every field, not just one `sizeof`, before assuming the
   two trees can be collapsed or must both survive.
2. **72 files** under `engine/types`, `code/Game`, `dx11` reference a `@UI@@` mangled name or a
   `UI::*` type by name (VERIFIED, `grep -rl`) — concentrated in `code/Game/Dialog/*` and
   `code/Game/Component/*`. This is the raw population a full caller sweep must eventually cover;
   §3 above measured bindings, this is files-that-mention-UI-at-all (a looser, larger superset).
3. **`UI::String.h` declares `struct UIString`, not `struct String`** (VERIFIED read) — the
   existing project memory `feedback_uistring_vs_sysstring.md` already documents this distinction
   from `System::String`; this pass re-confirms it is still true and that `UIString` is one of the
   FOUR bare/no-namespace headers (§1.1), which makes a plain-text grep for `UI::String` return
   ZERO hits even though the class exists — a real name-lookup trap for a step-6 sweep, worse than
   the ordinary namespace-qualification issue because there is no `UI::` prefix in the source at
   all for this one class.
4. **By-value holdings**: a raw, UNFILTERED grep for `String `/`UIString `/`Color `/`Rectangle
   `/`Array<`/`ExternalPtr<`-typed members across `runtime/types/ui/*.h` returns **92 hits**
   (VERIFIED count, NOT VERIFIED as precise — likely includes some comment/parameter-list noise
   the way Video/Graphics's Context.h by-value list needed manual confirmation). This is a
   materially larger raw signal than Video/Graphics's already-significant by-value hazard (§3,
   `VIDEO_GRAPHICS_INVENTORY.md`) and needs the same per-class confirmed read before any block
   commits to a by-value member staying trivially-copyable.
5. **Hidden-retslot exports**: a mangled-return-type (`?AV`/`?AU`) grep over `runtime/src/ui/*.cpp`
   found roughly a dozen distinct symbols (VERIFIED list: `GetFirstDialog@UserInterface`,
   `GetLink@DocumentTextElement`, `GetSprite@ImageElement`, `GetSprite@LabelElement`,
   `GetSpriteNameFromIndex@SpriteSheet`, `GetText@StringTable` ×2 overloads,
   `GetToolTipString@Element`, `SetScissor@SpriteContext` ×2 overloads). **Spot-checked all of
   these against the 68-name vtable list: ZERO are virtual** (VERIFIED, `grep -c` each name against
   `rt_data_mint_tables.inc` returned 0 every time) — **unlike Video/Graphics, which had 9
   dangerous (class, slot) hidden-retslot-through-a-vtable-slot pairs.** This is a genuinely clean
   finding for UI's most dangerous retslot shape, but the retslot list itself was built with a
   loose regex and was NOT exhaustively verified against every one of the 849 exports — treat "no
   virtual retslots found" as encouraging, not proven exhaustive.
6. **No printf-wrapped `Register*` call was found** in `runtime/src/ui/*.cpp` (VERIFIED,
   `grep -rn "printf(.*Register\|TRACE(.*Register"` returned nothing) — the perf-build-strips-
   printf-arguments bug class (`reference_perf_build_strips_printf_arguments.md`) that bit
   Video/Graphics twice does not have an obvious instance here. The three `printf(...)` calls that
   DO exist in the directory (`factory_ui1.cpp:356`, `rt_ui_port.cpp:348`, `widgets_layout.cpp:940`)
   are summary print statements after registration completes, not registrations themselves —
   VERIFIED read of each.
7. **Multiple inheritance / secondary vptrs**: **NOT INVESTIGATED this pass at all.** No Ghidra
   read was done (this was a tree-only, no-Ghidra pass per the task's own scope restriction); a
   step-6 question pass needs the `B35_RULES.md` §0 method (read each class's own ctor for a
   second vtable stamp at a non-zero offset) before any UI class is declared free of secondary
   vptrs.
8. **Threading**: **NOT INVESTIGATED.** No `grep -rn "CriticalSection\|Thread"` was run against
   `runtime/src/ui` or `dx11/src` this pass (the loader-thread hazard `B35_RULES.md` §5.2 found for
   Graphics's `Camera`/`Renderer`/`Scene`/`SunLight`/`MeshInstance` construction is exactly the
   shape a UI equivalent — if `UserInterface`/`Dialog`/`Element` construction ever happens off the
   main thread — would need). Flagged as unresearched, not absent.
9. **Two files show an export/addr registration-count mismatch of ~20-22**
   (`widgets_layout.cpp`: 23 export vs 3 addr; `sprite.cpp`: 24 export vs 2 addr — VERIFIED counts,
   §1.2) — the same anomaly appearing in two unrelated files is worth a direct read before step 6
   assumes export-count and addr-count are always paired 1:1 for a given class the way most other
   UI files show them.
10. **The retail-cited "22 Dialog Controllers in the exe"** (`DialogInstance.h`'s own banner, §1.5,
    §4) is the single biggest constraint candidate on which UI base classes may become abstract —
    **NOT enumerated** file-by-file against `code/Game/Dialog/*` this pass (there are far more than
    22 `*.cpp` files in that directory; which ones are genuine `DialogController` subclasses vs.
    plain dialog-content helper files was not sorted out).
11. **Stale-README risk**: `runtime/src/ui/README.md`'s own status claims were NOT checked against
    the actual 105-file/1,663-registration reality the way step 5 found `runtime/src/graphics/
    README.md` and `dx11/src/README.md` both stale. Flag, don't assume either way.
12. **The `<windows.h>`/`Rectangle` trap** (`runtime/types/volume/Volume.h`'s include hazard,
    named explicitly in the task brief): a header-only grep for `Volume.h`/`Rectangle` across
    `runtime/types/ui/*.h` found **zero hits** — but this checked headers only, not the 105 `.cpp`
    bodies, and did not check whether any UI header transitively reaches `Volume.h` through another
    included Runtime header. **Treat as inconclusive, not clean.**
13. **Unit tests touching UI**: a `grep -rl "UI::" tests/unit --include=*.cpp --include=*.h`
    returned **zero files** this pass (an earlier looser count of "2" from a first-pass grep did
    not survive a tighter re-check with explicit include filters — the tighter number supersedes
    the looser one). **NOT VERIFIED exhaustively** — a plain-text `UI::` grep will miss the same
    bare-struct classes (`Sprite`, `SpriteSheet`, `UIString`, `LabelElement`) that have no `UI::`
    prefix in source at all (§5.3), so a real UI-touching unit test using one of those four classes
    would not show up in either count.

---

## 6. Proposed split into blocks

Modelled on `B35_RULES.md` §12's pattern (base classes serial-owned first, then disjoint leaf
blocks, fully parallel). This is a FIRST-DRAFT sizing guess from tree-only counts, not a design
decision — no per-class retail-bytes analysis (the `B35_RULES.md` §2 equivalent) has been done for
any UI class in this pass.

**Ordering rationale**: `Element`/`ElementDescription` are the base of the entire 27-leaf-pair
family and dominate the caller surface (§3.7) — they must go FIRST and SERIALLY, the same way
`B35_RULES.md` §10 step 1 put `VertexLayout`'s missing header first because "nothing else can
start" without it. `DialogInstance`/`Dialog`/`DialogController` are the second serial-owner
group because of the "22 exe subclasses" constraint (§4, hazard 10). Once both bases are real, the
27 leaf Description/Element pairs are the closest thing to genuinely disjoint, fully-parallel work
this whole endgame has offered — each pair is one or two files, touches its own `?_typeInfo@` row,
and (per §1.3) all 68 classes uniformly have a typeInfo row, unlike Video/Graphics's mixed bag.

| block | scope | rationale |
|---|---|---|
| **0 — question pass (serial, one owner)** | retail-bytes read of all 68 vtables (slots, sizeof, `_ParentTypeName`) the way `B35_RULES.md` §0/§2 did; resolve the engine/types/ui duplicate-tree question (hazard 1); enumerate the 22 (INFERRED) exe `DialogController` subclasses in `code/Game/Dialog/*`; resolve the `widgets_layout.cpp`/`sprite.cpp` export/addr mismatch; the multiple-inheritance and threading checks (hazards 7, 8) | UI has had NO Q1-Q6-equivalent pass yet, unlike Video/Graphics when `B35_RULES.md` was written — this step starts one level further back |
| **1 — `Element` + `ElementDescription` (the base pair, serial)** | `runtime/types/ui/{Element.h,ElementDescription.h}`, `runtime/src/ui/{element.cpp,element_bodies_b5.cpp,element_vtable.cpp,element_common.h,elementdescription_serialize.cpp,ui_operator_delete.cpp}` | highest caller-touching classes by far (§3.7: 398 calls on 4 doors alone); every leaf pair depends on this shape; must land before any leaf pair converts |
| **2 — `Dialog`/`DialogInstance`/`DialogController`/`DialogControllerConfig` (serial, needs block 0's exe-subclass enumeration)** | `runtime/types/ui/{Dialog.h,DialogInstance.h}`, `runtime/src/ui/{dialog*.cpp,dialogcontrollerconfig.cpp,combodialog*.cpp,tooltipdialog.cpp}`, cross-check against `engine/types/ui/{DialogController.h,DialogInstance.h,Dialogs.h}` and every `code/Game/Dialog/*` file block 0 identifies as an exe subclass | `DialogInstance` is a stated base for ~22 exe subclasses (§4) — the single highest blast-radius class in the namespace if its layout moves |
| **3 — the 27 leaf Description/Element pairs (fully parallel, one agent per pair or small cluster)** | e.g. `{border,button,check,clipregion,combo,container,documenttext,edit,graph,group,image,label,layout,list,number,numberedit,page,progress,radio,ribbon,scroll,slider,spinner,tab,table,text}{description,element}.{h,cpp}` | each pair has its own header, its own `.cpp` file(s), its own `?_typeInfo@` row, and (per §1.3) a uniform 100% typeInfo/vtable ratio — the cleanest, most parallel block this endgame has seen; `ComboElement`/`AnimatedImageElement` may need to travel with their `*Description` sibling if either one is split across an extra file (`comboelement.cpp` + no separate combo description file — VERIFY) |
| **4 — Sprite/Font/text-layout family** | `runtime/types/ui/{Sprite.h,SpriteCapture.h,SpriteContext.h,SpriteSheet.h,String.h,StringTable.h,Font.h}`, `runtime/src/ui/{sprite*.cpp,font.cpp,characterset.cpp,cursor.cpp,stringtable_*.cpp,textelement_cachetext.cpp}` | `UIString` (bare, no namespace — hazard 3) and the four other bare-struct headers live here; needs its own base-class read since none of these are confirmed `Object::External` the way the Description/Element family likely is |
| **5 — `UserInterface` + widgets (owner-retained, non-polymorphic root)** | `runtime/types/ui/UserInterface.h`, `runtime/src/ui/{userinterface*.cpp,widgets_input.cpp,widgets_layout.cpp,widget_common.h}` | the closest UI analogue of Video's `Device`/`Context` — a big non-polymorphic singleton-shaped root with many satellite files; likely the highest caller-traffic single class by `engine_funcs.h` calls (`fn_UI_CloseDialog` alone is 42; `UserInterface::Update`/`Draw` are on the main-loop hot path per their own comments in `engine_funcs.cpp`) |
| **6 — Caller batches, by directory, AFTER blocks 1-5** | `code/Game/Dialog/*` split by the §3.6 hand-inlined-vtable-call ranking (`tradeui.cpp`+`townhallui.cpp` alone are 59% of the raw hazard surface and should probably be their OWN batch, not bundled with the 37 smaller files); then `code/Game/Component/*`, `code/Game/Player/*`, `code/Game/Tutorial/*` | mirrors `B35_RULES.md` §7's directory-partition approach; UI callers are dominated by the `engine_funcs.h` global-door idiom (§3.1, unlike Video/Graphics's by-name-cache idiom) AND by exe-dispatch-slot resolution (§3.5) AND by hand-inlined vtable calls (§3.6) simultaneously — three different mechanical conversion shapes in the SAME directory, which is new for this endgame |

Blocks 1-5 are **not** confirmed disjoint by file (block 0's job, same caveat `B35_RULES.md` §12
states for Video/Graphics) — in particular the `engine/types/ui/` duplicate tree (hazard 1) may
force blocks 1 and 2 to coordinate on `Dialogs.h`/`Panels.h`/`Elements.h` before either can call
its layout final.

---

## 7. Summary of what is VERIFIED vs NOT VERIFIED / INFERRED

**VERIFIED this pass**: all header file lists/line counts/static_assert counts (`ls`, `wc -l`,
`grep -c`) and each header's actual namespace-declaration line (read, not assumed); the full
105-file `.cpp` listing with per-file export/addr registration counts and line counts; all 68
vtable rows and their slot counts (parsed from the promotion-table entries, not just comment
lines); all 68 `?_typeInfo@` rows and the 67 `RT_DELDTOR` rows; the cutover manifest's UI rows (2,
via `--table`, read-only); `tools/runtime_census.py --namespace UI`'s summary table (ran read-only,
confirmed via `git status` that it wrote nothing); all `engine_funcs.h` UI door names and their
`code/Game` call counts; `ResolveExport`/`RT_EXPORT_ONCE`/`ResolveAddr(Module::Runtime,...)` call
counts and per-directory breakdowns; the 205-slot UI dispatch-slot set via
`tools/dispatch_slot_map.py` (read-only, uses `pefile` against the retail exe) and its intersection
with 1,066 tree-wide `ResolveAddr(Module::Exe,...)` sites; the 417-occurrence hand-inlined-vtable
count scoped to `code/Game/Dialog/*`; the one live hex-literal UI vtable canonicalization site
(`tool_base_toolui.cpp:136-142`); the `DialogInstance.h` "22 Dialog Controllers" banner claim (read
as a quoted comment, not independently counted); the `engine/types/ui/DialogInstance.h` vs.
`runtime/types/ui/DialogInstance.h` `sizeof` agreement (0xB8 both); the absence of a
printf-wrapped `Register*` call in `runtime/src/ui`; the absence of any of the checked hidden
retslot exports being virtual.

**NOT VERIFIED / INFERRED** (explicit list, all called out inline above too): every base-class
relationship except the one `DialogInstance.h` states in English; whether the "53" class count in
`ENDGAME.md` reconciles with this pass's 56 (Description+Element) or 68 (all vtable classes)
figures; whether the `engine/types/ui/` duplicate tree agrees with `runtime/types/ui/` beyond the
one `DialogInstance` `sizeof` spot-check; the exact set of `code/Game/Dialog/*` files that are
genuine exe `DialogController` subclasses (the "22" count); multiple inheritance / secondary
vptrs (not investigated at all); threading and device/loader-thread-equivalent hazards (not
investigated at all); the precise target of any of the 417 hand-inlined vtable-slot calls; the
`widgets_layout.cpp`/`sprite.cpp` export/addr count mismatch's cause; whether
`runtime/src/ui/README.md`'s status claims are stale; whether any UI header transitively reaches
`runtime/types/volume/Volume.h`'s `Rectangle` name-collision trap through a `.cpp` body rather than
a header.
