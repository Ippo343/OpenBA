# B.3.5 Video/Graphics caller batch brief (coordinator, 2026-09-19)

Every `Video::*` and `Graphics::*` class is a real class on main (headers under
`runtime/types/video/` and `runtime/types/graphics/`; hierarchy and rules in
`docs/runtime_port/endgame_b/B35_RULES.md`, raw slot/member lists in `B35_SLOTS.md`). Their DOORS
(the registered `RtVideo_*` / `RtGraphics_*` / `RtGfx*` functions) still exist and delegate. You
convert CALLERS in YOUR directories from calling through a resolved door pointer / mangled name /
retail address / hand-inlined vtable slot to calling the MEMBER (or the virtual) directly. You
delete NOTHING except a file-local cache (typedef + static + resolver + its readiness guard) whose
LAST caller in that file you converted; globals in `engine/funcs/engine_funcs.{h,cpp}` stay (a
closing pass removes them together, so the build proves the invariant).

READ FIRST, in full: `CLAUDE.md` (hard rules); `B35_RULES.md` (all of it, including the COORDINATOR
AMENDMENT under B5-12 and section 11 "deliberately NOT converted"); `b35_session/BLOCK_BRIEF.md`
(the shape the classes took); `b34_session/SYSTEM_CALLER_BRIEF.md` (the previous step's caller
brief: binding spellings, call shape, verification — everything there applies with `System`
replaced by `Video`/`Graphics`); the class headers for every class you touch.

SETUP: worktree base may be stale: `git checkout -b <your-name> && git merge main` first, and
`git merge main` again before the final build. Never `git stash`, amend, rebase, push. No
file-editing sub-agents. Helper scripts only under the session scratchpad dir your prompt names;
COPY shared scripts (`b34_session/sweep.py`, `warn_ab2.sh` …) there first, never run in place.

WHAT CONVERTS — only a door whose member is DEFINED (not merely declared `// not ported`) on the
class today. Check the header AND that a definition exists (grep `Ns::Class::Member(` under
`runtime/src`). A declared-undefined member link-errors: that is your proof, do not work around it.
- NEVER a CONSTRUCTOR door (`fn_Graphics_Camera_Ctor`, `_Renderer_Ctor`, `_Scene_Ctor`,
  `_SunLight_Ctor`, `_MeshInstance_Ctor`, `_Geometry_Ctor`, `_Particle_Ctor`,
  `fn_Video_VertexBuffer_Ctor`, `fn_Video_IndexBuffer_Ctor`, any `??0`/`??1`/`??_G`/`??2`/`??3`
  name): no Video/Graphics class has a real ctor/dtor (B4b-2; sections 2.4, 11). List them.
- NEVER `MemoryAllocator` doors, `System::String`/`Filename`, static storage reads
  (`?_singleton@Device@Video@@` etc. stay `RT_DATA_SLOT` / by-name reads — section 9).
- By-value returns (`Math::Matrix`, `Transform`, `Volume::Box`, `Frustum`, `Camera`, `Viewport`,
  `DisplayMode`, `String`, `Time`, and the `ExternalPtr<T>` getters — block B read the prologues:
  they ARE retslot in retail): the door has an explicit `(this, ret, args)` signature. Convert
  only in the "door body moved to the call site" shape — `::new (&slot) T(obj->Member(args))`
  into the SAME caller storage, exactly one construction — and only if the member returns the
  prvalue in C++; otherwise LEAVE and list. VIRTUAL slots 5/6 of the Geometry family are declared
  `T* M(T* ret, …)`: call them exactly so.
- Hand-inlined `(*(void***)p)[N](p, …)` on a receiver you can PROVE is a Video/Graphics object
  (from the code that produced the pointer — never from the variable's name) becomes a virtual
  call on the proved static type: `reinterpret_cast<Graphics::Geometry*>(p)->Update(dt)`. A slot
  call on an `Object::External` / `Object::Base` slot (0..7) of a Video/Graphics object converts
  the same way through the real base. Unproved receiver: leave, list.
- `code/Game` sometimes implements its OWN subclass of a Runtime class with a hand-built vtable
  (tornado's Particle subclass, status icons / frustum outline Geometry subclasses,
  `statusicon_geometry.cpp` names Geometry's vtable 0x180154e60): these are exe-phase classes —
  do NOT convert the subclass; calls they make INTO base-class members convert normally.

HOW TO FIND THE SITES — by BINDING, never by name. Nine spellings (eight in
`SYSTEM_CALLER_BRIEF.md`, the ninth in B35_RULES 7.1: a mangled name in an initialised table,
resolved by a loop). Video/Graphics callers are dominated by spelling 3
(`ResolveExport("…@Video@@…"/"…@Graphics@@…")`, literals sometimes WRAPPED over two lines) into
file-local `s_fn_*` statics, plus spelling 1/`ResolveAddr(Module::Runtime, 0x18…)` and inlined
slots; only 61 sites go through `engine_funcs.h` `fn_Video_*`/`fn_Graphics_*` globals. Establish
each cache's mangled name / address FIRST, map it to exactly one member (THE DOOR DECIDES THE
OVERLOAD — never the argument's apparent type; check ARITY against the member: Ghidra drops
arguments on forwarders), and put the door->member table in your commit message.

CALL SHAPE: `reinterpret_cast<Graphics::Scene*>(p)->AddObject(reinterpret_cast<Graphics::Geometry*>(g));  // B.3.5 bridge`
— `reinterpret_cast` (never C-style, never `static_cast` from `void*`) so the B.4 metric counts the
bridge; cast-free only where the declared type already IS the class. Spell the door's parameter
types at the call. Guards around a converted call go ONLY when the guard tested nothing but the
door pointer's readiness; a guard that also tests the object stays. A call inside a `printf`/TRACE
argument is NOT converted (the perf build strips it) — list it. Includes at GLOBAL scope in the
FIRST include block, never inside `#if` or a namespace. Inside `namespace Video`/`Graphics` (rare
outside runtime/) a bare POD name binds the CLASS — spell PODs `RtVideo::`/`RtGraphics::`.
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
`ENDGAME B.3.5 Video/Graphics callers (<your-name>): <dirs>`, message via `git commit -F <file>`,
ending with `Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>`. A shorter honest commit
beats a risky one; if your context gets deep, commit what is verified and report the rest.

REPORT: branch + hashes; door->member table with counts; sites converted per binding spelling;
bridges added; caches/guards deleted; every site LEFT with its reason (ctor door, unproved
receiver, by-value shape, printf argument, member not defined — name the member: that list drives
the closing pass); anything in the class headers you found wrong.
