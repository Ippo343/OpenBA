# Brief: Register the 145 remaining NullFactory stubs (standalone factory cascade)

Goal: replace the 145 `REGISTER_NULL_FACTORY(...)` stubs in
`code/Game/Object/factory_registrations_bulk.cpp` with real per-type factories +
standalone vtables, so every exe-defined type instantiates (no
`IsDerivedFrom(NULL)` landmine) in standalone mode.

## Reality check (read first)
- **It's 145, not ~70.** `grep -c REGISTER_NULL_FACTORY factory_registrations_bulk.cpp` = 145.
- **None currently block anything.** The in-game preload (`BANISHED_GAME_PRELOAD=1`)
  runs the whole "Game/Game" tree with **0 NullFactory hits** (verified 2026-06-17).
  These stubs are dormant landmines, hit only if some path loads/instantiates that
  exact type. This is completeness / future-gameplay enablement, NOT a HUD unblock.
- **Per-type it's a real RE job, not a one-liner.** Each needs: the exe vtable
  contents, size, base ctor, dtor, GetFileVersion (External types only), and slot→
  method mapping. The good news: **the reimpl method bodies already exist** in
  `code/Game/{Action,Work,Tutorial,Player,Dialog,...}` and **most files document
  their vtable layout in the header comment** (e.g. consumerawmaterial.cpp lines
  9-19). So extraction is mostly "read the file + read the vtable", not full RE.
- **Ghidra is single-state.** Parallel agents must NOT drive Ghidra concurrently.
  Either the main loop pre-extracts each type's vtable/size/version and passes it as
  data, or the Ghidra phase is serialized. Agents then only write C++.
- **Merge chokepoints:** `standalone/address_registry.cpp` + `factory_registrations_bulk.cpp`.
  Agents should RETURN their `RegisterXVtable()` decl/call + the bulk line to delete
  as DATA; the main loop applies these two files centrally (avoids conflicts; no
  worktree needed since each agent only creates its own new `x.cpp` standalone block).

## Family breakdown (145 total)
| Family | count | parent string(s) | base layout |
|---|---|---|---|
| UI dialogs | 57 | UI::DialogController / DialogInstance / Config | Dialog vtable (RISKY — some Runtime-self-registered; overlaps HUD work) |
| Action | 17 | "Action" | Object::Internal, 10-slot (see recipe below) |
| Work | 16 (+6 AutoPickupWork) | "Work"/"AutoPickupWork" | Object::Internal, work vtable |
| Tutorial | 11 | TutorialSetup/Goal/Image | varies |
| Tool | 9 | Tool/DragTool/ToolSetting | varies |
| External resource | 16 | Object::External/Internal | External vtable (GetFileVersion @ +0x18) — MusicList/Scenario pattern |
| misc | ~13 | Element/Component/Particle/etc | varies |

Do it **family by family** (one keystone per family, then fan a wave). Recommend
order: External-resource (proven MusicList/Scenario pattern) → Action → Work →
Tutorial → Tool → misc → UI dialogs LAST (riskiest; do alongside the HUD bring-up).

## VALIDATED RECIPE — Action family (keystone: PlayAnimation)
Worked out 2026-06-17 from Ghidra. `code/Game/Action/playanimation.cpp` already has
the method bodies; only the standalone factory block + registry wiring are missing.

**PlayAnimation_Factory (exe 0x14014a5..):** `MemAlloc(0x40,8)` → `Object::Internal::Internal(this)`
→ set `vtable_PlayAnimation` → zero +0x10,+0x20,+0x30,+0x38. Size = **0x40**.
Parent = "Action". RegisterType @ 0x14014a580.

**Vtable @ 0x140158950 (10 slots, 8 bytes each):**
| slot | off | exe addr | role | standalone source |
|---|---|---|---|---|
| 0 | +0x00 | 0x140001260 | ScalarDtor | shared action dtor (set vtable; if +0x10!=0 InternalBasePtr::DeletePtr; Internal::~Internal; if flag&1 MemFree) |
| 1 | +0x08 | 0x1400011e0 | returns DAT_1401c3df8 | SHARED (resolve the global once) |
| 2 | +0x10 | 0x1400011d0 | GetTypeId → TypeInfo_PlayAnimation | per-type: return GetTypeIdByName("PlayAnimation") |
| 3 | +0x18 | 0x140042480 | Serialize | per-type: `PlayAnimation_Serialize_Impl` (exists) |
| 4 | +0x20 | 0x140042420 | AbstractImpl | per-type: `PlayAnimation_AbstractImpl_Impl` (exists) |
| 5 | +0x28 | 0x140042400 | Execute | per-type: `PlayAnimation_Execute_Impl` (exists); some actions use base 0x1400c4a90 (returns 0) |
| 6 | +0x30 | 0x1400c4b90 | Enter | SHARED nop (`return;`) |
| 7 | +0x38 | 0x1400c4ba0 | Exit | SHARED nop (`return;`) |
| 8 | +0x40 | 0x1400423f0 | CheckEntity | per-type: `PlayAnimation_CheckEntity_Impl` (exists) |
| 9 | +0x48 | 0x1400423b0 | GetTarget | per-type: `PlayAnimation_GetTarget_Impl` (exists) |

NOTE: Internal/Action types have **no GetFileVersion slot** (unlike External). Type id
comes from the registered BaseTypeInfo, surfaced via slot 2. The external-resource
family (MusicList/Scenario) DOES have GetFileVersion @ +0x18 — different layout.

**Shared Action base (build once, e.g. `code/Game/Action/action_base_standalone.cpp`):**
- `Action_ScalarDtor(self, flags, vtableForType)` — generic: set vtable, delete
  Internal IBP @ +0x10 (InternalBasePtr::DeletePtr export), `Object::Internal::~Internal`
  export, MemFree if flags&1. Pass the per-type vtable ptr.
- `Action_Slot1()` → return the DAT_1401c3df8 value (resolve `ResolveAddr(Module::Exe,0x1401c3df8)` then deref; confirm indirection level).
- `Action_Enter()/Action_Exit()` → `return;` (nops).
- `Action_BaseExecute()` → return 0 (for actions whose +0x28 is 0x1400c4a90).

**Per-action standalone block (mirror musiclist.cpp's `#ifdef STANDALONE_EXE`):**
```cpp
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "address_registry.h"
static uint32_t X_GetTypeId(void*){ static uint32_t c=0; if(!c) c=::Object::GetTypeIdByName("X"); return c; }
static void* g_X_Vtable[10] = { /* slot0 dtor thunk */, &Action_Slot1, &X_GetTypeId,
  &X_Serialize_Impl, &X_AbstractImpl_Impl, &X_Execute_Impl, &Action_Enter, &Action_Exit,
  &X_CheckEntity_Impl, &X_GetTarget_Impl };
extern "C" void RegisterXVtable(){ RegisterExeAddr(0x<vtableAddr>, (uintptr_t)g_X_Vtable); }
extern "C" void* X_Factory(){ void* p=fn_MemAlloc(g_pMemAllocSingleton,0x40,8); if(!p)return 0;
  /* Object::Internal::Internal(p); set vtable; zero fields */ return p; }
REGISTER_TYPE(X, "Action", &X_Factory, &X_CreateInPlace);
#endif
```
Then in `address_registry.cpp`: declare + call `RegisterXVtable()` in the vtable-register
block; DELETE `REGISTER_NULL_FACTORY(X, "Action")` from the bulk file.

## Agent-team structure (per family wave)
1. **(main loop, serial)** For each type in the wave, pre-extract from Ghidra: vtable
   addr (often in the file comment already), the 10 slot addrs (`read_memory`), size +
   factory body (`decompile` the RegisterType→Factory), GetFileVersion (External only).
   Build a per-type data record.
2. **(agents, parallel — NO Ghidra)** One agent per type (or small batch). Input: the
   data record + the existing reimpl file + the keystone (PlayAnimation) as template.
   Output (structured): the new `#ifdef STANDALONE_EXE` block to append to `x.cpp`, the
   `RegisterXVtable` decl+call line, the bulk line to delete. Agent writes only `x.cpp`.
3. **(main loop, serial)** Apply registry + bulk edits centrally, build, fix compile
   errors, run the game once (`BANISHED_AUTOSTART=1`) to confirm no regression
   (world still builds, menu still draws). Re-run factory-enum to confirm count drops.

## Status
- Foundation committed (ac29016): preload validated clean, HUD ctor/typeid wiring.
- This brief written 2026-06-17. Action recipe VALIDATED (not yet implemented).

### PROGRESS 2026-06-17 (uncommitted) — Action + Work families DONE. 145 → 107 NullFactory.
- **Action family — 17 concrete types DONE.** Shared base `code/Game/Action/action_base_standalone.{h,cpp}`
  (Action_ScalarDtor / Action_Slot1(null) / Action_BaseExecute(0) / Action_Enter/Exit nop /
  Action_BaseCheckEntity nop / Action_BaseGetTarget(0) / Action_Alloc / Action_InitInPlace). Every vtable
  ground-truthed from Ghidra memory; 10-slot layout (slot0 dtor, slot1 null, slot2 GetTypeId, slot3 Serialize,
  slot4 AbstractImpl, slot5 Execute, slot6/7 Enter/Exit, slot8 CheckEntity, slot9 GetTarget). Each type's
  reimpl Impls reused; base addrs recognized: 0x1400c4a90 Execute=0, 0x1400c4b90/c4ba0 Enter/Exit nop,
  0x1400c4a30 CheckEntity nop, 0x1400c4a40 GetTarget=0.
- **Work family — 21 concrete types DONE** (BuildRoad keystone + 20 via 5-agent team). Shared base
  `code/Game/Work/work_base_standalone.{h,cpp}`. 13-slot layout (slot0 dtor, slot1 null, slot2 GetTypeId,
  slot3 Serialize, slot4 Execute, slot5 0x1400f7540=0, slots 6-11 OnComplete/OnCancel/Cleanup/Validate/
  GetTarget/GetPriority, slot12 0x140006b10 void nop). The factory `MemAlloc(size)` then the type's own
  `*_Ctor_Impl` where it exists, ELSE `Work_GenericCtor` (Internal::Internal + zero[0x10,size) + Work base
  defaults +0x20/+0x28 = -1 + install vtable). KEY GOTCHA: most Work reimpls have NO ctor (DLL mode only
  hooked slots; the exe ctor ran) — the generic ctor rescues them. Missing slots backed by stubs:
  Work_GenericScalarDtor / Work_SerializeNop / Work_SlotRet0(=0). Types still missing real ctor/slots are
  scaffold-level (dormant; FLAG comments left in-file): FireWork, FishingWork, ClearStorage (no Serialize/
  OnCancel either), GatherRawMaterials, LocationWork, TendArea, TradeWork, ForesterWork, HarvestArea,
  HarvestOrchard, HerbalistWork, HuntWork, PastureWork.
- **Validated**: standalone-debug builds + links clean; runtime smoke = 279 addrs registered, factory regs
  invoked, 0 crashes, 0 NullFactory hits, menu loads. (Slot role-mapping trusted from file docs + ground truth;
  not all 21×13 slots exhaustively re-verified — dormant so a mis-ordered slot only matters if dispatched.)
- **Abstract bases NOT done (not landmines — _purecall, never instantiated):** Action, Work, AutoPickupWork,
  Tool, ToolSetting, DragTool, Component, Description, ControllerComponent, ComponentEntity(?), TutorialInstance.
  Give these proper abstract registrations at the very end.
- **NEXT families** (recommended order): External-resource (Tutorial*/TutorialGoal*/TutorialImage*/TutorialSetup*/
  TutorialStep — External vtable + GetFileVersion, MusicList pattern) → Tool (9: BridgeTool/DragPlacementTool/
  EditTool/PlacementTool/RemovalTool/RoadTool/FencedTool/PathTool/RotationSetting) → misc (Tornado/
  FrustumOutlineElement/CaptureBoxElement/Component subtypes/Description) → UI dialogs (57, LAST, with HUD).

### Original status (pre-implementation):
- Build/test: `mingw32-make sa-deploy`; run via `cmd //c start "" reimpl-x64.exe` from
  `C:\GOG Games\Banished` with `BANISHED_AUTOSTART=1` (RTSS/Afterburner must be CLOSED —
  they hook the present path and stall the window/menu).
- Related: PRELOAD_CASCADES_PLAN.md (Phase 1 is the same cascade), feedback_factory_cascade_strategy.
