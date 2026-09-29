# B.4 gate table

Every row: the coordinator's `gate.sh` on main after the merge (clean-rebuild warning baseline cut at
`b2a5434c`: debug 100, perf 201). Invariants: new game `calls=5683 openFileFail=0`, tutorial
`5773/12`, typetable 428 lines md5 `b983f144`, resave == golden, unit 0 failures, stub set 0.
`metric` is `castmetric.py code runtime standalone dx11 engine` (to remove; floor columns apart).

| # | label | main after | what | builds / unit | routes | resave | extra routes | problems |
|---:|---|---|---|---|---|---|---|---:|
| 0 | baseline | `b2a5434c` | clean rebuild, warning baseline | exit 0 · 918 / 0 | newgame 5683/0 · tutorial 5773/12 · loadgame 4766 · perf exe exit 0 | 3/3 | — | 0 |
| 1 | R0-EXE | `e738df25` | 49 unreferenced exe C bodies deleted; four grab-bag files split | exit 0 · 918 / 0 · 0 new warnings | 5683/0 · 5773/12 · 4766 · perf 0 | 5/5 | ui ok · tools ok | 0 |
| 2 | E0 | `9fd39a60` | the metric knows the floor; 162 marker lines; `acceptance.sh`; `FLOOR.md` | exit 0 · 918 / 0 · 0 | 5683/0 · 5773/12 · 4758 · perf 0 | 5/5 | — | 0 |
| 3 | W0-H1 | `6d4ade58` | seven citizen-side headers: retail types, all asserted | exit 0 · 918 / 0 · 0 | 5683/0 · 5773/12 · 4758 · perf 0 | 5/5 | ui ok · tools ok | 0 |
| 4 | R0-RT | `321e650c` | 697 unreferenced runtime bodies and doors deleted (merge `f41a352a` + the coordinator's `allocator.cpp` correction `321e650c`) | exit 0 · 918 / 0 · 0 | 5683/0 · 5773/12 · 4767 · perf 0 | 5/5 | ui ok · tools ok · devlost COMPLETE=103, crash log empty, killed at 600 s (as before) | 0 |

Coordinator errors, each fixed in its own commit:
- `f41a352a` resolved the `allocator.cpp` conflict at a comment banner of `=` characters (the
  script searched for the separator as a substring): the merge commit kept the region R0-RT deleted
  and a stray separator line, and does not build. `321e650c` is the file as R0-RT left it with E0's
  33 marker lines. **A bisect across `f41a352a` must skip it.** Conflict separators are matched as
  whole lines (`^=======$`).

| 5 | W0-H2 | `b6f73067` | GameUpdateLoop typed and asserted; GameSettings, GameContainer; five component headers; Map, PlayerController, PlayerToolManager; `behaviour:` sky mesh store order 0x140097380 | exit 0 · 918 / 0 · 0 | 5683/0 · 5773/12 · 4766 · perf 0 | 5/5 | ui ok · tools ok | 0 |

**W0 closed at `b6f73067`.** Metric 46,733 (all casts, block 0) = 46,226 to remove + 507 floor at the
base; at W0's close **45,357 to remove** (tier I 327, CAST-OK 180), bridge markers 6,849 -> 6,535.
`acceptance.sh`: 9 of 12 lines off target.
