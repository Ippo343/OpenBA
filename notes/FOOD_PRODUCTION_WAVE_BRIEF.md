# Food Production Wave — architecture brief

Scope: Crop Field, Orchard, Pasture, Fishing Dock, Hunting Cabin, Gatherer's Hut.
Recovered from Application-x64.exe (Ghidra), 2026-08-11.

## The six split into TWO families

| Building | Template | Component | Size | Parent |
|---|---|---|---|---|
| Crop Field | `CropField.rsc` | `CropFieldComponent` | 0x68 | ControllerComponent |
| Orchard | `Orchard.rsc` | `OrchardComponent` | 0x68 | ControllerComponent |
| Pasture | `Pasture.rsc` | `PastureComponent` | 0x68 | ControllerComponent |
| Fishing Dock | `FishermansDock.rsc` | `FishingComponent` | 0x58 | **ResourceProduceComponent** |
| Hunting Cabin | `HunterLodge.rsc` | `HuntComponent` | 0x58 | **ResourceProduceComponent** |
| Gatherer's Hut | `GathererShelter.rsc` | **`HerbalistComponent`** | 0x50 | **ResourceProduceComponent** |

**Family A** (0x68, ControllerComponent) = drag/fence-created *areas* with their own
per-class state machine. Orchard is fully ported and is the reference.

**Family B** (0x50–0x58, ResourceProduceComponent) = placed *radius* buildings that
all share ONE state machine. Porting that base covers three of the six at once.

> The Gatherer's Hut is backed by `HerbalistDescription`/`HerbalistComponent` — there is
> no "GathererComponent". Confirmed from `GathererShelter.rsc`'s description list.

## Family B: the shared ResourceProduce state machine

`ResourceProduceComponent`: ctor `0x14012b3d0`, vtable `0x1401643f0` (**22 slots**),
controller vtable `0x1401643a0` (9 slots). Subclasses do **not** replace the controller
vtable, so all of them share one state table.

State table `DAT_1401db920` (16 × 0xf8 entries), count `DAT_1401dc8a0`.
Constructed by `ResourceProduceDescription_StaticInit` (0x14014f910) — which only
runs the array ctor and sets count=0 — and *populated* by
`ResourceProduceDescription_helper_b230` (0x14012b230), reached via
`ResourceProduceDescription_Setup` (0x14014f970).

Registration helpers (the sub_f500/sub_f620 analogues):
- `0x140037dc0` register state — args `(id, enterHandler, …, trans38Handler)`;
  the middle two args are passed through and **ignored**. Writes id→+0x00,
  enter→+0x08, 0→+0x10 (trans30), arg5→+0x18 (trans38), 0→+0x20 (update).
- `0x140037ee0` add transition
- `0x140037f90` add event

### The table (exactly three states)

| State | Enter | trans38 (per-frame) | Transitions | Events |
|---|---|---|---|---|
| **2** (idle) | `helper_b1b0` 0x14012b1b0 | — | →13 `helper_af90`, →14 (null), →3 (null) | — |
| **13** (producing) | `helper_ae60` 0x14012ae60 | `sub_b190` 0x14012b190 | →14 (null), →3 `helper_af70` | 3 → `helper_adf0` |
| **14** (paused/full) | `helper_ad20` 0x14012ad20 | — | →13 (null), →3 (null) | 3 → `helper_acb0` |

- **State 2 enter** picks the next state from the component's `+0x48` byte
  (`produceFlag`): 0 → request state **13**, non-zero → request state **14**.
  It uses the standard "nested Tick if index<0, then node slot+0x08 RequestTransition"
  idiom already modelled by `ControllerLive::RequestStateTransition`.
- **State 13 enter** (if `param_1[9]`==0): resolves the owner entity, calls component
  vtable **+0x90 (CreateWork)**, hands the resulting work to `CitizenAI_impl_ec60(ai, 1, 0, work)`,
  flips the entity flag via `Entity_impl_fc00(entity, 1)`, then tail-calls **+0x98**.
  If `param_1[9]`!=0 it instead requests state 14.
- **State 14 enter** clears work (`FUN_14012ead0`/`FUN_14012eb50`),
  `Entity_impl_fc00(entity, 0)`, then tail-calls **+0xa0**.
- **State 13 trans38** is a one-line forward to component vtable **+0xa8** (produce tick).

### Component vtable slots 18–21 = the per-class production hooks

| Slot | Offset | Meaning | RPC base | Fishing | Hunt | Herbalist |
|---|---|---|---|---|---|---|
| 18 | +0x90 | **CreateWork** | `_purecall` (abstract) | `0x14011efe0` → FishingWork (0x70) | `0x1401226a0` → HuntWork (0x58) | `0x14004b160` → HerbalistWork (0x60) |
| 19 | +0x98 | OnEnterProducing | `0x140006b10` no-op | `0x14011ee80` | *(base)* | *(base)* |
| 20 | +0xa0 | OnExitProducing | `0x140006b10` no-op | `0x14011ee50` | *(base)* | *(base)* |
| 21 | +0xa8 | **ProduceTick** | `0x140006b10` no-op | `0x14011ec10` | *(base)* | *(base)* |

Slots 4–17 are byte-identical across all three subclasses (inherited from the base).
Each subclass overrides only 0,1,2,3 (dtor family + Shutdown) plus the table above.

- **Fishing** is the only one with a real produce tick. `0x14011ec10` scans a circular
  radius (radius from `+0x28` of the Radius description component) around the dock,
  counts tiles whose terrain-flag bit is set against the total in-radius count, and
  stores `count / (total * DAT_140172114)` into `FishingComponent+0x54` (`fishingRate`).
- **Hunt** additionally overrides component slot **9** (`0x140122740`, the per-frame
  Update — a time accumulator scaled by the terrain speed ratio) and its ctor
  (`0x1401228e0`) ORs capability bit **8** into `+0x20` to opt into update ticking.
  Fishing and Herbalist do NOT set bit 8.
- **Herbalist/Gatherer** overrides CreateWork only; all production happens citizen-side
  in HerbalistWork / GatherRawMaterials.

## Ghidra naming traps found in this wave

Read the vtable **bytes**, not Ghidra's names — several are wrong by position:
- `HerbalistComponent_Slot13` @ `0x14012b520` is shared by *every* RPC subclass
  (it is the base's slot 13), not Herbalist-specific.
- `ResourceProduceComponent_Controller_Slot13` @ `0x14012b4a0` is actually the
  **component** vtable slot 3 (Shutdown), not a controller slot.
- `HerbalistWork_Ctor` @ `0x14004b160` is **HerbalistComponent's vtable slot 18**
  (CreateWork), not the HerbalistWork constructor.

## Current port status (start of wave)

- Orchard: COMPLETE and default-on (`BANISHED_LIVE_ORCHARD/GROWTH/FRUIT`).
- Pasture: substantially ported, `BANISHED_LIVE_PASTURE`/`_HERD` opt-in.
- Crop Field / Fishing / Hunt / Gatherer: **no env flags exist at all**. Fishing and
  Hunt have only `Shutdown` ported plus a `COMPONENT_INERT_FACTORY` — i.e. the
  components are created inert and never run.
