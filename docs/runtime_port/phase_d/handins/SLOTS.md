# Phase D SLOTS -- hand-in (rows 1202 / 1203 / 1204, the decal race; new rows 1270-1272)

Branch `d0-slots`, base 657b50266, `main` merged in at the end (clean merge). Static gate only (the
game was never run): debug / perf / unit builds exit 0 after the merge, 0 new warnings against
`b9_session/runs/warn_baseline_{debug,perf}.txt`, unit 920 / 0, castmetric 2.

**B11 base.** `b9_session/runs/b11base/perf` is NOT main's B9-close objects: against it 30+ untouched
TUs DIFF (e.g. `zonecomponent.o`'s `CemeteryListErase_Reimpl` -> `rtcoll::ArrayRemove8`, a B9 RT2
change). Every verdict below is against objects built from a `git archive 657b50266` export:
`.claude/worktrees/d0-slots-b11base/standalone/build-perf/obj` (not a git worktree; kept for the
coordinator as the Phase D base, delete when done). Against it exactly six TUs differ at the tip:
decalcomponent, mapcomponent, keystone_a_map, starttown, action_base_cnr, rawmaterialcomponent.

## The decal "material-load race" -- row 1270, `03f74d3d4`

**There is no race in today's tree; the root cause is a July driver's leftover.** The slot-8 retry
was written in 2ac870693 / 85845b95b while slot 6 was not dispatched in the standalone (a per-frame
`DecalComponent_DrainDraw`, then the draw-flag bucketing). Its comment "a still-loading material
leaves decalId == -1" was never measured. Read today:
- the material array is resolved synchronously when the DecalDescription loads (`DecalDescription::
  Serialize` -> `SerializeExternalArray` -> `ExternalPtrSerializeBody` -> `ExternalFactory::FindOrLoad`),
  long before any entity exists;
- `GameUpdateLoop::_singleton` is set at the top of Load and WorldGeometry at step 23, before worldgen,
  start town or a save's entities;
- slot 6 is dispatched by `ComponentEntity::OnAddedPass` at `EntityList::RegisterEntity` (bit 0 of the
  ctor's 0xc3), independent of any bucketing.

What hid it: `OnAdded` had six early returns retail lacks (singleton, MapComponent, null description /
failed cast, world geometry, `_materials`, `_uvRects`), each leaving `decalId == -1` silently so the
retry looked necessary. Retail 0x14011b560 tests only the ModelComponent; the one bail is AddDecal's
own `material == nullptr` (0x1400b0dbb). Fix: the six returns dropped (row 1270, FIXED). If a decal is
missing on a route now, the faithful body faults or AddDecal returns -1 at the cause.

B11: decalcomponent.o DIFF 2 IDENTICAL 24 ONLY-BASE 1 SAME 1 at this commit (the dropped compares, the
`OnAdded.part.0` split folded back).

## Row 1202 -- DecalComponent slot 4, `aa942c0cf` -- FIXED

- Slot 4 = retail's Startup (0x14011b7d0): `decalScale = desc->_initialAlpha`, unguarded. The free
  `DecalComponent_Serialize` and `decalcomponent_funcs.h` are deleted.
- Slot 8 is Component's again: `DecalComponent::Tick` (the retry) and `DecalComponent_SetDrawFlag` are
  deleted, and `OnAdded`'s `decalScale == 0` seed with them.
- **Load question settled.** Retail Startup runs only from `StartupComponents` (description ctor,
  0x1400c38bc) and from `Persist` for a component the save LACKS (0x1400c3343). A loaded decal keeps its
  saved decalScale.
- The port's completion ramp (`DecalComponent_MarkOwnerBuilt`, slot 12 still a PORT GAP) re-bucketed
  through the retry. It now runs retail's msg-1 arm (0x14011b769-0x14011b79c): only when decalScale != 1.0
  and decalId >= 0, then OnRemove, 1.0, OnAdded. Start town calls it after RegisterEntity, so the 0.5
  decal is rebuilt at 1.0.
- B11: decalcomponent.o DIFF 3 IDENTICAL 21 ONLY-BASE 3 SAME 1. Startup gains the IsDerivedFrom call and
  the +0x30 store; MarkOwnerBuilt loses the flag store; Tick, `DecalComponent_Serialize` and
  `OnAdded.part.0` are ONLY-BASE.

## Row 1203 -- MapComponent slot 6 and the 0x100000 story, `211d34c68` -- FIXED

- Slot 6 = retail's 0x140124370 (`if (desc->_claimOnAdd) AddToMap()`). Slot 12 = retail's 0x1401243c0
  (`if (msg == 1) AddToMap()`). Before this, slot 12 ran only the occupancy half, so **building minimap
  dots were never painted**. Slot 7 loses its duplicated minimap clear.
- `MapComponent_Draw`, `MapComponent_PaintMinimapRect`, `MapComponent_ApplyOccupancy_41b0` and
  `mapcomponent_funcs.h` are deleted. AddToMap's cast is now retail's unguarded IsDerivedFrom.
- Every 0x100000 site is read. On a MapComponent the bit is now set only by AddToMap and cleared only by
  RemoveFromMap, as retail. The other `|= 0x100000` hits in work / action / pasture / disease files are a
  different component's own bit 20.
- **Removed collapses:**
  - `StartTown_MarkBuildingCells` with both callers (start town, upgrade clone) and its pre-set.
  - `StartTown_PlaceResourceCell`'s `StartTown_MarkResourceCellGrid` call and pre-set (function deleted).
  - `NaturalResourceComponent::RemoveResource`'s pile claim and bit.
- **Who claims now.** Read from `ToolKit/resource/Template`:
  - All 26 NaturalResource and all 40 RawMaterial templates set `_addOnCreate`, so they claim at
    RegisterEntity, after the rect write.
  - Every building template has `_addOnCreate = false` and claims at ClearArea state 2 (event 1).
  - Every start-town structure (WoodHouse, StorageBarn, StorageYard, StorageCart) carries a
    ClearAreaDescription. Inside the instant-build bracket the 1 -> 8 -> 2 chain enters ClearArea state 2.
  - StorageYard has no BuildComponent: it claims when its SM reaches 2 (the existing fallback stamp), at
    the latest on the first tick.
- **Retail behaviour now visible:**
  - A tree's or pile's cell carries dc20's `_cellType` stamp (e.g. Obstacle), not starttown's classified
    type.
  - dc20's stale-occupant eviction runs when a start-town building claims over standing resources.
  - The rect write in MarkBuildingCells duplicated the pre-register one.
- B11:
  - mapcomponent.o DIFF 3 IDENTICAL 16 ONLY-BASE 4 SAME 2.
  - keystone_a_map.o DIFF 1 IDENTICAL 13 (AddToMap block order, the null-safe test gone).
  - starttown.o DIFF 4 IDENTICAL 40 ONLY-BASE 2 SAME 15 (MarkBuildingCells.part.0 and ClassifyCell are
    ONLY-BASE; ClassifyCell is inlined into its last caller).
  - action_base_cnr.o DIFF 1 IDENTICAL 14.
  - zonecomponent.o and clearareacomponent_states.o IDENTICAL (comments only).
- Left: RemoveFromMap is still the tree's 0x140124750 body (`StartTown_UnmarkResourceCellGrid`, dee0's
  semantics). `CreateNaturalResource_impl_4750` is the faithful second body (section 6 naming row,
  unchanged). `MapComponent::OnRemove` keeps its singleton / minimap null tests, which are not this row's.

## Row 1204 -- RawMaterialComponent slot 9, `033343ce3` -- FIXED

- Slot 9 = retail's 0x140129130. The free `RawMaterialComponent_Update`, its `_singleton == nullptr`
  return and `rawmaterialcomponent_funcs.h` are deleted. GameUpdateLoop+0xe8 is read before the entity
  test, as retail.
- A pile unclaimed past Time(3600) is now removed.
- B11: rawmaterialcomponent.o DIFF 1 IDENTICAL 19 ONLY-BASE 2 ONLY-TIP 1.

## New rows left open

- **1271:** `DecalComponent::Slot11` is retail's `JMP [RAX+0x30]` (OnAdded). `EntityList_PostLoad_c4730`
  calls slot 11 on every loaded component, so retail rebuilds every foundation decal after a load. The
  tree's slot is inert, so **a loaded game has no foundation decals**. This predates this block and was
  left out of scope; installing it is one line.
- **1272:** `Grid_AllEmpty`'s `occ != selfEntity` skip lost its only reason with MarkBuildingCells. Drop
  it after the upgrade / rebuild route is confirmed.

## Routes for the coordinator, and what to look for

1. **New game (`sa-run`)**
   - `[startTown::struct]` lines show "no fallback needed" for houses, and the StorageYard fallback still
     reaches state 2.
   - The minimap shows building dots and tree dots.
   - `[dc20Census:worldgen-seed]` evictions and `skipSceneTeardown` are at baseline. A rise is start-town
     buildings evicting resources under them; if Scene::Clean then faults, that is row 1203's eviction.
   - Foundation decals are visible and opaque on the finished start town.
   - No crash in `DecalComponent::OnAdded`.
2. **Tutorial:** the same checks, plus placing a building: its foundation decal is translucent (0.5)
   until construction completes, then opaque.
3. **Placement / tool_placement:**
   - A placed building gets its minimap dot and becomes cell-pickable only when its ClearArea finishes
     (retail).
   - Citizens path around trees: tree cells now carry Obstacle.
   - Foresters and gatherers still find trees (`MapCell::occupant` set at RegisterEntity).
4. **Orchard:** the orchard bind reads `occupant` of the planted tree, which AddToMap claims at
   RegisterEntity.
5. **Stress (`sa-stress`):** `[TownCensus]` raw-material count may drop as abandoned piles expire after
   one game year. Watch for a crash in `MarkForRemoval` from `RawMaterialComponent::Update`.
6. **Resave gate:** it should stay at golden. Loaded MapComponents carry their bit and the grid from the
   save, and loaded decals are not re-started. A fixture pile past Time(3600) inside the resave window
   would be removed and would show as a DIFF.
7. **Upgrade / rebuild:** the replacement claims its cells at ClearArea state 2, not at spawn. It should
   still reach BuildComponent state 1 (row 1272's skip is now inert).
8. **Disaster route:** the destroy state-6 scorch re-add (`decal->OnRemove(); decal->OnAdded()`) and the
   quarry deform resume still rebuild the decal.
