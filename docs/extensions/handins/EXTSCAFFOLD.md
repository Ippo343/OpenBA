# EXTSCAFFOLD hand-in (2026-10-11)

DESIGN.md §3 and §4 step 1: the registry, the settings keys, the gate's faithful force, the
Extensions dialog and its menu button, unit tests, CMake. Static gate only: the game was NOT run
by this block (coordinator's rule). Everything that touches the UI at run time is unverified until
the coordinator's by-hand run (§6).

## 1. What exists

| File | What |
|---|---|
| `src/ext/registry.h` / `registry.cpp` | THE table (id, tier Fix/Extension, class None/A/B/C, default, display name, description, formatVersion, the bool's accessor); `Ext::Fix()` / `Ext::Ext()` read-once structs; profiles; the B/C lock; the pseudo-mod list for the save. Standard C++ only (the unit tests link it alone). |
| `src/ext/registry_boot.cpp` | Engine glue, exe only: `Ext::InitFromSettings()` (settings map + command line), `Ext::SetAndRecord()` / `Ext::PersistSettings()` (write-back through `Persistence::Set` + `WriteSettings`). |
| `src/game/Dialog/ExtensionsDialog.h` / `extensionsdialog.cpp` | The dialog, its opener, and the menu button (`ExtensionsDialog_AttachMenuButton`). |
| `tests/unit/ext_registry_test.cpp` | 11 tests (below). |

Hook form for block FIXPILOT: `if (Ext::Fix().icon_slide) { corrected } else { retail }`
(`#include "ext/registry.h"`; `src` is now the LAST include directory, so only `ext/...` resolves
there). Registered: three fixes, default ON — `icon_slide`, `residence_goto`, `tavern_counter`
(descriptions from retail_bugs.md §1-3). No extensions. Under `-DBANISHED_UNIT_TEST` only, three
test extensions `unit_a` / `unit_b` / `unit_c` (one per class) exist so the lock is tested now; they
are not in the exe.

Seams added to existing bodies (each one line + an `// EXT SEAM` comment; no faithful logic changed):
- `src/game/Game/game_main.cpp` — `Ext::InitFromSettings()` right after `Engine::Startup` succeeds
  (ReadSettings has run inside Startup).
- `src/game/Game/gameupdateloop_load.cpp` — `Ext::LockForGame(this)` right after `_singleton = this`
  in `GameUpdateLoop::Load` (new game and load both pass there).
- `src/game/Game/gameupdateloop.cpp` — `Ext::UnlockForGame(this)` in `~GameUpdateLoop`.
- `src/game/Dialog/mainmenudialog.cpp` — `ExtensionsDialog_AttachMenuButton(this)` at the end of
  `OnOpened`; `case kExtensionsMenuButtonId (13)` in `OnElementEvent` opening the dialog.

## 2. Settings keys (settings.cfg, read once at boot)

| Key | Values | Absent / other |
|---|---|---|
| `fix.<id>` | `0` / `1` | the entry's default (fixes ON); a non-0/1 value is counted "malformed" and keeps the default |
| `ext.<id>` | `0` / `1` | the entry's default (extensions OFF) |
| `profile` | `faithful` (all off) / `all` (all on) | per-entry keys |

Precedence: command line `/profile:<name>` > settings `profile` > `fix.*` / `ext.*`. Unknown ids
are never looked up (ignored). Boot logs one `[Ext] profile=...` line plus one per entry to
**standalone_error.log**. The dialog writes `fix.<id>=0|1` / `ext.<id>=0|1` (Persistence's `%i`).
Ids are `[a-z0-9_]+` (`Ext::ValidId`; the unit test checks every row).

Lock semantics: fixes and class A toggle live. B / C: `LockForGame(owner)` copies the request
into the active state and freezes it; a toggle in game changes only the request; the lock is
released only by the same owner (a reload's new loop locking before the old one is destroyed keeps
its lock). At the menu nothing is locked and a B/C toggle takes effect at once (for the next game).

## 3. The gate's faithful force — what I did

**Mechanism: the exe's own command line, `/profile:faithful`.** Not a settings.cfg write and not an
environment flag. Reasons:
- settings.cfg is NOT in the game dir: it is `<Documents>\Banished\settings.cfg`
  (`SettingsStore::DefaultPath`), the player's real file. A gate writing `fix.*=0` or
  `profile=faithful` there would either leave the player's game faithful afterwards or need a
  restore, and `tools/build.sh run` returns at launch (no point to restore at).
- Retail already parses a command line: `System::Configuration::SetCommandLine` (`/key:value`,
  ParseCommand 0x1800c4a00 / ParseValue 0x1800c4930; the same map `/nomods` and `/reset` are read
  from). `registry_boot.cpp` reads key `profile` with `GetCommandLineString`. No getenv, no
  BANISHED_* flag, nothing persists past the run.
- The settings key `profile=faithful` ALSO exists (a player's choice), but the command line wins.

Scripts changed:
- `tools/build.sh run` — launches `start reimpl-x64.exe /profile:faithful` (with
  `MSYS2_ARG_CONV_EXCL` so the shell cannot rewrite the leading `/`; a native-exe probe showed Git
  Bash leaves `/profile:faithful` alone anyway). `run-menu` (the by-hand look) does NOT pass it.
  `deploy` only copies; nothing to add there.
- `tools/resave_gate/resave_gate.sh` — `./reimpl-x64.exe /profile:faithful`.
- Not changed: `tools/stress_run.sh` (the everything-on gate is a later decision; `/profile:all`
  exists for it), `tools/linux_run.sh`, other kits.

A run forced faithful from the COMMAND LINE also leaves the main menu exactly retail's (no
Extensions button; `Ext::GateProfileForced`). The log says `-- GATE: faithful forced, menu unchanged`.

**Risk to watch on the first gate run:** before this block no automated run passed ANY command
line, so `SetCommandLine`'s parse loop (ParseCommand / ParseValue / `rtavl::StringMapInsert`) runs
for the first time in a gate. All three are ported (configuration.cpp, avl_trees.cpp); by hand,
`/profile:faithful` parses to key `profile`, value `faithful`. I tried a unit test of the parse, but
it needs the real `MemoryAllocator` under `StringMapInsert` while the unit build's `System::String`
allocates with malloc (string.cpp's BANISHED_UNIT_TEST swap): it segfaulted, and I removed it rather
than rework the unit allocator. So: check the `[Ext] profile=faithful (command line /profile)` line
in standalone_error.log of the first gate run.

## 4. Can the shipped loader load a loose text `.rsc`? — No.

- `Object::External::Load` (src/engine/object/external_load.cpp, retail 0x1800bc620) reads a
  COMPILED resource only: a `CompressedFileStream`, magic `0x5412a91f`, type id, version, dependency
  array, then the object's binary Serialize.
- The Text encoding's CreateReader / CreateWriter are stripped from the shipped DLL (both return null;
  src/engine/io/encoding_visitor_text.cpp, slots 0x1800b9a80 / 0x1800b9a70).
- In the packages, `Dialog/Mod.rsc:modLine` is stored as `Dialog\Mod_modLine.crs`: one compiled
  object per named sub-resource (read with tools/audio_tail/pkg_sounds.py's reader). A loose override
  would have to be such a compiled `.crs`.
- `Tools-x64.exe` (the compiler) is not on this machine (searched C:/GOG Games, C:/Games, C:/tools,
  the repo).

So no `.rsc` sources were written and `src/ext/ui/` does not exist. What I did instead:
- **The dialog is built on retail's `Dialog/Mod.rsc`** at run time: its row template (`modLine`) per
  registry entry; textModName = display name, textModAuthor = kind/class ("Fix: a retail bug
  corrected", "Extension, class B: changes play, locked per game", ...), textModDescription =
  description, textModVersion retargeted to the general table's `Parameter` string (`@0`) = lock
  state ("live" / "locked for this game", + "(on/off from the next game)" when the request
  differs), checkLoaded = the toggle. Hidden: imageConflict, imageIcon, the Top/Up/Down/Details
  buttons, textOrder/textTop/textDetails headers, buttonBrowse, groupProgress. labelTitle →
  "Fixes and Extensions" (+ "(profile: ...)" when a settings profile overrides), the tab label →
  "Fixes and extensions".
- **The front-end button is a clone of the menu's `buttonMod`** (its own `_description`
  instantiated against MainMenu's dialog, renamed `buttonExtensions`, id 13, moved in the button
  list to right after Mods), label retargeted to `@0` = "Extensions", and **its own icon**: a
  procedurally drawn cog-with-plus texture at the Mods icon's size, built with ModDialog's thumbnail
  pipeline (ImageBuffer + MaterialInstance on `Material/UI/UIMaterial.rsc` + SpriteSheet), held for
  the process. It follows buttonMod's enabled state (the pause menu disables Mods for some games).
  The pause menu gets the same button (Pause.rsc also uses MainMenuDialog and buttonMod).
- The tooltip is still the cloned "ModTip" text ("Show and configure player created additions to
  the game."): changing it means a new string-table entry or the tooltip-string path, which I did
  not risk without a run.

If a real `.rsc` / sprite is wanted later: get Tools-x64.exe (the mod kit), compile sources under
`src/ext/ui/`, and ship the `.crs` loose or in a `.pkm`; the dialog code would swap
`Dialog/Mod.rsc` for it.

## 5. The B/C notice and the save seam

- OK with a B/C change while a game runs shows `Dialog/Reload.rsc:game` (the Mods dialog's notice,
  `BANISHED_MODAL_ANSWER` honoured as in ModDialog::Apply). Confirmed: request recorded, settings
  written, the game save-restores and reloads through the menu (ModDialog::CloseAndReload's game
  arm), and the new loop takes the lock with the new request. Cancelled: the B/C changes are
  dropped, fixes and class A changes are still applied. Cancel (id 100) applies nothing.
- SAVETAIL's amendment (coordinator message; DESIGN.md at main 1a180d42f) is in: every B and C entry
  carries a `formatVersion >= 1` (unit-tested); `Ext::PseudoModsForSave` / `Ext::PseudoMod` give
  name = display name, path = `ext:<id>`, field10 = 0, field14 = `0x010007ff`, formatVersion
  (ModInfo +0x18). The engine-facing seam: `Ext::PseudoModRecord` (plain pointers), the provider
  type `Ext::ExtraModsProvider = int (*)(PseudoModRecord*, int capacity)`, its body
  `Ext::ProvideExtraModsForSave`, and `Ext::SaveModsProvider()` = the value to install at boot —
  **null under the faithful profile and while no B/C extension is registered** (always null in this
  exe). No save code was touched: `Save::SaveLoad::SetExtraModsProvider` and the SaveMetaData call
  are the next block's.

## 6. Results and what is owed

- `tools/build.sh build debug --jobs 2`: exit 0, 109 warnings = the baseline build of the same base
  (109), none in a touched or new file.
- `tools/build.sh unit debug --jobs 2`: `1013 tests, 0 failures` (11 new `ext_registry_*`).
- `tools/acceptance/acceptance.sh`: `acceptance: 0 line(s) off target`.
- New warnings: 0.

Owed (coordinator, by hand, the game was not run here):
1. The gate run: `[Ext] profile=faithful (command line /profile)` in standalone_error.log, no
   `[ExtMenu] Extensions button added` line, invariants unchanged.
2. `tools/build.sh run-menu`: the Extensions button beside Mods with its own icon and the
   "Extensions" label; the layout of the button list with one more button; click it — the dialog
   lists three fixes, toggles, OK writes `fix.<id>` to settings.cfg, Cancel does not.
3. The same from the pause menu.
Things most likely to need a fix on that run: the clone's click routing (the id is set on the clone;
the dialog dispatch is the parent dialog's), the list layout after the child reorder, and the icon's
channel order (the greys are chosen so RGB order does not matter; alpha is the top byte).
