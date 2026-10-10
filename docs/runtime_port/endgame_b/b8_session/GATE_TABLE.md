# B8 gate table (one row per gated merge; `runs/<label>/summary.txt` is the record, untracked)

Invariants on every row unless noted: builds debug / release / perf / unit exit 0, 0 new warnings, new game
`5683/0`, tutorial `5773/12`, loadgame clean, -O2 new game clean, typetable == baseline (`b983f144`), resave
5/5 == golden (`994eb184a8e88db9372b5b9cd13a9ed9`, unchanged unless the row says RE-CUT).

| # | label | commit | what | gate | unit | castmetric | notes |
|---|---|---|---|---|---|---|---|
| 0 | DEC | ed149bf42 | the two user decisions of 2026-10-09 on main: `c60271c1b` NaturalResourceComponent slot 8 installed (the body is the member `Tick`, its free declaration and `naturalresourcecomponent_funcs.h` deleted), `ed149bf42` SpawnSubResource's placement test 0x14009f5d0 wired and its spawn-chance constant corrected (1.0f at 0x14017214c, not the 0.98f UV scale) | 0 problems (the 3 `!!` lines in runs/DEC/summary.txt are the kit's missing RESOLVE_CACHE_GONE marker, copied after the run) | 920/0 | 3 (unchanged) | tools: placement / road / removal exit 0 crash 0 (removal's harness never reaches CalcPath: pre-existing, "removalDesc null"); sa-stress and worldcmp vs oldbase in `runs/DEC_routes.out` (recorded in the next row's notes if they finish after the kit commit); resave 5/5 unchanged (the fixture is a loaded save) |
