# B.3.4b System caller batch brief (coordinator, 2026-09-19)

Twelve `System::` classes are real classes on main (runtime/types/system/{Thread,Value,Timer,Crc,Error,
WideString,Compression,Configuration}.h): CriticalSection, Thread, Random, Time, Color, DateTime, Timer, CRC,
Error, WideString, Compression, Configuration. Their DOORS (the registered `RtSystem_*` functions) still exist
and delegate. You convert CALLERS in your directories from calling through a resolved door POINTER to calling the
MEMBER directly. You delete NOTHING (pointers, typedefs, statics, resolver blocks, readiness guards all stay —
a later closing pass removes guard and pointer together).

READ FIRST, in full: CLAUDE.md (hard rules); docs/runtime_port/endgame_b/B34B_RULES.md (the class table §1.1-§1.5
— WHICH members exist, which deliberately do NOT, the caller surfaces per class, rules B4b-1..5);
docs/runtime_port/endgame_b/B34_RULES.md §6.5 + §7 and both coordinator amendments; the commit messages of
e7498818 (batch A), 920e78a7 (batch G), 6d1c0d28 (batch H) — `git log -1 --format=%B <hash>` — for the worked
call shapes, the removed-line scan, and the traps.

SETUP: your worktree base is STALE: `git checkout -b <your-name> && git merge main` first; `git merge main`
again before the final build. Never `git stash`, amend, rebase, push. No sub-agents. Helper scripts go in
...\scratchpad\<your-name>\ ; COPY any shared script (scratchpad\closing\sweep.py etc., and the FIXED warning A/B
scratchpad\b34-guards-b\warn_ab2.sh) into your own dir — never run them in place.

WHAT CONVERTS — only a door whose MEMBER EXISTS on the class today:
- Random: Rand (3 overloads), RandFloat, RandFloatTol, RandTol, NextGroup, Serialize.  Time: AsSeconds, Serialize.
  Color: Serialize.  DateTime: AsString, DateAsString, TimeAsString.  CriticalSection: Lock, Unlock.
  Thread: Start, WaitForFinish.  Timer / CRC / Error: their static members (`System::Timer::CurrentTime()` …).
  WideString, Configuration, Compression: the members their headers declare.
- DO NOT CONVERT: any CONSTRUCTOR or DESTRUCTOR door of Random, Time, Color, DateTime, Thread, CriticalSection,
  Compression, Configuration (those classes have NO real ctor — B4b-2 / §4.3; a member-form ctor would double-seed
  or swap a vtable). WideString HAS real ctors/dtor, but its holders are raw POD buffers: convert a WideString
  ctor/dtor site only in the placement-new-into-the-SAME-raw-buffer / explicit-dtor-at-the-SAME-statement shape of
  B34_RULES §7.3, and REPORT anything else. MemoryAllocator doors (fn_MemAlloc, MemFree, s_fn_MemAllocSA …) are
  DEFERRED PAST THIS STEP: never touch them. System::String / Filename were B.3.1: not yours.
  By-value String returns are hidden-retslot doors: convert only in the "door body moved to the call site" shape
  (`::new (&buf) T(obj->Member())`, exactly one AddRef, see 53d7b837) or REPORT.

HOW TO FIND THE SITES — by BINDING, never by name. Eight binding spellings have each cost a batch a miss:
 1 `ResolveAddr(Module::Exe, <slot>)` on the cache's line; 2 helpers: RSlot(), ResolveSlotPtr<T>(), a file-local
 ResolveSlot(addr, cache, tried, tag), loadExref(addr) lambdas, ResolveFn<T>("mangled"); 3 `ResolveExport("…@System@@…")`
 by mangled name — sweep with string literals INTACT, and ALSO when the call is WRAPPED over two lines; 4 two-statement
 indirection through a bare uintptr_t scratch local (`iat = ResolveAddr(…); if (iat) s_fn = *(T*)iat;`) — resolve
 POSITIONALLY (nearest preceding assignment wins; reassignment from an unrecognised source KILLS the binding;
 moveto.cpp rebinds one `iat` ten times); 5 plain alias `s_fn_X = fn_Y;` and uintptr_t round-trips; 6 globals bound in
 engine/funcs/engine_funcs.cpp and called from a game dir (NOTE: 8 of 13 System families have NO fn_* door in
 engine_funcs.h at all — Thread, CriticalSection, Timer, Error, WideString, Compression reach you ONLY through
 spellings 1-5); 7 handle-struct fields (`g_h.randFloat`); 8 RT_EXPORT_ONCE(T, "mangled")(args) used inline.
 A static's NAME and COMMENT prove nothing; the binding decides (citizencomponent.cpp:43 called the
 Serialize(float&) slot "RandFloat"). Establish each door's exe slot / mangled name FIRST, put the door->member table
 in your commit message, and check ARITY against the member (Ghidra drops arguments on forwarders; a by-value
 Math/String return has a hidden first argument).

CALL SHAPE: `reinterpret_cast<System::Random*>(p)->RandFloat()   // B.3.4 bridge`. THE DOOR DECIDES THE OVERLOAD,
never the argument's apparent type: arguments are mostly untyped `s + 0xNN`; spell the door's parameter types at the
call so overload resolution cannot pick a sibling (Random::Rand has THREE overloads — map each slot to exactly one).
Cast-free only where the argument's declared type is EXACTLY the door's. Timer/CRC/Error are static calls — no
receiver at all; if the old call passed a dummy `this`, say so. Guards stay; a call inside a printf argument is NOT
converted (the perf build strips printf arguments) — list it. Includes go at GLOBAL scope in the FIRST include block
(some files have a second include block far down beside a registration function — appending there puts the
declaration after the call sites), never inside `#if`. Hand-inlined reads of a System object's fields
(`*(int64_t*)(p)` for Time ticks etc.) are NOT doors: leave byte-identical and list them.

VERIFY: /c/msys64/ucrt64/bin/mingw32-make.exe standalone-debug, standalone-perf, unit all exit 0 (`standalone` and
`standalone-debug` share an obj dir; perf is the real NDEBUG build); warning A/B per changed file with BOTH flag
sets using warn_ab2.sh; `python tools/dispatch_slot_map.py --check` 0 mismatches / 109; removed-line scan (every
removed line is a door call you re-emitted as a member call; nothing else); per-door removed CALLs == added member
calls; re-run your sweep on the final tree and quote remaining door CALLs per class with a reason each. Do NOT run
the game and do NOT take the game-dir lock: the coordinator gates the merged tree.

HARD RULES: never fix a faithful crash with a null check; never `getenv`; no new BANISHED_* flags; never `find /` or
unscoped searches; no pacman; never hand-edit .inc files; edit ONLY files under your directories (anything else:
REPORT). ONE commit titled `ENDGAME B.3.4b System callers (<your-name>): <dirs>`, body in batch G's format, ending with
Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>
Report the hash to `main`. A shorter honest commit beats a risky one; if your context gets deep, commit what is
verified and send handover notes.
