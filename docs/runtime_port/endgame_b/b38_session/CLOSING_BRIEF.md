# B.3.8 closing pass brief (one owner, after the last caller batch merges)

You are the closing owner of ENDGAME B.3 step 8. Every exe-phase class in `code/Game` (+ the loop tables
and the Runtime bases they sit on) is a real C++ class with a compiler vtable on main; the pilot, the
14 class blocks (RB, K1, ACT, WRK, TOOL, CIT, DLG, PNL, LOOP, VIS, MI-A, MI-B, GOAL) and the seven
caller/fold batches (CB1a, CB1b, CB2-CB6) have merged, each behind the coordinator's gate
(`b38_session/GATE_TABLE.md`). You do what `b37_session/CLOSING_BRIEF.md` did, for step 8.

READ FIRST: `CLAUDE.md`; `B38_RULES.md` (all); `b38_session/{BLOCK_BRIEF,CALLER_BATCH_BRIEF,CALLER_SURFACE,
GATE_TABLE,LEFTOVERS_RAW}.md/.txt`; `b37_session/CLOSING_BRIEF.md` and `b37_session/CLOSE_NUMBERS.md` (the
numbers template — same units, same scripts shape); `B37_RULES.md` sections 10-14 (the shape of the
sections you write); the merge commit messages `git log --merges --format='%h %s' 1d7866d4..HEAD` and the
block/batch commits under them (their messages carry the reports' facts).

COMMITS, in this order (each behind the static gate; `git commit -F <file>`; title
`ENDGAME B.3.8 closing pass (b38-close) N: <what>`; end with
`Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`):
1. **`engine_funcs` zero-caller sweep BY BINDING** for the exe classes: every global in
   `engine/funcs/engine_funcs.{h,cpp}` whose initialiser resolves an exe address (`ResolveAddr(Module::Exe,
   0x14…)` — map slots through `python tools/dispatch_slot_map.py`, tables/bodies through
   `b38_session/step8_appendix_raw.txt`) that now belongs to a CONVERTED class (member, table, ctor) —
   count code-only references tree-wide (inline bodies in `engine_funcs.h` included, the initialiser
   excluded; a use inside a printf argument is not a caller). Zero → delete global + typedef + extern +
   initialiser (+ readiness guards / DiagPrint rows). The block/batch reports already name
   `g_AchievementDialog_Vtable`, `g_GoalDialog_Vtable`, `g_ModDetailsDialog_Vtable`,
   `g_ModUpdateDialog_Vtable`, `g_PauseDialog_Vtable`, `g_ReloadDialog_Vtable`, `g_ToolbarDialog_Vtable`,
   `g_TutorialInstanceVtable`, `g_FollowDialog_Vtable`, `fn_ConsumeProduceUI_Slot4`, `fn_DragTool_OnKey`,
   `fn_DragTool_Shutdown`, `fn_Work_SerializeBase`, `fn_ToolbarDescription_cleanup`, `fn_Geometry_Init`,
   `fn_Geometry_ChunkUpdate` — VERIFY each; then the rows/doors that only they read
   (`LEFTOVERS_RAW.txt` "CLOSE items": reg_code_lane2 0x14005f3d0, reg_code_lane6 0x1400fc6b0/0x14012db70,
   reg_player_drag 0x1400fc640, reg_mods 0x14015c670 + `ModDetailsDialog_GetVtable`, the
   AchievementDialog/GoalDialog rows in menu_dialog_standalone_vtables.cpp, the MainMenuDialog row, the
   empty `RegisterMenuDialogVtables`/`RegisterMainMenuDialogVtable` + calls, the InnerLoaderLoop row
   0x140158870, the six tool doors CB4 names once their rows are gone, `ForesterComponent_CommandMask`/
   `Command`, `ResourceProduceComponent_BaseShutdown`), and `engine_funcs.h`'s `struct SaveLoadDialog;` /
   `struct TownHallUI;` → `class`. Also any remaining `struct X;` forward declaration of a converted class
   tree-wide. Finish with a re-sweep of the final tree. A deleted row that makes a "NOT IMPLEMENTED" line
   appear at boot means something still reads it — the coordinator's step close checks the stub set.
2. **Dead code the batches left**: reader-less `RegisterExeAddr` rows of converted classes that remain
   (re-measure BY HEX AND BY DEMANGLED OWNER, B8-12 — CALLER_SURFACE §5/§7 at 9fb05e19 is the start
   list); `component_factory_base.*` / `controller_live_base.*` / the inert helpers if their last user is
   gone (CB1b counted ~86 users at its close — re-measure; if still used, list who); stale banners
   (comments that still say "hand-built table", "inert", "POD", "not a real class" for a converted class;
   `herdcomponent.cpp:1080`; `reg_code_lane5.cpp:81`; `toolbardescription.cpp:243`;
   `runtime/types/ui/DialogController.h`; the `RtEngine::` loop-POD comment at
   `runtime/types/engine/UpdateLoop.h:247` vs B38_RULES D8).
3. **`B38_RULES.md` section 8 (dated corrections from the blocks)** — every "rules/slots doc was wrong"
   item in the merge reports, e.g.: ENDGAME B.1 decision 2 (already corrected in §0.1); D1's list (three
   bases had no ctor door: DialogController, DialogControllerConfig, AnimationEvent; DialogInstance has a
   real default ctor 0x180114ca0; ~Particle 0x180103c70 and ~Light 0x180058d20 exist); B8-13 (the Tool
   createInPlace 0x14002d7e0 builds in place); controller slot 8 reads a "built" flag not the count;
   TutorialHighlightDialog / TutorialUI / TutorialGoalDialog / TutorialSetupDialog slot labels; Ghidra
   ctor/dtor label swaps (0x140125d80, 0x1400e01a0, 0x140044510, 0x1400c6970); `Animation.h` slot 8
   naming vs AnimAudioEvent; the FILEMAP misassignments (ModBrowseUI, thumb_vtables.cpp, …);
   `GameUpdateLoop` +0x30/+0x34 are seed/mapSize; the `RtEngine::` PODs STAY (runtime side); the menu
   scene factory-bit root cause (LOOP) and the resave golden re-cut (758597e2). Mark §6 NOT VERIFIED items
   closed / still open.
4. **`b38_session/CLOSE_NUMBERS.md`** (the `b37_session/CLOSE_NUMBERS.md` shape): B.4 cast metric per
   directory (LINE counts of `reinterpret_cast` over `*.cpp *.h *.inc`, build dirs excluded) at the start
   `1d7866d4` (measure in a throwaway `git worktree add`, then remove it) and now; `// B.3.8 bridge`
   lines per directory; hand-built `void* g_*[N]` vtable arrays in `code/Game` + `standalone` (start vs
   now — the headline of step 8); `RegisterExeAddr` calls (functions / vtables / data) start vs now;
   `REGISTER_TYPE` vs `DEFINE_EXE_TYPE` counts; `GetTypeIdByName("…")` literal sites start vs now;
   delegating members left (the CALLER_SURFACE §6 definition) start vs now; unit tests 516 → now; the
   class list from the `code/Game/**.h` headers cross-checked against `B38_SLOTS.md` (parent, slot count,
   sizeof — N of N agree, list any disagreement); PORT GAP / STANDALONE DEVIATION markers per directory
   (the honest ceiling); `engine/types` headers start vs now.
5. **`B38_RULES.md` sections 9 (the blocks and batches as run — the coordinator's gate table is
   `GATE_TABLE.md`, paste it), 10 (deliberately left, with sizes: PORT GAP slots, leaking dtors, the lazy
   `X_ResolveFuncs` resolvers, the `g_tid_*`/type-id-cell reads, the kept vtable rows with their readers,
   the mint/`_typeInfo`/`RT_DELDTOR` rows, singleton storage, unproved raw dispatches per directory), 11
   (corrections from the close).**

GATE per commit (FOREGROUND, 600000 ms): `mingw32-make -j12 standalone-debug`, `standalone-perf`, `unit`
(0 failures), `python tools/dispatch_slot_map.py --check` (0), `python tools/cutover_manifest.py --gate`,
`python tools/rt_mint_gen.py --check` (then `git checkout -- standalone/rt_export_addrs.inc`); no new
warning in touched files (compare against a build of your branch point). No game runs (the coordinator
runs the step close — but a deletion that could change boot output must be listed in your report).
HARD RULES: never `find /`; never `getenv`; no `pacman`; no `git stash`/amend/rebase/push; never hand-edit
`.inc`; a crash is never fixed with a null check; do not delete a door/row whose reader exists under ANY
binding spelling; do not touch `tests/unit/sources.mk`; no sub-agents that edit files.

REPORT: commit list; globals/rows/doors deleted and kept (reason per kept); the numbers file path and its
headline numbers; every correction you recorded; what you did NOT do.
