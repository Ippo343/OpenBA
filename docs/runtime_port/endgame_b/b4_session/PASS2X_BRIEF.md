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

## The line procedure (rounds 6 onward: completeness)
The job is to read EVERY remaining cast line of your partition, file by file, and leave none unread. Your
lines are in `data/pass2x/assign/X3-<P>.lines.tsv` (regenerated before each round by `round3.py`; the reason
tagged on each line is often STALE: the tree has moved). For each line decide and act:
1. **The type exists now** -> convert by name; delete the stale tag.
2. **It waits on a declaration defined in your partition** -> the five steps above (typed declaration and
   every caller in one commit; a pure forwarder is replaced by the member and deleted; a function with no
   caller is deleted).
3. **A hand-written array resize** -> read retail (the address is in the function's name or comment). If
   it is step-for-step the engine's `SetSize`, the body becomes the ONE erased core through an element-spec
   row (as `StorageComponent_DynArrayResize` and the CitizenBehavior rows are); otherwise the body stays,
   typed by element. Typed WRAPPERS over the one core only: never a per-type copy, never a container swap.
4. **Raw storage of a non-trivial object (H5)** -> per CLASS, one `behaviour:` commit quoting retail's
   constructor AND destructor (read the DISASSEMBLY), only where the typed member reproduces retail's
   construction / destruction order exactly (members are destroyed in reverse declaration order after the
   dtor body), the hand-written ctor stops zeroing it, the dtor stops releasing it, nothing constructs it
   twice, and nothing memsets / memcpys the class or asserts it trivial. Where neighbours block the order,
   type the neighbours in the same commit (CitizenBehavior, EntityList, the Work leaves and Persistence-like
   classes were done that way). A class local with a constexpr / zeroing constructor ADDS stores: use a
   constructor-less POD struct, or leave. A finding recorded in the ledger is not retried without its
   named prerequisite.
5. **A POD mirror of a real class** -> the real class where the mirror is field-identical and the lifetime
   is the same; delete a mirror / bridge overload when nothing names it.
6. **A genuine reinterpretation FLOOR.md defines** -> `// CAST-OK: <category>` (M2: never on a `+ 0x…`
   field offset or a `self` / `this` operand). Vtable-word probes, callback `void*` contexts, the erased
   core's views of its own receiver and casts inside macros stay `LEFT: FLOOR? <what the cast does>` for
   the audit. `FindOrLoad` / `FindNext` const results stay tagged CONST. Allocation results the metric's
   tier-I shape does not recognise stay, each naming its allocator.
7. **Only a behaviour change would remove it** -> it stays, with a precise self-contained reason; a retail
   difference found on the way goes to `docs/known_port_issues.md` entry 14, never fixed.
Always: no null guard added or dropped; no log text changed; H7 split stores write every byte; H8 bytes stay
bytes; no door; no getenv. B11 at -O2 and -O0 with every DIFF read (a store whose width changes writes the
same bytes with the same values; a store or call that APPEARS is reverted or explained from retail).
At hand-in say exactly which files you finished (every line read) and which you did not reach.
