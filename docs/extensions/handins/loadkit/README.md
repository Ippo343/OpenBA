# loadkit -- the loading session's coordinator scripts (LOADPLAN §4)

- `census.sh <menu|new|load> <outdir> [sampleHz]` -- one route on the deployed exe; `stamp.py` stamps every stderr line
  with ms since launch (`stamped_error.log`); `[LoadTime]` lines carry the exe's own clock. Report a window with
  `python tools/sample_report.py <outdir>/standalone_samples.txt --exe build/cmake-perf/OpenBA.exe --thread loader --from A --to B`.
- `hash.sh <exe> <seed> <small|medium|large>` -- one new game, prints its `[WorldHash]` lines (deterministic run to run).
- `hash_base_4bdffa55a.txt` -- the baseline: seeds 12345 / 777 / 951625142 x small / medium / large on main `4bdffa55a`.
  A block that changes world generation must reproduce every row exactly.
The scripts deploy into the game directory: run them under the game-dir lock, one run at a time, mods parked.
