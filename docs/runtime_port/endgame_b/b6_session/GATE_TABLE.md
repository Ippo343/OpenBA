# B6 gate table (one row per gated merge; `runs/<label>/summary.txt` is the record, untracked)

Invariants on every row unless noted: builds debug / release / perf / unit exit 0, 0 new warnings, new game
`5683/0`, tutorial `5773/12`, loadgame clean, -O2 new game clean, typetable == baseline (`b983f144`), resave
5/5 == golden (`Gate.resaved.md5` unchanged unless the row says RE-CUT).

| # | label | commit | what | gate | unit | castmetric | notes |
|---|---|---|---|---|---|---|---|
| 1 | SAVE | e2304f999 | Merge b6-SAVE: the 14b rows 13, 14, 15, 16, 17, 19, 21, 22, 61, 63, 810, 820 (one `behaviour:` per retail body); 18 already retail; 20 read, not a difference | 0 problems | 920/0 | 89 -> 81 | resave 5/5 PASS with the golden unchanged, as predicted (B6_RULES C2); routes 30 s / 21 s / 9 s |
| 2 | WORK | a80f69506 | Merge b6-WORK: rows 45, 51 + G68, 135, 220, 221 (+ new row 900), 260 (re-read: the arm falls into 0x1400e837c), 643 (ClearStorage's InteractComponent; its null return is retail's), 861 + G46, 44, 52, 35 / 43 / 80 / 450 (one CalcMaxWorkers body: the MEMBER differed, fixed), pickup / playanimation probes, G39-G45, G55, G70, G72 | 0 problems | 920/0 | 81 -> 67 | tool routes run after the gate (see the .out); loadgame loader 4758 -> 4767 (that route floats); G39 + G40 share one commit (419bef6d0); the branch's own merge-of-main commit a1861e9b3 has git's default message |
| 3 | COMP | b864cd289 | Merge b6-COMP: rows 1 + 551, 400 / 550, 552, 430, 392 / 510, 511, 704, 705, 170, 280, 850, 390 / 391, 46, 643's citizenai callers, 28, 27, 26, statusicon probe, G3-G5 (new rows 920-922) | routes 0 problems; resave 5/5 PASS but crash log on every run (5 problems) -> FINDING row 990: the FIXTURE held 11 null-profession citizens; fixture + golden RE-CUT (md5 3efc80a8…), then resave 4/4 PASS crash 0 on the same exe | 920/0 | 67 -> 49 | tool routes NOT run by mergegate (it skips them on a non-zero result): run separately below; loadgame loader 4766 (floats) |
| 4 | CLOSEA | d9d36f868 | Merge b6-CLOSEA: the Pf_EntityResolvePos_2e20 / NotifyEntityChanged / WorkComponent_CalcMaxWorkers forwarders gone, every caller on the member | 0 problems (resave 5/5 on the RE-CUT golden, crash 0) | 920/0 | 49 -> 48 | COMP's tool routes ran separately on b864cd289: placement / road / removal exit 0, crash 0 |
