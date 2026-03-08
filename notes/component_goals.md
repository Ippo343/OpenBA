# Component/Engine Function Reimplementation Goals

Exe-resolved fn_* pointers that are engine/component functions, not UI logic.
These belong in `code/Game/Component/`, `code/Game/Entity/`, `code/Game/Player/`, etc.

Currently called from UI code but are game-logic functions.

---

## DestroyComponent methods

- [ ] `fn_DestroyComponent_CanReclaim` — DestroyComponent method
- [ ] `fn_DestroyComponent_CanRebuild` — DestroyComponent method
- [ ] `fn_DestroyComponent_Reclaim` — DestroyComponent method
- [ ] `fn_DestroyComponent_Rebuild` — DestroyComponent method
- [ ] `fn_DestroyComponent_Destroy` — DestroyComponent method

## WorkplaceComponent / CitizenList (ProfessionList)

- [ ] `fn_WorkplaceComponent_Toggle` — toggle workplace enabled/disabled
- [ ] `fn_CitizenList_RequestWorker` — request a worker for a profession slot
- [ ] `fn_CitizenList_AssignWorker` — assign a worker to a profession slot
- [ ] `fn_CitizenList_RemoveWorker` — remove a worker from a profession slot

## World/Entity queries

- [ ] `fn_WorldGrid_FindEntity` — world grid spatial lookup
- [ ] `fn_EntityTrackingSet_Remove` — entity tracking BST removal
- [ ] `fn_CountOccupiedBuildTiles` — count occupied tiles for a building
- [ ] `fn_BuildComplete` — mark build as complete
- [ ] `fn_GetDescPage` — get description page for an entity

## Component calculations

- [ ] `fn_RadiusCalc` — radius component tile calculation
- [ ] `fn_RadiusDisplay_Remove` — radius display cleanup
- [ ] `fn_OrchardGetGrowth` — orchard growth percentage

## Player tools

- [ ] `fn_EditTool_SelectEntity` — select entity in edit tool
- [x] `fn_FindComponent_Character` — already inlined via FindComponent template
