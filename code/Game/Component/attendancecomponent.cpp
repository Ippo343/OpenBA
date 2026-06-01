// code/Game/Component/attendancecomponent.cpp
// Clean reimplementation of AttendanceComponent::Update from Application-x64.exe.
//
// AttendanceComponent manages a queue of citizens waiting to attend a building
// (e.g., church, tavern, school). When the building is idle (not busy), the
// component processes the attendance queue.
//
// Update logic (0x140044370, 22 instructions):
//   1. Check if queue count (+0x30 as int) is nonzero; early-out if empty
//   2. Get the entity's building component via FUN_140002e10
//   3. Check if building is idle (building+0x60 == 0)
//   4. If idle, call FUN_140043f60 to process the attendance queue
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- portable for standalone exe.

#include "Component/attendancecomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

// Object::InternalBasePtr::CheckPtr -- dispatch slot at [0x140152ab0]
typedef void (*fn_CheckPtr_t)(void* internalBasePtrAddr);
static fn_CheckPtr_t s_fn_CheckPtr = nullptr;

// FUN_140002e10(entityPtr, outBuildingPtr) -- get building component from entity
typedef void (*fn_GetBuildingComponent_t)(void* entity, void* outPtr);
static fn_GetBuildingComponent_t s_fn_GetBuildingComponent = nullptr;

// FUN_140043f60(self) -- process attendance queue
typedef void (*fn_ProcessAttendanceQueue_t)(void* self);
static fn_ProcessAttendanceQueue_t s_fn_ProcessAttendanceQueue = nullptr;

static bool s_initialized = false;

// ============================================================
// AttendanceComponent::Update (vtable slot [9], 0x140044370)
// ============================================================
void AttendanceComponent_Update(AttendanceComponent* self) {
    uintptr_t selfAddr = (uintptr_t)self;

    // Check if queue count at +0x30 (as int32) is nonzero
    int32_t queueCount = *(int32_t*)(selfAddr + 0x30);
    if (queueCount == 0)
        return;

    // CheckPtr on owner smart pointer at +0x10
    if (s_fn_CheckPtr)
        s_fn_CheckPtr((void*)(selfAddr + 0x10));

    // Get entity pointer
    void* entity = *(void**)(selfAddr + 0x10);

    // Get building component from entity
    int64_t buildingPtr = 0;
    if (s_fn_GetBuildingComponent)
        s_fn_GetBuildingComponent(entity, &buildingPtr);

    // CheckPtr on returned building pointer
    if (s_fn_CheckPtr)
        s_fn_CheckPtr(&buildingPtr);

    // Check if building is idle (building+0x60 == 0)
    int32_t busyState = *(int32_t*)(buildingPtr + 0x60);
    if (busyState == 0) {
        // Building is idle -- process the attendance queue
        if (s_fn_ProcessAttendanceQueue)
            s_fn_ProcessAttendanceQueue(self);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void AttendanceComponent_InitFuncs() {
    if (s_initialized) return;

    // CheckPtr -- indirect call through dispatch slot at [0x140152ab0]
    uintptr_t slot = ResolveAddr(Module::Exe, 0x140152ab0);
    if (slot) s_fn_CheckPtr = *reinterpret_cast<fn_CheckPtr_t*>(slot);

    s_fn_GetBuildingComponent = reinterpret_cast<fn_GetBuildingComponent_t>(
        ResolveAddr(Module::Exe, 0x140002e10));

    s_fn_ProcessAttendanceQueue = reinterpret_cast<fn_ProcessAttendanceQueue_t>(
        ResolveAddr(Module::Exe, 0x140043f60));

    s_initialized = true;

#ifdef _DEBUG
    printf("[AttendanceComponent] Funcs resolved: CheckPtr=%p GetBuilding=%p ProcessQueue=%p\n",
           (void*)s_fn_CheckPtr, (void*)s_fn_GetBuildingComponent,
           (void*)s_fn_ProcessAttendanceQueue);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// Inert-but-valid AttendanceComponent: size 0x58 + Object::Internal base + shared stub
// vtable + capability flags +0x20=0x8a (from AttendanceComponent_Ctor) + zeroed body.
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

COMPONENT_INERT_FACTORY(AttendanceComponent, 0x58, 0x8a)
REGISTER_TYPE(AttendanceComponent, "ControllerComponent",
              &AttendanceComponent_Factory, &AttendanceComponent_CreateInPlace);
#endif // STANDALONE_EXE
