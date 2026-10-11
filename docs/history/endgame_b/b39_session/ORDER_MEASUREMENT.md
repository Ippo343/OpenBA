# B.3.9 — the type-table order measurement (B38_RULES §6 item 2): ANSWERED

Run by the coordinator 2026-09-27 on `870b9ea7`, debug build, throwaway worktree, `orderprobe.sh` (this
directory); patches in `probes/` (inventory 0C §C designed them). Each variant: new-game route + resave ×2.
`tree` = the sorted `[OrderProbe]` lines (id, name, parent, winner, children per record — the order-free
tree `Factory::Startup` built); `chain` = the `[OrderProbeChain]` walk order.

```
== orderprobe @ 870b9ea7 00:17:10
dump: build exit 0
dump: newgame exit 0 crash=0 (32s) TRAP=0 calls=5683 openFileFail=0
dump: tree lines 416 md5 dd808e0c; chain lines 416 md5 4f1321dd; typetable md5 1f3e44cb
dump: [GameMain] Post-Startup _firstTypeInfo: 416 entries, 416 have non-FF typeId
dump: resave 1 exit 0 [gate] crash log bytes: 0 [gate] verdict: PASS (resave == golden, 21075114 bytes) 
dump: resave 2 exit 0 [gate] crash log bytes: 0 [gate] verdict: PASS (resave == golden, 21075114 bytes) 
A: build exit 0
A: newgame exit 0 crash=0 (32s) TRAP=0 calls=5683 openFileFail=0
A: tree lines 416 md5 dd808e0c; chain lines 416 md5 e0e70c2f; typetable md5 9751bd07
A: [GameMain] Post-Startup _firstTypeInfo: 416 entries, 416 have non-FF typeId
A: resave 1 exit 0 [gate] crash log bytes: 0 [gate] verdict: PASS (resave == golden, 21075114 bytes) 
A: resave 2 exit 0 [gate] crash log bytes: 0 [gate] verdict: PASS (resave == golden, 21075114 bytes) 
A: tree diff vs dump: 0 line(s); typetable diff: 20 line(s)
B: build exit 0
B: newgame exit 0 crash=0 (32s) TRAP=0 calls=5683 openFileFail=0
B: tree lines 416 md5 dd808e0c; chain lines 416 md5 78b88d2e; typetable md5 1f3e44cb
B: [GameMain] Post-Startup _firstTypeInfo: 416 entries, 416 have non-FF typeId
B: resave 1 exit 0 [gate] crash log bytes: 0 [gate] verdict: PASS (resave == golden, 21075114 bytes) 
B: resave 2 exit 0 [gate] crash log bytes: 0 [gate] verdict: PASS (resave == golden, 21075114 bytes) 
B: tree diff vs dump: 0 line(s); typetable diff: 0 line(s)
C: build exit 0
C: newgame exit 0 crash=0 (32s) TRAP=0 calls=5683 openFileFail=0
C: tree lines 416 md5 dd808e0c; chain lines 416 md5 0ace2039; typetable md5 1f3e44cb
C: [GameMain] Post-Startup _firstTypeInfo: 416 entries, 416 have non-FF typeId
C: resave 1 exit 0 [gate] crash log bytes: 0 [gate] verdict: PASS (resave == golden, 21075114 bytes) 
C: resave 2 exit 0 [gate] crash log bytes: 0 [gate] verdict: PASS (resave == golden, 21075114 bytes) 
C: tree diff vs dump: 0 line(s); typetable diff: 0 line(s)
== done 00:25:54
```

| variant | reorder took effect (chain md5 differs) | tree | typetable | resave | new game |
|---|---|---|---|---|---|
| dump (baseline) | — | dd808e0c | 1f3e44cb | 2/2 PASS | calls=5683 |
| A: exe registrations reversed | yes | identical | 20 lines differ, all `[GameMain]   typeId=` (the first-ten dump) | 2/2 PASS | calls=5683 |
| B: minted records children-first | yes | identical | identical | 2/2 PASS | calls=5683 |
| C: minted chain appended after the CRT-init records | yes | identical (incl. both `Internal` lines) | identical | 2/2 PASS | calls=5683 |

**Verdict.** The type table is ORDER-INDEPENDENT: ids are `~CRC32(name)` (retail 0x1800c06e1), the tree is
keyed by id, parents are linked in a second pass. Nothing persistent depends on registration order. The
`DEFINE_TYPE` fold (ENDGAME B.1 decision 4) may let static-init order decide, provided (a) no two records
share a name — the one duplicate today, `Internal`, goes in the fold — and (b) the gate's first-ten dump
(`game_main.cpp:499-503`) is made order-free and the typetable baseline re-cut in writing in that commit.
Variant C did not swap the `Internal` winner as 0C predicted; the tree is the same either way.
