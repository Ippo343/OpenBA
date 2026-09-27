# B.3 step 9 — block 0 (inventory) brief, read-only

Coordinator brief, 2026-09-26, main @ ab1e0846. Step 9 is the END of B.3: it RETIRES the migration
scaffolding that steps 1-8 needed while hand-built and compiler vtables coexisted — the mint
(`standalone/rt_data_mint_tables.inc`: 152 minted vtables, 104 `BaseTypeInfo` records, 48 storage
cells), the `RT_DELDTOR` table (`runtime/src/object/deleting_dtor.cpp` + `deleting_dtor_leaves.cpp`),
the ctor / dtor doors, the `RegisterExeAddr` rows of vtable addresses, the type-id cells
(`g_tid_*` / `g_TypeId_*`, `ResolveAddr(Module::Exe, 0x1401c…/0x1401d…)`) and the singleton storage.
The plan is `docs/runtime_port/endgame_b/NEXT_SESSION.md` section 3 — read it in full; 3b is what
block 0 answers. The retirement ORDER is `B38_RULES.md` D10 ("END of B.3").

Block 0 is READ-ONLY: you edit NO source, header, `.inc`, Makefile or tool. You write ONLY the one
output file your prompt names (plus helper scripts under
`C:/Users/USPC/AppData/Local/Temp/claude/b39/<your-id>/`). Nothing is built or run by you: a build
and the game gate are running in the main tree while you read.

## Read first
1. `CLAUDE.md` (hard rules: never `find /` or any unscoped search — exclude `standalone/build*/`,
   `tests/unit/build/` and `.claude/worktrees/` from every grep; never `getenv`; no `pacman`).
2. `docs/runtime_port/endgame_b/NEXT_SESSION.md` section 3 (3a-3d) and section 2 (carry-overs).
3. `docs/runtime_port/endgame_b/B38_RULES.md`: D1, D2, D4, D5, D10, section 4 (rules), section 6
   (NOT VERIFIED), section 10 (deliberately left), section 11.
4. `docs/runtime_port/endgame_b/b38_session/CLOSE_NUMBERS.md`.
5. `docs/runtime_port/ENDGAME.md` B.1 (decision 4: `DEFINE_TYPE`) and B.3.

## Conventions
- A caller / reader / stamper is found BY BINDING, never by name alone. The spellings seen so far:
  `ResolveExport("…")`, `RT_EXPORT_ONCE`, `RT_VTABLE_ONCE`, `RT_DATA_SLOT`, `RtResolveCached`,
  `ResolveAddr(Module::Runtime|Exe, 0x…)`, per-file wrappers (`RtAddr<Fn>(0x…)`, `RtAddrFn`,
  `ByAddr(0x…)`, `AtomicExport` / `AtomicAddr`, an address in a named `constexpr`, `FooAddr::Vtable`),
  `engine_funcs.{h,cpp}` globals BY INITIALISER (and `engine_funcs.h`'s own inline bodies), exe import
  slots (`0x14015…` IAT addresses in `code/Game`), generated rows in `rt_export_addrs.inc`.
  A comment, a `printf` / `static_assert` string or a doc citing an address is NOT a caller.
- Every claim carries `file:line` (tree) or an address (retail). Say how you measured (the grep /
  script) at the end. Be exhaustive where the prompt says "every"; say "not measured" rather than
  guess. A count without its grep is worthless to the rule-sheet owner.
- Retail truth: Ghidra program `Application-x64.exe` for the exe, `Runtime-x64.dll` for the DLL.
  The bridge is SHARED: pass `program="Application-x64.exe"` / `"Runtime-x64.dll"` on every call
  that takes it; NEVER `switch_program`; no rename, no annotation, no save. Ghidra's decompiled C
  drops arguments on indirect calls and forwarders: read the disassembly for anything about
  arguments. Ghidra scripts must be Java.
- Output: tables first (one row per item, machine-greppable), prose after. End with a section
  "Retirement consequences" (what can be deleted at once, what needs a conversion first, what must
  stay and why) and a section "Not measured".
