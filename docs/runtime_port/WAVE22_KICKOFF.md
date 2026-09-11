# WAVE 22 KICKOFF — System, and the eleven bodies that are six

Base: `wave22/base`, cut from `wave21/base` at `e0bd4c24` (Wave 21 merged at `9fa1d02c`;
`e0bd4c24` adds only WAVE22_PLAN.md). Opened 2026-09-11. Five lanes, private worktrees.

## 0. RE-DERIVED AT KICKOFF — the plan holds

Run at kickoff on the base tip, not inherited:

```
tools/ghidra_mcp/.venv/Scripts/python.exe tools/runtime_census.py --csv w22_base.csv
awk -F, '$3=="System" && $4=="0"{print $NF"\t"$2}' w22_base.csv | sort
```

```
TOTAL   4632   1313 called   1129 ported   9 detour   86.0%
System   262    103 called     78 ported   2 detour   75.7%
=> 50 of 184 gap rows (27%) are a function to reimplement.
```

The 25 System gap rows classify **BODY 11 · DATA 11 · CTOR 1 · DTOR 1 · VTABLE 1**,
exactly as WAVE22_PLAN.md §1 states. **The plan is NOT stale on its headline.** Whether
its §1 finding (five of the eleven bodies are the already-written, gate-returning-zero
allocator) still holds is **lane ALLOC's first task**, not a kickoff claim.

## 1. THE 25 ROWS, AS DEALT TO LANES

**ALLOC** (6)
```
BODY   ?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z
BODY   ?MemFree@MemoryAllocator@System@@SAXPEAX@Z
BODY   ?Get@MemoryAllocator@System@@SAPEAV12@XZ
BODY   ?Startup@MemoryAllocator@System@@QEAA_N_K0@Z
BODY   ?Shutdown@MemoryAllocator@System@@QEAAXXZ
DATA   ?_singleton@MemoryAllocator@System@@0PEAV12@EA
```
**TIMER** (5)
```
DATA   ?_secondsPerTick@Timer@System@@0NA
DATA   ?_ticksPerSecond@Timer@System@@0_JA
DATA   ?_systemSecondsPerTick@Timer@System@@0NA
DATA   ?_systemTicksPerSecond@Timer@System@@0_JA
DATA   ?_systemInitialTick@Timer@System@@0_JA
```
**ERRCFG** (8)
```
BODY   ?Display@Error@System@@SAXXZ
DATA   ?_errorString@Error@System@@0PADA
DATA   ?_hideDisplay@Error@System@@0_NA
DATA   ?_stopApplication@Error@System@@0_NA
BODY   ?GetCommandLineString@Configuration@System@@QEBA?AVString@2@AEBV32@@Z
DTOR   ??1Configuration@System@@QEAA@XZ
DATA   ?_singleton@Configuration@System@@0PEAV12@EA
DATA   ?_disableHandler@Debug@System@@0_NA
```
**SYSBODY** (6)
```
BODY   ?Compress@Compression@System@@QEAAHPEBXIPEAXI@Z
CTOR   ??0Compression@System@@QEAA@XZ
BODY   ?DateAsString@DateTime@System@@QEBA?AVString@2@XZ
BODY   ?TimeAsString@DateTime@System@@QEBA?AVString@2@XZ
BODY   ?StrCompare@String@System@@SAHPEBD0@Z
VTABLE ??_7Thread@System@@6B@
```
**GEOM** — not a census row. The `statusicon_geometry.cpp:341` cutover blocker,
owed by name (§2).

## 2. LANE GEOM IS A DEBT, NOT A CHOICE

`standalone/reg_mint_promoted.cpp` carries a tag committed by Wave 21:

> `[ADDRESSEE: Wave 22 lane GEOM, which owns the statusicon_geometry.cpp:341 cutover
> blocker — WAVE21_CLOSED.md §7 item 1]`

An expiry naming a condition with no addressee never fires. GEOM runs, or the tag is
re-pointed **in the same breath** with the new addressee named in writing.

## 3. THE INSTRUMENT QUESTION — owned by ALLOC

**Nothing detects a GATED port.** The census already prints
`40 registration(s) are present in the source but NOT counted above` and its own rule
(*"A gated registration is not a port. Arm it, measure it, then move it."*). It does
**not** connect that list to the namespace rows those registrations would move. Making
it do so converts WAVE22_PLAN §1 from a grep into a printed line. ALLOC has to read that
file anyway.

## 4. SCHEDULING — Wave 21's one mechanical failure

Seven lanes against one game directory cost a lane its measurement runs. **Only ALLOC
and GEOM are expected to need the game directory.** TIMER, ERRCFG and SYSBODY are
binary-and-source work until their verification step; queue that behind ALLOC rather
than racing it. `tools/game_dir_lock.sh` is mandatory and **must never be piped** — a
pipeline reports the LAST command's status, so a timeout reads as a grant.

## 5. RULES FOR EVERY LANE

1. **Re-derive your own inputs first.** A lane that finds this doc stale reports that as
   its headline. That is a successful lane, not a blocked one.
2. **Grep before you port** — three bodies in one wave were re-written that the tree
   already had. Also grep `standalone/` and the `BANISHED_*` list before writing
   "no route / unverifiable".
3. **A body ported but never REGISTERED is indistinguishable from an absent one.**
4. **Do not fix a faithful crash with a null check.** Find the upstream null.
5. **A green readiness screen is not a route.** COMPLETE ≠ promotable; promotability
   turns on owning the function that STAMPS the table.
6. **`calls=0` is not "unexercised"**, and a clean oracle with zero callers is not a
   verification. Read the `calls=` column.
7. **State your denominator**, and **say what you did NOT check**.
8. **UNPROMOTABLE is route-scoped** — say DLL-present or cutover; they can differ.
9. **A static retail WRITES cannot be minted without split brain.** A minted copy that
   nothing publishes into the exported slot is invisible.
10. Private worktree; never `git checkout` in a shared tree.
11. Write findings to `docs/runtime_port/WAVE22_LANE_<NAME>.md`, commit on branch
    `wave22/<lane>`.
