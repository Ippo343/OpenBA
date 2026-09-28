# B3X (normal-virtual cutover) — the gate table

Every merge was gated by the coordinator with `gate.sh <label> 5` (debug + release + perf build at -j4,
new-warning set against a clean-rebuild baseline of 1,093 files, unit, new game + pause-menu quit,
tutorial, Load Game, `newgame_perf` on the -O2 exe, resave x5, typetable == baseline, `[RtResolve] 0
misses`, 0 TRAP, 0 GATE WARN, dispatch 0, mint UP TO DATE 0/0/2). Run directories: the session
scratchpad's `kit/runs/<label>/` (not committed).

Invariants on EVERY row: new game `calls=5683 openFileFail=0`, tutorial `calls=5773 openFileFail=12`,
Load Game 4758-4767, typetable 428 lines md5 `b983f144` == baseline, resave 5/5 == golden
(21,075,114 bytes), 0 new warnings (debug and perf), `newgame_perf` exit 0.

| # | label | main after | what | unit | extra routes | problems |
|---|---|---|---|---|---|---:|
| 0 | baseline | `deaf604c` | clean rebuild, warning baseline cut (debug 112, perf 263 entries) | 914 / 0 | — | 0 |
| 1 | rsc | `eafb0fa7` | behaviour: SpriteCaptureUI's `".rsc"` literal (owed from B.3.9) | 914 / 0 | — | 0 |
| 2 | RGFX | `c445c813` | W0 R-GFX: graphics / video reads by name; the twelve `*Data` pins | 914 / 0 | — | 0 |
| 3 | T0 | `a302d6a7` | W0 T0: `slot_check.h`, 1,197 PMF checks, raw-slot helpers | 916 / 0 | — | 0 |
| 4 | GA | `bc10e6df` | W0 G-A: Component / Entity / Action; `Action::Enter(stack, time)` | 916 / 0 | tools 3/3 | 0 |
| 5 | RUI | `e7890cb1` | W0 R-UI: runtime/src/ui; the ui slot helpers deleted | 916 / 0 | ui (mods, quit, options, achievements, goaldialog) | 0 |
| 6 | RIO | `d85f0fb7` | W0 R-IO: io / object / system / save | 916 / 0 | — | 0 |
| 7 | RREST | `3d380b91` | W0 R-REST: audio … picking, engine/, standalone/; the wrong-root delete | 916 / 0 | ui, tools 3/3 | 0 |
| 8 | GB | `76c634c1` | W0 G-B: Dialog / Game / Work / Player / Tutorial; `Work::Validate()`, `Execute(citizen, context)` | 916 / 0 | tools 3/3 | 0 |
| 9 | W0_devlost | `76c634c1` | forced device loss at Present #40000 | — | INJECT=1 COMPLETE=95 crash log 0 TRAP=0 (killed at 600 s) | 0 |
| 10 | AUDIT | `82e4621f` | W0 audit: five missed reads; 1,114 product sites classified | 916 / 0 | — | 0 |
| 11 | W1pre | `1386db46` (branch, detached) | the PRODUCT flip alone, before the tests were rewritten (skipbuild: no unit) | — | ui, tools 3/3, devlost INJECT=1 COMPLETE=110 crash log 0 | 0 |
| 12 | W1 | `7b577c20` | W1: the flip + the rewritten tests | 919 / 0 | ui, tools 3/3 | 0 |
| 13 | closeclean | `0419d0ce` | step close: true release compile, `make clean`, clean rebuild of debug / perf / unit, gate (resave x3) | 919 / 0 | stepclose routes (`CLOSE_NUMBERS.md` §5) | 0 |

Notes.
- `goaldialog` ends `exit 1 killed=YES` by design (the dialog stays open; judged on crash log 0 and one
  `OpenGoal` line).
- `tool removal` prints `SKIP CalcPath: removalDesc(+0x38) null` — the harness's own skip, unchanged from
  the B.3.9 close.
- `devlost` is a crash check only (the device-lost work is DEFERRED until the endgame is finished): it is
  killed at its 600 s limit with the round trip COMPLETE and an empty crash log, as at the B.3.9 close.
- No merge needed a re-run, a re-cut baseline or a fix-up commit. No resave DIFF, no typetable diff.
