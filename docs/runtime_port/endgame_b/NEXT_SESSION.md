# Next session plan (written 2026-09-18, at the B.3.3 close)

State and counts live in `../STATUS.md`; this file is only the plan. Delete it (or replace
it) when the session it describes has started.

## 0. Sync (5 minutes)

- `git log --oneline -8`, `git worktree list`, `git branch --no-merged main` (expect none).
- Prune any merged agent worktrees under `.claude/worktrees/` (check each is clean first;
  `git worktree remove` without `--force`, `git branch -d`) — they slow every unscoped grep.
- Greps over `standalone/` or `tests/` exclude `standalone/build*/` and `tests/unit/build/`,
  anchored on a path separator.
- Open Ghidra (all four programs) BEFORE launching an agent that needs it.

## 1. Small carry-overs from B.3.3 (each its own gated change; none blocks step 4)

1. **The IAT-cached `InternalBasePtr` callers.** `fn_InternalBasePtr_Ctor` / `_DeletePtr` are
   not at zero: ten files cache them in file-statics behind an IAT preference
   (`s_fn_IBP_Ctor = resolveIAT(0x140152a90)` -> fallback) and call through the cache —
   `placementtool`, `edittool`, `removenaturalresource`, `performwork`, `dropoffrawmaterial`,
   `work_act_perform`, `work_act_path`, `work_act_clear`, `pickup_gate`, `work_base_reimpl`.
   A small caller batch (sonnet is enough; `git show ad54c8a4` has the worked examples),
   then the two pointers die by name AND address.
2. **One `?_typeInfo@` record per name.** `Base`, `External`, `ExternalList`,
   `CompiledExternal`, `Internal` still have MINTED reflection records; `Internal` also has a
   `DEFINE_TYPE` one beside it (the chain reads 417, not 416) and the External classes do not
   take the macro. Blockers, measured at the close: ~20 by-name `RT_DATA_SLOT` readers, 2
   `CUTOVER_REQUIRED` manifest rows, 2 by-address readers, `reg_mint_promoted.cpp` promotion
   rows, and `standalone/main.cpp` reading `_firstTypeInfo` at 0x1801fc6e0 by address. Needs
   `standalone/rt_data_mint.cpp` + `tools/rt_mint_gen.py` and a live boot comparing the type
   table — so it is an owner task with the coordinator in the loop, not a sweep.
3. `ExternalList::Find`'s second parameter: `External.h` spells it `uint32_t`, retail's is
   `Object::Type` BY VALUE (mangled `VType@2@`); four exe callers pass a pointer in a dead
   R9. Name `Object::Type`, then the four `fn_ExternalList_Find` sites convert.
4. The 6C `GetTypeIdFromName` conversions in `OptionsDialog` / `NewGameDialog` have build
   coverage only on the gate routes. A menu-driver route that opens Options would close it
   (`menu-driver` skill).

## 2. B.3 step 4 — `IO` (Stream, CompressedStream, Serializer, Encoding, Package), `System` rest

The most design-heavy step left after B.3.3. Same shape that has now worked five times:
read-only inventory (sonnet) -> committed rule sheet `B34_RULES.md` -> ONE owner for the
core (opus, Ghidra open) -> coordinator gates each merge -> caller batches by directory
(each fans out sub-workers inside its own worktree) -> one closing agent.

### 2a. What B.3.3 handed over (already measured)
- 159 call sites over 9 serializer pointers, untouched on purpose: `fn_ExternalPtr_*Serialize*`,
  `fn_InternalBasePtr_Serialize`, `fn_ExternalRefArray_Serialize`. `fn_ExternalPtr_Serialize3`
  is the LAST reader of exe 0x140007140.
- Every converted `Serialize` body still goes through `RtObject::SerializerLite` and the
  RETAIL `Encoding` vtable byte offsets. Those offsets DIFFER in the profile build — never
  cross-check a Serialize body against the profile disassembly.
- `Internal::Clone` (0x1800bfb10, unported: a serialize round-trip deep copy that needs
  `IO::MemoryStream`) and `ExternalFactory::SerializeReferenceTable` as a member (its 2nd
  parameter is a file-local struct today) both land here.
- `runtime/src/io/file_stream.cpp:393` `NoteSlotDispatch` — a vptr-identity attendance
  counter — dies with `IO::BinaryFileStream`'s class; do not leave it counting zero.
- ~25 files use setjmp/longjmp or SEH to cross retail frames (ENDGAME decision 7); the IO
  ones are decided in this step's rule sheet.

### 2b. Questions the owner answers first (into `B34_RULES.md`, before any code)
1. The `Encoding` hierarchy's real slot list from the RETAIL DLL, and how a real class keeps
   retail's offsets while `SerializerLite` callers still exist (coexistence, as B33 §4 did
   for Object — prove it in `tests/unit/` first).
2. What replaces `SerializerLite`, in what order bodies move onto it, and what the resave
   gate must show at each move (byte-identical to the golden — this gate is the step's whole
   safety net; read `tools/resave_gate/README.md` incl. the loader-race flake signature).
3. Every vptr-identity read and every minted vtable in `runtime/src/io/`, with the decision
   per site (B33 §5/§6.1: check `rt_data_mint_tables.inc` for `RT_MINT_UNNAMED` slots filled
   from a row before retiring it).
4. The exception/longjmp sites in IO: which become C++ exceptions, which stay, and why.

### 2c. Standing rules every prompt carries (learned this session)
- Rule-sheet counts are TOKENS until proven otherwise: 609 -> 380, 1,190 -> 815, 106 -> 30.
  Re-measure call sites with a comment/string-aware lexer before sizing a batch.
- A ctor/dtor door does NOT convert if the member form would change which vtable a live
  object wears (minted vs compiler) — it waits for the receiver's class.
- Guards on a pointer a batch stops calling STAY until the closing pass removes guard and
  pointer together. A call inside a printf/fprintf argument list does not convert (the perf
  build strips printf arguments).
- Includes go at global scope, above any `namespace X {` forward decl, and NEVER inside an
  `#if` block. Agents build `standalone-debug`, `standalone`, `standalone-perf` AND `unit`.
- Deleting a statement inside a multi-line `#define` can eat the previous line's `\`.
- Sub-workers report to the coordinator too; the owning agent integrates. Verify the final
  diff (removed-line scan), not the reports.
- `make sa-stress` is a functional check while B is open — record the number, never chase it
  (B33_RULES §10). A resave DIFF with count 5689 and ONE extra row is the loader-race flake:
  re-run twice; 5690+ or several rows is real. The gate script tests the DEPLOYED exe.

### 2d. The gate for every merge (coordinator only)
debug + release + perf build exit 0 · `make unit` · `sa-run` + pause-menu quit ·
`BANISHED_AUTOTUTORIAL=1` with `BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120` · the Load
Game dialog route · resave gate PASS (x3 for anything touching IO) · live-boot type table vs
the previous boot · `[RtResolve] 0 misses`, no `[RtMint] TRAP` · `dispatch_slot_map.py
--check` 0 · 0 GATE WARN · `git diff --stat` against the agent's file set before merging ·
at a step close also the Mods route with `.pkm` mounted (move them back) and one functional
`make sa-stress`.
