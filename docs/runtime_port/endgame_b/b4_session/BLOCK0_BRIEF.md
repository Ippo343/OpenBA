# Block 0 brief — B.4, the cast metric (read-only inventories)

Read first: `../NEXT_SESSION.md` §3 (the step, all of it), `../../ENDGAME.md` B.4 (the end state and
the table of cast causes), `.claude/rules/type-headers.md`, `CLAUDE.md` (hard rules).

## What the step is
The bodies are C-shaped: `void*` receivers, `reinterpret_cast<T*>(p + 0x…)` field reads, C functions
declared in `code/Game/<Module>/exe_bodies.h` and `Rt<Ns>_<Class>_<Member>` bodies in `runtime/src`.
B.4 makes a function that retail compiled as a member a MEMBER of its class, its receiver `this`, a
field read a read by NAME. Block 0 does NOT edit product code: each owner answers one question and
writes ONE inventory file plus ONE machine-readable table. The coordinator writes the rule sheet
(`../B4_RULES.md`) and the class / file map from the inventories.

## The questions (one owner each)
| letter | question (NEXT_SESSION §3b) | table columns (TSV, header row) |
|---|---|---|
| A `exe_bodies` | §3b.1 for `code/Game`: every declaration in the nine `exe_bodies.h` (and the other `*_funcs.h` C-body headers): which class is it a member of (retail `this` use, the vtable holding it, the file it is defined in), is that class a real C++ class with a header, is the member already DECLARED there, is it DEFINED, how many call sites | `addr name module defining_file class class_header member_declared member_defined static_or_member callers confidence note` |
| B `runtime_bodies` | §3b.1 for `runtime/src`: every `Rt<Ns>_<Class>_<Member>` / `extern "C"` / free C body: same columns; plus members declared in `runtime/types` with NO definition | same as A |
| C `offsets` | §3b.2 and §3b.5: every raw offset read in PROD code (`p + 0x…` under any cast spelling, `[idx]` on a cast pointer): the class of the receiver (from the enclosing function / the variable's origin), the offset, the width and signedness read, whether the class header declares a field at that offset (named + asserted / not declared / guess name `field_0x..`); per class: which MODULES read it (the cross-module classes whose header must be finished first) | `file line class offset width_type header field_name state(named|undeclared|guess|unknown_class) module` |
| D `big_objects` | §3b.3: full layouts from RETAIL (Ghidra, `program="Application-x64.exe"`) of `GameUpdateLoop` (every `_singleton + 0x…` read in `code/Game`), `CitizenComponent`, the settings block `g_gameSettings` (0x34 bytes, cell 0x1401c3de0), the Game container `g_gameContainer` (cell 0x1401c3de8): offset, width, type, proposed name with the evidence (which retail function reads / writes it, a serialized field name, a string), who should own the header | `object offset size type proposed_name evidence readers_in_tree confidence` |
| E `floor_and_tests` | §3b.4 and §3b.7: the LEGITIMATE casts, by file: serialized byte streams, allocator block headers, the dx11 COM seam, `Collection::Array<T>` storage, hashing, `VSlot.h` vtable diagnostics, the hardware-watchpoint code; and the 526 casts of `tests/unit`: which are the test's subject (stay), which fall when the class gets a member | `file casts_total legit_count category reason` |
| F `all_spellings` | §3b.6: the ONE summed metric. Count every spelling of a reinterpretation per directory and per file: `reinterpret_cast`, C-style pointer casts (exact: `-Wold-style-cast`), functional casts to pointer / `uintptr_t`, `static_cast` to / from `void*` or through `uintptr_t` / `intptr_t`, `const_cast`, `memcpy` / union type puns of a pointer. Classify each `static_cast`: hierarchy downcast after a type check (stays), arithmetic conversion (stays), crossing unrelated types (goes). Deliver the script that IS the metric: `docs/runtime_port/endgame_b/b4_session/castmetric.py DIR… [--by-file]`, deterministic, no build needed to re-run it (the compiler pass is only your cross-check; say how far the script is from the compiler's count) | `file reinterpret cstyle_ptr functional static_void static_uintptr static_cross const_cast pun total` |

## Rules for counting (each cost a wrong number before)
- Count CODE lines only: `python docs/runtime_port/endgame_b/b3r_session/codegrep.py PATTERN DIR… [--count]`
  strips comments and keeps strings. A hit in a comment or in `docs/` is not code.
- PROD = `code/ runtime/ standalone/ dx11/ engine/`; `tests/` is its own row.
- Exclude `standalone/build*/`, `tests/unit/build/`, `.claude/worktrees/`. Anchor exclusions on a path
  separator.
- The measure undercounts until the code has been read. When a grep number and a read disagree, the
  read wins and the inventory says by how much. Sample at least 40 rows of your own table by hand
  and report the error rate.
- "Member exists" means a DEFINITION exists, not a declaration.
- A class is named from retail evidence (the vtable that holds the function, `this` use, the mangled
  export name in Runtime-x64.dll), not from our C body's name: body names are known to mislead
  (`docs/known_port_issues.md` entry 6).

## Hard rules
- READ-ONLY on product code, tests, tools and generated files. You write only
  `docs/runtime_port/endgame_b/b4_session/INVENTORY_<letter>_<NAME>.md`,
  `docs/runtime_port/endgame_b/b4_session/data/<letter>_<name>.tsv` and the script that produced the
  table, `docs/runtime_port/endgame_b/b4_session/scripts/<letter>_<name>.py` (re-runs from the repo
  root with no arguments and reproduces the table). F also writes `b4_session/castmetric.py`.
- Scratch files go in `<scratchpad>/b4/<your letter>/` only.
- Do NOT run the game, do NOT touch `C:/GOG Games/Banished`, do NOT run `mingw32-make` (a gate build
  is running and shares the object directories). F alone may run the compiler with
  `-fsyntax-only -Wold-style-cast`, at most 2 processes at once, output in its scratch directory,
  taking the flags from the Makefile; nothing else compiles.
- Never `find /` or any unscoped filesystem search; Grep / Glob scoped to the repo.
- Never `git stash`, `git commit`, `git checkout`, `git reset`: the coordinator commits.
- No sub-agents.
- Ghidra is read-only: `check_connection` first, `program=` on EVERY call
  (`Application-x64.exe` or `Runtime-x64.dll`), never `switch_program`, rename nothing, save nothing.
  Ghidra's decompiler drops arguments on indirect calls and register-passthrough forwarders: when a
  signature matters read the disassembly.

## The inventory file (under 400 lines; the table carries the rows)
1. The answer to your question in ten lines or fewer, with the numbers.
2. How the table was built: the patterns, what each column means, how to re-run.
3. Counts per class / per directory; the top 30 classes or files.
4. **Blind spots**: what your method cannot see, and what you read by hand to cover it.
5. **What surprised you**: every place where the code contradicts `NEXT_SESSION.md` §3a or a comment.
6. A proposal for how the conversion splits into blocks with DISJOINT FILE SETS (a block owns
   CLASSES' headers plus the files that define their members), with the file count and cast count
   of each, and what must come first. Name anything that would change behaviour (a width or
   signedness retail contradicts, a receiver that is not the class the body name says).

## Hand-in (your final message, under 40 lines)
The paths you wrote, the headline numbers, the blind spots, the surprises.
