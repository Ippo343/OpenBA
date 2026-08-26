// Component registration wave — batch economy
//
// Registers the reimplemented exe-internal helper addresses for the economy
// component batch so that the guarded ResolveAddr(Module::Exe, ...) call sites in
// code/Game/Component/ resolve to our clean C++ bodies instead of returning null.
//
// The reimpl bodies live (non-static) in their owning component .cpp files; we
// extern-declare them here and take their address. The integrator wires the
// RegisterComponentBatch_economy() call into standalone/address_registry.cpp.
#include "resolve.h"
#include <cstdint>

// ---- Reimpl bodies (defined in the component .cpp files) ----
extern void     RawMaterialComponent_RegisterRawMaterial(void* worldMgr, void* entityChain);
extern uint64_t RawMaterialComponent_GetPickupCategory(uintptr_t rawMatDesc);

extern void     ResidenceComponent_ResizeResidentArray(void* arrayFieldPtr, int newCount);
extern void     ResidenceComponent_SerializeResidentArray(void* serializer, int count, void* arrayPtr);
extern void     ResidenceComponent_ResidenceInit(void* self);
extern "C" char ResidenceComponent_HeatTick(void* comp, void* sm, void* dt);

extern void     StorageDisplayComponent_MeshArrayResize(void* arrayBase, int newCount);
extern void     StorageDisplayComponent_SetDisplayMesh(uintptr_t self, int index, float fraction,
                                                       uintptr_t materialType);

extern void     StorageComponent_DynArrayResize(void* arrayBase, int newCount);

extern void     ConsumeProduceComponent_ResizeArray(void* arrayFieldPtr, int newCount);
extern void     ResourceProduceComponent_BaseShutdown(void* self, void* stream);

extern bool     WorkComponent_CheckWorkSlot(void* self);
extern void     WorkComponent_RegisterWorkSlot(void* self);

extern int      BuildComponent_FindInQueue(void* arrayBase, void* entityPtr);
extern void     BuildComponent_DrawReimpl(void* self);          // body  0x140108720
extern "C" void BuildComponent_OnAddedThunk(void* self);        // thunk 0x140108d40

extern void     WorkPlaceComponent_OnCreateInit(void* self);
extern void     WorkPlaceComponent_SetAccepting(int64_t comp, bool accepting);

extern void     AttendanceComponent_GetJobHostComponent(void* entity, void* outPtr);
extern void     AttendanceComponent_ProcessQueue(void* self);
extern "C" uint8_t AttendanceComponent_CommandMask(void* self, int cmd);   // 0x140043f10
extern "C" void    AttendanceComponent_Command(void* self, int cmd);       // 0x140043e80
extern "C" uint8_t ForesterComponent_CommandMask(void* self, int cmd);     // 0x14011f920
extern "C" void    ForesterComponent_Command(void* self, int cmd);         // 0x14011f890
extern "C" uintptr_t* ForesterComponent_CreateWork(void* self, uintptr_t* out); // 0x14011f780
extern "C" uintptr_t* MarketComponent_CreateWork(void* self, uintptr_t* out);   // 0x14004ea40
extern "C" uintptr_t* ConsumeProduceComponent_CreateWork(void* self, uintptr_t* out); // 0x140118d00
extern "C" void* ConsumeProduceAction_Ctor(void* mem, uintptr_t* entityIbp,
                                          const int* productIdx, int second); // 0x1400e9200
extern "C" void    AttendanceComponent_SerializeSlot(void* self, void* stream);  // 0x140044410
extern "C" void    AttendanceComponent_Unregister(void* self);             // 0x1400443d0
extern "C" char    AttendanceComponent_AllFull(void* self);                // 0x140043db0

extern void     InteractComponent_FindWorkplaceComponent(uintptr_t entity, void* retBuf);
// InteractComponent_Draw takes the typed pointer; declared with a matching opaque sig.
struct InteractComponent;
extern void     InteractComponent_Draw(InteractComponent* self);

extern void     TradeComponent_Setup(void* self);
struct TradeComponent;
extern void     TradeComponent_Draw(TradeComponent* self);      // 0x14005c1d0 — really OnAdded (slot 6)
extern void     TradeComponent_OnCreate(TradeComponent* self);  // 0x14005c190
extern uint8_t  TradeComponent_Slot13(TradeComponent* self);    // 0x14005c140 — work-state query
extern void     TradeComponent_Slot14(TradeComponent* self);    // 0x14005c000 — work-state toggle

extern void     SpawnComponent_DoSpawn(void* self, void* resourceData, int entryIndex, char flag);

extern void     FishingComponent_NotifyTile(void* self, int16_t param2);

extern void     StateMachine_TickReimpl(void* sm, int64_t time, char param3, char param4);

extern void     Component_SerializeReimpl(void* self, void* stream);

void RegisterComponentBatch_economy() {
    // rawmaterialcomponent.cpp
    RegisterExeAddr(0x140091e60, reinterpret_cast<uintptr_t>(&RawMaterialComponent_RegisterRawMaterial));
    RegisterExeAddr(0x140129480, reinterpret_cast<uintptr_t>(&RawMaterialComponent_GetPickupCategory));

    // residencecomponent.cpp
    RegisterExeAddr(0x140037ad0, reinterpret_cast<uintptr_t>(&ResidenceComponent_ResizeResidentArray));
    RegisterExeAddr(0x140037c50, reinterpret_cast<uintptr_t>(&ResidenceComponent_SerializeResidentArray));
    RegisterExeAddr(0x140129ef0, reinterpret_cast<uintptr_t>(&ResidenceComponent_ResidenceInit));
    // residence_heattick.cpp — state-2 per-frame heat/firewood-burn tick
    RegisterExeAddr(0x140129970, reinterpret_cast<uintptr_t>(&ResidenceComponent_HeatTick));

    // storagedisplaycomponent.cpp
    RegisterExeAddr(0x14000ca10, reinterpret_cast<uintptr_t>(&StorageDisplayComponent_MeshArrayResize));
    RegisterExeAddr(0x1400562b0, reinterpret_cast<uintptr_t>(&StorageDisplayComponent_SetDisplayMesh));

    // storagecomponent.cpp
    RegisterExeAddr(0x14000f090, reinterpret_cast<uintptr_t>(&StorageComponent_DynArrayResize));

    // consumeproducecomponent.cpp
    RegisterExeAddr(0x140023a00, reinterpret_cast<uintptr_t>(&ConsumeProduceComponent_ResizeArray));
    RegisterExeAddr(0x14012b4a0, reinterpret_cast<uintptr_t>(&ResourceProduceComponent_BaseShutdown));

    // workcomponent.cpp
    RegisterExeAddr(0x14012ead0, reinterpret_cast<uintptr_t>(&WorkComponent_CheckWorkSlot));
    RegisterExeAddr(0x14012eb50, reinterpret_cast<uintptr_t>(&WorkComponent_RegisterWorkSlot));

    // buildcomponent.cpp
    RegisterExeAddr(0x140017550, reinterpret_cast<uintptr_t>(&BuildComponent_FindInQueue));
    // 0x140108d40 is only a 4-instruction thunk — it tests the instant-build bit
    // (gameWorld+0xd0 bit 0) and either falls through to the body at 0x140108720 or
    // returns. Register BOTH: State8Enter calls the body directly (from 0x140107bb0),
    // and it does so exactly when the bit is set, so it must not go through the gate.
    RegisterExeAddr(0x140108d40, reinterpret_cast<uintptr_t>(&BuildComponent_OnAddedThunk));
    RegisterExeAddr(0x140108720, reinterpret_cast<uintptr_t>(&BuildComponent_DrawReimpl));

    // workplacecomponent.cpp
    RegisterExeAddr(0x14012ee60, reinterpret_cast<uintptr_t>(&WorkPlaceComponent_OnCreateInit));
    // Entity_impl_fc00 = WorkPlaceComponent::SetAccepting, the workplace enable/disable
    // toggle. BuildUI slot 9 (the under-construction panel's pause button, event 0x12
    // id 6000) resolves this address through fn_WorkplaceComponent_Toggle; unregistered it
    // came back NULL and the click called address 0 (live crash 2026-07-25). NOTE the port
    // implements the accepting=TRUE path only — accepting=false stores the flag and returns
    // without the retail evict-and-re-home pass (see workplacecomponent.cpp:391).
    RegisterExeAddr(0x14012fc00, reinterpret_cast<uintptr_t>(&WorkPlaceComponent_SetAccepting));

    // attendancecomponent.cpp
    RegisterExeAddr(0x140002e10, reinterpret_cast<uintptr_t>(&AttendanceComponent_GetJobHostComponent));
    RegisterExeAddr(0x140043f60, reinterpret_cast<uintptr_t>(&AttendanceComponent_ProcessQueue));
    // Vtable slots 13/14 — the panel command pair behind the "work" checkbox. Attendance
    // is not a ResourceProduceComponent subclass; both bodies delegate to the entity's
    // WorkPlaceComponent (the checkbox IS WPC+0x50 accepting).
    RegisterExeAddr(0x140043f10, reinterpret_cast<uintptr_t>(&AttendanceComponent_CommandMask));
    RegisterExeAddr(0x140043e80, reinterpret_cast<uintptr_t>(&AttendanceComponent_Command));
    // Controller-promotion wave 2026-08-15. Slot 3 (Shutdown/Serialize, Ghidra's
    // "AttendanceComponent_Controller_Slot13") and slot 7 (leave the town attendance
    // registry, Ghidra's "AttendanceComponent_OnCreate" — the usual OnCreate/OnRemove
    // mislabel). Slot 7 is also the first thing state 3's enter calls.
    RegisterExeAddr(0x140044410, reinterpret_cast<uintptr_t>(&AttendanceComponent_SerializeSlot));
    RegisterExeAddr(0x1400443d0, reinterpret_cast<uintptr_t>(&AttendanceComponent_Unregister));
    // The private AttendanceComponent state table's three handlers
    // (attendancecomponent_states.cpp). helper_3db0 is the per-description "every
    // building of my kind is full" test that drives the closed/full mesh flag.
    RegisterExeAddr(0x140043db0, reinterpret_cast<uintptr_t>(&AttendanceComponent_AllFull));

    // forestercomponent.cpp — vtable slots 13/14. The forester lodge is the one class in
    // this group that genuinely OVERRIDES the ResourceProduce base pair, because it has
    // two independent toggles (+0x50 plant / +0x51 cut) rather than one.
    RegisterExeAddr(0x14011f920, reinterpret_cast<uintptr_t>(&ForesterComponent_CommandMask));
    RegisterExeAddr(0x14011f890, reinterpret_cast<uintptr_t>(&ForesterComponent_Command));
    // Controller-promotion wave 2026-08-15 — slot 18, CreateWork. Ghidra calls it
    // "ForesterWork_CopyCtor"; it is ForesterComponent's slot 18, and its prologue
    // re-arms the two work flags when the player has cleared both.
    RegisterExeAddr(0x14011f780, reinterpret_cast<uintptr_t>(&ForesterComponent_CreateWork));

    // marketcomponent.cpp / resourceproduce_creatework.cpp — slot 18, CreateWork.
    // Ghidra calls 0x14004ea40 "MarketWork_Ctor"; it is MarketComponent's slot 18 and
    // has the same shape as HuntComponent's (MemAlloc 0x58 + Work_Ctor25 + vtable).
    // It was still the inert base stub, so no MarketWork was ever minted.
    RegisterExeAddr(0x14004ea40, reinterpret_cast<uintptr_t>(&MarketComponent_CreateWork));

    // resourceproduce_creatework.cpp -- the SIXTH slot-18 CreateWork, and the one that
    // was still missing entirely: ConsumeProduceComponent (the tavern and every other
    // consume->produce workshop). With the slot inert, state 13 minted no work and the
    // building brewed nothing -- measured as ZERO "[ConsumeProduce]" log lines across a
    // four-minute staffed-tavern soak. 0x1400e9200 is the 4-argument work ctor slot 18
    // calls (NOT the plain 0x1400e9340 ctor the Work factory uses).
    RegisterExeAddr(0x140118d00, reinterpret_cast<uintptr_t>(&ConsumeProduceComponent_CreateWork));
    RegisterExeAddr(0x1400e9200, reinterpret_cast<uintptr_t>(&ConsumeProduceAction_Ctor));

    // interactcomponent.cpp
    RegisterExeAddr(0x140004740, reinterpret_cast<uintptr_t>(&InteractComponent_FindWorkplaceComponent));
    RegisterExeAddr(0x140123110, reinterpret_cast<uintptr_t>(&InteractComponent_Draw));

    // tradecomponent.cpp
    RegisterExeAddr(0x14005c220, reinterpret_cast<uintptr_t>(&TradeComponent_Setup));
    // Both were ported but never registered, so nothing could reach them by address.
    // TradeComponent_Draw is the ONLY caller of Setup, which is the only thing that
    // allocates the merchant-name array at +0x30 that TradeUI_RefreshBuyTable indexes.
    RegisterExeAddr(0x14005c1d0, reinterpret_cast<uintptr_t>(&TradeComponent_Draw));
    RegisterExeAddr(0x14005c190, reinterpret_cast<uintptr_t>(&TradeComponent_OnCreate));
    RegisterExeAddr(0x14005c140, reinterpret_cast<uintptr_t>(&TradeComponent_Slot13));
    RegisterExeAddr(0x14005c000, reinterpret_cast<uintptr_t>(&TradeComponent_Slot14));

    // spawncomponent.cpp
    RegisterExeAddr(0x140055780, reinterpret_cast<uintptr_t>(&SpawnComponent_DoSpawn));

    // happinesscomponent.cpp (FishingComponent_impl_a300)
    RegisterExeAddr(0x14004a300, reinterpret_cast<uintptr_t>(&FishingComponent_NotifyTile));

    // statemachinecomponent.cpp
    RegisterExeAddr(0x1400a9660, reinterpret_cast<uintptr_t>(&StateMachine_TickReimpl));

    // absorbresourcecomponent.cpp (shared Component base serialize)
    RegisterExeAddr(0x1401189b0, reinterpret_cast<uintptr_t>(&Component_SerializeReimpl));
}
