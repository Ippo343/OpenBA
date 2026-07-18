// code/Game/Work/buildroad.cpp
// Clean C++ reimplementation of BuildRoad Work class methods.
// Source: Game/Work/BuildRoad.cpp (from profile build debug strings)
//
// BuildRoad — road construction work action. Size = 0x70.
// Inherits: Object::Internal -> Work (0x48) -> BuildRoad
// Ctor: 0x1400e4ad0, Vtable: 0x140161020
//
// Decompiled from Application-x64.exe using Ghidra.

#include "BuildActions.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Local function pointers resolved at init time
// (NOT in engine_funcs.h — BuildRoad-specific helpers)
// ============================================================

// Object::Internal::Internal(this) — via IAT at 0x140152768
static void* (*s_fn_ObjectInternal_Ctor)(void* self) = nullptr;

// Object::Internal::~Internal(this) — via IAT at 0x140152770
static void (*s_fn_ObjectInternal_Dtor)(void* self) = nullptr;

// Work::Serialize(this, stream) — base class serialize at 0x1400f7400
static void (*s_fn_Work_Serialize)(void* self, void* stream) = nullptr;

// IO::Serializer ctor/dtor/getversion — via IAT
static void (*s_fn_Serializer_Ctor)(void* self, void* stream, int version) = nullptr;
static void (*s_fn_Serializer_Dtor)(void* self) = nullptr;
static int  (*s_fn_Serializer_GetVersion)(void* self) = nullptr;

// Object::InternalBasePtr::Serialize(this, serializer) — via IAT at 0x140152aa0
static void (*s_fn_IBP_Serialize)(void* basePtr, void* serializer) = nullptr;

// FUN_1400f7100 — Work::ValidateTask helper (checks if assigned task still valid)
static uint8_t (*s_fn_Work_ValidateTask)(void* self, void* param2, void* ibp, void** outTask) = nullptr;

// FUN_1400c5400 — task list cleanup (removes pending work entries)
static void (*s_fn_TaskListCleanup)(void* taskList) = nullptr;

// TypeId at DAT_1401c7158 — used for IsDerivedFrom check in Serialize
static uint32_t* s_typeId_1401c7158 = nullptr;

static bool s_buildroad_funcs_resolved = false;

// ============================================================
// Resolution — called once from Install
// ============================================================
bool BuildRoad_ResolveFuncs() {
    if (s_buildroad_funcs_resolved) return true;

    // Object::Internal ctor/dtor via IAT
    uintptr_t iatCtor = ResolveAddr(Module::Exe, 0x140152768);
    uintptr_t iatDtor = ResolveAddr(Module::Exe, 0x140152770);
    if (iatCtor) s_fn_ObjectInternal_Ctor = *reinterpret_cast<void*(**)(void*)>(iatCtor);
    if (iatDtor) s_fn_ObjectInternal_Dtor = *reinterpret_cast<void(**)(void*)>(iatDtor);

    // Work::Serialize
    s_fn_Work_Serialize = reinterpret_cast<void(*)(void*, void*)>(
        ResolveAddr(Module::Exe, 0x1400f7400));

    // IO::Serializer ctor/dtor via IAT
    uintptr_t iatSerCtor = ResolveAddr(Module::Exe, 0x140152ad8);
    uintptr_t iatSerDtor = ResolveAddr(Module::Exe, 0x140152ad0);
    if (iatSerCtor) s_fn_Serializer_Ctor = *reinterpret_cast<void(**)(void*, void*, int)>(iatSerCtor);
    if (iatSerDtor) s_fn_Serializer_Dtor = *reinterpret_cast<void(**)(void*)>(iatSerDtor);

    // IO::Serializer::GetVersion — correct IAT slot verified from original disasm
    uintptr_t iatGetVer = ResolveAddr(Module::Exe, 0x140152a48);
    if (iatGetVer) s_fn_Serializer_GetVersion = *reinterpret_cast<int(**)(void*)>(iatGetVer);

    // InternalBasePtr::Serialize via IAT at 0x140152aa0
    uintptr_t iatIBPSer = ResolveAddr(Module::Exe, 0x140152aa0);
    if (iatIBPSer) s_fn_IBP_Serialize = *reinterpret_cast<void(**)(void*, void*)>(iatIBPSer);

    // fn_ExternalPtr_BuildSerialize (FUN_140002340) — already in engine_funcs.h

    // Work::ValidateTask (FUN_1400f7100)
    s_fn_Work_ValidateTask = reinterpret_cast<uint8_t(*)(void*, void*, void*, void**)>(
        ResolveAddr(Module::Exe, 0x1400f7100));

    // Task list cleanup (FUN_1400c5400)
    s_fn_TaskListCleanup = reinterpret_cast<void(*)(void*)>(
        ResolveAddr(Module::Exe, 0x1400c5400));

    // TypeId for IsDerivedFrom check
    s_typeId_1401c7158 = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7158));

    bool ok = s_fn_ObjectInternal_Ctor && s_fn_ObjectInternal_Dtor &&
              s_fn_Work_Serialize && s_fn_Serializer_Ctor && s_fn_Serializer_Dtor &&
              s_fn_Serializer_GetVersion && s_fn_IBP_Serialize &&
              fn_ExternalPtr_BuildSerialize && s_fn_Work_ValidateTask &&
              s_fn_TaskListCleanup && s_typeId_1401c7158 &&
              fn_InternalBasePtr_Ctor && fn_InternalBasePtr_CheckPtr &&
              fn_IsDerivedFrom && fn_MemFreeStatic &&
              g_fn_Serialize_Int && g_pGameWorldSingleton;

#ifdef _DEBUG
    printf("[BuildRoad] ResolveFuncs: ObjCtor=%p ObjDtor=%p WorkSer=%p SerCtor=%p SerDtor=%p\n",
           (void*)s_fn_ObjectInternal_Ctor, (void*)s_fn_ObjectInternal_Dtor,
           (void*)s_fn_Work_Serialize, (void*)s_fn_Serializer_Ctor, (void*)s_fn_Serializer_Dtor);
    printf("[BuildRoad]   GetVer=%p IBPSer=%p ExtBuildSer=%p ValidateTask=%p TaskClean=%p\n",
           (void*)s_fn_Serializer_GetVersion, (void*)s_fn_IBP_Serialize,
           (void*)fn_ExternalPtr_BuildSerialize, (void*)s_fn_Work_ValidateTask,
           (void*)s_fn_TaskListCleanup);
    printf("[BuildRoad]   typeId=%p SerInt=%p GameWorld=%p\n",
           (void*)s_typeId_1401c7158, (void*)g_fn_Serialize_Int,
           g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr);
    if (!ok) printf("[BuildRoad] ERROR: some function pointers not resolved!\n");
#endif

    s_buildroad_funcs_resolved = ok;
    return ok;
}

// ============================================================
// Constructor (0x1400e4ad0)
// Object::Internal::Internal() -> Work fields -> BuildRoad vtable + fields
// ============================================================
void* BuildRoad_Ctor_Impl(BuildRoad* self) {
    s_fn_ObjectInternal_Ctor(self);

    // Work base fields
    self->vtable = (void*)ResolveAddr(Module::Exe, BuildRoadAddr::WorkVtable);
    self->field_10 = nullptr;
    self->field_18 = nullptr;
    self->field_20 = -1;
    self->field_24 = 0;
    self->field_28 = -1;
    self->field_2c = 0;
    self->field_30 = nullptr;
    self->field_38 = nullptr;
    self->field_40 = 0;

    // Set BuildRoad vtable
    self->vtable = (void*)ResolveAddr(Module::Exe, BuildRoadAddr::Vtable);

    // BuildRoad fields
    self->field_48 = nullptr;
    self->field_50 = nullptr;
    self->field_58 = -1;
    self->field_5c = 2;

    return self;
}

// ============================================================
// ScalarDtor (0x1400295b0)
// Set vtables to BuildRoad then Work, call ~Internal, optionally free.
// ============================================================
void* BuildRoad_ScalarDtor_Impl(BuildRoad* self, uint32_t flags) {
    // Set BuildRoad vtable (for correct virtual dispatch during destruction)
    self->vtable = (void*)ResolveAddr(Module::Exe, BuildRoadAddr::Vtable);
    // Set Work vtable (base class destructor phase)
    self->vtable = (void*)ResolveAddr(Module::Exe, BuildRoadAddr::WorkVtable);
    // Call Object::Internal::~Internal
    s_fn_ObjectInternal_Dtor(self);

    if (flags & 1) {
        fn_MemFreeStatic(self);
    }
    return self;
}

// ============================================================
// Serialize (0x1400e4980)
// Work_Serialize + Serializer(version=1) + serialize fields
// Serializes: road type (+0x5c via stream vtable +0x98), cell index (+0x58),
//   entity ref (+0x48), building desc (+0x50), material fields (+0x60..+0x68)
// ============================================================
void BuildRoad_Serialize_Impl(BuildRoad* self, void* stream) {
    // Call base Work::Serialize
    s_fn_Work_Serialize(self, stream);

    // Create IO::Serializer with version 1
    uint8_t serializerBuf[24];
    s_fn_Serializer_Ctor(serializerBuf, stream, 1);

    // Serialize road type (+0x5c) via stream's vtable call at +0x98
    // The Serializer stores stream ptr at offset +0x8
    void* serStream = *(void**)(serializerBuf + 8);
    void* streamVtable = *(void**)serStream;
    typedef void (*SerializeVt_t)(void* stream, void* field);
    SerializeVt_t serVtFn = *(SerializeVt_t*)((uintptr_t)streamVtable + 0x98);
    serVtFn(serStream, &self->field_5c);

    // Serialize cell index (+0x58) via IO::Serializer::Serialize
    g_fn_Serialize_Int(serializerBuf, &self->field_58, nullptr);

    // InternalBasePtr::Serialize for entity ref (+0x48)
    s_fn_IBP_Serialize(&self->field_48, serializerBuf);

    // Validate entity ref type after deserialize
    if (self->field_48 != nullptr) {
        bool valid = fn_IsDerivedFrom(self->field_48, *s_typeId_1401c7158);
        if (!valid) {
            self->field_48 = nullptr;
        }
    }

    // ExternalPtr serialize for building description (+0x50)
    fn_ExternalPtr_BuildSerialize(&self->field_50, serializerBuf);

    // Version-dependent fields
    int version = s_fn_Serializer_GetVersion(serializerBuf);
    if (version < 1) {
        // Default values for old save format
        self->field_60 = 0xff;
        self->field_64 = 0;
        self->field_68 = 2;
    } else {
        // Serialize material fields
        g_fn_Serialize_Int(serializerBuf, &self->field_60, nullptr);
        g_fn_Serialize_Int(serializerBuf, &self->field_64, nullptr);
        g_fn_Serialize_Int(serializerBuf, &self->field_68, nullptr);
    }

    s_fn_Serializer_Dtor(serializerBuf);
}

// ============================================================
// OnComplete (0x1400e4150) — slot 6
// Clears target entity ref, returns (param4 == 1).
// ============================================================
int BuildRoad_OnComplete_Impl(BuildRoad* self, void* /*param2*/, void* /*param3*/, int param4) {
    // Clear entity ref: InternalBasePtr_Ctor(temp, NULL) then store to +0x48
    uintptr_t temp = 0;
    fn_InternalBasePtr_Ctor(&temp, nullptr);
    self->field_48 = (void*)temp;

    return (param4 == 1) ? 1 : 0;
}

// ============================================================
// OnCancel (0x1400e4120) — slot 7
// Clears target entity ref.
// ============================================================
int BuildRoad_OnCancel_Impl(BuildRoad* self) {
    uintptr_t temp = 0;
    fn_InternalBasePtr_Ctor(&temp, nullptr);
    self->field_48 = (void*)temp;
    return 0;
}

// ============================================================
// Cleanup (0x1400e4910) — slot 8
// If target entity exists, clean up its pending task list.
// Walks entity -> +0x278 (StateMachineComponent) -> +0x48 (task list)
// ============================================================
int BuildRoad_Cleanup_Impl(BuildRoad* self) {
    if (self->field_48 == nullptr) return 0;

    fn_InternalBasePtr_CheckPtr(&self->field_48);
    uintptr_t entity = (uintptr_t)self->field_48;

    fn_InternalBasePtr_CheckPtr((void*)(entity + 0x278));
    uintptr_t smComp = *(uintptr_t*)(entity + 0x278);

    // Call task list cleanup: FUN_1400c5400(smComp + 0x48)
    s_fn_TaskListCleanup((void*)(smComp + 0x48));
    return 0;
}

// ============================================================
// Validate (0x1400e40c0) — slot 9
// Checks if the assigned task (from field_48) is still valid.
// Uses shared Work::ValidateTask helper (FUN_1400f7100).
// ============================================================
int BuildRoad_Validate_Impl(BuildRoad* self, void* param2) {
    uintptr_t entityRef = (uintptr_t)self->field_48;

    if (entityRef != 0) {
        // Call shared validate helper with entity ref
        uintptr_t ibpCopy[1] = { entityRef };
        uint8_t result = s_fn_Work_ValidateTask(self, param2, ibpCopy, nullptr);
        if (result == 0) {
            // Task invalid — clear entity ref
            uintptr_t temp = 0;
            fn_InternalBasePtr_Ctor(&temp, nullptr);
            self->field_48 = (void*)temp;
            return 0;
        }
    }
    return 1;
}

// ============================================================
// GetTarget (0x1400e4960) — slot 10
// Returns the game world's road build target entity.
// Uses hidden return pointer (member function ABI: RCX=this, RDX=retbuf).
// NOT HOOKED — RIP-relative prologue (bug class 2).
// ============================================================
void* BuildRoad_GetTarget_Impl(BuildRoad* /*self*/, void* retbuf) {
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t roadMgr = *(uintptr_t*)(gameWorld + 0xb0);
    *(uintptr_t*)retbuf = *(uintptr_t*)(roadMgr + 0x38);
    return retbuf;
}

// ============================================================
// GetPriority (0x1400295a0) — slot 11
// Returns true (1) if field_48 is non-null, false (0) otherwise.
// NOT HOOKED — only 10 bytes total, too small.
// ============================================================
int BuildRoad_GetPriority_Impl(BuildRoad* self) {
    return (self->field_48 != nullptr) ? 1 : 0;
}

// ============================================================
// Execute (0x1400e4190) — slot 4
// Road construction work logic. Determines what the citizen
// needs to do next: build directly, haul resources, or assign
// to a workplace.
// Returns: 1 = retry/not ready, 2 = action pushed
// ============================================================

// Helper: get position data from entity IBP chain
static uintptr_t GetPositionData(void* entityIBP) {
    fn_InternalBasePtr_CheckPtr(entityIBP);
    uintptr_t entity = *(uintptr_t*)entityIBP;
    fn_InternalBasePtr_CheckPtr((void*)(entity + 0x278));
    uintptr_t comp278 = *(uintptr_t*)(entity + 0x278);
    fn_InternalBasePtr_CheckPtr((void*)(comp278 + 0x68));
    return *(uintptr_t*)(comp278 + 0x68);
}

// Helper: convert position data to clamped map cell index
static int PosToCellIndex(int* mapGrid, uintptr_t posData) {
    int mapSize = *mapGrid;
    int halfMap = mapSize >> 1;
    float posX = *(float*)(posData + 0x158);
    float posY = *(float*)(posData + 0x15c);
    int cellX = (int)(posX + (float)halfMap);
    int cellY = (int)(posY + (float)halfMap);
    if (cellX < 0) cellX = 0;
    if (cellX >= mapSize) cellX = mapSize - 1;
    if (cellY < 0) cellY = 0;
    if (cellY >= mapSize) cellY = mapSize - 1;
    return mapSize * cellY + cellX;
}

// Helper: convert cell index to world-space Vector3 via dispatch
static void CellToWorldVec3(int* mapGrid, int cellIndex, float* outVec3) {
    int mapSize = *mapGrid;
    float halfOffset = (float)(mapSize >> 1) - 0.5f;
    float cellX = (float)(cellIndex % mapSize) - halfOffset;
    float cellY = (float)(cellIndex / mapSize) - halfOffset;
    fn_Vector3_Ctor(outVec3, cellX, cellY, 0.0f);
}

// Shared epilogue: finalize action on citizen entity
static int FinalizeAction(BuildRoad* self, void* entityIBP, void* actionIBP, int priority) {
    uintptr_t citizenEntity = *(uintptr_t*)entityIBP;

    uintptr_t vtable = *(uintptr_t*)self;
    typedef uint64_t* (*VtSlot50_t)(BuildRoad*, void*);
    VtSlot50_t vtShared = *(VtSlot50_t*)(vtable + 0x50);
    uint64_t* result = vtShared(self, actionIBP);

    uint64_t actionPtr = *result;
    *(int32_t*)(citizenEntity + 0x30) = priority;
    *(uint32_t*)(citizenEntity + 0x54) |= 4;
    *(int32_t*)(citizenEntity + 0xb0) = 0;

    if (actionPtr == *(uint64_t*)(citizenEntity + 0x1f0) && priority >= 0) {
        *(uint32_t*)(citizenEntity + 0x88) |= 0x20000;
    }

    self->field_48 = *(void**)entityIBP;
    return 2;
}

int BuildRoad_Execute(BuildRoad* self, void* entityIBP) {
    if (self->field_48 != nullptr) {
        return 1;
    }

    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
    int* mapGrid = *(int**)(gameWorld + 0xf8);

    uintptr_t posData = GetPositionData(entityIBP);
    int mapSize = *mapGrid;
    uint32_t heightU32 = *(uint32_t*)(posData + 0x160);
    int cellIndex = PosToCellIndex(mapGrid, posData);

    int targetCell = self->field_58;

    uint64_t passResult = fn_Map_CheckPassable(mapGrid, 0xe1f, (uint32_t)cellIndex, targetCell);
    if ((char)passResult == '\0') {
        return 1;
    }

    uintptr_t structArrayBase = *(uintptr_t*)((uintptr_t)mapGrid + 0x80);
    int64_t structEntity = *(int64_t*)(structArrayBase + 8 + (int64_t)targetCell * 0x30);

    if (structEntity != 0) {
        fn_InternalBasePtr_CheckPtr((void*)&structEntity);
        uint64_t workplaceComp = 0;
        fn_FindComponent_WorkplaceC(structEntity, &workplaceComp);

        if (workplaceComp != 0) {
            fn_InternalBasePtr_CheckPtr((void*)&structEntity);
            uint64_t storageLocComp = 0;
            fn_FindComponent_StorageLocC(structEntity, &storageLocComp);
            fn_InternalBasePtr_CheckPtr((void*)&storageLocComp);

            int workerCount = *(int*)(*(uintptr_t*)&storageLocComp + 0x28);
            uint64_t citizenRef = *(uint64_t*)entityIBP;

            uintptr_t assignMgr = *(uintptr_t*)(gameWorld + 0x118);
            uint64_t assignResult = fn_TryAssignWorkplace(assignMgr, &citizenRef, workerCount, '\0');
            if ((char)assignResult != '\0') {
                return 2;
            }
            return 1;
        }

        uint64_t buildStateComp = 0;
        fn_FindComponent_BuildStateC(structEntity, &buildStateComp);

        if (buildStateComp == 0) {
            return 1;
        }

        fn_InternalBasePtr_CheckPtr((void*)&structEntity);
        fn_FindComponent_BuildStateC(structEntity, &buildStateComp);
        fn_InternalBasePtr_CheckPtr((void*)&buildStateComp);

        uint32_t bsFlags = *(uint32_t*)(*(uintptr_t*)&buildStateComp + 0x20);
        if ((bsFlags >> 20) & 1) {
            return 1;
        }

        posData = GetPositionData(entityIBP);
        mapSize = *mapGrid;
        heightU32 = *(uint32_t*)(posData + 0x160);
        cellIndex = PosToCellIndex(mapGrid, posData);

        int adjacentCell = fn_Map_FindNearestPassable(mapGrid, targetCell, cellIndex, 0xe07);
        if (adjacentCell < 0) {
            return 1;
        }

        passResult = fn_Map_CheckPassable(mapGrid, 0xe1f, (uint32_t)adjacentCell, cellIndex);
        if ((char)passResult == '\0') {
            return 1;
        }

        float worldPos[3];
        CellToWorldVec3(mapGrid, adjacentCell, worldPos);

        float terrainPos[4] = {};
        fn_MapGetTerrainPos(mapGrid, terrainPos, worldPos, '\0', nullptr);

        uint64_t targetRef = buildStateComp;
        uint64_t citizenRef = *(uint64_t*)entityIBP;

        fn_PerformWork_Action_Ctor3(
            &citizenRef, self->field_20, &targetRef, terrainPos, 0, 0);

        int priority = self->field_20;
        fn_InternalBasePtr_CheckPtr(entityIBP);
        return FinalizeAction(self, entityIBP, &targetRef, priority);
    }

    if ((int64_t)self->field_50 == 0) {
        uint64_t actionRef = 0;
        fn_InternalBasePtr_Ctor(&actionRef, nullptr);

        float velocity[3] = { 0.0f, 0.0f, 0.0f };
        float worldPos[3];
        CellToWorldVec3(mapGrid, targetCell, worldPos);

        float terrainPos[4] = {};
        fn_MapGetTerrainPos(mapGrid, terrainPos, worldPos, '\0', nullptr);

        uint64_t citizenRef = *(uint64_t*)entityIBP;

        fn_PerformWork_Action_Ctor(
            &citizenRef, self->field_20, (void*)terrainPos,
            (uint64_t)(uint32_t)targetCell,
            *(uint32_t*)((uintptr_t)self + 0x5c), 0xff, 1, 0, 0,
            velocity, &actionRef);

        int priority = self->field_20;
        fn_InternalBasePtr_CheckPtr(entityIBP);
        return FinalizeAction(self, entityIBP, &actionRef, priority);

    } else {
        fn_InternalBasePtr_CheckPtr(entityIBP);
        uint64_t homeEntity = *(uint64_t*)(*(uintptr_t*)entityIBP + 0x10);
        fn_InternalBasePtr_CheckPtr(&homeEntity);

        uint64_t storageComp = 0;
        fn_FindComponent_Storage((void*)(uintptr_t)homeEntity, &storageComp);
        fn_InternalBasePtr_CheckPtr(&storageComp);

        int available = fn_BuildRoad_QueryStorage(storageComp, (int64_t)self->field_50);

        if (available > 0) {
            uint64_t actionRef = 0;
            fn_InternalBasePtr_Ctor(&actionRef, nullptr);

            float velocity[3] = { 0.0f, 0.0f, 0.0f };
            float worldPos[3];
            CellToWorldVec3(mapGrid, targetCell, worldPos);

            float terrainPos[4] = {};
            fn_MapGetTerrainPos(mapGrid, terrainPos, worldPos, '\0', nullptr);

            uint64_t citizenRef = *(uint64_t*)entityIBP;

            fn_PerformWork_Action_Ctor(
                &citizenRef, self->field_20, (void*)terrainPos,
                (uint64_t)(uint32_t)targetCell,
                *(uint32_t*)((uintptr_t)self + 0x5c), 0xff, 1,
                (int64_t)self->field_50, 0, velocity, &actionRef);

            int priority = self->field_20;
            fn_InternalBasePtr_CheckPtr(entityIBP);
            return FinalizeAction(self, entityIBP, &actionRef, priority);

        } else {
            mapGrid = *(int**)(gameWorld + 0xf8);
            void** citizenList = *(void***)(gameWorld + 0xf0);

            posData = GetPositionData(entityIBP);
            mapSize = *mapGrid;
            heightU32 = *(uint32_t*)(posData + 0x160);
            cellIndex = PosToCellIndex(mapGrid, posData);

            float velocity[3] = { 0.0f, 0.0f, 0.0f };
            uint64_t foundStorageIBP = 0;

            uint64_t foundEntity = 0;
            fn_FindStorageForResource(
                citizenList, &foundEntity,
                mapSize * ((cellIndex / mapSize)) + (cellIndex % mapSize),
                (int64_t)self->field_50, 1, '\0', velocity, &foundStorageIBP);

            fn_FindComponent_RawMaterialC((int64_t)self->field_50, &foundStorageIBP);

            if (foundEntity == 0) {
                return 1;
            }

            fn_InternalBasePtr_CheckPtr(entityIBP);
            int remainingCapacity = fn_GetRemainingCapacity(*(uintptr_t*)entityIBP);

            if (remainingCapacity < *(int32_t*)(foundStorageIBP + 0x48)) {
                fn_InternalBasePtr_CheckPtr(entityIBP);
                fn_CitizenAI_LongTermPlan(*(void**)entityIBP);
                return 2;
            }

            foundStorageIBP = foundEntity;

            float worldPos[3];
            CellToWorldVec3(mapGrid, targetCell, worldPos);

            float terrainPos[4] = {};
            fn_MapGetTerrainPos(mapGrid, terrainPos, worldPos, '\0', nullptr);

            uint64_t citizenRef = *(uint64_t*)entityIBP;

            fn_PerformWork_Action_Ctor(
                &citizenRef, self->field_20, (void*)terrainPos,
                (uint64_t)(uint32_t)targetCell,
                *(uint32_t*)((uintptr_t)self + 0x5c), 0xff, 1,
                (int64_t)self->field_50, 1, velocity, &foundStorageIBP);

            int priority = self->field_20;
            fn_InternalBasePtr_CheckPtr(entityIBP);
            return FinalizeAction(self, entityIBP, &foundStorageIBP, priority);
        }
    }
}

// ============================================================
// Standalone factory + vtable — Work family keystone (factory cascade).
// Vtable @ 0x140161020 (13 slots), size 0x70, parent "Work".
// Shared slot helpers: work_base_standalone.{h,cpp}.
// ============================================================
#include "work_base_standalone.h"
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t BuildRoad_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("BuildRoad");
    return c;
}

static void* g_BuildRoad_Vtable[13] = {
    (void*)&BuildRoad_ScalarDtor_Impl,  // +0x00 ScalarDtor
    (void*)&Work_Slot1,                 // +0x08 slot1 (null)
    (void*)&BuildRoad_GetTypeId,        // +0x10 GetTypeId
    (void*)&BuildRoad_Serialize_Impl,   // +0x18 Serialize
    (void*)&BuildRoad_Execute,          // +0x20 Execute
    (void*)&Work_Slot5,                 // +0x28 slot5 (returns 0)
    (void*)&BuildRoad_OnComplete_Impl,  // +0x30 OnComplete
    (void*)&BuildRoad_OnCancel_Impl,    // +0x38 OnCancel
    (void*)&BuildRoad_Cleanup_Impl,     // +0x40 Cleanup
    (void*)&BuildRoad_Validate_Impl,    // +0x48 Validate
    (void*)&BuildRoad_GetTarget_Impl,   // +0x50 GetTarget
    (void*)&BuildRoad_GetPriority_Impl, // +0x58 GetPriority
    (void*)&Work_Slot12,                // +0x60 slot12 (void nop)
};

extern "C" void RegisterBuildRoadVtable() {
    RegisterExeAddr(0x140161020, (uintptr_t)g_BuildRoad_Vtable);
}
extern "C" void* BuildRoad_CreateInPlace(void* mem) {
    BuildRoad_ResolveFuncs();
    return BuildRoad_Ctor_Impl((BuildRoad*)mem);
}
extern "C" void* BuildRoad_Factory(void) {
    BuildRoad_ResolveFuncs();
    void* p = Work_Alloc(0x70);
    return p ? BuildRoad_Ctor_Impl((BuildRoad*)p) : nullptr;
}
REGISTER_TYPE(BuildRoad, "Work", &BuildRoad_Factory, &BuildRoad_CreateInPlace);
