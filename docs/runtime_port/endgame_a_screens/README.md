# ENDGAME A.5 — screenshot pairs (menu, world) vs. the tagged DLL-present build

Purpose: ENDGAME.md §A.5 asks for "screenshot pairs against the tagged build for the
menu and the world" as one of the routes that must pass before phase A (DLL removal)
is recorded closed. This directory holds that evidence.

## Builds compared

- **current** — `main` HEAD at the time of capture, commit `527133fe709dad92b151009e49a7809261d2823b`
  ("ENDGAME C: flag inventory (FLAGS.md) and batch rules"). Built with
  `mingw32-make standalone-debug`. Never loads Runtime-x64.dll or VideoDX11-x64.dll —
  runs entirely on the ported code/address-registry path.
- **tagged** — the `dll-present-last` tag, commit `86d28a737f2164478ef606b2a38aaaed35ccee4f`
  ("Resave gate + ENDGAME design: port CompressedStream's grow path, add the in-process
  load/resave check"), the last commit before the DLL-absent path became the default.
  Built the same way (`mingw32-make standalone-debug`) in a separate worktree
  (`../dll-present-last`). This build DOES load Runtime-x64.dll and VideoDX11-x64.dll
  from `C:/GOG Games/Banished` and renders through the retail engine DLLs.

## How they were taken

Both builds were deployed one at a time into `C:/GOG Games/Banished/reimpl-x64.exe`
under `tools/game_dir_lock.sh` (the shared-directory mutex other agents also use), and
captured with `tools/capture_window.ps1` (foreground-window screen-region capture, with
a blank-frame sanity check built in — every capture below reported 16 distinct sample
colours, well above the blank-detection floor of 2).

Common env for framing, used on all four runs:
`BANISHED_SEED=12345 BANISHED_CAMERA_PIN=0,0,60 BANISHED_MENU_SCENE=0`

- **Menu shot**: launched with no autostart var, waited ~10-12s for the main menu to
  settle (`tools/boot_state.sh` reporting `RUNNING`), then captured.
  - The **tagged** build hit the old Win32 launcher dialog
    (`Engine::PlatformConfigure`) on its first attempt, because the previous run in
    that directory had been killed rather than exited cleanly (`boot_state.sh` reported
    `BLOCKED_DIALOG`). A plain relaunch and a relaunch with `BANISHED_AUTOSTART=0`
    explicit both still hit the dialog. What worked: one launch with
    `BANISHED_AUTOQUIT=1` (lets `HarnessGuardUnattendedBoot` clear the
    `VideoSettings` arming bit in the registry and self-quit cleanly), followed by a
    plain relaunch with no harness vars at all — that came up straight to the menu with
    no dialog, and that is the run captured as `tagged_menu.png`.
  - The **current** build never hit the dialog (its own prior run in this session had
    exited cleanly).
- **World shot**: launched with `BANISHED_AUTOSTART=1`, waited for
  `[AutoStart] sequence fired` in `standalone_error.log`, then waited an additional
  ~30s for the new town to finish building and start simulating (verified by
  `[EntityCensus]` / `[Vitals]` lines appearing in the log) before capturing.

The exe was killed (`taskkill /IM reimpl-x64.exe /F`) and fresh
`standalone_{debug,error,crash}.log` / `standalone_boot_state.txt` cleared between every
run, and the game-directory lock was released after each build's pair was done.

**Note on reproducibility**: per `CLAUDE.md`'s Camera Pinning section, seed+pin gives
stable *framing*, not a byte-identical frame — the two builds are not expected to show
pixel-identical scenes (different world generation happened on each run's own boot),
only the same *kind* of scene (menu chrome / world render) rendered correctly.

## Files

| File | Build | Screen |
|---|---|---|
| `current_menu.png` | current (main HEAD, no DLLs) | main menu |
| `current_world.png` | current (main HEAD, no DLLs) | in-game world, ~30s post-autostart |
| `tagged_menu.png` | tagged (`dll-present-last`, loads Runtime-x64.dll) | main menu |
| `tagged_world.png` | tagged (`dll-present-last`, loads Runtime-x64.dll) | in-game world, ~30s post-autostart |

## By eye

- **Menu**: both builds render an identical-looking main menu — the same background
  town render (houses, river, bridge, autumn trees), the gold "Banished" title text,
  the same six buttons (New/Load/Tutorials/Achievements/Options/Mods) in the same
  positions, and the same copyright/version footer text ("Banished (c) 2015 Shining
  Rock Software LLC... Version 10.7 - Build 17Se10"). No layout, text-rendering, or
  asset differences visible.
- **World**: both builds land in a freshly generated town with the same visual
  vocabulary — terrain with grass/dirt path texturing, a rock/boulder cluster, a
  mixed pine/birch forest band, a river, a handful of starting houses with brown
  shingled roofs and smoke, and the in-game bottom-right toolbar (clock, gear,
  house, wagon-wheel, and the resource-category icons) fully drawn.
- **Toolbar / UI chrome**: present and legible in both world shots; icons render
  crisply, not as placeholder boxes.
- **Terrain/trees/buildings**: present in both; trees render as full 3D models (not
  billboards/blanks), rocks have individual shading, buildings show roof/wall texture
  detail and cast shadows.
- **Text rendering**: menu title and footer text render with correct font, outline,
  and color in both builds; no missing-glyph boxes.
- **Overall verdict**: the current (DLL-absent) build's menu and world screens are
  visually equivalent in kind to the tagged (DLL-present) build's — same UI elements,
  same asset classes, same rendering fidelity. Nothing here suggests a regression from
  removing the Runtime-x64.dll/VideoDX11-x64.dll load path.
