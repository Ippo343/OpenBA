# Next session plan (written 2026-10-05, end of the B.4 finishing session, "S4")

B.4 (the cast metric) is done as far as B.4 can take it: **336** counted casts, 240 of them `dx11/` and the
96 outside it all filed under a deferred or ruled group with **0 open work** (`b4_session/LEFT_FREE.md`
section C; `python docs/runtime_port/endgame_b/b4_session/leftfree_c.py --check`). Three close checks are
owed (§1). The next step is the **dx11 seam** (§2). Numbers: `b4_session/CLOSE_NUMBERS.md`. The plan this
session executed: `git show b6d3fdf62:docs/runtime_port/endgame_b/NEXT_SESSION.md`.

## 0. Scope, as the user set it (unchanged)

**The DANGEROUS casts go completely; only safe casts stay.** The number is
`python docs/runtime_port/endgame_b/b4_session/castmetric.py code runtime standalone dx11 engine`.
The differences from retail are a SEPARATE task (`docs/known_port_issues.md` entries 13 and 14; rows now run
to 890). A raw-storage member becomes a real member only in a `behaviour:` commit that reproduces retail's
constructor AND destructor; any other difference found is recorded, not fixed.

## 1. Where it stands, and what is owed before B.4 is called closed

| | |
|---|---|
| main | the close documents on top of `9903a0f1f` (the last GATED code commit: `stepclose.sh` clean on it) |
| metric | 389 -> **336** this session; outside `dx11/` 149 -> **96** = 63 retail-difference + 23 named prerequisite + 7 dx11 seam + 3 ruled to stay listed + 0 work |
| gated merges this session | 11 (`b4_session/GATE_TABLE.md` rows 159-169), 0 problems |
| invariants | new game 5683 / 0, tutorial 5773 / 12, typetable `b983f144`, resave 5/5 on every merge, 0 new warnings, unit 914 / 0 |
| acceptance | 5 lines off: 04 / 05 / 06 / 07 are `dx11/`'s (07 also counts 6 DEFERRED casts that still carry their marker), 10 is the accepted template-field miscount (21). Line 01 is in its CLOSING FORM (casts outside `dx11/` in no DEFERRED / RULED group = 0) |

Owed (cheap; do them first):
1. **The removal tool's CalcPath BY HAND — the user**: new game, toolbar Remove button, drag over a tree and
   a building. The harness route skips CalcPath (`removalDesc(+0x38)` null). Nothing else exercises it.
2. `devlost.sh` once on the final tree. It passed at `d04813af` and `b32b283f`; two merges (RT2, LAST: no
   video code) came after. **Read §3 first: the script is capped now (`DLMAX`, default 5 round trips).**
3. `worldcmp.sh .claude/worktrees/oldbase/standalone/build/reimpl-x64.exe standalone/build/reimpl-x64.exe <label>`
   once on the final tree (identical at `3e57f2ee` and `b32b283f`). After it the `oldbase` worktree and the
   ten `b4s4-*` branches / agent worktrees can go (all merged: `git worktree remove`, `git branch -d`;
   `b4y1-CW`, `b4y1-GAMEA`, `b4y1-GAMEB`, `defects-open-5` are older unmerged branches: not this step's).
4. Then write "CLOSED" on the STATUS block and in the memory file.

## 2. The next step: the dx11 seam

What it is: `dx11/` still talks to the runtime's `Video::` classes through reinterpretation. It holds
**240 counted casts**, **5 offset-accessor definitions and 17 calls** (`Field`, `FieldPtr`, `IbI32`, `IbU32`:
acceptance 04 / 05), **1 address pun** (06), **51 bridge-marker lines** (07), and the `dx11_seam` floor
category (20 marked lines: `CompanionData.h`'s 12 `DeletingDtor` arms, `Context.h`'s two inline
`SetRenderBuffer(const void*)` / `SetVertexBuffer` forwarders, `video_interface.cpp`). In scope with it, the 7
casts `leftfree_c.py` files under "dx11 seam": `standalone/main.cpp:879 / :951` (the swap chain read at
`+0x18` of the DX11 DeviceData subclass), `standalone/boot_window.cpp:49`, and
`code/Game/Dialog/optionsdialog.cpp:492 / :520` (`Device::Adapter::name` / `GraphicsInterface::name` are
`void*` String storage because `dx11/src/devicedata.cpp:1548-1590` builds and grows those records BY HAND
through the `RtVideo_*` Ctor / Dtor doors; a String member today would release the name twice).

Decisions already taken (do not re-litigate): the twelve `Video::*Data` classes stayed on `DeletingDtor` BY
DECISION at the normal-virtual cutover; they flip to `virtual ~T()` here; dx11's companion types become
SUBCLASSES of `Video::*Data`; `DeletingDtor` and `Dx9Placeholder` leave the tree; the `dx11_seam` category
ends at 0 and is deleted from `FLOOR.md` / `castmetric.py`; acceptance line 01 goes back to the PROD-total
form (edit `acceptance.sh`: the S4 comment says where).

Shape (the one that worked every time):
1. **Block 0, read-only inventory** (sonnet): every cast / accessor / bridge line of `dx11/` keyed by the
   `Video::` class it reinterprets; every `Video::*Data` class with its retail ctor / dtor
   (`program="VideoDX11-x64.dll"` in Ghidra) and vtable slot count; every place `runtime/` or `standalone/`
   reaches INTO dx11 state by offset.
2. **One opus question pass** into a rule sheet `endgame_b/B5_RULES.md`: the subclass shape per companion
   type, the virtual-dtor flip (retail slot N is compiler slot N+1: memory `reference_virtual_dtor_cutover_rules`),
   who owns `GraphicsInterface` / `Adapter` construction, how COM pointers are typed (`ID3D11X*` forward
   declarations remove 36 casts by themselves — FLOOR §5), and the device-lost question of §3.
3. **Blocks by class hierarchy** (whole hierarchies, disjoint files), opus, worktrees; the coordinator alone
   gates. `devlost.sh` is part of EVERY gate of this step (it rebuilds every video object); rendering is
   verified with `tools/trace_oracle/` (skill `camera-pin`), never a pixel diff.
4. Independent audit, acceptance (lines 01, 04-07 reach their targets only now), step close.

## 3. Read before running `devlost.sh`: the 20 GB finding (known_port_issues row 890)

After the ONE injected `DXGI_ERROR_DEVICE_RESET`, every later Present reports hr 0x887a0005 and the exe runs
a complete device-lost round trip PER FRAME, about 100 MB of working set each: 207-219 round trips and
20.8 GB by the 600 s kill. That — not Ghidra — is what stopped the third session during its last gate and
stopped the S4 close script (the harness reaps background shells when system memory is critical; the orphaned
`reimpl-x64.exe` must then be killed BY PID and the game-dir lock released).
- `devlost.sh` now ends the run after `DLMAX` (default 5) logged round trips; `killed=YES` is then expected.
  `finishmerge.sh`'s `devlost` arm (`DLFRAME=40000 DLPAUSE=48000`) uses the same script.
- For the dx11 step this is the FIRST thing to understand, before any block edits video objects: why the
  recreated device still reports removed (the injection flag never cleared? the swap chain not rebuilt?),
  and which objects a round trip does not release. It is in the DEFERRED device-lost area, so: find and
  record; fix only what the seam work itself touches, in a `behaviour:` commit against `VideoDX11-x64.dll`.
- The other 20 GB process at the start of S4 was VS Code's `cpptools` indexing 40 agent worktrees: remove
  merged worktrees as you go, and check `Get-Process | sort WS` before blaming a build.

## 4. After that

- **The retail-difference task** (`known_port_issues.md` 13-14): 63 casts wait on it. New this session:
  row 861 (`GatherRawMaterials::Execute` hands the WorkPlaceComponent as the Time: a one-line `behaviour:`
  fix that also removes the cast), row 820 (`_entityFlags` through the uint overload: one line, removes the
  cast), row 860 (the mesh-group type check), rows 810, 850, 880. Plus the tree-only deviations listed in
  `CLOSE_NUMBERS.md` §5 and the third session's (rows 792, 752, 754, 122's note).
- **The inline-release pass** (unblocks 13 of the 23 named-prerequisite casts in one go): make
  `System::String`'s ctor / copy / release and `ArrayDtorImpl` inlinable so they compile to retail's
  open-coded form (dec refcount, `MemFree` 0x1800c7ad0, zero), then type `documenttext.cpp:155-159`,
  `modification.cpp:272-287`, `avl_trees.cpp:268`, `introsort.cpp:355`, `wave24_lane_gfxmat.cpp:363`. It
  touches every String user's codegen: B11 against retail's shape, not against the tree's.
- The other named prerequisites: the ctor doors (`System::Random`, `Engine`, `Renderer` / `Scene` in
  `minimap_paint.cpp`), the Frustum / Vector4 zeroing-ctor locals, `set_clear.cpp:210` (the SettingsMap
  member), `goals_serialize.cpp:86`, and the two harness puns (decide: retire `force_work` / the ClearPlace
  probe, or keep them listed).
- The quality pass nobody did: 234 free functions that are members of a class in retail (`LEFT_FREE.md` A;
  sections A / B were not regenerated in S4).
- ENDGAME B.1 decision 7 (exceptions), the PORT GAPs, and only then device-lost (still DEFERRED).

## 5. Working rules that held (do not relax) and what this session added

- Agents never run the game; builds at `-j2`; at most four building agents; the coordinator alone gates;
  the gate builds the MERGED tree before the merge commit exists; B11 at -O2 and -O0 with every DIFF read.
- No null guard added or dropped; a class local with a zeroing ctor ADDS stores (use a ctor-less POD).
- An agent never kills processes by image name; a fresh worktree is reset to the named base commit first.
- NEW: `ccache` makes a clean rebuild take about a minute: the "40 minute base build" of older briefs is
  gone, and a full merge gate is about 12 minutes (25 with `ui tools`).
- NEW: base every agent on a NAMED commit (`git reset --hard <hash>` in its first step) and merge finished
  branches while the others run; three rounds of 2-4 agents with merges in between produced two trivial
  conflicts (a comment both sides touched; a deletion against a tag edit). Resolve by hunk, never
  `--theirs` on a whole file another branch also edited, and BUILD before `finishmerge.sh`.
- NEW: a one-page session brief in the scratchpad (start commit, the rules that cost a re-run, hand-in) plus
  a prompt that lists the agent's lines WITH the previous agent's measured finding got precise hand-ins;
  "measure only" units (count the void* callers, list the incomplete T's) made the next round's units small.
- NEW: at -O0, calls to small inline typed accessors with no new store to the object are the accepted
  effect of typing (ruled for `InternalPtr`); anything else at -O0 is read like an -O2 diff.
- NEW: a marker proposal is not a marker: `castmetric.py --floor` rejected three (`this` / `self`
  operands). Run it before raising a ceiling.
- NEW: write files that contain backslashes with the Write / Edit tool, never a heredoc (it bit again).
- Never `git add` the session directory wholesale: `runs/` and the `gate_*.out` / merge logs are untracked.
