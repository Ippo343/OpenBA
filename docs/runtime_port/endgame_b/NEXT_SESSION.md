# Next session plan (written 2026-10-09, end of B6 — the retail-difference task)

**B6 is CLOSED.** The 63 "retail-difference task" casts are gone: every one was the symptom of a place
where the tree did something retail does not, and each difference is fixed in its own `behaviour:` commit
that quotes the retail instructions (160 `behaviour:` commits, one per retail BODY; 11 gated merges, 0
problems; `b6_session/GATE_TABLE.md`). The PROD metric is **26** = 23 named prerequisite + 3 ruled + **0
retail-difference + 0 work**; acceptance has ONE line off (10 = 19, the accepted template-field miscount);
`worldcmp.sh` vs `oldbase` identical. Row 13 (`TradeWork::Validate`'s stale entry) is SETTLED: it was the
mirrored find-remove core 0x14002c690, fixed on main by a4ba1f7ed (row 742) — bisected and re-proven.
The resave fixture was RE-CUT (row 990: the old `Gate.sav` held 11 null-profession citizens born under the
tree's old birth path; retail never makes one). Numbers: `b6_session/CLOSE_NUMBERS.md`; rule sheet
`B6_RULES.md` (§7 C1-C8); inventory, gate table, audit in `b6_session/`.

## 0. Scope, as the user set it (unchanged)

The DANGEROUS casts go completely; only safe casts stay. The number is
`python docs/runtime_port/endgame_b/b4_session/castmetric.py code runtime standalone dx11 engine` (26).
A raw-storage member becomes a real member only in a `behaviour:` commit that reproduces retail's constructor
AND destructor; a guard retail lacks is dropped (B6_RULES D4), never added; a faithful crash is root-caused
upstream — and the root may be a FIXTURE (C5). Device-lost stays DEFERRED (§3).

## 1. Where it stands

| | |
|---|---|
| main | `a8e786963` (CLOSE-D: the audit applied, after `Merge b6-CLOSEC` c3f50319a, the last gated code merge) + the close documents |
| metric | 89 -> **26**: code 13, runtime 13, standalone 0, engine 0, dx11 0 (`LEFT_FREE.md` §C: 23 named prerequisite + 3 ruled) |
| invariants | new game 5683 / 0, tutorial 5773 / 12, typetable `b983f144`, resave 5/5 on every merge against the RE-CUT golden (md5 `3efc80a84907fbb8e25d64b4f2c99cff`), 0 new warnings, unit **920 / 0** |
| acceptance | 1 line off (10 = 19, accepted); 01 = 26 of target 26; 02-09, 11, 12 at 0 |
| known_port_issues | §13 SETTLED; §14 rows: 111 `FIXED (B6 …)`, 2 `NOT A DIFFERENCE`, the 14d bullets DROPPED with addresses or KEPT under D4(a)/(b) with the harness route / PORT GAP named; new rows 900, 920-922, 940-944, 960-962, 990 |
| close checks | `worldcmp.sh` vs `oldbase` (`b2a5434c`) identical (118 lines, seed 12345); `stepclose.sh` (CLOSE_NUMBERS §6); audit `b6_session/AUDIT.md` |
| worktrees | only `oldbase` (`b2a5434c`) remains |

## 2. The next step: the named-prerequisite pass (`LEFT_FREE.md` §C, 23 casts -> 0)

**What it is.** The 23 casts left outside the 3 ruled ones each name a PREREQUISITE the cast waits on.
They fall into four units; each unit is a small B11-measured commit series, gated as B6 (per merge:
`b6_session/mergegate.sh` copied to `b7_session/`, resave x5, `worldcmp.sh` at the close).

1. **The inline-release pass (13 casts, one blocker).** `System::String`'s ctor / copy / release and
   `rtcoll::ArrayDtorImpl` compile to out-of-line calls where retail open-codes them (the S4 RT2 / LAST
   measurements quoted on each line). Make them inlinable (a header-defined release with the same block
   header reads, `-O2` B11 against retail's shape at the five measured callers), then type the locals:
   `runtime/src/ui/documenttext.cpp:155-159` (GetNextItem's String / Filename locals), `runtime/src/io/modification.cpp:272-287`
   (the by-value Array<uint32_t> / Array<uint8_t> shapes), `runtime/src/collection/avl_trees.cpp:268`,
   `runtime/src/collection/introsort.cpp:355`, `runtime/src/graphics/wave24_lane_gfxmat.cpp:363` (a
   `System::String MaterialTexture::name` once AddTexture 0x180103350 / Draw 0x1800c2eb0 match). B11 is the gate
   for this unit: the typed local must not add a call retail lacks.
2. **The ctor doors (4 casts).** `System::Random` gets its C++ ctor (0x1800c7f20 is already a port, called
   as a door by weather_particles / noiseui / newgamedialog / `exe_bodies.h:274`'s 2520-byte raw local:
   `sizeof(Random)` 2500 vs the local's 2520 — measure which is right); `Engine` (`game_main.cpp:334`, a
   stack Engine built by a ctor door + memset); `Renderer` / `Scene` in `minimap_paint.cpp:173 / :175`
   (ctor doors over stack storage with a MID-FUNCTION dtor: a scoped block gives retail's destruction
   point). Each is a `behaviour:` commit if a store changes (the memset), else `B7:`.
3. **The zeroing-ctor locals (4 casts).** `::Frustum` (`gameupdateloop_load.cpp:574 / :577 / :800`) and
   `Math::Vector4` (`gameupdateloop.cpp:1416`, `Context.h`'s float[4] row): retail hands an untouched stack
   block to the perspective ctor 0x1800fa1e0 and stores the row with one MOVUPS. Rule: give Frustum /
   Vector4 a trivial default ctor (no zeroing) ONLY if no reader relies on the zero (grep every
   value-initialised Frustum / Vector4; B4 S4 listed none), else a tagged `Frustum(NoInit)` ctor. B11.
4. **The two leftovers.** `runtime/src/collection/set_clear.cpp:210` (the `RtIo::SettingsMap` header viewed as
   `Collection::Set<Pair<String,String>>`: its owners Persistence / Settings get the Set member — an H5
   `behaviour:` commit with 0x18000f7d0 as the Clear) and `code/Game/Game/goals_serialize.cpp:86`
   (`WorkCellBST_Remove_ca60`: one body over five node / header types by ICF — a template over the node
   type, as `AvlRebalance<>` was done in B4). Plus the two harness puns (`citizenaicomponent.cpp:620`,
   `clearareacomponent.cpp:191`): retire the force-work / ClearPlace harness probes or keep them listed
   (user's call; they are test code).

**Also in the step, as the B6 residue (no cast, same task):** the §14 rows D did not reach — 82, 83, 84,
89, 78, 98, 99, 250, 251, 252, 291, 330, 350, 352, 451, 443, 692 (INVENTORY §B.5 verdicts), row 942
(`IsDerivedFromOrTrust` ignores its type: every caller must first get its retail serializer's type cell,
as rows 2 / 4 did), G1 (unenumerated), G28's sibling bullets of row 310, the GatherRawMaterials site-ctor
/ SendComponentMessage / Pf_RegionPairCacheTick guards, rows 920-922 / 940-944 / 960-962, and the two
LIVE PORT GAP wirings the user decides: `NaturalResourceComponent` slot 8 (row 5) and `SpawnSubResource`
(entry 3). Row 34 (the launcher dialog) needs a port of 0x1800a1ae0.

**Shape (B6's, which held for eleven merges):** Block 0 read-only inventory (one row per cast with the
measured blocker) -> `B7_RULES.md` -> units 2-4 as three opus blocks in worktrees (disjoint files, `-j2`,
static gate only) while the coordinator does unit 1 serially (it is one runtime change with five
measured callers) -> gate per merge -> close (acceptance line 01's target falls 26 -> 3; LEFT_FREE §C;
`worldcmp.sh`; `stepclose.sh`; audit).

## 3. Device-lost (DEFERRED by the user — read before touching it)

Unchanged from B5: `b5_session/DEVLOST_FINDING.md`, known_port_issues row 890. The leading hypothesis is
the exe PORT GAP pair `GameUpdateLoop::OnVideoError / OnVideoRestart` (0x140095970 / 0x140095930). Do it
only when the user lifts the deferral; `devlost.sh` keeps `DLMAX=5`.

## 4. After that

- **The quality pass:** 234 free functions that are members of a class in retail (`LEFT_FREE.md` A);
  `exe_bodies.h` shrinks per class.
- **ENDGAME B.1 decision 7** (exceptions), the PORT GAPs (the device-lost pair among them), then device-lost.
- The B6-found retail quirks copied as-is (docs/retail_bugs.md §2 row 452's over-read; row 961's pathfinder
  race; row 44's never-notify; row 49's stale dword): a list to revisit only if the user wants a
  "fixed" build flavour someday — NOT part of ENDGAME B.

## 5. Working rules that held (do not relax) and what B6 added

- Agents never run the game (B6's one exception, TRADE, ran the stress route under the lock as holder
  `trade` — and a gate waiting on that lock is fine); builds `-j2`, at most four building agents; the
  coordinator alone gates; the gate builds the MERGED tree before the merge commit exists; commit the
  gate table BEFORE `mergegate.sh`; a fresh worktree is reset to a NAMED commit.
- NEW: **one `behaviour:` commit per retail BODY** with the instruction range read, BEFORE / AFTER, the
  observable, what was RUN (§14g); rows that share one body share one commit (C1).
- NEW: **a guard retail lacks goes** unless the site's comment names a harness route, a shared panel
  template or the PORT GAP it covers (D4); a dropped guard that then crashes a route is a FINDING to
  root-cause upstream — **and the root may be a fixture** (C5: the resave save held citizens no retail
  path makes).
- NEW: **a resave PASS with a non-empty crash log** is "the loaded game died AFTER the roundtrip":
  symbolize `standalone_crash.log` with nm (image base 0x140000000) before anything else.
- NEW: **a worktree's copy of `runs/warn_baseline_*.txt` is CRLF** (`tr -d '\r'` before `comm`, C3);
  `mergegate.sh` skips the `ui` / `tools` routes on a non-zero result — run them by hand afterwards.
- NEW: **`keepboth.py` doubles a table row both sides edited**; the next merge from a branch that
  merged main de-duplicates it, or do it by hand (rows 7-10 / 76 / 90 / 92 / 95 in UI's second merge).
- NEW: a block's hand-in that names a row done by another block (UI vs D on rows 2 / 4 / 11 / 311 / 653)
  is resolved by the LATER merger keeping main's body; tell blocks the moment a row changes hands.
- Never `git add` the session directory wholesale (`runs/`, `gate_*.out`, merge logs are untracked).
