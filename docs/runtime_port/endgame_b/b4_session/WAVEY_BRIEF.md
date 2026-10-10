# B.4 wave Y — executing the re-inventory by class (agent brief)

Read first, in this order: `../B4_RULES.md` (all, §8 corrections included), `PASS2_BRIEF.md` (the rules that
cost a re-run, B11, working discipline, hand-in), `PASS2X_BRIEF.md` (the five steps of a family, the
PARTITION rule, the line procedure), then this file. Where they differ, this file wins.

## Your job
You own ONE region of the tree. Your inventory is `data/pass2y/out/<REGION>.md` (one page),
`<REGION>_units.tsv` (one row per unit: retail ctor / dtor, the dtor's release ORDER, the neighbours that must
be typed in the same commit, the reader files, the verdict) and `<REGION>.tsv` (one row per cast line).
Execute every unit whose verdict is `doable` / `doable-with:<neighbours>`, LARGEST FIRST, one unit per commit:
- `CLASS` units that move construction / destruction: ONE commit titled `behaviour: B4 Y <REGION>: …` quoting
  retail's ctor AND dtor addresses. FIELD-only units (a trivial POD field retyped): a plain `B4 Y <REGION>:` commit.
- `SIG` units: the five steps of PASS2X_BRIEF (declaration + definition + EVERY caller in one commit).
- `POD` / `LOCAL` units: by name; a stale tag is deleted with its cast.
- `RULING` and `ERASED … stays` units are NOT your work: do not touch those lines (the coordinator rules
  once per class at the close). `blocked` units: only if you can PROVE the blocking finding no longer holds
  (say so in the hand-in); otherwise leave them.

The inventory is a MAP, not a proof. Before you build on a row, re-read retail (Ghidra, read-only,
`program=` on every call: `Application-x64.exe` for `code/`, `Runtime-x64.dll` for `runtime/`; the DISASSEMBLY
for an order or a signature, never the decompiled C alone). Where retail contradicts the row, retail wins:
do what retail does if it is still a typed-member commit, else LEAVE the unit with the finding.

## What makes a `behaviour:` CLASS commit correct (every point, checked at merge)
1. The typed members, in declaration order, are constructed where retail's ctor constructs them and
   destroyed (reverse declaration order, AFTER the dtor body) exactly where retail's dtor releases them.
   Type the neighbours in the same commit when the order needs it.
2. The hand-written ctor stops zeroing / building the member; the dtor stops releasing it; nothing
   constructs it twice (search for placement new, `Rt…_Ctor` doors, lazy `::new` arms).
3. Nothing memsets / memcpys the class or `static_assert`s it trivial (Grep the class name across
   `code runtime standalone dx11 engine tests`). A memset that stops short of the member is fine: say where.
4. Every "is it built yet" test (a null-vptr probe, a lazy construct arm) is replaced by WHAT RETAIL DOES AT
   THAT SITE (read the site in retail), not merely deleted.
5. `static_assert(offsetof…)` on every member touched and `sizeof` of the class still hold.
6. A class local with a constexpr / zeroing ctor ADDS stores: use a ctor-less POD, or leave.
7. No null guard added or dropped; no log text changed; no container swap; H7 / H8 as in PASS2_BRIEF.

## Boundaries
- Your region's files are yours. The PARTITION rule of PASS2X_BRIEF applies: retyping a declaration / member
  DEFINED in your region lets you edit its reader / call-site LINES in another region's file (those lines
  only; list each). Another region's declaration is not yours: the cast stays `// LEFT: SIG <name>`.
- Your prompt names the units that cross regions and who owns them. A header another running agent owns is
  read-only.
- `docs/known_port_issues.md`: append only, in your reserved row range (in your prompt).
- Never `git add` this session directory wholesale; scratch goes in `<scratchpad>/b4y/<REGION>/` (outside the repo).

## Working discipline (PASS2_BRIEF, unchanged; the points that were broken before)
- You are in a git worktree. Branch `b4y2-<REGION>` (create it at your start: `git checkout -b`). `git log
  --oneline -1` must show the hash in your prompt. Never `git stash`, amend, rebase, push; no sub-agents; no
  search outside the worktree (never `find /`); never `pacman`.
- NEVER run the game (`sa-run`, `sa-deploy`, the exe, anything touching `C:/GOG Games/Banished`). You gate
  statically: debug + perf + unit builds, B11, the metric.
- Builds in the FOREGROUND at `-j2` only (`/c/msys64/ucrt64/bin/mingw32-make.exe -j2 <target>`; Bash timeout
  600000 ms; re-invoke when it times out, make is incremental). Output to a file; read only the errors.
  The first base build of a fresh worktree is long (about 40 minutes at -j2 for debug, the same for perf):
  start it FIRST, keep its objects for B11 (`cp -r build/debug/obj …`, `build/perf/obj …`),
  read retail while it runs.
- Commit per unit; each commit builds. When most of your context is used, STOP at a building commit and hand
  in with the exact list of what remains. An honest partial hand-in is worth more than an unverified full one.
- B11 at the tip, -O2 AND -O0, every DIFF / RENAMED-DIFF / ONLY-* of the perf report READ and explained in
  one line each in the hand-in. For a `behaviour:` commit the explanation is retail's address.
- Before the final build: `git merge main` (`python docs/runtime_port/endgame_b/b4_session/keepboth.py
  docs/known_port_issues.md` resolves the append-only conflict).

## Hand-in (final message, under 60 lines)
Branch and last commit; debug / perf / unit exit codes (unit: 918 / 0 or the new count); new warnings;
metric base -> tip for the tree (`castmetric.py code runtime standalone dx11 engine | tail -1`); units DONE
(name, casts removed, commit); units LEFT with the one-line finding each; `behaviour:` commits with retail's
ctor / dtor addresses; cross-region lines edited; B11 totals at -O2 and -O0 with one line per DIFF kind;
`known_port_issues` rows added; what you did NOT verify, plainly.
