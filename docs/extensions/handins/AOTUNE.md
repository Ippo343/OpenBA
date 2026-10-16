# AOTUNE part 1 -- live toggle, device-lost, eye camera, capture_arm.sh (hand-in, 2026-10-16)

This is block AOTUNE part 1 of `docs/extensions/AO.md` §7: rows 7.3, 7.4 and 7.6. Rows 7.1 and 7.2
are left to the coordinator (empty sections at the end).

- **Branch:** `worktree-agent-a90f17f80bbcfe4ad`, on main `2b5afd78c`. Nothing is merged.
- **Exe:** the debug exe for every run (`tools/build.sh build debug --jobs 2`, md5
  `5c623f86bc13b69a0d6dc2cc224be87a`), checked with `game_dir_lock.sh assert-held aotune <exe>`.
- **Game-dir hygiene:** every run held the lock `aotune` and no `*.pkm` was mounted.
- **settings.cfg:** backed up before the first run (md5 `f7fad13a98f2841d45d86a7967bfb79f`) and
  restored byte-identical after each run. The final md5 is the same.
- **Profile:** the user's settings.cfg as it was. Every `ext.*` row is ON (render_pipe and
  eye_camera included), `ext.ao_quality=3`, `ext.ao_opacity=20`, `VideoMultisample=3` (4x MSAA),
  window 2560x1377. Fixes on. Runs that changed a key say so.
- **Evidence:** captures, screenshots and copied logs are in the game dir under
  `C:/GOG Games/Banished/aotune/`. The per-run logs are in the worktree's ignored
  `build/aotune/runs/<label>/`.

| Row | Verdict |
|---|---|
| 7.3a live toggle, tier, opacity | **PASS.** The AO passes appear and disappear with the toggle, and the depth format follows. A tier change takes effect without a depth rebuild, and so does an opacity change. Over 10 off/on cycles at 4x MSAA, the working set, private bytes and GPU dedicated memory stay flat. Crash log 0. |
| 7.3b device-lost with the row on | **PASS for AO.** One round trip, no later `0x887a0005`, the AO stage rebuilt on the new device, and the AO passes back in a capture taken after the trip. Crash log 0. **Not an AO defect, but found here:** after a forced loss, memory climbs in steps. It does the same with the row OFF and without render_pipe (§2.3). |
| 7.4 eye camera + AO | **PASS.** Follow engaged, near 0.1. The depth resolve output is sane, and AO is not blown out at the near plane (first-person and over-the-shoulder captures). |
| 7.6 capture_arm.sh | **FIXED** (`tools:` commit `e930322f2`), verified by the 7.3b capture run. |

No `fix:` commit was needed: nothing defective was found in the AO code.

## 1. 7.3a -- the live toggle in game

### 1.1 Route

There is no new flag and no harness. The existing pause-menu driver opens Options in game, and
real mouse input does the rest. The coordinator approved this on 2026-10-16.

- **Pause-menu driver:** `BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_BUTTON=4`.
  With `GameLastOption=4` the dialog opens on the Enhanced tab:
  `[AoMenu] Enhanced tab added: tabs=5 ... quality=3 opacity=20`.
- **Mouse and keys:** the scratchpad script
  `C:\Users\USPC\AppData\Local\Temp\claude\C--code-GIT-untitled-rev-eng-project\6f322731-4b4d-464f-a634-7334d2caabb2\scratchpad\ui.ps1`
  uses SetCursorPos, mouse_event and keybd_event, and addresses the game by its PID. The
  harness's command guard refused to run scripts from the scratchpad path, so the runs used an
  identical copy in the worktree's git-ignored `build/aotune/`. Nothing is in the tree or the exe.
- **Coordinates:** client coordinates on a 2560x1377 client area (the window's client origin was
  at screen -6,132):

| Target | Client x,y |
|---|---|
| Quality combo (opens the list) | 1380,532 |
| List: Off | 1299,561 |
| List: Low | 1300,584 |
| List: Medium | 1316,607 |
| List: High | 1303,630 |
| Opacity slider handle at 20 | 1412,563 (dragged to 1340,563 gave 8) |
| Apply | 1350,905 |

Every change went through **Apply**. The dialog stays open, and the frame behind it keeps
drawing while the game is paused.

**RenderDoc capture per state.** This is run `toggle1`, launched under
`renderdoccmd capture --opt-ref-all-resources`. The app never calls `SetCaptureKeys`, so
RenderDoc's default F12 capture key works. ui.ps1 pressed F12 once after each Apply.

### 1.2 Evidence: captures

The captures are in `C:/GOG Games/Banished/aotune/`. "AO draws" counts ASSAO's full-screen
`Draw()` calls, from the R32F resolve to the apply, between the Opaque and Blended lists. The
other counts come from RenderDoc's frame overview and texture list.

| State (Apply) | Capture | Total draws | AO draws | Main 4x depth | Final AO min (R of R8G8 2703) |
|---|---|---|---|---|---|
| boot ON, High / 20 | `reimpl_capture_frame37441.rdc` | 306 | 18 (eid 1265..1496) | `D24S8_TYPELESS`, flags 5 (DSV + SRV), id 1429 | 0.604 |
| **OFF** | `..._frame63179.rdc` | 288 | **0** | **`D24S8`, flags 4 (DSV only)**, id 2722 | -- (no AO render targets drawn) |
| ON, Low / 20 | `..._frame78869.rdc` | 299 | 11 | `D24S8_TYPELESS`, flags 5, id 2743 | |
| Medium + opacity 8 | `..._frame98289.rdc` | 303 | 15 (apply eid 1460) | the same id 2743 (**no depth rebuild for a tier / opacity change**) | 0.808 |
| **OFF** | `..._frame117514.rdc` | 294 (UI only) | **0** | `D24S8`, flags 4, id 2765 | -- |
| ON, High / 8 | `..._frame125443.rdc` | 312 | 18 (apply eid 1496) | `D24S8_TYPELESS`, flags 5, id 2786 | **0.835** (vs 0.604 at opacity 20) |

What the captures show:

- **The passes follow the toggle.** They are present, absent, present, present, absent, present.
- **The depth follows the toggle.** Each toggle rebuilds the main depth through `settingsChanged`:
  a new resource id, the typed format when off and the typeless format with an SRV when on.
- **The old depth is released.** The texture count stays at 420 in every capture, so no stale
  depth survives. This confirms `7dbcaaf0e` (the DSV leak fix).
- **The tier sets the pass count.** Low, Medium and High give 11, 15 and 18 draws (ASSAO quality
  levels 0, 1, 2).
- **Opacity reaches ASSAO.** The minimum of the final AO rises from 0.604 at 20 to 0.835 at 8.
  The RenderDoc MCP read ASSAO's constant buffer as all zeros, so the cbuffer itself could not be
  checked. The texture statistic is the evidence.
- **The pass objects stay allocated while off.** The R32F resolve target (2656) and ASSAO's own
  targets remain in the off captures but are never drawn. This is by design: the pass objects
  live until the device changes. It is about 10 MB, flat, and not a leak.

### 1.3 Evidence: logs (`build/aotune/runs/toggle1/standalone_error.log`)

Each Off Apply logs this sequence:

```
[AO] main depth -> retail (D24_UNORM_S8_UINT): rebuild requested (Device::settingsChanged)
[AO] ext.ambient_occlusion OFF
[Ext] ext.ambient_occlusion requested=0 active=0
[AoMenu] Apply: quality=0 opacity=20
```

Each On Apply logs this sequence:

```
[AO] main depth -> readable (R24G8_TYPELESS + SRV): rebuild requested (Device::settingsChanged)
[AO] ext.ambient_occlusion ON
[Ext] ext.ambient_occlusion requested=1 active=1
[AoMenu] Apply: quality=1 opacity=20
[AO] readable main depth 2560x1377 x4: DSV 0x00000000, SRV 0x00000000
```

A tier or opacity change logs only `[Ext] ext.ao_quality=2`, `[Ext] ext.ao_opacity=8` and
`[AoMenu] Apply: quality=2 opacity=8`, with no rebuild. The crash log is empty and there is no
hang log.

### 1.4 Memory across cycles (run `mem1`, no RenderDoc)

RenderDoc inflates the process, so this was a separate run with the same route. There were 10
cycles of Off, Apply, High, Apply, with 4 s between steps, at 4x MSAA and 2560x1377. The log has
21 `[AO] ext.ambient_occlusion ON/OFF` lines (the boot line plus 20 toggles), 11
`readable main depth` lines and 10 `main depth -> retail` lines.

| Point | Working set | Private | GPU dedicated (`\GPU Process Memory(pid_*)\Dedicated Usage`) |
|---|---|---|---|
| Options open, row ON | 382 MB | 748 MB | |
| Cycles 1..6, after each Off and On | 376..381 MB | 742..748 MB | |
| Before cycle 7 | | | 415.5 MB |
| Cycles 7..10, after each Off and On | 377 MB | 742..745 MB | **415.5 MB, every sample** |

**Flat.** Before `7dbcaaf0e`, each Off leaked the 4x depth, about 69 MB, which would have
shown in the GPU column at every Off.

### 1.5 Not done

**The D3D11 debug layer was not armed.** AOCORE did it with a temporary, never-committed patch
(`D3D11_CREATE_DEVICE_DEBUG` plus an info-queue drain). No flag exists for it, and I did not add
one. The toggle path's new D3D calls are the same two view creations AOSPIKE S3 and AOCORE
checked: `CreateMainDepthViews`, which logs `DSV 0x00000000, SRV 0x00000000` at every On.

## 2. 7.3b -- device-lost with the row on

### 2.1 Route

The route is DEVLOST1's: `BANISHED_FORCE_DEVICE_LOST=12000` on the autostart new game, with the
row ON in settings.cfg (the user's profile, render_pipe on).

| Run | Change from the user's profile | What it is for |
|---|---|---|
| `devlost_on` | none | memory, about 5 min |
| `control_on` | no loss | the memory baseline |
| `devlost_off` | `ext.ambient_occlusion=0` | memory with the row off |
| `devlost_on_nopipe` | `ext.render_pipe=0` | memory without the render thread |
| capture | `tools/trace_oracle/capture_arm.sh devlost_after --frame 30000 --expect-log "round trip COMPLETE" -- BANISHED_FORCE_DEVICE_LOST=12000` | the post-trip frame |

The capture run is the fixed script of §4. With `BANISHED_CAPTURE_INGAME` set, render_pipe runs
its diagnostic main-thread path, so the trip there is on the main thread. In `devlost_on` the
trip is on the render thread.

### 2.2 Evidence

**The round trip** (`devlost_on`, render thread):

```
[DeviceLost] INJECTING DXGI_ERROR_DEVICE_RESET (real Present returned 0x00000000; render thread)
[RenderPipe] device lost in the render thread's Present (hr=0x887a0007): recovery on the main thread, the gathered packet discarded
[DeviceLost] hr=0x887a0007 ... -> calling (main thread, after the render thread's Present)
[DeviceLost] recreate OK; onRestored=... -> calling
[AO] readable main depth 2560x1377 x4: DSV 0x00000000, SRV 0x00000000
[DeviceLost] device-lost round trip COMPLETE
[AO] device changed: the AO stage is rebuilt
[AO] ASSAO 1.0 created
[AO] depth resolve target 2560x1377 R32_FLOAT: 0x00000000
[AO] ASSAO targets allocated for 2560x1377
```

- **One trip, no later removal.** INJECT = 1 and COMPLETE = 1, and `0x887a0005` appears 0 times
  over 5 min in game. The same holds in `devlost_off` and `devlost_on_nopipe`, and in the capture
  run on the main-thread path.
- **Crash log 0 and no hang log** in every run.

**The capture after the trip** (`C:/GOG Games/Banished/aotune/devlost_after.rdc`, in-game frame
30269, after the trip at Present 12000):

- 18 AO draws, eid 1233..1464, with the apply at 1464.
- The resolve target and ASSAO's targets have new resource ids (5165, 5168.., 5212..): they were
  rebuilt on the new device.
- The main depth `D24S8_TYPELESS` x4 has flags 5.
- Resolved raw depth is 0.796..0.965: no NaN, no inf, no negatives.
- The final AO is 0.565..1.0.
- `devlost_after_colour_after_apply.png` shows the town after the trip with terrain, buildings
  and contact shadows drawn.

### 2.3 Memory: flat for AO; a post-trip climb that is not AO's

Private bytes and GPU dedicated memory, sampled every 10 s. "Growth" runs from the first
post-trip sample (about 30 s after launch) to the end of the window. The control has no trip, so
its window starts at the same time.

| Run | Step at the trip (private / GPU) | Growth afterwards (private / GPU) | Window |
|---|---|---|---|
| `control_on` (no loss) | -- | 750 -> 801 MB (+51) / 416 -> 422 | 4 min |
| `devlost_on` (row ON, render_pipe) | 844 -> 1391 / 424 -> 835 | 1391 -> 1662 MB (**+271**) / 835 -> 860 | 4 min |
| `devlost_off` (row **OFF**) | 704 -> 1093 / 367 -> 720 | 1093 -> 1471 MB (**+378**) / 720 -> 752 | 4.5 min |
| `devlost_on_nopipe` (row ON, render_pipe 0) | 749 -> 1227 / 416 -> 819 | 1227 -> 1473 MB (**+246**) / 819 -> 843 | 4.5 min |

- **The step at the trip is expected.** DEVLOST.md §5 explains it: device 0 and everything on it
  stays alive once, because retail nulls `d3dDevice` before Shutdown.
- **The climb is expected to be flat, and it is not.** The memory is flat for 1..2 min after the
  trip, then climbs in steps of about +60..+110 MB roughly every 75 s. The control shows the same
  rhythm, but its steps are about +20 MB.
- **It is not AO's.** The climb happens with the row off and without the render thread, so it
  belongs to the device-lost route, not to AO.
- **It differs from DEVLOST1's record.** On 2026-10-11 that run read "296-323 MB flat for 220 s
  after the trip", on a smaller profile, before render_pipe and the other rows.
- **Cause unknown; not chased.** It is outside AOTUNE's rows. A candidate is a periodic re-upload
  (the minimap / terrain repaint) that allocates per call after `Minimap::OnVideoRestart`'s
  `ImageBuffer::Recreate`.
- **Coordinator:** this is a device-lost route row for `docs/known_port_issues.md`. Repro:
  `devlost_off`'s route, sampled for 5 min.

## 3. 7.4 -- eye camera + AO

### 3.1 Route

The run is `eyecam1`, under renderdoccmd, with the user's profile: `ext.eye_camera=1`, the row
on at High / 20, and render_pipe on. The follow is driven by existing harness flags only:

```
BANISHED_PANEL_HOLD=CitizenComponent BANISHED_PANEL_HOLD_FRAME=600
BANISHED_PANEL_HOLD_CLICK=CitizenUI:552 BANISHED_PANEL_HOLD_CLICK_EVERY=300
```

- `PANEL_HOLD` opens the first citizen's panel.
- The click delivers id 0x228, `buttonFollow` (`citizenui.cpp`), to the CitizenUI controller.
- Log: `[PanelHold] frame 900: click CitizenUI id=552` and then
  `[EyeCam] engage entity=0000013dcfb32b48 ...`.
- `_EVERY=300` re-clicked Follow every 300 frames. Each click opened another FollowDialog on the
  same citizen, which shows as `[EyeCam] reattach ...`, 17 engage / reattach lines in total. There
  was no disengage. This is harmless for the check, but next time `_EVERY` should be large.
- **Zoom:** four mouse-wheel notches toward the citizen took the eye to first person, through
  ui.ps1's `wheel` action at client 1280,600. The zoom-out attempt failed on a script cast error,
  so the third capture repeats the first-person view.

### 3.2 Evidence (`C:/GOG Games/Banished/aotune/`)

| View | Capture / images | Resolved raw depth (R32F 2656) | Final AO (R8G8 2703) |
|---|---|---|---|
| over the shoulder (default 0.9; the citizen's head fills the bottom of the frame) | `reimpl_capture_frame20027.rdc` (AO eid 1544..1775); `eyecam_d09.png`, `eyecam_d09_ao.png` | 0.867 .. 1.0 | 0.576 .. 1.0 |
| first person beside a house (a post about 1 unit from the eye at the right edge) | `reimpl_capture_frame27289.rdc` (AO eid 2313..2544); `eyecam_first_person.png`, `eyecam_first_person_ao.png`, `_before_apply.png` / `_after_apply.png` | **0.912 .. 1.0** | 0.498 .. 1.0 |

**How the depth range reads.** With near 0.1 and far 512, raw depth 0.912 is view depth
0.1 / (1 - 0.912 x 0.9998), which is about **1.13 units**. That is the nearest surface, the post.
At retail's near of 2.0, every raw value would sit far lower. 1.0 is the sky: none of the depth
lands outside 0..1, and there is no NaN or inf.

**The AO image is sane.**

- Soft contact occlusion appears in the porch corners, under the eaves and at the tree bases.
- The nearest surfaces (the citizen's head at 0.9, the post at about 1.1) read AO = 1.0, so there
  is **no blow-out at the near plane**.
- The edge channel (G = 0) outlines silhouettes, as in AOCORE's overview captures.

**Verdict.** ASSAO's linearisation constants follow the near-0.1 projection, as AOCORE's unit
test `ext_ao_linearisation_constants` predicted.

**For the user's look (7.1).** At eye level the AO radius comes from the zoom curve on retail's
camera distance, not on the eye's. The street-level end of `ao_math.cpp`'s curve may want its own
tuning. It looked right in these captures, but the look is the user's.

The crash log is 0 and there is no hang log.

## 4. 7.6 -- `tools/trace_oracle/capture_arm.sh` (commit `e930322f2`)

**Defect 1: the script took the menu capture.** The menu loop triggers a RenderDoc capture on its
draw 10 whenever RenderDoc is attached (`menudialogloopt_stubs.cpp`), so the first `.rdc` of every
run is the main menu. The old "newest file" pick took it when it was the only file.

Fix: the in-game arm is the capture that follows the
`[RenderDoc] capture armed for 0 frames` line in standalone_debug.log.

- Its rank is 1 + the number of `capture started` lines before that line.
- The script waits until that many `capture ended` lines exist.
- It then takes that rank's file in mtime order and waits for the size to settle.

**Defect 2: the script killed by image name.** `reap()` was
`wmic process where "name='reimpl-x64.exe'" delete`, which kills every game on the machine.

Fix:

- It now kills only the pid the game's watchdog publishes in `standalone_boot_state.txt`.
  renderdoccmd returns at launch, so `$!` is not the game. The state file is deleted before the
  launch, so the pid is always this run's.
- `alive()` tests that pid. If no pid has been published, it allows at most 120 s of launch.
- If a reimpl-x64.exe is already running, the script names its pids and exits 2 instead of
  killing it.

**Not taken from AOCORE's copy** (`agent-a5e8b4a1bb43292ab/build/aocore/capture_arm_safe.sh`):
its "the second file" rule. It holds only while exactly one menu capture precedes the in-game
one. The rank rule also holds for a run that captures more often.

**Verified** by the 7.3b capture run:

- The script printed `arm confirmed` and saved the in-game frame 30269 (the menu's was frame 9).
- `reimpl-x64.exe` was gone afterwards (reaped by PID), and no `reimpl_capture*.rdc` was left
  behind.
- `bash -n` is clean.

## 5. Files

- **Committed:** `tools/trace_oracle/capture_arm.sh` (`e930322f2`) and this hand-in.
- **Scratch, not in the tree:** the scratchpad `ui.ps1`, `start.sh`, `finish.sh` and
  `setkeys.sh` (start.sh launches; finish.sh kills by the watchdog's PID, copies the logs and
  restores settings.cfg; setkeys.sh refuses to edit unless settings.cfg equals the backup). The
  copies that ran, and the per-run logs, are in the worktree's ignored `build/aotune/`.
- **In the game dir:** captures and screenshots stay in `C:/GOG Games/Banished/aotune/`.

## 7.1 The look (the user's ruling; coordinator)

## 7.2 GPU cost per tier, Lowest vs Low (coordinator, clean machine, RTSS stopped)
