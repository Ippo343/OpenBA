# Next session plan (written 2026-10-05, end of B5 — the dx11 seam)

**B5 is CLOSED.** The twelve `VideoDX11::*Data` types are real `final` subclasses of the twelve
`Video::*Data` bases (`virtual ~XData()`, compiler vtables, `new` / `delete` through the roots' own
operators); the hand-table seam (`dx11/funcs/dx11_seam.*`, `DX11_REGISTER_SLOT`, `Dx11SeamInstall`) is
gone; `dx11/` counts **0** casts; the PROD metric is **89** = 63 retail-difference + 23 named prerequisite +
3 ruled + **0 work**; acceptance has ONE line off (10 = 21, the accepted template-field miscount) and line
01 is back in the PROD-total form. Eight gated merges, 0 problems, every one with a bounded device-lost
run. Numbers: `b5_session/CLOSE_NUMBERS.md`; rule sheet `B5_RULES.md`; gate table, inventories, audit and
the device-lost finding in `b5_session/`. No class in the tree declares `DeletingDtor` any more.

## 0. Scope, as the user set it (unchanged)

The DANGEROUS casts go completely; only safe casts stay. The number is
`python docs/runtime_port/endgame_b/b4_session/castmetric.py code runtime standalone dx11 engine` (89).
The differences from retail are a SEPARATE task (`docs/known_port_issues.md` 13 and 14, rows to 890). A
raw-storage member becomes a real member only in a `behaviour:` commit that reproduces retail's constructor
AND destructor; any other difference found is recorded, not fixed. Device-lost stays DEFERRED (§3).

## 1. Where it stands

| | |
|---|---|
| main | `020a4e3ae` (Merge b5-CLOSE, the last gated code commit) + the close documents |
| metric | 336 -> **89**: dx11 240 -> 0; code 74, runtime 15, standalone 0, engine 0 |
| invariants | new game 5683 / 0, tutorial 5773 / 12, typetable `b983f144`, resave 5/5 on every merge, 0 new warnings, unit **920 / 0** |
| acceptance | 1 line off (10 = 21, accepted); 01 = 89 of target 89 (PROD-total form); 02-09, 11, 12 at 0 |
| floor | `dx11_seam` DELETED; `gpu_map` 5, `com_iid` 3, `win32` 88, `byte_io` 42 (measured; FLOOR.md) |
| close checks | `worldcmp.sh` vs `oldbase` identical (118 lines); `stepclose.sh` (CLOSE_NUMBERS §6); audit `b5_session/AUDIT.md` |
| worktrees | only `oldbase` (`b2a5434c`) remains, for the next world comparison |

## 2. The next step: the retail-difference task (`known_port_issues.md` 13-14)

**What it is.** 63 of the 89 remaining casts sit on lines whose tag says "retail-difference task": each is
a place where the tree does something retail does not (a guard retail lacks, a wrong overload or object,
a store that normalises, a construction / destruction / leak difference — the §14a-14e lists) and the cast
is the SYMPTOM. Fixing the difference removes the cast; the fix is a `behaviour:` commit per row with the
retail evidence (address, the instructions read) in the message, gated with resave x5 (every §14b row
re-cuts the golden). This is the first step of ENDGAME B that changes what RUNS on purpose, row by row.

**Ready-made rows (NEXT_SESSION of B4 S4, still open):** 861 (`GatherRawMaterials::Execute` hands the
WorkPlaceComponent as the Time: one line, removes the cast), 820 (`_entityFlags` through the uint
overload: one line), 860 (the mesh-group type check), 810, 850, 880; the tree-only deviations in
`b4_session/CLOSE_NUMBERS.md` §5 and the third session's (792, 752, 754, 122's note); 13 (`TradeWork::Validate`
walks a stale entry).

**Shape (the one that worked eight times):**
1. **Block 0, read-only inventory** (sonnet): one row per retail-difference cast (`leftfree_c.py` lists the
   63 with their tags; `LEFT_FREE.md` §C) joined to its known_port_issues row: the retail address, what the
   tree does, what retail does, which §14 bucket, whether a save-format change is involved (14b), and the
   OTHER rows of 13-14 that have NO cast (they are the same task; count them). Verdict per row: one-line
   fix / needs a retail re-read / save-format.
2. **One opus question pass into `B6_RULES.md`:** what a `behaviour:` commit must carry (address, the
   disassembly lines, the before / after observable), the order (no-save-format rows first; 14b rows last,
   each alone with a golden re-cut in its own commit — `tools/resave_gate/README.md`), the rule for a guard
   retail lacks (drop it and let the crash show the upstream null: CLAUDE.md "never fix a faithful crash
   with a null check"), and which rows are the user's to decide (anything that changes a save).
3. **Blocks by bucket, not by file** (opus, worktrees, `-j2`, at most four): 14a wrong callee / object /
   overload; 14c construction / destruction / leaks (one `behaviour:` commit per class, ctor AND dtor);
   14d guards; 14e stores; then 14b save-format rows serially by the coordinator. Every block hands in the
   retail evidence per row; the coordinator reads it before the merge (feedback: cast-removal agents change
   behaviour — here the behaviour change is the point, so the evidence is the gate).
4. Gate per merge as B5 (`b5_session/mergegate.sh` copied to `b6_session/`; `devlost.sh` only on merges
   that touch video objects), `worldcmp.sh` vs `oldbase` at the close (several rows may legitimately change
   the generated world — then `oldbase` moves to the B6 close commit, by decision, recorded).
5. Acceptance: line 01's target falls with each fixed row (63 -> 0); the 23 named-prerequisite and 3 ruled
   stay until §4's inline-release pass.

**Before any block:** read `known_port_issues.md` §14g ("commit messages that state something nobody
verified") — the standard for a `behaviour:` message in this step.

## 3. Device-lost (DEFERRED by the user — read before touching it)

`b5_session/DEVLOST_FINDING.md` and known_port_issues row 890: the harness injects ONCE; the per-frame
loop is real repeated removals of a genuinely recreated device (`GetDeviceRemovedReason` = 0x887a0005, no
sub-cause, printed since B5). Leading hypothesis: the exe-side PORT GAP pair `GameUpdateLoop::OnVideoError /
OnVideoRestart` (retail 0x140095970 / 0x140095930, `gameupdateloop.cpp:2345-2352`) leaves game-owned
buffers on the dead device. Porting that pair is the one change that would BOUND the route by being
correct — it is an exe `behaviour:` commit in the deferred area; do it only when the user lifts the deferral.
Until then `devlost.sh` keeps `DLMAX=5` and its pass criterion (INJECT=1, COMPLETE>=1, crash 0).

## 4. After that

- **The inline-release pass** (unblocks 13 of the 23 named-prerequisite casts): make `System::String`'s
  ctor / copy / release and `ArrayDtorImpl` inlinable so they compile to retail's open-coded form, then
  type `documenttext.cpp:155-159`, `modification.cpp:272-287`, `avl_trees.cpp:268`, `introsort.cpp:355`,
  `wave24_lane_gfxmat.cpp:363`. B11 against retail's shape.
- The other named prerequisites: the ctor doors (`System::Random`, `Engine`, `Renderer` / `Scene` in
  `minimap_paint.cpp`), the Frustum / Vector4 zeroing-ctor locals, `set_clear.cpp:210`, `goals_serialize.cpp:86`,
  the two harness puns (retire or keep listed).
- The quality pass: 234 free functions that are members of a class in retail (`LEFT_FREE.md` A).
- ENDGAME B.1 decision 7 (exceptions), the PORT GAPs (189; the device-lost pair among them), then device-lost.

## 5. Working rules that held (do not relax) and what B5 added

- Agents never run the game; builds `-j2`, at most four building agents; the coordinator alone gates; the
  gate builds the MERGED tree before the merge commit exists; a fresh worktree is reset to a NAMED commit.
- No null guard added or dropped; a class local with a zeroing ctor adds stores; retail quirks untouched.
- NEW: **commit the gate table (and anything else) BEFORE `mergegate.sh`** — it refuses a dirty tree and
  the refusal is easy to grep away (row 5 was lost once to that).
- NEW: **a stopped background task does not stop the script it launched**: a long close runs with
  `nohup bash … &` from a short foreground call; never chain it after a game run in one background task
  (the 10-minute cap cut one, orphaning an exe and a lock; the orphan was killed BY PID).
- NEW: when a base slot's parameter is `void*`, the CALLER's declared type or retail's mangling of the
  owner-side member decides it (`Resolve`'s `PEAV12@` = `RenderBuffer*`; `Serialize`'s `AEAVSerializer@IO@@`);
  a dx11 body that ignores a parameter still declares it.
- NEW: a shared body registered in two tables becomes ONE static helper called from two overrides (Lock /
  Unlock); a retail `jmp` to another slot becomes a one-line override that calls it.
- NEW: converting a class's callers of `DeletingDtor(1)` to `delete` belongs to the SAME commit as its flip
  (the base loses the member; the other classes' callers stay until theirs).
- NEW: the metric mis-resolves a same-named member (`_data`, `deviceData`) through another class: a typed
  local (`Base* const b = x->_data; return static_cast<Derived*>(b);`) is both honest and counted right.
- Never `git add` the session directory wholesale: `runs/` and the `gate_*.out` / merge logs are untracked.
