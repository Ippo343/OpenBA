// Component registration wave — batch nature
// Registers exe helper addresses -> our clean reimplementations so the guarded
// ResolveAddr(Module::Exe, ...) call sites in code/Game/Component/ actually fire.
//
// The integrator wires RegisterComponentBatch_nature() into address_registry.cpp.
#include "resolve.h"
#include "Object/factory_registration.h"   // Object::GetTypeIdByName
#include <cstdint>
#include "Component/NaturalResourceComponent.h"

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

// ---- huntcomponent.cpp ----

// ---- naturalresourcecomponent.cpp ----
extern void NaturalResourceComponent_FindComponentByType(void* entity, void* outIBP);

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

// NOTE: the brief's cropfield addresses 0x1400035b0 / 0x1400036c0 were a
// dropped-digit typo (they point at UNRELATED functions —
// AutoPickupComponent_ScalarDtor mid-body / BoatComponent_GetType). The real
// crop entity-list helpers are 0x1400335b0 / 0x1400336c0; the call sites in
// cropfieldcomponent.cpp were corrected to those, and we register the corrected
// addresses below. 0x1400035b0 / 0x1400036c0 are NOT registered.

// ------------------------------------------------------------------

// Backing storage for the growthcomponent .rdata constant-pool entries (exact
// bit patterns from Application-x64.exe .rdata, verified in Ghidra).
static const uint32_t k_rdata_172140 = 0x3f7ae148u;          // 0.98f
static const uint32_t k_rdata_1721d0 = 0x42000000u;          // 32.0f
static const uint32_t k_rdata_17222c = 0x437f0000u;          // 255.0f
static const uint32_t k_rdata_172234 = 0x43960000u;          // 300.0f
static const uint32_t k_rdata_172248 = 0x447a0000u;          // 1000.0f
static const uint32_t k_rdata_172288 = 0xc1200000u;          // -10.0f
static const uint64_t k_rdata_1722b0 = 0x7fffffffffffffffull; // double fabs mask

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

    // 0x14012e5c0 (WorkComponent::CalcMaxWorkers) used to be registered here TOO, as
    // "HuntComponent_ComputeTerrainSpeed". Both copies had identical bodies, so which one
    // won depended purely on batch order (RegisterExeAddr is last-write-wins). The Ghidra
    // body computes a footprint-scaled worker capacity from WorkDescription+0x24/+0x30/+0x34
    // — nothing to do with terrain speed; HuntComponent::Update merely reuses the same
    // workComp+0x30 cache as its divisor. The duplicate is gone; the single body is
    // WorkComponent_CalcMaxWorkers, registered from reg_tutorial_base.cpp.

    // naturalresourcecomponent.cpp
    RegisterExeAddr(0x140005200, reinterpret_cast<uintptr_t>(&NaturalResourceComponent_FindComponentByType));

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

    // .rdata float/double constant-pool entries read by growthcomponent.cpp
    // (GrowthComponent_Update / _Startup / _SpawnSubResource) via
    // ResolveAddr(Module::Exe, addr). The original exe stores these as image
    // .rdata; in standalone WE provide backing storage holding the EXACT bit
    // patterns verified in Ghidra. (0x14017214c = 1.0f is already registered in
    // gameupdateloop_load.cpp.) See growthcomponent.cpp EnsureHelpers/EnsureSubHelpers.
    RegisterExeAddr(0x140172140, reinterpret_cast<uintptr_t>(&k_rdata_172140));  // 0.98   UV/spawn scale
    RegisterExeAddr(0x1401721d0, reinterpret_cast<uintptr_t>(&k_rdata_1721d0));  // 32.0   freeze temperature
    RegisterExeAddr(0x14017222c, reinterpret_cast<uintptr_t>(&k_rdata_17222c));  // 255.0  byte<->float scale
    // 0x140172234 (300.0f growth-time scale) is registered by reg_citizen_needsupdate.cpp,
    // which runs later. Same bit pattern; duplicate removed 2026-08-30.
    RegisterExeAddr(0x140172248, reinterpret_cast<uintptr_t>(&k_rdata_172248));  // 1000.0 spawn-distance scale
    RegisterExeAddr(0x140172288, reinterpret_cast<uintptr_t>(&k_rdata_172288));  // -10.0  growth rate at freeze
    RegisterExeAddr(0x1401722b0, reinterpret_cast<uintptr_t>(&k_rdata_1722b0));  // double fabs bitmask

}
