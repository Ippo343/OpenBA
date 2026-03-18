// code/Game/Work/fishingwork.cpp
// Clean C++ reimplementation of FishingWork Work class methods.
// Source: Game/Work/FishingWork.cpp (from profile build debug strings)
//
// FishingWork — fishing dock work action. Size = 0x70.
// Inherits: Object::Internal -> Work (0x48) -> FishingWork
// Ctor: 0x1400eabe0, Vtable: 0x140161320
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- it is portable for standalone exe.

#include "Work/fishingwork_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Local function pointers resolved at init time
// ============================================================

// Work::Serialize(this, stream) -- base class at 0x1400f7400
static void (*s_fn_Work_Serialize)(void* self, void* stream) = nullptr;

// IO::Serializer ctor/dtor via IAT
static void (*s_fn_Serializer_Ctor)(void* self, void* stream, int version) = nullptr;
static void (*s_fn_Serializer_Dtor)(void* self) = nullptr;

// IO::Serializer::Serialize(serializer, field, name) for int -- IAT 0x140152ac8
static void (*s_fn_Serialize_Int)(void* serializer, void* field, const char* name) = nullptr;

// Object::InternalBasePtr::Serialize(this, serializer) -- IAT 0x140152aa0
static void (*s_fn_IBP_Serialize)(void* basePtr, void* serializer) = nullptr;

// fn_IsDerivedFrom via dispatch slot 0x140152a98
static bool (*s_fn_IsDerivedFrom_Slot)(void* self, uint32_t typeId) = nullptr;

// IO::Serializer::Serialize for int array: (serializer, count, arrayPtr, name) -- IAT 0x140151278
static void (*s_fn_Serialize_IntArray)(void* serializer, int count, void* arrayPtr, const char* name) = nullptr;

// TypeId at DAT_1401c7158 -- entity type check in Serialize loop
static uint32_t* s_typeId_1401c7158 = nullptr;

// FUN_140002e10 -- find PositionComponent on entity (returns IBP)
static void* (*s_fn_FindPositionComponent)(uintptr_t entity, void* outIBP) = nullptr;

// FUN_1400c5400 -- task list cleanup
static void (*s_fn_TaskListCleanup)(void* taskList) = nullptr;

// ============================================================
// Resolution -- called once from Install
// ============================================================
static bool s_funcs_resolved = false;

bool FishingWork_ResolveFuncs() {
    if (s_funcs_resolved) return true;

    // Work::Serialize
    s_fn_Work_Serialize = reinterpret_cast<void(*)(void*, void*)>(
        ResolveAddr(Module::Exe, 0x1400f7400));

    // IO::Serializer ctor/dtor via IAT
    uintptr_t iatSerCtor = ResolveAddr(Module::Exe, 0x140152ad8);
    uintptr_t iatSerDtor = ResolveAddr(Module::Exe, 0x140152ad0);
    if (iatSerCtor) s_fn_Serializer_Ctor = *reinterpret_cast<void(**)(void*, void*, int)>(iatSerCtor);
    if (iatSerDtor) s_fn_Serializer_Dtor = *reinterpret_cast<void(**)(void*)>(iatSerDtor);

    // IO::Serializer::Serialize (int) via IAT 0x140152ac8
    uintptr_t iatSerInt = ResolveAddr(Module::Exe, 0x140152ac8);
    if (iatSerInt) s_fn_Serialize_Int = *reinterpret_cast<void(**)(void*, void*, const char*)>(iatSerInt);

    // InternalBasePtr::Serialize via IAT 0x140152aa0
    uintptr_t iatIBPSer = ResolveAddr(Module::Exe, 0x140152aa0);
    if (iatIBPSer) s_fn_IBP_Serialize = *reinterpret_cast<void(**)(void*, void*)>(iatIBPSer);

    // fn_IsDerivedFrom via dispatch slot 0x140152a98
    uintptr_t iatIsDerived = ResolveAddr(Module::Exe, 0x140152a98);
    if (iatIsDerived) s_fn_IsDerivedFrom_Slot = *reinterpret_cast<bool(**)(void*, uint32_t)>(iatIsDerived);

    // Serialize int array via IAT 0x140151278
    uintptr_t iatSerIntArr = ResolveAddr(Module::Exe, 0x140151278);
    if (iatSerIntArr) s_fn_Serialize_IntArray = *reinterpret_cast<void(**)(void*, int, void*, const char*)>(iatSerIntArr);

    // TypeId for entity IsDerivedFrom check
    s_typeId_1401c7158 = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7158));

    // FUN_140002e10 -- find component on entity (PositionComponent)
    s_fn_FindPositionComponent = reinterpret_cast<void*(*)(uintptr_t, void*)>(
        ResolveAddr(Module::Exe, 0x140002e10));

    // FUN_1400c5400 -- task list cleanup
    s_fn_TaskListCleanup = reinterpret_cast<void(*)(void*)>(
        ResolveAddr(Module::Exe, 0x1400c5400));

    bool ok = s_fn_Work_Serialize && s_fn_Serializer_Ctor && s_fn_Serializer_Dtor &&
              s_fn_Serialize_Int && s_fn_IBP_Serialize && s_fn_IsDerivedFrom_Slot &&
              s_fn_Serialize_IntArray && s_typeId_1401c7158 &&
              s_fn_FindPositionComponent && s_fn_TaskListCleanup &&
              fn_InternalBasePtr_Ctor && fn_InternalBasePtr_CheckPtr;

#ifdef _DEBUG
    printf("[FishingWork] ResolveFuncs: WorkSer=%p SerCtor=%p SerDtor=%p SerInt=%p\n",
           (void*)s_fn_Work_Serialize, (void*)s_fn_Serializer_Ctor,
           (void*)s_fn_Serializer_Dtor, (void*)s_fn_Serialize_Int);
    printf("[FishingWork]   IBPSer=%p IsDerived=%p SerIntArr=%p typeId=%p\n",
           (void*)s_fn_IBP_Serialize, (void*)s_fn_IsDerivedFrom_Slot,
           (void*)s_fn_Serialize_IntArray, (void*)s_typeId_1401c7158);
    printf("[FishingWork]   FindPos=%p TaskClean=%p IBPCtor=%p IBPCheck=%p\n",
           (void*)s_fn_FindPositionComponent, (void*)s_fn_TaskListCleanup,
           (void*)fn_InternalBasePtr_Ctor, (void*)fn_InternalBasePtr_CheckPtr);
    if (!ok) printf("[FishingWork] ERROR: some function pointers not resolved!\n");
#endif

    s_funcs_resolved = ok;
    return ok;
}

// ============================================================
// Serialize (vtable +0x18, 0x1400ea910)
// Work_Serialize + Serializer(v0) + IBP array + int array + int
// ============================================================
void FishingWork_Serialize_Impl(FishingWork* self, void* stream) {
    // Call base Work::Serialize
    s_fn_Work_Serialize(self, stream);

    // Create IO::Serializer with version 0
    uint8_t serializerBuf[24];
    s_fn_Serializer_Ctor(serializerBuf, stream, 0);

    // Serialize citizen count (+0x50)
    int count = self->citizenCount;
    s_fn_Serialize_Int(serializerBuf, &count, nullptr);

    // On load (serializer stream direction == 0 at offset +0x8),
    // resize the IBP array to match the deserialized count
    uintptr_t serStream = *(uintptr_t*)serializerBuf;
    int direction = *(int*)(serStream + 0x8);
    if (direction == 0) {
        fn_ExeArrayResize(&self->citizenRefs, count);
    }

    // Serialize each InternalBasePtr in the citizen array
    uintptr_t ibpPtr = (uintptr_t)self->citizenRefs;
    int64_t remaining = (int64_t)count;
    if (remaining > 0) {
        do {
            // Serialize the IBP entry
            s_fn_IBP_Serialize((void*)ibpPtr, serializerBuf);

            // After deserialization, validate the entity type
            void* entity = *(void**)ibpPtr;
            if (entity != nullptr) {
                bool valid = s_fn_IsDerivedFrom_Slot(entity, *s_typeId_1401c7158);
                if (!valid) {
                    *(void**)ibpPtr = nullptr;
                }
            }

            ibpPtr += 8;
            remaining--;
        } while (remaining != 0);
    }

    // Serialize spot state count (+0x60)
    int spotCount = self->spotCount;
    s_fn_Serialize_Int(serializerBuf, &spotCount, nullptr);

    // On load, resize the int array
    if (direction == 0) {
        fn_ExeArrayResize2(&self->spotStates, spotCount);
    }

    // Batch serialize the int array (spot states)
    s_fn_Serialize_IntArray(serializerBuf, spotCount, self->spotStates, nullptr);

    // Serialize retry count (+0x68)
    s_fn_Serialize_Int(serializerBuf, &self->retryCount, nullptr);

    // Destroy serializer
    s_fn_Serializer_Dtor(serializerBuf);
}

// ============================================================
// OnComplete (vtable +0x30, slot 6, 0x1400e9eb0)
// Clears the citizen ref at index (param4 & 0xf).
// If bit 15 of param4 is set, marks the corresponding spot as done (2).
// Returns 0 (via clearing upper bytes).
// ============================================================
int FishingWork_OnComplete_Impl(FishingWork* self, void* /*param2*/, void* /*param3*/, uint32_t param4) {
    // Clear citizen ref at citizen index (low 4 bits)
    uint32_t citizenIdx = param4 & 0xf;
    uintptr_t temp = 0;
    fn_InternalBasePtr_Ctor(&temp, nullptr);
    *(uintptr_t*)((uintptr_t)self->citizenRefs + (uint64_t)citizenIdx * 8) = temp;

    // If bit 15 set, mark spot as done
    if ((param4 >> 15) & 1) {
        uint32_t spotIdx = (param4 >> 4) & 0xf;
        *(int32_t*)((uintptr_t)self->spotStates + (uint64_t)spotIdx * 4) = 2;
    }

    return 0;
}

// ============================================================
// OnCancel (vtable +0x38, slot 7, 0x1400e9e80)
// Copies param2 and param3, then delegates to OnComplete (slot 6)
// via vtable call at +0x30.
// NOTE: param4 is implicitly forwarded via R9 (bug class 6).
// ============================================================
void FishingWork_OnCancel_Impl(FishingWork* self, void* param2, void* param3, uint32_t param4) {
    // Original dereferences param2/param3 (they're pointers to values),
    // copies to locals, then calls vtable +0x30 (slot 6 / OnComplete)
    uintptr_t localP3 = *(uintptr_t*)param3;
    uintptr_t localP2 = *(uintptr_t*)param2;
    uintptr_t vtable = *(uintptr_t*)self;
    typedef int (*Slot6_t)(FishingWork*, void*, void*, uint32_t);
    Slot6_t slot6fn = *(Slot6_t*)(vtable + 0x30);
    slot6fn(self, &localP2, &localP3, param4);
}

// ============================================================
// Cleanup (vtable +0x40, slot 8, 0x1400ea830)
// For each valid citizen ref, clean up its StateMachineComponent
// task list (entity -> +0x278 -> +0x48).
// ============================================================
void FishingWork_Cleanup_Impl(FishingWork* self) {
    int count = self->citizenCount;
    if (count <= 0) return;

    uintptr_t arrayBase = (uintptr_t)self->citizenRefs;
    for (int i = 0; i < count; i++) {
        uintptr_t* ibpSlot = (uintptr_t*)(arrayBase + (int64_t)i * 8);
        if (*ibpSlot != 0) {
            fn_InternalBasePtr_CheckPtr(ibpSlot);
            uintptr_t entity = *ibpSlot;

            fn_InternalBasePtr_CheckPtr((void*)(entity + 0x278));
            uintptr_t smComp = *(uintptr_t*)(entity + 0x278);

            // Call task list cleanup: FUN_1400c5400(smComp + 0x48)
            s_fn_TaskListCleanup((void*)(smComp + 0x48));
        }
    }
}

// ============================================================
// GetTarget (vtable +0x50, slot 10, 0x1400ea8b0)
// Looks up the workplace entity (+0x10), finds its component
// that holds the position/target (FUN_140002e10 finds component
// with typeId 0x1401c80e0), then returns that component's
// +0x48 field via hidden return pointer.
// ============================================================
void* FishingWork_GetTarget_Impl(FishingWork* self, void* outPtr) {
    // CheckPtr on workplace IBP at +0x10
    fn_InternalBasePtr_CheckPtr(&self->_field10);
    uintptr_t workplace = (uintptr_t)self->_field10;

    // Find component via FUN_140002e10 (finds component by typeId 0x1401c80e0)
    uintptr_t compIBP[1] = {0};
    s_fn_FindPositionComponent(workplace, compIBP);

    // CheckPtr on found component
    fn_InternalBasePtr_CheckPtr(compIBP);

    // Return component's +0x48 field
    uintptr_t comp = compIBP[0];
    *(uintptr_t*)outPtr = *(uintptr_t*)(comp + 0x48);

    return outPtr;
}
