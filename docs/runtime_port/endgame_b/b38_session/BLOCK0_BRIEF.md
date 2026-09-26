# B.3 step 8 — block 0 (inventory) brief, read-only

Coordinator brief, 2026-09-26, main @ 1d7866d4. Step 8 (`ENDGAME.md` B.3 line 8) converts the
exe-phase classes in `code/Game` (components, descriptions, dialogs, controllers, panels, tools,
actions, works, goals, loops, tutorial) into real C++ classes whose vtables the compiler generates,
deriving from the Runtime bases that steps 3-7 made real. The plan is
`docs/runtime_port/endgame_b/NEXT_SESSION.md` section 2 — read it in full; section 2b is what block
0 answers. Block 0 is READ-ONLY: you edit NO source, header, `.inc`, Makefile or tool. You write
ONLY the one output file your prompt names (plus helper scripts under
`C:/Users/USPC/AppData/Local/Temp/claude/b38/<your-id>/`). Nothing is built or run by you.

## Read first
1. `CLAUDE.md` (hard rules: never `find /` or any unscoped search — exclude `standalone/build*/`
   and `tests/unit/build/` from every grep; never `getenv`; no `pacman`).
2. `docs/runtime_port/ENDGAME.md` B.1 (decisions 1, 2, 4) and B.2 (the per-class recipe).
3. `docs/runtime_port/endgame_b/NEXT_SESSION.md` section 2 (2a-2d).
4. Models of what a finished step-7 inventory looked like: `b37_session/EXE_SUBCLASSES.md`,
   `b36_session/EXE_SUBCLASSES.md`, `STEP7_INVENTORY.md`, `B37_FILEMAP.md` (first 120 lines).
5. The finished Runtime bases the exe classes sit on: `runtime/types/object/{Base,External,
   Internal,CompiledExternal}.h`, `runtime/types/ui/{Element,ElementDescription,Dialog*}.h`
   (DialogInstance/DialogController shapes), `runtime/types/engine/{UpdateLoop,LoaderLoop,
   DialogLoopT,DialogLoaderLoopT}.h`, `runtime/types/achievement/*.h`.

## Conventions (same as step 7)
- "slot N" = vtable index N (byte offset 8N). "own" = the file installs its own body; "base" =
  the slot holds or reimplements the Runtime / exe base's virtual; "stub" = a standalone body that
  reimplements or no-ops what retail inherits; "inert" = `ComponentFactory::InertStubSlot()` (NOT
  reimplemented); "null" = literal `nullptr`.
- Every claim carries `file:line` (tree) or an address (retail). Say how you measured (the grep /
  script) at the end, like `b37_session/EXE_SUBCLASSES.md` section 5.
- Retail truth: Ghidra program `Application-x64.exe` (11473 functions) for the exe, `Runtime-x64.dll`
  (15290) for the DLL. The Ghidra bridge is SHARED and `switch_program` is GLOBAL: assert
  `get_current_program_info` before EVERY read batch and switch back to `Runtime-x64.dll` when you
  are done. Ghidra's decompiled C drops arguments on indirect calls and forwarders: read the
  disassembly for anything about arguments. Ghidra scripts must be Java.
- Be exhaustive where the prompt says "every"; say "not measured" rather than guess. A count
  without its grep is worthless to the rule-sheet owner.
