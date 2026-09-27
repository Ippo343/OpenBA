# B.3.9 gate table (coordinator). One block per merge; full summaries are the coordinator's scratch kit runs.

| merge | main | builds | warnings new | unit | routes (new game / tutorial / load) | resave | mint (vt/ti/storage) | RT_DELDTOR | extra routes | problems |
|---|---|---|---|---|---|---|---|---|---|---|
| baseline (clean rebuild, = B.3.8 step close) | ab1e0846 | ok | — | 736 | 5683/0 · 5773/12 · 4758 | 5/5 | 152/104/48 | 135 | — | 0 |
| b39-rules (mechanism + pilot: 4 dead rows, PickingMesh, SunLight) | 08e9b580 | ok (-j4; the -j12 run was stopped by the harness for low memory) | 0 | 740 | 5683/0 · 5773/12 · 4758 | 5/5 | 146/104/48 | 130 | — | 0 |
| b39-EXA (EXE-1: loop builders, copy sites, exe builders, dead scaffolding) | 41b05317 | ok | 0 | 744 | 5683/0 · 5773/12 · 4758 | 5/5 | 146/104/48 | 130 | Mods (.pkm) exit 0; tool placement / road / removal exit 0 | 0 |
| b39-COORD0 (W0: storage redirect, promoted-file strip, 5 storage cells, 21 globals, dead type-id cells, 286 reader-less registry rows) | cf9fc8e7 | ok | 0 | 749 | 5683/0 · 5773/12 · 4766 | 5/5 | 146/104/43 | 130 | — | 0 |
| b39-UIB (W1: 12 widget pairs concrete) | a1750148 | ok | 0 | 764 | 5683/0 · 5773/12 · 4758 | 5/5 | 122/104/43 | 106 | Mods exit 0; Quit / Options / Achievements exit 0; GoalDialog opens, crash log 0 | 0 |
| b39-UIA (W1: 15 widget pairs concrete) | e564f680 | ok | 0 | 786 | 5683/0 · 5773/12 · 4767 | 5/5 | 92/104/43 | 76 | Mods exit 0; Quit / Options / Achievements exit 0; GoalDialog opens, crash log 0; tool placement / road / removal exit 0 | 0 |
| b39-IO (W1: encodings, streams, PackageFile concrete; 3 titled behaviour commits) | d0acb883 | ok | 0 | 799 | 5683/0 · 5773/12 · 4758 | 5/5 | 78/104/43 | 72 | Mods exit 0; Quit / Options / Achievements exit 0; GoalDialog opens; in-game save COMPLETE, crash log 0 | 0 |
| b39-C3 (caller batch: code/Game/Work type-id cells, ~100 sites) | 96cba89a | ok | 0 | 799 | 5683/0 · 5773/12 · 4763 | 5/5 | 78/104/43 | 72 | tool placement / road / removal exit 0 | 0 |
