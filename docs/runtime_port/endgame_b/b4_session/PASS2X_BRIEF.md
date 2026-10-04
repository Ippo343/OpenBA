# B.4 second pass — wave X (declaration families)

Read after `B4_RULES.md`, `BLOCK_BRIEF.md`, `W1_BRIEF.md` and `PASS2_BRIEF.md`. Everything in PASS2_BRIEF
holds (the rules, the B11 object comparison with `b11cmp.py`, the working discipline, the hand-in format);
this file wins where they differ.

## What changed since waves L / R
Every file has been visited once. What is left (2,896 casts outside `dx11/` at `3b606e4b`) waits on a
DECLARATION that several files share: a C signature taking `void*` / an integer, a virtual slot's parameter,
a POD mirror type, a type-erased entry point. The inventory is in `data/pass2x/`:
- `X<region>.tsv` — one row per cast line: `file, line, family, note`
- `X<region>_families.tsv` — one row per declaration family: where it is declared and defined, its current
  and its typed signature, the function it forwards to, and EVERY file in the tree that names it
- `X<region>.md` — the families by size, the forwarders with no caller left, the stale `// LEFT` comments
- `assign/<agent>.tsv` — YOUR families (or lines), cut so that no two running agents share a file

The inventory was made by scripts and greps: a `typed_signature` marked body-derived is a proposal — read
the body and the retail function (the address is in the name or the declaring comment) before you believe
it; `all_caller_files` is a superset for member names that several classes share. Grep each name yourself
before you change it (`code runtime standalone dx11 engine tests`; a local `extern` IS a caller; the ten
binding spellings of `feedback: sweep callers by binding` apply to runtime names).

## Your unit of work is the family, not the file
For each family in your assignment, in one commit per family (or per small group that shares a type):
1. Change the declaration and the definition to the typed signature. The retail function decides the type;
   a parameter the body reads as two different classes stays what it is and the family is LEFT with that
   finding.
2. Fix EVERY caller in the same commit — in every file, whatever region: remove the caller's cast where the
   argument already has the type; where the caller itself only holds a `void*` / integer that came from
   ANOTHER untyped declaration, keep one cast there, original spelling, `// LEFT: SIG <that declaration>`.
3. A pure one-line forwarder: replace its callers by the member it forwards to and delete it with its
   declaration. A forwarder that holds a guard, an order, or a second statement is NOT pure: type it, keep it.
4. A function with no caller left anywhere: delete it with its declaration (read the whole body first; a
   body with a side effect that something relied on is listed, not deleted).
5. Then the cast lines of the family listed in `X<region>.tsv` should be gone: check with
   `castmetric.py --by-file --tsv` before / after. No column may rise in any file (a move is reported).

Stale `// LEFT` comments on lines you touch are deleted with the cast, or corrected if the cast stays.

## Boundaries
- You own the files your families name. Files outside them are read-only, except that step 2 obliges you to
  fix a caller wherever it is: the assignments are cut so that those callers are not in another running
  agent's set. If you find a caller in a file of another running agent (the prompt names their sets), keep
  the old signature callable for it (do NOT fork the body: leave the old-typed declaration as the one-line
  forwarder to the typed function, marked `// B4 FORWARD: <file> still calls this`) and list it.
- Shared declaration headers (`*/exe_bodies.h`, `*_funcs.h`): edit only the lines of your families; never
  reorder or reformat.
- Class headers: a member's type changes only for the families / lines in your assignment; H5 members only
  in a `behaviour:` commit quoting retail's ctor AND dtor (PASS2_BRIEF).
- No null guard added or dropped; no log text changed; no container swap; raw bytes stay bytes (H8); split
  stores write every byte (H7); nothing registered or resolved by retail name or address.
- You never run the game. Builds at `-j2`, foreground. Scratch in an untracked `b4scratch/`.
- `git merge main` before the final build; `python docs/runtime_port/endgame_b/b4_session/keepboth.py
  docs/known_port_issues.md` resolves the append-only conflict.

## Hand-in (under 60 lines)
Branch and last commit; builds (debug / perf / unit: 918 / 0) and new warnings; metric base -> tip for your
files and the tree; families DONE (count), families LEFT with the one-line reason each; forwarders deleted /
kept with their remaining callers; `behaviour:` commits; B11 totals at -O2 and -O0 with one line per DIFF
kind you read; `known_port_issues` rows added; what you did not verify.

## Rounds 3 and 4: partitions (added by the coordinator)
From round 3 the tree is cut by PARTITION, not by file list (`data/pass2x/round3.py` regenerates
`assign/X3-<P>.lines.tsv`: every remaining cast with the reason tagged on its line):
- **WORK**: `code/Game/Work/`, `code/Game/Action/`, `tests/unit/wrk_abi_test.cpp`
- **COMP**: `code/Game/Component/`, `code/Game/Entity/` (its lines are the rows of `X3-GAME.lines.tsv` under those two directories)
- **GAME**: the rest of `code/`, and `standalone/`
- **RT**: `runtime/`, `engine/`, the other tests
Cross-partition rule: an agent that retypes a declaration or member DEFINED in its partition may edit a
reader / call-site line of it in another partition's file — that line only — and lists each such line. A
declaration defined in another partition is not its own: the cast stays, tagged `// LEFT: SIG <name>`.
Every cast left at hand-in carries a self-contained `// LEFT:` reason ("as above" and "not examined" are not
reasons) or a `// CAST-OK:` marker. Vtable-word probes, callback contexts and the erased core's views of
its own receiver are tagged `// LEFT: FLOOR? <what the cast does>` for the audit's ruling.
