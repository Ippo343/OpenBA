# Wave 10 Lane PERFFIX — standalone-perf build fix

## Problem

`mingw32-make standalone-perf` failed on `wave10/base`. `standalone/log_strip.h`
strips logging in release/perf builds via `#define printf(...) ((void)0)`. The
preprocessor cannot rewrite a qualified call, so any `std::printf(...)` site
expands to `std::((void)0)` and fails to compile. This exact invariant is
already documented in `log_strip.h`'s own comment block (added by a prior fix,
commit `965361d`, which unqualified two sites in `entitylist.cpp` and the same
day's landing broke this file's neighbours by continuing to use `std::printf`
in new code without following that rule).

## True count (measured, not assumed)

```
grep -rl "std::printf" code engine runtime standalone --include=*.cpp --include=*.h --include=*.hpp | wc -l
```
gave **50 files**, but one of those (`standalone/log_strip.h` itself) only
*mentions* `std::printf` in a comment describing the invariant — it has no
call site. The actual affected set is **49 files** containing **270**
`std::printf(` call sites. This matches the number given in the task
description; I verified it directly rather than trusting it.

`code/Game/Dialog/ui_dialog_stub_factories.cpp` also matched the original grep
but turned out to use `std::fprintf` (not `std::printf`) at every call site —
`fprintf(stderr, ...)` is explicitly exempted from stripping per
`log_strip.h`'s own rules (kept for crash/FATAL diagnostics), so it needed no
change. It only matched the initial file-list grep because of the same
explanatory comment quoting `std::printf` as prose.

## Fix chosen: normalize call sites (not the strip macro)

Two directions were possible:
1. Make the strip mechanism handle the qualified spelling (e.g. also
   `#define std::printf(...) ((void)0)` — not legal C preprocessor syntax for
   a scoped name, or wrap with a namespace-level `#define std ...` trick —
   fragile and would only mask future new sites reintroducing the same bug).
2. Normalize every `std::printf(` call site to unqualified `printf(`.

Chose **(2)**, because:
- It matches the project's own already-established convention for this
  exact file (`log_strip.h` header comment already mandates unqualified
  `printf`; `entitylist.cpp` was already fixed this way in commit `965361d`).
- `std::printf` and unqualified `printf` are the same function (`<cstdio>`'s
  `using std::printf;` in most implementations, and `log_strip.h` includes
  `<cstdio>` before undefining `printf`), so this is a pure spelling
  normalization with no semantic effect on non-stripped (debug) builds.
- The alternative would need a macro trick to intercept a *qualified* token,
  which the C preprocessor does not support for scoped names — there is no
  clean equivalent of macro-replacing `std::printf`.

Applied with a plain `sed 's/std::printf(/printf(/g'` over 47 of the 49
affected files (log_strip.h's comment text was left untouched — it is
documentation of the invariant, not a call site). The remaining 2 files sit in
another lane's owned territory — see below.

## Build verification

`standalone` (release) and `standalone-perf` both pass `STRIP_LOGS_FLAGS`
(`-DBANISHED_STRIP_LOGS -include standalone/log_strip.h`, see Makefile lines
98/103/146), so **both** targets hit the exact same qualified-call problem.
`standalone-debug` does not pass that flag, so `log_strip.h` is never
included there and `std::printf` compiles fine regardless — debug was never
affected by this bug.

With this branch's 47-file fix alone (the committed state):
- `mingw32-make standalone-perf` — fails only on the 2 out-of-scope UI files
  (confirmed by a clean `rm -rf standalone/build-perf` + rebuild):
  ```
  mingw32-make[1]: *** [...obj/runtime__src__ui__dialog.o] Error 1
  mingw32-make[1]: *** [...obj/runtime__src__ui__dialog_settings_probe.o] Error 1
  ```
- `mingw32-make standalone` (release) — **fails identically**, on the same 2
  files, for the same reason (it uses the same `STRIP_LOGS_FLAGS`). This is
  **pre-existing on `wave10/base`**, not something introduced by this lane's
  fix: the `std::printf` sites in `runtime/src/ui/dialog.cpp` and
  `dialog_settings_probe.cpp` were already present, unmodified, before any of
  my edits (verified via `git diff` against the committed content). The
  project's own memory log independently records that "the RELEASE target was
  already broken PRE-EXISTING" as of the Wave 9 consolidation — this is that
  same latent defect, not a new one.
- `mingw32-make standalone-debug` — builds clean (`standalone/build/reimpl-x64.exe`,
  ~29 MB, `-O0 -g`, logs intact). Unaffected by any of this because
  `log_strip.h` is never included in debug builds.

To prove the *mechanism* of the fix is correct and the perf target's true
blocker really is scoped to just those 2 files, I applied the same
one-line-per-site normalization **locally and temporarily** to
`runtime/src/ui/dialog.cpp` / `dialog_settings_probe.cpp`, rebuilt
`standalone-perf` clean end-to-end (`standalone/build-perf/reimpl-x64.exe`,
~32 MB, `-O2 -g`), confirmed success, then reverted both files byte-for-byte
before committing anything (`git diff -- runtime/src/ui/` is empty on this
branch). The same would make `standalone` (release) build clean too — I did
not commit that revert-test, only used it to validate scope.

## Proof the strip is still effective

A fix that compiles but silently leaves the newly-unqualified `printf` calls
in the perf binary would defeat the whole point of `BANISHED_STRIP_LOGS`. Verified
with `strings` against bracketed log tags that are unique to printf call sites
this lane touched (present only inside `printf("[TAG] ...")` literals, not in
any symbol name):

| tag            | debug (logs intact) | release | perf |
|----------------|---------------------|---------|------|
| `[PlatOracle]` | 5                   | 0       | 0    |
| `[AvlColl]`    | 33                  | 0       | 0    |
| `[PlatProbe]`  | 6                   | 0       | 0    |
| `[LanePLAT]`   | 1                   | 0       | 0    |
| `[SetAvl]`     | 10                  | 0       | 0    |
| `[Encoding]`   | 6                   | 0       | 0    |

(An unqualified substring match on `PlatOracle` alone also hit mangled symbol
names like `PlatOracleSnapshotClass` in both release and perf — that is
expected and unrelated to the printf strip; the bracketed-tag match above
isolates the actual format-string literals and confirms they are gone.)

Binary sizes also corroborate: perf (`-O2 -g`, un-stripped-of-symbols but
logs-stripped) is 33.5 MB; debug (`-O0 -g`, logs intact) is 29.8 MB; release
(`-O3`, logs stripped, no `-g`) is 7.6 MB.

## Scope discipline

Only the 49 files' `std::printf(` call sites were touched — no other lines
changed, no feature flags touched, no runtime port bodies, address registry,
or `runtime/src/ui/`, `standalone/allocator*`, `standalone/rt_alloc_swap.cpp`
(ALLOC/EXTPTR/UI-lane territory) modified.

## Scope conflict found mid-fix: 2 sites live in UI-lane territory

Two of the 49 files with `std::printf(` sites are inside `runtime/src/ui/`,
which this lane's hard rules forbid touching (owned by the UI1/UI3/UI4/UIDATA/
UIF/UIORACLE lanes this wave):

- `runtime/src/ui/dialog.cpp` (1 site)
- `runtime/src/ui/dialog_settings_probe.cpp` (2 sites)

I applied the same mechanical `std::printf(` -> `printf(` fix to these two
files locally, confirmed it makes `mingw32-make standalone-perf` build the
**entire** tree cleanly (`standalone/build-perf/reimpl-x64.exe`, ~32 MB), then
**reverted both files** (`git diff -- runtime/src/ui/` is empty on this
branch) rather than commit changes in another lane's owned area.

**Net result on this branch (`wave10/lane-perffix`)**: `mingw32-make
standalone-perf` still fails, but *only* on these two pre-identified files —
confirmed by a clean rebuild:
```
mingw32-make[1]: *** [Makefile:207: standalone/build-perf/obj/runtime__src__ui__dialog.o] Error 1
mingw32-make[1]: *** [Makefile:207: standalone/build-perf/obj/runtime__src__ui__dialog_settings_probe.o] Error 1
```
47 of the 49 affected files are fixed and committed here. Whoever owns
`runtime/src/ui/` on this wave (or the integrator at merge time) needs to
apply the identical one-line-per-site normalization to these 2 files. I
messaged the active UI lanes about this directly rather than silently leaving
it for merge time to discover.

**Update**: lane UIF applied the fix to their 3 sites on `wave10/lane-uif`
(commit `bbe90d7`) shortly after this report. UIF also correctly flagged that
my "only thing standing between the tree and a clean perf build" framing was
branch-relative, not a statement about `wave10/base`: on UIF's branch
(base + UIF's own lane), `standalone-perf` still fails at the ~267 other
`std::printf` sites this lane (PERFFIX) fixed — `array.cpp`, `array_destroy.cpp`,
etc. are unfixed there. **Neither lane's branch alone yields a green perf
build; the integrator needs BOTH `wave10/lane-perffix` and `wave10/lane-uif`
merged together** for `mingw32-make standalone-perf` (and release `standalone`)
to build clean.

## Sanity run — not attempted

`tools/game_dir_lock.sh status` showed the shared `C:/GOG Games/Banished`
directory lock **held by another lane** (`wave10-ui1`) at the time this lane
finished. Per the hard rule against blocking lock acquires, no deploy/run was
attempted. The perf exe (`standalone/build-perf/reimpl-x64.exe`) builds and is
ready for a sanity run once the lock frees, but that was not verified here.

## Worktree / branch

- Worktree: `C:\code\GIT\untitled_rev_eng_project\.claude\worktrees\agent-aabba9a5e814db44b`
- Branch: `wave10/lane-perffix` (branched from `wave10/base` at `94ca43a`)
