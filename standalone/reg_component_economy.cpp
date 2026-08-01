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
