# B.3.9 gate table (coordinator). One block per merge; full summaries are the coordinator's scratch kit runs.

| merge | main | builds | warnings new | unit | routes (new game / tutorial / load) | resave | mint (vt/ti/storage) | RT_DELDTOR | extra routes | problems |
|---|---|---|---|---|---|---|---|---|---|---|
| baseline (clean rebuild, = B.3.8 step close) | ab1e0846 | ok | — | 736 | 5683/0 · 5773/12 · 4758 | 5/5 | 152/104/48 | 135 | — | 0 |
| b39-rules (mechanism + pilot: 4 dead rows, PickingMesh, SunLight) | 08e9b580 | ok (-j4; the -j12 run was stopped by the harness for low memory) | 0 | 740 | 5683/0 · 5773/12 · 4758 | 5/5 | 146/104/48 | 130 | — | 0 |
| b39-EXA (EXE-1: loop builders, copy sites, exe builders, dead scaffolding) | 41b05317 | ok | 0 | 744 | 5683/0 · 5773/12 · 4758 | 5/5 | 146/104/48 | 130 | Mods (.pkm) exit 0; tool placement / road / removal exit 0 | 0 |
