# B9 gate table (coordinator; one row per gated merge, written BEFORE `mergegate.sh` runs)

Kit commit: `cf62fb35a` (Block 0: the exception inventory, `-fexceptions`, rows 1210 / 1211, the rule sheet).
Baseline: `runs/baseline` (B8's close run), warning baselines `runs/warn_baseline_{debug,perf}.txt`, B11 base
`runs/b11base/{perf,debug}` = main `22b1d9613`'s objects (identical to the kit commit's: the flag spelling
recompiles nothing).

| row | branch | merge commit | gate problems | new game | tutorial | typetable | resave | unit | warnings | castmetric | routes | notes |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| RT | b9-RT | | | | | | | | | | tools | |
| RES | b9-RES | | | | | | | | | | ui, tools, panelclose, pick, fire | |
| SURV | b9-SURV | | | | | | | | | | tools, fire (tornado) | |
