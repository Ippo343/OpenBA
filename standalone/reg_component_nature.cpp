// Component registration wave — batch nature
// Registers exe helper addresses -> our clean reimplementations so the guarded
// ResolveAddr(Module::Exe, ...) call sites in code/Game/Component/ actually fire.
//
// The integrator wires RegisterComponentBatch_nature() into address_registry.cpp.
#ifdef STANDALONE_EXE
#include "resolve.h"
#include <cstdint>

// ---- growthcomponent.cpp ----
extern void GrowthComponent_ModifyFlags(void* comp, uint32_t orMask, uint32_t andNotMask);
extern int  GrowthComponent_GetResourceIndex(void* comp);
extern void GrowthComponent_SetResourceAmount(void* comp, float amount);
extern void GrowthComponent_ResetGrowthState(void* entityPtr);
extern void GrowthComponent_SpawnSubResource(void* growthComp, void* config);

// ---- floorcomponent.cpp ----
extern void* FloorComponent_FindResData(void* owner, void* outIBP);
extern void  FloorComponent_RemoveFloorMesh(void* scene, uint32_t floorId,
                                            int x, int y, int w, int h);
extern void  FloorComponent_SubF390(void* selfV);

// ---- orchardcomponent.cpp ----
extern void OrchardComponent_ResizeRefArray(void* arrayPtr, int newCount);
extern void OrchardComponent_SerializeRefEntries(void* serializer, int count, void* arrayBase);
extern void OrchardComponent_ValidateRefs(void* selfV);

// ---- zonecomponent.cpp ----
extern void ZoneComponent_ZoneMapAdd(void* zoneMap, void* entityPtr);
extern void ZoneComponent_Draw(void* self);
extern void ZoneComponent_OnCreate(void* self);

// ---- huntcomponent.cpp ----
extern int  HuntComponent_ComputeTerrainSpeed(int64_t tileData);

// ---- naturalresourcecomponent.cpp ----
extern void NaturalResourceComponent_FindComponentByType(void* entity, void* outIBP);

// ---- boatcomponent.cpp ----
extern void BoatComponent_ResizeVec3Array(void* arrayField, int newCount);

// ---- cropfieldcomponent.cpp ----
extern void CropFieldComponent_ValidateEntityRefs(uintptr_t self);
extern void CropFieldComponent_ResizeEntityList(void* arrayPtr, int newSize);
extern void CropFieldComponent_SerializeEntityList(void* serializer, int count, uintptr_t arrayPtr);

// ---- pasturecomponent.cpp ----
extern uint64_t PastureComponent_ComputeCapacity(uintptr_t self);
extern void     PastureComponent_Helper5ed0(void* entityRef);
extern void     PastureComponent_ReassignEntityRef(uintptr_t entityPtr);

// ---- herdcomponent.cpp ----
extern float HerdComponent_CheckFood(int64_t entityComp, int gridCell);
extern char  HerdComponent_CheckMigrationDone(int64_t herdComp);
extern void  HerdComponent_AdjustSize(void* herdComp, int targetSize);

// NOTE: 0x140106420 (AnimalComponent::Update) is intentionally NOT registered —
// it remains a passthrough that resolves and calls 0x140106420 itself; registering
// it would cause infinite recursion. Needs a genuine 452-instruction reimpl first.
//
// NOTE: the brief's cropfield addresses 0x1400035b0 / 0x1400036c0 were a
// dropped-digit typo (they point at UNRELATED functions —
// AutoPickupComponent_ScalarDtor mid-body / BoatComponent_GetType). The real
// crop entity-list helpers are 0x1400335b0 / 0x1400336c0; the call sites in
// cropfieldcomponent.cpp were corrected to those, and we register the corrected
// addresses below. 0x1400035b0 / 0x1400036c0 are NOT registered.

void RegisterComponentBatch_nature() {
    // growthcomponent.cpp
    RegisterExeAddr(0x140118950, reinterpret_cast<uintptr_t>(&GrowthComponent_ModifyFlags));
    RegisterExeAddr(0x140120410, reinterpret_cast<uintptr_t>(&GrowthComponent_GetResourceIndex));
    RegisterExeAddr(0x140120470, reinterpret_cast<uintptr_t>(&GrowthComponent_SetResourceAmount));
    RegisterExeAddr(0x140126780, reinterpret_cast<uintptr_t>(&GrowthComponent_ResetGrowthState));
    RegisterExeAddr(0x14011fba0, reinterpret_cast<uintptr_t>(&GrowthComponent_SpawnSubResource));

    // floorcomponent.cpp
    RegisterExeAddr(0x140004680, reinterpret_cast<uintptr_t>(&FloorComponent_FindResData));
    RegisterExeAddr(0x14009cba0, reinterpret_cast<uintptr_t>(&FloorComponent_RemoveFloorMesh));
    RegisterExeAddr(0x14011f390, reinterpret_cast<uintptr_t>(&FloorComponent_SubF390));

    // orchardcomponent.cpp
    RegisterExeAddr(0x140009f80, reinterpret_cast<uintptr_t>(&OrchardComponent_ResizeRefArray));
    RegisterExeAddr(0x14000a230, reinterpret_cast<uintptr_t>(&OrchardComponent_SerializeRefEntries));
    RegisterExeAddr(0x140051830, reinterpret_cast<uintptr_t>(&OrchardComponent_ValidateRefs));

    // zonecomponent.cpp
    RegisterExeAddr(0x140088e20, reinterpret_cast<uintptr_t>(&ZoneComponent_ZoneMapAdd));
    RegisterExeAddr(0x1401301f0, reinterpret_cast<uintptr_t>(&ZoneComponent_Draw));
    RegisterExeAddr(0x140130080, reinterpret_cast<uintptr_t>(&ZoneComponent_OnCreate));

    // huntcomponent.cpp  (0x14012e5c0 also referenced from workcomponent.cpp — body lives here)
    RegisterExeAddr(0x14012e5c0, reinterpret_cast<uintptr_t>(&HuntComponent_ComputeTerrainSpeed));

    // naturalresourcecomponent.cpp
    RegisterExeAddr(0x140005200, reinterpret_cast<uintptr_t>(&NaturalResourceComponent_FindComponentByType));

    // boatcomponent.cpp  (0x14000d030 also referenced from terraform/tornado — body lives here)
    RegisterExeAddr(0x14000d030, reinterpret_cast<uintptr_t>(&BoatComponent_ResizeVec3Array));

    // cropfieldcomponent.cpp  (0x1400335b0/0x1400336c0 = corrected from typo'd 0x1400035b0/0x1400036c0)
    RegisterExeAddr(0x14011aa20, reinterpret_cast<uintptr_t>(&CropFieldComponent_ValidateEntityRefs));
    RegisterExeAddr(0x1400335b0, reinterpret_cast<uintptr_t>(&CropFieldComponent_ResizeEntityList));
    RegisterExeAddr(0x1400336c0, reinterpret_cast<uintptr_t>(&CropFieldComponent_SerializeEntityList));

    // pasturecomponent.cpp
    RegisterExeAddr(0x140128570, reinterpret_cast<uintptr_t>(&PastureComponent_ComputeCapacity));
    RegisterExeAddr(0x140105ed0, reinterpret_cast<uintptr_t>(&PastureComponent_Helper5ed0));
    RegisterExeAddr(0x1401240a0, reinterpret_cast<uintptr_t>(&PastureComponent_ReassignEntityRef));

    // herdcomponent.cpp
    RegisterExeAddr(0x140053f20, reinterpret_cast<uintptr_t>(&HerdComponent_CheckFood));
    RegisterExeAddr(0x140121980, reinterpret_cast<uintptr_t>(&HerdComponent_CheckMigrationDone));
    RegisterExeAddr(0x140121450, reinterpret_cast<uintptr_t>(&HerdComponent_AdjustSize));
}
#endif // STANDALONE_EXE
