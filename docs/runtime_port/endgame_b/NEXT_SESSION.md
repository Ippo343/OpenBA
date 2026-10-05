# Next session plan (written 2026-10-05: B.4 second pass, wave Y round 1 interrupted)

B.4 (the cast metric) is NOT closed. This session ran the whole second pass down to a re-inventory by
class; wave Y (executing that inventory) had just started when the machine became unstable, and the user
closed the session there. This file says where it stands and how to start. Earlier plans:
`git show 6a3ccdca:docs/runtime_port/endgame_b/NEXT_SESSION.md` (the second-pass plan this session
executed) and the same file at `b2a5434c` (its §0 owed-from-the-teardown, §2 carry-overs and §4
after-B.4 still hold).

## 0. Scope, as the user set it (unchanged)

**The DANGEROUS casts go completely; only safe casts stay.** The number is
`python docs/runtime_port/endgame_b/b4_session/castmetric.py code runtime standalone dx11 engine`.
The differences from retail are a SEPARATE task: `docs/known_port_issues.md` entries 13 and 14 (rows now
run to 704; numbers are global). A raw-storage member becomes a real member only in a `behaviour:` commit
that reproduces retail's constructor AND destructor (that makes the tree EQUAL to retail, and is in
scope); any other difference found is recorded, not fixed.

User decisions this session:
1. Cross-partition work is planned so that no two agents share a file, and asks are carried in a ledger.
2. Templates: typed WRAPPERS over the one ported erased core, never per-type copies.
3. After wave X round 7: **re-inventory by class, then keep drilling to the floor** (not a close).

## 1. Where it stands

| | |
|---|---|
| main | `91709c75` plus this close-out commit |
| metric | **8,950 -> 1,051** this session (46,226 at block 0); 240 of it is `dx11/` (the seam step's), so **811 in scope** (the inventory of §2 was cut one merge earlier, at 1,052 / 812) |
| gated merges this session | 77 (`b4_session/GATE_TABLE.md` rows 45-121); one problem all session (row 119, a warning, fixed) |
| invariants | new game 5683 / 0, tutorial 5773 / 12, typetable `b983f144`, resave 5/5, unit 918 / 0, 0 new warnings |
| stress (quiet machine) | exit 0, update 1.70-1.77 ms/frame (base 1.92) |

How the pass ran (all in `b4_session/`):
- `PASS2_BRIEF.md` — waves S / L / R: 45 file-owning blocks.
- `PASS2X_BRIEF.md` — wave X: declaration families, then PARTITIONS and the per-line procedure (read its
  last two sections).
- `PASS2_LEDGER.md` — what each hand-in left, with the findings that block a member.
- `GATE_TABLE.md` — every merge.
- `data/pass2/`, `data/pass2x/`, `data/pass2y/` — the three inventories and the scripts that cut each
  round (`pack.py`, `round2.py`, `round3.py`, `slice.py`).

## 2. The re-inventory (the starting point): `b4_session/data/pass2y/out/`

812 casts in 306 units, one row per CLASS / declaration with retail's ctor and dtor, the dtor's release
ORDER, the neighbours that must be typed in the same commit, every reader file and a verdict
(`<REGION>_units.tsv`; `<REGION>.md` is the one-page reading; regions RT, CW, GAMEA, GAMEB):

| verdict | casts | what it means |
|---|---:|---|
| doable (197 units) | 408 | execute as the row says; CLASS units are one `behaviour:` commit each |
| ruling-only | 223 | ONE coordinator ruling per class at the close (amend `FLOOR.md` / castmetric's tier-I shape), not agent work: const `FindOrLoad` results 48, recorded retail differences 50, allocation shapes 38, H8 21, erased-core views 16, macros 16, vtable probes 14, callback contexts 9, dx11 seam 5, other 6 |
| blocked, with the finding | about 130 | `RtSystem::String` / `Filename` mirrors still named in 50 files; `Mod` and the erased-container element types; classes the loader memsets (CharacterGrid, Minimap, EventLog); WideString sites (rows 580-582); `MeshInstance::_animation` (0x48 against retail's 0x18) |
| erased cores that stay | about 50 | the one ported container core's own views |

So the floor after the doable work is about 400, about 220 of it settled by rulings.

The inventory is a MAP, not a proof: an executing agent re-reads retail before building on a row. Two
things it overturned:
- The "order reversed" findings for MenuDialogLoopT, TutorialStep, Map and MenuScene hold only when their
  arrays are typed ALONE; typed with their neighbours each is one commit.
- `HighlightComponent` (row 573): the GAMEB inventory read dtor 0x14004c8c0 and says doable; the CW
  inventory cites the old row. Settle it from retail.

## 3. Wave Y round 1: what was running, and what survives

Four agents (one partition each, "execute the doable units, largest first") were cut off twice by power /
machine instability. Nothing unverified is on main.
- **On main, gated:** `91709c75` — `Audio::Scene` and `Picking::Scene` have real C++ constructors (retail
  0x1800e0160; 0x1801258e0 / 0x18007aef0). This unblocks `MenuDialogLoopT` and `Map` as real classes.
- **Branches kept, NOT merged, NOT verified.** Each is the partial work of an interrupted agent: rebuild,
  run B11 and read the diffs, or redo from the inventory row (a takeover verifies first):
  - `b4y1-CW` (`d022ad8f`): the reused scratch slots as typed locals.
  - `b4y1-GAMEA` (`3c290ee0`): `behaviour:` TutorialStep's two Strings and three arrays.
  - `b4y1-GAMEB` (`b711e4a9`): WIP saved by the coordinator, never built — HighlightComponent.
  - `b4y1-RT`: nothing beyond the merged prerequisites.
- Decided mid-round: `CitizenComponent::meshInstance` becomes a real member built where retail builds it
  (0x140114808) and destroyed where retail does (0x140114717), together with `consumedTypes`; row 650 is
  then FIXED. Each "built yet" test is replaced by what retail does at that site, not just deleted.

## 4. How to start

1. `git status` clean; `mingw32-make clean && mingw32-make -j4 standalone-debug`; one gate run on main
   (`bash docs/runtime_port/endgame_b/b4_session/gate.sh <label> 5`) to see the invariants of §1 first.
2. Decide the three kept branches: verify-and-merge, or delete and redo.
3. Wave Y by partition (`PASS2X_BRIEF.md`, last two sections): four agents, each "the doable units of
   `data/pass2y/out/<REGION>_units.tsv`, largest first", one unit per commit. The coordinator alone gates:
   `mergegate.sh <branch> <label> "<title>" ui tools`; `finishmerge.sh` after a hand-resolved conflict;
   the append-only `known_port_issues.md` conflict is resolved automatically (`keepboth.py`).
4. Regenerate the inputs before every round (`castmetric.py ... --detail`, then `data/pass2y/slice.py`).
   Re-inventory again when a round's yield drops: it went 200 -> 110 -> 200 -> 60 over rounds 5-7, because
   the line tags go stale faster than the tree changes.
5. Reserved `known_port_issues.md` rows: 705-709 free; 710-749 were handed to the wave-Y agents (unused).

Working rules that held all session (do not relax):
- Agents never run the game; builds at `-j2`; at most four agents.
- The gate builds the MERGED tree before the merge commit exists.
- B11 (`b11cmp.py`, -O2 and -O0) with every DIFF read. It caught zeroing stores added by class locals
  three times: a class local with a constexpr ctor adds stores (use a ctor-less POD).
- No null guard added or dropped.
- Never `git add` the session directory wholesale: the `runs/` evidence is untracked on purpose.

## 5. The close of B.4 (when the doable units are done)

- **The closing wave:** the remaining `B4 FORWARD` forwarders; the mirror types whose last user is gone
  (`RtIo::Persistence` went this session; `RtSystem::String` / `Filename` and `Sys()` / `SysFile()`
  remain); the offset helpers; `LEFT_FREE.md` (every cast left, by reason).
- **The independent read-only AUDIT:** rule on the `RULING:` classes (one decision each); re-cut
  `FLOOR.md`'s ceilings from a READ of every marked line (`crash_walk`, `byte_io`, `elem_stride`,
  `ptr_int` grew); check the on-line reasons; confirm `acceptance.sh` (no doors: the by-address launcher
  lookup was deleted this session).
- **The close:** `stepclose.sh`, quiet `sa-stress`, `devlost.sh` alone, the generated-world comparison
  against `b2a5434c`, the removal tool's CalcPath by hand, `CLOSE_NUMBERS.md`, the STATUS block.
- **Recorded for the audit:**
  - WeatherSimulation's array zero stores precede the PerlinNoise ctor call where retail's follow it
    (gate table row 115).
  - The Set ctor writes its pad dword (rows 640 / 651 / 652).
  - `ClearStorage::Execute` passes the wrong component (row 643) and `HuntComponent::Update` reads the
    pointer bits as ticks (row 511): real defects, recorded, not fixed.

## 6. After B.4

The dx11 seam (`dx11/`, 240 casts: the `dx11_seam` floor and the Video interface), then the
retail-difference task (`known_port_issues.md` 13-14), then what the plan at `b2a5434c` lists in its §4.
Device-lost stays DEFERRED.
