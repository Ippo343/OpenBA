# B36 file ownership map — `UI` class refactor (ENDGAME B.3 step 6)

Scope: every `.cpp`/`.h`/`.inc` under `runtime/`, `standalone/`, `engine/`, `code/`, `dx11/`
(1,656 candidate files; `standalone/build*/`, `tests/unit/build/`, `.claude/worktrees/`, `docs/`
excluded) that carries a `RegisterRuntimeExport("...@UI@@...", ...)`, a
`RegisterRuntimeAddr(0x..., &fn)` whose function name follows the `Rt(Ui|UI)_Class_Method` /
`(Ui|UI)_Class_Method` convention, or an `RT_EXPORT_ONCE(Type, "...@UI@@...")` call. Method and
counting convention follow `B35_FILEMAP.md` (same regex-scan approach, same demangling rule:
class = second-to-last `@`-segment before `@@`, namespace = last segment). Script:
`scratchpad/b36/filemap/scan.py` (session scratchpad, not checked in) plus five small one-off
debug scripts in the same directory used to pin down specific counts (named inline below).
Anything not backed by a script/grep line is marked **INFERRED**.

**Class list used** (from `standalone/rt_data_mint_tables.inc`, all 68 `??_7X@UI@@6B@` rows,
VERIFIED by a direct grep+parse of that file — see B0's own count, matches `UI_INVENTORY.md`
§1.3): 28 `*Description`, 28 `*Element`, 6 Dialog-family (`Dialog`, `DialogController`,
`DialogControllerConfig`, `DialogInstance`, `ComboDialog`, `ToolTipDialog`), 6 other
(`CharacterSet`, `Cursor`, `Font`, `SpriteCapture`, `SpriteSheet`, `StringTable`). Plus the
non-polymorphic roots the task named: `UserInterface`, `SpriteContext`, `Sprite`,
`String`/`UIString` (the mangled class name is literally `String@UI@@` in every retail export —
`UI_INVENTORY.md` §5.3 and the standing memory `feedback_uistring_vs_sysstring.md` already flag
that the port's own header spells it `UIString` to avoid colliding with `System::String`; this
filemap uses the mangled spelling `String` as the table key and calls out the rename inline).
`UiStringTable`, `Event`, `DialogTimer`, `FontGlyph`, `FontLayout`, `SpriteInfo`, `CaptureEntry`,
`ClipRect` etc. never appear as their own registered class in this scan (they are by-value
members inside a polymorphic class's own registrations, same shape B35 found for Video's
`Viewport`/`DisplayMode`) — **0 file registers a body under one of those names**, VERIFIED (they
are not in the per-class table in §2 below because nothing demangles to them).

**Known scan limitation (regex, not hand-verified per line):** a body's mangled export string is
sometimes held in a named `const char*` constant defined earlier in the file
(`kSpinnerGetMaxValue`, `kMemFree`, …) rather than as an inline string literal at the call site —
this is a THIRD instance of the pattern `UI_INVENTORY.md` §1.2 already flagged for
`widgets_layout.cpp`/`sprite.cpp` (see §4 below), and it hides the export-string demangle path
entirely for those calls. Per-file **`RegisterRuntimeExport` raw call counts therefore
undercount** for `widgets_input.cpp` (8 of 26 actual calls have an inline `@UI@@` literal; the
other 18 use named constants) and `runtime/src/save/saveload_draw.cpp` (0 of ~7 `RT_EXPORT_ONCE`
calls have an inline literal — all route through named `Fn` aliases). **Per-class attribution
is NOT compromised for `widgets_input.cpp`**, because every affected body also has a
`RegisterRuntimeAddr(0x..., &RtUi_Class_Method)` sibling call whose function-name convention is
matched by the fn-name fallback — so the class table in §2 is complete for that file even though
its raw export-call count in §1 reads low. `dialog.cpp` also under-reports raw `RegisterRuntimeAddr`
calls (0 addr calls counted, 29 export) because the file registers name-only (no address stamp)
for essentially every body — a real finding, not a scan artifact (confirmed by direct read: the
file's addresses are stamped in `standalone/reg_mint_promoted.cpp`/`address_registry.cpp`
instead, consistent with `Dialog`'s WITHHELD cutover-manifest row already flagged in
`UI_INVENTORY.md` §2).

**Printf-wrapped registrations (perf-build bug shape):** zero found under `runtime/src/ui`
(`debug scripts/scan.py`'s multiline check for `printf`/`TRACE`/`fprintf`/`sprintf`/`LOG*`
wrapping a `RegisterRuntime*` call — VERIFIED, 0 hits), matching `UI_INVENTORY.md` §5 hazard 6.
See §7 below for the full answer including which 3 plain `printf` calls in the directory are
merely post-registration summary prints.

---

## 1. Per-file table (files with ≥1 UI registration)

100 files carry at least one matched UI registration call. 96 are `runtime/src/ui/*.cpp`,
1 is `runtime/src/object/deleting_dtor_leaves.cpp`, 2 are `runtime/src/save/{saveload,
saveload_draw}.cpp`, 1 is `standalone/reg_mint_promoted.cpp`. Column "classes (count)" is a
DISTINCT-FUNCTION-SYMBOL count (a body's export call and its addr call, when both exist, are
deduplicated by function name — same convention `B35_FILEMAP.md` used, so this table's counts
are directly comparable to it).

### `runtime/src/ui/` — the 27 leaf `Description`/`Element` pairs + `Element`/`ElementDescription`

| File | Lines | Export | Addr | Classes (count) |
|---|---:|---:|---:|---|
| element.cpp | 1351 | 23 | 24 | Element (23), ContainerElement (1 — cross-cutting, see §3) |
| element_vtable.cpp | 881 | 8 | 8 | Element (8) |
| elementdescription_serialize.cpp | 238 | 0 | 1 | ElementDescription (1) |
| element_bodies_b5.cpp | 1024 | 11 | 11 | EditElement, LayoutElement, RibbonElement, ScrollElement, SliderElement, TextDescription, TextElement (1 each), ListElement (2), TableElement (2) — **name is misleading**: despite "element_bodies" this file holds ZERO `Element`/`ElementDescription` base-class members; every body is a leaf-pair class (see §5 correction) |
| animatedimagedescription.cpp / animatedimageelement.cpp | 339/224 | 9/5 | 9/5 | AnimatedImageDescription (9) / AnimatedImageElement (5) |
| borderdescription.cpp / borderelement.cpp | 254/261 | 9/7 | 9/7 | BorderDescription (9) / BorderElement (7) |
| buttondescription.cpp / buttonelement.cpp | 272/419 | 9/10 | 10/10 | ButtonDescription (10) / ButtonElement (10) |
| checkdescription.cpp / checkelement.cpp | 221/236 | 9/9 | 9/9 | CheckDescription (9) / CheckElement (9) |
| clipregiondescription.cpp / clipregionelement.cpp | 237/333 | 9/10 | 9/10 | ClipRegionDescription (9) / ClipRegionElement (10) |
| combodescription.cpp / comboelement.cpp | 303/343 | 9/5 | 9/5 | ComboDescription (9) / ComboElement (5) |
| containerdescription.cpp / containerelement.cpp | 296/463 | 9/9 | 9/9 | ContainerDescription (9) / ContainerElement (9) |
| documenttextdescription.cpp / documenttextelement.cpp | 425/563 | 9/8 | 10/8 | DocumentTextDescription (10) / DocumentTextElement (8) |
| documenttext.cpp | 1091 | 6 | 7 | DocumentTextElement (7) — split from documenttextelement.cpp, no overlap by fn name |
| editdescription.cpp / editelement.cpp | 222/612 | 9/9 | 9/9 | EditDescription (9) / EditElement (9) |
| graphdescription.cpp / graphelement.cpp | 194/271 | 9/7 | 9/7 | GraphDescription (9) / GraphElement (7) |
| groupdescription.cpp / groupelement.cpp | 431/210 | 9/8 | 9/8 | GroupDescription (9) / GroupElement (8) |
| imagedescription.cpp / imageelement.cpp | 392/226 | 9/7 | 10/7 | ImageDescription (10) / ImageElement (7) |
| labeldescription.cpp / labelelement.cpp | 241/428 | 9/8 | 9/8 | LabelDescription (9) / LabelElement (8) |
| layoutdescription.cpp / layoutelement.cpp | 574/302 | 9/7 | 10/7 | LayoutDescription (10) / LayoutElement (7) |
| listdescription.cpp / listelement.cpp | 383/314 | 10/8 | 10/8 | ListDescription (10) / ListElement (8) |
| numberdescription.cpp / numberelement.cpp | 301/248 | 9/8 | 9/8 | NumberDescription (9) / NumberElement (8) |
| numbereditdescription.cpp / numbereditelement.cpp | 402/128 | 10/5 | 10/5 | NumberEditDescription (10) / NumberEditElement (5) |
| pagedescription.cpp / pageelement.cpp | 310/94 | 10/4 | 10/4 | PageDescription (10) / PageElement (4) |
| progressdescription.cpp / progresselement.cpp | 340/310 | 10/7 | 10/7 | ProgressDescription (10) / ProgressElement (7) |
| radiodescription.cpp / radioelement.cpp | 342/187 | 10/5 | 10/5 | RadioDescription (10) / RadioElement (5) |
| ribbondescription.cpp / ribbonelement.cpp | 377/269 | 9/7 | 9/7 | RibbonDescription (9) / RibbonElement (7) |
| scrolldescription.cpp / scrollelement.cpp | 318/419 | 10/8 | 10/8 | ScrollDescription (10) / ScrollElement (8) |
| sliderdescription.cpp / sliderelement.cpp | 374/409 | 10/8 | 10/8 | SliderDescription (10) / SliderElement (8) |
| spinnerdescription.cpp / spinnerelement.cpp | 300/321 | 10/8 | 10/8 | SpinnerDescription (10) / SpinnerElement (8) |
| tabdescription.cpp / tabelement.cpp | 410/373 | 10/9 | 10/9 | TabDescription (10) / TabElement (9) |
| tabelement_addtab.cpp | 282 | 1 | 1 | TabElement (1) |
| tabledescription.cpp / tableelement.cpp | 406/256 | 10/7 | 10/7 | TableDescription (10) / TableElement (7) |
| textdescription.cpp / textelement.cpp | 390/230 | 10/6 | 10/6 | TextDescription (10) / TextElement (6) |
| textelement_cachetext.cpp | 306 | 2 | 2 | TextElement (2) |
| widgets_input.cpp | 1613 | 8 (raw; **18 more via named constants**, see limitation note) | 26 | ComboElement (7), SpinnerElement (7), NumberEditElement (3), ProgressElement (3), RadioElement (3), NumberElement (2), EditElement (1), SliderElement (1) — **all leaf-pair classes, ZERO `UserInterface`** (correction to `UI_INVENTORY.md`'s file-name grouping guess, see §5) |
| widgets_layout.cpp | 943 | 23 | 0 | TableElement (5), ImageElement (4), DocumentTextElement (3), GraphElement (3), LabelElement (2), PageElement (2), TextElement (2), TabElement (1), Element (1 — cross-cutting) — see §4 for the export/addr mismatch explanation |
| ui_factory_wave28.cpp | 1481 | 79 | 0 | 34 classes touched, 2-3 regs each (see §2); wave/factory entry point, cross-cutting by design |

### `runtime/src/ui/` — Dialog family

| File | Lines | Export | Addr | Classes (count) |
|---|---:|---:|---:|---|
| dialog.cpp | 2330 | 29 | 0 | DialogInstance (19), ElementDescription (6 — cross-cutting, see §3), Dialog (2), DialogController (2) |
| dialog_controller_base.cpp | 323 | 9 | 0 | DialogController (9) |
| dialog_create.cpp | 818 | 4 | 0 | DialogInstance (4) |
| dialog_ctor_dtor.cpp | 436 | 2 | 0 | Dialog (2) |
| dialog_factory.cpp | 273 | 2 | 0 | Dialog (2) |
| dialog_settings.cpp | 477 | 3 | 3 | DialogInstance (3) |
| dialog_vtable.cpp | 744 | 8 | 0 | Dialog (8) |
| dialogcontrollerconfig.cpp | 92 | 3 | 3 | DialogControllerConfig (3) |
| combodialog.cpp | 291 | 6 | 6 | ComboDialog (6) |
| combodialog_ctor.cpp | 787 | 3 | 3 | ComboDialog (2), DialogInstance (1 — cross-cutting) |
| tooltipdialog.cpp | 481 | 5 | 5 | ToolTipDialog (4), Element (1 — cross-cutting, see §3) |
| spritesheet_new.cpp | 272 | 5 | 5 | SpriteSheet (3), DialogInstance (1), ToolTipDialog (1) — cross-cutting operator-delete hub, see §3 |

### `runtime/src/ui/` — Sprite/Font/text family

| File | Lines | Export | Addr | Classes (count) |
|---|---:|---:|---:|---|
| sprite.cpp | 760 | 23 (raw) | 2 | Sprite (6), SpriteSheet (8), String/UIString (5), StringTable (4) — see §4 |
| spritesheet_vtable.cpp | 645 | 8 | 10 | SpriteSheet (10) |
| spritecapture.cpp | 479 | 3 | 3 | SpriteCapture (3) |
| spritecapture_class.cpp | 519 | 9 | 10 | SpriteCapture (10) |
| spritecontext.cpp | 602 | 3 | 3 | SpriteContext (6) |
| spritecontext_add.cpp | 488 | 0 | 4 | SpriteContext (4) |
| spritecontext_flush.cpp | 576 | 0 | 1 | SpriteContext (1) |
| stringtable_factory.cpp | 760 | 10 | 0 | StringTable (10) |
| stringtable_lifecycle.cpp | 235 | 2 | 0 | StringTable (2) |
| font.cpp | 932 | 12 | 12 | Font (12) |
| characterset.cpp | 231 | 8 | 8 | CharacterSet (8) |
| cursor.cpp | 218 | 8 | 8 | Cursor (8) |

### `runtime/src/ui/` — `UserInterface` + widgets

| File | Lines | Export | Addr | Classes (count) |
|---|---:|---:|---:|---|
| userinterface.cpp | 1623 | 18 | 18 | UserInterface (18) |
| userinterface_close.cpp | 571 | 0 | 1 | UserInterface (1) |
| userinterface_dragstart.cpp | 255 | 1 | 1 | UserInterface (1) |
| userinterface_draw.cpp | 714 | 1 | 1 | UserInterface (1) |
| userinterface_frame.cpp | 557 | 1 | 1 | UserInterface (1) |
| userinterface_hover.cpp | 489 | 4 | 4 | UserInterface (3), DialogInstance (1 — cross-cutting) |
| userinterface_showdialog.cpp | 433 | 1 | 1 | UserInterface (1) |
| userinterface_standalone.cpp | 191 | 2 | 2 | UserInterface (2) |

**`widgets_input.cpp` and `widgets_layout.cpp` do NOT belong in this family** despite their name
suggesting otherwise — see §5.

### Files elsewhere that register a `@UI@@` body

| File | Registers | Note |
|---|---|---|
| `runtime/src/object/deleting_dtor_leaves.cpp` | `ToolTipDialog::~ToolTipDialog` (1 row) | Shared deleting-dtor leaf table — **OWNER**, one row happens to be UI's |
| `standalone/reg_mint_promoted.cpp` | `ToolTipDialog` (1 attributable row of 13 raw `RegisterRuntimeExport` calls in the file — the other 12 are non-UI promoted-mint rows) | Mint-promotion machinery — **OWNER**, not a UI class-body file |
| `runtime/src/save/saveload.cpp` | none (1 raw export call references `Sprite@UI@@` only as a PARAMETER type of `Save::SaveLoad::SetSaveSprite`) | Same "parameter type, not the registered class" pattern B35 found for `Video::Context` — **OWNER**, excluded |
| `runtime/src/save/saveload_draw.cpp` | none as owner; 7 `RT_EXPORT_ONCE` CALLER sites resolving `SpriteSheet`/`SpriteContext`/`SpriteCapture` exports by name | This is exactly the caller `sprite.cpp:708`'s own comment names ("Reached by name from runtime/src/save/saveload_draw.cpp") — a cross-namespace CALLER, not a body owner — **OWNER/caller, excluded** |
| `standalone/reg_element_typeids.cpp`, `standalone/reg_unreg_lane2.cpp`, `standalone/address_registry.cpp` | 0 / 0 / 1 `RegisterRuntime*` calls matched (the "1" is address-registry data, not a body); but 22 / 9 / 17 raw `@UI@@` mangled-string mentions each (VERIFIED grep) | Address-table / type-id / dispatch-slot machinery that references UI mangled names and vtable addresses as DATA — **OWNER**, same category B35 gave `standalone/rt_data_mint_tables.inc` etc. |
| `runtime/src/ui/factory_ui1.cpp` (+`.inc`), `runtime/src/ui/rt_ui_port.cpp` | 0 matched (calls use a table-driven variable `sp.newName`/`sp.ipName`/`sp.ctorName`, not literals) | Wave/factory table-driven entry point and the namespace dispatcher — **OWNER**, same convention as `rt_video_port.cpp`/`rt_factories_video.cpp` in B35 |
| `runtime/src/ui/description_dtors.cpp`, `element_sprite_debug.cpp`, `sprite_debug.cpp`, `ui_operator_delete.cpp`, `element_common.h`, `widget_common.h` | 0 matched | Zero-registration helper/debug/shared-header files (matches `UI_INVENTORY.md` §1.2's own zero-count list, plus the two headers) |

---

## 2. Per-class inverse table

All 68 polymorphic classes plus the 4 non-polymorphic roots the task named that actually own a
registered body (`UserInterface`, `SpriteContext`, `Sprite`, `String`/`UIString`) — 72 rows.
**Bold** = split across >1 file. Total = distinct-function-symbol count summed across every file
in §1 (dedup by fn name within each file, not globally — a fn shared verbatim by two files would
double count, but no such case was found).

| Class | Total | Files (count each) |
|---|---:|---|
| **Element** | 33 | element.cpp (23), element_vtable.cpp (8), tooltipdialog.cpp (1), widgets_layout.cpp (1) |
| **DialogInstance** | 29 | dialog.cpp (19), dialog_create.cpp (4), dialog_settings.cpp (3), combodialog_ctor.cpp (1), spritesheet_new.cpp (1), userinterface_hover.cpp (1) |
| **UserInterface** | 28 | userinterface.cpp (18), userinterface_hover.cpp (3), userinterface_standalone.cpp (2), userinterface_close.cpp (1), userinterface_dragstart.cpp (1), userinterface_draw.cpp (1), userinterface_frame.cpp (1), userinterface_showdialog.cpp (1) |
| **SpriteSheet** | 23 | spritesheet_vtable.cpp (10), sprite.cpp (8), spritesheet_new.cpp (3), ui_factory_wave28.cpp (2) |
| **DocumentTextElement** | 18 | documenttextelement.cpp (8), documenttext.cpp (7), widgets_layout.cpp (3) |
| **SpinnerElement** | 17 | spinnerelement.cpp (8), widgets_input.cpp (7), ui_factory_wave28.cpp (2) |
| **TableElement** | 16 | tableelement.cpp (7), widgets_layout.cpp (5), element_bodies_b5.cpp (2), ui_factory_wave28.cpp (2) |
| **StringTable** | 16 | stringtable_factory.cpp (10), sprite.cpp (4), stringtable_lifecycle.cpp (2) |
| **SpriteCapture** | 16 | spritecapture_class.cpp (10), spritecapture.cpp (3), ui_factory_wave28.cpp (3) |
| **Dialog** | 14 | dialog_vtable.cpp (8), dialog.cpp (2), dialog_ctor_dtor.cpp (2), dialog_factory.cpp (2) |
| **ImageElement** | 14 | imageelement.cpp (7), widgets_layout.cpp (4), ui_factory_wave28.cpp (3) |
| **TextDescription** | 13 | textdescription.cpp (10), ui_factory_wave28.cpp (2), element_bodies_b5.cpp (1) |
| **TextElement** | 13 | textelement.cpp (6), textelement_cachetext.cpp (2), ui_factory_wave28.cpp (2), widgets_layout.cpp (2), element_bodies_b5.cpp (1) |
| **ImageDescription** | 13 | imagedescription.cpp (10), ui_factory_wave28.cpp (3) |
| **LabelElement** | 13 | labelelement.cpp (8), ui_factory_wave28.cpp (3), widgets_layout.cpp (2) |
| **LayoutDescription** | 13 | layoutdescription.cpp (10), ui_factory_wave28.cpp (3) |
| **TabElement** | 13 | tabelement.cpp (9), ui_factory_wave28.cpp (2), tabelement_addtab.cpp (1), widgets_layout.cpp (1) |
| **ComboElement** | 12 | widgets_input.cpp (7), comboelement.cpp (5) |
| **ListElement** | 12 | listelement.cpp (8), element_bodies_b5.cpp (2), ui_factory_wave28.cpp (2) |
| **SliderElement** | 12 | sliderelement.cpp (8), ui_factory_wave28.cpp (2), element_bodies_b5.cpp (1), widgets_input.cpp (1) |
| Font | 12 | font.cpp (12) |
| **LabelDescription** | 12 | labeldescription.cpp (9), ui_factory_wave28.cpp (3) |
| **ListDescription** | 12 | listdescription.cpp (10), ui_factory_wave28.cpp (2) |
| **NumberDescription** | 12 | numberdescription.cpp (9), ui_factory_wave28.cpp (3) |
| **NumberEditDescription** | 12 | numbereditdescription.cpp (10), ui_factory_wave28.cpp (2) |
| **NumberElement** | 12 | numberelement.cpp (8), ui_factory_wave28.cpp (2), widgets_input.cpp (2) |
| **PageDescription** | 12 | pagedescription.cpp (10), ui_factory_wave28.cpp (2) |
| **ProgressDescription** | 12 | progressdescription.cpp (10), ui_factory_wave28.cpp (2) |
| **ProgressElement** | 12 | progresselement.cpp (7), widgets_input.cpp (3), ui_factory_wave28.cpp (2) |
| **RadioDescription** | 12 | radiodescription.cpp (10), ui_factory_wave28.cpp (2) |
| **RibbonDescription** | 12 | ribbondescription.cpp (9), ui_factory_wave28.cpp (3) |
| **ScrollDescription** | 12 | scrolldescription.cpp (10), ui_factory_wave28.cpp (2) |
| **SliderDescription** | 12 | sliderdescription.cpp (10), ui_factory_wave28.cpp (2) |
| **SpinnerDescription** | 12 | spinnerdescription.cpp (10), ui_factory_wave28.cpp (2) |
| **TabDescription** | 12 | tabdescription.cpp (10), ui_factory_wave28.cpp (2) |
| **TableDescription** | 12 | tabledescription.cpp (10), ui_factory_wave28.cpp (2) |
| **DialogController** | 11 | dialog_controller_base.cpp (9), dialog.cpp (2) |
| **EditElement** | 11 | editelement.cpp (9), element_bodies_b5.cpp (1), widgets_input.cpp (1) |
| **ScrollElement** | 11 | scrollelement.cpp (8), ui_factory_wave28.cpp (2), element_bodies_b5.cpp (1) |
| **SpriteContext** | 11 | spritecontext.cpp (6), spritecontext_add.cpp (4), spritecontext_flush.cpp (1) |
| **ToolTipDialog** | 10 | tooltipdialog.cpp (4), ui_factory_wave28.cpp (3), deleting_dtor_leaves.cpp (1), spritesheet_new.cpp (1), reg_mint_promoted.cpp (1) |
| ButtonDescription | 10 | buttondescription.cpp (10) |
| ButtonElement | 10 | buttonelement.cpp (10) |
| ClipRegionElement | 10 | clipregionelement.cpp (10) |
| **ContainerElement** | 10 | containerelement.cpp (9), element.cpp (1) |
| DocumentTextDescription | 10 | documenttextdescription.cpp (10) |
| **LayoutElement** | 10 | layoutelement.cpp (7), ui_factory_wave28.cpp (2), element_bodies_b5.cpp (1) |
| **RibbonElement** | 10 | ribbonelement.cpp (7), ui_factory_wave28.cpp (2), element_bodies_b5.cpp (1) |
| **GraphElement** | 10 | graphelement.cpp (7), widgets_layout.cpp (3) |
| **NumberEditElement** | 10 | numbereditelement.cpp (5), widgets_input.cpp (3), ui_factory_wave28.cpp (2) |
| **RadioElement** | 10 | radioelement.cpp (5), widgets_input.cpp (3), ui_factory_wave28.cpp (2) |
| AnimatedImageDescription | 9 | animatedimagedescription.cpp (9) |
| BorderDescription | 9 | borderdescription.cpp (9) |
| CheckDescription | 9 | checkdescription.cpp (9) |
| CheckElement | 9 | checkelement.cpp (9) |
| ClipRegionDescription | 9 | clipregiondescription.cpp (9) |
| ComboDescription | 9 | combodescription.cpp (9) |
| ContainerDescription | 9 | containerdescription.cpp (9) |
| EditDescription | 9 | editdescription.cpp (9) |
| GraphDescription | 9 | graphdescription.cpp (9) |
| GroupDescription | 9 | groupdescription.cpp (9) |
| CharacterSet | 8 | characterset.cpp (8) |
| **ComboDialog** | 8 | combodialog.cpp (6), combodialog_ctor.cpp (2) |
| Cursor | 8 | cursor.cpp (8) |
| GroupElement | 8 | groupelement.cpp (8) |
| **PageElement** | 8 | pageelement.cpp (4), ui_factory_wave28.cpp (2), widgets_layout.cpp (2) |
| BorderElement | 7 | borderelement.cpp (7) |
| **ElementDescription** | 7 | dialog.cpp (6), elementdescription_serialize.cpp (1) |
| Sprite | 6 | sprite.cpp (6) |
| AnimatedImageElement | 5 | animatedimageelement.cpp (5) |
| String (ported as `UIString`) | 5 | sprite.cpp (5) |
| DialogControllerConfig | 3 | dialogcontrollerconfig.cpp (3) |

All 68 mint-table classes appear at least once. **44 of 72 rows are bold** (span >1 file);
28 are single-file (mostly the simplest leaf pairs: `ButtonDescription`, `ButtonElement`,
`ClipRegionElement`, `DocumentTextDescription`, `AnimatedImageDescription`, `BorderDescription`,
`CheckDescription`, `CheckElement`, `ClipRegionDescription`, `ComboDescription`,
`ContainerDescription`, `EditDescription`, `GraphDescription`, `GroupDescription`,
`CharacterSet`, `Cursor`, `GroupElement`, `BorderElement`, `Sprite`, `AnimatedImageElement`,
`String`, `DialogControllerConfig`, plus `Font`).

---

## 3. Cross-cutting files (touch classes of more than one proposed block)

Blocks per `NEXT_SESSION.md` §2c: **B1** = `Element`+`ElementDescription`; **B2** = Dialog family
(`Dialog`, `DialogController`, `DialogControllerConfig`, `DialogInstance`, `ComboDialog`,
`ToolTipDialog`); **B3** = the 27 leaf Description/Element pairs; **B4** = Sprite/Font/text
family (`Sprite`, `SpriteSheet`, `SpriteCapture`, `SpriteContext`, `Font`, `CharacterSet`,
`StringTable`, `String`/`UIString`, `Cursor`); **B5** = `UserInterface` (the widgets files are
**removed** from B5, see §5).

Sorting every file with ≥1 matched class into exactly one block by that rule, **93 of the 100
hit files are already disjoint** — only **7 files are cross-cutting**:

| File | Classes touched | Blocks | Note |
|---|---|---|---|
| `dialog.cpp` | Dialog, DialogController, DialogInstance (B2) + ElementDescription (B1) | B1+B2 | The `ElementDescription` fragment (6 regs) needs a manual split before B1/B2 can each own a disjoint file — same shape B35 found for `constant_buffer.cpp`'s stray `Context::AssignConstants` |
| `element.cpp` | Element (B1) + ContainerElement (B3, 1 reg) | B1+B3 | One stray `ContainerElement` body registered in the base-class file |
| `tooltipdialog.cpp` | ToolTipDialog (B2) + Element (B1, 1 reg) | B1+B2 | Symmetric stray in the other direction |
| `spritesheet_new.cpp` | SpriteSheet (B4) + DialogInstance, ToolTipDialog (B2) | B2+B4 | Cross-namespace `operator delete`/`operator new` hub — the exact `closure_batch2b.cpp` shape B35 found for Video/Graphics; registers 3 unrelated classes' allocation ops in one file |
| `userinterface_hover.cpp` | UserInterface (B5) + DialogInstance (B2, 1 reg) | B2+B5 | One stray `DialogInstance` body (hover state needs to know about the active dialog) |
| `widgets_layout.cpp` | 7 B3 leaf classes + Element (B1, 1 reg) | B1+B3 | See §4/§5 |
| `ui_factory_wave28.cpp` | 32 B3 leaf classes + SpriteSheet, SpriteCapture (B4) + ToolTipDialog (B2) | B2+B3+B4 | The wave/factory entry point — historically grabbed whatever bodies were reachable that session, exactly the CLAUDE.md "no more lanes" shape; a THREE-way straddler, the widest cross-cutting file in the namespace |

**`widgets_input.cpp` is NOT cross-cutting** — all 8 classes it touches (`ComboElement`,
`EditElement`, `NumberEditElement`, `NumberElement`, `ProgressElement`, `RadioElement`,
`SliderElement`, `SpinnerElement`) are B3 leaf classes; it belongs wholly inside B3 (correcting
`UI_INVENTORY.md`'s file-name-based guess that grouped it with `UserInterface`, see §5).

**Net disjoint partition:**
- **B1** (2 owned files + fragments in `dialog.cpp`, `element.cpp`, `tooltipdialog.cpp`,
  `widgets_layout.cpp`): `element_vtable.cpp`, `elementdescription_serialize.cpp`.
- **B2** (11 owned files + fragments in `dialog.cpp`, `spritesheet_new.cpp`,
  `userinterface_hover.cpp`, `ui_factory_wave28.cpp`): `combodialog.cpp`, `combodialog_ctor.cpp`,
  `dialog_controller_base.cpp`, `dialog_create.cpp`, `dialog_ctor_dtor.cpp`, `dialog_factory.cpp`,
  `dialog_settings.cpp`, `dialog_vtable.cpp`, `dialogcontrollerconfig.cpp`, `tooltipdialog.cpp`
  (itself also carries the 1-reg `Element` stray), `runtime/src/object/deleting_dtor_leaves.cpp`,
  `standalone/reg_mint_promoted.cpp`.
- **B3** (59 owned files + fragments in `element.cpp`, `widgets_layout.cpp`,
  `ui_factory_wave28.cpp`): all 27 leaf-pair `.cpp` files, `documenttext.cpp`,
  `element_bodies_b5.cpp`, `tabelement_addtab.cpp`, `textelement_cachetext.cpp`,
  `widgets_input.cpp` (in full).
- **B4** (12 owned files + fragments in `spritesheet_new.cpp`, `ui_factory_wave28.cpp`):
  `sprite.cpp`, `spritesheet_vtable.cpp`, `spritecapture.cpp`, `spritecapture_class.cpp`,
  `spritecontext.cpp`, `spritecontext_add.cpp`, `spritecontext_flush.cpp`,
  `stringtable_factory.cpp`, `stringtable_lifecycle.cpp`, `font.cpp`, `characterset.cpp`,
  `cursor.cpp`.
- **B5** (7 owned files + fragment in `userinterface_hover.cpp`): `userinterface.cpp` +
  6 `userinterface_*.cpp` files.
- **OWNER / cross-cutting-only**: `dialog.cpp`, `spritesheet_new.cpp`, `ui_factory_wave28.cpp`
  need a coordinator fragment pass before B1-B5 agents can each start on a clean tree (same
  process B35 §12 used); `standalone/reg_element_typeids.cpp`, `reg_unreg_lane2.cpp`,
  `address_registry.cpp`, `runtime/src/ui/factory_ui1.cpp`(+`.inc`), `rt_ui_port.cpp`,
  `description_dtors.cpp`, `element_sprite_debug.cpp`, `sprite_debug.cpp`, `ui_operator_delete.cpp`,
  `element_common.h`, `widget_common.h`, `runtime/src/save/{saveload,saveload_draw}.cpp`.

This is a **much cleaner starting position than B35's Video/Graphics split** (which needed 4
named fragments plus 3 line-split wave files before any family was disjoint) — UI has only 3
files needing a real fragment pass (`dialog.cpp`, `spritesheet_new.cpp`, `ui_factory_wave28.cpp`)
and 4 more with a single 1-reg stray each that a one-line move settles.

---

## 4. The `widgets_layout.cpp` / `sprite.cpp` export/addr mismatch

`UI_INVENTORY.md` §1.2 flagged both as NOT VERIFIED. Both are now read and the cause is the
same, plain shape in both files:

- **`widgets_layout.cpp`** (23 export vs 3 addr, per raw grep): reading lines 908-938 directly
  shows most bodies (e.g. `PageElement::GetActivePage`, `GraphElement::RemoveGraphs`) are
  registered **name-only** — a single `RegisterRuntimeExport` call, `++n`, done. Only 3 bodies
  (`Element::RemoveChildren`, `GraphElement::AddGraphPoint`, `GraphElement::AddGraph`) ALSO get a
  `RegisterRuntimeAddr` call. The file's own comment at line 913-924 explains why: an audit tool
  (`tools/port_bypass_audit.py`) found `Element::RemoveChildren` was reachable only by name, and
  a by-address caller (`??1Element`'s dtor in `element.cpp`) later bypassed the name-only door and
  fell through to the DLL — the "register TWICE, name AND address" convention exists specifically
  to close that bypass class. The other 20 bodies in this file have no known by-address caller
  yet, so they were never given the second registration. **This is not a scan bug — it is a real,
  intentional (if incomplete) convention**: address registration is added reactively, per body,
  only once a by-address call site is found.
- **`sprite.cpp`** (24 export vs 2 addr): same shape. `sprite.cpp:708`'s own comment says
  `SpriteSheet::Set` is "Reached by name from `runtime/src/save/saveload_draw.cpp`" — confirmed,
  `saveload_draw.cpp` calls it (and 6 other Sprite-family bodies) via `RT_EXPORT_ONCE` on the
  mangled name, never by address. Only `SpriteSheet::Set` and
  `SpriteSheet::GetSpriteNameFromIndex` have a `RegisterRuntimeAddr` row.

**A third instance of the same shape, not previously flagged**: `widgets_input.cpp` (8 raw
export calls vs 26 addr calls — the INVERSE ratio of the other two). Reading it shows the
opposite mechanism: most of its `RegisterRuntimeExport` calls pass a **named `const char*`
constant** (`kSpinnerGetMaxValue`, `kComboGetCurrentItem`, `kNumEditGetValue`, …, defined earlier
in the file) rather than an inline string literal, so a literal-string regex undercounts the
export side even though every body IS in fact double-registered (its sibling
`RegisterRuntimeAddr(0x..., &RtUi_Class_Method)` call always exists and always uses a real
address). This is a scan-visibility artifact, not a bypass risk — unlike the other two files'
gap, which is a genuine incomplete-registration state.

---

## 5. Correction to `NEXT_SESSION.md`/`UI_INVENTORY.md`'s widgets grouping guess

Both source documents grouped `widgets_input.cpp` and `widgets_layout.cpp` with
`UserInterface` ("the closest UI analogue of Video's `Device`/`Context`... `UserInterface` (the
singleton) + widgets"). **Reading both files' actual registrations shows this is wrong**: neither
file registers a single `UserInterface` body.

- `widgets_input.cpp`: 8 classes, all B3 leaf-pair `*Element` classes (input-widget behaviour:
  `ComboElement`, `EditElement`, `NumberEditElement`, `NumberElement`, `ProgressElement`,
  `RadioElement`, `SliderElement`, `SpinnerElement`).
- `widgets_layout.cpp`: 7 B3 leaf-pair classes (`DocumentTextElement`, `GraphElement`,
  `ImageElement`, `LabelElement`, `PageElement`, `TabElement`, `TableElement`) plus a single
  stray `Element` (B1) body.

Both files belong in **B3** (the leaf-pair block), not B5. The word "widgets" in their names
refers to leaf UI *widget* classes' shared layout/input-handling code, not to `UserInterface`
itself. A step-6 planner should drop the B5 "+widgets" framing and fold both files into B3's
file set, which also removes B5's only originally-claimed cross-cutting risk (leaving only the
1-reg `DialogInstance` stray in `userinterface_hover.cpp`, §3).

Also worth correcting: `element_bodies_b5.cpp`'s name suggests it holds `Element`/
`ElementDescription` base-class bodies (the same misreading risk `widgets_*.cpp`'s names
created). It holds **zero** — every body is a B3 leaf-pair class (`EditElement`, `LayoutElement`,
`ListElement`, `RibbonElement`, `ScrollElement`, `SliderElement`, `TableElement`,
`TextDescription`, `TextElement`). The "b5" in the name is a historical wave/batch label
(`B34_RULES`/`B35_RULES`'s numbering), not a reference to Block 1's `Element`.

---

## 6. The missing `RT_DELDTOR` row (67 of 68)

`UI_INVENTORY.md` §1.3 flagged 67 `RT_DELDTOR` rows for 68 vtable classes, unreconciled. Diffing
the 68-name mint-table list against every `RT_DELDTOR(..., "UI::ClassName")` row in
`runtime/src/object/deleting_dtor.cpp` (VERIFIED grep+diff; `deleting_dtor_leaves.cpp` has 0
`RT_DELDTOR(` rows of its own — despite the name, it holds one-off leaf dtor bodies registered
directly, not table rows) gives exactly one class absent from the table: **`StringTable`**.

Its slot-0 (`DeletingDtor`) body lives instead in `runtime/src/ui/stringtable_factory.cpp`
(function `RtUiStringTable_ScalarDelete`, ~line 718, registered at lines 742/750) as an inline
special case, NOT a `deleting_dtor.cpp` table row. The file's own banner comment (lines 70-113)
explains why: `StringTable::operator delete` (0x18011d8f0 in retail) was found, by an
instruction-for-instruction diff, to be BYTE-IDENTICAL to
`System::MemoryAllocator::operator delete`/`MemFree` (0x1800c7ad0) — "not similar, the same
shape... every instruction, every displacement and every immediate agrees." Rather than add a
68th generic table row that just re-implements the free, the file registers the class's slot-0
body as a one-line delegation to the allocator singleton directly, under two doors
(`ResolveAddr(Module::Runtime, 0x18011d8f0)` for the dtor and
`RT_EXPORT_ONCE(FreeFn, "??3StringTable@UI@@SAXPEAX@Z")` for the scalar-delete export) — both
registry-aware, making slot 0 self-contained at cutover without ever touching
`deleting_dtor.cpp`. The comment is explicit that this is a documented, deliberate exception
(the class's own MSVC-emitted `operator delete` happened to be a compiler-deduped alias of the
global free), not an oversight.

---

## 7. Printf-wrapped registrations under `runtime/src/ui`

Zero, confirmed by a multiline regex (`scan.py`'s helper, matching
`(printf|TRACE|fprintf|sprintf|LOG\w*)\s*\([^;]*RegisterRuntime` with `re.S` across every file in
the directory — VERIFIED, 0 hits, same conclusion `UI_INVENTORY.md` §5 hazard 6 already reached
by a looser single-pattern grep). The three plain `printf(...)` calls that DO exist in the
directory (`factory_ui1.cpp:356`, `rt_ui_port.cpp:348`, `widgets_layout.cpp:940`) were each read
directly and are all post-registration summary prints of the form
`printf("[RtPort] UI widgets(layout): %d exports registered\n", n);` — the counter `n` is
accumulated by ordinary `++n`/`++count` statements next to (not inside) each
`RegisterRuntimeExport`/`RegisterRuntimeAddr` call, so none of them exhibit the
perf-build-strips-printf-arguments bug shape (`reference_perf_build_strips_printf_arguments.md`)
that bit Video/Graphics 7 times in step 5.

---

## Method note / what to re-verify before trusting a count

Every number above comes from `scratchpad/b36/filemap/scan.py` (a from-scratch rewrite of B35's
scan script, same conventions) plus five small ad-hoc scripts in the same directory used to
chase down specific gaps (`debug_once.py`, `debug_export_gap.py`) — none of those helper scripts
are checked in. The scan required two regex fixes beyond B35's original approach, both
UI-specific discoveries worth carrying into any future namespace's filemap:
1. **Concatenated string literals**: a mangled export string is often split across 2-3 adjacent
   C++ string literals on separate source lines (`"??0DialogInstance@UI@@..." "V?$InternalPtr@...@Z"`),
   which a single-`"[^"]*"` regex does not join. Fixed by matching a RUN of adjacent literals and
   concatenating before checking for `@UI@@`.
2. **Named-constant indirection**: `RT_EXPORT_ONCE`'s mangled-string argument and (less often)
   `RegisterRuntimeExport`'s are sometimes a named `const char*`/`constexpr` alias defined earlier
   in the file rather than an inline literal (§4's `widgets_input.cpp` finding) — this is
   invisible to ANY string-literal regex and was only caught by cross-checking raw call counts
   against per-class attribution via the `RegisterRuntimeAddr` sibling call's function-name
   convention. A per-file raw call count that looks low relative to its own file's apparent size
   (as `widgets_input.cpp`'s 8 export / 26 addr does) is the tell to re-read the file directly
   rather than trust the count.

Given the volume (100 files, ~1,480 attributed registrations), individual single-digit
per-file/per-class counts were NOT all re-verified by hand the way `renderer.cpp`'s
`MaterialInstance` count was in B35 — treat this filemap the way B35_FILEMAP.md asked to be
treated: a structural map with named methods, not a byte-verified inventory. The retail-bytes
question pass (`NEXT_SESSION.md` §2b, Block 0) still owns verifying slot lists, `sizeof`, and
`_ParentTypeName` from the DLL directly.
