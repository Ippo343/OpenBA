# B7 independent audit (2026-10-09)

Scope: the non-merge commits `b4aeac7c0..455db4163` (main at the RT merge). Read-only: no build,
no run, nothing written to Ghidra. Every `behaviour:` commit (42) and every `B7:` commit (25) was
read in full with `git show`. Of the 25 `B7:` commits, 16 change code, 4 are block doc commits
(audited below), and 5 are the coordinator's doc / gate-table commits, confirmed doc-only and not
given a row: `13c5c2f2d`, `3503095f0`, `f31221ae6` (docs, two session `.sh` scripts, the golden
md5, the resave README: no code), `f76d71ea6`, `b29b329a0`. `d9da93563` (ENDGAME decision 7) is
neither title and is doc-only. The eight merge commits were not audited, except where a finding
comes from what a merge did to `known_port_issues.md` (F6).

Retail was checked in Ghidra with `program="Application-x64.exe"` / `"Runtime-x64.dll"` passed
explicitly, never `switch_program`.

Evidence column legend:
- **G**: the quoted body / instruction range was re-read in Ghidra and matches the message.
- **M**: the diff was read against the message and the site's own retail comments. The diff does
  what the message says. The address was not re-read.

| verdict | count | commits |
|---|---|---|
| OK | 54 | the rest |
| FINDING | 5 | ef38fa8f5, 84b58b58f, 647551ebf, 0e3708da8, f013829fe |
| QUESTION | 3 | 2649715a8, c6cd23e61, 3c37c2597 |

There are also two general findings (F6, F7) and one general question (Q4). The Ghidra re-read
covered 11 of the 62 audited commits.

## 1. Verdicts, one per commit

### RT (unit 1, the inline release)

| # | commit | site / row | verdict | ev. | reason |
|---|---|---|---|---|---|
| 1 | b8f7ffa9d | String / ArrayDtorImpl inline | OK | M | Bodies moved verbatim (Q-C). It reported 1 new perf warning, honestly, resolved by 2649715a8. |
| 2 | dd4cd10b2 | always_inline, operator=, Filename | OK | M | operator= / Filename ctors / RtMemFree moved verbatim (Q-C). B11 DIFF read as inliner only (Q4). |
| 3 | 91f7767c7 | documenttext 155 / 157 / 159 | OK | M | Every typed local ends at the quoted release point; `^d` temporary order kept. B11 DIFF (Q4). |
| 4 | 74a217f9c | rtcoll::MemFree inline | OK | M | The same MemoryAllocator::MemFree call, one level less. Unit build keeps std::free. |
| 5 | 0e3708da8 | modification 272 / 273 / 286 / 287 | FINDING F4 | M | Typing is right. RefreshList no longer calls MemFree(null): a call that runs changed, under `B7:`. |
| 6 | f8aa82c58 | avl_trees 268 (TypeRecord::children) | OK | G | 0x1800093e0: `MOV RSI,[RCX]; TEST RSI,RSI; JZ` before the lock / free, two qword zeros. Matches. |
| 7 | e3454d514 | introsort 355 | OK | M | Comment only; the cast stays re-tagged with the escape measurement. |
| 8 | 44ad327ed | gfxmat 363 (MaterialTexture::name) | OK | M | Pair locals use the String copy ctor; Draw's dead `image = nullptr` store is gone (codegen only). |
| 9 | 647551ebf | eventlog 286 | FINDING F3 | M | The `{}` pads add two zero stores retail lacks, under `B7:`, not in the B11 account. |
| 10 | cd81636d3 | widestring 196 (RULED cast) | OK | M | Open-coded release matches the quoted 0x18000a730. It leaves RtString_ReleaseInline callerless (F7). |
| 11 | 001fe0d4e | row 1000 (doc) | OK | M | Row exists, SUSPECTED, addresses 0x140095580 / 0x140095750 quoted. |
| 11a | 2649715a8 | FileSystem.h / scene_grid.cpp pragmas | QUESTION Q1 | M | Only silences, zeroes nothing (Q-D). scene_grid's pragma is whole-file; no B11 / RUN line. |

### CTOR (units 2 and 3)

| # | commit | site / row | verdict | ev. | reason |
|---|---|---|---|---|---|
| 12 | aa8986c15 | System::Random ctors, 8 callers | OK | M | Every by-value / raw holder is listed (grep agrees). The index-before-time order change is named. |
| 13 | 5fe276fc9 | game_main 334 (Engine) | OK | G | 0x140041f2d `LEA RCX,[RSP+0x60]; CALL [0x140152bf8]`: no memset. `_allocator` is trivial (asserted). |
| 14 | 1d16fa4e7 | minimap 173 / 175 (Renderer / Scene) | OK | M | No return inside the block. Doors and `extern "C"` declarations deleted with every caller. |
| 15 | ecbcfbc0c | Frustum ctors, load 574 / 577 / 800 | OK | M | Zero-first ctors as C1. Identity-before-ctor reorder in PaintTerrain is named. |
| 16 | 2aaf0339c | gameupdateloop 1416 (pixel rows) | OK | M | Member shape 1 (C2). The memsets take `void*`; per-dword copies kept. |
| 17 | 49ba494b0 | row 1020 (doc) | OK | M | Row exists, SUSPECTED, 0x140071120 / 0x1400711a9 quoted. |

### LX (unit 4, harness puns, rows 992 / 942 / 993 / 994)

| # | commit | site / row | verdict | ev. | reason |
|---|---|---|---|---|---|
| 18 | 82d4b5bf8 | set_clear 210 (H5) | OK | M | Configuration ctor stores match 0x1800c51b0 (+0x1c untouched); dtor order Set then Array. |
| 19 | 59445a7cb | goals_serialize 86 (CellBst.h) | OK | M | The template is the old body; AvlRebalance<> equals CellBST_Retrace's loop (compared). |
| 20 | a36958d85 | citizenaicomponent 590 (D8a) | OK | M | Only the +0x68 store and its log field go; no harness reader of +0x68 exists. |
| 21 | 81afa3ef6 | clearareacomponent 191 (D8b) | OK | M | Real local; the dtor unregisters (the message names ~Internal 0x1800bfdb0). |
| 22 | ef38fa8f5 | row 992 | FINDING F1 | M | "14 stay on StringTable, every one a retail 0x14000bf40 call" is false; fixed later by LX2. |
| 23 | 06a5e2598 | row 942 | OK | M | The `loaded &&` test is retail's own; IsDerivedFrom is the member. No guard added (Q-A). |
| 24 | f013829fe | rows 992 / 942 / 993 / 994, 1040 / 1041 (doc) | FINDING F5 | M | Row 1040 says OWED, not CONFIRMED / SUSPECTED. Row 1041 has no retail address. |
| 25 | cbefc82cc | 993 / row 7 | OK | M | NullSafe replaced by the member, as 0x140067da8 quoted. |
| 26 | f20ee2c81 | 993 / row 8 | OK | M | As above, 0x14006784c. |
| 27 | 6b4fd62fa | 993 / row 10 | OK | M | B11 IDENTICAL, so `B7:` is right by D1. |
| 28 | bb485ad07 | 993 / row 40 | OK | M | Three narrowings, 0x14007b791 / 0x14007b804 / 0x14007b839. |
| 29 | 51426a12b | 993 / row 92 | OK | M | Keeps `obj != nullptr` as retail's own TEST RSI,RSI (0x14001fd69); B11 IDENTICAL. |
| 30 | ec39248e3 | 993 BuildRequirementUI | OK | M | The element is already dereferenced by Type(); B11 IDENTICAL. |
| 31 | 6ab596a99 | 993 TownHallUI | OK | M | Same shape as #30; B11 IDENTICAL. |
| 32 | 672ab232b | 994 G30 | OK | M | One guard dropped, ruled, address 0x1400798d0. |
| 33 | 3c37c2597 | 994 G33 | QUESTION Q3 | G | 0x140067050: id 1 is `JMP 0x140102b60` with RCX=[[gw]+0x120], first. Route coverage is Q3. |
| 34 | 5c75316e7 | LX2: bucket-tree keys | OK | M | +0x100 on Profession, +0x160 on ComponentDescription, as the site's old comments named. |
| 35 | d6a00ff9f | LX2: Nomad materials | OK | M | The two materials move to the 0x140007140 core. |
| 36 | 06451b697 | LX2: EdgeMesh array core | OK | G | 0x140007580 reads `[0x140151330]` before `CALL [0x140152a98]`; the shared loop is used (Q-F). |
| 37 | 1b014f59d | LX2: MaterialInstance array core | OK | G | 0x140020150 is the same body over `[0x140151350]` (Q-F). |

### RES (residue rows and rulings)

| # | commit | row | verdict | ev. | reason |
|---|---|---|---|---|---|
| 38 | 61722cc2c | 960 | OK | G | 0x1400d3f80's last statement reads `[[desc+0x20]+0x20]` with no test. |
| 39 | 73af10044 | 921 | OK | G | 0x14012fc2d FindComponent<SM> and the +0x52 / +0x50 read run before `TEST BL,BL` at 0x14012fc5d. |
| 40 | cd620ae1c | 922 | OK | M | Also drops IsBeingDemolished's and BeginBurning's tests; both named in the message. |
| 41 | afe9661e0 | 920 | OK | M | Writers listed per reader, as D10 asks. |
| 42 | 84b58b58f | 962 | FINDING F2 | G | Costs and command 4 match (Q-E). Retail does test the MemAlloc result (0x14009e8f3), unlike the message. |
| 43 | 6a822b49a | 82 | OK | M | The singleton `goto compare` drop is named. |
| 44 | 97f505c5a | 83 | OK | M | The STANDALONE DEVIATION load-on-miss goes; the observable risk is named. |
| 45 | 184a69e9f | 84 | OK | G | 0x1400265b0: no factory test, `?Find` only; `TEST RSI,RSI` at 0x140026609 is retail's own; generation re-read at 0x140026630. |
| 46 | ac5b279f9 | 89 | OK | M | Two bodies, one row (B6 C1). |
| 47 | 5e8769ead | 692 | OK | G | 0x1400fcc90: CMOVNZ narrowing, `CMP byte [RAX+0x40]` unguarded, CloseDialog on `[[0x1401c3dd8]+0x98]`. |
| 48 | 403e827c1 | 98 | OK | M | The private copies give way to the ported bodies; every inert arm named. |
| 49 | d3050f0c4 | 99 | OK | M | FindComponent<T> has the stop-at-first-match shape the scans need (ComponentEntity.h:82). |
| 50 | c92d90453 | 250 | OK | M | Phase C / phase 2 keep retail's one StorageLocationComponent test. |
| 51 | a7ea5f3a1 | 251 | OK | M | Retail's kept tests (home, dd30 result, icon entity, chosen row) are named. |
| 52 | c6cd23e61 | 252 | QUESTION Q2 | M | Drops the tree-only pick-mesh registrar. Correct by the message, but citizen picking is not on any gated route. |
| 53 | 5425633a3 | 291 | OK | M | Three STANDALONE GUARD returns and CarryCount's warning go. |
| 54 | 18ae6aa9c | 330 | OK | M | std::set / std::vector stand-ins left and said so. |
| 55 | 3d370f904 | 310 G1 + 994 (0x140057b20) | OK | G | 0x140046c60: `CALL [0x1401512e8]` (Rand) at 0x140046c88 before `CMP [R13+0x50]` at 0x140046caa. |
| 56 | 9484c6b94 | 350 | OK | M | MemFree(null) on first append named as retail's. |
| 57 | 19ec01903 | 352 | OK | M | The entity-list load moved before the entity test, as quoted. |
| 58 | c8abe9e76 | 451 | OK | M | Adds an unnamed re-read of `workState` for the sum; value-identical (the stored value). |
| 59 | 8e6224207 | 443 | OK | M | The deleted Dialog copy was the same walk; one port left. |
| 60 | 06d4c2579 | ACT-a bullet | OK | M | The forwarder stays with a deletion note (not a door: see Q-B). |
| 61 | dd7025ff2 | RES status cells (doc) | OK | M | New rows 1060-1064 CONFIRMED and 1065 SUSPECTED, each with an address. |

62 rows in all (61 numbered plus 11a): 42 `behaviour:`, 16 `B7:` code commits and 4 `B7:` block
doc commits.

## 2. Findings

**F1. Row 992's message states a mapping that was wrong (ef38fa8f5).** The message says the 14
sites left on the StringTable core are "every one a retail 0x14000bf40 call". The LX gate's probe
(B7_RULES C4) proved otherwise:
- `citizenlist_serialize.cpp` +0x100 and +0x160 trees stayed on StringTable. The site's own
  comments already read `// 0x140018020` and `// 0x140002340`, the two right instantiations.
- NomadDescription's two materials went to ComponentDescription. Retail 0x14004ff20 calls
  0x140007140 for them.
- HighlightDescription's and DecalDescription's arrays stayed on the GraphicsMesh array core.

LX2 fixed all four (5c75316e7, d6a00ff9f, 06451b697, 1b014f59d), and the gate passed with no
re-cut. What is still wrong is the record. Row 992's status cell repeats the false claim. No row
in `known_port_issues.md` names LX2 or its four commits (grep: 0 hits). Fix: append the LX2
correction to rows 992 and 942.

**F2. Row 962's message misreads the allocation (84b58b58f).** The message says the MemAlloc
result at 0x14009e8e6 is "stored and dereferenced with no test". Retail has the new-expression
test: 0x14009e8f3 `TEST RAX,RAX; JZ 0x14009e908` skips the ctor 0x1400a67f0 and stores null at
+0x68, then faults at 0x14009e90f. The tree now runs `::new (pfV) PathfinderThread` on an
unchecked pointer, so the fault comes inside the ctor instead. The two differ only on allocation
failure, which faults either way, so this is a message error. Fix: correct the row 962 status cell
to quote 0x14009e8f3. Optionally spell the tree's allocation as retail's new-expression.

**F3. EventLogEntry's aggregate adds two stores retail lacks (647551ebf).** The message itself
quotes retail as storing color and the five dwords with "no store to the two pads". The new
`{ color, {}, formatted, ..., popup, {} }` zeroes `_pad04` and `_pad24`. That is a zeroing that
runs. By D1 it needs a `behaviour:` title, and B6 C8 says an unwritten window stays unwritten. The
B11 account mentions only the landing pad's call and one DEC store, so the pad stores are either
merged into wider stores (a store-width change) or unaccounted. It is not observable: the grow's
copy writes fields only. Fix: build the entry without touching the pads, or record a ruled
deviation in row 1000.

**F4. A changed MemFree call under a `B7:` title (0e3708da8).** RefreshList's two temporaries
used to free with an unconditional `MemFree`, whose own null test skipped the work. Their inline
`~Array` now tests `_data` first, so `MemFree(nullptr)` is no longer called.
`System::MemoryAllocator::MemFree` calls the REDZONE instrument before its own null test
(allocator.cpp:622), so the instrument sees the difference. f8aa82c58 makes the same class of
change and is titled `behaviour:`. The message also says RefreshList's B11 shows "no store or
compare difference", which does not fit an added null test. The code matches retail, so this is a
title / message finding only.

**F5. Two new rows lack the required status (f013829fe).** Row 1040 has the retail addresses
(0x1400049c0, 0x140032090) but its status is "OWED", not CONFIRMED or SUSPECTED. Row 1041 says
"each caller's retail body not read in B7": it has no address and no status. Rows 1000, 1020 and
1060-1065 meet the bar.

**F6 (general: the RES and LX merges). Duplicated rows in `known_port_issues.md`.** At 455db4163
the rows 900, 920, 961, 962, 992, 993 and 994 each appear twice:
- f1b47c667 (the RES merge) introduced 920.
- 501a36d41 (the LX merge) introduced the others.

This is the `keepboth.py` doubling B7_RULES §5 warns about. The B7 status is split across the
copies:
- 920: one copy ends "LEFT", the other "FIXED (B7 RES, afe9661e0)".
- 994: the RES half and the LX half are on different lines.
- 992 / 993: one copy is still "OWED".

A grep of the first match, which is what §6's acceptance reads, gets the stale copy. There are no
conflict markers. Fix: de-duplicate by hand, keeping one row per number with both halves of the
status.

**F7 (general). A zero-caller shim (after cd81636d3).** `RtSystem::RtString_ReleaseInline`
(string.cpp:1103, string.h:29) has no caller left. Seven files keep a `using
RtSystem::RtString_ReleaseInline;` declaration and never call it: file_stream,
filesystem_find_win, filesystem_openfile_win, filesystem_setupdirs_win, filesystem_timestamp_win,
modification_setupfilesystem and keymap_load. cd81636d3 calls it "the one remaining out-of-line
copy" but did not delete it. B31_RULES says to delete a shim with zero callers. Not a behaviour
issue.

## 3. Questions for the coordinator

**Q1. The scene_grid.cpp pragma is file-wide (2649715a8).** FileSystem.h's pragma is scoped with
push / pop around the two defaulted members. scene_grid.cpp gets an unscoped `#pragma GCC
diagnostic ignored "-Wuninitialized"` near the top, which silences every future -Wuninitialized
in that TU, not only the retail window. The message gives the reason: a narrower region does not
reach the middle-end diagnostic. It omits D2's B11 verdict and the RUN line ("0 new warnings" is
the only result). A pragma changes no code, so B11 would be IDENTICAL. Please rule whether a
whole-TU suppression is acceptable, or whether it should be scoped to the insert paths' functions.

**Q2. Row 252's pick registrar and route coverage (c6cd23e61).** The tree-only
RegisterCitizenPickMesh registered a citizen's pick mesh from the move tick when
PickingComponent::OnAdded had not. Retail has no such step, so dropping it is right. But if OnAdded
leaves any citizen at pick id -1 in our tree, citizens silently become unclickable. None of the
gated routes clicks a citizen (new game, tutorial, loadgame, ui = mods / menuquit / options /
achievements / goaldialog, tools = placement / road / removal). Please confirm citizen selection
on one route before the close.

**Q3. Row 994 G33 changes what closing an entity panel does (3c37c2597).** The commit says so:
closing a panel now tears the tool down to the default EditTool, retail's behaviour. The reading is
verified at 0x140067050. As with Q2, no gated route closes an entity panel. The message also has a
lost word: "the tree's own  (2026-07, EntityUI pin/center wiring)", probably shell quoting
(reference_heredoc_eats_backslashes).

**Q4. `B7:` titles over a B11 DIFF.** D1 says `B7:` only "when the B11 verdict at -O2 is
IDENTICAL or SAME for every function". b8f7ffa9d, dd4cd10b2, 91f7767c7, 44ad327ed, 59445a7cb and
2aaf0339c (PaintTerrain) carry DIFFs that each message reads as inlining only, with the same calls
in the same order. I agree with every reading I checked. The exceptions are F3 and F4, where
something does run differently. Please confirm that "DIFF read as inlining only" satisfies D1, so
the close does not re-title them.

## 4. The specific questions

- **Q-A (null tests retail lacks).** None added. No B7 diff adds `IsDerivedFromNullSafe`; the
  diff greps only removals. The `if (p)` tests that remain are retail's own: row 942's
  `loaded &&`, 51426a12b's 0x14001fd69, 184a69e9f's 0x140026609, c92d90453's StorageLocation
  test. The one added store is F3's pad zeroing, which is a store, not a guard.
- **Q-B (doors left beside a member).** None. At 455db4163 nothing defines or declares
  `RtSystem_Random_Ctor*`, `RtEngine_Engine_*`, `RtGraphics_Renderer_*` / `_Scene_*`,
  `RtVolume_Frustum_Ctor_*`, `RtSystem_Configuration_Ctor`, `RtSystem_ConfigMap_Find`,
  `rtsetclear::`, `rtavl::PtrArrayDtor` or `fn_GetCurrentProductIndex`. Two leftovers are noted:
  - `Pf_RegionPairCacheTick_5d10`: a pre-existing forwarder, deletion owed by its own comment.
  - `RtString_ReleaseInline`: a callerless shim (F7).
- **Q-C (always_inline bodies).** Yes, byte-for-byte the same reads and stores:
  - String(), String(const String&), ~String, Release, Adopt and operator= are the deleted
    string.cpp bodies verbatim.
  - The freeing arm keeps data-8, the sharing arm nulls.
  - ArrayDtorImpl is array.cpp's body verbatim: test, MemFree, three zero stores, return null.
  - The only spelling changes are `RtSystem::` qualifiers, `static_cast<String&&>` for
    `std::move`, and RtMemFree / rtcoll::MemFree becoming inline forwarders to the same
    `System::MemoryAllocator::MemFree`.
- **Q-D (the pragmas, 2649715a8).** They only silence a diagnostic and zero nothing; the diff is
  two pragma blocks and comments. scene_grid.cpp's pragma is file-wide (Q1).
- **Q-E (row 962's priming).** Yes. The table stays `{3,3,4,8,16,16,16,16,16,3,4}` for indices
  0..10, and retail 0x14009e9e5..0x14009ea98 loads exactly those (`LEA R8D,[RDX+n]`). Command 4 is
  `{4,1,0,0}`: 0x14009e93e / 0x14009e946 / 0x14009e94f, sent by SetSearchFlags(1,0,0) to the same
  type and arguments. Retail's order is kept: search flags first, then the eleven costs, then the
  cell-flag loop. The one message error is F2.
- **Q-F (the LX2 array cores).** Yes. Retail 0x140007580 reads `[0x140151330]` (EdgeMesh) and
  0x140020150 reads `[0x140151350]` (MaterialInstance) before `CALL [0x140152a98]`; the two bodies
  are otherwise instruction-identical. The tree's cores pass those types to
  `ArraySerializeExternalGeneric`. That is the same loop the GraphicsMesh and Particle cores use:
  count, then `SerializeExternalRef` per element. The MaterialInstance id is the one the 0x140007140
  core uses. The ComponentDescription array port (`Desc_LoadExternalArray`) has the same
  count-then-element shape but no type test at all (row 1040).
