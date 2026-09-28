# runtime/src/ui — original `code\Runtime\UI\`

Port target for the Runtime-x64.dll `UI` directory. The file list below is the
ORIGINAL source tree recovered from `Runtime-x64-profile.dll` debug strings
(`docs/runtime_port/original_source_tree.txt`); mirror these names here, lowercased,
as `runtime/src/ui/<file>.cpp`, with binary-compatible types in
`runtime/types/ui/<Class>.h` (`static_assert` on every field offset).

A `_win` file is a PLATFORM SEAM (PLAN.md §4.1 decision 1): keep `<windows.h>` out of
every other file in this directory so a `_posix`/`_sdl` twin can be written later.
A `_compile` file is the offline asset compiler and is likely absent from retail —
confirm against the retail DLL before porting one.

## Original files (31 cpp, 38 h)

- `AnimatedImageElement.cpp`
- `AnimatedImageElement.h`
- `BorderElement.cpp`
- `BorderElement.h`
- `ButtonElement.cpp`
- `ButtonElement.h`
- `CharacterSet.h`
- `CheckElement.cpp`
- `CheckElement.h`
- `ClipRegionElement.cpp`
- `ClipRegionElement.h`
- `ComboDialog.h`
- `ComboElement.cpp`
- `ComboElement.h`
- `ContainerElement.cpp`
- `ContainerElement.h`
- `Cursor.h`
- `Dialog.h`
- `DialogInstance.cpp`
- `DialogInstance.h`
- `DocumentTextElement.cpp`
- `DocumentTextElement.h`
- `EditElement.cpp`
- `EditElement.h`
- `Font.h`
- `Font_compile.cpp`
- `GraphElement.cpp`
- `GraphElement.h`
- `GroupElement.cpp`
- `GroupElement.h`
- `ImageElement.cpp`
- `ImageElement.h`
- `LabelElement.cpp`
- `LabelElement.h`
- `LayoutElement.cpp`
- `LayoutElement.h`
- `ListElement.cpp`
- `ListElement.h`
- `NumberEditElement.cpp`
- `NumberEditElement.h`
- `NumberElement.cpp`
- `NumberElement.h`
- `PageElement.cpp`
- `PageElement.h`
- `ProgressElement.cpp`
- `ProgressElement.h`
- `RadioElement.cpp`
- `RadioElement.h`
- `RibbonElement.cpp`
- `RibbonElement.h`
- `ScrollElement.cpp`
- `ScrollElement.h`
- `SliderElement.cpp`
- `SliderElement.h`
- `SpinnerElement.cpp`
- `SpinnerElement.h`
- `SpriteCapture.h`
- `SpriteSheet.h`
- `SpriteSheet_compile.cpp`
- `StringTable.h`
- `TabElement.cpp`
- `TabElement.h`
- `TableElement.cpp`
- `TableElement.h`
- `TextElement.cpp`
- `TextElement.h`
- `ToolTipDialog.h`
- `UserInterface.cpp`
- `UserInterface.h`

## Status (ENDGAME B.3.6 closing pass, 2026-09-26)

Every Runtime UI class is a REAL C++ class under its retail name: 68 polymorphic
(`UI::Element` + 27 leaf elements, `UI::ElementDescription` + 27 leaf
descriptions, the dialog family `Dialog` / `DialogInstance` / `DialogController` /
`DialogControllerConfig` / `ComboDialog` / `ToolTipDialog`, and the resources
`CharacterSet` / `Cursor` / `Font` / `SpriteCapture` / `SpriteSheet` /
`StringTable`) and 5 non-polymorphic (`UserInterface`, `SpriteContext`, `Sprite`,
`String`, `Element::UpdateState`). Declarations live in `runtime/types/ui/<Class>.h`
(one header per class; `Element::UpdateState` is nested in `Element.h`). Rules
and history: `docs/runtime_port/endgame_b/B36_RULES.md`; numbers:
`docs/runtime_port/endgame_b/b36_session/CLOSE_NUMBERS.md`.

Where the member bodies are:
- one file per class, lowercased: `<class>.cpp` for each of the 27 leaf
  elements and 27 leaf descriptions, `characterset.cpp`, `cursor.cpp`, `font.cpp`,
  `combodialog.cpp`, `tooltipdialog.cpp`, `dialogcontrollerconfig.cpp`;
- `Element`: `element.cpp` + `element_vtable.cpp`; `ElementDescription`:
  `dialog.cpp` + `elementdescription_serialize.cpp`;
- `DialogInstance`: `dialog.cpp`, `dialog_create.cpp`, `dialog_settings.cpp`
  (`EndHover` in `userinterface_hover.cpp`); `Dialog`: `dialog_vtable.cpp`,
  `dialog.cpp`; `DialogController`: `dialog_controller_base.cpp`;
- `UserInterface`: `userinterface*.cpp`; `SpriteContext`: `spritecontext*.cpp`;
  `SpriteCapture`: `spritecapture_class.cpp`, `spritecapture.cpp`;
  `SpriteSheet`: `spritesheet_vtable.cpp`, `sprite.cpp`; `StringTable`:
  `stringtable_factory.cpp`, `sprite.cpp`; `Sprite` and `String`: `sprite.cpp`;
- FRAGMENT files (bodies of several classes, kept where the Wave-7 lanes put
  them): `widgets_input.cpp` (Combo/Spinner/NumberEdit/Number/Radio/Edit/Slider/
  Progress non-slot members), `widgets_layout.cpp` (Table/Image/Label/Text/
  DocumentText/Tab/Page/Graph and `Element::RemoveChildren`),
  `element_bodies_b5.cpp` (thirteen assorted leaf bodies), `documenttext.cpp`,
  `textelement_cachetext.cpp`, `tabelement_addtab.cpp`;
- allocator doors, not members: `spritesheet_new.cpp`, `ui_operator_delete.cpp`;
  the aggregator is `rt_ui_port.cpp`. (The factory interpreters
  `ui_factory_wave28.cpp` and `factory_ui1.cpp` are deleted: every UI class
  registers its own `CreateNewObject` / `CreateInPlaceObject`, ENDGAME B.3.9.)

Every registered export door is a one-line QUALIFIED delegation to its member.
Since ENDGAME B.3.9 every UI class is a real C++ class: compiler vtable, its own
`DeletingDtor`, its `DEFINE_TYPE` record; no minted table or deleting-dtor row
is left. `element_sprite_debug.cpp` and
`sprite_debug.cpp` are debug-print wrappers over the members, not ports.
