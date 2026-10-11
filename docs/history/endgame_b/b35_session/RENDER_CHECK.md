# B.3.5 rendering verification — trace oracle, pre-step vs step tip

Question answered: **did turning Video/Graphics into real C++ classes change what the
renderer tells the GPU?** Not "do two frames look the same" — a pixel diff between two
runs of this game measures nothing (`tools/trace_oracle/README.md`, Wave 23). This
compares the D3D11 command stream.

No tracked file was modified by this check.

## Arms

| arm | tree | exe | md5 |
|---|---|---|---|
| baseline | `bd483075` (pre-step) | built here in a throwaway worktree, `standalone-debug`, exit 0 | `5fa9dfe0430a19d8c86f0311987f3624` |
| current | `8d72408b` (step tip) | prebuilt and set aside by the coordinator | `eb852e9b672ff2f3c7aea27e54d7f65b` |

The baseline worktree (`.../b35-render/base`, detached at `bd483075`) was built, its exe
copied out, and the worktree removed. `tools/renderdoc_mcp/.venv` is gitignored and absent
from a worktree, so all dumping/oracle work ran from the main checkout.

## Captures

Four captures, each under its own bare `tools/game_dir_lock.sh acquire b35-render 900`
… `release` cycle, one capture per acquire, from the main checkout root:

```
bash tools/trace_oracle/capture_arm.sh <tag> --frame 150 \
     --expect-log '\[AutoStart\] sequence fired' \
     -- BANISHED_AUTOSTART=1 BANISHED_SEED=12345 BANISHED_CAMERA_PIN=0,0,60
```

`capture_arm.sh` greps `standalone_debug.log` **and** `standalone_error.log`, so the
stderr-only `[AutoStart]` line is a valid arm witness; it is printed by any healthy
in-game run and by nothing else.

| capture | exe | capture_arm exit | arm confirmed | .rdc | draws (records) |
|---|---|---|---|---|---|
| `baseA`  | baseline | 0 | yes | 105 MB | **557** |
| `baseA2` | baseline | 0 | yes | 105 MB | **558** |
| `curB`   | current  | 0 | yes | 105 MB | **509** |
| `curB2`  | current  | 0 | yes | 104 MB | **587** |

No capture failed, no crash log, no retry needed. Draw counts 509–587 sit in and just
above the README's measured same-exe spread (566/557/509) — that spread *is* the known
non-determinism, which is why no screen keys on the draw count (`draw counts` is a
negative control in the selftest).

Dumped with `tools/dx11_state_diff/dump_state.py` via
`tools/renderdoc_mcp/.venv/Scripts/python.exe`; each trace also reports
`textures=365 buffers=1147 resources=2176` (baseline `baseA2`: 369/1151/2188).

## Harness soundness on this machine

`trace_selftest.py curB.jsonl` — exit 0:

```
positive controls: 19/19 CAUGHT
negative controls: 9/9 clean
EFFECTIVENESS: every scored mutation changed >=1 field value; 0 inert
open controls: 0
total field values altered across all controls: 10525
per-screen recall S1 17/19  S2 14/19  S3 1/19  S4 2/19  S5 1/19  S6 1/19
```

The instrument is alive here, so its silence on S3/S4/S6 is evidence and not an artefact.

## The three oracle runs

| screen | run 1 `baseA→curB`, control `baseA/baseA2` | run 2 `baseA→curB`, control `curB/curB2` | run 3 (sanity floor) `curB→curB2`, control `baseA/baseA2` |
|---|---|---|---|
| S1 configSet | **49** | **21** | **47** |
| S2 sigSet | **3** | **3** | **3** |
| S3 fieldDiff (floored) | **0** | **0** | **0** |
| S4 invariants | **0** | **0** | **0** |
| S5 srvPool | **25** | **7** | **28** |
| S6 strideSet | **0** | **0** | **0** |

Exit code 1 in all three, which here means only "at least one screen non-zero" and is
produced by the descriptive screens alone.

Supporting numbers:

* aligned pairs — run 1/2: 415 (unaligned 142 baseA, 94 curB); run 3: 437 (72 / 150).
  Field comparisons: 9960 with 1041 differing (runs 1–2), 10488 with 1104 (run 3) — the
  comparison did real work, it was not empty.
* `ds, gs, hs, uav` are NOT-EXAMINED on every trace (0/N populated) in all three runs, on
  both arms: this renderer still binds no hull/domain/geometry shader and no UAV. 20 of
  24 fields carry content, exactly as the README records.
* S3, every verdict-bearing field at `0.0% test / 0.0% floor → ok`: `blend`,
  `cbBindings`, `depthState`, `ibStride`, `layout`, `om`, `ps`, `raster`, `sampler`,
  `scissors`, `srv`, `topology`, `viewports`, `vs`. The `non-verdict` fields
  (`cbBufferSizes` 35.2% vs floor 33.7–37.1%, `cbContents` 100% = floor, `draw`
  82.2% vs 75.5–81.7%, `flags`/`name` 9.2% vs 9.4–9.6%, `vbStrides` 15.2% vs
  14.2–14.6%) are the documented ring-allocation / per-object-transform artefacts and are
  at or below their floors.
* S4 checked 1252 texture-derived invariant triples on `curB` and 1452 on `baseA`
  (1467 on `curB2`); `maxLOD`, `numMips` and `numSlices` each held on **all** of them
  against the bound texture, on both arms.
* S6: 4 distinct vb/ib strides on every trace, all 4 shared, nothing one-sided anywhere.

## VERDICT

**PASS. No finding on any verdict-bearing screen.**

Restricted to the three screens the README licenses as verdict-bearing — S3 floored by a
same-build control pair, S4, S6 — the step's tree is indistinguishable from the pre-step
tree. All three read **0** in run 1, and again in run 2 when the floor is taken from the
*other* arm's control pair, so the zero is not an artefact of which side supplied the
control. There is nothing to list field by field: every field in every verdict-bearing
screen agreed.

S1 (49 / 21), S2 (3 / 3) and S5 (25 / 7) are **descriptive, not findings**:

* README noise band, single control pair floored: **S1 51, S2 3, S5 29**.
* Run 3 measures that band on this machine today with the *same* exe on both sides of the
  test: **S1 47, S2 3, S5 28**.

The real base→current comparison (49/3/25 and 21/3/7) is at or **below** that same-exe
floor on every one of the three. Which materials and textures happened to draw in frame
150 follows what was on screen, and the set of run-dependent keys is itself
run-dependent — a few dozen keys is the game moving, not the port. A port that had
deleted a render pass would have pushed these far outside the band; instead the current
exe differs from the baseline by *less* than it differs from itself.

S2's 3 one-sided signatures appear in all three runs, including same-exe run 3 — exactly
the 3 the README measured between identical runs, and the README also records that the
content classifier explains 0 of those 3, so its `UNEXPLAINED` line is noise here and was
not read as a finding.

## State left behind

* Game dir exe = the **current** step exe, `eb852e9b672ff2f3c7aea27e54d7f65b`.
* `tools/game_dir_lock.sh status` → `[gamelock] free`; 0 mounted `.pkm` mods throughout.
* Baseline worktree removed; `git worktree list` no longer shows it.
* Artefacts (exes, `.jsonl`, build log) under
  `C:/Users/USPC/AppData/Local/Temp/claude/b35-render/`; captures, arm files and per-arm
  logs under `C:/GOG Games/Banished/rdoc_trace/`.
