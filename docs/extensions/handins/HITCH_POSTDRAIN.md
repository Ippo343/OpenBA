# HITCH_POSTDRAIN hand-in (2026-10-18)

Branch `postdrain`, based on `hitch` `abb0b1c2c`. The EntityList-phase hitches left after the PathGraph
drain fix (`HITCH_PATHGRAPH.md`) and outside the cold-wave class (`HITCH_PFBURST.md`, ext.spread_warmth).
**Result: faithful, no extension.** Each of the three classes was mostly our own diagnostic `fprintf`.
The game work underneath is retail's and stays under the criterion. Not merged.

| Commit | What |
|---|---|
| `68d5935c4` perf | `ErrLine` (`src/diag/err_line.h`): one write per diagnostic stderr line, in six game files |
| (this commit) docs | this hand-in, the `perf/README.md` note |

## 1. The mechanism

- `main.cpp` opens `standalone_error.log` with `setvbuf(stderr, NULL, _IONBF, 0)`, so a line survives a crash.
- MinGW's `fprintf` (`__mingw_vfprintf` -> `__pformat_putc`) writes an unbuffered stream one character at a
  time. Each character is a `WriteFile`.
- Only `printf` is stripped in perf and release (`src/app/log_strip.h`). Every `fprintf(stderr, ...)` runs
  in every flavour, the shipped exe included.

Measured with a 90-character `[AnimalUpd]` line, 200 lines each, `_IONBF` file (scratch test, not committed):

| writer | us per line |
|---|---|
| `fprintf(stderr, ...)` | 245 |
| `ErrLine(...)`: `vsnprintf` to a stack buffer, one `fwrite` | 3.6 |

The two outputs are byte-identical. A trace that prints a few dozen lines in one tick is therefore a
frame hitch, and the bursts below are exactly that.

## 2. The three classes

How they were found: a temporary probe, not committed (the diff is in the session scratchpad).
- Per Draw-to-Draw period, it took the deltas of the existing per-component-type profile, plus
  Startup / OnAdded kinds, and the EntityList sub-phases.
- It also counted `ValidateAndEnterState` calls, times and entries per state, `TransitionState` calls,
  entity constructions, `RegisterEntity` calls and `GiveBirth` calls.
- One leg ran `COMPPROF=2` for the per-frame tree ticks, and one ran with the 4 kHz sampler.
- Sampler stacks were cut to one hitch period with `sample_report.symbolize_exe`.

### Frames 15 / 21 / 33 at 50x: the stress spawn, plus `[HomeInvalid]` dumps

- **Where the time is.** The EntityList tree walk takes 16.3-16.6 ms against about 3.4 ms on other frames.
  In the `COMPPROF=2` leg, the per-frame CharacterComponent tick is 15.3 ms over 52,250 calls, against
  1.7 ms at frame 33.
- **What it is.** 22 of the 31 samples in frame 15 are `fprintf`, called from `citizencomponent.cpp`'s
  `[HomeInvalid]` block. Its first three hits print two 25-line ASCII maps of the region grid. That is
  about 2,600 characters, about 7 ms per hit.
- **The trigger.** The stress town's 800 citizens are spawned on one tick and act in lockstep. The first
  three home invalidations land on three frames.
- **The game work** of those frames is the per-frame action ticks of 800 citizens. That is the spawn
  shape, not reachable with a real town's 10-20 founders.

### Frames 1081-1083: the 900 s birth wave

- **The trigger.** The conception gate (`CitizenComponent::Update`, retail `0x140113873` onward) seeds
  `pregnancyTimer = desc->pregnancyTime` (900 s) when the citizen is married, housed, in the age window,
  and the household has room.
  - The stress town spawns about 400 couples married and housed on the first tick.
  - So every eligible woman conceives together and gives birth at 900 s. 900 sim seconds is frame 1081
    at 50x.
  - The post-partum cooldown is random (`rand * range + base`), so the later waves are spread out.
- **Measured** (probe leg `20261018_035255_postdrain_probe2`):

| frame | what | counts | ms |
|---|---|---|---|
| 1082 | `GiveBirth` | 143 births, 144 entities built | 1.23 (ctor 0.38) |
| 1082 | CitizenComponent Updates | 750 | 3.50 (ema 0.28) |
| 1083 | CitizenAIComponent Updates: the newborns' first decisions | 173 transitions (ema 17): state 3 x152, 6 x107, 11 x79, 12 x76 | 4.37 (ema 0.28) |

- **Retail does the same.** `GiveBirth`'s scan over every citizen is retail's (`0x140111b90`), at about
  9 us per birth here. The newborns' first decisions cost about 25 us each.
- **The prints.** The `[Unassign]` and `[Burial]` traces (`citizen_assignworkplace.cpp`,
  `citizen_lifeevent_handler.cpp`) fired in the same frames.
- **Scaling.** The cost is synchronised conceptions times about 35 us, over two frames. Reaching 7 ms of
  game work needs about 200 couples housed on the same tick. A real game houses a few couples at a time:
  founders are about 5 couples, nomads arrive in tens. **The game work is a stress-town shape**, and
  nothing is built for it.
- **Not the drain.** With the PathGraph fix, the drain is at frames 748 / 1108 / 1468 and has no
  EntityList burst. 1081 coincides with a rebuild start only by cadence.

### Frame ~4351: every herd grows on the same tick, and is traced

- **Retail.** The herd constructor `0x140122150` zeroes `spawnTimer` (+0x58) at `0x140122197`.
  - `Populate` (`0x140120f50`) writes only +0x48 / +0x50.
  - `Update` adds dt to +0x58 (`0x140121c26`) and fires `AdjustSize` past the grow interval (3600 s here).
  - So **in retail, and in every real game, all herds created at world generation double on the same
    tick.**
- **Measured.** 17 herds fired in one frame and built 31 animals (probe leg `..._probe3`).
  - `AnimalComponent::OnAdded`, which runs the animal's first Update, took 5.51 ms for 31 calls.
  - The timed parts of that Update (destination loop, MoveTo, wander timer, FindHabitat) took 0.03 ms.
  - In the sampled leg, 12 of 32 samples are `fprintf` at `animalcomponent.cpp:512`. That is the
    `[AnimalUpd] first tick` trace, capped at 24 lines. The new animals are age 0, so they are the
    first to reach it.
- **The game work** (31 entities built, registered and given a first Update) is well under 1 ms.

## 3. The fix

- **`ErrLine`.** It formats into a 2 KB stack buffer and `fwrite`s the line once. A longer line falls
  back to `vfprintf`.
- **Same guarantee.** stderr stays `_IONBF`, and a line is written before the call returns.
- **Converted: every `fprintf(stderr, ...)` in these six files.**
  - `animalcomponent.cpp` (7)
  - `herdcomponent.cpp` (5)
  - `citizencomponent.cpp` (9, including `[HomeInvalid]`)
  - `citizen_assignworkplace.cpp` (1)
  - `citizen_lifeevent_handler.cpp` (2, `[Burial]`, uncapped: about 250 characters per death)
  - `nomadcomponent.cpp` (3)
- **Class.** Faithful, no sim change. Only the I/O path of diagnostics changes: the same arguments are
  formatted into the same bytes. The COMPPROF=4 shadow does not apply, because no sim value is
  computed differently.
- **Why not a run comparison.** Stress runs are not reproducible run to run: six legs of builds that do
  not touch the sim differ by 1-25 citizens from row 1200. So identity cannot be shown by comparing two
  runs.
- **Not converted: `citizen_updatehealth.cpp`.** It is committed with CRLF line endings, so any edit
  renormalises the whole file. Its `[Plague] CENSUS` line is periodic, not a burst.

## 4. Before / after

Same machine, both legs CLEAN, 50x, 12000 frames, fed stress town, faithful profile.
- **Before:** `perf/results/20261018_101614_postdrain_before`, built from this branch with the six files
  at `abb0b1c2c`.
- **After:** `perf/results/20261018_101136_postdrain_errline`.

Hitch rows that are not the harness's own, excess in ms (`-` = no row):

| class | before | after |
|---|---|---|
| spawn 15 / 21 / 34 | 13.6 / 13.9 / 13.0 | - / - / - |
| births 1082 | 9.2 | - |
| herds 4353 | 11.6 | - |
| cold wave (pfburst's, faithful) | 3094 14.4, 7320-7321 8.7 / 10.1, 11817-11818 15.6 / 11.2 | 3372 9.0, 7475 11.0, 11969-11970 10.2 / 12.7 |
| startup 180 / 300 (update-other) | 24.1 / 11.0 | 23.4 / 11.6 |
| 5761 (`ent+4.9 cit+2.9`) | - | 8.3 |

`update_ms_per_iter`:

| rows | change, after vs before |
|---|---|
| 600-8400 | within +-2.5% per row, no direction |
| 9000-12000 | -4% to -10%; the before leg holds 5-15 more citizens there, not attributed |
| whole-run mean | 0.1613 -> 0.1571 ms, -2.6% |

- `tools/build.sh unit perf`: 1167 tests, 0 failures, 3 skipped (no kit fonts).
- `tools/acceptance/acceptance.sh`: 0 lines off target.

## 5. Open

- **Frame 5761 of the after leg.** It is a rebuild-start frame (`cit+2.9`) with an EntityList bump of
  4.9 ms, 8.3 ms excess, 1.2 ms over the criterion. It was seen once, absent from the before leg, and not
  probed.
- **The general case is not fixed.**
  - The tree has about 1,444 `fprintf` sites, 577 of them `std::fprintf`, and 546 `fprintf(stderr` sites
    in `src/game`.
  - In the 4500-frame probe leg, game code wrote 275 KB to stderr after frame 60. At about 2.7 us per
    character that is about 0.17 ms per frame on average.
  - Any capped trace that fires in a burst, or any uncapped per-event trace such as `[Burial]` or
    `[Plague] CENSUS`, is a hitch candidate in the shipped exe.
  - Two ways out, both a decision for the coordinator:
    - a forced-include wrapper for `fprintf`, like `log_strip.h`, plus `namespace std { using ...; }`
      for the qualified calls;
    - or converting the per-event traces in the sim to `ErrLine`.
- **Not run here:** the game gate (`tools/build.sh run`), the resave gate, and the 10x / 20x legs.
