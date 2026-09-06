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

## Ported here

- `element_sprite_debug.cpp`
- `element_sprite_debug.h`
- `sprite_debug.cpp`
- `sprite_debug.h`
