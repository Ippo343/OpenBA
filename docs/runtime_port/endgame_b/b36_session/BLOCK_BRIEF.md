# B.3.6 class-block brief (every UI class block runs under this)

You convert a named CLASS SET of `UI::*` into real C++ classes under retail's
names, on your own worktree branch. The coordinator merges and gates. Your prompt names your
class set, your FILE SET and your branch name; this brief is everything else.

## Read first, in this order
1. `CLAUDE.md` (hard rules: no null-check "fixes", never `getenv`, never an unscoped filesystem
   search, no `pacman`).
2. `docs/runtime_port/ENDGAME.md` B.1 and B.2 (the design: retail layout kept, slots in retail
   order, slot 0 is `DeletingDtor(unsigned)`, NO virtual destructor, no real ctor on a
   polymorphic class).
3. `docs/runtime_port/endgame_b/B36_RULES.md` — the whole sheet. Rules B6-1..10 bind you, and so do
   `B35_RULES.md`'s B5-4, B5-11, B5-12 (as amended), B5-15, B5-16 and `B34B_RULES.md`'s B4b-1..5.
   B36_RULES section 13 lists what is NOT verified: if your class is on it, closing that gap from
   the RETAIL DLL is part of your block (42 classes have no documented layout: RULE B6-8).
4. `docs/runtime_port/endgame_b/B36_SLOTS.md` — raw slot lists, type-name literals, retslot
   exports, member export lists, ctor stamp chains. COPY from it; do not reconstruct.
5. `docs/runtime_port/endgame_b/B36_FILEMAP.md` — which file registers which class's members, and
   the cross-cutting files that are the coordinator's, not yours.
6. `docs/runtime_port/endgame_b/B34_BRIEFS.md` section A (A.1 qualified delegation, A.2 member
   bodies cannot be defined inside a sibling `Rt*` namespace, A.3/A.4 abstract vs concrete +
   protected `= default` ctor + friend holder, A.5 layout probe first, A.6 tooling friction, A.8
   includes) and `B34B_RULES.md` rules B4b-1..5. Look at one finished example before writing:
   `runtime/types/graphics/Material.h` (a step-5 class with its banner, doors and declared-unported
   members), `runtime/types/object/External.h` (the real `Object::External` the Descriptions and
   resources derive from) and `runtime/types/object/Internal.h` (the real `Object::Internal` that
   `Element`, `DialogInstance` and `DialogController` derive from — 4 slots, `Persist` at 3).

## The shape (unchanged from B.3.3 / B.3.4)
- The class is declared in `runtime/types/<ns>/<Header>.h` under the RETAIL namespace and name,
  deriving from its PROVED parent (B36_RULES 2.1 — the `_ParentTypeName` literal; ten leaves derive from an INTERMEDIATE leaf, RULE B6-9). Same fields, same order, explicit padding kept.
  Keep every existing `static_assert(offsetof)`; add `sizeof` AND `alignof` asserts. The old POD
  keeps living in its `Rt*` namespace (B4b-1: if a POD occupies the retail name, move the POD to
  the `Rt*` namespace in the same commit) until its last user converts; where class and POD are
  field-identical prefer ONE definition and an alias over a duplicate layout.
- Virtuals in retail slot order, one line per slot with the retail address in the comment.
  `virtual` (not `override`) where the branch introduces the slot (B6-1: `Construct` is slot 9 on leaf Descriptions and 8 on the resources; `EditElement`
  introduces slot 12). Distinct names for overloaded virtuals (B5-17; UI has none in a vtable,
  but `Child`/`GetRoot`/`SetScissor`/`SetClipRegion`/`GetText` are non-virtual const/non-const or
  arity pairs — the door decides which one a caller meant). Abstract unless something holds the class by value (then concrete +
  protected `= default` ctor + `friend` the holder + banner, B5-4).
- Every exported member in B36_SLOTS section 6's list for your class is DECLARED in the header up front,
  ported or not (an unported one is declared and left undefined with a `// not ported` note only
  if nothing calls it; otherwise port it). Other blocks and the caller batches must never need
  to edit your header.
- Each ported body MOVES into the member (`self->x` becomes `x`). The registered door keeps its
  registration (name and address) and becomes a one-line delegation with a QUALIFIED non-virtual
  call: `reinterpret_cast<Ns::C*>(self)->Ns::C::Method(args)`. A retail null-`this` test stays in
  the door. Registrations are NOT removed, mint rows / deleting-dtor rows / ctor doors are NOT
  retired (B36_RULES 7).
- By-value returns (`InternalPtr<Element>`, `InternalPtr<DialogInstance>`, `ExternalPtr<T>`,
  `System::String`, `UI::String`, `UI::Sprite`, `Video::Viewport`, `ClipRegion`, the LinkedList
  `Iterator`) are hidden-retslot doors: the door placement-news the member's prvalue into the
  caller's slot and returns the slot pointer (B5-12 as amended). THE TWO VIRTUAL ONES — slot 11
  `GetBorder` on every Element and slot 8 `CreateInstance` on every Description — are declared
  `Object::InternalPtr<Element>* M(Object::InternalPtr<Element>* ret, …) const` in RETAIL order
  (RULE B6-2), never as a C++ by-value return.
- Runtime-internal callers INSIDE YOUR FILE SET that reach your classes through
  `RT_EXPORT_ONCE` / `ResolveExport` / `ResolveAddr` / hand-inlined `(*(void***)p)[N]` convert to
  direct member / virtual calls in the same block. When the receiver is still a `void*` or a POD,
  bridge it: `reinterpret_cast<Ns::C*>(p)->M()` with the trailing comment `// B.3.6 bridge`
  (must be `reinterpret_cast`, so the B.4 metric counts it). Find callers by BINDING (ten
  spellings, NEXT_SESSION 2d + B5-14 + spelling 10, the exe import slot), never by name. THE DOOR DECIDES THE OVERLOAD.
- Callers OUTSIDE your file set stay untouched — doors keep them working. The caller batches
  come after the class blocks.
- Member bodies go at GLOBAL scope or inside `namespace Ns {` AFTER the `Rt*` namespace block
  closes; inside `namespace UI` always spell a POD with its `RtUI::` qualifier (a bare
  name binds the class silently). Sweep forward declarations (`struct X;` for a class compiles).
- No side-effecting call inside a `printf`/`TRACE` argument (B5-11: the perf build strips it).
- Add unit coverage in `tests/unit/` for the ABI claims that matter: slot order (a probe subclass
  whose vtable is read back slot by slot against the expected member), sizes, the qualified-call
  rule, the two retslot slots (B6-2). Model: `tests/unit/graphics_geometry_abi_test.cpp` (PART C is
  the retslot-virtual proof), `io_vtable_abi_test.cpp`. `UNIT_SRCS` is a wildcard: do NOT edit
  `tests/unit/sources.mk`.
- UI is CONSTRUCTED ON THE LOADER THREAD (B36_RULES 5, RULE B6-5): no static initialiser, lazy
  singleton or non-atomic first-call cache in any member you write.
- The 42 undocumented layouts (RULE B6-8): the ported `.cpp` bodies carry the offsets as file-local
  constants or raw `+0x..` reads. MOVE them under the retail name in the header, assert the retail
  `sizeof`/`alignof` from B36_SLOTS, name only the bytes a body reads (`_unknownNN` for a byte the
  ctor writes but nothing names; an explicit pad for bytes nothing writes). Do not invent names.

## Process rules
- Work ONLY in your worktree, on your branch. First command: `git merge main` (fast-forward) and
  confirm `git log -1` shows the coordinator's launch commit named in your prompt.
- Touch ONLY your file set. If you need a change outside it, do the minimum, list it in your
  report under "outside file set", and never reformat. Never drop existing code when adding to a
  shared file.
- FORBIDDEN: `git stash`, `git commit --amend`, rebase, push, force anything, spawning
  sub-agents that edit files, `pacman`, unscoped filesystem searches, `getenv`.
- Commit in small commits (one class or one chain per commit), message via `git commit -F file`
  (no heredoc; avoid the section sign and arrows). End each message with
  `Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>`.
- STATIC GATE before each commit, in the FOREGROUND with a 600000 ms timeout, exit code read from
  the command itself (`make ... > log 2>&1; echo $?`), logs under
  `C:/Users/USPC/AppData/Local/Temp/claude/<your-branch>/`:
  `mingw32-make -j12 standalone-debug`, `standalone-perf`, `unit` (all exit 0, unit 0 failures),
  `python tools/dispatch_slot_map.py --check` (0), `python tools/cutover_manifest.py --gate`,
  `python tools/rt_mint_gen.py --check` (then `git checkout -- standalone/rt_export_addrs.inc`
  if it only changed line endings). No NEW warnings in files you touched (debug AND perf flags).
  Make is `/c/msys64/ucrt64/bin/mingw32-make.exe`.
- RUNTIME CHECK, once per chain and once at the end (not per commit): one new-game route under
  the game-dir lock, in the foreground:
  `tools/game_dir_lock.sh acquire <branch> 900` … `mingw32-make sa-run` equivalent from YOUR
  worktree with `BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120`, poll
  with `tools/boot_state.sh --wait 300` (never a grep-until-timeout loop; a quiet log is not
  "loading") … `tools/game_dir_lock.sh release <branch>` ALWAYS, also after a failure or after
  killing a waiter. Expect exit 0, empty `standalone_crash.log`, no `standalone_hang.log`,
  `[RtResolve] 0 misses`, no `[RtMint] TRAP`, `calls=5686`. `[AutoStart]` lines are in
  `standalone_error.log`. If the exe dies read `standalone_crash.log` FIRST. A crash on an
  unguarded deref is never fixed with a null check: find what handed you the null.
  If the lock is not granted within 15 minutes, skip the runtime check, say so in the report,
  and carry on — the coordinator gates the merged tree anyway.
- Before your final build: `git merge main` again (keep both sides in append-only shared files;
  check no conflict marker split a closing brace), rebuild, re-run the static gate.
- Ghidra (if you need it): the bridge is shared and `switch_program` is GLOBAL. Before every
  read batch assert `get_current_program_info` = `Runtime-x64.dll`, 15290 functions (retail). The
  profile DLL (16002) is for names only, never offsets. Switch BACK. Ghidra's decompiled C drops
  args on indirect calls and forwarders: read the disassembly. Scripts must be Java.

## Report (short)
Branch + commit list; classes converted with slot count / sizeof / parent; bodies moved vs doors
left as-is and why; runtime-internal call sites converted (count, by binding spelling);
`// B.3.6 bridge` casts added; anything in B36_RULES / B36_SLOTS / B36_FILEMAP you found WRONG
(with the retail evidence); files touched outside your file set; static gate and runtime check
results verbatim; what you did NOT do and why. Do not skip a prerequisite "for scope": port it,
or say exactly what blocks it.
