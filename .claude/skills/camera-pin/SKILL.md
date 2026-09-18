---
name: camera-pin
description: Frame a specific building or map location for a screenshot with BANISHED_CAMERA_PIN (world-coordinate math from startTown log lines), and verify rendering changes with tools/trace_oracle instead of pixel diffs. Use for any visual or rendering verification.
---

# Camera pinning and visual verification

## Pixel diffs between runs are meaningless
Pin + seed do NOT give a reproducible frame. Measured: five runs at identical seed, pin
and command line split into two clusters ~98% apart, independent of the flag under test
(two *armed* runs differed by 97.98%; an armed and an unarmed run by 0.79%). Details:
`docs/runtime_port/WAVE23_CLOSED.md` §3. Screenshots are for *looking at*, not *diffing*.

**To compare rendering between runs use `tools/trace_oracle/`**: it compares the
draw-call / pipeline-state command stream, so it needs no frame determinism. Read the
measured noise-floor table in its README before quoting any of its six screens — only
three are verdict-bearing.

## Pinning
`BANISHED_CAMERA_PIN=x,y[,dist]` (`code/Game/Game/camera_input.cpp`) pins the camera focus
to a world position every frame, overriding pan/follow. Without it the autostart camera
roams and lands differently every run. `BANISHED_CAMERA_PIN=town[,dist]` pins the town.

- **World coords**: `world = buildCell − mapDim/2` (small map: cell − 128). For a placed
  structure take the tile from its `[startTown::…] tile=(x,y) fp=WxH` log line and pin the
  footprint center: `x + W/2 − 128, y + H/2 − 128`.
- The 512 terrain grid has a 128-cell non-playable border; build cells map 1:1 onto its
  center. There is NO 2-cells-per-tile scaling.
- `dist` defaults to 40; ~50 frames a 21x17 building with context.
- Flags are read once at boot: relaunch to move the pin. Placement is deterministic per
  seed+flags, so a wrong guess costs one probe run — read the tile from the log, relaunch.

Example (demo quarry):
```
BANISHED_SEED=12345 BANISHED_BUILD_DEMO_QUARRY=1 BANISHED_CAMERA_PIN=9.5,-0.5,50
```
