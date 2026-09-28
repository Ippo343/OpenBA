# Block 0 brief — the registry teardown (read-only inventories)

Read first: `../NEXT_SESSION.md` §3 (the step), `../../ENDGAME.md` B.2 and B.4 (the end state),
`standalone/resolve.h` and `standalone/resolve_standalone.cpp` (the three registries: exe address map,
runtime name map, runtime address map; the name and address doors mirror each other through
`standalone/rt_export_addrs.inc`).

## What the step is
A real class needs no door. Every row that registers one of OUR bodies under a retail name or a retail
address, and every resolve that reads such a row, is converted to a direct C++ reference or deleted.
Block 0 does NOT edit product code: it answers one question each and writes ONE inventory file plus
ONE machine-readable table. The coordinator writes the rule sheet from the inventories.

## Row classes (use exactly these words)
| class | meaning |
|---|---|
| `DEAD` | no reader by name AND none by address (code lines only): the row is deleted |
| `DIRECT` | every reader can name the C++ function / member / object the row points at: convert the reader, then the row is DEAD |
| `STORAGE` | the row is a data cell (singleton pointer, static, constant): becomes a class static or a namespace-scope object |
| `STUB` | the target is a `NOT IMPLEMENTED` stub, a PORT GAP or unported: stays, listed by name |
| `CALLBACK` | the address is stored in DATA (a table, a serialized id, a state-machine row, a vtable define) and resolved later: cannot be found by grepping names |

## Rules for counting (every one of these cost a wrong number before)
- Count CODE lines only: `python docs/runtime_port/endgame_b/b3x_session/codegrep.py PATTERN DIR...`
  strips comments and keeps strings. A hit in `docs/`, in a `// 0x18…` provenance comment or in a
  printf / static_assert STRING is not a reader.
- Exclude `standalone/build*/`, `tests/unit/build/`, `.claude/worktrees/`. Anchor exclusions on a path
  separator (`\build` once swallowed `buildinggoal.cpp`).
- Find readers by BINDING, never by the pointer's name or its comment. The spellings:
  1. `ResolveAddr(Module::Exe, 0x140151xxx / 0x140152xxx)` — a PE import slot of the retail exe (map the
     literal through `tools/dispatch_slot_map.py`'s table to the Runtime symbol it imports)
  2. helpers: `RSlot`, `ResolveSlotPtr<T>`, file-local `ResolveSlot(addr, cache, tried, tag)`,
     `loadExref` lambdas, `ResolveFn<T>`
  3. `ResolveExport("…")` with the literal intact, including calls wrapped over two lines
  4. a two-statement `uintptr_t iat` scratch local rebound positionally
  5. plain aliases and round trips
  6. globals bound in `engine/funcs/engine_funcs.cpp` and called elsewhere (bound by INITIALISER)
  7. handle-struct fields (`g_h.x`)
  8. inline `RT_EXPORT_ONCE`
  9. a mangled name held as `const char*` in an initialised table resolved by a loop
  10. per-file by-address wrappers: `RtAddr<Fn>(0x18…)`, `RtAddrFn`, `ArrayDtor(addr, …)`,
      `ArraySetSize`, `SetSizeWithFill`, `PlainArrayDtor`, `ByAddr(addr)`, an address in a named
      `constexpr` or a `k…Addr[]` table, `rtuielem::AtomicExport / AtomicAddr`, `RtResolveCached`
  List any spelling you meet that is not on this list.
- The measure undercounts until the code has been read. When a grep number and a read disagree, the
  read wins and the inventory says by how much.

## Hard rules
- READ-ONLY on product code, tests, tools and generated files. You write only your inventory file
  `docs/runtime_port/endgame_b/b3r_session/INVENTORY_<letter>_<NAME>.md`, your table
  `docs/runtime_port/endgame_b/b3r_session/data/<letter>_<name>.tsv`, and the script that produced
  the table, `docs/runtime_port/endgame_b/b3r_session/scripts/<letter>_<name>.py` (it must re-run
  from the repo root with no arguments and reproduce the table).
- Scratch files go in `<scratchpad>/b3r/<your letter>/` only, never a bare file in the scratchpad root.
- Do NOT build, do NOT run the game, do NOT touch `C:/GOG Games/Banished`. A gate build is running.
- Never `find /` or any unscoped filesystem search; Grep / Glob scoped to the repo.
- Never `git stash`, `git commit`, `git checkout`, `git reset`: the coordinator commits.
- No sub-agents.
- Ghidra is optional and read-only: `check_connection` first, `program=` on every call, never
  `switch_program`, rename nothing.

## The inventory file (under 400 lines; the table carries the rows)
1. The answer to your question in ten lines or fewer, with the numbers.
2. How the table was built: the patterns, what each column means, how to re-run.
3. The count per row class, per directory.
4. **Blind spots**: what your method cannot see, and what you read by hand to cover it.
5. **What surprised you**: every place where the code contradicts `NEXT_SESSION.md` §3a or a comment.
6. A proposal for how the conversion of YOUR rows splits into blocks with DISJOINT FILE SETS
   (blocks own files, not classes), with the file count and row count of each, and what must come
   first. Name anything that would change behaviour: those are separate titled commits.

## Hand-in (your final message, under 40 lines)
The three paths you wrote, the headline numbers, the blind spots, the surprises.
