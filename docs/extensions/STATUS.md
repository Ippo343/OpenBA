# Fixes and extensions -- status (newest first)

## 2026-10-12 -- SHARPUI + EXTRSC merged (gate SHARPUI at 0); the stress false alarm

- `ext.sharp_ui` (class A, default off, `dee1fd743`): when on, the effective UI scale snaps to an integer
  device-pixel ratio -- `ceil(100 / slider%)`, so 75-99% = 2x, 100-150% = 1x, capped at the largest ratio
  that leaves an 800x600 layout (1080p stays 1x, `clamped=1` in the trace; 1440p and 4K get 2x); the slider
  value is never written. Pixel-snapped quads and per-batch point sampling through
  `UI::SpriteContext::_extSharp` / `Video::ImageBufferData::_extPointSampling` (rule 5 shape); retail's
  `UserInterface::Draw` runs unchanged inside the wrapper. NOTE the slider's sense: 75% is the BIGGER UI.
  Sharp text (c) is a costed plan in `handins/SHARPUI.md` §11, not code. `[SharpUI] on ...` trace line.
- EXTRSC (`404aede4d`): the Extensions dialog, button, 19 strings and 5 placeholder sprites are our own
  resources under `src/ext/ui/`, compiled by `ToolKit/bin/x64/Tools-x64.exe` through `tools/ext_resources.sh`
  (`/build <T> /pathres <res> /pathdat <WinData> /pathpkg <WinData>`, the kit's `resource/` overlaid with ours),
  28 `.crs` deployed loose into WinData/Dialog by `tools/build.sh deploy`. Deploy now NEEDS the kit. The
  script hard-links the game's `.pkg` into the build dir (mtimes intact, but a hazard: make it a copy or a
  read-only reference). A failed compile pops a modal Assert box (unattended hazard; 900 s timeout per call).
- **False alarm, two hours**: the first stress pair read 0.01 ms per iteration. Not the loose files (the
  correlation was coincidence): the game window came up UNFOCUSED while the user typed elsewhere,
  `Application::IsPaused` + `GameAutoPause=true` made `PlayerToolManager::Update` (retail 0x1401026a0) open
  the pause menu at frame 1, and a pause menu pauses the sim. `tools/stress_run.sh` now switches
  GameAutoPause off for the run and restores settings.cfg at exit, as `linux_run.sh` already did
  (`8f15c5936`). Lesson: before bisecting a stress stall, check for `[MainMenu] OnOpened ... game=<world>` at
  frame 1 in the error log.
- **LOOKED (the user, 2026-10-12)**: the Extensions dialog on compiled resources works, glyphs show. The
  integer snap at 75% = 2x: crisp but "blows it up to the point of being unusable" (screenshot 100 vs 75).
  VERDICT: no scale snap; the slider means what it always meant. The kit's Font/*.rsc name an installed
  typeface + pixel height and the compiler rasterizes the sheet, so sharp text at a non-integer scale =
  scale-matched font atlases with metrics divided back to layout units. Block SHARPUI2 dispatched: snap
  removed, compiled font variants under src/ext/ui/Font/, a 2x sprite-sheet lookup convention (no art).

## 2026-10-11 night -- decisions: resources for extensions, the toolkit compiler

- **Now (route 1)**: the official mod kit's `Tools-x64.exe` compiles the sources under `src/ext/<name>/ui/`
  offline; a build step deploys the compiled `.crs` loose into WinData (loose loads first); the Extensions
  dialog moves from its Mod.rsc clone to its own `Dialog/Extensions.rsc` + icon sprite. The compiler is a
  developer-machine dependency (not redistributable, not in the repo); Linux consumes the compiled outputs.
  NOT blocked after all: the kit is in the repo at `ToolKit/` (git-ignored; `bin/x64/Tools-x64.exe`, the
  profile binaries, `resource/`, the example mods) -- EXTSCAFFOLD searched only outside the repo. Block EXTRSC
  dispatched: the compile step + the dialog swap.
- **Later (route 2)**: port the compiler -- the text Encoding reader (stripped from the shipped DLL, present
  in Runtime-x64-profile.dll) + the Visitor read path over the existing Serialize bodies + the source-tree
  driver -- as `tools/rsc_compile` in CMake, cross-platform. The start of our own modding toolkit. A planned
  block once two or three extensions need resources.
- SHARPUI in flight with the user's snap decision: when on, the effective UI scale snaps to an integer
  device-pixel ratio (slider untouched), fit-checked for the largest dialogs, clamped to 1x below the fit.

## 2026-10-11 night -- ext.eye_camera DONE: the user's look clean

- The idle drop's OBSERVED cause (`handins/EYECAM.md` §11): EditTool::Update's retail off-display rule
  (0x1400fcebc) closed the citizen panel, and with it the FollowDialog, because the followed citizen
  projects to no screen rect at eye level. One seam in `edittool.cpp` exempts the eye's own citizen
  (`EyeCamera::FollowedEntity()`), `a39611200`. The user: "lgtm no issues" (follow through stops and
  buildings, the four fixes, the starting town on the ground).
- Lesson for the next extension: when a drop or close is intermittent, put the trace in FIRST and let the
  user's run name the cause; EYECAM2 and EYECAM3 fixed real defects but inferred the wrong one twice.
- Next in the user's order: class B pathfinding (`INVENTORY.md` §5.2), then class A sharp UI at high scale
  (§5.3).

## 2026-10-11 night -- EYECAM3 + the fourth fix merged (gate EYECAM3 at 0)

- BARNZ: the floating starter barn is RETAIL's (0x1400a1fc0 samples the corners before the flatten; the
  player path 0x1400fda00 stores z = 0): retail_bugs §4, row 1323, and the fourth fix `fix.start_barn_height`
  (`e696a0633`, default on; applies to every start-town structure).
- EYECAM3 (`fe72a743e`): the followed ENTITY is the identity, read each step from the frontmost
  FollowDialog; hand-back only on no dialog / no citizen / entity removed; `[EyeCam] engage / disengage
  reason=` lines in standalone_error.log. Cause inferred, not reproduced: if the view drops again the
  trace names the test. Row 1324 (CitizenUI::OnClosed skips the FollowDialog close on a marked citizen).
- **Owed**: the user's look (idle follow in a busy town for minutes, then grep `[EyeCam]`; the starting
  town sitting on the ground, no building sunk).

## 2026-10-11 late -- EYECAM2 merged and LOOKED; EYECAM3 + BARNZ dispatched

- EYECAM2 (`10aeeb57c`, gate row EYECAM2 at 0): child eye height from the age scale (confirmed good), shadows
  at eye level through `SunLight::_extShadowFitView` (confirmed good; DESIGN hook rule 5 names that shape),
  the slot-reuse weak ref. The idle drop is only MOSTLY fixed: standing still SOMETIMES still hands the view
  back to retail's follow -- block EYECAM3 enumerates every exit condition, adds an `[EyeCam] disengage
  reason=` stderr trace and fixes the live one.
- The user also saw the STARTER STORAGE BARN floating above uneven terrain on a new game: a faithful-port bug,
  not the extension's -- block BARNZ (root cause against retail's placement order; known_port_issues row).

## 2026-10-11 late -- EYECAM merged: the first extension

- `ext.eye_camera` (class A, default off) merged `a67c8b445`: `src/ext/eye_camera/` (eye_math pure + the
  game glue), three seams (Update step 6, Draw step 4 at the renderer's alpha + near 0.1, the zoom in
  camera_input.cpp), six unit tests; PlayerController's saved fields never written. Gate EYECAM at 0;
  stress faithful vs `/profile:all` identical (0.17 ms / iteration, 140 fps). `tools/stress_run.sh` takes
  `STRESS_PROFILE` (default faithful).
- **LOOKED (the user, 2026-10-11 late)**: engage, the three exit paths, the return to retail's camera and the
  zoom all good. Three defects, block EYECAM2 dispatched: children get the adult eye height; shadows do not
  render at the default distance 0.9 (back at ~3 units); an idle citizen kicks the view back to retail's
  follow until they walk again.
- Was owed: the user's look (`handins/EYECAM.md` §7: 13 checks -- eye height 0.88 units, the follow
  engage / disengage, first person, hidden / removed citizen, faithful load of a save made while following).

The design is `DESIGN.md`; the classified deviation list is `INVENTORY.md`; every block's hand-in is
`handins/<BLOCK>.md`. The gate table for this phase is `docs/runtime_port/phase_d/GATE_TABLE.md`
(rows EXT1 / EXT2).

## 2026-10-11 night -- the phase opened; scaffold, three fix pilots, verification, inventory

- **Merged on main**: EXTSCAFFOLD (`src/ext/registry.*`, `Ext::Fix()` / `Ext::Ext()`, settings keys
  `fix.<id>` / `ext.<id>` / `profile=faithful|all`, the class lock, the pseudo-mod provider type, the
  Extensions dialog + button beside Mods in the front end and the pause menu, 11 unit tests),
  FIXPILOT (`fix.icon_slide` at slot 11 AND `SetFlag`, `fix.residence_goto`, `fix.tavern_counter`;
  default on), SAVETAIL (`handins/SAVETAIL.md`, five verdicts VERIFIED), EXTINVENTORY
  (`INVENTORY.md`: RESTORE 18, FIX 7, EXT-A 1, KEEP 7).
- **Gate EXT2 at 0 problems** (faithful profile forced by `/profile:faithful` on the exe's command
  line: `tools/build.sh run`, `resave_gate.sh`, `phase_d/gate.sh`): new game 5694 / 0, tutorial
  5773 / 12, loadgame 3436 / 0, typetable `965b527a`, resave 3/3 == golden (23,453,453 bytes), unit
  1013 / 0, castmetric 2, acceptance 0 off target, 0 new warnings. EXT1 (one problem, castmetric 6)
  was the dialog's three casts and the residence faithful arm's; fixed in `e2d8be2f4`.
- **Design corrections from SAVETAIL**: a class-C chunk lives at the tail of save block 1, not after
  the file (retail checks the exact file size); every B/C extension writes a manifest there; the
  pseudo-mod's format version goes in ModInfo+0x18 (+0x10 is the package CRC); both write seams emit
  zero bytes under the faithful profile.
- **Facts learned**: a loose text `.rsc` is NOT loaded by the shipped loader and `Tools-x64.exe` is
  not on this machine, so the Extensions dialog reuses `Dialog/Mod.rsc` with its texts replaced and
  draws its icon in code (EXTSCAFFOLD hand-in). The button's tooltip still reads the Mods text.
- **LOOKED at by the user (2026-10-11 night)**: the Extensions dialog's name / author / description
  rows and the enable / disable toggles work in the front end and in game; the row icon column is empty
  (the user will author programmer art later; the slot stays). The three fixes seen working: icon_slide,
  residence_goto, tavern_counter. Fixes are global and live, NOT recorded in the save (a disable + save +
  re-enable + load shows the fix enabled): confirmed as intended by the user.
- **Next**: the first extension, class A eye-level camera (seam in `INVENTORY.md` §2).
