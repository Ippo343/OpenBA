# HITCH -- the stutter hunt in dense towns at 10x / 20x / 50x (2026-10-18)

The user's brief: late-game saves must not stutter at 20x and 50x. Acceptance: **no frame longer
than one 140 fps frame (7.14 ms) over its baseline**. Absolute 7.14 ms is unreachable at speed 50
(one Update of 50 sim iterations is ~8 ms in the 800-citizen town, ~10 ms at 1200), so the
criterion is the excess of a Draw-to-Draw period over the rolling median of its class
(draw-only period vs Update period; k Updates = median + (k-1) x the median Update). The
coordinator's session; three opus agents in worktrees (pathgraph, pfburst, postdrain) plus one
for the town (food). Everything below is on branch `hitch`.

## 1. The instrument (perf/README.md "Hitches", `src/diag/stress_harness.cpp` HITCH)

The stress CSV averaged frame time per 600 frames; a 24 ms frame once every 1800 frames moved
`frame_ms_mean` by 0.01 and was invisible. The harness now closes every Draw-to-Draw period, and
every period more than `BANISHED_STRESS_HITCH_MS` (default 7.14) over its class baseline becomes a
row of `standalone_stress_hitch.csv`: the split (update / draw / outside / `harness_ms`), the six
SIMPROF phases, the main-thread pathfinding brackets, the class medians of each, the Update count,
markers, and the `[LoadTime]` stamps so `tools/hitch_report.py` can pull the sampler's lines that fell
inside a hitch and rank the functions enriched there. `stress_run.sh` runs the report (`== hitches`).
Two instrument defects found on the way: the sampler's 1 ms waitable timer ticked at 15.6 ms (every
earlier sample report was ~15x sparser than its "1000 Hz"; now the high-resolution timer), and the
stress town starved at ~100k iterations whatever `BANISHED_STRESS_FOOD_MULT` said (the four food
barns cap the seed at ~23k units): `FoodPass` tops storage up to 15 units per citizen every 60 frames
(branch food, e062ddc5e; the town now grows 803 -> ~1190 over 12000 frames at 50x).

## 2. What was found (fed town, 12000 frames = 600k iterations at 50x; 120k at 10x)

| class | where | size | at 10x | at 50x | verdict |
|---|---|---|---|---|---|
| H1 PathGraph drain | CitizenList phase, every 300 sim-s (1800 frames at 10x, 360 at 50x) | 25 ms at 800 citizens -> 55 ms at 1180 | yes | yes | **FIXED, faithful** (HITCH_PATHGRAPH.md): two port divergences -- AssignCitizens priced every idle citizen x workplace with an exact Map::PathDistance where retail reads the region-pair tree; BuildPathGraph kept 5 of 278 residences. Drain now 1.7-2.8 ms; no CitizenList row left |
| H4 cold-wave scans | EntityList phase, 9-21 ms of Map::PathDistance in one frame, once per winter wave | 13-22 ms | not reached in 120k iterations | 3 waves | retail's own work (one PathDistance per residence per cold citizen). **ext.spread_warmth** (class B, OFF; HITCH_PFBURST.md): per-frame budget, a deferred citizen seeks warmth ~75 ms late. With it on: no row |
| H5 "post-drain" burst | EntityList, frames 1081-1083 and ~4351 | 7-12 ms | no | yes | **FIXED, faithful** (HITCH_POSTDRAIN.md): not the drain -- a synchronized birth wave (400 couples housed on tick 1 give birth at 900 s) and the world-gen herds all doubling on one tick (retail: the herd ctor zeroes spawnTimer), and the cost was OUR stderr traces: stderr is unbuffered and MinGW fprintf writes one character per WriteFile (a 90-char line ~245 us). `ErrLine` (one write per line) in the six burst files; the sim work itself is 1-4 ms |
| spawn burst | EntityList, the first second (800 citizens' first decisions) | 13-21 ms | yes (frames 44-105) | yes (15-34) | mostly the [HomeInvalid] region-grid trace (two 25-line maps, ~7 ms each): gone with ErrLine; the rest is the stress town's 800-at-once spawn |
| harness | the CSV row (census, compprof, fprintf) 22-44 ms; the orchard / crop-field force-start at frames 180 / 300 (dialog open + stderr Trace) | | yes | yes | bracketed as `harness_ms`, excluded from the verdict |

After H1 + H4: the 10x run has no sim hitch row; the 50x run keeps H5 and a few 7-10 ms frames where
the restored 7-pairs-per-tick path-graph spread (350 region searches per frame at 50x, retail's
cadence) lands on an EntityList bump.

## 3. Runs (perf/results/, main checkout unless noted)

| run | tree | speed / frames | hitch rows (non-harness) |
|---|---|---|---|
| 20261018_015253_hitch_fed_s50_f12000 | recorder + food | 50 / 12000 | 42: H1 32 (to 55 ms), H4 4, spawn 3, 180/300 |
| 20261018_015934_hitch_fed_s10_f12000 | recorder + food | 10 / 12000 | 11: H1 6 (24-32 ms), spawn 3, 180/300 |
| 20261018_024518_hitch_pg_s50_f12000 | + pathgraph | 50 / 12000 | 12: H4 + H5 + spawn + 180/300; H1 gone |
| 20261018_032847_hitch_all_s50_f12000 | + spread_warmth (off) | 50 / 12000 | 14: H4 3 (to 20.7 ms), H5 4 (7-9 ms), spawn 3, 180/300 |
| 20261018_033233_hitch_all_s50_f12000_warmth | same, `STRESS_EXT=spread_warmth` | 50 / 12000 | 12: H4 gone; H5 + spread 7-10 ms, spawn, 180/300 |
| 20261018_034748_hitch_all_s10_f12000 | same | 10 / 12000 | 5: spawn 3 (13 ms), 180/300 -- no sim class left |

Gates on the merged tree: unit 1167 / 0 / 0 (after `tools/ext_resources.sh`), resave gate PASS
(the gate's 600 frames never reach a drain, so it cannot see H1's `behaviour:` change), acceptance 0.
Machine load was BUSY on most runs (agents compiling); hitch magnitudes are robust to it, the
per-iteration comparisons are quoted with it.

## 3b. Close-out (the fully merged `hitch`, exe f1210152, machine CLEAN, 2026-10-18 evening)

| leg | rows over the bar (non-harness) | verdict |
|---|---|---|
| 50x faithful, 12000 frames | 2: the cold-wave scans at 3238 / 3239 (retail's work; ext.spread_warmth's) | by design |
| 50x, `spread_warmth,drain_split,background_save,async_save_compress`, autosave at frame 6000 | none; the save frame has no row | **PASS** |
| 10x faithful, 12000 frames | none | **PASS** |

Unit 1173 / 0 / 0 (after `tools/ext_resources.sh`), resave gate PASS on the perf exe (compressed hash
4127020a2ae682eb), acceptance 0. Merged in order: food, pathgraph, pfburst, postdrain (ErrLine), stderr
(one_write_stdio.h), pairspread (PathLength memo + PathCostIndex), savehitch (GetReferenceId memo,
Write fast path, ext.async_save_compress, the gate's GATE_EXE), drainsplit (ext.drain_split, class B),
savesnap + bgsave (ext.background_save, class A: the save serialised on a worker while the sim is frozen
and the screen keeps drawing; quit / device-lost / toggle / camera / debug / refcounts closed), icons
(the Extensions sheet 512 x 512). Hand-ins: HITCH_PATHGRAPH, HITCH_PFBURST, HITCH_POSTDRAIN,
HITCH_STDERR, HITCH_PAIRSPREAD, HITCH_SAVE, HITCH_DRAINSPLIT, HITCH_SAVESNAP, HITCH_BGSAVE.

## 4. Open

- Not merged to main yet (the user's call); the ten agent worktrees / branches under C:/code/GIT/wt_* await removal.
- Which of the four hitch extensions earn a default is the user's call: spread_warmth (B, deaths 17 vs 25 in one pair), drain_split (B, early-game only), background_save + async_save_compress (A, identical files; resize during a save never driven).
- A tighter save writer (the walk alone is 13-14 ms of the 39 ms serialize) is a possible faithful block (HITCH_SAVESNAP.md 2b).
- The 10x stress baseline town is now fed: `perf/baselines/current.csv` predates it.
- ext.spread_warmth is OFF by default (every extension is); whether a 75 ms warmth delay is worth a
  default is the user's call. Its death count needs more legs (17 vs 25 in one pair).
- Found on the side: loading a save that carries `ext:` pseudo-mods under `/profile:faithful`
  crashes (`SaveLoadDialog::DoLoad` builds the notice `MessageDialog` from a null Dialog:
  `Ext::PseudoModNoticeDialog`'s resource did not load; `DialogInstance::InitFields` derefs it).
- The sampler at 4000 Hz hung one boot (main thread in ntdll at frame 0); 2000 Hz was fine.
- The 10x stress baseline town is now fed: `perf/baselines/current.csv` predates it (re-cut when
  a perf comparison is next needed).
