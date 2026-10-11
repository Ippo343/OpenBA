# SLOTS2A hand-in (PORT GAP hunt, branch `d4-slots2a`, base `b9006081e`, main merged at `ca6cc314c`)

Block SLOTS2A of GAPHUNT_RULES.md: the component slot bodies of Animal, AutoPickup, Character,
Growth, Picking, UI, StateMachine, StatusIcon and Work. Every `PORT GAP` line in those files is
closed (`grep -rn "PORT GAP"` over the nine classes' files returns nothing).

**Static gate only.** The game was never run from this worktree. Every route below is the
coordinator's.

## Commits

| commit | title (short) | B11 verdict (b11pair vs b11base_d4e) |
|---|---|---|
| `55453b927` | behaviour: AnimalComponent::Slot11 = 0x140106dd0 | animalcomponent DIFF 1 (Slot11), IDENTICAL 18; wildanimalcomponent IDENTICAL 17 |
| `6e71688d6` | behaviour: AutoPickupComponent ctor = 0x140044ae0 (isEnabled 1) | ctor + create entries SAME (the metric does not count the stored value) |
| `6bc439b4f` | behaviour: AutoPickupComponent::CommandMask = 0x140044970 | DIFF CommandMask (+0x28 compare) |
| `8e0363fef` | behaviour: AutoPickupComponent::Command = 0x140044930 | DIFF Command (MemFree; +0x28 / +0x30 / +0x38 stores) |
| `842f20fcc` | behaviour: PickingComponent::Startup = 0x140128c30 | ONLY-TIP Startup; its -O2 body is retail's 3 instructions |
| `ce0677b47` | behaviour: PickingComponent::Slot11 = 0x140128ba0 | IDENTICAL 19; Slot11 and OnAdded fold to one `jmp RefreshMesh` |
| `ffe944bf9` | behaviour: CharacterComponent::OnAdded = 0x1401099c0 | DIFF OnAdded (Register, +0x3c / +0x40), DIFF Startup (allocation and Picking stores gone); serialize TU IDENTICAL 12 |
| `9dd7f5931` | behaviour: GrowthComponent::OnRemove = 0x140120560 | ONLY-TIP OnRemove; DoHarvestReset DIFF = GetResourceIndex no longer inlined (same source) |
| `24367a584` | behaviour: GrowthComponent::Slot11 = 0x1401206a0 | DIFF Slot11 (IsDerivedFrom, +0x38, PostUpdate tail) |
| `62a935b2e` | behaviour: UIComponent::Startup = 0x14012dca0 | DIFF Startup (IsDerivedFrom, SetSizeCore, memset inlined) |
| `562ffb003` | behaviour: StateMachineComponent ctor = 0x14012ba30 | DIFF ctor + create entries (+0x48, +0x50..+0x53) |
| `d38bc74e7` | behaviour: StateMachineComponent::Slot11 = 0x14012b7d0 | DIFF RelinkAndResume (retail's shape) |
| `f49dab2bb` | behaviour: StateMachineComponent::RootController = 0x140038630-0x140038800 | DIFF FindTransition; FindState / HasStates SAME (constants); StateMachine::Tick DIFF is GCC's speculative devirtualisation of the root's slots (checked in objdump) |
| `936271ded` | behaviour: StatusIconComponent::Slot11 = 0x14012bf40 | b11pair SAME (it skips tail JMPs and null tests); objdump: Slot11 `ret` -> `jmp ShowAfterLoad`, ShowAfterLoad loses two null tests |
| `acc58f7ac` | behaviour: StatusIconComponent::SetFlag = 0x14012c3f0 (row 1263) | DIFF StatusIcon_Toggle (now `jmp SetFlag`); SetFlag SAME (dropped test not in the metric) |
| `84dbc83ce` | port: WorkComponent::OnAdded 0x14012e6f0 already installed | no TU change |
| `508513bb7` | docs: rows 1263, 1291 | docs |
| `9eb5736ca` | Merge branch 'main' into d4-slots2a | clean, no conflict. Its message is git's default and lacks the two attribution lines (amend is not allowed) |
| (this commit) | docs: this hand-in | docs |

Commits that share a TU (AutoPickup x3, Picking x2, Growth x2, StateMachine x3, StatusIcon x2) were
cut from one built tip by staging hunks; the intermediate states were not built on their own.

## Gate (at the merged tip)

| check | result |
|---|---|
| `standalone-debug`, `standalone-perf`, `unit` (`-j2`) | EXIT 0, EXIT 0, EXIT 0 |
| unit | 1002 / 0 |
| warnings (gate.sh `norm()`, `comm -23` vs the main checkout's `runs/warn_baseline_{debug,perf}.txt`) | 0 new (debug), 0 new (perf) |
| castmetric (`src`) | 2 |
| `tools/abi_audit.py` | OK |
| `b3r_session/acceptance.sh` | 0 line(s) off target |

Setup note: this worktree's `third_party/SDL` submodule was empty; it was checked out at the pinned
`829a65d7` from the main checkout's local copy with a per-command `url.insteadOf` (the shared
`.git/config` is unchanged).

## Items

| item | verdict | note |
|---|---|---|
| AnimalComponent Slot11 0x140106dd0 | PORTED | UpdateBehavior, then PenInPasture via FindComponent<LivestockComponent> while flag bit 22 is clear |
| AutoPickupComponent ctor `isEnabled = 1` | PORTED | 0x140044b0b `MOV byte [RBX+0x28],1` |
| AutoPickupComponent [13] CommandMask 0x140044970 | PORTED | `isEnabled ? 3 : 1` |
| AutoPickupComponent [14] Command 0x140044930 | PORTED | toggle; OFF frees field30 |
| CharacterComponent OnAdded 0x1401099c0 | PORTED | the fold into Startup removed |
| GrowthComponent OnRemove 0x140120560 | PORTED | soil byte + description stamp on the tile |
| GrowthComponent Slot11 0x1401206a0 | PORTED | +0x38 back, PostUpdate |
| PickingComponent Startup 0x140128c30 | PORTED | userData = owner |
| PickingComponent Slot11 0x140128ba0 | PORTED | RefreshMesh on load |
| UIComponent Startup 0x14012dca0 | PORTED | controller byte store sized and zeroed |
| StateMachineComponent Slot11 0x14012b7d0 | PORTED | needed the ctor stores (the file's other PORT GAP, 0x14012ba86 / 0x14012ba8a), also PORTED |
| StateMachineComponent root 0x140038630-0x140038800 | PORTED | read from bytes (no function defined in Ghidra; not created): one empty state, id 0 |
| StatusIconComponent Slot11 0x14012bf40 | PORTED | ShowAfterLoad installed |
| known_port_issues row 1263 (SetFlag 0x14012c3f0) | PORTED / FIXED | guard dropped; the citizen copy StatusIcon_Toggle forwards to SetFlag |
| WorkComponent OnAdded 0x14012e6f0 | ALREADY THERE | body installed and faithful; banner fixed |

Every `PORT GAP` line in the owned files at the base (animalcomponent.cpp:750, AnimalComponent.h:43,
autopickupcomponent.cpp:39 / :82, AutoPickupComponent.h:18 / :39 / :40 / :46,
charactercomponent.cpp:272, CharacterComponent.h:50, growthcomponent.cpp:361 / :624,
GrowthComponent.h:46 / :49, pickingcomponent.cpp:201 / :233, PickingComponent.h:16 / :19 / :39 / :42,
statemachinecomponent.cpp:102 / :492 / :621 / :626, StateMachineComponent.h:24 / :78 / :109,
statusiconcomponent.cpp:12 / :151 / :696, StatusIconComponent.h:23 / :89 / :99, uicomponent.cpp:25,
UIComponent.h:17 / :37, WorkComponent.h:16) is closed by the commits above.
`CitizenAIComponent.h:121` matched the grep but is CAI's file.

## Save-format consequences

- **GrowthComponent::OnRemove writes the map grid**: MapCell +0x2C (the soil byte) and +0x10 (the
  description stamp) are serialized. Any save written after a grown crop / tree / plant is removed
  differs from today's.
- **UIComponent::Startup sizes a serialized array**: a CREATED entity's UIComponent now persists
  N zero bytes (UIDescription +0x20) where it persisted 0. New-game saves grow; a loaded entity keeps
  the count its save stored (Startup does not run on load).
- **GrowthComponent::Slot11 runs one PostUpdate at load**: PostUpdate moves the serialized displayed
  scale (+0x48) one smoothing step toward its target. A load-then-save of a fixture whose growth
  entities are mid-ramp writes different +0x48 floats. This is the likeliest resave-golden mover.
- AnimalComponent::Slot11 rewrites the wander box (+0x38..+0x3E) and flag bit 22 of penned livestock
  from the pasture; both are serialized but normally already hold those values.
- StatusIcon Slot11 sets component flag bit 2 (serialized) for icons with a mask; a saved icon with a
  mask already had it.
- None of the others writes serialized state (Picking, Character, StateMachine ctor / Slot11 / root,
  SetFlag, AutoPickup beyond its already-serialized fields).

## Routes for the coordinator (one per ported body)

1. **AnimalComponent::Slot11**: load a save with a stocked pasture; the animals stay inside the
   pasture fence from the first frame, and young animals show their small size at once.
2. **AutoPickup ctor / CommandMask / Command**: no vanilla route. All seven shipped templates comment
   out the `Dialog/AutoPickup.rsc:enableAutoPickupButton` ControlUI. Only a mod enabling the button
   shows the checkbox (checked = enabled; unchecking drops the collected list).
3. **PickingComponent::Startup**: new game, place a house, click it after it is built; it selects
   the house (unchanged from before, the stand-in in CharacterComponent::Startup is gone).
4. **PickingComponent::Slot11**: load the tutorial save (or Gate.sav) and click a building, a
   citizen and an animal straight away; each opens its dialog. Before, only entities that later
   changed build frame were pickable after a load.
5. **CharacterComponent::OnAdded**: new game; citizens and the starting livestock walk as before.
   A crash in OnAdded (null +0x68 or grid) would mean a character entity without a ModelComponent
   reached RegisterEntity; that is upstream, not a guard to add.
6. **GrowthComponent::OnRemove**: remove a crop field with crops standing, or let foresters cut
   grown trees; the tiles' soil byte changes (visible in the resave payload, MapCell +0x2C / +0x10).
7. **GrowthComponent::Slot11**: load a save with growing crops / orchard / trees; their size is
   right on the first frame, with no ramp-up pop.
8. **UIComponent::Startup**: new game, place a workplace and open its dialog. The DialogInstance
   now receives a non-null controller store of the UIDescription's size (edittool_select.cpp hands
   it over); what the dialog's controllers keep there was not read in this block, so look for any
   panel setting that now persists across a reopen or a save / load, and for a dialog that behaves
   differently with a non-empty store.
9. **StateMachineComponent ctor**: new game boot; the `calls` invariant (5694 / 0) and the
   tutorial (5773 / 12) should not move.
10. **StateMachineComponent::Slot11**: load a save. Immediately press Destroy or Rebuild in a
    building's dialog (DestroyUI posts to `current`); a construction site keeps building; a trading
    post's boat keeps its route. Before, the per-frame handlers and UI events of a loaded machine hit
    the root until its next transition.
11. **StateMachineComponent root**: watch new game, tutorial and the resave load for a crash in
    StateMachine::Tick's FindState climb or in RelinkAndResume (a null parent). Retail climbs the
    same way; a crash means some entity was asked for a state no controller of it owns (upstream).
    `f49dab2bb` is the commit to isolate it with.
12. **StatusIconComponent::Slot11**: load the tutorial save; the status icons (no workers, hungry,
    homeless, ...) show on the first frame instead of after their condition next changes.
13. **SetFlag / Toggle**: new game; citizen icons (hungry, cold) and building icons (no workers)
    appear and clear as before; Options, status icons off, then on.

## Needs in other blocks' files

- `src/game/Component/citizencomponent.cpp:110-131` (not in any wave-1 row): the comment says the
  adapter routes to `StatusIcon_Toggle` "for the sprite setup retail's slot 11 guarantees". Slot 11
  now does that and Toggle forwards to `SetFlag`, so the adapter could call
  `StatusIconComponent::SetFlag` directly and the comment (and its BANISHED_LIVE_* notes) is stale.
- Row 1261 (LUT's `tool_base_tile.cpp` / `bridgetool_calcpath.cpp`): unaffected by the Picking
  commits; RefreshMesh is idempotent, and userData is now seated by Startup before those calls.

## Rows

- **1263** edited: FIXED by `acc58f7ac`.
- **1291** added (appended after 1289; renumber if another block took 1291): the stand-ins left in
  this block's files that are separate retail bodies outside the item list:
  `PickingComponent::RefreshMesh` (0x140128f50) still seats userData when null, null-tests six
  pointers and prints `[PickMesh]`; StateMachineComponent's lazy BodyInit / link arms and Startup's
  `LinkControllersStandalone` (retail Startup 0x14012b6a0 is `LinkControllers`); and
  `AutoPickupComponent::Startup`'s description test (also in row 991).

## Not done, and why

- RefreshMesh, StateMachineComponent::Startup and AutoPickupComponent::Startup are left as they were
  (row 1291): separate retail bodies outside the list, and StateMachine's Startup carries measured
  history (the founding-house and stockpile regressions) that a port must re-read first.
- CharacterComponent::OnRemove keeps its `idx >= 0` guard: a character never added to an EntityList
  still holds -1, and retail's Release would corrupt the free list. The comment now says so.
