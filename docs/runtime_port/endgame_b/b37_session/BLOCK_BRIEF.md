# B.3.7 class-block brief (every step-7 class block runs under this)

You convert a named CLASS SET of one of `Engine`, `Save`, `Achievement`, `Audio`, `Input`,
`Pathfinding`, `Picking`, `Application` into real C++ classes under retail's names, on your own
worktree branch. The coordinator merges and gates. Your prompt names your class set, your FILE SET
and your branch name; this brief is everything else. It is the B.3.6 brief with the step-7
instantiations; where the two differ, this one wins.

## Read first, in this order
1. `CLAUDE.md` (hard rules: no null-check "fixes", never `getenv`, never an unscoped filesystem
   search, no `pacman`).
2. `docs/runtime_port/ENDGAME.md` B.1 and B.2 (retail layout kept, slots in retail order, slot 0 is
   `DeletingDtor(unsigned)`, NO virtual destructor, no real ctor on a polymorphic class).
3. `docs/runtime_port/endgame_b/B37_RULES.md` — the whole sheet. Rules B7-1.. bind you, and so do
   `B36_RULES.md`'s B6-1..11 with its sections 10 and 14 (hidden-pointer arguments, the abstract-
   alias correction, the atomic-slot shape), `B35_RULES.md`'s B5-4, B5-11, B5-12 (as amended),
   B5-15, B5-16, B5-17 and `B34B_RULES.md`'s B4b-1..5. B37_RULES' NOT VERIFIED section: if your
   class is on it, closing that gap from the RETAIL DLL is part of your block.
4. `docs/runtime_port/endgame_b/B37_SLOTS.md` — raw slot lists, sizes, literals, retslot exports,
   by-value-argument prologues, member export lists, ctor stamp chains. COPY from it.
5. `docs/runtime_port/endgame_b/B37_FILEMAP.md` — which file registers which class's members and
   the cross-cutting files that are the coordinator's, not yours.
6. `docs/runtime_port/endgame_b/b37_session/EXE_SUBCLASSES.md` — the exe-phase subclasses of your
   bases (the five loop tables, the `Goal` family, `PickingComponent`) and the QUALIFIED-call sites.
7. `docs/runtime_port/endgame_b/b36_session/BLOCK_BRIEF.md` "The shape" and `B34_BRIEFS.md`
   section A. Finished examples to imitate: `runtime/types/ui/Element.h` (abstract base with exe
   subclasses, retslot virtual, hidden-pointer arguments), `runtime/types/ui/UserInterface.h`
   (concrete non-polymorphic root held by value), `runtime/types/ui/Sprite.h` (value type with
   real ctors), `runtime/types/object/External.h` / `Internal.h` (the real Object bases),
   `runtime/types/system/Thread.h` (what `Engine::Loader` and `Audio::Stream` derive from).

## The shape (unchanged from B.3.6, with step-7 specifics)
- The class is declared in `runtime/types/<ns>/<Header>.h` under the RETAIL namespace and name,
  deriving from its PROVED parent (B37_RULES section 2). Same fields, same order, explicit padding
  kept; keep every `static_assert(offsetof)`, add `sizeof` AND `alignof`. The old `Rt<Ns>::` POD
  keeps living until its last user converts; where class and POD are field-identical prefer ONE
  definition and an alias — EXCEPT that an abstract class cannot be the alias target of an
  EMBEDDED POD (B36_RULES section 10 correction): the POD then stays `Rt<Ns>::` and is
  cross-asserted field for field. Headers that today have no struct at all (`Pathfinding`,
  `Audio::SoundBuffer/SoundEffect/Stream`) get a new header in `runtime/types/<ns>/`; the
  ported `.cpp`'s file-local offsets MOVE under the retail name (B6-8), asserting the retail size.
- Virtuals in retail slot order, one line per slot with the retail address in the comment.
  `virtual`, not `override`, where the branch introduces the slot. Distinct names for overloaded
  virtuals (B5-17). Abstract unless something holds the class by value (then concrete + protected
  `= default` ctor + `friend` the holder + banner, B5-4).
- **Loop bases (`UpdateLoop`, `LoaderLoop`, `DialogLoopT`, `DialogLoaderLoopT`) are exactly the
  `DialogInstance` shape:** the exe builds hand-made tables on them (`GameUpdateLoop`,
  `MenuDialogLoopT`, the splash loop, two `LoaderLoop` subclasses), so they stay abstract-by-policy
  with slot 0 `DeletingDtor(unsigned) = 0` and their ctor doors; a call INSIDE one of those slot
  bodies to a member that is VIRTUAL on the base is QUALIFIED (`->Engine::UpdateLoop::Update()`);
  the tables themselves are step-8 work and are NOT converted. `System::Thread`-derived classes
  (`Loader`, `Stream`) follow whatever shape `Thread.h` has today — read it.
- **`Goal::GetInstanceType` (slot 9) returns `Object::Type` BY VALUE through a VIRTUAL slot**:
  declare it `Object::Type* GetInstanceType(Object::Type* ret) const` in RETAIL order (GCC ms_abi
  puts the hidden return pointer BEFORE `this`; B6-2) on `Goal` and every class that overrides it;
  pin it with a unit test modelled on `tests/unit/graphics_geometry_abi_test.cpp` PART C. Every
  other by-value return in these namespaces is non-virtual (B37_SLOTS): the door keeps the explicit
  `(this, ret, …)` signature and placement-news the member's prvalue (B5-12).
- By-value ARGUMENTS: read the prologue per member (B6-11; B37_SLOTS lists them): an
  `InternalPtr<T>`, `System::String`, `Filename`, `Time` or any >=16-byte class by value arrives by
  HIDDEN POINTER (`T* p` in retail order); an `ExternalPtr<T>` by value is a bare register pointer.
- Every exported member in B37_SLOTS' list for your class is DECLARED in the header up front,
  ported or not (an unported one is declared and left undefined with a `// not ported` note only
  if nothing calls it; otherwise port it). Other blocks and the caller batches must never need to
  edit your header.
- Each ported body MOVES into the member (`self->x` becomes `x`). The registered door keeps its
  registration (name and address) and becomes a one-line delegation with a QUALIFIED non-virtual
  call: `reinterpret_cast<Ns::C*>(self)->Ns::C::Method(args)`. A retail null-`this` test stays in
  the door. Registrations are NOT removed; mint rows / `RT_DELDTOR` rows / `_typeInfo` rows / ctor
  and dtor doors of polymorphic classes are NOT retired; singleton storage (`_singleton`,
  `LoaderLoop::_loader`) stays a `RT_DATA_SLOT` / manifest row (carry-over 5: the class-static
  conversion is the END of B.3).
- Runtime-internal callers INSIDE YOUR FILE SET that reach your classes through `RT_EXPORT_ONCE` /
  `ResolveExport` / `ResolveAddr` / `RtAddr<>` hex literals / hand-inlined `(*(void***)p)[N]`
  convert to direct member / virtual calls in the same block. When the receiver is still a `void*`
  or a POD, bridge it: `reinterpret_cast<Ns::C*>(p)->M()` with the trailing comment
  `// B.3.7 bridge` (must be `reinterpret_cast`, so the B.4 metric counts it). Find callers by
  BINDING (ELEVEN spellings: `b36_session/CALLER_BATCH_BRIEF.md` + B36_RULES 14.1 — scan every
  `0x18…` hex literal in your files against `standalone/rt_export_addrs.inc`), never by name. THE
  DOOR DECIDES THE OVERLOAD.
- Callers OUTSIDE your file set stay untouched — doors keep them working. The caller batches come
  after the class blocks.
- Member bodies go at GLOBAL scope or inside `namespace Ns {` AFTER the `Rt*` namespace block
  closes; inside `namespace Ns` always spell a POD with its `Rt<Ns>::` qualifier. THREE classes
  are spelled `Scene` (`Audio::Scene`, `Picking::Scene`, the Graphics one) — always qualify.
  Sweep forward declarations (`struct X;` for a class compiles).
- No side-effecting call inside a `printf`/`TRACE` argument (B5-11: the perf build strips it).
- THREADS (B37_RULES section 5): the loader thread constructs Save/Achievement/UI objects while the
  main thread draws; `Audio::Stream` owns a `_beginthreadex` worker and XAudio2 voice callbacks
  run on a third thread. No static initialiser, lazy singleton or non-atomic first-call cache in
  any member you write (B6-5): a namespace-scope atomic slot with the `RtResolveCached` shape
  (`element_common.h`'s `AtomicExport` after the B36_RULES 14.4 fix) is the pattern.
- `runtime/src/engine/singleton_ctors.cpp` stamps `Audio::{Configuration,Device}`,
  `Input::{Configuration,Device}`, `Video::{Configuration,Device}`, the three Object factories,
  three IO classes and `MemoryAllocator`: it is OWNER-RETAINED by the Engine block; Audio and
  Input blocks list the lines they need changed under "outside file set" and the coordinator
  sequences it.
- Add unit coverage in `tests/unit/` for the ABI claims that matter (slot order via a probe
  subclass, sizes, the retslot virtual, hidden-pointer arguments). `UNIT_SRCS` is a wildcard: do
  NOT edit `tests/unit/sources.mk`.
- Retail slips are transcribed, not fixed; a PORT bug found on the way is fixed faithfully and
  recorded in your report.

## Process rules
- Work ONLY in your worktree, on your branch. First command: `git merge main` (fast-forward) and
  confirm `git log -1` shows the coordinator's launch commit named in your prompt.
- Touch ONLY your file set. If you need a change outside it, do the minimum, list it in your
  report under "outside file set", and never reformat. Never drop existing code when adding to a
  shared file.
- FORBIDDEN: `git stash`, `git commit --amend`, rebase, push, force anything, spawning
  sub-agents that edit files, `pacman`, unscoped filesystem searches, `getenv`, hand-editing
  `.inc` files.
- Commit in small commits (one class or one chain per commit), message via `git commit -F file`
  (no heredoc; avoid the section sign and arrows), title `ENDGAME B.3.7 <block>: <what>`. End each
  message with `Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>`.
- STATIC GATE before each commit, in the FOREGROUND with a 600000 ms timeout, exit code read from
  the command itself (`make ... > log 2>&1; echo $?`), logs under
  `C:/Users/USPC/AppData/Local/Temp/claude/<your-branch>/`:
  `mingw32-make -j12 standalone-debug`, `standalone-perf`, `unit` (all exit 0, unit 0 failures),
  `python tools/dispatch_slot_map.py --check` (0), `python tools/cutover_manifest.py --gate`,
  `python tools/rt_mint_gen.py --check` (then `git checkout -- standalone/rt_export_addrs.inc`
  if it only changed line endings). No NEW warnings in files you touched (debug AND perf flags).
  Make is `/c/msys64/ucrt64/bin/mingw32-make.exe`. The compiler is behind `ccache`: a rebuild of
  an unchanged file replays its cached warnings, so a warning diff is honest.
- RUNTIME CHECK, once per chain and once at the end (not per commit): one new-game route under
  the game-dir lock, in the foreground:
  `tools/game_dir_lock.sh acquire <branch> 900` … copy YOUR worktree's
  `standalone/build/reimpl-x64.exe` to `C:/GOG Games/Banished/`, run it from there with
  `BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120`, poll with
  `tools/boot_state.sh --wait 300` (never a grep-until-timeout loop; a quiet log is not
  "loading") … `tools/game_dir_lock.sh release <branch>` ALWAYS, also after a failure. Expect
  exit 0, empty `standalone_crash.log`, no `standalone_hang.log`, `[RtResolve] 0 misses`, no
  `[RtMint] TRAP`, `calls=5686`. `[AutoStart]` lines are in `standalone_error.log`. If the exe
  dies read `standalone_crash.log` FIRST. A crash on an unguarded deref is never fixed with a null
  check: find what handed you the null. If the lock is not granted within 15 minutes, skip the
  runtime check, say so in the report, and carry on — the coordinator gates the merged tree.
  Blocks on a serialize/load path (`Save`) also run `tools/resave_gate/resave_gate.sh Gate` once.
- Before your final build: `git merge main` again (keep both sides in append-only shared files;
  check no conflict marker split a closing brace), rebuild, re-run the static gate.
- Ghidra (if you need it): the bridge is shared and `switch_program` is GLOBAL. Before every
  read batch assert `get_current_program_info` = `Runtime-x64.dll`, 15290 functions (retail). The
  profile DLL (16002) is for names only, never offsets. Switch BACK. Ghidra's decompiled C drops
  args on indirect calls and forwarders: read the disassembly. Scripts must be Java.

## Report (short)
Branch + commit list; classes converted with slot count / sizeof / parent; bodies moved vs doors
left as-is and why; runtime-internal call sites converted (count, by binding spelling);
`// B.3.7 bridge` casts added; anything in B37_RULES / B37_SLOTS / B37_FILEMAP you found WRONG
(with the retail evidence); files touched outside your file set; static gate and runtime check
results verbatim; what you did NOT do and why. Do not skip a prerequisite "for scope": port it,
or say exactly what blocks it.
