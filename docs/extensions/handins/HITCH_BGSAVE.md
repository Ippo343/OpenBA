# HITCH_BGSAVE hand-in (2026-10-18)

Branch `bgsave`, based on `hitch` `320badbea`. The task (the user's decision): close the list of
paths `ext.background_save` (HITCH_SAVESNAP) had not exercised, so the row can ship as a
trustworthy class A extension. **Result: every scenario passes, after two real defects were
fixed.** A window close during the frozen frames freed the world under the worker, and the
device-lost route raced the worker on the External path refcounts.

| Commit | What |
|---|---|
| `e35a4587c` ext | the Shutdown join, the device-lost barrier seam, the toggle rule, and the diagnostics below |
| `9cef01ce1` ext | the verify line carries the image hash; the destructor's join leaves the freed SaveLoad alone |
| `1e75c54f1` probe | the temporary refcount / factory probe (section 3) |
| `6053fb445` | its revert (nothing of the probe stays in the tree) |
| (this commit) docs | this hand-in |

All runs: perf exe of this branch unless a row says debug, `BANISHED_MENU_SCENE=0`, GameAutoPause
off. "Faithful" is `/profile:faithful`. The extension legs use the per-entry profile with every
other `ext.*` and `fix.*` off. The run logs are under the session scratchpad, not in the tree.

## 1. The defects found and fixed

**Quit while the worker runs freed the world under it.** `Engine::MainLoop::Run` calls the loop's
`Shutdown` before deleting it, and `GameUpdateLoop::Shutdown` frees the world. The prototype
joined only in the destructor, which is too late. A window close is pumped by
`MainLoop::Frame` during the frozen frames like any other message.
- **Fix.** `GameUpdateLoop::Shutdown` opens with `BackgroundSave::Join(true)`. It waits for the
  worker and finishes UpdateSave's state 1 (the name, the blocks, the enqueue), as retail's
  one-Update save always has by the time a quit is pumped. It then waits for the persistence
  worker's write (status 1), because the teardown frees the SaveLoad and its embedded metadata
  block, which the write reads. The destructor keeps `Join(false)`, which never touches the
  SaveLoad: Shutdown freed it and does not null the pointer.
- **Negative control** (the Shutdown join commented out, same quit): no crash in the run itself.
  The destructor's late join reported a 0.03 ms wait, so the worker had read the world while
  Shutdown freed it. **The file it wrote is corrupt: loading it is an ACCESS_VIOLATION** (READ of
  0, `OpenBA.exe+0xBCB46`, exit 3).

**The device-lost route raced the worker.**
- **What the route does.** `MainLoop::OnVideoError` walks every External and copies its path
  String. It destroys the video Externals. Through the loop it releases the terrain's height image
  and the minimap's images, and the restore reloads resources. All of this happens with the
  ExternalFactory still in reference mode.
- **What the worker does.** It copies the same path Strings (the reference-id lookup's first ask
  per External) and serialises the world geometry and the minimap.
- **Fix.** A new null engine seam, `Engine::MainLoop::_extVideoErrorBarrier`, is called first
  thing in `OnVideoError`. The extension points it at a join of the worker. The next Update's poll
  still finishes the save.
- **Negative control** (barrier off): two runs gave no crash and verify IDENTICAL. The device
  recreate is slow, so the worker finished before the images were touched. The probe shows the
  race is real (section 3): without the barrier the route touched 240,098 refcounts on 8,848
  Strings while the worker ran, and **all 741 Strings the worker touched were among them**.
  With the barrier the count is 0.

**The toggle.** The dialog cannot meet a running worker: the UI's Update does not run during the
frozen frames. A programmatic toggle was already mostly safe. `Sync` now states the rule: a
toggle off stops new saves taking the worker but leaves the poll and the barrier installed until
the running save has finished, so it ends on the path it started.

## 2. The scenario table

The **verify** is the in-process proof. `BANISHED_STRESS_COMPPROF=4` serialises the save
synchronously at the save point, and the background image is compared byte for byte after the
join. That level now also arms outside the stress town. Two processes cannot be compared: two
faithful runs of the same load save different states after 300 frames (raw hashes `7251bc15…`
and `a6231ec2…`), so cross-process byte pairs exist only for a save with no sim tick before it,
which the resave gate is.

The **reload** check loads the written file again (faithful, roundtrip mode). It hashes the
decompressed block it read and compares that with the save's own hash: the save-test harness's
new identity line, or the verify line.

| # | Scenario | Faithful | `background_save` | `background_save` + `async_save_compress` |
|---|---|---|---|---|
| 1a | save right after a load (Stress50, frame 300) | pass | verify IDENTICAL (24,364,937 B, 8 frozen Updates) | verify IDENTICAL (24,384,825 B, 6 frozen) |
| 1b | a second save in the same process (frame 954; the file 1a and 1b leave is this save's) | pass; reload = save 2's hash | IDENTICAL, toggle exercise on; reload = save 2's hash | IDENTICAL, toggle exercise on; reload = save 2's hash |
| 1c | new game, quit to menu, new game, save (world 2, frame ~1020) | pass; reload = save | IDENTICAL both saves; reload = save | IDENTICAL both saves; reload = save |
| 1d | WM_CLOSE posted the moment the save starts | the save completes, the quit follows; reload = save | **was broken (section 1)**; now Shutdown waited 65.9 ms, write status 1, exit 0; reload = verify hash | waited 36.7 ms, write status 1, exit 0; reload = verify hash |
| 2 | the row toggled off on the first frozen Update, on after the join | n/a | IDENTICAL; the save finished on the worker | IDENTICAL |
| 3 | camera moving every frame (`CAMERA_PIN=0,0,60,0,sweep`) | n/a | IDENTICAL both saves; the real floats unchanged until the copy-back | IDENTICAL, with `render_pipe` on too |
| 3c | negative control: the frozen camera on the real tool manager | n/a | **verify DIFF at byte 23,177,917**, the tool manager's floats, so the check has teeth | |
| 4 | device reset injected on the first Present after the save starts | n/a | barrier waited 68.8 ms, round trip COMPLETE, IDENTICAL | `render_pipe` Present path: waited 40.6 ms, COMPLETE, IDENTICAL |
| 5 | debug flavour, full redzone shim (16-byte guards, full scan every 200k ops), new game, 3 saves, camera sweep | n/a | | IDENTICAL x3, 33-36 frozen Updates each, no redzone report, no assertion, exit 0 |

**Camera evidence** (scenario 3, `background_save`, save 1). At the start the floats +3c / +40 /
+44 were 45.717 / -46.929 / 0.618. After 8 frozen Updates the real floats were unchanged and the
shadow's were 40.815 / -42.713 / 0.700. The shadow's became the real ones at the join, so the
camera ends where the moving input put it. Yaw (PlayerController, serialised in the head) went
from 200.7 to 508.7 between the two saves.

**Window resize** (driven 2026-10-18, after the merge). The resize path
(`ContextData::BeginCommands` -> `Dx11_DeviceData_Resize`) touches only the swap chain, the
colour and depth buffers and the dynamic buffers. It makes no ExternalFactory call and touches no
serialised object, so the worker and a resize do not meet. The run that shows it: the debug exe,
the new defaults (every `ext.*` / `fix.*` key removed from settings.cfg, so `background_save` and
`async_save_compress` on by default), `BANISHED_AUTOLOAD=Stress50 BANISHED_SAVE_TEST=300
BANISHED_SAVE_TEST_REPEAT=300 BANISHED_STRESS_COMPPROF=4`, and an outside PowerShell loop calling
`SetWindowPos` on the game window every 40 ms through four sizes (no new flag). With the device's
resize trace cap raised locally for the run (not committed), every save's frozen window held 2-3
applied swap-chain resizes:

| save | frozen Updates | `ResizeTarget` calls inside the window | verify |
|---|---|---|---|
| 1 | 9 | 2 | IDENTICAL |
| 2 | 8 | 3 | IDENTICAL |
| 3 | 9 | 2 | IDENTICAL |
| 4 | 8 | 3 | IDENTICAL |
| 5 | 8 | 2 | IDENTICAL |
| 6 | 8 | 2 | not read (the run was stopped after five); its file is the one reloaded below |

581 `ResizeTarget` calls in the run, every one S_OK; no crash or hang log. An earlier untraced run
(1051 resizes) verified IDENTICAL x5 as well. The reload: `BANISHED_AUTOLOAD=Auto
BANISHED_SAVE_TEST=roundtrip` read back compressed 10,245,867 / raw 24,396,372 bytes, and the raw
image's FNV-1a is `83e5522b26ccf80e`, the last save's own `[SaveTest]` rawHash.

## 3. The refcount audit

Retail's `INC [data-4]` is not atomic, so a String whose refcount both threads change can lose an
update.

**What the worker touches.** A static read of the save direction finds one kind of refcount edit
on the worker: `GetReferenceIdRetail` copies an External's path twice on the first ask per
External. That is about 925 Externals; the memo answers the other 185,000 asks. `String::Serialize`
only reads on the save direction, and entity-owned Strings (names, event text, the tutorial's)
are only read.

**The probe** (`1e75c54f1`, reverted) hooked every `StringRefCount(char*)`, plus
`InternalFactory::AddObject` / `RemoveObject` and `ExternalFactory::FindOrLoad`, while the worker
ran. Stress50 load, camera sweep, `render_pipe` on:

| leg | worker refcount ops (Strings) | main thread (Strings) | shared with the worker | main-thread factory ops |
|---|---|---|---|---|
| save 1 | 2,964 (741, all resource paths) | 0 | 0 | 0 |
| save 2 (toggle exercise) | 2,964 (741) | 12 (3, the toggle's own settings writes) | 0 | 0 |
| device lost, barrier on | 2,984 (746) | 0 | 0 | 0 |
| device lost, **barrier off** | 2,964 (741) | **240,098 (8,848)** | **741** | 0 |

**Verdict.** On the normal path the sets are disjoint, and nothing on the main thread changes a
refcount the worker touches. The one overlap was the device-lost route, which the barrier now
serialises. No other thread touched a String while the worker ran. The main thread made no
InternalFactory add or remove (a swap-remove would renumber the factory indices the worker
writes) and no resource load.

The probe covers every `StringRefCount(char*)` site. It does not cover open-coded `*(int*)(p-4)`
edits. The only one on a save-adjacent path is the load-only reference preload.

## 4. The save-frame rows

Stress town, speed 50, save at frame 6000, 7000 frames, machine load CLEAN on all three legs.

| leg | main-thread save ms | worker ms | frozen Updates | sim frozen (wall ms) | hitch row at the save |
|---|---|---|---|---|---|
| `20261018_151529_bgsave_faithful` | 72.83 (SaveRestore) | | | | frame 6001: period 81.81, **excess 73.45** |
| `20261018_152010_bgsave_bg` | 3.31 | 85.56 | 6 | 93.3 | none |
| `20261018_151749_bgsave_bgasync` | 2.28 | 48.41 | 4 | 59.3 | none |

The other hitch rows are the same in every leg and none is near the save. They are the harness's
frame-600 staffing row and one entity-list row at frames 2785-3080, plus one frame-4200 harness
row in the bgasync leg. The save completes at frame 6055 (bg) and 6057 (bgasync), against 6061
for faithful. The 1 s spinner minimum still holds.

## 5. The gates

| check | result |
|---|---|
| resave gate, faithful, `GATE_EXE=build/cmake-perf/OpenBA.exe` | PASS; the golden is byte-identical to the main checkout's (md5 `971aaec2cf95`), compressed resave hash `4127020a2ae682eb` |
| unit (`tools/build.sh unit perf`) | 1173 tests, 0 failures, 3 skipped (the kit cross-checks) |
| acceptance | 0 lines off target |
| debug build | builds; scenario 5 |

The faithful path pays one null test in `OnVideoError`, in addition to HITCH_SAVESNAP's tests.

## 6. The diagnostics added

All are existing flags; `src/app/options.h` documents each new value.
- **`BANISHED_CAMERA_PIN=<x,y|town>,<dist>,<yaw>,sweep`.** The focus stays pinned. The yaw turns at
  40 deg/s, the zoom target swings by 25 and the manual pitch sweeps 0..1. These are the fields the
  keys write, every UpdateCamera call, including the frozen ones on the shadow.
- **`BANISHED_FORCE_DEVICE_LOST=bgsave`.** The reset is injected on the first Present after a
  background save starts its worker (`src/diag/device_lost_drive.h`). Both injectors honour it:
  ContextData and render_pipe.
- **`BANISHED_STRESS_COMPPROF=4` without `BANISHED_STRESS`.** It arms the background save's verify
  for load and new-game routes. Every second verified save also runs the toggle exercise.
- **The save-test harness's identity line**: `[SaveTest] frame N: saved state compressed=… hash=…
  raw=… rawHash=…`. It is not printed in a stress run, where the decompress would be a hitch row.
- **The extension's own lines.** One `[BackgroundSave]` line at start and one at done (the camera
  floats), the barrier's wait, the quit-time join, and the verify line with the image hash.

## 7. Left open

- **The barrier's negative control did not fail by itself.** The probe is the evidence for it.
- ~~No resize harness.~~ Driven 2026-10-18 from outside the process (section 2): IDENTICAL x5, reload exact.
- **Retail's own race is kept.** A quit within the 1 s spinner after any save frees the embedded
  metadata block while the persistence worker may still be writing it. This is retail's code and
  is unchanged. The extension's quit path waits for the write; the faithful path does not.
- **No row icon.** The sheet is full, as noted in HITCH_SAVESNAP.
