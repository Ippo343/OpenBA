# B6 independent audit (2026-10-09)

Scope: the 147 commits `338120596..6366923fe` (main at the CLOSEB gate-table commit). Read-only:
no build, no run. Every `behaviour:` commit (110 of them; the 111th grep hit is the B6 kit commit,
whose subject merely quotes the word) was read in full with `git show`. Retail evidence was checked
in Ghidra with `program="Application-x64.exe"` / `"Runtime-x64.dll"` passed explicitly.

Evidence column legend:
- **G**: the quoted body / instruction range was re-read in Ghidra (decompile, disassembly or both)
  and matches the message.
- **M**: the diff was read against the message and the tree's own comments. The diff does exactly
  what the message says, and the message is internally consistent. The address was not re-read.
  None of these carries a sign of trouble. The M rows are the ones to sample if a second pass is wanted.

The b6-D merge (`86275dafd`) landed on main after the audit base. It is not audited here. Where a
finding is still true at `86275dafd`, the finding says so.

## 1. Verdicts, one per `behaviour:` commit

| # | commit | row(s) | block | verdict | ev. | reason |
|---|---|---|---|---|---|---|
| 1 | d068b9ad1 | 820 | SAVE | FINDING F1 | G | The instructions 0x1400c3212..0x1400c321e and slot 0x140152a50 = `Serialize(unsigned int&)` are correct. The message calls the body "0x1400c3100": the body is 0x1400c3140 (0x1400c3100 lies inside 0x1400c30a0). |
| 2 | 0b8872080 | 63 | SAVE | OK | G | 0x14001bbf0: seven `[0x140152a60]` float calls, two `[0x140152468]` short calls, `0 < (int)count` only. |
| 3 | eb8b5e335 | 17 | SAVE | OK | G | 0x140055ee0 tests `*(int*)(stream+8)` for both arms; IsDerivedFrom/CMOVNZ, then +0x28 is read unguarded; the loop is `0 < count`. |
| 4 | 40167d99c | 61 | SAVE | OK | G | 0x1400b04d0 calls `Serialize(int, short*)` on +0xd8 right after the load-side SetSize, with no buffer test. |
| 5 | 26fe016f0 | 16 | SAVE | OK | G | 0x14009538a calls `[0x140152a50]`, the uint overload. |
| 6 | 43e05e02b | 21 + 22 | SAVE | OK | G | Save uses MOVZX byte -> `[0x140152300]` (raw bool). GameMain writes byte +0xc, dword 0x1010001 at +0x14 and byte +0x18; the pads are unwritten. |
| 7 | e4eebc992 | 19 | SAVE | OK | G | 0x14012e7ab `LEA RDX,[RDI+0x2c]` -> `[0x140152ac8]` (int&). |
| 8 | 565addbb3 | 13, 14, 15, 810 | SAVE | OK | G | Decompiled 0x14008f760 in full. Time is at entry+0x18; the 0x47 clamp is as quoted; the only version-5 clear is +0x140's; the version<2 arm uses a stack {root,count} freed through 0x14000e320 and MemFree. |
| 9 | 4dae85996 | 551 | COMP | OK | G | 0x14012cf28 `CMP [RCX+0x30]` has no null test. The deletion of the forwarder is owed (see F4). |
| 10 | b96e68c10 | 1 | COMP | OK | G | 0x14010f7a0 calls the StorageComponent finder twice and reads profession +0x50 and capacity +0x38, all unguarded. |
| 11 | 55c57fc34 | 400 / 550 | COMP | OK | G | The three `CALL 0x140003f60` and `MOVSX [RAX+0x164]` are as quoted. |
| 12 | 397788e90 | 552 | COMP | OK | G | 0x140125c60 finder 0x140003f60, then `ADD RCX,0x28; CALL [0x1401514b0]`. |
| 13 | 1df7f9e5a | 45 | WORK | OK | G | 0x140117523..0x140117536: RCX=RDI on a miss, then `[RCX+0x20]` -> `[+8]` -> `[0]`, unguarded. |
| 14 | 96ab96c4c | 51 + G68 | WORK | OK | G | CMOVNZ, then `[RBP+0x114]` unguarded, two AsSeconds (`[0x140151318]`), MULSS and Time(float). |
| 15 | a84c8e424 | 452 | UI | QUESTION Q1 | G | The retail over-read is real (see Q1). The fix is a ruled STANDALONE DEVIATION, which contradicts D7's text. |
| 16 | 1bed32c6e | 430 | COMP | OK | G | 0x140057f30..0x140057f3f is `[RAX+0x38]` -> `[RCX]` -> `[RAX+0x7c]`. |
| 17 | c086860bc | 392 / 510 | COMP | OK | G | 0x14005b24f and 0x14011e83f..0x14011e85c hand `&Time(0)` as quoted. The guards left in place (row 922, TickIdle's `smV &&`) are Q2. |
| 18 | 4a37f358c | 220 | WORK | OK | G | 0x1400e5c25..0x1400e5c34 appends `[node+0]` (the entity key; the citizen is +8, read at 0x1400e5b80). |
| 19 | 87445808c | 135 | WORK | OK | G | 0x1400e5e3a `CMP qword [R8],0` has no test of R8. |
| 20 | a5f1be55c | 690 / 664 | UI | OK | G | 0x1400649d0: the at-school arm passes &dialog and &[school+0x10]. |
| 21 | afd18174e | 221 + 900 | WORK | OK | G | Decompiled 0x1400e4be0. It calls 0x1400f7100 (RDX is unused there), unlinks by node through 0x14001a6e0, returns 1 when nothing was collected and 0 after removals. |
| 22 | 399ec15cc | 511 | COMP | OK | M | Pointer-as-ticks replaced by `time->AsSeconds()` and Time(float). |
| 23 | a4b2ce507 | 704 | COMP | OK | G | The own-home inline insert (0x14010a9c9..0x14010aaf4) counts its own inserts and skips duplicates; the tree matches. |
| 24 | c1b49d382 | 260 | WORK | OK | G | 0x1400e8080 -> 0x1400e833a..0x1400e8377 falls through into 0x1400e837c (the shared setup, `[RBP+0x98]=RDI`). |
| 25 | 09806424e | 705 | COMP | OK | G | 0x1401140b8 CMOVNZ, then 0x1401140c8 / 0x1401140ce are read unguarded. |
| 26 | d18c16740 | 170 | COMP | OK | M | Three D4 drops with the quoted ranges. |
| 27 | 792530f5c | buildreq + 940 | UI | OK | G | The 0x14005e7c0 resolve chain and inline finds match. The icon/title `buildReqComp` tests are honestly left as row 940. |
| 28 | ebb034eb8 | 643 | WORK | OK | G | 0x140002ed0 reads cell 0x1401c7900 (InteractComponent); the null test at 0x1400e70ee is retail's own. |
| 29 | 68d496a74 | townhall | UI | OK | G | 0x14007ca70..0x14007cacc: the inline find falls to `XOR EBX,EBX`, then `[RBX+0x20/0x28]` unguarded. |
| 30 | 9ca9d90fe | 572 (b) | UI | OK | M | |
| 31 | b33ebb2cb | 861 | WORK | OK | G | RBP = entry-0x5f, so `[RBP+0x77]` is R8's home; the finder writes `[RBP+0x7f]` (the R9 home); 0x1400ec355 / 0x1400ec35c pass `[RBP+0x77]`. |
| 32 | faa036212 | G46 | WORK | OK | M | |
| 33 | c3830aa8f | 280 | COMP | OK | M | |
| 34 | ec396cd34 | 850 | COMP | OK | G | 0x14008f3b4..0x14008f40f as quoted. |
| 35 | 7d22ea40e | 390 / 391 | COMP | OK | G | 0x14012ea20 disassembled in full. The node layout (left +0x18, right +0x20) is asserted in WorkSchedulerNodes.h. |
| 36 | c4c8b27b2 | statusicon | COMP | OK | M | |
| 37 | 85ffa7716 | 572 (a) / 941 | UI | OK | M | |
| 38 | 73659b3a2 | 46 | COMP | OK | M | |
| 39 | abade1867 | 44 | WORK | OK | G | 0x1400f22c0 in full: ComponentEntity cell 0x1401c5d70, BTR 0x14, `MOVZX EAX,AX`. The quirk is copied, not fixed. |
| 40 | 675feb667 | 52 | WORK | OK | M | |
| 41 | 202285004 | 643 (callers) | COMP | OK | M | Typing only. |
| 42 | f550d4113 | menu clamp | UI | OK | M | Ruled by the coordinator. |
| 43 | fdd5c13bf | splash clamp | UI | OK | M | Ruled by the coordinator. |
| 44 | 4d813e005 | 95 | UI | OK | M | Dead arm (D5). |
| 45 | 5aa212d8c | 35 / 43 / 80 / 450 (D12) | WORK | OK | G | 0x14012e5c0 in full: CMOVNZ, then `[RDI+0x34]` unguarded, `CMP EAX,R8D; JG` returns `_defaultWorkers` as is, and `CMOVL` clamps only below. The member now matches retail. |
| 46 | 77c641274 | 28 | COMP | OK | M | The +0x30 / +0x34 stores are honestly left as row 920. |
| 47 | 81679c9bd | 27 | COMP | OK | G | 0x14005c430: +0x60 qword, +0x68 dword, +0x71 byte; nothing at +0x6c..+0x70 or +0x72..+0x77. See Q7. |
| 48 | 00234d93d | 76 | UI | OK | G | 0x1400db7eb `LEA RAX,[RBP+0x58]` is arg 7, a separate slot from arg 2 `[RBP+0x80]`. |
| 49 | d1a5fc963 | 26 | COMP | OK | M | |
| 50 | 0a7a67055 | G45 | WORK | OK | G | 0x1400f3976 -> 0x1400f3991, no test between. |
| 51 | 2826e4502 | playanim + G70 | WORK | OK | G | 0x140042420 in full (21 instructions). |
| 52 | 028da0a62 | G72 | WORK | OK | M | The message names the crash risk honestly. |
| 53 | 133a2c79a | G5 | COMP | OK | M | |
| 54 | 37a99c94a | 62 | UI | OK | M | |
| 55 | 419bef6d0 | G40 (+G39) | WORK | FINDING F2 | G | G40 is correct. The commit also drops HarvestArea::Execute's natRes1 null skip (G39), which the message does not mention. |
| 56 | a13db9d97 | 70 | UI | OK | G | `read_memory 0x140172248` = `00 00 7a 44` (1000.0f); 0x1400fd6e6 / 0x1400fd6ee as quoted. |
| 57 | 73360b5ad | G4 | COMP | OK | M | The `!mesh` arms are honestly left as row 921. |
| 58 | 239722f12 | G3 | COMP | OK | M | |
| 59 | b66778341 | G42 | WORK | OK | M | |
| 60 | 762cf3004 | G41 / G43 | WORK | OK | M | |
| 61 | 35ab82a18 | G11 / G55 / G44 | WORK | OK | M | |
| 62 | 6234b3611 | 741 | UI | OK | G | 0x1400667de `ADD RCX,0x60`; 0x1400667e2 `CALL 0x14002c690`. |
| 63 | 62bc12e9b | 735 | UI | OK | M | |
| 64 | 6c573b776 | 92 | UI | OK | G | 0x14001fd10: generation test, Find, IsDerivedFrom-or-null, stores. See Q5 on IsDerivedFromNullSafe. |
| 65 | 2419abeeb | 470 | UI | OK | M | True at its commit. Its "unconditional MemFree" comment became false after row 104 (F3). |
| 66 | 0bff54cd8 | 3 + 942 | UI | OK | M | |
| 67 | 21ba25607 | 7 + G13 | UI | OK | G | 0x140067cc2 / 0x140067cc9 save the count to `[RBP+0x67]`; 0x140067fd5 / 0x140068047 store it. Q5: a null FindOrLoad now reaches the ctor through NullSafe. |
| 68 | 0dce4df8a | 8 | UI | OK | M | Q5. |
| 69 | a162bc754 | 10 | UI | OK | M | Q5. |
| 70 | 881bced30 | 40 + G38 | UI | OK | M | Q5. |
| 71 | c8d172a80 | 38 | UI | OK | G | 0x14006abf0 narrows each "whole" / "half" find to ImageElement. The dtor order is unchanged (+0xD0, +0xC0, +0xB0, +0xA0). |
| 72 | c2398da5e | 640 / 651 / 652 | C | OK | M | |
| 73 | 52e94d505 | 36 | UI | OK | G* | *Runtime claim checked: userinterface_frame.cpp:310 calls `DialogInstance::ComputeLayout` for every dialog every frame, and that dispatches controller slot 7 (dialog.cpp:914). |
| 74 | 961d64615 | 90 + 91 | C | OK | M | Its "unguarded" dtor comment became false after row 104 (F3). |
| 75 | bbd9358d2 | 37 + G29 | UI | OK | M | |
| 76 | e4cd05794 | G29 / G31 | UI | OK | M | G30 is kept and not ruled (Q6). |
| 77 | 78990f93d | 42 | C | OK | M | |
| 78 | c6c51c2a6 | G29 / G32 | UI | OK | M | |
| 79 | b82c6a526 | 9 | C | OK | M | |
| 80 | 5b097cde4 | 39 | UI | OK | G | 0x140015200 / 0x140015500 have the same call layout as TradeUI's 0x1400162d0, with small sorts 0x140015ad0 / 0x140015c40. Comparator 0x14007c620 matches. The core moved unchanged. |
| 81 | 08f2ad6bb | 60 | UI | OK | M | |
| 82 | 090bf3b89 | 74 (1/4) | UI | OK | M | |
| 83 | 32e87432d | 74 (2/4) | UI | OK | M | Edge over-read copied from retail, and stated. |
| 84 | abd5d7213 | 74 (3/4) | UI | OK | M | |
| 85 | 8f6d95736 | 74 (4/4) | UI | OK | M | |
| 86 | 9a600869a | 50 | C | OK | M | |
| 87 | fdd5efb5b | 733 / 943 | UI | OK | M | |
| 88 | cd79fa582 | 47 + 48 | C | OK | G | 0x1400a6564 Lock, 0x1400a6582 SetSize, 0x1400a658b Unlock, the unlocked loop reloading `[RBP+0x160]` (0x1400a65b0..0x1400a65d3), then the ready store at 0x1400a65e3. See Q4. |
| 89 | 275d79090 | 733 (Draw) | UI | OK | M | |
| 90 | 7c201f68d | 49 | C | OK | M | |
| 91 | 85fc1907a | 93 | UI | OK | M | |
| 92 | f81359e72 | 94 | UI | OK | M | |
| 93 | 770f27e5b | 371 | C | OK | M | |
| 94 | 0aa66dde7 | G17 | UI | OK | M | |
| 95 | 553e29d54 | 104 | C | FINDING F3 | G | Runtime 0x18001c11c..0x18001c122 tests the pointer, so the fix is right. It left the row 470 / 91 comments stating an unconditional free. |
| 96 | 9b6e09de6 | G75 + 944 | UI | OK | G | 0x14001e7c0 decompiled and compared branch by branch with introsort24.h (median-of-3, both scans, the equal-run walk, smaller-side choice, small sort): equal. |
| 97 | 050580734 | G33 | UI | QUESTION Q6 | M | The kept EditTool-deselect block is labelled D4(b) but names no PORT GAP address. |
| 98 | ffb05e84c | G35 | UI | OK | M | |
| 99 | bbe286c51 | 32 | C | OK | M | |
| 100 | 3e022f7c1 | G36 | UI | OK | M | |
| 101 | 32f2ace01 | G37 | UI | OK | M | The D4(a) keep names a shared template. |
| 102 | e0b0ec267 | G14 (1/2) | UI | OK | M | |
| 103 | 48561bfbc | G14 (2/2) | UI | OK | M | |
| 104 | a18432d17 | 2 | UI2 | OK | M | |
| 105 | acd1e913c | 4 | UI2 | OK | M | |
| 106 | a2c282ed0 | 11 | UI2 | OK | M | |
| 107 | 40063fb93 | 311 | UI2 | OK | M | Entity_PlaceAtCell 0x140057b20's own null tests are kept with no row (Q6). |
| 108 | 058e70353 | 653 | UI2 | OK | G | 0x14000f9e0: unconditional MemFree, qword zero, slot-address test only. |
| 109 | 90d256918 | 13 (TRADE) | TRADE | OK | G | 0x1400c5af4 / 0x1400c5aff read the singleton and +0x118 with no test. The bisect narrative is consistent with row 742's fix. |
| 110 | e35119a91 | ACT-b bullet + tradework | CLOSEB | FINDING F7 | G | The code is right. The message misdescribes retail's control flow (see F7). |

NOT-A-DIFFERENCE / already-retail closures, checked:
- **Row 18:** 0x14012b940 serializes +0x50..+0x53 as `char`. Holds.
- **Row 20:** FruitComponent 0x14004a070 reads +0x30 as float and +0x34 as bool, the tree's declared types. Holds.
- **Row 370:** the message quotes `~Font` order. The tree's empty-body dtor over String / Array members gives that order. Holds (M).
- **Rows 30 and 723:** these are "fixed before B6" or a partial LEFT with a stated owing. Holds (M).

## 2. Findings

**F1. Wrong body address in the row 820 message (d068b9ad1).** Retail Persist is 0x1400c3140
(Ghidra body 0x1400c3140-0x1400c33c5). The message says 0x1400c3100, an address inside
0x1400c30a0. The instruction addresses it quotes are right, and so is the code comment. Fix: correct
the address where the row's status cell or CLOSE_NUMBERS cites it. This is a §14g citation error only.

**F2. G39 is folded into the G40 commit without a word (419bef6d0).** The message documents only
`HarvestArea::Cleanup` (G40). The diff also deletes `HarvestArea::Execute`'s `natRes1 == nullptr`
skip (harvestarea.cpp ~266, block G39). The evidence for it lives only in the new code comment. I
verified that evidence: 0x1400ed078..0x1400ed086 CheckPtr `[RBP-0x29]` and read `[RAX+0x20]` with
no test, and natRes2's test at 0x1400ed0e9 is retail's own. So the change is right, but it breaks
D1 / D2 (one row per commit, evidence in the message). The gate table notes the sharing, but no
message carries the evidence. Fix: in G39's known_port_issues status, cite 419bef6d0 and
0x1400ed078..0x1400ed086.

**F3. The comments now claim an unconditional free (553e29d54 against 2419abeeb / 961d64615).**
Row 104 made the one erased `~Array` core test `_data` before MemFree (array.cpp:254). That matches
the Runtime's J1 bodies. But every `Collection::Array` in code/Game now runs that core, including the
two members rows 470 and 91 retyped. Their comments still say retail's exe-inlined dtor's
unconditional free is what runs:
- `src/game/Game/MapGenerator.h:79`: "unconditional MemFree, then the header zeroed".
- `src/game/Game/mapgenerator.cpp:73`: "ClearCallback is the unconditional MemFree".
- `src/game/Game/weather_particles.cpp:154` and `WeatherParticleGeometry.h:61-63`: "unguarded".

The behaviour is value-identical, because MemFree(nullptr) is a no-op. Only the REDZONE instrument's
null-free count differs. Fix: reword them, for example "the exe's inlined copy frees unconditionally;
the tree's core tests first, value-identical (row 104)". Do not touch the code.

**F4. Row 551's forwarder and its stale references.** At `6366923fe`, 11 Work-file calls still went
through `CitizenComp_StorageComponent_QueryCount`: foresterwork.cpp:587, gatherrawmaterials.cpp:265,
herbalistwork.cpp:405, huntwork.cpp:152, locationwork.cpp:104 and others. Row 551's status cell says
"the coordinator moves them and deletes it".

At `86275dafd`, b6-D's 982d46f99 moved the callers. The body is still there, at
`citizenaicomponent.cpp:317` and `Component/exe_bodies.h:140`, with no caller.

Include-line comments still name forwarders deleted in B6:
- `huntcomponent.cpp:18`, `workcomponent.cpp:24`, `workplacecomponent.cpp:37`, `town_workplace_registry.cpp:22` name `WorkComponent_CalcMaxWorkers`.
- `tradecomponent_livestock.cpp:75`, `debugui.cpp:58`, `fishingwork.cpp:27` name `Pf_EntityResolvePos_2e20`.
- `removeanimal.cpp:26`, `removenaturalresource.cpp:32` name `NotifyEntityChanged`.
- `work_leaf_hunt.cpp:25` names `StateMachine_TickReimpl`.
- `action_pf_roots_route.cpp:65` names the QueryCount forwarder.

CLOSE-A's claim "every former caller calls the member" holds: grep finds no call of the three
CLOSE-A forwarders, nor of `Toolbar_CenterOnEntity_Impl`, `CalcMaxWorkersGuarded`,
`CalcWorkerSlots_Impl`, `ResizeStride0x10` or `NaturalResourceComponent_FindComponentByType`. Fix:
delete the QueryCount forwarder and its declaration, and drop the stale include comments.

**F5. The retired reasons survive at sites with no row.** B6 ruled two reasons stale: "IsDerivedFrom
is unreliable in standalone's partial factory tree" (rows 705, 51, 11, 311) and "partial world /
inert" (G3, G4, G14, CLOSEB). The same reasons still justify skipped IsDerivedFrom checks or guards
retail lacks at sites with no known_port_issues row and no 14d bullet. All of these are still present
at `86275dafd`; 104 such comment lines remain in code/.
- **The IsDerivedFrom-trust family:**
  - `growthcomponent.cpp:295-310` (Startup).
  - `mapcomponent.cpp:250-256` and `:359`.
  - `modelcomponent.cpp:68-79` (LinkedModelDescription).
  - `zonecomponent.cpp:178-188`, which also has the `!desc` / `!_singleton` returns.
  - `starttown.cpp:203`, `:272`, `:4384-4395`, `:4568-4575`.
  - `menudialogloopt.cpp:825-835`, `menudialogloopt_stubs.cpp:125`.
  - `mapparamsdialog.cpp:233`, `newgamedialog.cpp:146-153`.
  - `componententity.cpp:152-160`, the "no factory for component" skip.
- **Self-declared guards retail lacks, in files known_port_issues never mentions:**
  - `herdcomponent.cpp:169`, `:250`, `:252`, `:334`, `:395`, `:511`.
  - `floorcomponent.cpp:142`, `:153`, `:310`.
  - `citizenai_handlers2.cpp:357`, `citizenai_handlers3.cpp:358`.
  - `action_pf_ctors.cpp:148`, `autopickupcomponent.cpp:68`.
  - `tradework.cpp:88`, `:110`, `:274-275`, `:284`, `:378`, `:395`. CLOSEB edited the lines next to these and left them.

B6_RULES §6 requires every difference to end FIXED, NOT A DIFFERENCE or LEFT with an owing row.
These have no row at all. Fix: file one new 14d row listing them, so they are owed. Alternatively,
rule them as D4(a) with a reason stated at each site.

**F6. Section headers and status cells that contradict their own fix** (sampled from the `B6:` doc commits):
- **§10 header.** "`0x140172248` is 1000.0f ... — SUSPECTED" still carries "What a fix must do", although row 70 fixed it (a13db9d97).
- **§13 header.** It still says "OPEN"; its body ends in "FIXED (B6 TRADE, 90d256918)".
- **Row 452's status cell.** It reads "... not re-read); LEFT raw FIXED (B6 UI, a84c8e424) ...". The old "LEFT raw" was not removed when FIXED was appended.

Fix: update the two headers and drop "LEFT raw".

**F7. The CLOSEB message states retail control flow that is not there (e35119a91).** The message
says 0x1400c5b40 calls `CompleteTask_helper_8760(*(g+0x118), ...)` "straight after the +0x30 lazy
resolve". Retail tests `if (-1 < taskIndex)` in between, and calls 0x1400f8e30 when the helper
returns true. The tree has both, so the code is right and only the message is wrong (§14g). Fix: none
in code. Note it in CLOSE_NUMBERS or the row cell.

## 3. Questions for the coordinator

**Q1. Row 452 (a84c8e424) against D7.** The retail over-read is confirmed:
- 0x1400b56a0 calls 0x140006270, which compares `Type()` for exact equality with cell 0x1401c7e78.
- That cell has one writer: `StorageComponent_RegisterType`, `LEA` at 0x14014fb35 (xrefs read).
- The body then reads `[comp+0x58]` / `[comp+0x50]` past the 0x40-byte object.

The commit replaces this with ResidenceComponent by name, a STANDALONE DEVIATION. D7 says "No 'fix'
of retail", while INVENTORY records this as "452 by the user's ruling". Please amend D7 (or add a §7
correction) to record the ruling and its scope, so the deviation is ruled in the rule sheet and not
only in INVENTORY.

**Q2. Pre-B guards still kept after rows 392 / 510.** These remain in place:
- `RequestStateTransition`'s `!m` / `!node` returns (controller_state_table.h).
- `TradeComponent::TickIdle`'s `smV &&`. 0x14005ab65 reads `[sm+9]` unguarded. This guard is not named in row 922.
- `ForceSMState`'s two returns (row 922).
- `SetAccepting`'s `!mesh` arms (row 921).

Their reasons are "inert controllers" and "partial world", the same class B6 retired elsewhere.
Rows 921 and 922 say "LEFT for a ruling". Please rule them, and add TickIdle's guard to row 922.

**Q3. Row 990, the fixture re-cut: the cause is asserted, not shown.** The re-cut itself is sound.
The new fixture comes from the current tree, and retail never makes a citizen without a profession.
But the stated cause, "children born under the tree's old birth path, which passed no profession",
does not match the history. The newborn path, `citizencomponent.cpp:1420-1432`, has passed
`m_professionList->field_0x30` since d0156a1d2 (2026-06-18), before the 2026-09-12 fixture was made.
So the nulls came either from `field_0x30` being null at those births or from a later writer
clearing `profession`. The mechanism is not identified.

The new fixture is a frame-661 new game, which most likely holds no newborns. The resave gate
therefore no longer exercises a birth.

Related: `ProfessionList::Serialize` (professionlist.cpp:99-102) loads its four Profession refs
through the StringTable ExternalPtr instance, where retail 0x1400a8e30 calls 0x140018020, which
validates `TypeInfo_Profession`. This is the row 2 / 4 pattern, masked today by row 942. Fixing 942
first would null every default profession.

Suggested: one run on the current tree that produces births and checks that the newborns'
profession is non-null, before CLOSE_NUMBERS says "root-caused".

**Q4. Rows 47 + 48 (cd79fa582): a known race, deliberately restored.** The retail reading is right.
Two points need a ruling:
- The tree relies on `std::atomic_signal_fence` to stop GCC hoisting the two loads out of the loop.
  That works in practice, but it is not guaranteed for non-atomic data, and it was checked only on
  the perf object at that commit.
- The B4-era tree saw a real AV from this window. Row 961 records it.

Please rule whether a race that has produced an AV stays as "faithful", or goes to retail_bugs.md
with a deliberate deviation, as with 452.

**Q5. `IsDerivedFromNullSafe` in new narrowing code.** Retail's `IsDerivedFrom` (Runtime 0x1800c0390)
dereferences `this`. Some B6 commits switched to the member call: rows 9, 44, 51, 52, 705, G46, G55.
Others introduced new `IsDerivedFromNullSafe` narrowings, which add a null test, the thing D4 says
not to add:
- rows 7, 8, 10, 40 and 92;
- the BuildRequirementUI and TownHall find templates.

This is mostly value-identical, because the narrowed pointer is dereferenced next anyway. Row 7 is
the exception: a null FindOrLoad result now becomes a null `popupRes` passed to the ctor, where
retail would fault in IsDerivedFrom. Please rule on one spelling.

**Q6. Keeps that are labelled but not ruled:**
- **G33 (050580734).** The EditTool-deselect block before CloseDialog is called "D4(b)". Its reason, "EntityUI slot 9's teardown the standalone does not run", names no PORT GAP address. Is it (a) or (b)?
- **G30 (e4cd05794).** The textSeason string-table test is kept and listed as not ruled.
- **0x140057b20 (40063fb93).** Entity_PlaceAtCell's null tests are kept as "a separate body, not in this row", with no row recording them.

**Q7. Row 27, `workEnabled` (81679c9bd).** The commit is faithful: retail's ctor leaves +0x70 as
allocated. Persist writes the raw byte through `Serialize(bool&)`. A trading post saved before any
Command carries whatever byte the allocation held. That matches retail, provided the tree's
allocator returns the same bytes. Confirm the policy holds for C++ `bool` members, since any read of
a non-0/1 bool in tree code is UB. No tree reader exists today besides Persist and Command's store.

## 4. Counts and measurements

| verdict | commits |
|---|---|
| OK | 104 |
| FINDING | 4 (d068b9ad1, 419bef6d0, 553e29d54, e35119a91) |
| QUESTION | 2 (a84c8e424, 050580734) |

There are also three general findings (F4, F5, F6) from the doc-commit sample and the tree sweep,
and five general questions (Q2-Q5, Q7). The Ghidra re-read covered 50 of the 110 commits (the G and
G* rows). The other 60 were checked against the diff and the message (M).

Measured at `6366923fe`, and again at `86275dafd`:

```
# castmetric total: 26
01 counted casts in PROD ... = 26 (target 26)   [0 DEFERRED: retail-difference task + 23 DEFERRED: named prerequisite + 0 DEFERRED: dx11 seam step + 3 RULED: stays listed + 0 WORK]
10 fields of layout-asserted classes with no static_assert(offsetof) = 19 (target 0)
11 Rt<Ns>:: POD mirrors beside a real <Ns>:: class in runtime/types = 0 (target 0)
12 b3r_session/acceptance.sh lines off target = 0 (target 0)
ACCEPTANCE: 1 line(s) off target
```

Lines 02-09, 11 and 12 are at 0. Line 10 reads 19 against the accepted 21, so it is lower, not
higher. The `cast_ok` count was 723 at `6366923fe` and is 720 at `86275dafd`.

| check | result |
|---|---|
| `grep -c '^<<<<<<<' docs/known_port_issues.md` | 0 |
| duplicated row numbers (`grep -oE '^\| [0-9]+[a-z]? \|' ... \| sort \| uniq -d`) | none |

b6-D (after the base) carries its own commits for rows 2, 4, 11, 310 / 311 and 653, which UI2 had
already fixed. `git diff 6366923fe 86275dafd` on those five files is empty, so the merge kept UI2's
bodies.
