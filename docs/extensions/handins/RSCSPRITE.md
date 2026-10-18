# RSCSPRITE hand-in (2026-10-17): the sprite sheet built in the exe

Block RSCSPRITE of KITFREE (docs/extensions/KITFREE.md §3). A generic in-exe builder turns N RGBA
bitmaps into the three objects the kit's `SpriteSheet` compile writes, and the Extensions sprites
(the menu button icon, the Enhanced tab icon, the four tier glyphs, the 21 per-entry icons) are
provided through it, so no loose `Dialog/ExtensionsSprites*.crs` is needed in the game directory.
Branch `worktree-agent-af0a6c28eb1591f70`; NOT merged to main.

## 1. Files

| File | What |
|---|---|
| `tools/ext_sprites_bake.py` | Reads `src/ext/ui/Dialog/ExtensionsSprites.rsc` (sheet size, `_padForFiltering`, `_materialName`, `_imageName`, the `_sprites` table) and decodes every source PNG with Python's zlib (no PIL) into straight RGBA8; writes the generated header. Run by hand after a PNG or table change; the header is committed (our art). |
| `src/ext/resources/ext_sprites_data.h` | GENERATED: `SpritesData::kSheetWidth/Height`, `kPadForFiltering`, `kMaterialName`, `kImageName`, one `kRgba_<name>[]` per sprite and `kBitmaps[kBitmapCount]` (27; table order = sprite index). 760 KB of text, 200 KB of pixels. |
| `src/ext/resources/sheet_builder.h/.cpp` | The generic builder, host-linkable: `PackSheet`, `RecordFor`, `FillSpriteSheet`, `FillMaterialInstance`, `FillImageBuffer`, `CreateTexture`. |
| `src/ext/resources/sheet_objects.cpp` | The exe half: `BuildSheetObjects` / `DestroySheetObjects` (type factories, the base material by reference, the GPU texture). |
| `src/ext/resources/ext_sprites.h/.cpp` | `Ext::Resources::InitSprites()`: the provider for prefix `Dialog/ExtensionsSprites`. |
| `src/ext/resources/resources_stub.cpp` | **DELETE AT MERGE.** A stand-in for RSCTEXT's `resources.cpp` (Register / Provide / SetPath / Publish) so this branch links alone; no seam. |
| `src/ext/registry_boot.cpp` | `Resources::InitSprites()` after `Install()`. RSCTEXT puts its `InitText()` under `if (CurrentProfile() != Profile::Faithful)`; `InitSprites()` belongs in the same block (done that way on the e2e branch, section 5). |
| `tests/unit/ext_sheet_builder_test.cpp` (+ `_stubs.cpp`, `sources.cmake`) | The packer test and the kit cross-check (section 4). |

## 2. The builder API (for block RSCFONT)

```cpp
namespace Ext::Resources {
struct SheetBitmap { const char* name; const char* group; int width, height; const uint8_t* rgba; };
struct SheetSpec   { int sheetWidth = 256, sheetHeight = 256; bool padForFiltering = true;
                     const char* materialName; const char* imageName;      // the two SpriteSheet strings
                     int32_t imageKind = 2, imageFormat = 5; uint32_t imageFlags = 0;
                     int32_t imageAddress = 1, imageFilter = 1; };          // ImageBuffer parameters
struct SheetPlacement { int x, y; };                                      // the bitmap's own pixels
struct SheetLayout { int width, height; bool padded; std::vector<SheetPlacement> places; std::vector<uint8_t> bgra; };
struct SheetRecord { int32_t x, y, width, height, reserved; float u0, v0, u1, v1; };

bool        PackSheet(const SheetSpec&, const SheetBitmap*, int count, SheetLayout& out);  // false: does not fit
SheetRecord RecordFor(const SheetLayout&, const SheetBitmap&, int i);
void FillSpriteSheet(UI::SpriteSheet&, const SheetSpec&, const SheetBitmap*, int, const SheetLayout&, Graphics::MaterialInstance*);
void FillMaterialInstance(Graphics::MaterialInstance&, Graphics::Material* base, const char* slot, Video::ImageBuffer*);
void FillImageBuffer(Video::ImageBuffer&, const SheetSpec&, const SheetLayout&);   // fields only
bool CreateTexture(Video::ImageBuffer&, const SheetSpec&, const SheetLayout&);     // ImageBuffer::Create, one level

struct SheetObjects { UI::SpriteSheet* sheet; Graphics::MaterialInstance* material; Video::ImageBuffer* texture; };
SheetObjects BuildSheetObjects(const SheetSpec&, const SheetBitmap*, int count,
                               const char* baseMaterialPath, const char* textureSlot = "diffuse");
void DestroySheetObjects(SheetObjects&);
}
```

- `PackSheet` is a shelf packer: tallest first (ties in table order), rows left to right, each
  padded bitmap `w + 2` x `h + 2` with its pixels at `(+1, +1)`; returns false when the sheet is
  too small (RSCFONT picks the next size and retries, its `ext_fonts.py` rule). The atlas is
  BGRA8, top-down; unused texels 0.
- A FONT caller: bitmaps = the glyphs (name = the kit's glyph sprite name, group = whatever the
  kit writes for a font sheet -- read a compiled `Build/FontSheet_resource.crs` through the
  loader to see), spec from `Font/<Kit>.rsc`'s `_sheetWidth/_sheetHeight`, `_imageName`,
  `_materialName`; then `BuildSheetObjects(spec, glyphs, n, "Font/<Kit>Material.rsc"'s base
  material path, "diffuse")`, `SetPath` on the three, hook `font->_spriteSheet` at the sheet.
  Paths are NOT set by the builder (the provider does it); nothing is cached (every call builds
  fresh objects, the factory owns what Publish hands it).
- `BuildSheetObjects` needs `ExternalFactory::_singleton` and `Video::Device::_singleton` (both
  exist by the time any resource loads) and logs every failure as `[ExtRes] sheet: ...`.

## 3. Facts about the kit's form (read off `tools/ext_resources.sh`'s output for ExtensionsSprites.rsc)

- **Container** (`.crs`): `IO::CompressedStream` blocks (LZF); decoded = the loader's frame
  `aaaaaaaa len ver=0` | magic `1f a9 12 54` | typeId | GetFileVersion | depCount | deps
  (String + 2 x u32 = the source's FILETIME) | the object's own frame `aaaaaaaa len ver=0` |
  Serialize body | `bbbbbbbb` | `bbbbbbbb` (object's trailer, then the loader's).
- **SpriteSheet** (type `1821d6a5`, version 6): `_materialName` (+0x40, FIRST; "ExtensionsSpritesMaterial.rsc"
  as written), `_imageName` (+0x38; "Build/ExtensionsSprites.png", forward slash kept), width,
  height, `_padForFiltering`, count, then per sprite: name, group = the SOURCE PATH with
  backslashes ("Sprite\Extensions\TierA.png"), x, y, width, height, `_reserved18`, u0, v0, u1, v1;
  then the material ExternalPtr (bool 1 + "Dialog\ExtensionsSpritesMaterial.rsc:resource").
  With padding: **x = y = 1 and `_reserved18` = 1** (the pad offset; nobody in the engine reads
  x / y / +0x18), width / height = the bitmap's own, uv = pixel edge / sheet size (exact floats).
- **Padding**: a 1-texel ring around every bitmap that REPLICATES its edge row / column, corners
  the corner texel; neighbours 2 texels apart; the first bitmap's pixels at (1, 1). Verified
  texel by texel against the kit's `Build/ExtensionsSprites.png` and its texture blob.
- **The kit's packing order is not derivable** (48 x 48 before 16 x 16, then a permutation that
  is neither table nor name order); ours differs, which KITFREE.md §5 allows.
- **MaterialInstance** (type `2ebeed54`, version 4): the base material ExternalPtr (1 +
  "Material\UI\uiMaterial.rsc:resource"), 1 texture {"diffuse", 1 + "Dialog\ExtensionsSpritesTexture.rsc:resource"},
  0 constants; on load `BuildParameters()` runs -- `FillMaterialInstance` does the same.
- **ImageBuffer** (type `84e6f2dd`, version 7), the `Texture/UITexture.rsc` derivation: width,
  height, `_dimension` 0, `_kind` 2, **`_format` 5 = DXGI B8G8R8A8_UNORM**, `_flags` 0 (bit 0 clear
  = ONE mip level), `_addressU/V/W` 1 (clamp), `_filter` 1 (linear), `_sourceName` =
  "Build\ExtensionsSprites.png" (a PATH: backslash), then PlatformSerialize: level count 1, one
  blob of `w * h * 4` BGRA8 bytes, row-major, top-down. The DX11 backend
  (`src/render/dx11/imagebufferdata.cpp` `Create(levels)`) makes an IMMUTABLE texture from the
  levels with pitch `kFormatBytes[5] * width`; the builder hands the same `LevelArray` to
  `ImageBuffer::Create` that `PlatformSerialize` hands it on load.
- **ImageBuffer::Serialize is NOT replayable on the write path**: `PlatformSerialize` writes an
  EMPTY level list and then `Create`s the texture again (destroying the GPU contents). The
  provider therefore never serializes the texture; the cross-check compares its fields by parse.

## 4. Cross-check (KITFREE.md §5)

`tests/unit/ext_sheet_builder_test.cpp`, with the kit's `build/ext_resources/WinData/Dialog/
ExtensionsSprites*.crs` present (skipped, not passed, without them):

| Object | Compared | Result |
|---|---|---|
| SpriteSheet | our filled object written in the BinaryWriter's record format vs the kit's object body, byte for byte, the 4 uv floats of each of the 27 records blanked in both | **identical, 3116 bytes** (names, groups, x/y/w/h/reserved, both strings, size, flag, count, the material path, the frame) |
| SpriteSheet placement | every sprite's rectangle (+ ring) in the kit's atlas vs ours | **27 / 27 identical pixels** (placement differs: ours is shelf order) |
| ImageBuffer | parsed header fields vs the spec: 256 x 256, dim 0, kind 2, format 5, flags 0, address 1/1/1, filter 1, "Build\ExtensionsSprites.png", 1 level of 262144 bytes | **all equal** |
| MaterialInstance | parsed records vs what FillMaterialInstance writes: base "Material\UI\uiMaterial.rsc:resource", 1 texture "diffuse" -> "Dialog\ExtensionsSpritesTexture.rsc:resource", 0 constants | **all equal** |
| Type ids | kit headers `1821d6a5` / `2ebeed54` / `84e6f2dd` vs the exe's `TypeId()` (logged) | **equal** |
| The real writer | the exe's provider writes the built sheet and material with the ported `IO::BinaryWriter` (`UI::SpriteSheet::Serialize`, `MaterialInstance::Serialize` over an `IO::MemoryStream`) and logs byte count + CRC32: `sheet record 3116 bytes crc cf50032b`, `material record 127 bytes crc 5b92b9ed`; the unit test's mirror of the same sheet prints `3116 bytes, crc cf50032b` | **equal**: the mirror IS the writer's output, so the byte compare above holds for the ported writer too; the material's 127 bytes = the kit's 0x73 body + 12 frame bytes |

Why a mirror in the unit: the host `unit` cannot link `spritesheet_vtable.cpp`, `encoding.cpp`,
`serialize_family.cpp` -- `ui_resources_abi_test.cpp` and `b39_io_classes_test.cpp` define
SpriteSheet's slots and the encodings' / BinaryWriter's bodies as slot-order PROBES. The
exe-side CRC line closes that gap. `unit`: **1154 tests, 0 failures, 0 skipped** (debug).

## 5. End-to-end run (the merged seam)

RSCTEXT's seam (`4c2c96e8d`) and `resources.cpp` (`24b433947`) were merged into a THROWAWAY local
branch `rscsprite-e2e` (not pushed; `resources_stub.cpp` deleted there, `InitSprites()` moved
under the non-faithful guard beside `InitText()`). `tools/build.sh deploy --jobs 2`, the three
`WinData/Dialog/ExtensionsSprites*.crs` moved to `_kitfree_off/`, `OpenBA.exe` launched at the
menu under `tools/game_dir_lock.sh acquire rscsprite`, then again with
`BANISHED_AUTOMENU_BUTTON=13` (the Extensions button):

```
[ExtRes] sprites: built sheet + material + texture (27 sprites, 256 x 256, pad 1) for Dialog/ExtensionsSprites.rsc:resource
[ExtRes] sprites: sheet record 3116 bytes crc cf50032b (IO::BinaryWriter)
[ExtRes] sprites: material record 127 bytes crc 5b92b9ed (IO::BinaryWriter)
[ExtRes] sprites: type ids SpriteSheet 1821d6a5 MaterialInstance 2ebeed54 ImageBuffer 84e6f2dd
[ExtRes] provided Dialog/ExtensionsSprites.rsc:resource (SpriteSheet)
[ExtMenu] Extensions button added (id 13, list ..., after index 5 of 8)          (standalone_debug.log)
[AutoStart] menu settled - injecting click id=13
[ExtDialog] opened: 21 row(s), game=0000000000000000 locked=0
```

No `did not load`, no `[ExtRes] ... fail`, no crash (`standalone_crash.log` empty), RUNNING at the
menu and with the dialog open. The loose files were moved back, the exe killed by PID, the lock
released. **Not seen by eye**: `tools/capture_window.ps1` returned "capture looks blank" for both
the menu and the dialog (the window was not presenting to GDI from this session), so the icon's
look is owed to the user's run-menu (KITFREE.md §6).

## 6. Not done / open

- `resources_stub.cpp` must be deleted when RSCTEXT's `resources.cpp` merges (duplicate symbols
  otherwise); `registry_boot.cpp` conflicts in two places (the include, the `InitSprites()` call --
  resolve as on `rscsprite-e2e`: both includes, `InitSprites()` inside the non-faithful block).
- No screenshot (above). The sprite-capture class exists in the engine but no diag drives it at
  the menu; `BANISHED_SCREENSHOT_AT` is in-game only.
- The provider rebuilds all three objects on every request. A request for the material or the
  texture path alone (never made by the game: the sheet holds its dependents by pointer) builds
  the set and deletes what was not asked for.
- The ImageBuffer parameters (kind / format / flags / address / filter) are the `SheetSpec`'s
  constants, not read from the game's `Texture/UITexture.rsc` at run time; format 5 (BGRA8) is the
  only format the builder paints.
- Linux: `sheet_objects.cpp` is platform-neutral (the engine's own Create path); untested there.
