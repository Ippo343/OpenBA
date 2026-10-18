# HITCH_STDERR hand-in (2026-10-18)

Branch `stderr`, based on `hitch` `8dfa0921a`. It generalises HITCH_POSTDRAIN's `ErrLine` finding to the
whole tree without editing a call site. **Result: every `fprintf` / `std::fprintf` / `vfprintf` to stdout
or stderr is now one write, in every flavour. Debug `printf` is too. The bytes are unchanged.** Faithful:
it is an I/O path change only, with no sim change and no flag. Not merged.

| Commit | What |
|---|---|
| `ad425ea34` perf | `src/app/one_write_stdio.h`, force-included; the CMake wiring; `tests/unit/one_write_stdio_test.cpp` |
| (this commit) docs | this hand-in, the `perf/README.md` note |

## 1. What is per character (measured, scratch benchmark, `_IONBF` file, 200 lines of 90 characters)

| writer | us per line |
|---|---|
| `fprintf(stderr, fmt, ...)` | 176 |
| `fprintf(stderr, "literal with no conversions\n")` | 151 |
| `vfprintf(stderr, fmt, ap)` | 156 |
| `printf(fmt, ...)`, stdout `_IONBF` | 181 |
| `fputs(line, stderr)` | 4.5 |
| `fwrite(line, 1, n, stderr)` | 4.4 |

- **`vfprintf` and `printf` are per character too.** All three go through `__mingw_vfprintf` ->
  `__pformat_putc` -> `fputc`. The brief assumed `vfprintf` was one write. It is not, so the wrapper
  covers it. `ErrLine`'s own long-line fallback was a `vfprintf`.
- **A literal `fprintf` is per character as well.** MinGW's `fprintf` is an inline overload, so GCC never
  turns it into `fputs`.
- `fputs`, `fwrite` and `fputc` were already one write per call. They are left alone, and so is `perror`.

## 2. The mechanism: `src/app/one_write_stdio.h`

- **Force-included** into every OpenBA and `unit` translation unit with `-include`, beside `log_strip.h`
  (`CMakeLists.txt`, `BANISHED_ONE_WRITE_STDIO`). Every flavour.
- **Function-like macros:**
  - `fprintf(...)` -> `BanishedOneWriteFprintf(...)`
  - `vfprintf(...)` -> `BanishedOneWriteVfprintf(...)`
  - `printf(...)` -> `BanishedOneWritePrintf(...)`, only without `BANISHED_STRIP_LOGS`. Perf and release
    still strip `printf` in `log_strip.h`.
- **Why function-like.** `namespace std { using ::BanishedOneWrite...; }` makes `std::fprintf(` and
  `::fprintf(` reach the wrapper too. Non-call spellings stay untouched. That matters for
  `__attribute__((format(printf, 1, 2)))` in `imgui.h`, which our overlay includes, and in
  `text_reader.cpp`. An object-like `#define printf` would break those.
- **The body.**
  - When the stream is `stdout` or `stderr`, it formats with `vsnprintf` into a 2 KB stack buffer and
    calls `fwrite` once. `vsnprintf` is the same `__mingw_pformat` engine, so the bytes are the same.
  - Every other `FILE*` goes to the real `vfprintf`: the stress CSVs, compprof, the boot-state file, the
    sampler. A dump routine that takes a `FILE*` and is handed `stderr` is covered at run time.
  - A line of 2 KB or more, or a format error, falls back to the real `vfprintf`. That is per character,
    as before. There is no heap use, because these calls run in crash and hang paths.
  - The return value is `fprintf`'s: the character count, or negative on a write error.
- **The same guarantees.** The streams stay `_IONBF`, so a line is written before the call returns. One
  `fwrite` holds the stream lock for the whole line, as `fprintf` did, so lines from two threads still
  do not interleave. Text-mode `\n` -> `\r\n` happens in both paths.
- **Coverage.** The header is empty off MinGW: glibc already stages an unbuffered printf through one
  buffer. It is also empty under `BANISHED_THIRD_PARTY_SOURCE`, which CMake sets on the vendored
  `ASSAODX11.cpp` and on the Dear ImGui sources. SDL, wmadec and FreeType are separate C targets and
  never see it. `font_dump` is not covered; it is a by-hand harness.
- **A CMake trap fixed on the way.** CMake de-duplicates repeated option tokens. With two force-includes,
  the second `-include` was dropped and its path became a bare input file, giving "cannot specify -o with
  multiple files". Both force-includes now use the `"SHELL:-include <file>"` form.
- **`ErrLine` stays.** The six converted files keep it: it is the same bytes and the same one write, and
  is now redundant. Its header says so.
- **Compiler output.**
  - The perf and debug exe builds give the same warning set before and after (183 and 109 lines).
  - The `unit` build gives the same 108 warnings. The pre-existing `-Wformat` in
    `ext_fast_paths_test.cpp` / `ext_path_workers_test.cpp` now reports the first line of a multi-line
    call, because of the macro.

## 3. Why not `setvbuf(stderr, ..., _IOLBF)`

- **The CRT treats `_IOLBF` as full buffering.** A crash or `ExitProcess` would lose everything since the
  last flush, including `[AutoStart]`, `[BootState]` and the trace lines that came just before a fault.
  The streams are unbuffered for that reason.
- **A flush-at-newline wrapper is this header.** It formats one call into a buffer and writes it before
  returning, so nothing is held across calls.

## 4. Identity proof

- **The route.** `BANISHED_AUTOSTART=1`, new game, `BANISHED_SEED=31337`, `BANISHED_MENU_SCENE=0`,
  `/profile:faithful`, quit through the pause menu at frame 600 (`BANISHED_PAUSEMENU=600`,
  `BANISHED_PAUSEMENU_QUIT=120`).
- **The legs.** Five legs in one lock hold: debug before twice, debug after, perf before, perf after. All
  exited 0 with an empty crash log. The lock's CLOBBER flag in that hold is my own exe swap between legs:
  the hashes alternate between the before and after builds.
- **Normalisation.** `0x` hex and bare 8-16 digit hex words (pointers, hashes) become `H`, then every
  decimal number becomes `N` (times, counts, frame numbers). The comparison is in order, and also as
  sorted line sets, because threads interleave.

| file | before vs before (noise) | before vs after |
|---|---|---|
| perf `standalone_debug.log` (414 lines) | | **raw byte-identical** |
| perf `standalone_error.log` (496 lines) | | 1 line: the random town name |
| debug `standalone_error.log` (506 lines) | 1 line: the town name | the town name, and one `[LoadTime] loader.resources` line (loader thread) interleaved one line earlier |
| debug `standalone_debug.log` (~2450 lines) | 37 sorted-line differences | 51 |

- **The debug-log residuals are wall-clock dependent in both columns.** They are how many splash-screen
  `[UIFade::SplashDLT::Draw]` / `[Stall]` lines a run prints, and `[BootTime]` column padding when a
  millisecond value gains a digit. Nothing else differs.
- **The unit test proves byte identity directly.** It points fd 2 at a temporary file and writes through
  `fprintf`, `std::fprintf`, a `vfprintf` helper, a 3,000-character line (the fallback) and an empty
  format. It compares the file byte for byte with `vsnprintf` plus the text-mode `\r\n`, and checks the
  return values. A second test checks that a `tmpfile()` stream is untouched.
- **No line reached the 2 KB fallback** in any identity log or stress error log. The longest is 438
  characters.

## 5. Before / after: stress town, fed, 50x, 12000 frames, faithful

Machine load was BUSY on every leg, because other agents were compiling. Leg 1 ran under heavier load
(median CPU 85%, up to 12 compilers) than the rest (76-81%, 2 compilers). Pairs 2 and 3 ran back to back
in one lock hold, in opposite orders. All six `exe_identity.txt` files show the deployed exe unchanged
from start to end.

| leg | results dir | hitch rows | harness's own rows (max excess) | `update-other` 180 / 300 |
|---|---|---|---|---|
| before 1 | `perf/results/20261018_104830_stderr_before` | 67 | 20 (55.3) | 34.4 / 18.2 |
| before 2 | `perf/results/20261018_111030_stderr_before2` | 35 | 20 (39.2) | 26.3 / 10.6 |
| before 3 | `perf/results/20261018_112517_stderr_before3` | 32 | 20 (56.5) | 26.7 / 11.3 |
| after 1 | `perf/results/20261018_110432_stderr_after` | 9 | 1 (13.2) | - / - |
| after 2 | `perf/results/20261018_111407_stderr_after2` | 12 | 2 (17.0) | - / - |
| after 3 | `perf/results/20261018_112140_stderr_after3` | 10 | 1 (13.5) | - / - |

- **The harness row was mostly stderr.** Every 600-frame row printed the component profile (`[COMPPROF]`
  lines, ~10 KB) to stderr as well as to its file, one WriteFile per character.
  - Before: the median `harness_ms` is 25-27 ms, 20 rows per leg.
  - After: 13-14 ms, and only the frame-600 row (plus 1200 once) is still over the criterion.
  - The CSV row and the census are what is left.
- **The orchard / crop-field force-start rows at frames 180 / 300 are gone.** HITCH.md called them
  "dialog open + stderr Trace". They were the trace.
- **Sim rows that remain in the after legs** (`ph_entitylist` / `ph_citizenlist`, 7-14 ms):
  - the post-drain frames 749 / 1109;
  - the cold-wave pairs (11568-11569, 11947-11948): H4, retail's own work, gone with ext.spread_warmth;
  - scattered 7-10 ms EntityList bumps.
  - The before legs have the same kinds plus the stderr ones.
- **One unexplained frame.** after 2, frame 1850: a single 182 ms `ph_entitylist` period, with no burst
  in the log around it and no long line. It did not recur in after 1 or after 3, and nothing in the
  change can block for 182 ms that the per-character path could not. I read it as a descheduling or a
  blocked file write on the loaded machine. It is not attributed.

`update_ms_per_iter`, pairs 2 + 3 averaged (leg 1 left out: heavier load):

| rows | before -> after |
|---|---|
| 600-3600 | -2% to -4%, one row +2% |
| 4200-12000 | -3% to -5% (9600: -12%, one after leg's dip) |
| whole-run mean, 3 legs each | before 0.2074 / 0.1726 / 0.1716, after 0.1661 / 0.1647 / 0.1661 |

The legs do not hold the same citizens row for row, because stress runs are not reproducible. So the
per-row deltas carry that noise as well as the load.

stderr volume. The bytes are the same by construction. What changed is one write per character down to
one per line.

| leg | bytes | per frame |
|---|---|---|
| before 1 / 2 / 3 | 722,849 / 824,754 / 710,408 | 60.2 / 68.7 / 59.2 |
| after 1 / 2 / 3 | 712,580 / 711,085 / 720,503 | 59.4 / 59.3 / 60.0 |

The 824 KB of before 2 is that run's own deaths and burials. stdout in perf holds only the boot lines,
because `printf` is stripped.

## 6. Gates

- `tools/build.sh unit perf --jobs 2`: 1169 tests, 0 failures, 3 skipped. That is 1167 plus the two new
  tests; the skips are the kit fonts / kit cross-check.
- `tools/acceptance/acceptance.sh`: 0 lines off target.
- **Not run:** the game gate (`tools/build.sh run`), the resave gate, the 10x / 20x legs, Linux (the
  header is empty there), and a clean-machine stress pair.

## 7. Open

- **A clean-machine pair** would put firm numbers on the per-iteration gain. The hitch-row verdict does
  not need one: 20 -> 1-2 harness rows, and the 180 / 300 rows gone, in all three pairs.
- **Lines over 2 KB still write per character.** None occur in today's logs. A one-call multi-line dump
  over 2 KB would, and splitting it into lines fixes it.
- **The after 2 frame-1850 period** (182 ms, once) is unattributed.
