# B.3.6 UI caller batch brief (coordinator, 2026-09-25)

Every `UI::*` class is a real class on main (headers under `runtime/types/ui/`; hierarchy and
rules in `docs/runtime_port/endgame_b/B36_RULES.md`, raw slot/member lists in `B36_SLOTS.md`).
Their DOORS (the registered `RtUi_*` / `RtUI_*` / `Ui_*` functions) still exist and delegate. You
convert CALLERS in YOUR directories from calling through a resolved door pointer / mangled name /
retail address / hand-inlined vtable slot to calling the MEMBER (or the virtual) directly. You
delete NOTHING except a file-local cache (typedef + static + resolver + its readiness guard) whose
LAST caller in that file you converted; globals in `engine/funcs/engine_funcs.{h,cpp}` stay (a
closing pass removes them together, so the build proves the invariant).

READ FIRST, in full: `CLAUDE.md` (hard rules); `B36_RULES.md` (all of it, especially B6-2 the two
retslot slots, B6-3 the exe subclasses and the QUALIFIED-CALL rule, B6-6 callers by binding, and
section 7 "deliberately NOT converted"); `B35_RULES.md` B5-12 with its COORDINATOR AMENDMENT;
`b36_session/BLOCK_BRIEF.md` (the shape the classes took); `b34_session/SYSTEM_CALLER_BRIEF.md`
(binding spellings, call shape, verification — everything there applies with `System` replaced by
`UI`); the class headers for every class you touch.

SETUP: worktree base may be stale: `git checkout -b <your-name> && git merge main` first, and
`git merge main` again before the final build. Never `git stash`, amend, rebase, push. No
file-editing sub-agents. Helper scripts only under the session scratchpad dir your prompt names;
COPY shared scripts (`b34_session/sweep.py`, `warn_ab2.sh` …) there first, never run in place.

WHAT CONVERTS — only a door whose member is DEFINED (not merely declared `// not ported`) on the
class today. Check the header AND that a definition exists (grep `Ns::Class::Member(` under
`runtime/src`). A declared-undefined member link-errors: that is your proof, do not work around it.
- NEVER a CONSTRUCTOR or DESTRUCTOR door of a POLYMORPHIC class (`fn_DialogInstance_Ctor`,
  `fn_DialogInstance_Dtor`, the `??0`/`??1`/`??_G`/`??2`/`??3` names, `s_fn_UI_Ctor` for
  `UserInterface`): those classes have no real ctor/dtor (B4b-2; B36_RULES 2.4, 3.1, 7). List them.
  EXCEPTION (RULE B6-10): `UI::String` (`fn_UIString_Ctor`, `fn_UIString_Ctor_Index`, ...) and
  `UI::Sprite` (`fn_Sprite_Ctor` ...) are VALUE TYPES with real ctors — a door call that constructs
  one into caller storage converts to a C++ construction of `UI::String` / `UI::Sprite` in the
  same storage, exactly once, IF the value-type block has landed (check the header).
- NEVER `MemoryAllocator` doors, `System::String`/`Filename`, static storage reads.
- The EXE-PHASE SUBCLASSES (`b36_session/EXE_SUBCLASSES.md`): a converted call INSIDE a slot body
  of a hand-built `DialogInstance` / `DialogController` / `Element` subclass to a member that is
  VIRTUAL on the base (`OnOpened`, `OnClosed`, `OnUpdateLayout`, `OnUpdate`, `OnElementEvent`,
  `OnSerializeSettings`, `Persist`, `Element` slots 4..11, `DialogController` 4..10) MUST be
  QUALIFIED: `reinterpret_cast<UI::DialogInstance*>(self)->UI::DialogInstance::OnUpdate(...)`. An
  unqualified call dispatches through the hand-built table back into the same body.
- By-value returns (`InternalPtr<Element>` — `Child` x2, `FindElement`, `FindElementType`,
  `GetRoot` x2, `GetContainer`, `CreateElement(s)`; `InternalPtr<DialogInstance>` — `GetParent`,
  `GetFirstContainedDialog`; `InternalPtr<DialogController>` — `GetController`; the
  `ExternalPtr<T>` getters; `System::String` — `GetText`, `GetLink`, `GetItemText`,
  `GetToolTipString`, `GetSpriteNameFromIndex`; `UI::String` — `GetToolTip`; `UI::Sprite` —
  `GetSprite` x2, `GetBackgroundSprite`; `Video::Viewport` — the four `Scissor` members;
  `ClipRegion` — `SetClipRegion` x2; `GetFirstDialog`'s `Iterator`): the door has an explicit
  `(this, ret, args)` signature. Convert only in the "door body moved to the call site" shape —
  `::new (&slot) T(obj->Member(args))` into the SAME caller storage, exactly one construction —
  and only if the member returns the prvalue in C++; otherwise LEAVE and list. The two VIRTUAL
  retslot slots (`GetBorder` slot 11, `CreateInstance` slot 8) are declared
  `Object::InternalPtr<UI::Element>* M(Object::InternalPtr<UI::Element>* ret, ...)`: call them
  exactly so.
- Hand-inlined `(*(void***)p)[N](p, ...)` on a receiver you can PROVE is a UI object (from the code
  that produced the pointer — `fn_Element_Child`, `FindElement`, a `_root` read, a `GetController`
  — never from the variable's name) becomes a virtual call on the proved static type:
  `reinterpret_cast<UI::Element*>(p)->Update(...)`. REMEMBER B6-1: `DialogInstance` and
  `DialogController` number `OnOpened...` one slot apart; `Element` slot N is not `Description`
  slot N. Unproved receiver: leave, list. `tradeui.cpp` (150) and `townhallui.cpp` (95) are the
  two hot files: they are their own batch.
- `code/Game` implements ~60 subclasses of `DialogInstance` / `DialogController` /
  `DialogControllerConfig` / `Element` / `ElementDescription` with hand-built vtables
  (`EXE_SUBCLASSES.md`): these are exe-phase classes — do NOT convert the subclass; calls they
  make INTO base-class members convert normally (QUALIFIED when the member is virtual).

HOW TO FIND THE SITES — by BINDING, never by name. TEN spellings (eight in
`SYSTEM_CALLER_BRIEF.md`, the ninth in B35_RULES 7.1, the tenth in NEXT_SESSION 2d: an exe import
slot `ResolveAddr(Module::Exe, 0x14015...)` + deref, mapped through `tools/dispatch_slot_map.py`).
UI callers are DOMINATED by spelling (a): 118 `engine_funcs.h` globals whose names are NOT
reliably `fn_UI_*` (`fn_Element_SetFlagsOnOff`, `fn_DialogInstance_FindElement`,
`fn_TextElement_SetParameter`, `fn_UIString_Ctor`, ...) with 1,434 call sites in `code/Game`. Find
each global by its INITIALISER string in `engine/funcs/engine_funcs.cpp`, never by its name. Establish
each cache's mangled name / address FIRST, map it to exactly one member (THE DOOR DECIDES THE
OVERLOAD — never the argument's apparent type; check ARITY against the member: Ghidra drops
arguments on forwarders), and put the door->member table in your commit message.

CALL SHAPE: `reinterpret_cast<UI::Element*>(p)->SetFlagsOnOff(on, off);  // B.3.6 bridge`
— `reinterpret_cast` (never C-style, never `static_cast` from `void*`) so the B.4 metric counts the
bridge; cast-free only where the declared type already IS the class. Spell the door's parameter
types at the call. Guards around a converted call go ONLY when the guard tested nothing but the
door pointer's readiness; a guard that also tests the object stays. A call inside a `printf`/TRACE
argument is NOT converted (the perf build strips it) — list it. Includes at GLOBAL scope in the
FIRST include block, never inside `#if` or a namespace. Inside `namespace UI` (rare outside runtime/) a bare POD name binds the CLASS — spell PODs
`RtUI::`.
Hand-inlined FIELD reads (`*(float*)(cam + 0x40)`) are not doors: leave byte-identical.
`<windows.h>` trap: a TU that includes `standalone/resolve.h` gets `<wingdi.h>`'s `Rectangle`
function, which breaks `Volume.h`; if an include fails that way include the narrowest class
header or leave the site and list it.

VERIFY (static only — do NOT run the game, do NOT take the game-dir lock; the coordinator gates
the merged tree): `/c/msys64/ucrt64/bin/mingw32-make.exe -j6 standalone-debug`, `standalone-perf`,
`unit` all exit 0 in the FOREGROUND (600000 ms timeout; exit code from the command itself, logs
under `C:/Users/USPC/AppData/Local/Temp/claude/<your-name>/`); no new warning in a changed file
under EITHER flag set (`warn_ab2.sh`; never during a build); `python tools/dispatch_slot_map.py
--check` 0 mismatches; `python tools/cutover_manifest.py --gate` OK; removed-line scan (every
removed line is a door call re-emitted as a member call, or a dead cache that went with its last
caller); per-door removed CALLs == added member calls; re-run your sweep on the final tree and
quote the remaining door CALLs per class with a reason each.

HARD RULES: never fix a faithful crash with a null check; never `getenv`; no new `BANISHED_*`
flags; never `find /` or an unscoped search (exclude `standalone/build*/`, `tests/unit/build/`);
no `pacman`; never hand-edit `.inc` files; edit ONLY files under your directories (anything else:
REPORT). Commit per directory or per ~10 files, title
`ENDGAME B.3.6 UI callers (<your-name>): <dirs>`, message via `git commit -F <file>`,
ending with `Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>`. A shorter honest commit
beats a risky one; if your context gets deep, commit what is verified and report the rest.

REPORT: branch + hashes; door->member table with counts; sites converted per binding spelling;
bridges added; caches/guards deleted; every site LEFT with its reason (ctor door, unproved
receiver, by-value shape, printf argument, member not defined — name the member: that list drives
the closing pass); anything in the class headers you found wrong.
