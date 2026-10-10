# WAVE 21 INTEGRATOR LOG — what the integrator VERIFIED PERSONALLY, and what is relayed

This file separates two things that a wave close normally blurs: claims a lane made, and
claims the integrator checked. **Everything in §1 was re-run or re-read by the integrator
against the binaries or the tree.** Everything in §2 is a lane's word, relayed.

The distinction matters because of Wave 20 §5.8: a confirmatory check inherits the
account's blind spots. Reading the artefact first is what found a deleted clause there.

---

## 1. VERIFIED BY THE INTEGRATOR

| claim | source lane | how I checked it | verdict |
|---|---|---|---|
| `??_7Stream@IO@@6B@` carries `NONE, EVER` and the tree deliberately declines to promote it | SHOT | read `standalone/reg_mint_promoted.cpp:1203` and `:923-930` | **CONFIRMED.** The decline is explicit, in words, citing that READY is not a route. |
| `??_7ReaderWriter@`'s `?` numerator was replaced with a real, cited number | ENC | read `standalone/reg_mint_promoted.cpp:1202` | **CONFIRMED.** Now `1/26 ported [RE-DERIVED …]`, citing the addressee by name. |
| `statusicon_geometry.cpp` copies 12 slots out of a 9-slot table | GFXVT | read `code/Game/Component/statusicon_geometry.cpp:337-342` | **CONFIRMED.** Loop is `i < 16` with `i < 12` guarding the copy from `baseVtbl`. |
| slots 10-11 belong to a DIFFERENT class's vtable, so the "12 slots" comment is wrong | GFXVT | Ghidra `get_xrefs_to` on both tables in Application-x64.exe | **CONFIRMED, and the stamper sets are fully disjoint.** `0x14015f0c0` ← `StatusIconComponent_Ctor` + `FUN_1400aa4e0`; `0x14015f110` ← `TradeWork_impl_ed50` + `FUN_1400b00e0`. No overlap. |
| Graphics gap of 29 decomposes 13 code / 8 vtable / 8 data | kickoff (mine) | `runtime_census.py --csv` plus a mangled-name classifier | **CONFIRMED**, and independently re-derived symbol-for-symbol by GFXBODY and GFXVT. |

**One incompleteness in a lane's account, found by checking it.** GFXVT named
`StatusIconComponent_Ctor` as the stamper of `0x14015f0c0`. There are **two**:
`FUN_1400aa4e0` also stamps it. This does not touch the conclusion — both stamp the SAME
table, so the disjointness that carries the argument holds — but the account said
"stamped by" where the binary says "stamped by, among others". Recorded because a wave
that only records corrections which change conclusions will slowly learn that
completeness is optional.

---

## 2. RELAYED, NOT INDEPENDENTLY VERIFIED

- SHOT's stamper counts (17/6/11 and 50/22/25+3) and the 89-read / 2-write split on
  `?_singleton@FileSystem@IO@@`. SHOT reports Ghidra corroborated these exactly.
- ENC's `UEAA` determination for `??1TextFileStream@` and `??1ReaderWriter@`, and its
  encoding-slot semantics — which ENC itself flags as **inference from a sibling table**,
  not attested fact.
- GFXBODY's corrected extents. **Nine of thirteen rest on the corrected unwind-chain
  method alone; only four were Ghidra-confirmed.** GFXBODY states this itself.
- GFXVT's Q2 derivation that nothing dispatches slots 9-11. GFXVT correctly labels this a
  **derivation, not an observation**, and notes it did not enumerate every dispatcher in
  the DLL.
- All attendance and build results.

---

## 3. INTEGRATOR ERRORS ON THE RECORD

**3.1 I split one subject across two lanes.** The `FrameCopyImageBuffer` cluster went to
GFXBODY and "what the screenshot hole means" went to SHOT. **They are the same lane**, and
neither the kickoff nor either brief said so. Found by GFXBODY, not by me. The kickoff's
§4 lane table is what should have caught it, and it does not check for overlapping
subjects at all.

**3.2 I passed along a wrong ranking.** The kickoff repeated Wave 20's framing that
`?Draw@Renderer@` is the large Graphics row. It is 921 bytes;
`?BuildParameters@MaterialInstance@` is **1422 with 12 indirect calls**. I had no
independent basis for the ranking and relayed it because it was in the handover — which is
precisely §4 of the last close, committed by the person who wrote the rule into this
kickoff.

**3.3 Seven lanes against one game directory serialised the runs, and a lane paid for
it.** GFXBODY's lock acquire timed out at 600s with GFXDATA holding it, so its three ports
are **STRUCTURAL — nothing ran**. The lock behaved correctly and refused rather than
granting falsely. The scheduling was mine. Those three bodies need a run before anyone
calls them verified, and that is an open integrator task, not a lane's debt.

---

## 4. OPEN AT TIME OF WRITING

- `statusicon_geometry.cpp:341` is a **cutover blocker on the critical path** and is
  **NOT APPLIED**. GFXVT deliberately left the loop at `i < 12` and put the full
  derivation at the point of use instead. Two residuals it recorded rather than smoothed:
  retail's slot 9 is `nullptr` where our stub is not, and Q2 is derived from two classes'
  extents rather than by enumerating dispatchers.
- GFXBODY's three ports await a run (see 3.3).
- A **sixth** instance of the census wrapper blind spot (`RtVtable`, 3 live cutover
  dependencies invisible to `called`) is diagnosed by GFXVT and handed to CLASS, unfixed
  by design.
- Lanes GFXDATA, VFY and CLASS had not reported when this was written.
