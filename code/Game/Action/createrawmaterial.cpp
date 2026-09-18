// code/Game/Action/createrawmaterial.cpp
// Clean C++ reimplementation of CreateRawMaterial action methods.
// Source: Game/Action/CreateRawMaterial.cpp (from profile build debug strings)
//
// CreateRawMaterial -- 0x58 bytes. Spawns raw material entities from a resource building.
// Inherits: Object::Internal -> Action (0x20) -> CreateRawMaterial
// Vtable at 0x14015fd10 (10 slots).
//
// Fields from Serialize:
//   +0x20: description entity (ExternalPtr/type ref, serialized via FUN_140002340)
//   +0x28: InternalBasePtr (source building)
//   +0x30: InternalBasePtr (optional secondary entity)
//   +0x38: InternalBasePtr (optional tertiary entity)
//   +0x40: int (total quantity)
//   +0x44: int (material type index)
//   +0x48: uint (placement flags)
//   +0x4c: float (yield multiplier)
//   +0x50: bool (natural resource flag)
//   +0x51: bool (place near flag)
//
// Decompiled from Application-x64.exe using Ghidra.

#include "CreateRawMaterial.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "Component/coll_bridge.h"
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// ============================================================
// Local function pointers resolved at init time
// (NOT in engine_funcs.h -- CreateRawMaterial-specific helpers)
// ============================================================

// FUN_140002340 -- serialize StringHash/type ref
static void (*s_fn_SerializeTypeRef)(void* field, void* serializer) = nullptr;

// ComponentEntity_CopyCtor (0x1400c3720) -- clone entity from description
static void* (*s_fn_ComponentEntity_CopyCtor)(void* self, void* desc) = nullptr;

// CreateRawMaterial_helper_bd40 (0x14009bd40) -- find placement cell
// Params: mapGrid*, cellIndex, naturalResourceFlag, outEntityPtr*, placementFlags
static int (*s_fn_FindPlacementCell)(int* mapGrid, int cellIndex, char nrFlag,
                                      int64_t* outEntity, uint32_t flags) = nullptr;

// CreateRawMaterial_helper_9760 (0x140129760) -- position entity at cell with rotation
// Params: componentPtr, cellIndex, rotationAngle
static void (*s_fn_PositionEntity)(int64_t comp, int cellIndex, float rotation) = nullptr;

// CreateRawMaterial_helper_8ef0 (0x140088ef0) -- register raw material with resource tracker
// Params: entityList, sourceIBP, placeNearFlag, descEntity, quantity
static void (*s_fn_RegisterRawMaterial)(int64_t entityList, void* sourceIBP,
                                         char placeNear, int64_t descEntity, int quantity) = nullptr;

// FUN_1400c48a0 -- register entity with entity list
static void (*s_fn_RegisterEntity)(void** entityList, void* entityIBP, int typeGroup) = nullptr;

// FUN_140022520 -- get child entities from source building
// Member ABI: RCX=entity, RDX=retbuf
static void* (*s_fn_GetChildEntities)(int64_t entity, void* retbuf) = nullptr;

// FUN_140003050 -- find component on entity (ResourceDescription?)
// Member ABI: RCX=entity, RDX=retbuf
static void* (*s_fn_FindComponent_RD)(int64_t entity, void* retbuf) = nullptr;

// System::Random::RandTol via IAT at 0x1401512f0
static int (*s_fn_Random_RandTol)(void* random, int range) = nullptr;

// IO::Serializer::Serialize(float) via IAT at 0x140152a60
static void (*s_fn_Serialize_Float)(void* self, void* field, const char* name) = nullptr;

// IO::Serializer::Serialize(uint) via IAT at 0x140152a50
static void (*s_fn_Serialize_UInt)(void* self, void* field, const char* name) = nullptr;

// TypeId for entity validation: 0x1401c7158 (used for +0x28 IBP)
static uint32_t* s_typeId_1401c7158 = nullptr;

// TypeId for ComponentEntity: 0x1401c5d70 (used for +0x30, +0x38 IBP)
static uint32_t* s_typeId_ComponentEntity = nullptr;

// TypeId for RawMaterialDescription: 0x1401c7cb8
static uint32_t* s_typeId_RawMatDesc = nullptr;

// TypeId for component on new entity: DAT_1401c7c80
static uint32_t* s_typeId_1401c7c80 = nullptr;

// TypeId for DAT_1401c8118 (used for secondary entity component)
static uint32_t* s_typeId_1401c8118 = nullptr;

// Constant: rotation scale factor (90.0f) at 0x140172204
static const float ROTATION_SCALE = 90.0f;

static bool s_crm_funcs_resolved = false;

// ============================================================
// Resolution -- called once from Install
// ============================================================
bool CreateRawMaterial_ResolveFuncs() {
    if (s_crm_funcs_resolved) return true;

    s_fn_SerializeTypeRef = reinterpret_cast<void(*)(void*, void*)>(
        ResolveAddr(Module::Exe, 0x140002340));

    s_fn_ComponentEntity_CopyCtor = reinterpret_cast<void*(*)(void*, void*)>(
        ResolveAddr(Module::Exe, 0x1400c3720));

    s_fn_FindPlacementCell = reinterpret_cast<int(*)(int*, int, char, int64_t*, uint32_t)>(
        ResolveAddr(Module::Exe, 0x14009bd40));

    s_fn_PositionEntity = reinterpret_cast<void(*)(int64_t, int, float)>(
        ResolveAddr(Module::Exe, 0x140129760));

    s_fn_RegisterRawMaterial = reinterpret_cast<void(*)(int64_t, void*, char, int64_t, int)>(
        ResolveAddr(Module::Exe, 0x140088ef0));

    s_fn_RegisterEntity = reinterpret_cast<void(*)(void**, void*, int)>(
        ResolveAddr(Module::Exe, 0x1400c48a0));

    s_fn_GetChildEntities = reinterpret_cast<void*(*)(int64_t, void*)>(
        ResolveAddr(Module::Exe, 0x140022520));

    s_fn_FindComponent_RD = reinterpret_cast<void*(*)(int64_t, void*)>(
        ResolveAddr(Module::Exe, 0x140003050));

    // FUN_140044810 (set up growth/maturity component) is the exe-side
    // Collection::Array<int32>::SetSize(n,fill) monomorphisation --
    // ENDGAME B.3.2 step 2 converted its call site below to a
    // GameColl::AsArray<GameColl::I32> member call, so its resolved-pointer
    // stand-in is gone.

    // System::Random::RandTol via IAT at 0x1401512f0
    uintptr_t iatRandTol = ResolveAddr(Module::Exe, 0x1401512f0);
    if (iatRandTol) s_fn_Random_RandTol = *reinterpret_cast<int(**)(void*, int)>(iatRandTol);

    // IO::Serializer::Serialize(float) via IAT at 0x140152a60
    uintptr_t iatSerFloat = ResolveAddr(Module::Exe, 0x140152a60);
    if (iatSerFloat) s_fn_Serialize_Float = *reinterpret_cast<void(**)(void*, void*, const char*)>(iatSerFloat);

    // IO::Serializer::Serialize(uint) via IAT at 0x140152a50
    uintptr_t iatSerUInt = ResolveAddr(Module::Exe, 0x140152a50);
    if (iatSerUInt) s_fn_Serialize_UInt = *reinterpret_cast<void(**)(void*, void*, const char*)>(iatSerUInt);

    s_typeId_1401c7158 = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7158));
    s_typeId_ComponentEntity = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c5d70));
    s_typeId_RawMatDesc = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7cb8));
    s_typeId_1401c7c80 = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7c80));
    s_typeId_1401c8118 = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c8118));

    bool ok = s_fn_SerializeTypeRef && s_fn_ComponentEntity_CopyCtor &&
              s_fn_FindPlacementCell && s_fn_PositionEntity &&
              s_fn_RegisterRawMaterial && s_fn_RegisterEntity &&
              s_fn_GetChildEntities && s_fn_FindComponent_RD &&
              s_fn_Random_RandTol &&
              s_fn_Serialize_Float && s_fn_Serialize_UInt &&
              s_typeId_1401c7158 && s_typeId_ComponentEntity &&
              s_typeId_RawMatDesc && s_typeId_1401c7c80 && s_typeId_1401c8118 &&
              fn_ObjectInternal_Dtor && fn_InternalBasePtr_DeletePtr &&
              fn_MemFreeStatic && fn_InternalBasePtr_CheckPtr &&
              fn_InternalBasePtr_Ctor && fn_IsDerivedFrom &&
              fn_InternalBasePtr_Serialize &&
              g_fn_Serializer_Ctor && g_fn_Serializer_Dtor &&
              g_fn_Serialize_Bool && g_fn_Serialize_Int &&
              g_fn_Serializer_GetVersion &&
              g_pGameWorldSingleton && fn_MemAlloc && g_pMemAllocSingleton;

#ifdef _DEBUG
    printf("[CreateRawMaterial] ResolveFuncs: SerTypeRef=%p CopyCtor=%p FindCell=%p\n",
           (void*)s_fn_SerializeTypeRef, (void*)s_fn_ComponentEntity_CopyCtor,
           (void*)s_fn_FindPlacementCell);
    printf("[CreateRawMaterial]   PosEnt=%p RegRM=%p RegEnt=%p GetChild=%p\n",
           (void*)s_fn_PositionEntity, (void*)s_fn_RegisterRawMaterial,
           (void*)s_fn_RegisterEntity, (void*)s_fn_GetChildEntities);
    printf("[CreateRawMaterial]   FindRD=%p RandTol=%p SerFloat=%p SerUInt=%p\n",
           (void*)s_fn_FindComponent_RD,
           (void*)s_fn_Random_RandTol, (void*)s_fn_Serialize_Float, (void*)s_fn_Serialize_UInt);
    if (!ok) printf("[CreateRawMaterial] ERROR: some function pointers not resolved!\n");
#endif

    s_crm_funcs_resolved = ok;
    return ok;
}

// ============================================================
// Helper: find RawMaterialDescription component in description entity
// Searches the component array at desc+0x28 (count at desc+0x30)
// for a component matching type g_TypeId_RawMaterialDescription.
// ============================================================
static void* FindRawMaterialDesc(void* descEntity) {
    uintptr_t desc = (uintptr_t)descEntity;
    int count = *(int*)(desc + 0x30);
    uintptr_t compArray = *(uintptr_t*)(desc + 0x28);

    for (int i = 0; i < count; i++) {
        void* comp = *(void**)(compArray + (int64_t)i * 8);
        // Get type ID via vtable slot +0x10 (GetType)
        uintptr_t vtable = *(uintptr_t*)comp;
        typedef uint32_t (*GetType_t)();
        uint32_t typeId = ((GetType_t)(*(void**)(vtable + 0x10)))();
        if (typeId == *s_typeId_RawMatDesc) {
            // Verify with IsDerivedFrom
            if (fn_IsDerivedFrom(comp, *s_typeId_RawMatDesc)) {
                return comp;
            }
        }
    }
    return nullptr;
}

// ============================================================
// Helper: find component by type ID on new entity
// Searches the component array at entity+0x30 (count at entity+0x38)
// using InternalBasePtr for each component.
// ============================================================
static void* FindComponentOnEntity(int64_t entity, uint32_t typeId) {
    int count = *(int*)(entity + 0x38);
    uintptr_t compArray = *(uintptr_t*)(entity + 0x30);

    for (int i = 0; i < count; i++) {
        void* compIBP = (void*)(compArray + (int64_t)i * 8);
        fn_InternalBasePtr_CheckPtr(compIBP);
        void* comp = *(void**)compIBP;

        // Get type ID via vtable slot +0x10
        uintptr_t vtable = *(uintptr_t*)comp;
        typedef uint32_t (*GetType_t)();
        uint32_t compTypeId = ((GetType_t)(*(void**)(vtable + 0x10)))();
        if (compTypeId == typeId) {
            fn_InternalBasePtr_CheckPtr(compIBP);
            comp = *(void**)compIBP;
            if (fn_IsDerivedFrom(comp, typeId)) {
                return comp;
            }
        }
    }
    return nullptr;
}

// ============================================================
// ScalarDtor (0x140022350)
// Set vtables, delete entity ref, call ~Internal, optionally free.
// ============================================================
void* CreateRawMaterial_ScalarDtor_Impl(CreateRawMaterial* self, uint32_t flags) {
    // Set CreateRawMaterial vtable
    self->vtable = (void*)ResolveAddr(Module::Exe, CreateRawMaterialAddr::Vtable);
    // Set Action base vtable
    self->vtable = (void*)ResolveAddr(Module::Exe, 0x14015f700);

    // If entity ref at +0x10 is non-null, delete it
    if (self->entity != nullptr) {
        reinterpret_cast< ::Object::InternalBasePtr*>(&self->entity)->DeletePtr();   // B.3.3 bridge
    }

    fn_ObjectInternal_Dtor(self);

    if (flags & 1) {
        fn_MemFreeStatic(self);
    }
    return self;
}

// ============================================================
// Serialize (0x1400c6f30)
// Versioned serialization with version 3 serializer.
// Version gates at 0, 1, 2, and 3 control which fields are serialized.
// ============================================================
void CreateRawMaterial_Serialize_Impl(CreateRawMaterial* self, void* stream) {
    // Lazy-resolve like every other method on this exe-ctor-built action. Serialize
    // was the one that never got it, so a save taken while one of these actions was
    // in flight called a null serializer pointer (EXEC of 0). Found live on
    // CompleteRoad once roads started being built; these four are the same shape.
    CreateRawMaterial_ResolveFuncs();
    uint8_t* s = (uint8_t*)self;
    uint8_t ser1[24];
    uint8_t ser2[24];

    // Serialize status (version 0)
    g_fn_Serializer_Ctor(ser1, stream, 0);
    g_fn_Serialize_Bool(ser1, s + 0x18, nullptr);
    g_fn_Serializer_Dtor(ser1);

    // Serialize resource data (version 3)
    g_fn_Serializer_Ctor(ser2, stream, 3);

    // Type reference (+0x20)
    s_fn_SerializeTypeRef(s + 0x20, ser2);

    // Source building IBP (+0x28)
    fn_InternalBasePtr_Serialize(s + 0x28, ser2);
    if (*(void**)(s + 0x28) != nullptr) {
        if (!fn_IsDerivedFrom(*(void**)(s + 0x28), *s_typeId_1401c7158)) {
            *(void**)(s + 0x28) = nullptr;
        }
    }

    // Material type index (+0x44) as int
    g_fn_Serialize_Int(ser2, s + 0x44, nullptr);

    // Yield multiplier (+0x4c) as float
    s_fn_Serialize_Float(ser2, s + 0x4c, nullptr);

    // Natural resource flag (+0x50) as bool
    g_fn_Serialize_Bool(ser2, s + 0x50, nullptr);

    // Total quantity (+0x40) as int
    g_fn_Serialize_Int(ser2, s + 0x40, nullptr);

    // Place near flag (+0x51) as bool
    g_fn_Serialize_Bool(ser2, s + 0x51, nullptr);

    // Version > 0: secondary entity IBP (+0x30)
    int version = g_fn_Serializer_GetVersion(ser2);
    if (version > 0) {
        fn_InternalBasePtr_Serialize(s + 0x30, ser2);
        if (*(void**)(s + 0x30) != nullptr) {
            if (!fn_IsDerivedFrom(*(void**)(s + 0x30), *s_typeId_ComponentEntity)) {
                *(void**)(s + 0x30) = nullptr;
            }
        }
    }

    // Version < 2: default placement flags; >= 2: serialize uint (+0x48)
    version = g_fn_Serializer_GetVersion(ser2);
    if (version < 2) {
        *(uint32_t*)(s + 0x48) = 4;
    } else {
        s_fn_Serialize_UInt(ser2, s + 0x48, nullptr);
    }

    // Version > 2: tertiary entity IBP (+0x38)
    version = g_fn_Serializer_GetVersion(ser2);
    if (version > 2) {
        fn_InternalBasePtr_Serialize(s + 0x38, ser2);
        if (*(void**)(s + 0x38) != nullptr) {
            if (!fn_IsDerivedFrom(*(void**)(s + 0x38), *s_typeId_ComponentEntity)) {
                *(void**)(s + 0x38) = nullptr;
            }
        }
    }

    g_fn_Serializer_Dtor(ser2);
}


int CreateRawMaterial_AbstractImpl_Impl(CreateRawMaterial* self, void* actionStack) {
    (void)actionStack;
    CreateRawMaterial_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    uint8_t* s = (uint8_t*)self;
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;

    // Get description entity at +0x20
    uintptr_t descEntity = *(uintptr_t*)(s + 0x20);

    // Find RawMaterialDescription component in the description
    void* rawMatDesc = FindRawMaterialDesc((void*)descEntity);

    // Check building maturity to determine yield
    fn_InternalBasePtr_CheckPtr(s + 0x28);
    uintptr_t building = *(uintptr_t*)(s + 0x28);

    int yield;
    // Compare building age (+0x210) with maturity threshold from description (+0x288 -> +0x118)
    int buildingAge = *(int*)(building + 0x210);
    uintptr_t buildDesc = *(uintptr_t*)(building + 0x288);
    int maturityAge = *(int*)(buildDesc + 0x118);

    if (buildingAge < maturityAge) {
        // Young: use yield at +0x40
        yield = *(int*)((uintptr_t)rawMatDesc + 0x40);
    } else {
        // Mature: use yield at +0x44
        yield = *(int*)((uintptr_t)rawMatDesc + 0x44);
    }

    // Apply yield multiplier
    float yieldMul = *(float*)(s + 0x4c);
    int totalItems = (int)((float)yield * yieldMul);

    if (totalItems == 0) {
        return 0;
    }

    // Max items per PILE = the carrier's volume capacity / this material's per-unit volume.
    // Retail 0x1400c6c22-2c, verified at instruction level:
    //     MOV EAX,[R15+0x40]     ; R15 = self, +0x40 = ctor arg7
    //     CDQ
    //     IDIV dword ptr [RBX+0x48]  ; RBX = rawMatDesc, +0x48 = per-unit VOLUME
    //     MOV ECX,EAX            ; ECX = cap, loop-invariant
    //     MOV [RBP+0x50],EAX     ; stashed; reloaded into ECX at the loop tail (6c6ef3)
    // then inside the loop at 6c6c40-46: MOV EAX,R13D / CMP R13D,ECX / CMOVG EAX,ECX
    // = min(totalItems, cap). The division is done ONCE, before the loop — matched here.
    //
    // This port previously used rawMatDesc+0x48 (the per-unit volume) DIRECTLY as the cap,
    // i.e. it dropped the division entirely, so every pile produced or culled has been the
    // wrong size since the function was written.
    //
    // action+0x40 is CreateRawMaterial's ctor arg7, and all four retail construction sites
    // pass StorageComponent+0x38 — the carrier's max volume capacity (0x1400bed3f loads it
    // as MOV R12D,[RAX+0x38] and 0x1400bed9f stores it to RSP+0x30 = arg7; our ported site
    // in action_pf_roots_drop.cpp does the same). So this is "how many whole units fit in
    // the hauler", which is why it caps the per-entity batch.
    int unitVolume = *(int*)((uintptr_t)rawMatDesc + 0x48);
    int carrierVolume = *(int*)(s + 0x40);
    // STANDALONE-ONLY TRIPWIRE (retail has no equivalent and needs none). Retail's IDIV
    // relies on +0x48 being non-zero and its cap being >= 1; a cap of 0 makes batchSize 0,
    // the loop tail's `SUB R13D,EDI` subtract nothing, and 0x1400c6efa jump back forever —
    // an in-world FREEZE, not a crash. We can still reach that state because a not-yet-
    // ported construction path would leave +0x40 at its zeroed ctor value, so fail LOUDLY
    // with the operands instead of hanging. If this ever fires, fix the ctor site that
    // passed a zero capacity — do not soften this into a clamp.
    if (unitVolume <= 0 || carrierVolume < unitVolume) {   // i.e. the quotient would be <= 0
        fprintf(stderr, "[CreateRawMaterial] ABORT: batch cap would be 0 — "
                "carrierVolume(action+0x40)=%d unitVolume(rawMatDesc+0x48)=%d totalItems=%d. "
                "Retail would spin here forever; the ctor site that built this action passed "
                "a zero/short capacity as arg7.\n", carrierVolume, unitVolume, totalItems);
        fflush(stderr);
        return 0;
    }
    int maxPerEntity = carrierVolume / unitVolume;

    // Spawn loop: create entities for all items
    while (totalItems > 0) {
        int batchSize = totalItems;
        if (maxPerEntity < totalItems) {
            batchSize = maxPerEntity;
        }

        // Check if natural resource flag needs to be set
        if (*(char*)(s + 0x50) == '\0') {
            int32_t matIdx = *(int32_t*)(s + 0x44);
            uintptr_t mapGrid = *(uintptr_t*)(gameWorld + 0xf8);
            uintptr_t cellArrayBase = *(uintptr_t*)(mapGrid + 0x80);
            int64_t existingEntity = *(int64_t*)(cellArrayBase + 8 + (int64_t)matIdx * 0x30);
            if (existingEntity != 0) {
                *(uint8_t*)(s + 0x50) = 1;
            }
        }

        // Find placement cell
        int32_t matIdx = *(int32_t*)(s + 0x44);
        int cellIndex;
        if (*(char*)(s + 0x50) != '\0') {
            // Natural resource placement: use helper
            void* thirdEntity = *(void**)(s + 0x38);
            cellIndex = s_fn_FindPlacementCell(
                *(int**)(gameWorld + 0xf8), matIdx, '\x01',
                (int64_t*)&thirdEntity, *(uint32_t*)(s + 0x48));
        } else {
            cellIndex = matIdx;
        }

        if (cellIndex < 0) {
            // No valid cell found, still consume items
            batchSize = batchSize;  // keep batchSize as is
        } else {
            // Allocate new ComponentEntity from description
            void* newEntityMem = fn_MemAlloc(g_pMemAllocSingleton, 0x48, 8);
            void* newEntity = nullptr;
            if (newEntityMem != nullptr) {
                newEntity = s_fn_ComponentEntity_CopyCtor(newEntityMem, (void*)descEntity);
            }

            // Wrap in InternalBasePtr
            int64_t entityIBP = 0;
            new (&entityIBP) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(newEntity));   // B.3.3 bridge

            // Find component on new entity (DAT_1401c7c80 type)
            fn_InternalBasePtr_CheckPtr(&entityIBP);
            void* comp = FindComponentOnEntity(entityIBP, *s_typeId_1401c7c80);

            // Wrap component in IBP
            int64_t compIBP = 0;
            new (&compIBP) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(comp));   // B.3.3 bridge

            // Position entity at cell with random rotation
            void* randomPtr = *(void**)(gameWorld + 0xd8);
            fn_InternalBasePtr_CheckPtr(&compIBP);
            int randVal = s_fn_Random_RandTol(randomPtr, 2);
            s_fn_PositionEntity(compIBP, cellIndex, (float)randVal * ROTATION_SCALE);

            // Handle secondary entity (+0x30) if present
            if (*(int64_t*)(s + 0x30) != 0) {
                fn_InternalBasePtr_CheckPtr(s + 0x30);
                uintptr_t secondaryEntity = *(uintptr_t*)(s + 0x30);
                uintptr_t secDesc = *(uintptr_t*)(secondaryEntity + 0x28);

                // Search secondary entity's description for DAT_1401c8118 component
                int secCount = *(int*)(secDesc + 0x30);
                for (int i = 0; i < secCount; i++) {
                    void* secComp = *(void**)(*(uintptr_t*)(secDesc + 0x28) + (int64_t)i * 8);
                    uintptr_t secVtable = *(uintptr_t*)secComp;
                    typedef uint32_t (*GetType_t)();
                    uint32_t secTypeId = ((GetType_t)(*(void**)(secVtable + 0x10)))();
                    if (secTypeId == *s_typeId_1401c8118) {
                        fn_IsDerivedFrom(secComp, *s_typeId_1401c8118);
                        break;
                    }
                }
            }

            // Set quantity on new entity component
            fn_InternalBasePtr_CheckPtr(&compIBP);
            *(int32_t*)(compIBP + 0x28) = batchSize;
            *(int64_t*)(compIBP + 0x30) = 0;


            // Register raw material with resource tracker
            int64_t sourceIBP = *(int64_t*)(s + 0x30);
            s_fn_RegisterRawMaterial(
                *(int64_t*)(gameWorld + 0xf0), &sourceIBP,
                *(char*)(s + 0x51), descEntity, batchSize);

            // Register entity with entity list
            int64_t entityRef = entityIBP;
            int typeGroup = *(int*)(descEntity + 0x20);
            s_fn_RegisterEntity(*(void***)(gameWorld + 0xe8), &entityRef, typeGroup);

            // Handle place-near behavior: link to secondary entity
            if (*(char*)(s + 0x51) != '\0' && *(int64_t*)(s + 0x30) != 0) {
                fn_InternalBasePtr_CheckPtr(s + 0x30);
                int64_t childIBP = 0;
                s_fn_GetChildEntities(*(int64_t*)(s + 0x30), &childIBP);
                if (childIBP != 0) {
                    fn_InternalBasePtr_CheckPtr(&entityIBP);
                    void* rdCompIBP = nullptr;
                    s_fn_FindComponent_RD(entityIBP, &rdCompIBP);
                    fn_InternalBasePtr_CheckPtr(&rdCompIBP);
                    uint32_t rdValue = *(uint32_t*)(*(uintptr_t*)&rdCompIBP + 0x28);

                    fn_InternalBasePtr_CheckPtr(&childIBP);
                    // B.3.2 bridge: FUN_140044810 == Collection::Array<int32>::
                    // SetSize(n, fill) (3-arg form; ActRm_DynArrayPush4_44810 in
                    // action_base_rawmat.cpp), gated on the byte flag at +0x28 and
                    // appending rdValue as the new tail element.
                    if (*reinterpret_cast<char*>(childIBP + 0x28) != 0) {
                        int newCount = *reinterpret_cast<int*>(childIBP + 0x38) + 1;
                        GameColl::AsArray<GameColl::I32>(reinterpret_cast<void*>(childIBP + 0x30))
                            .SetSize(newCount, GameColl::I32{ static_cast<int32_t>(rdValue) });
                    }
                }
            }
        }

        totalItems -= batchSize;
    }

    return 0;
}

// ============================================================
// GetTarget (0x1400c6aa0)
// Returns true if param matches entity at +0x28->+0x10, or +0x30, or +0x38.
// ============================================================
int CreateRawMaterial_GetTarget_Impl(CreateRawMaterial* self, void* param) {
    CreateRawMaterial_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    uint8_t* s = (uint8_t*)self;

    // Check +0x28 -> building entity -> +0x10 (the actual target entity)
    fn_InternalBasePtr_CheckPtr(s + 0x28);
    uintptr_t building = *(uintptr_t*)(s + 0x28);
    uintptr_t targetEntity = *(uintptr_t*)(building + 0x10);

    // Use a local IBP to CheckPtr the target
    uintptr_t targetIBP = targetEntity;
    fn_InternalBasePtr_CheckPtr(&targetIBP);

    if (*(uintptr_t*)param == targetIBP) {
        return 1;
    }

    // Check +0x30 (secondary entity)
    fn_InternalBasePtr_CheckPtr(param);
    if (*(int64_t*)(s + 0x30) == *(int64_t*)param) {
        return 1;
    }

    // Check +0x38 (tertiary entity)
    fn_InternalBasePtr_CheckPtr(param);
    if (*(int64_t*)(s + 0x38) == *(int64_t*)param) {
        return 1;
    }

    return 0;
}

// ============================================================
// Standalone factory + vtable — Action family (notes/FACTORY_CASCADE_145_BRIEF.md).
// Vtable @ 0x14015fd10 (10 slots), size 0x58, parent "Action".
// Shared slot helpers live in action_base_standalone.{h,cpp}.
// ============================================================
#include "action_base_standalone.h"
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t CreateRawMaterial_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("CreateRawMaterial");
    return c;
}

static void* g_CreateRawMaterial_Vtable[10] = {
    (void*)&Action_ScalarDtor,        // +0x00 ScalarDtor
    (void*)&Action_Slot1,             // +0x08 slot1 (returns null)
    (void*)&CreateRawMaterial_GetTypeId,           // +0x10 GetTypeId
    (void*)&CreateRawMaterial_Serialize_Impl,      // +0x18 Serialize
    (void*)&CreateRawMaterial_AbstractImpl_Impl,   // +0x20 AbstractImpl
    (void*)&Action_BaseExecute,   // +0x28 Execute
    (void*)&Action_Enter,  // +0x30 Enter
    (void*)&Action_Exit,   // +0x38 Exit
    (void*)&Action_BaseCheckEntity,  // +0x40 CheckEntity
    (void*)&CreateRawMaterial_GetTarget_Impl, // +0x48 GetTarget
};

extern "C" void RegisterCreateRawMaterialVtable() {
    RegisterExeAddr(0x14015fd10, (uintptr_t)g_CreateRawMaterial_Vtable);
}
extern "C" void* CreateRawMaterial_CreateInPlace(void* mem) {
    CreateRawMaterial_ResolveFuncs();
    return Action_InitInPlace(mem, 0x58, g_CreateRawMaterial_Vtable);
}
extern "C" void* CreateRawMaterial_Factory(void) {
    CreateRawMaterial_ResolveFuncs();
    return Action_Alloc(0x58, g_CreateRawMaterial_Vtable);
}
REGISTER_TYPE(CreateRawMaterial, "Action", &CreateRawMaterial_Factory, &CreateRawMaterial_CreateInPlace);
