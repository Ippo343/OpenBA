# Second-pass brief — B.4, blocks cut by BLOCKER (2026-10-03)

Read first, in this order: `../B4_RULES.md` (all, corrections §8 included), `BLOCK_BRIEF.md`,
`W1_BRIEF.md`, then this file. Where this file differs from those, this file wins.

The first pass (blocks cut by directory) left 8,950 casts. Almost every one waits on something
ANOTHER file owns: a `void*` field in a header, a C signature taking an integer, a raw-storage
member, a forwarder. The second pass gives each agent ONE BLOCKER FAMILY: you own the header that
changes AND every file that reads it. Your block is a section of `PASS2_FILEMAP.md`; your files are
exactly the ones it lists. Everything else is read-only.

## What a second-pass block does
1. **Remove the blocker at its declaration**: type the field (`void*` -> `T*`, H4), type the
   signature (parameters and return get retail's types, B6), name the record that had no type (a
   new struct with `static_assert(offsetof)` on every field and `sizeof`, layout from RETAIL: the
   ctor, the allocation size, the instructions that read it).
2. **Convert every reader in your files**: the cast goes because the source is now typed. A raw
   offset read (`*(int*)(p + 0x28)`) becomes the named field of the class the pointer really is.
3. **Rewire callers** of a function whose signature you typed, in your files; where a
   `// B4 FORWARD` forwarder is left with no caller anywhere (Grep the whole tree: `code runtime
   standalone dx11 engine tests`), delete it and its declaration in the same commit.
4. A cast you cannot remove without changing behaviour, or whose blocker is outside your block,
   keeps its ORIGINAL spelling plus `// LEFT: <owner / reason>` and goes in your hand-in.

## Rules that cost a re-run in the first pass (each is checked at merge)
- **B.4 changes how a field is REACHED, never what the code computes.** No container swapped for
  another, no algorithm rewritten, no local resized, no null test or failure return dropped or
  added, no loop bound "corrected", no call reordered. If retail contradicts the tree, that is a
  row in `docs/known_port_issues.md` entry 14 (your reserved row range is in your prompt), NOT a
  fix — unless it is a commit titled `behaviour:` quoting retail's address, which this pass makes
  only for H5 members (below).
- **A respelling is not a removal** (M5): C-style -> `reinterpret_cast` / `static_cast` through
  `void*` / `uintptr_t`, a union, a `memcpy`, a typed-view helper containing the cast. The metric
  counts every spelling; a column that RISES in any file is rejected.
- **`bool` normalises a byte (H8)**: where retail copies a byte that may not be 0 / 1, the field is
  `uint8_t`, or the store is LEFT. **A merged store split into named stores writes every byte
  (H7)**: an 8-byte zero store over two 4-byte fields becomes two stores, in address order, and
  padding bytes retail wrote are still written (declare the padding as a field).
- **Width and signedness are retail's** (MOVSX / MOVZX, the MOV's width, the compare's condition
  code). A field typed wider or narrower than a read site changes the value read: check each site.
- **H5 members** (raw storage of a `String`, `Array`, embedded object): typed ONLY in a
  `behaviour:` commit of its own, with retail's ctor AND dtor of the owning class read in the
  disassembly and quoted (address), proving the tree's ctor / dtor then do exactly what retail's
  do. If you cannot prove it in reasonable time: LEFT, listed with the member.
- **Never guard.** A faithful unguarded dereference stays unguarded. Never `getenv`. No new flag,
  helper, cache, instrument. No door (nothing resolves a body by retail name or address). Nothing
  reads a vtable by literal index.
- **Warnings**: a named type changes a standing warning's wording. No NEW warning text in a file you
  touched, in debug or perf. A local read only by a log line needs `[[maybe_unused]]` (the perf
  build strips logs).

## B11 — the object comparison is mandatory, at -O2 as well as -O0, and its diffs are READ
Before your first edit, build the base and keep its objects:
```
/c/msys64/ucrt64/bin/mingw32-make.exe -j2 standalone-debug > build_debug.log 2>&1; echo $?
/c/msys64/ucrt64/bin/mingw32-make.exe -j2 standalone-perf  > build_perf.log  2>&1; echo $?
cp -r standalone/build/obj      <scratch>/base_debug_obj
cp -r standalone/build-perf/obj <scratch>/base_perf_obj
```
At the tip (after `-j2 standalone-debug`, `-j2 standalone-perf`, `-j2 unit`, all exit 0):
```
python docs/runtime_port/endgame_b/b4_session/b11cmp.py <scratch>/base_perf_obj  standalone/build-perf/obj --out <scratch>/b11_perf.txt
python docs/runtime_port/endgame_b/b4_session/b11cmp.py <scratch>/base_debug_obj standalone/build/obj      --out <scratch>/b11_debug.txt
```
The tool pairs a function whose signature changed (`RENAMED-SAME` / `RENAMED-DIFF`).
Every `DIFF`, `RENAMED-DIFF`, `ONLY-BASE` and `ONLY-TIP` function of the PERF report is read (the call list, the
store widths, the compares it prints; `objdump -d` the two functions when the summary is not
enough) and explained in the hand-in in one line each, or fixed. Legitimate reasons: a C call
became a member call of the same body; a forwarder inlined; a header-only change with no code
difference. NOT legitimate without retail's address: a call that appeared, vanished or moved; a
store whose width or displacement changed; a compare that vanished. A hand-in that says "not run"
or "judged from the source" is not merged.

## Waves L and R: your block (added after wave S merged, main `8196d1cf`)
Wave S is on main: `Serialize` / `Persist` take `IO::Stream&`; `Work` slots [4]-[7], [10] and
`Action` [9] are typed; the controller state table's handlers are typed templates on the owner.
- **Your files**: `grep -P '	<BLOCK>	' docs/runtime_port/endgame_b/b4_session/data/pass2/filemap.tsv`.
  You own exactly those, plus the headers `PASS2_FILEMAP.md` §2 gives your block as owner.
- **What blocks each cast** (a map, not the authority; the first-pass `// LEFT` reasons are often
  STALE: e.g. `GameUpdateLoop::m_game`, `m_citizenList`, `m_entityList` are typed now):
  `grep -h '<your file>' docs/runtime_port/endgame_b/b4_session/data/pass2/[A-G]_*.tsv`.
- **`LOCAL:` rows are the bulk of the work**: raw offset reads of an object whose header already
  declares the fields, and file-local helpers taking `int64_t` / `void*`. Type the local from a
  typed source, read by name, type the file-local helper's parameters. For every offset replaced by
  a name, CHECK the header's `static_assert` for that offset and the field's width / signedness
  against the read you replace. A read that matches no declared field: add the field ONLY in a
  header you own (layout from retail), else LEAVE.
- **`NOCLASS:` rows**: the record gets a struct in a header you own (new header beside its user if
  none), every field asserted, layout from retail's code that builds / walks it (address quoted in
  the header).
- **A header you do not own is READ-ONLY**, even for a one-line typing: LEAVE the read with
  `// LEFT: <header> (<owner block>)`. Exception: none. (The owner may be running right now.)
- **Cross-file C signatures** (`exe_bodies.h`, `*_funcs.h` declarations whose callers are in other
  blocks' files) belong to wave X, not to you: do not change such a declaration. Inside your files
  you may type a function that is `static` / file-local, or declared in a header you own AND
  called only from your files (Grep the whole tree first). A call from your typed code into an
  untyped outside signature keeps ONE cast in its original spelling + `// LEFT: SIG <function>`.
- **Type-erased container cores (runtime)**: the policy is typed template WRAPPERS at the call
  sites over the ONE ported erased core (retail shares those bodies). The core's own stride /
  element-byte casts are floor: `// CAST-OK: elem_stride`. Do not duplicate a core per type, do not
  swap a hand-written container for another one (first allocation, growth and order must not move).
- `// B4 FORWARD` forwarders defined in your files: delete those with NO caller left anywhere
  (Grep `code runtime standalone dx11 engine tests`); leave the rest for wave X.

## Working discipline
- Worktree; branch `b4p2-<BLOCK>`; `git log --oneline -1` must show the hash in your prompt, else
  `git reset --hard main`. Never `git stash`, amend, rebase, push; no sub-agents; never `find /`
  or any search outside the worktree; never `pacman`.
- NEVER run the game, `sa-run`, `sa-deploy`, or touch `C:/GOG Games/Banished`. The coordinator gates.
- Builds in the FOREGROUND at `-j2` only (Bash timeout 600000 ms; re-invoke when it times out; make
  is incremental). Check the build's own exit code. If the shell is killed for low memory, say so
  and stop.
- **Context**: commit per file (each commit builds: at least compile the file you changed, a full
  build every few commits); tool and build output go to files under `<scratch>` and you read only
  the tail / the errors; never paste a disassembly listing into your reasoning when a grep answers.
  When you have used most of your context, STOP at a building commit and hand in with the exact
  list of what remains — an honest partial hand-in is worth more than an unverified full one.
- Scratch: `<scratchpad>/b4p2/<BLOCK>/` only (the scratchpad path is in your prompt).
- Ghidra (retail), when you need a layout, a width or a signature: `check_connection` first;
  `program=` on EVERY call (`Application-x64.exe` or `Runtime-x64.dll`), never `switch_program`;
  read-only (rename nothing, save nothing). Ghidra's C drops arguments on indirect calls and on
  register-passthrough forwarders: read the disassembly for a signature.
- Commits `B4 P2 <BLOCK>: …` ending with the attribution lines your session gives you (else
  `Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>`). Before each: `git diff --stat`, revert
  anything outside your file set.
- Before the final build: `git merge main` if main moved (keep both sides of append-only conflicts).

## Hand-in (final message, under 60 lines)
- branch, last commit; debug / perf / unit exit codes, unit test count and failures
- metric over your files: base -> tip per column (`castmetric.py <files> --by-file --tsv`), and
  the tree total (`castmetric.py code runtime standalone dx11 engine | tail -1`) base -> tip
- what changed at the blocker (fields typed, signatures, new structs with their retail evidence)
- forwarders deleted; forwarders left with their remaining outside callers
- LEFT: file:line + reason, grouped by owner
- B11: perf and debug totals (IDENTICAL / SAME / DIFF / ONLY-*), and one line per DIFF / ONLY-*
- `behaviour:` commits with the retail address each
- `known_port_issues` rows added
- what you could NOT verify (say it plainly)
