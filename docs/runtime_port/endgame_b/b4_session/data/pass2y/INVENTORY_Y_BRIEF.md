# B.4 wave Y — the re-inventory by CLASS (read-only)

Repo: `C:/code/GIT/untitled_rev_eng_project` (Banished reimplementation, C++). The project removes every
"dangerous" cast (a raw-offset read, a `void*` / integer reinterpreted as a class). After wave X round 7
about 870 counted casts remain outside `dx11/`, every one tagged on its line (`// LEFT: <reason>` or
`// CAST-OK: <category>`). Yield per round has fallen because what is left is per-CLASS work: a raw-storage
member can only become a real member in a `behaviour:` commit that reproduces retail's constructor AND
destructor order, often together with its neighbours. The rounds so far were cut from line tags, which
are stale and name the symptom, not the class. This inventory keys the remainder by the CLASS (or declaration)
whose change removes the casts, so the next rounds are cut from facts.

You change NOTHING in the repo. No builds, no game runs, no git writes, no search outside the repo (never
`find /`). Use Grep / Glob / Read scoped to the repo and Ghidra read-only (`program="Application-x64.exe"`
for `code/`, `"Runtime-x64.dll"` for `runtime/`). Write only the output files below.

## Inputs (`<INVY>` = this directory)
- `lines_<REGION>.tsv` — every counted cast of your region: `file, line, kind, left_tag, snippet`
- Background: `../../PASS2X_BRIEF.md` (rules, the five steps, the line procedure), `../../PASS2_LEDGER.md`
  (findings that block a class: do not re-derive what is written there — verify it still holds and cite it),
  `../../FLOOR.md` (floor categories), `docs/known_port_issues.md` entry 14 (recorded retail differences).

## What to produce
For every cast line of your region, the UNIT whose change removes it, using exactly these keys:
- `CLASS:<Ns::Class>` — a raw-storage / wrongly-typed MEMBER of this class (H5 / H4 / FIELD). For each class
  record: the members involved (name, offset, what retail holds there); retail's ctor and dtor addresses and
  the ORDER in which the dtor releases the members (read the disassembly: this is the fact every round
  needed and rarely had); which NEIGHBOURS must be typed in the same commit for reverse-declaration-order
  destruction to equal retail's; every reader file in the tree (Grep each member name; a file-local mirror
  struct of the class counts); whether anything memsets / memcpys / static_asserts the class trivial; the
  verdict `doable` / `doable-with:<neighbours>` / `blocked:<why>` (an ORDER that no member declaration can
  reproduce, a double construction, a memset — cite the ledger finding if it exists).
- `SIG:<function>` — a declaration whose parameter / return is `void*` / integer: declared_at, defined_at,
  current and typed signature (from the body / retail), every caller file, whether a pure forwarder.
- `POD:<mirror>` — a POD mirror type or a bridge (`Sys()`, `SysFile()`, `RtGraphics::` doors ...): the real
  class, field-identical yes/no, every file naming it, what blocks replacing it.
- `ERASED:<entry point>` — a type-erased container core's entry point / spec row.
- `RULING:<class>` — a line that only a coordinator ruling moves: `vptr_probe`, `callback_ctx`,
  `erased_self`, `macro`, `alloc_shape:<allocator>`, `const_find` (FindOrLoad / FindNext), `retail_diff:<row>`,
  `h8`, `dx11_seam`. These are NOT work: count them so the close can rule once per class.
- `LOCAL:<what>` — nothing outside the line blocks it (stale tag).

## Outputs
1. `<INVY>/out/<REGION>.tsv` — `file, line, unit, note` (one row per cast line).
2. `<INVY>/out/<REGION>_units.tsv` — one row per unit:
   `unit, casts, verdict, retail_ctor, retail_dtor, dtor_order, neighbours, readers(files), blockers, notes`.
3. `<INVY>/out/<REGION>.md` (under 80 lines): units ordered by casts; the `doable` classes first with their
   one-commit scope; the `blocked` ones with the exact finding; the RULING totals per class.
Final message: under 12 lines — paths, casts classified, the ten largest units with verdicts. No file dumps.
