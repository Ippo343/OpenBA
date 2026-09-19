# B.3.4b System closing — guards and door caches removed together (coordinator, 2026-09-19)

The four System caller batches (a221aea2 Action+Work, 05ea601f rest+standalone+dx11, 22058a57 Game, 4b7586d6
Component, + 5b332342) converted 381 call sites from System door POINTERS to MEMBER calls and deleted NOTHING.
You remove, in YOUR files only and TOGETHER: every System door cache that now has ZERO calls (static pointer or
handle-struct field + typedef + resolver statement + `*_resolved` latch / scratch local if nothing else reads it
+ its `_DEBUG` print row) and every readiness guard on such a cache or on a zero-call System GLOBAL.

READ FIRST, in full: CLAUDE.md; docs/runtime_port/endgame_b/b34_session/GUARD_SWEEP_SPEC.md — it was written for
the Serializer doors but its §2 (every guard SHAPE with its rule: plain `if (p) call;`, negated chains,
early-return, `p && other` keeps `other`, fallback ternaries lose the fallback arm and are LISTED with the
constant, else-arms that only compensate for an unresolved door go WITH the door and are LISTED, readiness
chains, the dangling-`&&` continuation trap, `_DEBUG` name->pointer tables lose rows not the table), §3 (why a
guard on a pointer the converted call no longer reads is deletable), §5 (scripts and their BLIND SPOTS) and §6
(verification) apply to you unchanged; docs/runtime_port/endgame_b/b34_session/SYSTEM_CALLER_BRIEF.md (the eight
binding spellings, and what deliberately did NOT convert); docs/runtime_port/endgame_b/B34B_RULES.md §1.
Commit-message models: `git log -1 --format=%B 8d5fd850` (guards-a2) and `12f6e15e` (2d).

THE DOORS. Converted-and-therefore-possibly-dead (verify ZERO CALLS by BINDING, tree-wide for a global, per
file for a cache): Random Rand(uint) 0x1401512e8, Rand(int,int), Rand(), RandFloat 0x1401512f8, RandFloatTol
0x140151400, RandTol 0x1401512f0, Random::Serialize; Time::AsSeconds 0x140151318, Time::Serialize 0x140152aa8;
Color::Serialize 0x140151ee0; CriticalSection Lock/Unlock; Thread Start/WaitForFinish; Error SetText/Display;
WideString members; and the globals fn_Random_Rand, fn_Random_RandFloat, fn_Random_RandTol, fn_Time_AsSeconds,
g_fn_Time_Serialize, fn_System_Color_Serialize (re-derive the list; a global is touched ONLY by the last pass).
ALIVE — NEVER TOUCH, their calls were left on purpose: every CONSTRUCTOR / DESTRUCTOR door (fn_Time_Ctor_Float and
its ~8 file-local aliases on slot 0x140152b00, fn_Random_Default_Ctor, Thread/CriticalSection ctor/dtor doors,
WideString ctor/dtor where a site was not converted); every by-value String return door (DateTime::AsString,
Configuration CurrentVersion / CurrentBuildId / GetVersionString, dx11 WideStringAsUTF8); `fn_Random_RandFloatTol`
— an `extern "C"` SHIM FUNCTION (code/Game/Game/worldgen_f390_shims.cpp), not a pointer: its call sites and its
body's `p ?` test stay; every allocator door (fn_MemAlloc, MemFree, s_fn_MemAllocSA, g_pMemAllocSingleton …) and a
guard conjunct on one; `g_Timer_*` data reads (class statics, rule B4b-4); all ExternalPtr exe doors; any
RegisterDispatchSlot / address_registry / reg_* registration row, RegisterRuntime* line, `.inc` file.
A cache that shares a resolver block or a scratch `iat` local with LIVE pointers loses only its own lines —
resolve POSITIONALLY. A static's name or comment proves nothing; the binding decides. A handle-struct FIELD goes
only if the field has zero reads left. If a guard tests a live door AND a dead one, only the dead conjunct goes.

VERIFY (spec §6): /c/msys64/ucrt64/bin/mingw32-make.exe standalone-debug, standalone-perf, unit exit 0; warning A/B
per changed file with BOTH flag sets using a COPY of docs/runtime_port/endgame_b/b34_session/warn_ab2.sh in your own
scratch dir (it writes `__abbase_*.cpp` beside the source: never during a build, none left behind — `git status`);
`python tools/dispatch_slot_map.py --check` 0 / 109; cutover manifest gate OK; rt_mint_gen --check UP TO DATE
(`git checkout -- standalone/rt_export_addrs.inc` if it rewrites it); removed-line scan with counts per class and
every per-site decision LISTED; zero-call re-measure on your files after the change. Do NOT run the game or take
the game-dir lock: the coordinator gates the merged tree. `git merge main` first AND before the final build. Never
`git stash`, amend, rebase, push; no sub-agents; never a null check to fix a crash; no getenv; no unscoped searches.
ONE commit `ENDGAME B.3.4b System closing (<your-name>): guards and door caches removed together`, ending with
Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>
Report the hash to `main`, plus — for the last pass — the per-global count of NON-engine_funcs occurrences left in
your files. A shorter honest commit beats a risky one: anything not provably dead stays, listed.
