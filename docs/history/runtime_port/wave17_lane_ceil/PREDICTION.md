# Lane CEIL — pre-registered prediction (written BEFORE any measurement)

Subject: the 62 DATA rows in the `Video` / `IO` / `Graphics` called-but-not-ported
sets (Video 25, IO 17, Graphics 20). The brief said "55"; the actual strict count
of `??_7` + `?_typeInfo@` + `?_singleton@` + `.data` statics is 62 — see §1 of the
lane doc for the reconciliation.

Static facts known at the time of writing (from the mint tables and the DLL
export table only — no run, no attendance, no readiness screen):

* 57 of the 62 have a minted row; **5 IO vtables have NONE**
  (`??_7BinaryFileStream/CompressedFileStream/FileStream/TextFileStream/VisitorStream@IO@@6B@`).
  All five ARE exported (0x1801554d0 / 0x1801555f0 / 0x180154960 / 0x180155980 /
  0x180155a20) and none of those VAs appears anywhere in
  `standalone/rt_data_mint_tables.inc`.

## Prediction

| bucket | predicted count | mechanism I expect to be the reason |
|---|---|---|
| NO-OP (ported-but-unpromotable) | **40** | in-DLL ctor stamps the vptr through a RIP-relative LEA, which is not a door; and the `?_typeInfo@` rows repeat lane TINFO's factory-tree mechanism |
| HARMFUL | **13** | every `?_singleton@` (6) and every `.data` static list-head/counter (7): mutable storage with in-DLL readers/writers -> split brain |
| PROMOTABLE | **4** | some of the 13 Video `??_7*Data@` companion vtables, because `dx11/` is OUR code and may be the thing that stamps them |
| BLOCKED | **5** | the five unminted IO vtables: nothing to promote until the mint harvest covers them |

Resulting predicted ceilings: Video ~ (109+? )..., i.e. I expect **Video and
Graphics to be within ~2 points of their ceiling** and **IO to keep the largest
genuine gap**, because IO's 20 non-data rows are real function work while
Video's and Graphics' are fewer.

Scoring rule fixed in advance: I score the COUNT per bucket **and** the
MECHANISM per row. A row that lands in the predicted bucket for a different
reason counts as a miss on mechanism.
