# EXTRSC hand-in (2026-10-11)

Extension resources through the official mod kit's compiler. The Extensions dialog, its menu button,
its texts and its sprites are now our own `.rsc` sources, compiled by the kit's `Tools-x64.exe` and
deployed loose; the Mod.rsc clone and the code-drawn icon are gone. Static gate only: the game was
NOT run by this block (`reimpl-x64.exe` was not started). Everything visual is owed to the by-hand
look (section 6).

## 1. Source layout (`src/ext/ui/`, laid out as under the kit's `resource/`)

| File | What |
|---|---|
| `Dialog/Extensions.rsc` | The dialog (our own markup: a column of heading row, one tab holding column headers over the game's scrolling Mods list taken by reference, and a right-aligned OK / Cancel row), its row template `entryRow` (template 0: icon, a name / kind / state column, description, toggle), the menu button `buttonExtensions` + `labelExtensions`, and the three load notices. Rewritten 2026-10-17 so no block restates `Dialog/Mod.rsc` (line-set diff against it: 0 identical non-trivial lines). |
| `Dialog/ExtensionsStringTable.rsc` | `StringTable resource`: 19 entries (button label + tooltip, title, profile note, tab, headers, toggle tooltip, 5 kinds, 4 lock states). Our own table; the game's `Dialog/StringTable.rsc` is not edited. |
| `Dialog/ExtensionsSprites.rsc` (+ `...Material.rsc`, `...Texture.rsc`) | The kit's sprite-sheet form (Dialog/SpriteSheet.rsc, example/building/UI/ApiarySpriteSheet*): 64 x 64 sheet, 5 sprites `ExtensionsIcon`, `ExtensionsTierFix`, `ExtensionsTierA`, `ExtensionsTierB`, `ExtensionsTierC`. |
| `Dialog/Sprite/Extensions/*.png` | 16 x 16 programmer art (the Mods button's "Mod" sprite, Dialog/Sprite/ButtonIcons/Scenario.png, is 16 x 16): a cog with a plus (the old code-drawn icon, redrawn); a coloured tile with F / A / B / C (green / blue / amber / red). Drawn by `tools/ext_sprites.py`; the user replaces the PNGs with real art, the next deploy recompiles them. `.gitignore` gains `!src/ext/ui/**/*.png`. |

Every name is under `Dialog/Extensions*`, nothing collides with the game's resources. Element names
the controller looks up: kept from Mod.rsc where the meaning is the same (`labelTitle`, `tableMods`,
`textModName`, `textModDescription`, `checkLoaded`, `buttonOk`, `buttonCancel`), new where the row
has its own texts (`textKind`, `textState`, `imageTier`, `textProfile`). Cross-file inheritance keeps
the look of the Mods dialog without restating it: `labelTitle : "Mod.rsc:labelTitle"`,
`tabExtensions : "Mod.rsc:tabMod"` (one tab overriding `_tabs`), `entryList : "Mod.rsc:modGroup"`,
`columnHeaders : "Mod.rsc:headerLine"`, `tableList : "Mod.rsc:tableList"`, `tableMods :
"Mod.rsc:tableMods"`, `imageTier : "Mod.rsc:imageIcon"`, `checkLoaded : "Mod.rsc:checkLoaded"` (own
tooltip), row border `Mod.rsc:borderLine`, `header : "Mod.rsc:textHeader"`, `tabCaption :
"Mod.rsc:labelMod"`; the compiler REPLACES an inherited array (`_tabs`, `_elements`, `_textures`)
with the derived one, it does not append (checked in the compiled objects, 2026-10-17). The button
is `buttonExtensions : "MainMenu.rsc:buttonMod"` / `labelExtensions : "MainMenu.rsc:labelMod"` with
our string table, sprite sheet, text, sprite and tooltip. The font variants (`Font/SharpFont*.rsc`,
`tools/ext_fonts.py`) derive the same way from `Font/<Kit>.rsc`, `Font/<Kit>Material.rsc` and
`Font/<Kit>Texture.rsc`, overriding only the height, sheet size and output names.

## 2. The compile step: `tools/ext_resources.sh`

```
tools/ext_resources.sh [--deploy <game-dir>] [--kit <ToolKit-dir>] [--pkg <WinData-dir>]
```

1. Finds the kit at `<repo>/ToolKit`, else (a git worktree) at the main checkout's `ToolKit` via
   `git rev-parse --git-common-dir`, else `--kit`. Absent: `ext_resources: FAILED -- the mod kit is
   absent: unpack the Banished Modding ToolKit 1.0.7 into <repo root>/ToolKit/ ...`, exit 1.
2. `build/ext_resources/res/` = `cp -r -u --preserve=timestamps` of the kit's `resource/` (57 MB, once),
   then `src/ext/ui/` over it. This is `/pathres`, so `Dialog/SharedElements.rsc`, `Mod.rsc`,
   `MainMenu.rsc`, fonts, `Material/UI/uiMaterial.rsc` resolve and inherit. The kit is never written.
3. The game's `*.pkg` (default `$BANISHED_DIR/WinData`, i.e. `C:/GOG Games/Banished/WinData`; never
   the `.pkm` mods) are hard-linked (copied if a link fails) into `build/ext_resources/WinData/`.
4. Four calls, each from the kit's directory under `timeout 900`:
   `bin/x64/Tools-x64.exe /build <T> /pathres <res> /pathdat <WinData> /pathpkg <WinData>` for
   `Dialog/Extensions.rsc`, `Dialog/Extensions.rsc:buttonExtensions` (nothing references the button,
   so the dialog's build does not reach it), `Dialog/ExtensionsStringTable.rsc`,
   `Dialog/ExtensionsSprites.rsc`. `/pathres` and `/pathdat` are passed RELATIVE to the kit's `bin/`.
   Each log (`build/ext_resources/logs/`) is grepped for Warning / Error / Failed / Unable / Assert.
5. Checks `WinData/Dialog/Extensions*.crs` exist and are non-empty; `--deploy <dir>` copies only those
   into `<dir>/WinData/Dialog/`.

Output of a clean run (4.9 s; an incremental rerun 3.7 s):

```
/build Dialog/Extensions.rsc                    -> exit 0, 80 compiled, 0 problem line(s)
/build Dialog/Extensions.rsc:buttonExtensions   -> exit 0, 8 compiled, 0 problem line(s)
/build Dialog/ExtensionsStringTable.rsc         -> exit 0, 0 compiled, 0 problem line(s)   (built by the first)
/build Dialog/ExtensionsSprites.rsc             -> exit 0, 0 compiled, 0 problem line(s)   (built by the first)
28 file(s) in build/ext_resources/WinData/Dialog: Extensions_<24 element objects>.crs,
  ExtensionsSprites_resource.crs (391 B), ExtensionsSpritesMaterial_resource.crs (145 B),
  ExtensionsSpritesTexture_resource.crs (1570 B), ExtensionsStringTable_resource.crs (601 B)
```

(The "80 compiled" are ours plus the base objects they depend on -- SharedElements borders, fonts,
the UI material, Mod.rsc:tableMods -- which stay in `build/ext_resources/WinData/` and are NOT
deployed: at run time those come from the game's packages.)

**Deploy.** `tools/build.sh deploy` (and so `run`, `run-menu`, `stress`) now ends with
`tools/ext_resources.sh --deploy "$BANISHED_DIR"`: the 28 `.crs` loose into
`C:/GOG Games/Banished/WinData/Dialog/`, where loose files load before the `.pkg`. A failed compile
fails the deploy (exit 1). The compiled outputs are not committed (`build/` is git-ignored).

**Linux (`tools/linux_gamedir.sh`): not hooked, not trivial.** That script symlinks `<gamedir>/WinData`
to the retail install's `WinData` and promises to write nothing there; a loose `.crs` would have to go
into the retail directory (or WinData would have to become a real directory of linked packages).
When the retail dir is `/mnt/c/GOG Games/Banished` (the WSL bed), a Windows `tools/build.sh deploy`
already puts the files there, so the Linux exe sees them through the link. Whether the Linux loader
finds `Dialog\Extensions_resource.crs` under a case-sensitive, `/`-separated path is unverified.

## 3. The dialog swap (`src/game/Dialog/extensionsdialog.cpp`, `ExtensionsDialog.h`)

- The opener loads `Dialog/Extensions.rsc` (was `Dialog/Mod.rsc`).
- `OnOpened`: no re-texting. The title, tab and headers are the resource's own texts.
  `textProfile` gets the profile name as `@0` when settings.cfg's `profile` overrides, else is
  hidden. Per row: `textModName` / `textModDescription` = the registry's display name /
  description (the general table's `Parameter`, as before); `textKind` = our table's
  `ExtensionsKind{Fix,A,B,C,Other}`; `imageTier` = `ExtensionsTier{Fix,A,B,C}` from our sheet
  (`ImageElement::SetSprite` with `Sprite(sheet, name)`); nothing hidden any more.
- `RefreshRow`: the lock state is `SetText` of `ExtensionsState{Live,Locked,LockedOn,LockedOff}`
  (was a `@0` parameter with C strings).
- The menu button: `Dialog/Extensions.rsc:buttonExtensions` is loaded (FindOrLoad, type-checked as an
  ElementDescription) and instantiated against the menu's dialog (was: a second instance of
  buttonMod's own description, re-texted). It carries its own label, icon sprite and tooltip
  (`ExtensionsTip`: "Turn fixes for the game's known bugs and new features on or off."). Name, id 13,
  the move to just after buttonMod and the follow-Mods enabled state are unchanged. Main menu and
  pause menu both pass `MainMenuDialog::OnOpened`, so both get it.
- Deleted: `MenuIcon` / `DrawIcon` / `BuildIcon` (the procedural texture, MaterialInstance and
  SpriteSheet held for the process), `ParameterString`, `SetElementText`'s retarget, the
  `ImageBuffer` / `Device` / `Material` / `exe_constants.h` / `LabelElement.h` / `<vector>` includes.
- Unchanged: the controller (Cancel 100, OK 0x65, row toggles 1000+i, `Apply` with the
  Dialog/Reload.rsc:game notice for a B / C change in game, `ReloadGame`, `SetAndRecord` +
  `PersistSettings`).
- Resource absent (not compiled / not deployed): `[ExtMenu] Dialog/Extensions.rsc:buttonExtensions
  did not load (tools/ext_resources.sh not deployed?): no Extensions button`; the menu is retail's.

## 4. Kit facts (for the later port of the compiler, `tools/rsc_compile`)

- **Binaries.** `Tools-x64.exe` (Build 170910, "SRS Toolset version 1.0.7") imports
  `Runtime-x64-profile.dll`, `VideoCompiler-x64-profile.dll`, `MSVCR110.dll`. The Runtime profile DLL
  imports `MSVCR110` / `MSVCP110`, `dbghelp`, `DINPUT8`, `libfbxsdk.dll` (beside it) and loads the
  platform compiler by name: format string `Compile%s%s%s%s.dll` -> `CompileWin-x64-profile.dll`
  (`LoadLibraryW`; it imports Runtime and VideoCompiler). `VideoCompiler-x64-profile.dll` imports
  `D3DCOMPILER_43.dll` and `OPENGL32.dll`. All present on this machine (System32 has msvcp110 /
  msvcr110 / D3DCompiler_43).
- **The text reader and the compilers live in `Runtime-x64-profile.dll`**: `Object::RSCCompiler`
  (`?Build@RSCCompiler@Object@@UEAAPEAVExternal@2@AEBVFilename@System@@PEAV32@AEAW4CompileResult@32@@Z`),
  `Object::ExternalCompiler`, `Object::ExtensionCompiler`, `Object::CompiledExternal`,
  `External::Build`, per-type `Compile` overrides (`SpriteSheet::Compile`, `Font::Compile`,
  `PackageFile::Compile`, `PickingMesh`, `EdgeMesh`, `AnimationGroup`, `PointList`, `CollisionMesh`),
  `ImageBufferCompiler`, `GraphicsMeshCompiler`, `SoundBufferCompiler`, `SRSLCompiler`,
  `VertexLayoutCompiler`, `VertexProgramCompiler`, `PixelProgramCompiler`. Messages:
  `Object\ExternalFactory.cpp(312)` "Source resource '...' does not exist, and compiled resource not
  found in pack!", `(364)` "Failed to load resource '...'", "Compiling '...'", "  --> Compiled %s '%s'",
  "Unable to create compiler for object type %s!". The shipped retail DLL has none of this (its Text
  encoding reader returns null); Tools-x64.exe itself is a thin driver.
- **Output format.** One `.crs` per named object: `<dir>\<file>_<ident>.crs`, the unnamed object is
  `<file>_resource.crs` (exactly `ExternalFactory::GetCompiledPath`'s concatenation in our port). Each
  file is a compressed stream (bytes `01 00 00 00 0c 00 00 00 ...`; the External magic `1f a9 12 54`
  = 0x5412a91f appears in the first block), then the type, version, the dependency list BY NAME (e.g.
  `Dialog\MainMenu.rsc`, `SharedElements...` inside `Extensions_buttonExtensions.crs`), then the
  object's binary Serialize. Inheritance (`: "File.rsc:parent"`) is resolved at compile time.
- **Paths.** `/pathres`, `/pathdat`, `/pathpkg` are relative to `/bin` (cmd.txt: `/bin ..\` = the
  kit's `bin/`); an absolute path is appended to it (`...ToolKit\bin\C:\...`) and fails. A `.rsc`
  reference is tried relative to the referencing file's directory, then to the root. The sprite
  sheet's packed image (`_imageName = "Build/ExtensionsSprites.png"`) is written under `/pathres`
  (`res/Build/ExtensionsSprites.png`).
- **Packages.** Base resources the kit ships no source for (`MeshLayout/UILayout.rsc`, which
  `Material/UI/uiMaterial.rsc` needs) must come compiled from a `.pkg` (data1.pkg has
  `MeshLayout\UILayout_resource.crs`). Found by trial: the packages are found only when they sit in
  `/pathdat` AND `/pathpkg` names that same directory; `/pathpkg` at the game's WinData (a path with a
  space), at a junction to it, or the packages in `/pathdat` with `/pathpkg` left default all gave
  "compiled resource not found in pack" and an `Assert: _vertexLayout != NULL ... Render pass must have
  vl, vs, ps, and rs!` (Video\VideoProgram.cpp(36)).
- **Failure mode.** The tool exits 0 even when it failed, and every Assert / missing-resource warning
  also pops a MODAL message box with a call stack (the user had to dismiss several during the trials).
  The script greps the log and fails, but a modal holds the process until it is dismissed or the
  900 s timeout kills it.
- `/build X.rsc` builds `X.rsc:resource` only; a file without an unnamed object (e.g. `Dialog/Reload.rsc`,
  only `menu` / `game`) fails with "Failed to load resource ..._resource.crs".

## 5. Results

- `tools/ext_resources.sh` (clean `build/ext_resources/`): 4 calls, exit 0, 0 problem lines;
  28 `Dialog/Extensions*.crs`, all non-empty (section 2). `--kit /c/nonexistent`: the install line,
  exit 1. `--deploy` was NOT run (it writes the game directory; `tools/build.sh deploy` does it).
- `tools/build.sh build debug --jobs 2` (fresh configure): exit 0, 109 warnings = EXTSCAFFOLD's
  baseline (109), none in a touched or new file. New warnings: 0.
- `tools/build.sh unit debug --jobs 2`: `1023 tests, 0 failures`.
- `tools/acceptance/acceptance.sh`: `acceptance: 0 line(s) off target`.
- `python tools/castmetric.py src | tail -1`: `# castmetric total: 2`.
- The game was not run.

## 6. What the user should look at (`tools/build.sh run-menu`)

1. The main menu: an "Extensions" button right after "Mods", with the cog-plus icon at the Mods
   icon's size; hover it -- the tooltip reads "Turn fixes for the game's known bugs and new features on
   or off." (not the Mods text).
2. Click it: the dialog titled "Fixes and Extensions" with the cog-plus icon, one tab "Fixes and
   extensions", headers Name / Description / On, three rows (the three fixes), each with a green "F"
   glyph, the fix's name, "Fix: a retail bug corrected", "live", the description, the toggle (tooltip
   "Turn this fix or extension on or off. Press OK to apply."). OK writes `fix.<id>`, Cancel does not.
3. With `profile=faithful` in settings.cfg: "(profile: faithful)" beside the title.
4. The pause menu: the same button.
5. standalone_debug.log: `[ExtMenu] Extensions button added ...`, `[ExtDialog] opened: 3 row(s) ...`,
   no `[ExtDialog] ... did not load`.

## 7. Doubts

- Nothing here was run in the game. Most likely to need a look: the button's layout in the list (it is
  now a full instance of buttonMod's description, as before, but from another file -- the `border`
  it inherits is `MainMenu.rsc:border`, resolved at compile time); `profile` text alignment beside the
  title; the glyph `SetSprite` (the image element's size is the description's 24 x 24 with a raised
  border, the sprite 16 x 16); the cross-file inherited `checkLoaded` tooltip override.
- Loose files are found because the port's loader checks the data directory before the packages
  (the task's premise; PackageFile.h's precedence note). Not exercised by a run here.
- The kit's sources are 1.0.7 (2016 timestamps); the game's packages are 2017. Our `.crs` reference
  base objects by name only, so a base object that changed between them is taken from the packages
  at run time; the inherited values (Mod.rsc / MainMenu.rsc fields) are copied at compile time from
  the kit's 1.0.7 sources.
- The deploy now needs the kit and the game's packages; on a machine without the kit `tools/build.sh
  deploy` fails with the install line. The gate itself runs `/profile:faithful`, which adds no button.
- The modal boxes on a compile failure (section 4) can stall an unattended deploy up to 900 s per call.
