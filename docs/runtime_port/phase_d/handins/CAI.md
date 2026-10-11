# CAI hand-in (PORT GAP hunt, branch `d4-cai`, base `b9006081e` = `1e842b306` + the rule sheet)

CitizenAIComponent's two empty slots, HappinessComponent's OnMessage, the 26 retail message arms
that reach them, the two Runtime import slots, Pickup's slot 12, and rows 290 / 1181 / 1262.

**Static gate only.** The game was never run from this worktree. Every route below is the
coordinator's, on the merged exe.

Main was merged at the end (`0a3796af2`, main `ca6cc314c`). The merge was clean, and no file
overlapped.

## 1. Commits

Each commit was gated on its own tree, with these results:
- `standalone-debug`, `standalone-perf` and `unit` at `-j2` all exited 0.
- Unit was 1002 / 0 at every commit.
- There were 0 new warnings against the main checkout's `runs/warn_baseline_{debug,perf}.txt`, using gate.sh's `norm()` and `comm -23`.

B11 is measured against `runs/b11base_d4e/perf/obj`. A SAME verdict means the text differs, but no call, store or compare changed.

| commit | title (short) | B11 |
|---|---|---|
| `c40611053` | behaviour: CitizenAIComponent::OnMessage = 0x14010f340, and the 26 arms | citizenaicomponent.o: DIFF 1 (OnMessage), IDENTICAL 22. Every arm TU: IDENTICAL, plus SAME on the arm function only (the type-id load replaces the immediate 0). |
| `22ae75e8a` | behaviour: CitizenAIComponent::OnAdded = 0x14010f880 | citizenaicomponent.o: SAME 1. OnAdded compiles to `jmp CheckAge`, retail's shape. |
| `3fd92ed20` | behaviour: HappinessComponent::OnMessage = 0x14004a780, and its arm | happinesscomponent.o: DIFF 1 (OnMessage), IDENTICAL 41. citizenai_actionctors.o: SAME 2. |
| `65beacc28` | port: the DeletePtr import slot 0x140151ab8 is ALREADY THERE | Comments only. No verdict changed. |
| `c6b693c49` | behaviour: DragTool::OnKey runs Volume::Box::Box (0x1400fc890) | dragtool.o: DIFF 1 (OnKey gains `RtVolume_Box_Ctor`), IDENTICAL 13. |
| `04e774b65` | behaviour: Pickup::Slot12 = 0x1400f3a70 | pickup.o: ONLY-TIP 1 (Slot12: six loads and `jmp WorkScheduler::Rekey`), IDENTICAL 27. |
| `c94db8f81` | behaviour: PickupRawMaterial ctor 0x1400c9b70 without tests (row 1181) | work_base_actionctor.o: SAME on the ctor. |
| `fc808ecce` | behaviour: the 296b0 / 9de0 citizen test as retail (row 1262) | work_base_cellbst.o: DIFF 1 (same calls, laid out differently). The ten 296b0 instances show DIFF, plus `Set::Clear` / three dtors with the same multiset of calls reordered (inlining layout). |
| `ea945a466` | behaviour: CitizenAI_SortByPriority = 0x14002fe10, the CRT qsort (row 290) | citizenai_candidatelist.o: ONLY-TIP 1 (the sort, now out of line), DIFF 1 (BuildCandidateStateList: the inlined insertion sort is gone). |
| `1a772a260` | behaviour: DragTool::OnKey drops the port-only ToolbarDescription null test | dragtool.o: DIFF 1 |
| `0196d79c0` | docs: rows 290 / 1181 / 1262, the new row 1291, and a comment address fix | docs only |
| `0a3796af2` | merge main | Rebuilt all three targets: EXIT 0, 0 new warnings, unit 1002 / 0 |

On the merged tree:
- castmetric (`src`) is 2.
- `tools/abi_audit.py` reports OK.
- `b3r_session/acceptance.sh` reports 0 lines off target.

The commit messages carry `Co-Authored-By: Claude Opus 5.5`. That is the attribution the session's system reminder gives, and it differs from the brief's line.

## 2. Items

| item | verdict | what |
|---|---|---|
| CitizenAIComponent::OnMessage [15] 0x14010f340 | PORTED | Ignores the message and all its arguments. Runs the sibling CitizenComponent's Update(zero time). Unless the entity is marked for removal (+0x40 bit 10), it then runs its own Update(zero time): an immediate re-decision. |
| CitizenAIComponent::OnAdded [6] 0x14010f880 | PORTED | `JMP CheckAge`. OnAddedPass dispatches it at RegisterEntity, after StartupComponents. |
| HappinessComponent::OnMessage [15] 0x14004a780 | PORTED | Message 0 with `_needsGoods` set: SubtractItem(1) on the first stored good whose RawMaterialDescription flags meet `_goodsFlags`, then town counter +0x170[category].+0xc += 1. |
| Happiness arm, `citizenai_actionctors.cpp:738` | PORTED | Reads `HappinessComponent::TypeId()`. The constant is deleted. |
| The CitizenAI arms | PORTED | Retail has 26 action-ctor readers of [0x1401c70e8], and all map to the tree. 17 are direct arms in Work/*, action_pf_roots_drop and citizenai_handlers. 9 are the callers of `InstallLeadMessage`. Each reads `CitizenAIComponent::TypeId()`, so delivery is retail's own: SendComponentMessage::AbstractImpl finds the component and calls slot 15. |
| `InternalBasePtr::DeletePtr` slot 0x140151ab8 (6 arms) | ALREADY THERE | The slot decompiles as `Object::InternalBasePtr::DeletePtr` (Runtime 0x1800c0150). The arms already called that ported member by name, so only the comments changed. |
| `Volume::Box::Box` slot 0x1401513f8 (dragtool.cpp:205) | PORTED | Calls `RtVolume_Box_Ctor` (0x180011ea0, `src/engine/volume/box.cpp`). |
| Row 1262 | PORTED | Four sites. The `citTid &&` and the null-safe wrapper are dropped. The pointer test stays, because it is retail's. |
| Row 1181 | PORTED | Both slot tests are dropped. Unobservable today. |
| Row 290 | PORTED | Retail's sort is not stable, and the order among ties is observable (see section 4). Ported exactly. |
| Pickup [12] 0x1400f3a70 (the `Pickup.h` banner's PORT GAP) | PORTED | Not in the brief, but it was the one PORT GAP left in this block's directory. Calls `WorkScheduler::Rekey(+0x20, +0x58, ProfessionList +0x30)`. |
| DragTool's `if (descResult != nullptr)` | PORTED | Found while doing the Box item. Retail reads the result unguarded at 0x1400fc816. |

No `PORT GAP` line is left in the block's files.

### LEFT, with the reason

These sit in block files but were not item-listed. Each is a wide behaviour change of its own.
- **CitizenAIComponent::Update keeps the tree's port-only harness.** This covers:
  - the lazy `Startup()` retry while +0x38 is null;
  - the `!citizen || !czEntity || !czDesc || !czChar` bail-out;
  - the `BANISHED_FORCE_WORK` / forester / construction drivers.
- **`CitizenComp_SetupAgeMesh` keeps its two null returns.** It is in `citizenaicomponent.cpp`, and the tests are on the character and the model.

## 3. Save format

No serialized layout changed.

| change | consequence |
|---|---|
| `Pickup::Slot12` | It runs in the WorkScheduler's post-load pass. A save whose pending pickups were filed under another profession now re-files them in the Laborer bucket on load. A resave then writes them there. **The resave golden can move.** If Gate.sav's pickups are already Laborer-keyed, it stays put. The gate's payload diff tells which. |
| Row 1262 | Behaviour-neutral on every registered path, because the type id is never 0. The resave gate is the oracle. |
| Happiness OnMessage | Storage counts and the consumed counters in a save now move on tavern visits. A resave of an untouched save is unaffected. |

## 4. Routes for the coordinator (merged exe)

1. **OnMessage, the new-game route.** Any work sequence completes with the lead message. For example, a laborer hauling a pile builds it with WorkAct_PerformWorkCtor9_c0ad0, and the stack's +0x10 holds the message.
   - Expect citizens to start their next task in the same tick the old one ends. Nothing new is printed.
   - Watch for a crash in `CitizenAIComponent::OnMessage` or `CitizenComponent::Update`. Nothing here guards +0x38 or the entity, as in retail. A null there would mean an upstream gap, not a guard to add.
   - The re-decision runs inside the executor's flush (0x1400c4bb0). It can Clear() and refill the stack that is being flushed. Retail does the same: the flush then DeletePtr's whatever sits at +0x10.
   - Invariants to compare: new game 5694 / 0 and tutorial 5773 / 12.
2. **OnAdded.** On a new game, or a birth, children should be at the small scale from their first frame rather than after their first AI tick. The age-mesh scale comes from `SetupElder`, which the tree's names use for children.
3. **Happiness.** Build a tavern, staff it, and stock ale (Tavern.rsc: `_requireStorage = true`, `_requiredTypes = Alcohol`). When a citizen's leisure visit ends there, the tavern's ale count drops by one, and so does the town's Alcohol stock. A chapel or well visit changes nothing, because it needs no goods.
4. **Box.** Drag a road or field with the cursor off the map edge. The create bar reads retail's empty box on that path, where it used to read stack bytes.
5. **Pickup Slot12.** Run the resave gate (Gate.sav). Read the payload table if the golden moves (section 3).
6. **Row 290.** A trace-oracle comparison of citizen state choices for the same seed. There is no single visible event.

## 5. Needs in other blocks' files

- **`src/game/Action/sendcomponentmessage.cpp`** (new row 1291). The ctor's msg==2 zero-type-id "heal" and AbstractImpl's `[SendCompMsg]` Orchard trace are port-only. The two zero-id arms now hand their real ids, so both can go in a `behaviour:` commit by the file's owner.
- **`docs/retail_bugs.md` candidate.** HappinessComponent::OnMessage re-reads the item's material after `SubtractItem` (0x14004a8e6 `MOV RDX,[R14]`). When the last unit of a stack was spent, the array has been compacted, so the consumed counter is bumped for the next item's category. The port keeps it.

## 6. Rows

| row | change |
|---|---|
| 290 | FIXED in code. The trace-oracle look is owed. |
| 1181 | FIXED |
| 1262 | FIXED in code. The resave gate is owed. |
| 1291 | Added, SUSPECTED: the SendComponentMessage heal and trace. This is the next free number on this branch, so renumber it if another block took 1291. |
