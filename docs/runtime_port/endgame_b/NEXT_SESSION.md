# Next session plan (written 2026-10-05, end of the third B.4 session)

B.4 (the cast metric) is NOT closed, but it is close: this session took the in-scope count from 811 to 149,
made about 120 more classes real, deleted every POD mirror and every bridge header, and brought B.4's own
acceptance script from 9 lines off target to 6 (3 of the 6 are entirely `dx11/`, the next step's). The session
was stopped by the harness for LOW SYSTEM MEMORY during the last merge's gate. This file says exactly where it
stands, what finishes B.4 (§2), and what the step after it is (§3, the dx11 seam). Earlier plans:
`git show e4e5a78a2:docs/runtime_port/endgame_b/NEXT_SESSION.md` (the plan this session executed).

## 0. Scope, as the user set it (unchanged)

**The DANGEROUS casts go completely; only safe casts stay.** The number is
`python docs/runtime_port/endgame_b/b4_session/castmetric.py code runtime standalone dx11 engine`.
The differences from retail are a SEPARATE task (`docs/known_port_issues.md` entries 13 and 14; rows now run to
806). A raw-storage member becomes a real member only in a `behaviour:` commit that reproduces retail's
constructor AND destructor; any other difference found is recorded, not fixed.

## 1. Where it stands

| | |
|---|---|
| main | `18edaf29b` (docs) on top of `12b982db8`, the last GATED code commit; `4f8da1d44` adds a one-line parameter rename + floor marker that was committed WITHOUT a build |
| metric | **1,051 -> 389** this session; 240 of it is `dx11/`, so **811 -> 149 in scope** |
| gated merges this session | 37 (`b4_session/GATE_TABLE.md` rows 122-158), 0 real problems (row 144: a standing warning's wording followed a rename; row 140: an agent killed the compilers during a gate, covered by row 141) |
| invariants | new game 5683 / 0, tutorial 5773 / 12, typetable `b983f144`, resave 5/5 on every merge, 0 new warnings; unit 918 -> 914 (four tests of deleted PODs / the minted Thread table were deleted with them) |
| close checks done | `worldcmp.sh` against the pre-B.4 exe `b2a5434c`, seed 12345: all 118 pointer-free world lines identical; `devlost.sh` alone: exit 0, round trip COMPLETE; the Mods route with two `.pkm` mounted: clean |
| close checks NOT done | `stepclose.sh` (true release compile, clean rebuild, the route set), the quiet `sa-stress`, one manual run of the removal tool's CalcPath (it needs the toolbar's Remove button: no unattended route), `CLOSE_NUMBERS.md` |
| not merged | branch `b4fin-VOID` (`eed39411e`): see §2.1 |

Acceptance (`bash docs/runtime_port/endgame_b/b4_session/acceptance.sh`, read-only, under a minute) at main:

| line | what | now | note |
|---|---|---:|---|
| 01 | unmarked casts, PROD | 389 | 240 `dx11/` (next step); 149 in scope: `LEFT_FREE.md` §C lists every one by reason |
| 02 / 03 | floor ceilings / BAD + STRAY markers | 0 / 0 | pass |
| 04 / 05 / 06 | offset accessors (defs / calls), pun | 5 / 17 / 1 | ALL in `dx11/src/{contextdata,devicedata,imagebufferdata}.cpp`: the next step's |
| 07 | bridge-marker lines | 69 | 52 `dx11/`; 17 live ones in `code/` + `runtime/` (each still has its cast) |
| 08 / 09 | free functions / `void*`-first functions listed in `LEFT_FREE.md` | 0 / 0 | pass mechanically — read §2.4 before believing it |
| 10 | fields with no `offsetof` assert | 21 | 19 template-class fields, 1 protected (`InternalBasePtr::_ptr`), 1 the measure's own miscount (`Device onLost`) |
| 11 | `Rt<Ns>::` POD mirrors beside a real class | 0 | pass: every mirror, `pod_bridge.h` and `coll_bridge.h` (on the unmerged branch) are gone |
| 12 | no doors (`b3r_session/acceptance.sh`) | 0 | pass |

What the 149 in-scope casts are (`LEFT_FREE.md` §C, generated from the tags on the lines):
60 DEFERRED to the retail-difference task · 5 dx11 seam · 10 DEFERRED on a named prerequisite · 18 floor
candidates awaiting a ruling · 56 WORK with the blocker written on the line (6 signatures, 4 H8 puns where a
typed spelling changes codegen, 46 others).

How the session ran (all in `b4_session/`): `WAVEY_BRIEF.md` (the executing agents' brief), `AUDIT.md` +
`data/audit/lines.tsv` (the independent audit: one ruling per class; ALL accepted, the tool rulings are in
`castmetric.py`), `FLOOR.md` (re-cut with measured ceilings; four new categories; the `elem_stride`
re-definition; the block-comment marker for macro lines), `LEFT_FREE.md`, `GATE_TABLE.md`, `worldcmp.sh`.

## 2. Finishing B.4 (do this first)

### 2.1 Recover, then merge what is waiting
1. Free memory first (the harness killed the last gate for low system memory while only ONE `-j4` gate build
   was running: check what else holds RAM — Ghidra was up all session). `git status` clean.
2. There are about 40 agent worktrees under `.claude/worktrees/` and about 170 local `b4*` branches. Every one
   listed in the gate table as merged can go (`git worktree remove`, `git branch -d`); keep `b4fin-VOID` and
   the worktree `oldbase` (the pre-B.4 exe for `worldcmp.sh`) until the close.
3. `mingw32-make clean && mingw32-make -j4 standalone-debug`, then one gate on main
   (`bash docs/runtime_port/endgame_b/b4_session/gate.sh s4_base 5`): this also builds `4f8da1d44`'s rename.
4. `b4fin-VOID`: merge it SQUASHED (its first commit `6f1c444e0` swept in a stale index, `026a21012` reverses
   it; the net diff against main was verified to be only the intended 80 files):
   `git merge --squash b4fin-VOID`, `mingw32-make -j4 standalone-debug`, commit, `gate.sh`, `uiroutes.sh`,
   `toolroutes.sh`. It types 27 `void*`-first functions, deletes 15 forwarders, tags 10 raw-address probes,
   leaves 10 with a reason, and deletes `code/Game/Component/coll_bridge.h` (metric 389 -> 386).
5. Regenerate `LEFT_FREE.md` after it (sections A / B by the script in the document's header comment; section C
   by `castmetric.py … --detail` grouped by tag).

### 2.2 Rulings the coordinator owes (cheap; no agent)
- The 18 floor candidates of `LEFT_FREE.md` §C and the 10 raw-address probe signatures: rule each (`CAST-OK:`
  with its category, or WORK). `wave9_lane_gfxd.cpp:580` (a stack byte image viewed as a record to skip its
  ctor) and `save/closure_batch2b.cpp:113` are NOT floor by the audit: they are work.
- Acceptance line 01's target is `0` over PROD including `dx11/` and the retail-difference rows: it cannot be
  met by B.4 alone. Decide the closing form — recommended: line 01 = "every unmarked cast outside `dx11/` is
  listed in `LEFT_FREE.md` §C under DEFERRED (retail-difference task / dx11 seam / named prerequisite)", i.e.
  the WORK and floor-candidate groups are empty.
- Line 10's remainder (21) is templates / one protected member / a miscount: amend the measure (skip template
  bodies and function-pointer parameters) or accept it in `CLOSE_NUMBERS.md`.

### 2.3 The work that is left (about 56 casts + 17 live bridge lines)
Every line carries its blocker; `LEFT_FREE.md` §C is the list. The groups that are real units:
- `ResolveExternalByName_Impl` (three record types; needs the const ruling or a template over the record).
- CitizenBehavior's 16 InternalBasePtr members + the bucket list as `InternalPtr<T>` (about 170 reader lines in
  49 files) — removes `citizenlist_serialize.cpp:208`.
- `InteractNodePosition`'s `float*` out-parameter (46 callers) — removes `CitizenComponent::Initialize`'s cast.
- `Graphics::Geometry`'s +0x84 / +0xa0 InterpolatedState pair (about 40 runtime reader lines) — removes
  `statusicon_geometry.cpp` and the geometry_draw line.
- `SetLight` (the SunLight fields as ctor-less 3-float PODs), `ParticleBoundsRecord` ctor 0x180103bf0,
  `documenttext.cpp:155-159`, `dialog_create.cpp:316` (`_root`: an InternalPtr member ADDS a store at +0x50,
  B11 caught it — needs a no-store member type), `imagebuffer_platform_serialize.cpp:337`,
  `modification.cpp:272-287`, `SortByKey70` (the unit test drives it with synthetic records).
- The 4 H8 lines (`game_settings_persist`, `eventlogui`, `consumeproduce:195`, `componententity:531`): each
  was TRIED and a typed spelling changed codegen; they need retail's field types read, then `byte_io` by
  FLOOR §5 or a retype.
- The dead-argument rows (3), the two harness puns: need retail's callers read or a harness decision.
Run them as before: one agent per disjoint file set, `WAVEY_BRIEF.md`, the coordinator alone gates.

### 2.4 What passes acceptance but is not finished (say it in the close)
- **234 of the 408 remaining free functions are members of a class in retail, still spelled as C functions**
  (`LEFT_FREE.md` §A, reason `member of <Class>, not moved`). Rule §6.4 only asks that they be listed with a
  reason; moving them is real work nobody did. Decide whether that is B.4 or a later quality pass.
- After `b4fin-VOID`, 20 `void*`-first functions outside the allowed headers remain (10 probes, 10 with a
  reason). Line 09 passes because the names are listed, not because the rule's second sentence holds.
- Tree-only deviations ADDED this session and recorded, to be read by the retail-difference task:
  row 792 (the dying-entity audit also re-fires `ParticleComponent::OnRemove`: the tree drains removals one
  frame late), row 122's note (MenuDialogLoopT builds its Scene member before seeding `_random`), the
  copy-ctor dead stores of `Mod` and `MeshInstance` (rows 752, gate row 144), row 754 (WeatherSimulation).
- New teardown behaviour that only the gate has exercised (no long play session): `~ActionStack` deleting
  live Actions (row 790), `~ParticleComponent` deleting emitters (791), about fifteen arrays that leaked and
  are now freed as retail (rows 722, 760, 770-775, 780, 805, 806).

### 2.5 The close
`stepclose.sh` (routes, true release compile + `make clean`, clean rebuild + gate), a quiet `sa-stress`
(record, never chase), `devlost.sh` once more on the final tree, `worldcmp.sh` once more, the removal tool's
CalcPath BY HAND (the user: toolbar Remove button on a new game), `CLOSE_NUMBERS.md`, the STATUS block.

## 3. The step after B.4: the dx11 seam

What it is: `dx11/` still talks to the runtime's `Video::` classes through reinterpretation. It holds
**240 counted casts** (line 01), **5 offset-accessor definitions and 17 calls** (`Field`, `FieldPtr`, `IbI32`,
`IbU32`: lines 04 / 05), **1 address pun** (06), **52 bridge-marker lines** (07), and the `dx11_seam` floor
category (20 marked lines: `CompanionData.h`'s 12 `DeletingDtor` arms, `Context.h`'s two inline
`SetRenderBuffer(const void*)` / `SetVertexBuffer` forwarders, `video_interface.cpp`). In scope with it: the 5
`standalone/` swap-chain lines (`main.cpp:880 / :952` `+0x18` reads, `boot_window.cpp:49`), and
`Video::Device::GraphicsInterface` / `Adapter`, which `dx11/src/devicedata.cpp` builds and grows BY HAND through
the `RtVideo_*` doors (B.4 left them for this reason).

The decisions already taken (do not re-litigate): the twelve `Video::*Data` classes stayed on `DeletingDtor`
BY DECISION at the normal-virtual cutover; they flip to `virtual ~T()` here; dx11's companion types become
SUBCLASSES of `Video::*Data`; `DeletingDtor` and `Dx9Placeholder` leave the tree; the `dx11_seam` category
ends at 0 and is deleted from `FLOOR.md` / `castmetric.py`.

Shape (the one that worked every time):
1. **Block 0, read-only inventory** (sonnet): every cast / accessor / bridge line of `dx11/` keyed by the
   `Video::` class it reinterprets; every `Video::*Data` class with its retail ctor / dtor
   (`VideoDX11-x64.dll` is loaded in Ghidra: `program="VideoDX11-x64.dll"`) and vtable slot count; every
   place `runtime/` or `standalone/` reaches INTO dx11 state by offset.
2. **One opus question pass** into a rule sheet `endgame_b/B5_RULES.md`: the subclass shape per companion
   type, the virtual-dtor flip (retail slot N is compiler slot N+1: `reference_virtual_dtor_cutover_rules`),
   who owns `GraphicsInterface` / `Adapter` construction, how COM pointers are typed (`ID3D11X*` forward
   declarations remove 36 casts by themselves — FLOOR §5).
3. **Blocks by class hierarchy** (whole hierarchies, disjoint files), opus, worktrees, `WAVEY_BRIEF.md`'s
   discipline; the coordinator alone gates. **`devlost.sh` is part of EVERY gate of this step** (it rebuilds
   every video object), and rendering is verified with `tools/trace_oracle/` (skill `camera-pin`), never a
   pixel diff.
4. Independent audit, acceptance (lines 01, 04-07 reach their targets only now), step close.

## 4. After that
The retail-difference task (`known_port_issues.md` 13-14: about 60 casts wait on it, plus the deviations of
§2.4), ENDGAME B.1 decision 7 (exceptions), the PORT GAPs, and only then device-lost (still DEFERRED).

## 5. Working rules that held (do not relax) and what this session added
- Agents never run the game; builds at `-j2`; at most four building agents; the coordinator alone gates;
  the gate builds the MERGED tree before the merge commit exists; B11 at -O2 and -O0 with every DIFF read.
- No null guard added or dropped; a class local with a zeroing ctor ADDS stores (use a ctor-less POD).
- NEW: an agent must NEVER kill processes by image name (`taskkill /IM cc1plus.exe` killed every build on the
  machine and interrupted a gate: gate row 140). It is in every brief since.
- NEW: a fresh worktree can come up at an OLD commit or with a stale index: every brief now says "check
  `git log -1`, else `git reset --hard main`", and a hand-in whose first commit touches unexpected files is
  merged squashed after `git diff --stat main <branch>` is read (row for `b4fin-VOID`).
- NEW: the inventory by tag goes stale within two rounds; the AUDIT (one read of every line, one ruling per
  class) is what turned "about 220 ruling-only casts" into markers, a grown tier-I shape and a work list.
- NEW: comment-only and assert-only passes are safe to run on sonnet with a B11 "all IDENTICAL" proof.
- Never `git add` the session directory wholesale: `runs/` and the `gate_*.out` / merge logs are untracked.
