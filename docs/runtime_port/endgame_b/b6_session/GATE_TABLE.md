# B6 gate table (one row per gated merge; `runs/<label>/summary.txt` is the record, untracked)

Invariants on every row unless noted: builds debug / release / perf / unit exit 0, 0 new warnings, new game
`5683/0`, tutorial `5773/12`, loadgame clean, -O2 new game clean, typetable == baseline (`b983f144`), resave
5/5 == golden (`Gate.resaved.md5` unchanged unless the row says RE-CUT).

| # | label | commit | what | gate | unit | castmetric | notes |
|---|---|---|---|---|---|---|---|
| 1 | SAVE | e2304f999 | Merge b6-SAVE: the 14b rows 13, 14, 15, 16, 17, 19, 21, 22, 61, 63, 810, 820 (one `behaviour:` per retail body); 18 already retail; 20 read, not a difference | 0 problems | 920/0 | 89 -> 81 | resave 5/5 PASS with the golden unchanged, as predicted (B6_RULES C2); routes 30 s / 21 s / 9 s |
