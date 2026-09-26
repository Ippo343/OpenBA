# B.3.7 caller batch brief (coordinator, 2026-09-26)

Every class in `Engine`, `Save`, `Achievement`, `Audio`, `Input`, `Pathfinding`, `Picking` and
`Application` that step 7 converted is a real class on main (headers under `runtime/types/<ns>/`;
hierarchy and rules in `docs/runtime_port/endgame_b/B37_RULES.md`, raw slot/member lists in
`B37_SLOTS.md`). Their DOORS (the registered `Rt<Ns>_*` functions) still exist and delegate. You
convert CALLERS in YOUR directories from calling through a resolved door pointer / mangled name /
retail address / exe import slot / hand-inlined vtable slot to calling the MEMBER (or the virtual)
directly. You delete NOTHING except a file-local cache (typedef + static + resolver + its readiness
guard) whose LAST caller in that file you converted; globals in `engine/funcs/engine_funcs.{h,cpp}`
stay (the closing pass removes them together, so the build proves the invariant).

READ FIRST, in full: `CLAUDE.md`; `B37_RULES.md` (all of it — the exe subclasses and the
QUALIFIED-CALL rule, the retslot virtual `Goal::GetInstanceType`, hidden-pointer arguments, the
"deliberately NOT converted" section); `b36_session/CALLER_BATCH_BRIEF.md` (everything there
applies with `UI` replaced by your namespaces — the binding spellings, the call shape, the
verification; the bridge marker here is `// B.3.7 bridge`); `b37_session/EXE_SUBCLASSES.md` (the
five loop tables, the `Goal` family, `PickingComponent`); `b37_session/CALLER_SURFACE.md` (where
the sites are, by binding and directory — your starting list, re-measured on the tree you have);
the class headers for every class you touch.

SETUP: `git checkout -b <your-name> && git merge main` first, and `git merge main` again before
the final build. Never `git stash`, amend, rebase, push. No file-editing sub-agents. Helper
scripts only under the session scratchpad dir your prompt names; COPY shared scripts
(`b34_session/sweep.py`, `warn_ab2.sh` …) there first, never run in place.

WHAT CONVERTS — only a door whose member is DEFINED (not merely declared `// not ported`) on the
class today. Check the header AND that a definition exists (grep `Ns::Class::Member(` under
`runtime/src`). A declared-undefined member link-errors: that is your proof, do not work around it.
- NEVER a CONSTRUCTOR or DESTRUCTOR door of a POLYMORPHIC class (the `??0`/`??1`/`??_G` names, the
  loop ctors the exe tables construct their bases through, `Goal`/`GoalInstance`/
  `AchievementInstance`/`SoundBuffer`/`SoundEffect`/`Stream`/`PointList`/`PickingMesh` ctors).
  EXCEPTION: the value types B37_RULES names (real ctors) convert to C++ construction in the same
  storage, exactly once, IF the value-type block has landed (check the header).
- NEVER `MemoryAllocator` doors, `System::String`/`Filename`, singleton STORAGE reads
  (`?_singleton@…`, `?_loader@LoaderLoop@Engine@@` stay `RT_DATA_SLOT` / manifest rows until the
  end of B.3). A door that RETURNS a singleton pointer (`Engine::GetSingleton`-shaped members)
  converts like any member.
- THE EXE-PHASE SUBCLASSES (`EXE_SUBCLASSES.md`): a converted call INSIDE a slot body of
  `GameUpdateLoop`, `MenuDialogLoopT`, the splash loop, the two `LoaderLoop` tables, or a `Goal`
  family class to a member that is VIRTUAL on the base MUST be QUALIFIED
  (`reinterpret_cast<Engine::UpdateLoop*>(self)->Engine::UpdateLoop::Update(...)`). An unqualified
  call dispatches through the hand-built table back into the same body. Do NOT convert the tables.
- By-value returns (`System::String`, `Filename`, `ExternalPtr<T>`, `InternalPtr<T>`, `Time`,
  `Object::Type`, `SaveInfo`…): the door has an explicit `(this, ret, args)` signature. Convert
  only in the "door body moved to the call site" shape — `::new (&slot) T(obj->Member(args))`
  into the SAME caller storage, exactly one construction — and only if the member returns the
  prvalue in C++; otherwise LEAVE and list. The VIRTUAL retslot slot (`Goal::GetInstanceType`,
  slot 9) is declared `Object::Type* GetInstanceType(Object::Type* ret) const`: call it exactly so.
- Hand-inlined `(*(void***)p)[N](p, ...)` on a receiver you can PROVE is one of these classes (from
  the code that produced the pointer, never from the variable's name) becomes a virtual call on
  the proved static type. Unproved receiver: leave, list.
- `Picking` is called through EXE IMPORT SLOTS (`ResolveAddr(Module::Exe, 0x14015…)` + deref,
  mapped through `python tools/dispatch_slot_map.py`) and through `RtAddr<>(0x18…)` hex literals
  in `code/Game/Component` and `code/Game/Player`: your sweep scans HEX LITERALS against
  `standalone/rt_export_addrs.inc` and the PE import table from the start (B36_RULES 14.1). A
  name-anchored sweep finds nothing there.

HOW TO FIND THE SITES — by BINDING, never by name. ELEVEN spellings (eight in
`SYSTEM_CALLER_BRIEF.md`, the ninth in B35_RULES 7.1, the tenth the exe import slot, the eleventh
the file-local hex-literal helper). Establish each cache's mangled name / address FIRST, map it to
exactly one member (THE DOOR DECIDES THE OVERLOAD — never the argument's apparent type; check
ARITY against the member: Ghidra drops arguments on forwarders), and put the door->member table in
your commit message.

CALL SHAPE: `reinterpret_cast<Save::SaveLoad*>(p)->GetOperation();  // B.3.7 bridge` —
`reinterpret_cast` (never C-style, never `static_cast` from `void*`) so the B.4 metric counts the
bridge; cast-free only where the declared type already IS the class. Guards around a converted
call go ONLY when the guard tested nothing but the door pointer's readiness; a guard that also
tests the object stays. A call inside a `printf`/TRACE argument is NOT converted (the perf build
strips it) — list it. Includes at GLOBAL scope in the FIRST include block, never inside `#if` or
a namespace. Three classes are spelled `Scene` (`Audio::`, `Picking::`, Graphics) — always
qualify. Hand-inlined FIELD reads are not doors: leave byte-identical. `<windows.h>` trap: a TU
that includes `standalone/resolve.h` gets `<wingdi.h>`'s `Rectangle`, which breaks `Volume.h`
(`runtime/types/picking/Scene.h` includes it); if an include fails that way include the
narrowest class header or leave the site and list it.

VERIFY (static only — do NOT run the game, do NOT take the game-dir lock):
`/c/msys64/ucrt64/bin/mingw32-make.exe -j6 standalone-debug`, `standalone-perf`, `unit` all exit
0 in the FOREGROUND (600000 ms timeout; exit code from the command itself, logs under
`C:/Users/USPC/AppData/Local/Temp/claude/<your-name>/`); no new warning in a changed file under
EITHER flag set (`warn_ab2.sh`; never during a build); `python tools/dispatch_slot_map.py --check`
0 mismatches; `python tools/cutover_manifest.py --gate` OK; removed-line scan (every removed line
is a door call re-emitted as a member call, or a dead cache that went with its last caller);
per-door removed CALLs == added member calls; re-run your sweep on the final tree and quote the
remaining door CALLs per class with a reason each.

HARD RULES: never fix a faithful crash with a null check; never `getenv`; no new `BANISHED_*`
flags; never `find /` or an unscoped search (exclude `standalone/build*/`, `tests/unit/build/`);
no `pacman`; never hand-edit `.inc` files; edit ONLY files under your directories (anything else:
REPORT). Commit per directory or per ~10 files, title
`ENDGAME B.3.7 callers (<your-name>): <dirs>`, message via `git commit -F <file>`, ending with
`Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>`. A shorter honest commit beats a risky
one; if your context gets deep, commit what is verified and report the rest.

REPORT: branch + hashes; door->member table with counts; sites converted per binding spelling;
bridges added; caches/guards deleted; every site LEFT with its reason (ctor door, unproved
receiver, by-value shape, printf argument, member not defined — name the member: that list drives
the closing pass); anything in the class headers you found wrong.
