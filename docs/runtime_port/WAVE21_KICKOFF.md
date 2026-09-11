# WAVE 21 KICKOFF — Graphics, and the re-derivation of Wave 20 §8

Branch base: `wave21/base` at `5ce25be7`. Opened 2026-09-11.

## 0. THE RULE THIS WAVE INHERITS, AND HOW IT IS ENFORCED HERE

WAVE20_CLOSED §4: **a handover's cheap-wins list is its stalest section and the first
one acted on.** Wave 20 paid for that three times. This kickoff therefore contains
**no unre-derived claim**. Every number in §2 was produced by a command run at kickoff
time, printed beside it. Anything this document could not re-derive is marked
`[NOT RE-DERIVED]` and is a lane's FIRST task, not a fact.

**Every lane's first action is to re-run its own re-derivation command and report
whether the kickoff was right.** A lane that finds this doc stale reports that as its
headline, not as a footnote. That is a successful lane, not a blocked one.

## 1. THE SCOPING FINDING THAT SHAPES THE WAVE

Wave 20 §8 item 8 named **Graphics, 99 called / 70 ported, ceiling 96.0%** as the
largest untouched gap. **The number re-derives exactly. Its MEANING does not.**

Re-derived at kickoff:
```
tools/ghidra_mcp/.venv/Scripts/python.exe tools/runtime_census.py --csv census21.csv
# then classify the 29 unported Graphics rows by mangled-name kind
```

| kind | n | what it actually is |
|---|---|---|
| BODY | 9 | function reimplementation — real port work |
| CTOR / DTOR | 2 | ditto (`FrameCopyImageBuffer`) |
| operator new | 2 | ditto, small |
| **VTABLE** (`??_7`) | **8** | **promotion work, not body work** |
| **DATA** (`?_typeInfo@`, `?_singleton@`, statics) | **8** | **mint/publish work, not body work** |
| total | 29 | |

**So Graphics 29 is 13 code rows, 8 vtable rows and 8 data rows.** A lane scoped as
"port 29 Graphics functions" would be scoped against a population that does not exist.

**This is the UI lesson repeating verbatim** (memory
`project_ui_namespace_gap_decomposition.md`: UI's gap "is NOT N unported functions — 13
were minted DATA"). It was found there in Wave 10 by a lane that spent itself
discovering it, and it is written down in a memory. **Nothing in the census prints the
decomposition, so nothing forced anyone to look.** Same shape as §5.10's denominator
failure: the fact exists, is recorded, and no instrument asks for it.

> **CANDIDATE DOCTRINE (for a lane to confirm or kill):** a namespace gap figure is
> uninterpretable until decomposed by symbol kind, and the census should print that
> decomposition rather than leave each wave to rediscover it.

### The 29 rows, classified

```
CTOR      ??0FrameCopyImageBuffer@Graphics@@QEAA@XZ
DTOR      ??1FrameCopyImageBuffer@Graphics@@UEAA@XZ
OPNEW     ??2FrameCopyImageBuffer@Graphics@@SAPEAX_K@Z
OPNEW     ??2MaterialInstance@Graphics@@SAPEAX_K@Z
VTABLE    ??_7AnimationEvent@Graphics@@6B@              (backlog: ours 2 / slots 4)
VTABLE    ??_7Geometry@Graphics@@6B@                    (backlog: ours 6 / slots 6  COMPLETE)
VTABLE    ??_7Light@Graphics@@6B@                       (backlog: ours 5 / slots 6)
VTABLE    ??_7MaterialInstance@Graphics@@6B@            (backlog: ours 2 / slots 9)
VTABLE    ??_7Particle@Graphics@@6B@                    (backlog: ours 2 / slots 6)
VTABLE    ??_7ParticleInstance@Graphics@@6B@            (backlog: ours 5 / slots 9)
VTABLE    ??_7ParticleSystemInstance@Graphics@@6B@      (backlog: ours 5 / slots 10)
VTABLE    ??_7SunLight@Graphics@@6B@                    (backlog: ours 5 / slots 11)
BODY      ?AddDrawItems@MeshInstance@Graphics@@UEBAXAEBVScene@2@AEAVRenderer@2@@Z
BODY      ?BuildParameters@MaterialInstance@Graphics@@QEAAXXZ
BODY      ?Create@FrameCopyImageBuffer@Graphics@@UEAA_N...
BODY      ?Draw@Renderer@Graphics@@QEAAXAEAVContext@Video@@MAEBVScene@2@AEBVCamera@2@@Z
BODY      ?Generate@FrameCopyImageBuffer@Graphics@@UEBAXAEAVContext@Video@@H@Z
BODY      ?Serialize@AnimationManager@Graphics@@QEAAXAEAVSerializer@IO@@@Z
BODY      ?Serialize@MeshInstance@Graphics@@QEAAXAEAVSerializer@IO@@@Z
BODY      ?Setup@MaterialInstance@Graphics@@QEBAXAEAVContext@Video@@H@Z
BODY      ?Update@AnimationManager@Graphics@@QEAAXAEBVTime@System@@_N@Z
DATA      ?_frameIndex@ParticleInstance@Graphics@@1HA
DATA      ?_rng@Particle@Graphics@@1VRandom@System@@A
DATA      ?_rootOnlyUpdates@MeshInstance@Graphics@@0_NA
DATA      ?_singleton@AnimationManager@Graphics@@0PEAV12@EA
DATA      ?_typeInfo@GeneratedImageBuffer@Graphics@@0VBaseTypeInfo@Object@@A
DATA      ?_typeInfo@Material@Graphics@@0VBaseTypeInfo@Object@@A
DATA      ?_typeInfo@MaterialInstance@Graphics@@0VBaseTypeInfo@Object@@A
DATA      ?_typeInfo@Particle@Graphics@@0VBaseTypeInfo@Object@@A
```

Note `??_7EdgeMesh@`, `??_7EdgeMeshInstance@` and `??_7MeshInstance@` appear in the
census PROMOTION BACKLOG but **not** in this unported list. That discrepancy is lane
GFXVT's first re-derivation question, not a kickoff claim.

## 2. THE RE-DERIVED BOARD

```
namespace      total  called  ported  detour   covered   ceiling
IO               407     216     200       0     92.6%     97.7%
UI              1625     208     203       5     97.6%     99.0%
internal                 182     141       0     77.5%
Video            573     158     140       0     88.6%     88.6%  AT CEILING
System           262     103      78       2     75.7%
Graphics         599      99      70       0     70.7%     96.0%
Math             228      64      64       0    100.0%
Object           191      61      51       2     83.6%     83.6%  AT CEILING
Achievement      118      52      36       0     69.2%
Engine           108      33      23       0     69.7%
Audio            137      30      23       0     76.7%
Save              37      30      29       0     96.7%
Input             63      29      25       0     86.2%
Application       26      16      15       0     93.8%     93.8%  AT CEILING
Volume            64      13      13       0    100.0%
Picking           49       9       9       0    100.0%
ordinal                    6       2       0     33.3%
Utility           16       2       2       0    100.0%
TOTAL           4632    1311    1124       9     85.7%
```

Unchanged from the Wave 20 close. **Do not read 85.7% as movement** — WAVE20_CLOSED §2.

**Score a namespace on the IDENTITY of its gap, never the percentage** — `ceiling` has
`called` in both halves and moves whenever a lane opens a door (memory
`reference_the_ceiling_percentage_is_a_moving_target.md`).

Raw gaps by size: internal 41, Graphics 29, System 25, IO 16, Achievement 16, Engine 10.
**None of these has been decomposed by §1's classifier except Graphics.** Doing so for
the rest is a cheap instrument change, not a survey — lane CLASS owns it.

## 3. §8 ITEM RE-DERIVATION STATUS AT KICKOFF

| # | item | status at kickoff | owner |
|---|---|---|---|
| 1 | `0x1801554a8` encoding vtable, 3 slots | **RE-DERIVES TRUE** — census backlog prints ours 2 / slots 5 | ENC |
| 2 | verify 8 promoted `BinaryFileStream` bodies | `[NOT RE-DERIVED]` — probe exists at `runtime/src/io/file_stream_slots_probe.cpp` | VFY |
| 3 | `??_7TextFileStream@` trap slots 0/2/8 | **PARTIALLY RE-DERIVED** — backlog prints ours 7 / slots 10 | ENC |
| 4 | 8-table COMPLETE gap, 28 live vs 36 static | `[NOT RE-DERIVED]` | VFY |
| 5 | what "close the screenshot hole" MEANS | `[NOT RE-DERIVED]` | SHOT |
| 6 | transitive half of door set, was 4 short | `[NOT RE-DERIVED]` | SHOT |
| 7 | `?SetupFileSystem@Modification@`, PackageFile cluster | `[NOT RE-DERIVED]` | (unscoped) |
| 8 | Graphics largest gap | **RE-DERIVES TRUE, MEANING CORRECTED** — see §1 | GFX lanes |
| 9 | cutover wall `?PlatformConfigure@Engine@1@` | `[NOT RE-DERIVED, and Wave 20 did not re-verify it either]` | (unscoped) |
| 10 | P2 residue-of-residue widenings | `[NOT RE-DERIVED]` | (unscoped) |
| 11 | implement `RT_UNPROMOTABLE` convention check | `[NOT RE-DERIVED]` | CLASS |
| 12 | `??_7ReaderWriter@` `?` numerator needs an ADDRESSEE | **ADDRESSEE ASSIGNED: lane ENC** | ENC |
| 13 | nothing checks denominators but a person asking | `[NOT RE-DERIVED]` — and §1 is a fresh instance of it | CLASS |

Item 12 is discharged by this table, not by a lane's work: an expiry naming a condition
and no addressee never fires (memory `reference_expiry_clause_needs_an_addressee.md`).
**If lane ENC does not run, item 12 reverts to unassigned and must be re-assigned at
close.**

Items 7, 9 and 10 are **deliberately unscoped this wave** and remain `[NOT RE-DERIVED]`.
Do not read their absence as a judgement that they are small.

## 4. LANES

| lane | subject | writes code? |
|---|---|---|
| **GFXBODY** | the 9 BODY rows + ctor/dtor/2 operator new — Graphics real port work | yes |
| **GFXVT** | the 8 Graphics VTABLE rows — promotion readiness and what STAMPS each | yes |
| **GFXDATA** | the 8 Graphics DATA rows — mint/publish | yes |
| **ENC** | items 1, 3, 12 — encoding and TextFileStream vtable slots | yes |
| **VFY** | items 2, 4 — verify what is already dispatched | yes |
| **SHOT** | items 5, 6 — decide what the screenshot hole MEANS before scoping it | analysis first |
| **CLASS** | §1 candidate doctrine + items 11, 13 — instruments | yes |

## 5. RULES FOR EVERY LANE

1. **Re-derive your own inputs first.** Report staleness as a headline.
2. **Grep before you port.** Three times in one wave a lane ported what the tree already
   had (memory `reference_grep_for_the_port_before_writing_it.md`). Also grep
   `standalone/` and the `BANISHED_*` list before writing "no route / unverifiable"
   (`reference_grep_the_harness_before_writing_unverifiable.md`).
3. **Do not fix a faithful crash with a null check** (CLAUDE.md). Find the upstream null.
4. **A green readiness screen is not a route.** COMPLETE says a promotion puts no trap in
   a live table. It does NOT say the row is promotable — that turns on owning the
   function that STAMPS the table
   (`reference_a_ported_row_can_be_permanently_unpromotable.md`).
5. **`calls=0` is not "unexercised"**, and a clean diff oracle with zero callers is not a
   verification. Read the `calls=` column, and remember a body can run every time and
   always take the branch the oracle does not cover.
6. **State your denominator.** Every count names the population it was taken against.
   §5.10 of the last close is an open wound; do not add to it.
7. **Say what you did NOT check.** A screen that cannot name its subject inflates
   (`reference_a_screen_that_cannot_name_its_subject.md`).
8. Work in your own worktree. Never `git checkout` in a shared tree
   (`reference_parallel_lanes_need_private_worktrees.md`).
9. Building and running the game is serialised by `tools/game_dir_lock.sh`. Never pipe an
   acquire into anything — a pipeline reports the LAST command's status, so a timeout
   then reads as a grant (`reference_a_piped_acquire_discards_the_refusal.md`).
10. Write findings to `docs/runtime_port/WAVE21_LANE_<NAME>.md` and commit on your own
    branch. Report what you found, what you changed, and what you could not check.
