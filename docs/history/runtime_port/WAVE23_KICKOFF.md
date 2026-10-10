# WAVE 23 KICKOFF — ten lanes, and the whole board dealt out by name

Base: `wave23/base`, cut from `wave22/base` at `fdeddf9a` (Wave 22's four closed lanes
merged; **lane ALLOC still running and NOT included**). Opened 2026-09-11.

## 0. THE BOARD, RE-DERIVED AT KICKOFF ON THIS TIP

```
tools/ghidra_mcp/.venv/Scripts/python.exe tools/runtime_census.py --csv w23.csv
```
```
TOTAL   4632   1314 called   1137 ported   9 detour   86.5%
=> 42 of 177 gap rows (24%) are a function to reimplement.

namespace       gap |  BODY CTOR DTOR OPNEWDEL VTABLE DATA UNKNOWN
internal         42 |     0    0    0        0      0    0      42
Graphics         25 |     8    1    1        0      8    7       0
Video            19 |     1    0    0        0     12    6       0
System           17 |     5    0    0        0      1   11       0   <- ALLOC owns; NOT a Wave 23 lane
Achievement      16 |     7    0    1        0      4    4       0
IO               16 |     3    0    1        0      8    4       0
Engine           10 |     3    0    0        0      4    3       0
Object           10 |     0    0    0        0      4    6       0
Audio             7 |     2    1    2        0      0    2       0
UI                5 |     2    0    0        1      2    0       0
Input             4 |     1    0    1        0      0    2       0
ordinal           4 |     0    0    0        0      0    0       4
Application       1 |     0    0    0        0      0    1       0
Save              1 |     1    0    0        0      0    0       0
```

**Ten lanes cover all 177 rows.** Every lane gets its rows BY NAME in §2 — no lane is
scoped against a total. A gap figure is uninterpretable until decomposed; that lesson has
now been paid for four times (UI/Wave 10, Graphics/Wave 21, System/Wave 22, and twice
inside Wave 22 alone).

## 1. TWO HARD CONSTRAINTS

### 1a. FILES LANE ALLOC OWNS — DO NOT TOUCH

Lane ALLOC of Wave 22 is **still running** against the same repo and will land on
`wave22/base` after you start. Do not edit:

```
runtime/src/system/allocator.cpp        runtime/src/system/allocator_win.cpp
runtime/src/system/rt_system_port.cpp   tools/runtime_census.py
```

If your work genuinely requires one, **stop and report it to the integrator** instead of
editing. The integrator merges `wave22/base` into `wave23/base` before any Wave 23 merge.

### 1b. THE GAME DIRECTORY IS ONE RESOURCE AND ALLOC HOLDS IT

Wave 21's one mechanical failure was seven lanes racing one game directory; a lane lost its
measurement runs and its ports closed STRUCTURAL. **Ten lanes cannot each get a run.**

- **Assume you are binary-and-source work.** Ghidra, `pefile`, source greps, and input
  sweeps against shipped exports need no game directory.
- If you need a run, acquire `tools/game_dir_lock.sh` **bare — NEVER piped** (a pipeline
  reports the LAST command's status, so a timeout reads as a grant), hold it briefly,
  release it.
- **"I could not get a run" is an acceptable and expected outcome.** Say so plainly.
  Do NOT fabricate verification, and do NOT write "unverifiable" without first grepping
  `standalone/` and the `BANISHED_*` env list — `tool_harness.cpp` has repeatedly already
  held the driver.

## 2. LANES AND THEIR ROWS

| lane | subject | rows |
|---|---|---|
| **GFXBODY** | Graphics function rows | 8 BODY + `??0FrameCopyImageBuffer` + `??1FrameCopyImageBuffer` |
| **GFXVT** | Graphics vtables | 8 VTABLE |
| **GFXDATA** | Graphics statics | 7 DATA |
| **ACHIEVE** | all of Achievement | 7 BODY + 1 DTOR + 4 VTABLE + 4 DATA |
| **VIDVT** | all of Video | 12 VTABLE + 6 DATA + `?SetupShaders@VideoProgram@` |
| **IOPORT** | all of IO | 3 BODY + 1 DTOR + 4 DATA + 8 VTABLE |
| **OBJ** | all of Object | 4 VTABLE + 6 DATA |
| **ENGINE** | all of Engine | 3 BODY + 4 VTABLE + 3 DATA |
| **TAIL** | Audio + UI + Input + Save + Application | 18 rows, 5 namespaces |
| **INTERNAL** | the 42 `UNKNOWN` internal rows | an INSTRUMENT lane, not a porting lane |

## 3. THREE ROWS THAT ARE WORTH MORE THAN THEIR COUNT

- **`?PlatformConfigure@Engine@1@`** (lane ENGINE) — the cutover boot wall, `0x1800a1670`,
  EXEC-of-0. `[NOT RE-DERIVED]` for **three waves**. Wave 20 item 9.
- **`?LoadResources@UpdateLoop@Engine@@`** (lane ENGINE, `0x1800dd7d0`) — Wave 22 lane
  ERRCFG proved this plus an 8-byte accessor are the **complete** unported reader set of
  `?_singleton@Configuration@System@@`. One body unblocks a DATA row in another namespace.
- **`?SetupFileSystem@Modification@IO@@`** (lane IOPORT) — Wave 20 §8 item 7, unscoped for
  two waves.

## 4. RULES FOR EVERY LANE — these are paid-for, not style

1. **Re-derive your own inputs first.** A lane that finds this doc stale reports that as
   its headline. That is a successful lane.
2. **Grep before you port.** `?StrCompare@String@System@@` had a correct body since Wave 2
   and read as unported for **twenty waves** because nobody registered it. A body ported
   but never REGISTERED is indistinguishable from an absent one. This has now cost the
   project three separate rediscoveries.
3. **Re-check a stated blocker before acting on it — INCLUDING YOUR OWN, however recent.**
   Wave 22 lane SYSBODY discharged a nine-wave-old holdback that was false by mechanism,
   and then retracted two of its own residuals that were **one turn old**.
4. **A green readiness screen is not a route.** `COMPLETE` says a promotion puts no trap in
   a live table. It does NOT say the row is promotable — that turns on owning the function
   that STAMPS the table, and an in-DLL rel32 call or RIP-relative LEA stamp is not a door.
5. **Promotion verdicts are ROUTE-SCOPED.** Say DLL-present AND cutover; they routinely
   differ, and two rows both tagged "NONE EVER" once meant opposite things.
6. **Before minting a DATA row, ask who READS it in the DLL, not just who writes it.**
   Unported DLL internals read statics as hard-coded RVAs baked into `.text` — no door we
   can intercept. Unpromoted, our write falls through to the real DLL address and they stay
   correct *by construction*; promoting freezes the DLL's copy and strands them. Wave 22
   lanes TIMER and ERRCFG both reached **zero promotions** on this reasoning, and ERRCFG
   refuted it for 3 of its 5 rows by answering each on its own xrefs rather than by analogy.
7. **Do not fix a faithful crash with a null check.** Find the upstream null.
8. **`calls=0` is not "unexercised"**, and a clean oracle with zero callers is not a
   verification. Read the `calls=` column.
9. **A negative control must corrupt something the probe actually compares.** A control
   that corrupts an unobservable value reports NOT CAUGHT and indicts a sound instrument;
   the mirror also bites. Run one, and say what it caught and what it did not.
10. **State your denominator. Say what you did NOT check.**
11. **An addressee is a NAME, not a condition.** "The next lane to touch this file" never
    fires. Name a wave lane, hand it to the integrator, or write a check that fails loudly.
12. **Never run `find /`.** Three Wave 22 lanes did; they saturated three cores and
    contaminated a perf A/B. Memory files live under
    `~/.claude/projects/C--code-GIT-untitled-rev-eng-project/memory/` (index `MEMORY.md`)
    and `~/.claude/memory/`. Python: `tools/ghidra_mcp/.venv/Scripts/python.exe`.
13. **Your census figures are BRANCH-LOCAL.** Label them so. The integrator re-measures on
    the merged tip — Wave 21 published from lane reports and had to correct itself, and in
    Wave 22 the merged System figure (86) matched **neither** lane that moved it (81, 83).
14. Private worktree; never `git checkout` in a shared tree. Write
    `docs/runtime_port/WAVE23_LANE_<NAME>.md`, commit on `wave23/<lane>`, merge nothing.
