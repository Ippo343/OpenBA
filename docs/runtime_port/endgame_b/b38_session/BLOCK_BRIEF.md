# B.3.8 class-block brief (every step-8 class block runs under this)

You convert a named CLASS SET of the exe-phase classes (`code/Game`, the `standalone/*_stubs.cpp`
loop tables, or — block RB only — the Runtime bases they derive from) into real C++ classes with
COMPILER-GENERATED vtables, on your own worktree branch. The coordinator merges and gates. Your
prompt names your block, your class set and your branch; your FILE SET is your block's section of
`B38_FILEMAP.md`. This brief is everything else. It is the B.3.7 brief with the step-8 rules; where
the two differ, this one wins.

## Read first, in this order
1. `CLAUDE.md` (hard rules: no null-check "fixes" of faithful crashes, never `getenv`, never an
   unscoped filesystem search — exclude `standalone/build*/` and `tests/unit/build/`, no `pacman`).
2. `docs/runtime_port/ENDGAME.md` B.1 and B.2 (retail layout kept, slots in retail order, slot 0 is
   `DeletingDtor(unsigned)`, NO virtual destructor; B.1 decision 2's multiple-inheritance paragraph is
   CORRECTED by B38_RULES §0.1: there is none).
3. `docs/runtime_port/endgame_b/B38_RULES.md` — the WHOLE sheet. D1-D10 and rules B8-1..14 bind you;
   so do `B37_RULES.md` §8 / §10 / §14 (by-value arguments per type B7-5, qualified calls inside
   slot bodies, sweep by demangled owner), `B36_RULES.md` B6-1..11 (+ §10, §14) and `B35_RULES.md`
   B5-11 (no side effect in a printf argument), B5-12 (by-value returns), B5-17 (distinct names for
   overloaded virtuals).
4. `docs/runtime_port/endgame_b/B38_SLOTS.md` — the RETAIL slot lists (§6), sizes, parent literals
   (§3), member tables. COPY from it. The name/parent literals in `B38_RULES.md` §1.3 are EXACT
   (saves hash the name).
5. `docs/runtime_port/endgame_b/B38_FILEMAP.md` — your file set (§1) and the coordinator-owned files
   (§2) with the lines your block needs in them.
6. The inventories for your classes: `b38_session/INVENTORY_A.md` (components, descriptions, actions,
   works), `INVENTORY_B.md` (dialogs, panels, loops, goals, tutorial, player),
   `SEAM_AND_FILEMAP.md` (§1 the RegisterExeAddr seam: which slot-body doors are reached by address
   and so stay; which vtable addresses are read back by address and by whom).
7. THE PILOT — the worked example you imitate, on main: `code/Game/Object/exe_type.h`,
   `code/Game/Component/{Component,ControllerComponent,Description,WellComponent,WellDescription}.h`,
   `component.cpp`, `wellcomponent.cpp`, `welldescription.cpp`, `controller_state_table.h`,
   `tests/unit/exe_class_abi_test.cpp`; `B38_RULES.md` §7. The Runtime bases:
   `runtime/types/object/{Base,External,Internal}.h`, and for UI/Engine/Achievement/Graphics bases
   the headers under `runtime/types/<ns>/` (RB gives them real protected ctors, D1).

## The shape (per class, D10's order)
1. Header `code/Game/<Dir>/<Class>.h` (the pilot's decision: the header lives with its `.cpp`, named
   after the class). `class X : public <proved parent>`; same fields, same order, explicit padding
   kept; every `static_assert(offsetof)` kept, `sizeof` AND `alignof` added. Virtuals in RETAIL slot
   order, one line per slot with the retail address in the comment; `override` where the parent
   declares the slot. A slot retail fills with the PARENT's body is INHERITED (not declared). A slot
   retail implements but the tree stubs/inerts is declared and marked `PORT GAP` (D3). Nothing past
   retail's slot count (grep for dispatches past it before deleting padding, B8-6). Distinct names for
   overloaded virtuals. Retslot virtuals (`Description` slot 8, `Goal::GetInstanceType`) are
   `Object::Type* M(Object::Type* ret) const` (B8-11). By-value arguments per B7-5: read the prologue.
2. REAL ctor (public on a concrete class, protected on an abstract base) = retail's body minus its
   vptr stores, the base's real ctor in the init list (B8-1). `DeletingDtor(unsigned) override` +
   protected non-virtual `~X()` chaining the base (B8-2). `Object::Destroy(p)` replaces
   `CallSlot0(p, 1)`. A class whose tree slot 0 SKIPPED a non-empty base dtor, whose null/nop slots
   become retail bodies, whose reflected factory changes from a stub to the real class, or whose
   PORT GAP body you transcribe: its OWN commit titled `… (behaviour: …)` (B8-3) — list each in your
   report so the coordinator gates it with resave x5.
3. Bodies MOVE into members (`self->x` → `x`). The old C entry point stays as a one-line QUALIFIED
   delegation ONLY if `SEAM_AND_FILEMAP.md` §1.2 (or your own sweep) says it is reached by ADDRESS
   (`ResolveAddr`, `RegisterExeAddr` fan-out, dispatch slot, another table); otherwise it is deleted
   with the table. Inside a slot body, a call to a member that is VIRTUAL on the base is QUALIFIED
   (`Base::M()`), or it recurses.
4. `DEFINE_EXE_TYPE` (exe_type.h) replaces `REGISTER_TYPE` IN PLACE (registration order is kept on
   purpose, D4 / B8-7; literals EXACTLY from B38_RULES §1.3). Per-class lazy `GetTypeIdByName` caches
   become the class static `TypeId()` (D4, B6-5).
5. The hand-built array / inert / live factory table, its fill helpers and its `Register*Vtable`
   function go. Its `RegisterExeAddr(kVtable, …)` row is KEPT serving `X::Vtable()` while any
   by-address reader of that table remains anywhere in the tree (D5, B8-8; sweep by hex AND demangled
   owner, B8-12), else deleted. A reader that stamps a converted class's table must build the same
   bytes the ctor builds (B8-5): convert readers INSIDE your file set to the real ctor.
6. The `engine/types` POD becomes a forwarder to the class header in the same commit (B8-4, never two
   definitions of one name). Shared POD headers (`Dialogs.h`, `Panels.h`, …) are coordinator-owned:
   author the per-class header in your file set and put the hunk that removes YOUR struct from the
   shared header in your report (or, if B38_FILEMAP §2 allows it for your block, apply it minimally).
7. Unit test `tests/unit/<block>_abi_test.cpp` (B8-14): sizeof, member offsets, slot order via PMF
   (adjustment 0), abstractness, `!std::has_virtual_destructor`, `Vtable()` == the vptr of a
   constructed object, one raw retail-order call per retslot / hidden-pointer member. `UNIT_SRCS` is
   a wildcard: never edit `tests/unit/sources.mk`.
8. THREADS (D7 / B8-10): the loader thread builds components, descriptions, actions and works while
   the main thread draws. No function-local static, lazy singleton or non-atomic first-call cache in
   any member you write; a namespace-scope atomic slot (`AtomicExport` shape) or a class static
   initialised at static-init time is the pattern. No lock added.
9. Retail-proved port bugs your block owns (B38_RULES D9 table) are fixed faithfully, each in its own
   titled behaviour-change commit. Retail slips are transcribed, not fixed.
10. Callers OUTSIDE your file set stay untouched (doors and `X::Vtable()` keep them working); the
   caller batches come after. Mark every cast you add at a converted call whose receiver is still a
   `void*`/POD with `// B.3.8 bridge` (a `reinterpret_cast`, so B.4 counts it).
11. Member bodies at global scope or inside `namespace X {`; three classes are spelled `Scene` —
   always qualify. Sweep forward declarations (`struct X;` → `class X;`).

## Process rules
- Work ONLY in your worktree, on your branch. First: `git checkout -b <branch>` then `git merge main`
  (fast-forward) and confirm `git log -1` shows the coordinator's launch commit named in your prompt.
- Touch ONLY your file set (+ new files you create in your block's directories + your unit test).
  Outside it: the minimum, listed in your report under "outside file set"; never reformat; never drop
  existing code when adding to a shared file.
- FORBIDDEN: `git stash`, `git commit --amend`, rebase, push, force anything, spawning sub-agents
  that edit files, `pacman`, unscoped filesystem searches, `getenv`, hand-editing `.inc` files,
  null-check "fixes".
- Commit small (one class or one family per commit) via `git commit -F <file>` (no heredoc through
  the shell; avoid the section sign and arrows in the message), title `ENDGAME B.3.8 <block>: <what>`,
  ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- STATIC GATE before each commit, in the FOREGROUND with a 600000 ms timeout, exit code read from
  the command itself (`make … > log 2>&1; echo $?`), logs under
  `C:/Users/USPC/AppData/Local/Temp/claude/<your-branch>/`:
  `/c/msys64/ucrt64/bin/mingw32-make.exe -j8 standalone-debug`, `standalone-perf`, `unit` (all exit
  0, unit 0 failures), `python tools/dispatch_slot_map.py --check` (0 mismatches),
  `python tools/cutover_manifest.py --gate`, `python tools/rt_mint_gen.py --check` (then
  `git checkout -- standalone/rt_export_addrs.inc` if it only changed line endings). No NEW warnings
  in files you touched (debug AND perf flags; the compiler is behind ccache so a warning diff is
  honest — compare against a build of your branch point, B37_RULES 14.7).
- NO GAME RUN. The game directory starves past three parties: you do NOT take the game-dir lock and
  do NOT launch the exe. The coordinator gates every merge with the full route set (new game,
  tutorial, Load Game, resave x3/x5). Say in your report which of your commits are behaviour changes
  so the coordinator can bisect.
- Before your final build: `git merge main` again (keep both sides in append-only shared files;
  check that no conflict marker split a closing brace), rebuild, re-run the static gate.
- Ghidra (if you need it): the bridge is shared and `switch_program` is GLOBAL — pass
  `program="Application-x64.exe"` on every call that accepts it instead of switching; if you must
  switch, assert `get_current_program_info` before every read batch and switch BACK to
  `Runtime-x64.dll`. Ghidra's decompiled C drops arguments on indirect calls and forwarders: read the
  disassembly. Scripts must be Java. Never rename or save in Ghidra from a block.
- If your context gets deep: commit what is verified, and report exactly which classes are left.
  A shorter honest block beats a risky one; the coordinator re-launches the remainder.

## Report (short)
Branch + commit list (mark the behaviour-change commits); classes converted with slot count / sizeof
/ parent; tables deleted; doors kept (reached by address) vs deleted; `RegisterExeAddr` vtable rows
kept (with the remaining reader) vs deleted; `// B.3.8 bridge` casts added; D9 bugs fixed; anything
in B38_RULES / B38_SLOTS / B38_FILEMAP you found WRONG (with the retail evidence); hunks for
coordinator-owned files; files touched outside your file set; static gate results verbatim; what you
did NOT do and why.
