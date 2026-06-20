// Component registration wave — batch nature
// Registers exe helper addresses -> our clean reimplementations so the guarded
// ResolveAddr(Module::Exe, ...) call sites in code/Game/Component/ actually fire.
//
// The integrator wires RegisterComponentBatch_nature() into address_registry.cpp.
#ifdef STANDALONE_EXE
#include "resolve.h"
#include "Object/factory_registration.h"   // Object::GetTypeIdByName
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
struct ZoneComponent;  // defined in engine/types/ZoneComponent.h; bodies take ZoneComponent*
extern void ZoneComponent_ZoneMapAdd(void* zoneMap, void* entityPtr);
extern void ZoneComponent_Draw(ZoneComponent* self);
extern void ZoneComponent_OnCreate(ZoneComponent* self);

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

// ------------------------------------------------------------------
// Component type-ID .rdata globals (e.g. DAT_1401c7b30 = NaturalResourceComponent).
// The shared FindComponent accessors (FUN_1400052c0 et al.) compare a sibling
// component's GetTypeId against *pTypeId where pTypeId = ResolveAddr(Exe, addr);
// in the original these globals are .data filled by RegisterType. In standalone
// WE provide the storage (registered below at AddressRegistryInit) and FILL it
// after InvokeAllRegistrations — see FillComponentTypeIdGlobals_nature(), called
// from standalone/main.cpp once the type tree is populated (GetTypeIdByName is a
// registration-list lookup, so it returns 0 until then). Without this every
// type-id-keyed FindComponent silently returns null in standalone.
static uint32_t g_tid_NaturalResourceComponent = 0;  // 0x1401c7b30

extern "C" void FillComponentTypeIdGlobals_nature() {
    g_tid_NaturalResourceComponent =
        ::Object::GetTypeIdByName("NaturalResourceComponent");
}

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

    // .rdata float/double constant-pool entries read by growthcomponent.cpp
    // (GrowthComponent_Update / _Startup / _SpawnSubResource) via
    // ResolveAddr(Module::Exe, addr). The original exe stores these as image
    // .rdata; in standalone WE provide backing storage holding the EXACT bit
    // patterns verified in Ghidra. (0x14017214c = 1.0f is already registered in
    // gameupdateloop_load.cpp.) See growthcomponent.cpp EnsureHelpers/EnsureSubHelpers.
    RegisterExeAddr(0x140172140, reinterpret_cast<uintptr_t>(&k_rdata_172140));  // 0.98   UV/spawn scale
    RegisterExeAddr(0x1401721d0, reinterpret_cast<uintptr_t>(&k_rdata_1721d0));  // 32.0   freeze temperature
    RegisterExeAddr(0x14017222c, reinterpret_cast<uintptr_t>(&k_rdata_17222c));  // 255.0  byte<->float scale
    RegisterExeAddr(0x140172234, reinterpret_cast<uintptr_t>(&k_rdata_172234));  // 300.0  growth-time scale
    RegisterExeAddr(0x140172248, reinterpret_cast<uintptr_t>(&k_rdata_172248));  // 1000.0 spawn-distance scale
    RegisterExeAddr(0x140172288, reinterpret_cast<uintptr_t>(&k_rdata_172288));  // -10.0  growth rate at freeze
    RegisterExeAddr(0x1401722b0, reinterpret_cast<uintptr_t>(&k_rdata_1722b0));  // double fabs bitmask

    // Component type-ID global (filled after InvokeAllRegistrations — see above).
    RegisterExeAddr(0x1401c7b30, reinterpret_cast<uintptr_t>(&g_tid_NaturalResourceComponent));
}
#endif // STANDALONE_EXE
