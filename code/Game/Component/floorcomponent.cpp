// code/Game/Component/floorcomponent.cpp
// Clean reimplementation of FloorComponent lifecycle methods from Application-x64.exe.
//
// FloorComponent (0x30 bytes) -- Component subclass.
// Fields: floorFlag (byte +0x28), floorId (int32 +0x2C, init -1).
// Manages floor/ground plane rendering for buildings.
//
// Vtable at 0x140163360:
//   [3] Shutdown  = 0x14011F560 -- serialize floorFlag as bool
//   [6] Draw      = 0x14011F340 -- IsDerivedFrom check + conditional floor render
//   [7] OnCreate  = 0x14011F2A0 -- remove existing floor mesh
//  [11] OnUpdate2 = 0x14011F540 -- 12-byte stub (unhookable, not reimplemented here)
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- portable for standalone exe.

#include "Component/floorcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

// Component_Serialize (base class) -- at 0x1401189b0
typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

// IO::Serializer::Serializer(stream, version) -- dispatch slot at 0x140152ad8
typedef void* (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

// IO::Serializer::~Serializer() -- dispatch slot at 0x140152ad0
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

// IO::Serializer::Serialize(bool*) -- dispatch slot at 0x140152a58
typedef void (*fn_Serializer_SerializeBool_t)(void* self, bool* value, const char* name);
static fn_Serializer_SerializeBool_t s_fn_Serializer_SerializeBool = nullptr;

// IsDerivedFrom -- dispatch slot at 0x140152a98
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* obj, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_d = nullptr;

// InternalBasePtr::CheckPtr -- dispatch at 0x140152ab0
typedef void (*fn_CheckPtr_t)(void* ptr);
static fn_CheckPtr_t s_fn_CheckPtr = nullptr;

// FUN_140004680 -- get entity data (owner -> entity data ptr)
typedef void* (*fn_GetEntityData_t)(void* owner, void* outPtr);
static fn_GetEntityData_t s_fn_GetEntityData = nullptr;

// FUN_14009cba0 -- remove floor mesh from picking scene
typedef void (*fn_RemoveFloorMesh_t)(void* scene, uint32_t floorId,
    int x, int y, int w, int h);
static fn_RemoveFloorMesh_t s_fn_RemoveFloorMesh = nullptr;

// FloorComponent_sub_f390 -- floor mesh placement sub-function (0x14011F390)
typedef void (*fn_FloorSubF390_t)(void* self);
static fn_FloorSubF390_t s_fn_FloorSubF390 = nullptr;

// Type ID for floor-related building type (DAT_1401c7708)
static uint32_t* s_pFloorBuildingTypeId = nullptr;

// Game state singleton
static void** s_pGameState = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    uintptr_t slot;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));
    s_fn_GetEntityData = reinterpret_cast<fn_GetEntityData_t>(
        ResolveAddr(Module::Exe, 0x140004680));
    s_fn_RemoveFloorMesh = reinterpret_cast<fn_RemoveFloorMesh_t>(
        ResolveAddr(Module::Exe, 0x14009cba0));
    s_fn_FloorSubF390 = reinterpret_cast<fn_FloorSubF390_t>(
        ResolveAddr(Module::Exe, 0x14011F390));
    s_pGameState = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));
    s_pFloorBuildingTypeId = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7708));

    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = reinterpret_cast<fn_Serializer_Ctor_t>(
        *reinterpret_cast<uintptr_t*>(slot));

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = reinterpret_cast<fn_Serializer_Dtor_t>(
        *reinterpret_cast<uintptr_t*>(slot));

    slot = ResolveAddr(Module::Exe, 0x140152a58);
    if (slot) s_fn_Serializer_SerializeBool = reinterpret_cast<fn_Serializer_SerializeBool_t>(
        *reinterpret_cast<uintptr_t*>(slot));

    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_d = reinterpret_cast<fn_IsDerivedFrom_dispatch_t>(
        *reinterpret_cast<uintptr_t*>(slot));

    slot = ResolveAddr(Module::Exe, 0x140152ab0);
    if (slot) s_fn_CheckPtr = reinterpret_cast<fn_CheckPtr_t>(
        *reinterpret_cast<uintptr_t*>(slot));

    s_initialized = true;
}

// ============================================================
// FloorComponent::Shutdown (vtable slot [3], 0x14011F560)
// ============================================================
// Serializes base component, then serializes floorFlag (bool at +0x28).

void FloorComponent_Shutdown(FloorComponent* self, void* stream) {
    EnsureHelpers();

    if (s_fn_Component_Serialize) {
        s_fn_Component_Serialize(self, stream);
    }

    uint8_t serializer[24];
    if (s_fn_Serializer_Ctor) {
        s_fn_Serializer_Ctor(serializer, stream, 0);
    }
    if (s_fn_Serializer_SerializeBool) {
        s_fn_Serializer_SerializeBool(serializer, (bool*)&self->floorFlag, nullptr);
    }
    if (s_fn_Serializer_Dtor) {
        s_fn_Serializer_Dtor(serializer);
    }
}

// ============================================================
// FloorComponent::Draw (vtable slot [6], 0x14011F340)
// ============================================================
// Checks IsDerivedFrom on _unk18 with floor building type ID.
// If building[0x28] != 0, calls FloorComponent_sub_f390 for floor mesh placement.
//
// Original disassembly:
//   MOV RBX,[RCX+0x18]              ; get sibling component
//   MOV EDX,DAT_1401c7708           ; floor building type ID
//   CALL [0x140152a98]              ; IsDerivedFrom
//   CMOVNZ RDX,RBX                 ; building = derived ? obj : nullptr
//   CMP byte [RDX+0x28],0          ; check building flag
//   JZ skip
//   CALL FloorComponent_sub_f390

void FloorComponent_Draw(FloorComponent* self) {
    EnsureHelpers();

    if (!s_fn_IsDerivedFrom_d || !s_pFloorBuildingTypeId) return;

    void* obj = self->_unk18;
    bool isDerived = s_fn_IsDerivedFrom_d(obj, *s_pFloorBuildingTypeId);
    void* building = isDerived ? obj : nullptr;

    if (!building) return;

    uint8_t flag = *(uint8_t*)((uintptr_t)building + 0x28);
    if (flag != 0) {
        if (s_fn_FloorSubF390) {
            s_fn_FloorSubF390(self);
        }
    }
}

// ============================================================
// FloorComponent::OnCreate (vtable slot [7], 0x14011F2A0)
// ============================================================
// If floorId >= 0, removes the existing floor mesh from the picking
// scene, then resets floorId to -1 and floorFlag to 0.

void FloorComponent_OnCreate(FloorComponent* self) {
    EnsureHelpers();

    if (self->floorId >= 0) {
        // Get scene from game state
        int* pScene = *(int**)((uintptr_t)*s_pGameState + 0xF8);

        // Check owner pointer
        if (s_fn_CheckPtr) {
            s_fn_CheckPtr((void*)((uintptr_t)self + 0x10));
        }

        // Get entity data from owner
        uintptr_t entityData = 0;
        if (s_fn_GetEntityData) {
            s_fn_GetEntityData(*(void**)((uintptr_t)self + 0x10), &entityData);
        }
        if (s_fn_CheckPtr) {
            s_fn_CheckPtr(&entityData);
        }

        // Extract coordinates from entity data
        int16_t x = *(int16_t*)(entityData + 0x28);
        int16_t y = *(int16_t*)(entityData + 0x2a);
        int16_t w = *(int16_t*)(entityData + 0x2c);
        int16_t h = *(int16_t*)(entityData + 0x2e);

        if (s_fn_RemoveFloorMesh) {
            s_fn_RemoveFloorMesh(pScene, (uint32_t)self->floorId,
                (int)x, (int)y, (int)w, (int)h);
        }

        self->floorId = -1;
        self->floorFlag = 0;
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void FloorComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[FloorComponent] Funcs resolved: Serialize=%p FloorSub=%p RemoveMesh=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_FloorSubF390,
           (void*)s_fn_RemoveFloorMesh);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
COMPONENT_INERT_FACTORY(FloorComponent, 0x30, 0xC3)
REGISTER_TYPE(FloorComponent, "Component",
              &FloorComponent_Factory, &FloorComponent_CreateInPlace);
#endif // STANDALONE_EXE
