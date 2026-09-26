# B.3.7 closing pass brief (one owner, after the last caller batch merges)

You are the closing owner of ENDGAME B.3 step 7. Every class in `Engine`, `Save`, `Achievement`,
`Audio`, `Input`, `Pathfinding`, `Picking` and `Application` is a real C++ class on main; the class
blocks (B1-B6), the fragment (F) and the caller batches (K1-K5) have merged, each behind the
coordinator's gate. You do what `b36_session`'s closing pass did (`B36_RULES.md` section 11, the
"closing pass" paragraph, and section 14), for step 7. Model reports: `b36_session/CLOSE_NUMBERS.md`
(the numbers template — same units, same scripts shape), `B36_RULES.md` sections 10-14.

READ FIRST: `CLAUDE.md`; `B37_RULES.md` (all); `b37_session/CALLER_BATCH_BRIEF.md` (its rules on
what converts and what never does bind you too); `b37_session/CALLER_SURFACE.md` sections 1-2 and 6;
`B36_RULES.md` sections 11, 12, 14; `b36_session/CLOSE_NUMBERS.md`; the block and batch reports the
coordinator pastes into your prompt (the "LEFT" lists drive commits 1-2).

COMMITS, in this order (each behind the static gate; `git commit -F <file>`; title
`ENDGAME B.3.7 closing pass (b37-close) N: <what>`; end with
`Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>`):
1. **The `engine_funcs` zero-caller sweep BY BINDING.** Population = every global in
   `engine/funcs/engine_funcs.cpp` whose INITIALISER names one of the eight namespaces
   (`ResolveExport("…@NS@@…")` including back-references `@Achievement@1@`, `@Application@1@`,
   `@Engine@1@`; `ResolveAddr(Module::Exe, slot)` mapped through `python tools/dispatch_slot_map.py`;
   `ResolveAddr(Module::Runtime, 0x18…)` through `standalone/rt_export_addrs.inc`; the two-statement
   `void** d = ResolveAddr(..); if (d) fn_X = *d;` form). For each: count code-only references
   tree-wide INCLUDING `engine_funcs.h`'s inline bodies (B36_RULES 14.2) and EXCLUDING
   `engine_funcs.cpp`'s initialiser statements. Zero references → delete the global, its typedef,
   its extern and its initialiser whole. A readiness guard, `Ready()` conjunct, `DiagPrint` row or
   `(void*)fn_X` printf of a door nobody calls is dead and goes with the global. Keep: ctor/dtor
   doors of polymorphic classes and loop ctor doors that are still called; data globals bound to
   singleton storage (`g_pMainLoopSingleton`, `g_pSaveLoadSingleton`, `g_ppAchievementManager`,
   `g_pAppSingleton`, `g_pKeyMapObjectExref` …). Rename the two MISBOUND globals to their bound
   symbol (`g_pEngineSingleton` is bound to `?_singleton@MainLoop@Engine@@` — it is a second name
   for `g_pMainLoopSingleton`: fold it; `g_pKeyMapObjectExref` is `Input::Configuration::_singleton`),
   like B36's commit 5. `fn_KeyMap_Released` (bound only to the unregistered exe slot 0x1401528f0,
   always null): if K3 converted its two callers, delete it; record the defect either way.
   Finish with a line-by-line re-sweep of the final tree.
2. **Dead caches and B6-5 leftovers outside `engine_funcs`**: file-local door caches whose last
   caller converted (the K2/K3 lists: `edittool.cpp`, `citizenai_executor.cpp`,
   `gameupdateloop_load.cpp`, `gameupdateloop_serialize.cpp`, `game_main.cpp`, `gameupdateloop.cpp`
   and whatever K3/K4/K5 report); function-local lazy `ResolveExport`/`ResolveAddr` caches in any
   member of a class constructed on the loader/audio threads → namespace-scope atomic slots.
3. **Hex-literal sweep** (B7-11 / B36_RULES 14.1): every `0x18[0-9a-f]{7}` literal in
   `code/ engine/ standalone/ runtime/src dx11/ tests/` joined against the eight namespaces' rows
   in `rt_export_addrs.inc`, and every `0x1401[0-9a-f]{4}` literal against the PE import table:
   a LIVE CALL through a file-local helper to a member that is DEFINED converts (bridge cast);
   registration rows, `_typeInfo`/`??_7` reads and ctor/dtor doors stay. Quote what remains and why.
4. **Stale banners and headers**: comments that still say "no type header", "not exported",
   "RtNs POD" for a class that is now real; `singleton_ctors.cpp:742`; `runtime/src/<ns>/README.md`
   status lines; `B37_RULES.md` corrections the blocks reported (Goal slot 8 is `CheckGoalState` not
   `Construct`; `Audio::Device::PlatformChannel` is the named voice-callback record; `SoundEffect`
   +0x20/+0x24/+0x28 names; `SoundBuffer` +0x48/+0x50; `Pathfinding::Scene` is exactly 0x90;
   `Engine::Engine` 0x6f8 pinned; `ModInfo` 0x20; `Draw`'s ExternalPtr in a register; `UpdateList`
   writes +0x2c; retail Scenario table 9 slots and Goal tables 11) go into `B37_RULES.md` section
   10 as dated corrections and section 13 items are marked CLOSED.
5. **`standalone/cutover_manifest.inc` refresh**: `python tools/cutover_manifest.py --inc` (never
   hand-edit) if the gate prints a COSMETIC `stale` warning; `python tools/rt_mint_gen.py --check`.
6. **`b37_session/CLOSE_NUMBERS.md`**: the B.4 cast metric per directory (LINE counts of
   `reinterpret_cast` over `*.cpp *.h *.inc`, build dirs excluded, at the start `951fd29b` measured
   in a throwaway worktree, and now); `// B.3.7 bridge` lines per directory; the caller surface
   (globals by initialiser start vs now, door CALL sites in `code/Game`, `ResolveExport` /
   `RT_EXPORT_ONCE` / `RT_DATA_SLOT` / exe-slot / hex-literal sites per directory start vs now);
   hand-inlined vtable calls; `tools/runtime_census.py` rows for the eight namespaces verbatim
   start vs now; unit tests start vs now; the class list from the headers cross-checked against
   `B37_SLOTS.md` (parent, slot count, sizeof — N of N agree); declared-but-undefined members by
   class (the honest ceiling); doors kept by design (registered names per family).
7. **`B37_RULES.md` sections 11 (the passes and gate table — the coordinator gives you the rows),
   12 (deliberately left, with sizes), 14 (corrections from the close)**; `runtime/src/<ns>/README.md`
   one-paragraph status each.

GATE per commit (FOREGROUND, 600000 ms): `mingw32-make -j12 standalone-debug`, `standalone-perf`,
`unit` (0 failures), `python tools/dispatch_slot_map.py --check` (0), `python tools/cutover_manifest.py
--gate`, `python tools/rt_mint_gen.py --check` (then `git checkout -- standalone/rt_export_addrs.inc`);
`warn_ab2.sh` no new warning in touched files. No game runs (the coordinator runs the step close).
HARD RULES: never `find /`; never `getenv`; no `pacman`; no `git stash`/amend/rebase/push; never
hand-edit `.inc`; a crash is never fixed with a null check; do not delete a door whose caller
exists under ANY of the eleven spellings; do not touch `tests/unit/sources.mk`.

REPORT: commit list; globals deleted / kept (with the reason per kept one); caches deleted; hex
sites converted / left; the numbers file path; every correction you recorded; what you did NOT do.
