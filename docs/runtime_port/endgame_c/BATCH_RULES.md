# ENDGAME C.2 / C.4 batch rules (coordinator, 2026-09-13)

Input: `docs/runtime_port/endgame_c/FLAGS.md` (the tiered inventory). Each batch owns a
disjoint set of files (stated in its prompt) and edits ONLY those. A flag whose readers
straddle two batches is handled by each batch in its own files; the edits compose.

## Per-tier action
- **GRADUATE** (96): delete the gate, keep the live body unconditionally. The `else` /
  off arm (inert stub, "older path", "harness-driven path") is deleted with the gate.
  Remove the entry from `kDefaultOnFlags` (comp-1 owns that file). Keep the GRADUATED
  comment's substance as a plain comment if it records a finding; drop the flag name.
- **DELETE** (282): delete the read AND the code only it enabled. If the guarded block does
  anything other than print/count/dump/assert (i.e. it changes game state), stop and check:
  a default-OFF feature nobody sets is deleted; a block that a KEEP-T3 flag also enters keeps
  the code and loses only this flag's read. Put every such non-trivial case in your report.
- **OPT-IN, coordinator-decided**: DELETE `BANISHED_AUDIO_AUDIT`, `BANISHED_AUDIO_PROBE`,
  `BANISHED_PROBE_WORKPLACE_KEY`, `BANISHED_LIVE_PRODUCTION`, `BANISHED_LIVE_WORKPLACE`,
  `BANISHED_LIVE_CITIZEN_POSTLOAD_TOOL` (diagnostics / default-off harness / known deviation).
  LEAVE THE OTHER 12 UNTOUCHED (CLEAR_FOOTPRINT, LIVE_ANIMALS, AUTOSAVE, BUILD_ONREMOVE,
  CEMETERY, CITIZEN_FAMILY, CROPDISEASE, FOUNDING_SPAWN, HAPPINESS, HEALER, MARKETLOC,
  TOMBSTONE): a forced-on gate run decides them in a later pass.
- **KEEP-T3** (67) + the whole `BANISHED_BUILD_DEMO_*` / `BANISHED_FORCE_*` family
  (ENDGAME §C.4 keeps the family; FLAGS.md's DELETE tier for some members is overridden):
  leave the `getenv` read as is. A later pass moves them onto `standalone/options.h`.
- Do not touch `standalone/` (another batch owns it). Do not add any new flag.

## Hygiene (project memory; every point has cost a run before)
1. Edit in place; never rewrite a file with Write. Never drop an existing function.
2. Build: `mingw32-make standalone-debug -j8 > build.log 2>&1; echo $?` — the exit code,
   not a pipe's. Fix every error and every new warning about unused statics you exposed.
3. One real run: `tools/game_dir_lock.sh acquire <batch> 900` (it is shared by ~6 agents;
   wait for it), then deploy: `cp standalone/build/reimpl-x64.exe "/c/GOG Games/Banished/"`,
   `rm -f "/c/GOG Games/Banished/standalone_{debug,crash,error}.log"`, then from that
   directory `BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120
   ./reimpl-x64.exe; echo exit=$?`, in the FOREGROUND with a 600000 ms Bash timeout
   (never run_in_background; never wait on a notification; if it times out re-invoke
   and check `tools/boot_state.sh`). Pass = exit 0, no standalone_crash.log, `[AutoStart]`
   lines in standalone_error.log. Release the lock in the same command line as the run
   (`; tools/game_dir_lock.sh release <batch>`) so a timeout cannot leave it held.
   If it dies, read standalone_crash.log FIRST and symbolize with
   `addr2line -f -C -e standalone/build/reimpl-x64.exe`.
4. Before the final build: merge `main` into your branch, keep both sides on conflicts,
   rebuild, rerun step 3.
5. Commit on your worktree branch (message: `ENDGAME C.2/C.4 batch <name>: ...`). Do not
   push. Do not run the resave gate or tools/cutover_closure (coordinator only).

## Report (short)
Flags graduated / deleted (counts, and the list of non-trivial DELETE cases with what you
decided), `grep -c 'getenv(\|LiveEnabled(' <your files>` before/after, build exit code,
run exit code + crash-log state, commit hash, anything left and why.
