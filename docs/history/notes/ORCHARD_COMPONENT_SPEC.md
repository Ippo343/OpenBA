# OrchardComponent — implementation-ready port spec

READ-ONLY recon against `Application-x64.exe` in Ghidra (base 0x140000000). Everything below
was read from decompilation / raw vtable bytes, not from Ghidra's function names — Ghidra's
names for this class are wrong in several places and are called out where they matter.

---

## 0. The single most important correction up front

The task brief listed `0x140009850/60/…/0x140009a70` as "Controller vtable bodies" and
`0x140051dd0/0x140051de0` as "Slot13/Slot14". **That grouping is wrong.** Reading the ctor's
`LEA`s (0x140051f40) and then the raw bytes:

| symbol | address | contents |
|---|---|---|
| `"OrchardComponent"` string | 0x14015a100 | — |
| `"OrchardDescription"` string | 0x14015a118 | — |
| **state-node vtable** (`vtable_OrchardComponent_Controller`) | **0x14015a130** | 9 entries + a null qword at idx9 |
| **component vtable** (`vtable_OrchardComponent`) | **0x14015a180** | 18 entries (idx0..17) |
| `vtable_OrchardDescription` | 0x14015a210 | starts immediately after; already ported (`orcharddescription.cpp`) |

`0x140009850/60/0x140009a70` are component vtable **idx2 / idx1 / idx0**, not controller slots
10/11/12. `0x140051de0` is component **idx3** and `0x140051dd0` is component **idx4**.
Ghidra's `OrchardComponent_Controller_SlotNN` prefix on the 0x140009xxx bodies is a naming
artefact — those functions belong to two *different* vtables.

---

## 1. `OrchardComponent` layout — 0x68 bytes

Confirmed against the ctor (0x140051f40), the dtor (0x140051ed0), `Serialize` (0x140051de0),
`GetGrowth` (0x140051640) and every state handler.

```
+0x00  void**        vtable                       = 0x14015a180
+0x08  ...           Object::Internal base fields (set by Component_Ctor)
+0x10  IBP           owning ComponentEntity        <- read by every state handler
+0x18  External*     OrchardDescription            <- IsDerivedFrom(DAT_1401c46e8) guarded
+0x20  uint32        component flags               <- ctor ORs 0x80
+0x24  ...           (part of the flags word / Component base)
--- embedded StateNode sub-object begins ---
+0x28  void**        node vtable                   = 0x14015a130
+0x30  uint64        0                             (node field, ctor-zeroed)
+0x38  uint64        0                             (node field, ctor-zeroed)
+0x40  void*         node->owner back-pointer      = self   (node this + 0x18)
--- own data ---
+0x48  bool          MASTER ENABLE ("orchard is operating"). Ctor sets 0. Toggled by
                     component vtable idx14 Command(2); read as the gate in state 2's
                     per-frame (0x1400514c0) and in TryHarvest. See §10.1 — an orchard
                     with +0x48 == 0 never leaves state 2. (Earlier drafts of this spec
                     called it "harvestReady"; that name was wrong.)
+0x4c  int32         lastHarvestYear, init 0xFFFFFFFF (-1 sentinel)
+0x50  Slot*         slot array base (stride 0x28)
+0x58  int32         slot count
+0x5c  int32         slot capacity   (written by the 0x140009f80 resizer via arrayPtr+0xc)
+0x60  ExternalPtr   chosen tree-type template (8 bytes) — set by OrchardUI's combo
```
sizeof = 0x68. The ctor zeroes +0x30, +0x38, +0x48, +0x50, +0x58 (8-byte store, so +0x5c too)
and +0x60; it writes +0x40 = self and +0x4c = -1.

**Not initialised by the ctor**: `+0x10` and `+0x18` — those are filled by the generic
`Component` attach path, not here. Everything else in the 0x68 is covered.

The ctor's tail is the standard flag-change notify:
`old=+0x20; new=old|0x80; if (((new^old)&0xffff) && +0x10) { CheckPtr(+0x10); FUN_1400c3470(*(+0x10)); }`
— `0x80 ^ 0` is inside the 0xffff mask, so this notify **does** fire (unlike the dead
`bit 23` variant documented in pasture).

`0x140051ed0` is the **destructor**, not a second ctor: it re-stamps the vtable, `MemFree`s
+0x50, nulls +0x50/+0x58, restores the node vtable to `_purecall`, then tail-jumps into
`Object::Internal::~Internal`. Ghidra labels both `OrchardComponent_Ctor`.

`OrchardComponent_Factory` (0x140009870) = `MemAlloc(0x68, 8)` then ctor. Type registered by
`OrchardComponent_RegisterType` (0x14014b400) as `("OrchardComponent", "ControllerComponent")`
into `DWORD_1401c46b0` — already handled by `standalone/reg_orchard_typeid.cpp`.

### Description fields the component reads (`OrchardDescription`, already ported)
```
+0x20 int32  enum (default 5)      — not read by the component
+0x24 int32  tree spacing X (2)    — PlantOrchard_impl_1070 grid step
+0x28 int32  tree spacing Y (2)    — PlantOrchard_impl_1070 grid step
+0x2c float  per-tend growth gain  — Slot15 case 2 multiplier; DEFAULT 0 (see landmines)
```

---

## 2. Per-slot entry — stride 0x28

```
+0x00 int32   tileIndex        index into *(map+0x80) tile array; record stride 0x30, entity at +8
+0x04 int32   state            0 = empty/unplanted, 1 = live tree, 2 = dead/cleared
+0x08 IBP     ownedEntity      the tree entity this slot believes it owns (staleness key)
+0x10 IBP     component A      result of FUN_1400052c0 on the tree entity
+0x18 IBP     growthComponent  result of FUN_14000a0b0 on the tree entity
                                  ->+0x30 float growth ratio (0..1)
                                  ->+0x34 byte  growth-active flag
+0x20 uint8   tendedThisSeason set by Slot15 case 1; cleared en masse by the harvest event
+0x21..0x27   padding
```

Lifecycle:
* **created** by `PlantOrchard_impl_1070` (state 9 enter): resizes the array to
  `ceil(w/spacingX) * ceil(h/spacingY)`, writes `+0x00 = y*mapStride + x`, nulls the three
  IBPs, sets `+0x04 = 0`.
* **bound** by `Slot15 case 0`: looks the tile entity up, runs both finders; if entity **and**
  both finder results are non-null → `+0x04 = 1`, `+0x08/+0x10/+0x18` filled. Otherwise
  `+0x04 = 2`.
* **tended** by `Slot15 case 1`: `+0x20 = 1`; if `+0x18` and `+0x08` are both set,
  `growth->+0x30 = 0` and `growth->+0x34 = 1` (restart the growth clock).
* **boosted** by `Slot15 case 2`: `helper_9fc0(growthComp, desc->+0x2c)`.
* **invalidated** by `OrchardComponent_ValidateRefs` (0x140051830, **already ported**): any
  state-1 slot whose tile no longer holds the same entity is demoted to state 2 and its three
  IBPs nulled.

---

## 3. Vtables, slot by slot (read from bytes)

### 3.1 Component vtable — 0x14015a180 (18 entries)

| idx | +off | address | body |
|----|------|---------|------|
| 0 | 0x00 | 0x140009a70 | **ScalarDtor** — dtor 0x140051ed0 then optional MemFree |
| 1 | 0x08 | 0x140009860 | GetTypeName — `return DAT_1401c46b8` |
| 2 | 0x10 | 0x140009850 | **GetTypeId** — `return DWORD_1401c46b0` |
| 3 | 0x18 | 0x140051de0 | **Serialize** — PORTED (`OrchardComponent_Shutdown`) |
| 4 | 0x20 | 0x140051dd0 | **Reset/Clear** — `*(self+0x60) = 0` (drops the tree type) |
| 5 | 0x28 | 0x140118a60 | base default (shared with Pasture) |
| 6 | 0x30 | 0x140118820 | base no-op `xor eax,eax; ret` — **OnAdded NOT overridden** |
| 7 | 0x38 | 0x140118830 | base no-op — **not overridden** |
| 8..11 | | 0x140118a70/a80/a90/aa0 | base defaults |
| 12 | 0x60 | 0x140118ab0 | base empty no-op (Pasture overrides this; Orchard does not) |
| 13 | 0x68 | 0x140051c90 | **CanCommand(int cmd) -> byte flags** |
| 14 | 0x70 | 0x140051ba0 | **Command(int cmd)** |
| 15 | 0x78 | 0x140051990 | **OnMessage(self, int msgId, void* srcEntityRef, uint32 p1, uint32 p2)** — p1 (R9D) is the slot index |
| 16 | 0x80 | 0x1400026c0 | **GetDefaultState** — bytes `b8 02 00 00 00 c3` = `MOV EAX,2; RET` → **default state 2** |
| 17 | 0x88 | 0x140004080 | **CreateState** — `LEA RAX,[RCX+0x28]; RET` → the embedded node |

The Slot15 ABI is confirmed against our own `code/Game/Action/sendcomponentmessage.cpp:231`,
which dispatches `vtable+0x78` as `(comp, msgId, srcEntityRef, p1, p2)`.

**There is no per-tick Update on the component vtable.** Orchard does not override idx6/7/12.
All behaviour runs through the embedded state node.

### 3.2 State-node vtable — 0x14015a130 (9 entries + null idx9)

These are *generic* dispatchers over a per-class state table (`DAT_1401cfe50`, stride 0xf8,
count `DAT_1401d0dd0`). They map **exactly** onto the ABI already documented in
`code/Game/Component/controller_state_table.h` — the mechanism is already built.

| idx | +off | address | body |
|----|------|---------|------|
| 0 | 0x00 | 0x140009d10 | **Step(node, int input) -> index / -1** — linear scan for `entry.id == input` |
| 1 | 0x08 | 0x140009d40 | FindTransition(node, stateIdx, int* targetId) — scans `entry+0x28` list, writes -1 on miss |
| 2 | 0x10 | 0x140009da0 | DispatchEvent(node, ?, stateIdx, eventId) — scans `entry+0xb0` list, calls its `+8` handler |
| 3 | 0x18 | 0x140009df0 | **Enter** — tail-calls `entry+0x08` (enterHandler) |
| 4 | 0x20 | 0x140009e20 | **Update** — tail-calls `entry+0x20` (updateHandler) |
| 5 | 0x28 | 0x140009e50 | OnState(node, ?, stateIdx, trIdx) — calls `entry+0x30 + trIdx*0x10` |
| 6 | 0x30 | 0x140009e80 | Transition30 — tail-calls `entry+0x10` |
| 7 | 0x38 | 0x140009eb0 | **Transition38 / per-frame** — tail-calls `entry+0x18` |
| 8 | 0x40 | 0x140009ee0 | `return DAT_1401d0dd8 != 0` (table-initialised flag) |

Every dispatcher does `MOV RCX,[RCX+0x18]; JMP handler`, i.e. handlers are called as
`handler(component, stateMachine)` — RDX passes through untouched. Ghidra renders this as a
one-argument call; it is not. (This is the documented `(...)()` trap.)

The state-table entry layout matches `ControllerLive::StateEntry` verbatim:
```
+0x00 int   id            +0x08 enterHandler   +0x10 trans30Handler
+0x18 trans38Handler      +0x20 updateHandler
+0x28 TransEntry[8] {int targetId; void* handler}   +0xa8 int transitionCount
+0xb0 EventEntry[8] {int eventId; void* handler}    +0xf0 int eventCount   <-- NOT in our header
```
⚠ **Gap in `controller_state_table.h`**: the `+0xb0` event list and its `+0xf0` count are
inside the header's anonymous `_tail`. Orchard is the first controller in the tree that
actually *populates* it (Pasture's registrar never calls the event API). The header needs two
new named fields plus a `TableAddEvent()` before Orchard's table can be built.

---

## 4. State machine — the actual tick flow

Registered once by `OrchardDescription_helper_20f0` (**0x1400520f0**) via three helpers
(all in the shared 0x140009xxx thunk block for this TU):

* `0x140009aa0  AddState(int id, void* enter /*RDX*/, void* update /*R8*/, R9 unused, void* trans38 /*[RSP+0x20]*/)`
* `0x140009bc0  AddTransition(int targetId)` — appends `{targetId, 0}` to the last state's `+0x28` list
* `0x140009c70  AddEvent(int eventId, void* handler)` — appends to the last state's `+0xb0` list

⚠ `AddState`'s 4th register argument (R9) is genuinely uninitialised at every call site
(`in_R9` in the decompile); the real prototype is 4 meaningful params with the 4th passed on
the stack. Do not invent an R9 field.

### The table (6 states)

| id | enter (+0x08) | update (+0x20) | trans38 / per-frame (+0x18) | transitions | events |
|----|---------------|----------------|-----------------------------|-------------|--------|
| 2 (default) | 0x140051530 | — | 0x1400514c0 | 3, 9 | 3 → 0x140051450 |
| 9 PLANT-GRID | 0x140051070 | — | 0x140050f90 | 3, 10, 20 | 5 → 0x1400513e0 |
| 10 IDLE/TEND | 0x140050e30 | 0x140050ba0 | 0x140050c00 | 3, 12, 11, 20 | 5 → 0x1400513e0, 4 → 0x140050900 |
| 12 REPLANT | 0x140050ae0 | 0x1400509b0 | 0x140050a10 | 3, 10, 11, 20 | 5 → 0x1400513e0, 4 → 0x140050900 |
| 11 HARVEST | 0x140050800 | — | 0x140050650 | 3, 10, 20 | 5 → 0x1400513e0 |
| 20 CLEAR | 0x1400504e0 | 0x1400503e0 | 0x140050430 | 3, 9 | — |

State-name labels above are mine, inferred from the bodies; the ids are literal.
State 3 is a shared teardown id that this table never registers a body for (it is the
"stop" target every state can transition to).

### What each body does

* **2 enter (0x140051530)** — `Entity_impl_fc00(workplaceEntity, 0)` = deactivate the
  workplace. Returns AL=0 (walk-stop tail preserved).
* **2 per-frame (0x1400514c0)** — if `+0x48` (master enable) is set, request state **9**.
  So a brand-new orchard sits in state 2 until `+0x48` goes true. See §10.1.
* **2 event 3 (0x140051450)** — unconditional request of state **9**. This is the
  "orchard finished construction / type chosen" kick.
* **9 enter = `PlantOrchard_impl_1070` (0x140051070)** — the grid builder. Activates the
  workplace (`Entity_impl_fc00(+0x48)`, `0x14012fc00`), reads the footprint rect from
  `FUN_140004680(entity)` (+0x28..+0x2e = x,y,w,h shorts), reads spacing from desc+0x24/+0x28,
  resizes the slot array to `cols*rows` via `0x140009f80`, fills `+0x00 = y*mapStride + x` and
  `+0x04 = 0` per slot, calls `CreateNaturalResource_impl_4750` (0x140124750) on the footprint,
  allocates a `PlantOrchard` work (`MemAlloc(0x70)` + `PlantOrchard_Ctor` 0x1400f5140, passing
  `*(self+0x60)` as the tree type) and posts it with `CitizenAI_impl_ec60(0x14012ec60, 1, 0, &work)`.
* **9 per-frame (0x140050f90)** — `ValidateRefs`, then count slots with `+0x04 == 1`. When
  **every** slot is state 1, deactivate the workplace (`FUN_14012eb50`) and request state **10**.
* **10 enter (0x140050e30)** — `ValidateRefs`; if any slot is state 2 (a hole), request state
  **12**; otherwise allocate a `TendArea` work (`MemAlloc(0x60)` + `TendArea_CopyCtor`
  0x1400f5a80, arg 1) and post it.
* **10 update (0x140050ba0)** — if the workplace is active (`FUN_14012ead0`), deactivate it
  (`FUN_14012eb50`). Returns AL=1.
* **10 per-frame (0x140050c00)** — `ValidateRefs`; `TryHarvest` (see below); if any hole →
  state **12**; then a seasonal gate: reads `Time::AsSeconds(gw+0x128 +8) / DAT_14017225c *
  DAT_1401721b8` (the season/month index) and only posts a fresh `TendArea` when the month is
  outside 1..11 minus 9..11 (i.e. the growing window). Otherwise it deactivates the workplace.
* **12 enter = `PlantOrchard_impl_0ae0` (0x140050ae0)** — allocate + post a `PlantOrchard`
  work carrying `*(self+0x60)`. This is the replant-the-gaps state.
* **12 per-frame (0x140050a10)** — `ValidateRefs`; `TryHarvest`; if no slot is state 2 any more,
  request state **10**.
* **11 enter = `HarvestOrchard_impl_0800` (0x140050800)** — allocate `MemAlloc(0x68)` +
  `HarvestOrchard_Ctor` (0x1400ee060), post it, then stamp the footprint cells with
  `FUN_14009f770(map, x, y, w, h, *(self+0x60))` (the same cell-stamp leaf pasture uses).
* **11 per-frame (0x140050650)** — counts state-1 slots vs slots with `+0x20 != 0`
  (tended/harvested). When `tendedCount >= liveCount`, deactivate the workplace and request
  state **10**. This is the harvest-complete latch.
* **20 enter = `ClearArea_impl_04e0` (0x1400504e0)** — posts a `ClearArea` work
  (`MemAlloc(0x78)` + `ClearArea_Ctor` 0x1400e6960) at priority 0x401.
* **20 update (0x1400503e0)** / **per-frame (0x140050430)** — deactivate workplace; when
  `FUN_140118440` (footprint-clear test) passes, request state **9**.
* **event 5 handler (0x1400513e0)**, wired on states 9/10/11/12 — request state **20**. This
  is the "player asked to clear/replace the orchard" path.
* **event 4 handler (0x140050900)**, wired on states 10/12 — request state **11** (harvest),
  clear every slot's `+0x20`, and write `+0x4c = *(int*)*(gw+0x128)` (the current year).

### `TryHarvest` — 0x140051580 (called by the state 10 and 12 per-frame bodies)
```
if (!self->+0x48)                                   return 0;    // master enable off
if (currentYear == self->+0x4c)                     return 0;    // already harvested this year
if (weather/*gw+0x130 +0x18*/ >= _DAT_1401721d4
    && month in [7..11]
    && GetGrowth(self) > 0.0f)                      -> harvest
if (GetGrowth(self) > DAT_140172144 /*ripeness threshold*/) -> harvest
harvest: call the event-4 handler 0x140050900 directly.
```
This is the growth→harvest trigger, and it is the only thing that reads `GetGrowth`
outside the UI.

### Where growth actually comes from
The component never advances growth itself. Each planted tree entity carries its own growth
component (`FUN_14000a0b0` result, field `+0x30` ratio / `+0x34` active flag) which the
world's natural-resource system ticks. `OrchardComponent_GetGrowth` (0x140051640, **ported**)
averages `+0x30` over **all** slots. The only orchard-side write is
`OrchardComponent_helper_9fc0` (0x140049fc0), invoked from Slot15 case 2:
```
desc = growthComp->+0x18 (IsDerivedFrom DAT_1401c4288)
Time t( desc->+0x20 * DAT_140172234 * gain );
growthComp->vtable[+0x48]( growthComp, t );      // advance growth by t
```
with `gain = OrchardDescription->+0x2c`.

### Command / message surface
`CanCommand` (idx13) returns a bit mask per command id, gated on
`hasType = (*(self+0x60) != 0)` throughout:
* cmd 0 — enabled when the workplace is inactive **and** `liveSlots < slotCount`, or when
  `slotCount == 0`; result ANDed with `hasType`.
* cmd 1 — enabled only when the node's current state is 10 **and** `+0x4c != currentYear`.
* cmd 2 — `(+0x48 ? 2 : 0) | hasType` (bit1 = "checked", bit0 = "enabled").
* cmd 3 — enabled when `liveSlots > 0` and the current state is not 20.

`Command` (idx14): cmd0 → post node event **3**, cmd1 → event **4**, cmd3 → event **5**
(all three go through `node->vtable[+0x10]`, i.e. the event dispatcher, with the node's
current state byte at `node+0x51`); cmd2 → toggle `+0x48` and push it to the workplace with
`Entity_impl_fc00`.

---

## 5. The tree-type dependency (+0x60)

`+0x60` is a plain 8-byte `ExternalPtr` to the chosen tree template, written by OrchardUI's
combo box (already wired). It gates the whole feature in three independent places:

1. **`CanCommand` (idx13)** ANDs `hasType` into the result of *every* command id, so with
   `+0x60 == 0` the panel's plant/harvest/clear buttons are all disabled — the player can
   never kick the machine.
2. **`PlantOrchard_Ctor`** is handed `*(self+0x60)` as its tree-type argument (both from state
   9 and state 12). A null there produces a work item with no plantable template.
3. **`FUN_14009f770`** in the harvest state is handed `*(self+0x60)` as the cell-stamp template.

**An untyped orchard is correctly inert in retail** — confirmed, and the inertness comes from
the command gate (path 1), not from a crash guard. Component vtable idx4 (0x140051dd0) exists
precisely to null `+0x60` and put the orchard back into that inert condition.

---

## 6. Dependency closure

### Must be written new (the wave itself)
| address | what | status |
|---|---|---|
| 0x140051f40 | ctor | UNPORTED |
| 0x140051ed0 | dtor | UNPORTED |
| 0x140009870 | Factory | UNPORTED (an inert factory exists; needs promoting) |
| 0x140009a70 / 860 / 850 | vtable idx0/1/2 | UNPORTED |
| 0x140051de0 | idx3 Serialize | **PORTED** (`orchardcomponent.cpp`) |
| 0x140051dd0 | idx4 clear type | UNPORTED (1 line) |
| 0x140051c90 | idx13 CanCommand | UNPORTED |
| 0x140051ba0 | idx14 Command | UNPORTED |
| 0x140051990 | idx15 OnMessage | UNPORTED |
| 0x1400026c0 | idx16 GetDefaultState | trivially reproduced elsewhere in tree |
| 0x140004080 | idx17 CreateState | trivially reproduced elsewhere in tree |
| 0x1400520f0 | state-table registrar | UNPORTED |
| 0x140051530, 0x1400514c0, 0x140051450 | state 2 | UNPORTED |
| 0x140051070, 0x140050f90, 0x1400513e0 | state 9 | UNPORTED |
| 0x140050e30, 0x140050ba0, 0x140050c00, 0x140050900 | state 10 | UNPORTED |
| 0x140050ae0, 0x1400509b0, 0x140050a10 | state 12 | UNPORTED |
| 0x140050800, 0x140050650 | state 11 | UNPORTED |
| 0x1400504e0, 0x1400503e0, 0x140050430 | state 20 | UNPORTED |
| 0x140051580 | TryHarvest | UNPORTED |
| 0x140049fc0 | growth boost helper | UNPORTED |

**24 bodies to write** (excluding the two one-liners already present elsewhere).

### Leaf callees — already in the tree
`0x140051830` ValidateRefs · `0x140051640` GetGrowth · `0x140009f80` ResizeRefArray ·
`0x14000a230` SerializeRefEntries · `0x1401189b0` Component_Serialize · `0x140002340`
ExternalPtr::Serialize · `0x140003050` (workplace finder) · `0x140002e10` · `0x140003ea0` ·
`0x140004680` (position/footprint finder) · `0x14000a170` · `0x1400052c0` · `0x14000a0b0` ·
`0x14012ead0` / `0x14012eb50` (workplace active test / deactivate) · `0x14012fc00`
(ActivateWorkplace) · `0x14012ec60` (CitizenAI post-work) · `0x14009f770` (cell stamp) ·
`0x140124750` (CreateNaturalResource) · `0x1400c3470` (flag notify) · `0x1400a9660`
(StateMachine::Tick) · `0x1400e6960` (ClearArea_Ctor) · `0x1400026c0` · `0x140004080`.

### Leaf callees — NOT in the tree (blockers)
| address | what | note |
|---|---|---|
| **0x1400f5140** | `PlantOrchard_Ctor` | the *work object* ctor. The PlantOrchard **vtable** and all its slot bodies are ported (`code/Game/Work/plantorchard.cpp`, `RegisterPlantOrchardVtable`) but the ctor is not. |
| **0x1400ee060** | `HarvestOrchard_Ctor` | same situation as above (`harvestorchard.cpp` exists). |
| **0x1400f5a80** | `TendArea_CopyCtor` | `code/Game/Work/tendarea.cpp` exists; this ctor is not registered. |
| 0x140118440 | footprint-clear test (state 20) | small; calls `0x14009d860/0x14009d930/0x14009dac0` |
| 0x1400031f0 | 0xf8-byte struct copy used by AddState | trivial |

**Tally: 24 component/state bodies + 5 leaves = 29 functions.** Three of the five leaves are
work-object constructors whose classes are otherwise fully ported, so they are cheap.

---

## 7. Comparison to `PastureComponent`

Both are `ControllerComponent`s of the same 0x68 size with an embedded state node, and the
port can follow the pasture pattern almost exactly.

**Same — reuse directly:**
* Same base component vtable (idx5/8/9/10/11/16 are literally the same addresses; both use
  `GetDefaultState = 2` via 0x1400026c0).
* Same idx13/14/15 command+message triple. `pasturecomponent_states.cpp:800-900`
  (`Pasture_CanCommand` / `Pasture_Command`) is a direct template for orchard's idx13/14, and
  `Pasture_MsgReceive` for idx15 — same ABI, same "post a node event" idiom.
* Same live-promotion scaffolding: `PastureComponent_LiveVtable` + `InstallLive` +
  `LiveEnabled(...)` + `CreateState` returning the embedded node. Copy that block wholesale,
  changing only the node offset.
* Same state-table mechanism and the same `controller_state_table.h` dispatchers.
* Same `Serialize` shape (already ported for orchard).

**Genuinely different:**
1. **Node offset.** Pasture's node is at **+0x48** with the owner back-pointer at +0x60;
   orchard's node is at **+0x28** with the back-pointer at +0x40. `CreateState` must return
   `self + 0x28`. Getting this wrong is silent.
2. **Table size and shape.** Pasture registers 2 states (2, 3) and *no* events. Orchard
   registers **6** states and **7** event entries — and the event list at `entry+0xb0` is a
   part of the table structure our header does not yet model (§3.2). This is the one real new
   piece of infrastructure in the wave.
3. **Pasture overrides idx6/idx7 (OnAdded/OnRemove); orchard overrides neither.** Do not
   copy pasture's `OnAddedVSlot`/`OnRemoveVSlot` wiring — orchard has no per-frame component
   entry point at all, everything is state-driven. (Note pasture's file *names* its idx6/idx7
   overrides `Draw`/`OnCreate` after Ghidra; those names are wrong there too.)
4. **Work posting.** Pasture's states manipulate an animal list in place; orchard's states
   allocate and post four different `Work` subclasses (PlantOrchard, HarvestOrchard, TendArea,
   ClearArea) through `CitizenAI_impl_ec60`. The orchard wave therefore depends on three work
   ctors that pasture never needed.
5. **Seasonality.** Orchard has real calendar logic (`Time::AsSeconds(gw+0x128+8)` converted
   to a month index, plus `gw+0x130 +0x18` weather compared to `_DAT_1401721d4`). Pasture has
   none. Expect the harvest to be invisible outside the modelled months.

---

## 8. Landmines

1. **`OrchardDescription->+0x2c` defaults to 0.** That is the per-tend growth gain fed to
   `helper_9fc0`. If the loaded `Orchard.rsc` leaves it 0, tending advances growth by exactly
   nothing and the orchard will never ripen — and the installed **betterFields.pkm** mod
   overrides Orchard (per the project's own note that `ToolKit/*.rsc` is not what runs). Verify
   the *live* value at runtime before concluding the growth path is broken.
2. **`AddState`'s R9 argument is uninitialised garbage** at all six call sites. The real
   signature has four meaningful parameters, the fourth on the stack.
3. **The `+0xb0` event list is missing from `controller_state_table.h`.** Ports that only fill
   `transitions[]` will build a table where every `AddEvent` target is silently dropped — and
   *all three* player commands (plant / harvest / clear) go through the event dispatcher. The
   orchard would look wired and be completely inert. This is the exact "gated field makes a
   wired controller inert" shape the project has hit before.
4. **The state-node dispatchers are `MOV RCX,[RCX+0x18]; JMP handler`.** Handlers take
   `(component, stateMachine)`. Ghidra shows a single argument. Porting them as one-arg
   functions will read the state machine pointer as garbage — the same two-arg-vtable-slot
   crash already documented for the citizen-death teardown.
5. **Return-AL discipline.** State-2's enter ends `& 0xffffffffffffff00` (AL = 0) while the
   per-frame bodies return AL = 1. Returning 1 from an enter starves every sibling node behind
   the orchard.
6. **Ghidra names in this class are actively misleading**: `OrchardComponent_Ctor` is applied
   to both the ctor and the dtor; `OrchardComponent_Controller_SlotNN` is applied to component
   vtable entries; `OrchardDescription_helper_*` is applied to OrchardComponent state handlers.
   Trust the vtable bytes and the ctor's `LEA`s only.
7. **`0x140051990` (idx15) is referenced exactly once** — from `0x14015a1f8`, i.e. the vtable
   slot. Registering its address without also filling live-vtable slot 15 leaves it dead
   (the documented "ported + registered but unwired" trap).

---

## 9. Addendum — state-table globals and the registration chain (verified)

### Globals
| global | role |
|---|---|
| `0x1401cfe50` | **table base** — 16 entries × 0xf8 bytes (the array size is literal in the static init) |
| `0x1401d0dd0` | **count** — the companion the port needs; init 0, incremented by AddState |
| `0x1401d0dd8` | "table built OK" flag — written once from the registrar's return value; read by node vtable **idx8** |

### Chain
```
OrchardDescription_StaticInit   0x14014b4ab   (CRT static init)
    _eh_vector_constructor_iterator_(0x1401cfe50, 0xf8, 0x10, ctor 0x140002c60, dtor 0x140002c90)
    DAT_1401d0dd0 = 0
    atexit(LAB_140150090)

OrchardDescription_Setup        0x14014b4e4
    DAT_1401d0dd8 = OrchardDescription_helper_20f0()      // returns literal 1

OrchardDescription_helper_20f0  0x1400520f0   <-- THE POPULATE FN to replicate
    0x140009aa0  AddState(int id, void* enter /*RDX*/, void* update /*R8*/, <R9 unused>, void* trans38 /*[RSP+0x20]*/)
    0x140009bc0  AddTransition(int targetId)      -> appends {targetId, 0} at entry+0x28, count entry+0xa8
    0x140009c70  AddEvent(int eventId, void* fn)  -> appends {eventId, fn} at entry+0xb0, count entry+0xf0

FUN_140009ef0                   (the atexit teardown, NOT a registration helper —
                                 zeroes both list counts, resets 0x1401d0dd0, destroys the array)
```

`AddState` writes `+0x00 = id`, `+0x08 = enter`, `+0x10 = 0`, `+0x18 = trans38`, `+0x20 = update`,
`+0xa8 = 0`, `+0xf0 = 0`. **`trans30Handler` (+0x10) is always 0 for orchard** — node vtable idx6
is therefore a permanent no-op on this class.

### The complete table, in registration order

| # | id | enter (+0x08) | update (+0x20) | trans38 (+0x18) | transitions (+0x28) | events (+0xb0) |
|---|----|---------------|----------------|-----------------|---------------------|----------------|
| 0 | **2** | 0x140051530 | — | 0x1400514c0 | 3, 9 | 3 → 0x140051450 |
| 1 | **9** | 0x140051070 | — | 0x140050f90 | 3, 10, 20 | 5 → 0x1400513e0 |
| 2 | **10** | 0x140050e30 | 0x140050ba0 | 0x140050c00 | 3, 12, 11, 20 | 5 → 0x1400513e0, 4 → 0x140050900 |
| 3 | **12** | 0x140050ae0 | 0x1400509b0 | 0x140050a10 | 3, 10, 11, 20 | 5 → 0x1400513e0, 4 → 0x140050900 |
| 4 | **11** | 0x140050800 | — | 0x140050650 | 3, 10, 20 | 5 → 0x1400513e0 |
| 5 | **20** | 0x1400504e0 | 0x1400503e0 | 0x140050430 | 3, 9 | — |

6 states, 18 transition entries, 7 event entries. Default state (vtable idx16) = **2**, so
`Step(2)` must resolve to **index 0**.

### Correction to the primary-vtable listing

The primary vtable has **18** entries, idx0..17 — it ends at `CreateState`.
`0x14015a180 + 0x90 = 0x14015a210` is the start of **`vtable_OrchardDescription`**, which is
already ported (`code/Game/Component/orcharddescription.cpp:19` names that exact address).
So the entries reported as component "[18] 0x140009a10, [19] 0x1400098c0" are
OrchardDescription's ScalarDtor and GetTypeName, not orchard-component slots. Confirmed
independently by `OrchardComponent_vslot18` (0x140009a10) storing `0x14015a210` as its own
`this` vtable.

Likewise the node vtable at 0x14015a130 is **9 live entries (idx0..8) plus a null qword at
idx9**; the null is real (idx8's `return DAT_1401d0dd8 != 0` is the last dispatcher). Treating
idx9 as a callable slot would dispatch through 0.

---

## 10. Handler write-ups

### 10.0 CORRECTION: `sub_9c70` is NOT "AddTransition with a handler"

`sub_9bc0` and `sub_9c70` write to **two different lists in the same entry**. Byte offsets,
derived from the global addresses each one indexes (relative to entry base `0x1401cfe50`; note
`(&DAT_X)[i * 0x3e]` is an `int*` index = 0xf8 bytes):

| helper | list base | stride | count field |
|---|---|---|---|
| `sub_9bc0(id)` | `DAT_1401cfe78` = **entry+0x28** | 0x10 | `DAT_1401cfef8` = **entry+0xa8** |
| `sub_9c70(id, fn)` | `DAT_1401cff00` = **entry+0xb0** | 0x10 | `DAT_1401cff40` = **entry+0xf0** |

Cross-checked against the node vtable consumers:

* node **idx1** (0x140009d40, FindTransition) scans `+0x28` / count `+0xa8` — this is
  `TableFindTransition` in our header.
* node **idx5** (0x140009e50, OnState) calls `entry+0x30 + i*0x10` = `transitions[i].handler`
  — `controller_state_table.h:184`.
* node **idx2** (0x140009da0) scans `+0xb0` / count `+0xf0` and calls `element+0x08`.
  **Our header has no equivalent for idx2 at all** — `+0xb0..+0xf0` sits inside `_tail`.

Correct reading of the table:

* state 2  transitions **{3, 9}**       · event **3 → 0x140051450**
* state 9  transitions {3, 10, 20}      · event **5 → 0x1400513e0**
* state 10 transitions {3, 12, 11, 20}  · events **5 → 0x1400513e0**, **4 → 0x140050900**
* state 12 transitions {3, 10, 11, 20}  · events **5 → 0x1400513e0**, **4 → 0x140050900**
* state 11 transitions {3, 10, 20}      · event **5 → 0x1400513e0**
* state 20 transitions {3, 9}           · no events

Building these as `TableAddTransition(t, 3, handler)` puts an event id into the transition
array. Two silent consequences: `FindTransition` would accept 3/4/5 as legal *transition
targets*, and every panel command (which dispatches through node idx2) would find an empty
event list and do nothing. `controller_state_table.h` needs a named `EventEntry events[8]` at
+0xb0, `int eventCount` at +0xf0, a `TableAddEvent()`, and a node idx2 dispatcher.

Also: `entry+0x10` (`trans30Handler`) is **0 for every orchard state**, so node idx6 is a
permanent no-op on this class.

### 10.0.1 Handler ABI (applies to all 18)

Dispatchers are `MOVSXD RAX,R8D / LEA R8,[table] / IMUL RAX,0xf8 / ADD RAX,R8 /
MOV R8,[RAX+field] / TEST R8,R8 / JZ ret0 / MOV RCX,[RCX+0x18] / JMP R8`.

At handler entry: **RCX = OrchardComponent** (the node's +0x18 backptr), **RDX = the
StateMachine**, **R8 = clobbered** (it held the handler address for the `JMP`), R9 = whatever
the caller left. Every handler is `handler(OrchardComponent*, StateMachine*)`.

**All 18 bodies checked for use of a dropped register argument: none read R8 or R9.** The
update-slot handlers (0x140050ba0, 0x1400509b0, 0x1400503e0) and the state-2 enter
(0x140051530) use only RCX; the rest use RCX and RDX. The state index is consumed by the
dispatcher, not forwarded.

**Return convention**: AL only. Enter handlers end with `& 0xffffffffffffff00` (AL = 0);
per-frame/trans38 handlers return AL = 1. Returning 1 from an enter starves every sibling node
behind this one.

**StateMachine fields** used by the "request a state" idiom (`sm` = RDX): `sm+0x00` = node-chain
head (the object whose vtable idx1 is called), `sm+0x09` = current state id (signed; `< 0` =
not started), `sm+0x0a` = pending state id, `sm+0x0b` = pending state index.

```
want = <id>;
if ((int8_t)sm->+9 < 0) { Time t(0.0f); StateMachine_Tick(sm, t, 0, 0); }      // 0x1400a9660
idx = (*(void**)sm)->vtable[+0x08]( *(void**)sm, (int)(int8_t)sm->+9, &want ); // node idx1
if (idx >= 0) { sm->+0x0b = idx; sm->+0x0a = want; }
```

`want` is an in/out — node idx1 writes -1 into it on a miss. Ten of the 18 handlers are built
around this idiom; factor it into one helper.

---

### 10.1 P1 — the idle to planting path

#### `0x140051530` — state 2 ENTER

* **ABI** `(self, sm)`; `sm` unused.
* **Reads** `self+0x10` (owner entity IBP). **Writes** nothing on the component.
* **Does** finds the workplace component and deactivates it: `SetWorkplaceActive(wp, 0)`.
* **Returns** AL = **0**.
* **Calls** `[0x140152ab0]` IBP::CheckPtr · `0x140002e10` finder — **PORTED** · `0x14012fc00`
  SetWorkplaceActive — **PORTED**.

#### `0x1400514c0` — state 2 TRANS38 (per-frame) — THE TRIGGER FOR THE WHOLE FEATURE

* **ABI** `(self, sm)`.
* **Reads** `self+0x48` **only**. **Writes** `sm+0x0a`, `sm+0x0b`.
* **Does**

```
if (self->+0x48 == 0) return 1;   // an orchard with +0x48 == 0 NEVER leaves state 2
request state 9 via the standard idiom
```

* **Returns** AL = 1.
* **Calls** `[0x140152b00]` `System::Time::Time` · `0x1400a9660` StateMachine_Tick — **PORTED** ·
  indirect node idx1.

**This answers "what triggers the feature".** `+0x48` is the master switch and the ctor sets it
to **0**, so a freshly built orchard sits in state 2 forever. There are exactly two ways out,
and *both* are gated on the tree type:

1. **Component vtable idx14 `Command(2)`** toggles `+0x48` (`+0x48 = (+0x48 == 0)`) and pushes
   it to the workplace. `CanCommand(2)` returns `(+0x48 ? 2 : 0) | hasType` — bit 0 (enabled)
   **is** `hasType = (*(self+0x60) != 0)`, so without a tree type the toggle is disabled.
2. **`Command(0)`** posts node **event 3**, whose handler `0x140051450` requests state 9
   unconditionally. `CanCommand(0)` is likewise ANDed with `hasType`.

For the demo: after promoting the component the orchard will do nothing until either
`Command(2)` or `Command(0)` runs, and neither is reachable until `+0x60` is set. To get
behaviour without driving the panel, set `+0x48 = 1` and `+0x60` directly in the harness.

#### `0x140051070` — state 9 ENTER (`PlantOrchard_impl_1070`) — THIS IS WHAT BUILDS THE SLOT ARRAY

* **ABI** `(self, sm)`; `sm` unused.
* **Reads** `self+0x10` entity · `self+0x18` OrchardDescription (`+0x24` spacingX, `+0x28`
  spacingY; guarded by `IsDerivedFrom(desc, DAT_1401c46e8)`) · `self+0x48` · `self+0x60`.
* **Writes** `self+0x50` base, `self+0x58` count, `self+0x5c` capacity (via the resizer);
  per slot `+0x00` tileIdx, `+0x04 = 0`, `+0x08/+0x10/+0x18` = null IBPs.
* **Does**
  1. `SetWorkplaceActive(wp, self->+0x48)`.
  2. footprint rect from `FUN_140004680(entity)` → `+0x28 x`, `+0x2a y`, `+0x2c w`, `+0x2e h`
     (int16 each).
  3. `cols = (w - 1 + spacingX) / spacingX`, `rows = (h - 1 + spacingY) / spacingY`.
  4. `ResizeRefArray(self+0x50, cols*rows)`.
  5. per cell `slot->+0x00 = (y + r*spacingY) * mapStride + (x + c*spacingX)` where
     `mapStride = *(int*)*(gw+0xf8)`; `slot->+0x04 = 0`; three null IBPs.
  6. `CreateNaturalResource_impl_4750(posComp)`.
  7. `work = new(MemAlloc(0x70)) PlantOrchard(entityIBP, *(self+0x60))`.
  8. scan the entity's component array (`entity+0x30`, count `entity+0x38`) for the component
     whose `GetTypeId()` matches `DWORD_1401c8070`, and post the work with
     `CitizenAI_impl_ec60(comp, 1, 0, &work)`. (Hand-rolled equivalent of `FUN_140003050`,
     which the sibling handlers use — same target.)
* **Returns** AL = 1.
* **Calls** `0x140002e10` PORTED · `0x14012fc00` PORTED · `0x140004680` PORTED ·
  `0x140009f80` ResizeRefArray **PORTED** (`orchardcomponent.cpp`) · `0x140124750`
  CreateNaturalResource PORTED · **`0x1400f5140` `PlantOrchard_Ctor` UNPORTED** ·
  `0x14012ec60` CitizenAI post-work PORTED · `[0x140152b60]` MemAlloc · `[0x140152a90]`
  IBP::Ctor · `[0x140152a98]` IsDerivedFrom.

**Does it need `+0x60` first?** Not structurally — it reads `*(self+0x60)` and hands it straight
to `PlantOrchard_Ctor` with no null test, so the array is built either way. The dependency is
upstream, in the `CanCommand` gate that lets you reach state 9 at all. Runtime `count == 0` is
exactly consistent with the orchard never having left state 2.

#### `0x140050f90` — state 9 TRANS38 (per-frame)

* **ABI** `(self, sm)`.
* **Reads** `self+0x50`/`+0x58` (each slot's `+0x04`), `self+0x10`. **Writes** `sm+0x0a`,
  `sm+0x0b`.
* **Does** `ValidateRefs(self)`; count slots with `+0x04 == 1`; if that count **equals**
  `self+0x58`, deactivate the workplace and request state **10**.
* **Returns** AL = 1.
* **Calls** `0x140051830` ValidateRefs **PORTED** · `0x140003050` FindComponent(WorkPlace)
  PORTED · `0x14012eb50` deactivate PORTED · `0x1400a9660` PORTED · `[0x140152b00]` Time.

**Landmine**: the test is `liveCount == slotCount`, evaluated before any planting has happened.
If the footprint yields `cols*rows == 0` then `0 == 0` and the orchard falls straight through
to state 10 on the first frame, never planting anything. Faithful to retail, but it makes a
zero-area orchard look like a working one.

### 10.2 Who calls the populator

`OrchardDescription_helper_20f0` (0x1400520f0) has exactly one caller,
`OrchardDescription_Setup` (**0x14014b4e0**), which does `DAT_1401d0dd8 = helper_20f0()`.
`OrchardDescription_Setup` itself has no code xrefs — its only data reference is
**0x140152e08**, a slot in a dense `.rdata` array of per-description Setup thunks
(0x140152df8 holds 0x14014b440, 0x14014b480, 0x14014b4e0, 0x14014b500, 0x14014b540,
0x14014b580 …). Retail walks that table once during description registration.

Our tree does not replicate the table; the established convention is a
`<X>Ctrl_PopulateTable()` guarded by `if (t.count != 0) return;` and called from the live
factory (see `ClearAreaCtrl_PopulateTable` at `clearareacomponent_states.cpp:674` and
`clearareacomponent.cpp:275`). `OrchardCtrl_PopulateTable()` on the same pattern is correct —
no new trigger mechanism is needed. `DAT_1401d0dd8` (the "table built" flag read by node idx8)
becomes `table.count != 0`.

### 10.2.1 Correction to P1: `0x140003050` finds **WorkComponent**, not WorkPlaceComponent

P1 labelled `0x140003050` "FindComponent(WorkPlace)". It is
`FindComponent(**WorkComponent**)` — it scans `entity+0x30` (count `entity+0x38`) for the
component whose `GetTypeId()` equals `DWORD_1401c8070`, and our tree already identifies
`0x1401c8070` as **WorkComponent** (`standalone/reg_economy_typeids.cpp:110`) and declares the
finder as `WorkBase_FindComponent_3050 // 0x140003050 type 0x1401c8070`
(`standalone/reg_work_base.cpp:17`). The hand-rolled scans inside `0x140051070` and
`0x140050650` are this same function inlined. The *target object* in my P1 description was
right; the type name was not.

There are therefore **two different components** in play, and the handlers do not confuse them:

| leaf | operates on | tree name | note |
|---|---|---|---|
| `0x140002e10` | the **WorkPlaceComponent** | `AttendanceComponent_GetJobHostComponent` (`reg_component_economy.cpp:111`) | that name is very likely another instance of the documented misnamed-finder trap — it is the object handed to `SetAccepting`. **UNVERIFIED** as a name; its role here is unambiguous. |
| `0x14012fc00` | WorkPlaceComponent | `WorkPlaceComponent_SetAccepting` | `SetAccepting(wpc, char)` |
| `0x140003050` | **WorkComponent** | `WorkBase_FindComponent_3050` | |
| `0x14012ead0` | WorkComponent | `WorkComponent_CheckWorkSlot` | "is a work slot pending?" |
| `0x14012eb50` | WorkComponent | `WorkComponent_RegisterWorkSlot` | ⚠ **our tree names this inconsistently** — `reg_component_economy.cpp:89` calls it `RegisterWorkSlot`, `buildcomponent_state8.cpp:125` binds the same address as `s_stopWorker`. Every orchard use site is `if (CheckWorkSlot) eb50(...)` or "we are done, eb50 then leave the state", i.e. **clear/stop the pending work slot**. Treat `RegisterWorkSlot` as the wrong name. |
| `0x14012ec60` | WorkComponent | `CitizenAI_impl_ec60` | `post(workComp, code, flag, workIBP)` |

So the pattern across all six states is: **`SetAccepting` drives the workplace's willingness to
take staff; `CheckWorkSlot`/`eb50`/`ec60` drive the actual queued work item.**

---

### 10.3 `PlantOrchard_Ctor` — 0x1400f5140 (state 9 and state 12 enter both call it)

* **ABI** `PlantOrchard* __fastcall (void* mem /*RCX*/, InternalBasePtr* entityIBP /*RDX*/,
  ExternalPtr treeType /*R8*/)`. Called as
  `PlantOrchard_Ctor(MemAlloc(0x70, 8), &self->+0x10, *(self+0x60))`. Returns `mem`.
* **Body**
  1. `Work_Ctor25(self, entityIBP)` — `0x1400f72e0`, **PORTED**. Builds the 0x48-byte `Work`
     base (vtable_Work, `+0x10 = entity`, `+0x20 = -1`, `+0x28 = -1`, rest zero).
  2. `self->vtable = vtable_PlantOrchard` (**0x140161910**, already registered by
     `RegisterPlantOrchardVtable`).
  3. `+0x48 = 0` (8 bytes), `+0x50 = 0` (4 bytes), `+0x58 = 0` (8 bytes), `+0x60 = 0` (8 bytes,
     covers count **and** capacity), **`+0x68 = treeType`**.
  4. `FUN_140005440(self->+0x10, &orchardIBP)` — find the **OrchardComponent** on the entity.
     `0x140005440`, **PORTED** (`reg_work_accessor.cpp`, used by `plantorchard.cpp`).
  5. `FUN_140051830(orchardComp)` — `OrchardComponent_ValidateRefs`, **PORTED**.
  6. `FUN_14000a590(self+0x58, orchardComp->+0x58)` — resize the byte array to the orchard's
     **slot count**. `0x14000a590`, **PORTED** (`address_registry.cpp`).
  7. zero every byte: `for (i = 0; i < orchardComp->+0x58; ++i) ((uint8_t*)self->+0x58)[i] = 0;`
     — note the loop re-reads `orchardComp->+0x58` each iteration and re-runs `CheckPtr`.
* **What it does with the tree type**: stores it verbatim at `+0x68` and nothing else. No null
  test, no validation, no refcount. `PlantOrchard::Serialize` (ported) already round-trips
  `+0x68` as an ExternalPtr, which corroborates the field.

#### Does `engine/types/BuildActions.h` match? — **YES on layout, with three notes**

The struct at `BuildActions.h:360-393` is **correct** and its `static_assert`s hold against this
ctor. Do not change the offsets. But:

1. ⚠ **`PlantOrchardAddr::Ctor = 0x1400f50b0` is a *different* constructor.** `0x1400f50b0` is
   the **default/zero-arg** ctor (`Object::Internal::Internal`, vtable_Work, manual field init,
   `+0x68 = 0`) used by the deserialize/factory path. The one state 9 and state 12 call is
   **`0x1400f5140`**, which takes the entity IBP and the tree type and does the array setup.
   The header does not mention it at all. Add it as e.g. `CtorFromEntity = 0x1400f5140` — and
   be aware that anything currently wiring `Ctor` is wiring the wrong one for this wave.
2. `field_68` is commented "ExternalPtr (orchard description resource)". It is the **tree-type
   template** the caller read from `OrchardComponent+0x60` (what OrchardUI's combo box picked),
   not the `OrchardDescription`. Layout unaffected; the comment will mislead.
3. `_pad64[4]` at `+0x64` is **not padding — it is the array capacity.** `FUN_14000a590` treats
   its argument as `{ void* base @+0x00; int count @+0x08; int capacity @+0x0c }` applied at
   `self+0x58`, so `+0x58` base, `+0x60` count, `+0x64` capacity. Same shape as the orchard's
   own `+0x50/+0x58/+0x5c` triple.

#### Transitive closure for state 9 — it bottoms out at ONE function

```
0x140051070  state 9 enter                                   UNPORTED (this wave)
  ├─ 0x140002e10  workplace finder                           PORTED
  ├─ 0x14012fc00  WorkPlaceComponent_SetAccepting            PORTED
  ├─ 0x140004680  footprint/position finder                  PORTED
  ├─ 0x140009f80  OrchardComponent_ResizeRefArray            PORTED (orchardcomponent.cpp)
  ├─ 0x140124750  CreateNaturalResource                      PORTED
  ├─ 0x14012ec60  post work                                  PORTED
  └─ 0x1400f5140  PlantOrchard_Ctor                          UNPORTED  <-- the only gap
       ├─ 0x1400f72e0  Work_Ctor25                           PORTED
       ├─ 0x140161910  vtable_PlantOrchard                    PORTED (registered)
       ├─ 0x140005440  FindComponent(OrchardComponent)       PORTED
       ├─ 0x140051830  OrchardComponent_ValidateRefs         PORTED
       └─ 0x14000a590  byte-array resize                     PORTED
```

**State 9 needs exactly one new function beyond the handler itself.** No further unported
leaves.

---

### 10.4 P2 — states 10 and 12

#### `0x140050e30` — state 10 ENTER

* **ABI** `(self, sm)`.
* **Reads** `self+0x50`/`+0x58` (slot `+0x04`), `self+0x10`. **Writes** `sm+0x0a`, `sm+0x0b`.
* **Does** `ValidateRefs(self)`; count slots with `+0x04 == 2` (holes). If **any** hole →
  request state **12** and return. Otherwise allocate
  `TendArea_CopyCtor(MemAlloc(0x60, 8), &self->+0x10, 1)` and post it to the WorkComponent.
* **Returns** AL = 1 on both paths.
* **Calls** `0x140051830` PORTED · `0x1400a9660` PORTED · `0x140003050` PORTED ·
  **`0x1400f5a80` `TendArea_CopyCtor` UNPORTED** · `0x14012ec60` PORTED · `[0x140152b60]`
  MemAlloc · `[0x140152a90]` IBP::Ctor · `[0x140152b00]` Time.

⚠ Note the hole scan is written as `if ((v != 0) && (v != 1) && (v == 2))` — a compiler
artefact of a switch; it means simply `v == 2`.

#### `0x140050ba0` — state 10 UPDATE

* **ABI** `(self)`; `sm` not used. **Reads** `self+0x10`. **Writes** nothing.
* **Does** find the WorkComponent; `if (CheckWorkSlot(wc)) eb50(wc);`
* **Returns** AL = **1**.
* **Calls** `0x140003050` PORTED · `0x14012ead0` PORTED · `0x14012eb50` PORTED.

#### `0x140050c00` — state 10 TRANS38 (per-frame)

* **ABI** `(self, sm)`.
* **Reads** `self+0x50`/`+0x58`, `self+0x10`, `gw+0x128` (calendar), `gw+0x130` (weather).
  **Writes** `sm+0x0a`, `sm+0x0b`.
* **Does**
  ```
  ValidateRefs(self);
  if (TryHarvest(self, sm)) return 1;              // 0x140051580 — see 10.5
  if (any slot +0x04 == 2) request state 12;       // note: does NOT return here
  wc = FindWorkComponent(entity);
  month  = (int)(Time::AsSeconds(gw->+0x128 + 8) / DAT_14017225c * DAT_1401721b8);
  inSeason = (unsigned)(month - 1) < 11 && !((unsigned)(month - 9) < 3);   // month in [1..8]
  hasWork  = CheckWorkSlot(wc);
  if (hasWork && !inSeason)      eb50(wc);                       // out of season: drop the work
  else if (!hasWork && inSeason) post TendArea (as in state 10 enter);
  ```
* **Returns** AL = 1.
* **Calls** `0x140051830` PORTED · `0x140051580` TryHarvest (this wave) · `0x1400a9660` PORTED ·
  `0x140003050` PORTED · `[0x140151318]` `System::Time::AsSeconds` — dispatch slot, **already
  registered** (`address_registry.cpp:1205`) · `0x14012ead0` PORTED · `0x14012eb50` PORTED ·
  **`0x1400f5a80` `TendArea_CopyCtor` UNPORTED** · `0x14012ec60` PORTED.

⚠ The state-12 request here is **not** followed by a `return` — the seasonal TendArea block
still runs in the same frame. Faithful; do not "tidy" it into an early return.

#### `0x140050ae0` — state 12 ENTER (`PlantOrchard_impl_0ae0`)

* **ABI** `(self, sm)`; `sm` unused.
* **Reads** `self+0x10`, `self+0x60`. **Writes** nothing on the component.
* **Does** `work = PlantOrchard_Ctor(MemAlloc(0x70, 8), &self->+0x10, *(self+0x60));` then
  post it to the WorkComponent. This is the replant-the-gaps state; unlike state 9 it does
  **not** touch the slot array — `PlantOrchard_Ctor` re-sizes its own byte array from the
  orchard's existing slot count.
* **Returns** AL = 1.
* **Calls** `0x140003050` PORTED (twice) · `[0x140152b60]` MemAlloc ·
  **`0x1400f5140` `PlantOrchard_Ctor` UNPORTED** · `[0x140152a90]` IBP::Ctor ·
  `0x14012ec60` PORTED.

#### `0x1400509b0` — state 12 UPDATE

Byte-for-byte the same body as `0x140050ba0` (separate function, same logic): find the
WorkComponent, `if (CheckWorkSlot) eb50()`. Returns AL = 1. All callees PORTED.

#### `0x140050a10` — state 12 TRANS38 (per-frame)

* **ABI** `(self, sm)`. **Reads** `self+0x50`/`+0x58`. **Writes** `sm+0x0a`, `sm+0x0b`.
* **Does**
  ```
  ValidateRefs(self);
  if (TryHarvest(self, sm)) return 1;
  if (self->+0x58 > 0 && any slot +0x04 == 2) return 1;   // holes remain, stay in 12
  request state 10;
  ```
* **Returns** AL = 1.
* **Calls** `0x140051830` PORTED · `0x140051580` TryHarvest (this wave) · `0x1400a9660` PORTED ·
  `[0x140152b00]` Time.

⚠ If `self+0x58 == 0` the scan loop is skipped entirely and it falls straight through to
requesting state 10 — the same zero-slot degeneracy flagged for `0x140050f90`.

---

### 10.5 P3 — state 11 (harvest) and the shared event handlers

#### `0x140051580` — `TryHarvest` (not a table entry; called by 0x140050c00 and 0x140050a10)

* **ABI** `(self, sm)` — same shape as a handler, but an ordinary internal call.
* **Reads** `self+0x48`, `self+0x4c`, `gw+0x128` (calendar), `gw+0x130 +0x18` (weather/temp).
* **Does**
  ```
  if (!self->+0x48)                        return 0;   // master enable off
  if (*(int*)*(gw+0x128) == self->+0x4c)   return 0;   // already harvested this year
  if (*(float*)(*(gw+0x130) + 0x18) >= _DAT_1401721d4) {
      month = (int)(Time::AsSeconds(*(gw+0x128) + 8) / DAT_14017225c * DAT_1401721b8);
      if ((unsigned)(month - 7) < 5 && GetGrowth(self) > 0.0f)  goto harvest;   // month 7..11
  }
  if (GetGrowth(self) > DAT_140172144)     goto harvest;   // ripeness threshold
  return 0;
harvest:
  return Event4Handler(self, sm);          // 0x140050900, returns 1
  ```
* **Returns** AL = 1 if it harvested, 0 otherwise. Both callers use it as
  `if (TryHarvest(...)) return 1;` — i.e. a harvest short-circuits the rest of that frame's
  per-frame body.
* **Calls** `0x140051640` `GetGrowth` — **PORTED** · `[0x140151318]` `Time::AsSeconds` —
  registered · `0x140050900` event-4 handler (this wave).

So there are **two** independent ripening paths: a late-season one (months 7–11, any growth at
all, gated on a weather value) and a year-round one (growth above `DAT_140172144`). The
year-round path means a fully grown orchard harvests regardless of season.

#### `0x140050800` — state 11 ENTER (`HarvestOrchard_impl_0800`)

* **ABI** `(self, sm)`; `sm` unused.
* **Reads** `self+0x10`, `self+0x60`. **Writes** nothing on the component.
* **Does**
  1. `work = HarvestOrchard_Ctor(MemAlloc(0x68, 8), &self->+0x10);` — **note: no tree-type
     argument**, unlike `PlantOrchard_Ctor`.
  2. post it to the WorkComponent.
  3. `pos = FUN_140004680(entity)`; `FUN_14009f770(*(gw+0xf8), pos->+0x28, +0x2a, +0x2c, +0x2e,
     *(self+0x60))` — stamp the footprint cells with the tree type.
* **Returns** AL = 1.
* **Calls** `[0x140152b60]` MemAlloc · **`0x1400ee060` `HarvestOrchard_Ctor` UNPORTED** ·
  `[0x140152a90]` IBP::Ctor · `0x140003050` PORTED · `0x14012ec60` PORTED · `0x140004680`
  PORTED · `0x14009f770` `MapGrid_StampCellRefs_9f770` — **PORTED** (`reg_pasture_live.cpp:88`).

#### `0x140050650` — state 11 TRANS38 (per-frame)

* **ABI** `(self, sm)`. **Reads** `self+0x50`/`+0x58` (slot `+0x04` and slot `+0x20`),
  `self+0x10`. **Writes** `sm+0x0a`, `sm+0x0b`.
* **Does**
  ```
  liveCount = count(slot->+0x04 == 1);
  doneCount = count(slot->+0x20 != 0);
  if (liveCount <= doneCount) {
      wc = <inlined FindComponent(WorkComponent) over entity+0x30 / +0x38>;
      eb50(wc);
      request state 10;
  }
  ```
* **Returns** AL = 1.
* **Calls** `[0x140152a98]` IsDerivedFrom · `[0x140152a90]` IBP::Ctor · `0x14012eb50` PORTED ·
  `0x1400a9660` PORTED · `[0x140152b00]` Time. (The WorkComponent lookup is inlined, not a call
  to `0x140003050`.)

⚠ Third instance of the zero-degenerate: with no slots, `0 <= 0` is true and state 11 exits to
state 10 on its first frame.

#### `0x140050900` — EVENT 4 handler (harvest), wired on states 10 and 12

* **ABI** `(self, sm)`. **Reads** `self+0x58`, `self+0x50`, `gw+0x128`.
  **Writes** every slot's `+0x20 = 0`, and `self->+0x4c = *(int*)*(gw+0x128)` (current year).
* **Does** request state **11** first, *then* clear the per-slot `+0x20` flags, *then* stamp the
  year. Order matters for fidelity: the slot clear happens after the transition request, and
  `+0x4c` is written unconditionally even if the transition request failed.
* **Returns** AL = 1.
* **Calls** `[0x140152b00]` Time · `0x1400a9660` PORTED · indirect node idx1.

The `+0x20` clear is what makes state 11's `liveCount <= doneCount` latch meaningful: the
harvest starts with every slot "not yet picked", and the HarvestOrchard work sets `+0x20` per
slot via component vtable **idx15 case 1** as citizens pick each tree.

#### `0x140051450` — EVENT 3 handler, wired on state 2

Unconditionally requests state **9**. Reads nothing on the component; writes `sm+0x0a`,
`sm+0x0b`. Returns AL = 1. Calls `[0x140152b00]` Time · `0x1400a9660` PORTED · node idx1.
This is the `Command(0)` entry point that bypasses the `+0x48` gate.

#### `0x1400513e0` — EVENT 5 handler, wired on states 9, 10, 11 and 12

Unconditionally requests state **20** (0x14). Same shape and callees as `0x140051450`.
Returns AL = 1. This is the `Command(3)` clear/replace path.

---

### 10.6 Remaining unported leaves after P1–P3

| address | what | needed by |
|---|---|---|
| `0x1400f5140` | `PlantOrchard_Ctor` | state 9 enter, state 12 enter |
| `0x1400f5a80` | `TendArea_CopyCtor` | state 10 enter, state 10 per-frame |
| `0x1400ee060` | `HarvestOrchard_Ctor` | state 11 enter |

That is the complete set for the plant → grow → harvest loop. All three are constructors of
work classes whose vtables and slot bodies are already ported. `0x140118440` and `0x1400031f0`
remain unported but are only reachable from the state-20 (clear-area) path, which is
deprioritised.
