# Component Reimpl Audit — 2026-03-29

Classifies every hooked vtable slot across all component reimpl files.

**Legend:**
- **CLEAN** = delegates to `code/Game/Component/` or has full inline reimpl (no trampoline)
- **PASSTHROUGH** = calls `g_hook.trampoline` (needs conversion)
- **NOT-HOOKED** = vtable slot too small or not represented

## Summary

| Status | Slot Count |
|--------|-----------|
| CLEAN/REIMPL | ~72 |
| PASSTHROUGH | ~55 |
| NOT-HOOKED | ~5 |

**Files with NO reimpl file:** ResourceProduceComponent (logic shared via parent class calls)
**Files with shortened names:** `animal.cpp` (AnimalComponent), `growth.cpp` (GrowthComponent)

---

## Tier 3 — Passive Components (target for Phase 1)

### Batch A

| Component | Reimpl File | Slots | CLEAN | PASS | Notes |
|-----------|------------|-------|-------|------|-------|
| AbsorbResource | absorbresourcecomponent.cpp | 1 | 1 | 0 | Shutdown only |
| AmbientEmitter | ambientemittercomponent.cpp | 2 | 0 | 2 | Shutdown + Draw |
| AmbientEnvironment | ambientenvironmentcomponent.cpp | 1 | 0 | 1 | Update (222 instr, audio deps) |
| Attendance | attendancecomponent.cpp | 1 | 1 | 0 | Update done |
| AutoPickup | autopickupcomponent.cpp | 2 | 0 | 2 | Shutdown + Serialize |
| Boat | boatcomponent.cpp | 1 | 0 | 1 | Shutdown (95 instr, Vector3 arrays) |
| Cemetery | cemeterycomponent.cpp | 3 | 2 | 1 | Draw still passthrough |
| ClearArea | clearareacomponent.cpp | 1 | 1 | 0 | Shutdown done |
| ClearResource | clearresourcecomponent.cpp | 1 | 1 | 0 | Shutdown done |
| Decal | decalcomponent.cpp | 4 | 0 | 4 | All 4 passthrough |

### Batch B

| Component | Reimpl File | Slots | CLEAN | PASS | Notes |
|-----------|------------|-------|-------|------|-------|
| Destroy | destroycomponent.cpp | 2 | 2 | 0 | Both done |
| Disease | diseasecomponent.cpp | 1 | 1 | 0 | Update thunk done |
| Fire | firecomponent.cpp | 1 | 1 | 0 | Full reimpl |
| Floor | floorcomponent.cpp | 3 | 2 | 1 | Draw passthrough |
| Fruit | fruitcomponent.cpp | 1 | 1 | 0 | Full reimpl (temp curve) |
| Happiness | happinesscomponent.cpp | 2 | 0 | 2 | Shutdown + OnCreate |
| Healer | healercomponent.cpp | 2 | 2 | 0 | Both done |
| Herbalist | herbalistcomponent.cpp | 1 | 0 | 1 | Shutdown passthrough |
| Highlight | highlightcomponent.cpp | 1 | 0 | 1 | Shutdown passthrough |
| Immigration | immigrationcomponent.cpp | 1 | 0 | 1 | Shutdown (68 instr, entity arrays) |

### Batch C

| Component | Reimpl File | Slots | CLEAN | PASS | Notes |
|-----------|------------|-------|-------|------|-------|
| Mine | minecomponent.cpp | 1 | 1 | 0 | Shutdown done |
| MultiObject | multiobjectcomponent.cpp | 2 | 1 | 1 | OnCreate done, Shutdown passthrough |
| NaturalResource | naturalresourcecomponent.cpp | 2 | 1 | 1 | Shutdown done, OnRemove passthrough |
| Nomad | nomadcomponent.cpp | 2 | 0 | 2 | Shutdown + OnCreate passthrough |
| Orchard | orchardcomponent.cpp | 1 | 0 | 1 | Shutdown passthrough (entity refs) |
| Particle | particlecomponent.cpp | 6 | 0 | 6 | ALL passthrough (graphics heavy) |
| Picking | pickingcomponent.cpp | 2 | 0 | 2 | Shutdown + OnCreate passthrough |
| Population | populationcomponent.cpp | 1 | 1 | 0 | Update full reimpl |
| Quarry | quarrycomponent.cpp | 1 | 1 | 0 | Shutdown done |
| Spawn | spawncomponent.cpp | 1 | 1 | 0 | Update full reimpl |
| Terraform | terraformcomponent.cpp | 5 | 5 | 0 | Shutdown + OnRemove (Ghidra mislabels it OnCreate) + msg-4 deformation trio |
| Tombstone | tombstonecomponent.cpp | 3 | 3 | 0 | ALL done |
| Tornado | tornadocomponent.cpp | 3 | 0 | 3 | ALL passthrough (complex) |
| Tracker | trackercomponent.cpp | 1 | 0 | 1 | Shutdown passthrough |
| Well | wellcomponent.cpp | 2 | 2 | 0 | Both done |
| Zone | zonecomponent.cpp | 3 | 1 | 2 | Shutdown done, Draw+OnCreate passthrough |

---

## Tier 2 — Economy/Building Components (target for Phase 2)

| Component | Reimpl File | Slots | CLEAN | PASS | Notes |
|-----------|------------|-------|-------|------|-------|
| ConsumeProduce | consumeproducecomponent.cpp | 2 | 1 | 1 | Shutdown done, Serialize passthrough |
| CropField | cropfieldcomponent.cpp | 1 | 0 | 1 | Shutdown passthrough (entity refs) |
| Fishing | fishingcomponent.cpp | 1 | 1 | 0 | Shutdown done |
| Forester | forestercomponent.cpp | 1 | 1 | 0 | Shutdown done |
| Hunt | huntcomponent.cpp | 1 | 1 | 0 | Update full reimpl |
| Livestock | livestockcomponent.cpp | 1 | 1 | 0 | Update full reimpl |
| Market | marketcomponent.cpp | 1 | 1 | 0 | Shutdown done |
| MarketLocation | marketlocationcomponent.cpp | 2 | 2 | 0 | Both done |
| Pasture | pasturecomponent.cpp | 3 | 0 | 3 | ALL passthrough |
| RawMaterial | rawmaterialcomponent.cpp | 4 | 3 | 1 | Draw passthrough |
| Residence | residencecomponent.cpp | 3 | 0 | 3 | ALL passthrough |
| StorageComponent | storagecomponent.cpp | 2+2h | 2h | 2 | Shutdown+Draw passthrough, 2 helper reimpl |
| StorageDisplay | storagedisplaycomponent.cpp | 5 | 2 | 3 | Draw+Update+OnUpdate2 passthrough |
| StorageLocation | storagelocationcomponent.cpp | 2 | 2 | 0 | Both done |
| Trade | tradecomponent.cpp | 3 | 2 | 1 | Shutdown passthrough |
| Work | workcomponent.cpp | 3 | 3 | 0 | ALL done |
| WorkPlace | workplacecomponent.cpp | 2 | 0 | 2 | Both passthrough |

---

## Tier 1 — Active Update Components (target for Phase 3)

| Component | Reimpl File | Slots | CLEAN | PASS | Notes |
|-----------|------------|-------|-------|------|-------|
| AnimalComponent | animal.cpp | 1 | 0 | 1 | Update passthrough (452 instr, pathfinding) |
| BuildComponent | buildcomponent.cpp | 3 | 1 | 2 | OnCreate done, Shutdown+Draw passthrough |
| CharacterComponent | charactercomponent.cpp | 1 | 1 | 0 | Update done |
| CitizenAI | citizenaicomponent.cpp | 1 | 1 | 0 | Update full reimpl |
| CitizenComponent | citizencomponent.cpp | 2 | 1 | 1 | Update passthrough (711 instr!) |
| GrowthComponent | growth.cpp | 1 | 0 | 1 | Update passthrough (seasonal growth logic) |
| HerdComponent | herdcomponent.cpp | 1 | 1 | 0 | Update full reimpl |
| InteractComponent | interactcomponent.cpp | 2 | 0 | 2 | Shutdown+Draw passthrough |
| ModelComponent | modelcomponent.cpp | 5 | 0 | 5 | ALL passthrough (Graphics heavy) |
| StatusIcon | statusiconcomponent.cpp | 4 | 0 | 4 | ALL passthrough |
| **MapComponent** | mapcomponent.cpp | 3 | **3** | 0 | **ALL CLEAN** |
| **StateMachine** | statemachinecomponent.cpp | 7 | **7** | 0 | **ALL CLEAN** |
| **UIComponent** | uicomponent.cpp | 2 | **2** | 0 | **ALL CLEAN** (code/Game/ has no-ops) |

---

## Actual Remaining Passthrough Count by Priority

**Tier 3:** ~33 passthrough slots across 30 components
**Tier 2:** ~14 passthrough slots across 17 components
**Tier 1:** ~14 passthrough slots across 13 components

**Grand total: ~61 passthrough slots to convert**

Plus: all non-hooked vtable slots need code/Game/ implementations for standalone exe goal (even if they're no-ops or too-small-to-hook).
