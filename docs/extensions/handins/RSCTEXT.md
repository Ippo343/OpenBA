# RSCTEXT hand-in (2026-10-18)

KITFREE block RSCTEXT (docs/extensions/KITFREE.md section 3): `Dialog/Extensions.rsc` (every named
object), `Dialog/EnhancedOptions.rsc` and `Dialog/ExtensionsStringTable.rsc` are built inside the exe
from the embedded text of those three files. With no loose `Dialog/Extensions_*.crs`,
`ExtensionsStringTable_*.crs` or `EnhancedOptions_*.crs` in the game directory the menu shows the
Extensions button, the dialog opens with its 21 rows and the Options dialog gets the Enhanced tab,
all from provided objects. Branch `worktree-agent-a8c225d6358726016`.

## 1. What was built

| Piece | Where |
|---|---|
| The provider table: `Register` / `Provide` / `SetPath` / `Publish` per `Resources.h` | `src/ext/resources/resources.cpp` |
| The one engine seam | `src/engine/object/external_compiled.{h,cpp}`: `RtObject::g_extProvideSeam`, a null-by-default function pointer; `RtObject_ExternalFactory_FindOrLoad`'s miss branch calls it right after `External::Load` returned null, and registers a non-null result exactly as a loaded object (`_path = expanded`, `ExternalFactory::Add`). The first `Register()` points it at `Ext::Resources::Provide`. No other engine line changed; no null guard added to retail logic. |
| Our .rsc dialect parsed into a tree | `src/ext/resources/text_parser.{h,cpp}` (pure, host-tested) |
| The objects from the tree | `src/ext/resources/text_reader.{h,cpp}` |
| The text provider (prefixes `Dialog/Extensions`, `Dialog/EnhancedOptions`) + the boot call `Ext::Resources::InitText()` | `resources.cpp`, `text_provider.h`; called from `registry_boot.cpp`'s `InitFromSettings` under `CurrentProfile() != Profile::Faithful` |
| The embedding | `tools/ext_rsc_embed.py` -> `<build>/gen/ext_rsc_text.h` (CMake target `ext-rsc-text`; OpenBA and unit depend on it, the gen dir is on `BANISHED_INCLUDES`) |
| The cross-check | `src/diag/rsc_text_check.{h,cpp}` (`/rsccheck:1` on the command line, read through `System::Configuration`, no env flag) + `tools/ext_rsc_check.py` |
| Unit tests | `tests/unit/ext_rsc_text_test.cpp` (5 tests over the embedded text and `ResolveReference`) |

`Provide()` receives the factory's EXPANDED path (`Dialog\Extensions.rsc:buttonExtensions`:
backslashes, always `:ident`) and hands each provider the same path with forward slashes, matched
by prefix in registration order. The text provider declines any file but its three (so RSCSPRITE's
`Dialog/ExtensionsSprites*` passes through). `_refs` stays empty on a provided object (the kit fills
it with the source files' FILETIMEs; nothing in the exe reads it but Load / SaveText).

## 2. The reader: positional, not name-driven

The brief assumed every Serialize call carries a field name. In the shipped DLL it does not: retail
stripped the names, every ported Serialize body passes `nullptr`, and the binary format carries none
(Serializer.h's banner). What the bodies guarantee is the ORDER of the slots, which is also the
.crs byte layout. So `text_reader.cpp` keeps a SCHEMA per description class (per class level, in
Serialize order: the kit's field name and the slot kind) and an `IO::ReaderWriter` whose cursor
walks it while the object's OWN Serialize body runs: each slot's kind is checked against the
schema (a transcription slip fails loudly and logs the slots the body asked for), a field the text
names gets the text's value, every other slot keeps the object's value. The engine does every
store, `SetSize`, String assignment and `ExternalPtr` resolve (a reference to an object of the
same file re-enters the provider through `FindOrLoad`).

Derivation `T name : "base"`: a fresh T from the type table, the base through `FindOrLoad`, a copy
through the ported `BinaryWriter` into a `MemoryStream` and the `BinaryReader` out of it (exactly
a load), then the text TWICE: a first pass answers 0 for every array the text names (the engine's
`SetSize(0)` destroys the inherited elements), the real pass after it gets zeroed fresh elements.
That is EXTRSC.md's REPLACE rule without touching a container.

Three facts the cross-check taught (all in the file's banner):
- a String the text does not name cannot be "left untouched": `System::String::Serialize`
  re-allocates its buffer on the length answer, before the chars slot. The keep answer is a zero
  length with bit 31 set (the engine then skips `SetLength` and asks for zero chars). The same for
  an `ExternalPtr`'s path string;
- an absent `ExternalPtr` is one bool and completes at once (the last slot of a class has no
  "next call");
- the kit's `~n` is a newline in the compiled string.

Schema facts pinned against the kit's bytes: ElementDescription's six ints are `_leftPad, _rightPad,
_topPad, _bottomPad, _minWidth, _minHeight` (enhancedOptions 6/2/4/4), `_alignment` is +0x40 (the
header's `_elementType`), `_color` is +0x3c (the header's `_id`), `_border` is +0x48;
LabelDescription's two uints are `_textAlignment` then `_labelPosition`; ButtonDescription's
`_content` is the fifth of its five ExternalPtrs; ContainerDescription's the third of three; a Tab
is `{_button, _content}`; a LayoutItem is `{_element, _name, _xAnchorTo, _yAnchorTo, _xLayout,
_yLayout, _xOffset, _yOffset, force}`; Dialog is `_flags, _rootElement, _spriteSheet, _spriteName,
_backgroundColor, _override, _templates, controllers, _references, drawLevel`. Enum values:
Alignment = `Element::GetAlignedPosition`'s row-major 3x3 (TopLeft 0 .. BottomRight 8); Dialog
flags PauseGame 1 / Modal 2 / FirstDraw 0x10 (retail `Dialog::PausesGame` 0x180038810, `IsModal`
0x180038800, `IsFirstDraw` 0x180038840 read bits 0 / 1 / 4 of +0x40); LayoutHorizontal LeftToLeft 1
/ LeftToRight 2, LayoutVertical TopToTop 1, LabelPosition TextBottom 3 (from our compiled output).
An identifier without a known value is an error.

## 3. Bases that exist in the packages (three .rsc edits)

The kit resolved inheritance at compile time, so a base only had to exist in the kit's `resource/`
sources. The exe derives at run time, so a base must be in the game's packages, which hold only
objects a game dialog REACHES. Three of ours were templates nothing reaches (`Mod.rsc:textHeader`,
`MessageBox.rsc`'s unnamed dialog, `SharedElements.rsc:raisedButton` / `textCombo` / `slider`) and
now derive from packed siblings that set only fields our text overrides too, so the kit's own
compiled output is byte-identical to before: `header : "Mod.rsc:textName"` (+ `_leftPad = 0`), the
three notices `: "SaveLoad.rsc:modsMissing"` (retail's own missing-mod notice), `buttonEnhanced :
"Options.rsc:buttonGame"`, the four combos `: "Options.rsc:comboAutosave"`, `sliderAoOpacity :
"Options.rsc:sliderUI"`.

## 4. Cross-check (KITFREE.md section 5)

`tools/ext_resources.sh` (the kit) compiled the current sources into `build/ext_resources/WinData/
Dialog/`; the exe ran with `/rsccheck:1` and no loose text files, writing every embedded object
(built through `Ext::Resources::Provide`, so a loose file cannot stand in) with the ported
`BinaryWriter` in `External::Load`'s layout (magic, type, version, an EMPTY dependency list, the
object) twice: at the ext boot and when the main menu attached the button. `tools/ext_rsc_check.py`
decodes the kit's LZF blocks, skips its dependency entries (source file + FILETIME, which the exe
cannot know) and compares the rest.

Result, both moments: **56 objects written, 52 compared, 52 MATCH byte for byte, 0 differ**;
4 have no kit output because the kit emits only reached objects (`Extensions.rsc:header`,
`:notice`, `:smallNote`, `EnhancedOptions.rsc:textAoOption`, templates our other objects derive
from). The only difference against the raw .crs is the kit's dependency list (0-3 entries per
object, 27-102 bytes) and, by that many bytes, the record length it back-patches.

| Object (kit .crs) | Result |
|---|---|
| Extensions.rsc: resource, dialogRoot, dialogBody, heading, labelTitle, textProfile, buttonRow, tabExtensions, tabButton, tabCaption, entryList, columnHeaders, headerName, headerDesc, headerOn, tableList, tableMods, entryRow, imageTier, entryColumn, textModName, textModDescription, textKind, textState, checkLoaded, buttonExtensions, labelExtensions, extensionNotInBuild, extensionNewer, extensionTurnedOff, noticeNotInBuild, noticeNewer, noticeTurnedOff (33) | MATCH |
| EnhancedOptions.rsc: aoStrings, aoQualityLevels, betterTreesModes, shadowResItems, seamlessLoopsItems, buttonEnhanced, labelEnhanced, enhancedOptions, textAoQuality, textAoOpacity, textBetterTrees, textMaxShadowRes, textSeamlessLoops, comboAoQuality, sliderAoOpacity, comboBetterTrees, comboMaxShadowRes, comboSeamlessLoops (18) | MATCH |
| ExtensionsStringTable.rsc:resource | MATCH |
| Extensions.rsc: header, notice, smallNote; EnhancedOptions.rsc: textAoOption | no kit .crs (unreached templates) |

The sprite sheet references (`ExtensionsSprites.rsc`) resolved to the loose
`ExtensionsSprites*.crs` (RSCSPRITE's block, left in place for the run); with RSCSPRITE merged they
come from its provider.

## 5. The run (debug exe, `C:/GOG Games/Banished`, loose text .crs moved to `_kitfree_off/`)

```
[ExtRes] Dialog/Extensions.rsc: 36 object(s) embedded
[ExtRes] Dialog/EnhancedOptions.rsc: 19 object(s) embedded
[ExtRes] Dialog/ExtensionsStringTable.rsc: 1 object(s) embedded
[ExtRes] provided Dialog/Extensions.rsc:buttonExtensions (ButtonDescription)      (37 provided lines on the button + dialog route)
[ExtMenu] Extensions button added (id 13, list ..., after index 5 of 8)            standalone_debug.log
[ExtDialog] opened: 21 row(s), game=0000000000000000 locked=0                       BANISHED_AUTOMENU_BUTTON=13
[EnhancedTab] Enhanced tab added: tabs=5 entries=4                                  BANISHED_AUTOMENU_BUTTON=4 (22 provided)
[RscCheck] 56 object(s) written to standalone_rsccheck, 0 failed                    /rsccheck:1 (boot), the same at menu time
```
`/profile:faithful`: `[Ext] profile=faithful ... GATE: faithful forced`, 0 `[ExtRes]` lines
(`InitText` never runs, the seam pointer stays null), `[ExtMenu] faithful profile forced on the
command line: no Extensions button`. No crash log, no hang log on any run.

Build `tools/build.sh build --jobs 2` exit 0; `tools/build.sh unit --jobs 2`: 1157 tests, 0
failures (5 new). The worktree needed `git submodule update --init` (third_party/SDL) once.

## 6. Not done / open

- `tools/ext_resources.sh` and the deploy step's call stay (KITFREE.md section 6: they go when the
  last block closes); `tools/build.sh deploy` still puts the loose files back, so a kit-free run
  moves them out after the deploy (or deletes them once RSCSPRITE and RSCFONT land).
- The provider builds on demand and is re-run after `ExternalFactory::Clean` dropped an object
  (KITFREE.md section 2); the clean path itself was not exercised by these runs.
- The schema covers the 16 classes our three files use (`KnowsType`); another description class
  needs its level transcribed from its Serialize body first.
- `_refs` of a provided object is empty where the kit's holds its source files' timestamps
  (unreproducible, unread by the exe).
- The faithful profile is the only gate on the seam; a provider registered under any other
  profile answers every miss for its prefix.
