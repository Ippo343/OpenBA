# DRAW hand-in (PORT GAP hunt wave 2, branch `d4-draw`, base `6430aca6f`, main merged at `53c6165c6`)

Weather::DrawUpdate, EntityUI::Startup / Update and rows 754, 1164, 1247 (GAPHUNT_RULES.md
section 2, the DRAW row). Application-x64.exe only.

**Static gate only.** The game was never run from this worktree. Every route below is owed.

## Gate (after `git merge main`, all three targets rebuilt)

| check | result |
|---|---|
| `standalone-debug`, `standalone-perf`, `unit` (`-j2`) | EXIT 0, EXIT 0, EXIT 0 |
| unit | 1002 / 0 (nothing added) |
| warnings (gate.sh `norm()`, `comm -23` against the main checkout's `runs/warn_baseline_{debug,perf}.txt`) | 0 new (debug: union of every debug log of the block, the first a full build), 0 new (perf: the same) |
| castmetric (`src`) | 2 |
| `tools/abi_audit.py` | OK |
| `b3r_session/acceptance.sh` | 0 line(s) off target |

The SDL submodule was already checked out in the worktree.

## Commits

B11 is perf objects against `b11base_d4e` (main `1e842b306`). That base predates the wave-1
merges, so TUs those merges changed show their own DIFFs; each commit message separates them.

| commit | title (short) | B11 |
|---|---|---|
| `71af34448` | behaviour: `WeatherSimulation::DrawUpdate` = retail 0x1400938d0 | weather_sun IDENTICAL 7 ONLY-BASE 1 (Weather_SunUpdate) ONLY-TIP 3; gameupdateloop DIFF 1 (Draw) IDENTICAL 25; gameupdateloop_load IDENTICAL 24 |
| `3ad9a1b0e` | behaviour: `PlayerToolManager::Startup` = retail 0x140101ee0 (EntityUI::Startup) | entityui IDENTICAL 27 ONLY-TIP 5; gameupdateloop DIFF 2 (Startup, Draw) IDENTICAL 24; the other 25 TUs that include PlayerToolManager.h IDENTICAL (bar the LUT merge's own edittool / tool_base_tile / roadtool_calcpath) |
| `b4221555f` | behaviour: `WeatherSimulation::WeatherSimulation` = retail 0x140093f60 (row 754) | weather_sim ONLY-TIP 2 IDENTICAL 7 RENAMED-IDENTICAL 1 SAME 1; gameupdateloop_load DIFF 1 (Load) ONLY-BASE 1 IDENTICAL 18 SAME 4 |
| `127ada2fc` | behaviour: 0x14002e470's two `s_tidCompEnt &&` dropped (row 1164) | gameupdateloop_serialize DIFF 1 (PlayerToolManager::Serialize) IDENTICAL 20 |
| `912c27fb0` | behaviour: `GameUpdateLoop::SerializeAll` = retail 0x140095a90 (row 1247) | gameupdateloop_serialize ONLY-BASE 2 ONLY-TIP 2 DIFF 2 IDENTICAL 17; gameupdateloop ONLY-BASE 1 DIFF 2 IDENTICAL 23; gameupdateloop_load DIFF 1 IDENTICAL 18 |
| `401a51c76` | behaviour: `PlayerToolManager::Update` = retail 0x1401026a0 (EntityUI::Update) | entityui ONLY-TIP 6 IDENTICAL 27; camera_input ONLY-BASE 1 ONLY-TIP 1 (UpdateHotkeys) IDENTICAL 11; gameupdateloop DIFF 3 (Update, Draw, Startup) ONLY-BASE 1 IDENTICAL 22 |
| `a1e846662` | docs: rows 754 / 1164 / 1247; new row 1296 | docs |
| `e4cbe24af` | merge main (clean: CAI's merge, its gate row, one comment commit) | |
| (this commit) | docs: this hand-in | docs |

SAME in b11pair means the code changed but calls, stores and compares did not.

## Items

| # | item | verdict | what was done |
|---|---|---|---|
| 1 | `Weather::DrawUpdate` 0x1400938d0 (`gameupdateloop.cpp:1375`) | PORTED | It is a `WeatherSimulation` member (`this` = GameUpdateLoop +0x130). Ported with its two helpers, the lighting blend 0x140093ac0 and the EnvironmentSetting lerp 0x1400949f0, into `weather_sun.cpp`. It replaces the port-only `Weather_SunUpdate` and the GUL-side push of the four terrain floats. Differences that change the picture: the colour lerp now runs in retail's packed domain (truncated to 0..255), and the fog colour's fourth channel is the blended fogMax (0.95) instead of 1.0. Dropped guards: the baked keyframe table fallback, `if (!sun)`, `if (wsim)`, `if (envRes)`, `if (!desc)`, and the year-time wrap and fraction clamp. The `BANISHED_FORCE_SNOW` diagnostic stays as an override after the call. |
| 2a | `EntityUI::Startup` 0x140101ee0 (`gameupdateloop.cpp:1859`) | PORTED | It is a `PlayerToolManager` member (GUL +0x120; retail's EntityUI is the 0x48 dialog controller). Body in `Dialog/entityui.cpp`: the eight registry inserts (0x14002dd10 / 0x14002ddb0 / 0x14002df10, retracing from the parent as retail does), the +0x50 reopen, then **new**: the +0x60 pinned entity panels rebuilt (UIDescription dialog, AddDialog, SetUpperLeft, pin box checked, SyncPin), and **new**: SelectTool(0) at the tail. |
| 2b | `EntityUI::Update` 0x1401026a0 (`gameupdateloop.cpp:1058`) | PORTED | It is a `PlayerToolManager` member, body in `Dialog/entityui.cpp`. Its helper 0x1400ff6e0 (`UpdateHotkeys`) lacked retail's tool half, so it is ported faithfully in `Game/camera_input.cpp`, a file no block owns (see below). The GUL caller now has retail's shape. **New**: AutoPause opens the pause menu when the application is paused, the camera-view keys are gated by the tool's OnKey, and the cursor is always the tool's. |
| 3a | row 754 (WeatherSimulation store order) | FIXED | The member order can express it: the class has its real ctor 0x140093f60 with member initialisers in declaration order, which is retail's store order. `WeatherSimulation_Construct` is deleted. |
| 3b | row 1164 (`s_tidCompEnt &&`) | FIXED (leftover) | The row's own site (0x140021ba0) was fixed in B9 (`d1f2280b4`). The same token survived at both sites of the +0x60 set serializer 0x14002e470. Retail tests neither (0x14002e4dd, 0x14002e6cf), so both are dropped. |
| 3c | row 1247 (SerializeAll tests + phantom parameter) | FIXED in part | The three tests are dropped. The phantom reaches further than the row says: SerializeAll reads RCX / RDX only, and SaveRestore 0x1400959c0 never reads R9. So `SerializeAll(stream)` lost the SavedState and the out pointer, and `SaveRestoreOut` folded into `SaveRestore`. Load's log line lost its port-only `(out=%d)`; it still prints `LOAD-FROM-SAVE done`. **OPEN**: the parameter in `CitizenBehavior::Serialize`'s declaration (`Entity/CitizenBehavior.h`, `Game/citizenlist_serialize.cpp`, not DRAW's files). The call passes nullptr. |
| - | `gameupdateloop.cpp` [8] 0x140095970 / [9] 0x140095930 (OnVideoError / OnVideoRestart) | LEFT | Row 890 (device-lost) is DEFERRED by the user. GAPHUNT_RULES section 2 says nobody touches them. |

`PORT GAP` lines in the block's files: at the start, gameupdateloop.cpp:358 (a comment listing the
three), :1058, :1375, :1859, :2347, :2352. None in weather*, entityui*, GameUpdateLoop.h, Weather*.h
or EntityUI.h. Now only the two device-lost slots remain (gameupdateloop.cpp:2261 / :2266).

## Save format and world build

- **DrawUpdate, the ctor, row 1164:** nothing saved changes. **Resave: expect PASS at the current golden.**
- **Row 1247:** the same serializer calls run in the same order in both directions. The dropped
  tests never fire on a healthy run (the AchievementManager and InternalFactory singletons exist, and
  the type id is non-zero). **Expect PASS.** A crash in `AchievementManager::Persist` would be a
  finding: it means a load with no manager, which the old guard hid.
- **Startup:** on a load, every +0x60 node now gets a panel, and `SyncPin` re-inserts the entity
  through `TrackEntity`, which rewrites the node's x / y from the dialog. The +0x60 set is
  serialized (0x14002e470). If `Gate.sav` holds pinned panels and the dialog's position comes back
  scaled, the resaved x / y could differ. A resave DIFF confined to PlayerToolManager's +0x60
  records would be that, not a golden re-cut by default. Startup's `SelectTool(0)` creates the
  EditTool one frame earlier; tools and their settings are not saved.
- **Update:** input only. Nothing saved.
- **World build (`worldcmp.sh`):** no new-game path is touched. Expect no difference.

## Routes for the coordinator (merged exe)

1. **DrawUpdate (lighting, fog).** New game, play at speed through a full year (`sa-run`, then
   fast-forward). The day cycle should look as before, with two expected differences:
   - Distant terrain should stay faintly visible through the fog at the far plane (fogMax 0.95, not
     fully opaque).
   - In winter, temperature at or below 34 enables the snow effect. The scene should take the snow
     Weather's tint as the effect fades in, and lose it as it fades out. In warm weather rain does
     the same, with grey light and nearer fog.

   For a capture: RenderDoc, `Context::SetFog`'s colour constant should carry 0.95 in `.w`.
   `BANISHED_FORCE_SNOW=1` must still whiten the ground.
2. **Startup (pinned panels).** Click a house and tick its pin box, then move the panel. Save, quit
   to menu and load that save. The panel must reopen where it was, pinned. Unpinned panels must
   not reopen. Also run the tutorial route: the tutorial panel (+0x50) must still open.
3. **Update (AutoPause).** With AutoPause on in the options, alt-tab away from the running game. The
   pause menu must open. With AutoPause off, nothing opens.
4. **Update (tools).** On the merged exe:
   - Place a house: the preview follows the mouse and a left click places it.
   - Start a road drag and right-click: the tool cancels back to selection.
   - Pick the removal tool: its cursor shows.
   - Save a camera view and recall it with the camera-view keys while the selection tool is current.
   - Run the tutorial: its restricted tool must arm on each step.
   - Run `harness_route.sh` for the tool and pick routes.
5. **Resave gate.** Expect 3/3 PASS at the current golden. Read the Load log for `LOAD-FROM-SAVE
   done`; the `(out=0)` suffix is gone.

## Needs in other blocks' files (row 1296 records them)

- **`Player/tool_activation_standalone.cpp`:** delete `EntityUI_BuildToolModeTree_SA`,
  `EntityUI_OpenRegisteredDialogs_SA`, `EntityUI_ToolTick_SA` and `EntityUI_ToolDraw_SA`, which have
  no caller now. Comments naming them should go too: `Game/starttown.cpp:2527`,
  `diag/tool_harness.cpp:115 / :336`, `Player/Tool.h:29`, `Entity/entitylist.cpp:765`.
- **`Entity/CitizenBehavior.h`, `Game/citizenlist_serialize.cpp`:** drop `int* outParam` from
  `CitizenBehavior::Serialize` (row 1247). Then drop the `nullptr` at
  `gameupdateloop_serialize.cpp`'s call.
- **`engine/save/saveload_update.cpp`, `engine/io/compressedstream.cpp:139`:** comments still name
  `SaveRestoreOut` or say the load path reads the out pointer. Retail's 0x1400959c0 never reads R9.
- **`Component/EnvironmentDescription.h`:** retail's lerp 0x1400949f0 is a member of the 0x20-byte
  record (`EnvironmentDescription::WeatherEntry`). It is file-local in `weather_sun.cpp` for now.
  The record's field names (skyColor / groundColor / sunIntensity / sunAngle / sunSpread) are
  `Weather.h`'s ambientColor / lightColor / fogMax / sunHeading / sunPitch for the same bytes.

## Files touched outside the DRAW row

- **`Game/PlayerToolManager.h`** (no block owns it): the `Startup` / `Update` declarations, the
  `System::Time` include, and `UpdateHotkeys`' signature. The members had to be declared on their
  class.
- **`Game/camera_input.cpp`** (no block owns it): `UpdateHotkeys` 0x1400ff6e0 replaced by retail's
  body. I asked the coordinator before making the edit and had no answer when Update was the last
  item, as the message said I would. Without the edit, wiring Update would have lost OnKey and the
  right-click cancel, which the standalone tick used to supply.

Shared files appended: `docs/known_port_issues.md` (row 1296; status cells of 754, 1164, 1247).

## Rows

- **Edited:**
  - 754: FIXED.
  - 1164: note appended, the leftover sites.
  - 1247: FIXED in part, the CitizenBehavior parameter OPEN.
- **Added:** 1296 (RECORDED), the stand-ins and stale notes listed above.
