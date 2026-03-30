// code/Game/Component/immigrationcomponent.cpp
// Clean reimplementation of ImmigrationComponent::Shutdown from Application-x64.exe.
//
// ImmigrationComponent (0x68 bytes) -- ControllerComponent subclass.
// Manages immigration/nomad arrival events.
//
// Vtable at 0x140159b30:
//   [3] Shutdown = 0x14004e4b0 -- complex serialization with entity array loop
//
// Ghidra decompilation:
//   Component_Serialize(param_1, param_2)
//   Serializer(local_20, param_2, 0); ~Serializer(local_20)  // empty controller block
//   Serializer(local_30, param_2, 0)
//   Serialize(local_30, param_1+0x50, nullptr)  // field50 (int)
//   local_res8 = *(param_1+0x60)                // array count
//   Serialize(local_30, local_res8, nullptr)
//   if loading: DynArray_Resize(param_1+0x58, count)
//   for each element in array:
//     InternalBasePtr::Serialize(element, serializer)
//     if ptr != null && !IsDerivedFrom(ptr, ComponentEntity): clear ptr
//   Time::Serialize(param_1+0x48, serializer)
//   ~Serializer(local_30)
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- portable for standalone exe.

#include "Component/immigrationcomponent_funcs.h"
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

// IO::Serializer::Serialize(int*) -- dispatch slot at 0x140152ac8
typedef void (*fn_Serializer_SerializeInt_t)(void* self, int* value, const char* name);
static fn_Serializer_SerializeInt_t s_fn_Serializer_SerializeInt = nullptr;

// System::Time::Serialize -- dispatch slot at 0x140152aa8
typedef void (*fn_Time_Serialize_t)(void* time, void* serializer);
static fn_Time_Serialize_t s_fn_Time_Serialize = nullptr;

// Object::InternalBasePtr::Serialize -- resolved globally in engine_funcs
// extern fn_InternalBasePtr_Serialize_t fn_InternalBasePtr_Serialize;

// IsDerivedFrom -- dispatch slot at 0x140152a98
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* self, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_d = nullptr;

// FUN_1400032e0 -- dynamic array resize
typedef void (*fn_DynArray_Resize_t)(void* arrayPtr, int newCount);
static fn_DynArray_Resize_t s_fn_DynArray_Resize = nullptr;

// Type ID for ComponentEntity (DAT_1401c5d70)
static uint32_t* s_pTypeId_ComponentEntity = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    uintptr_t slot;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));
    s_fn_DynArray_Resize = reinterpret_cast<fn_DynArray_Resize_t>(
        ResolveAddr(Module::Exe, 0x1400032e0));
    s_pTypeId_ComponentEntity = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c5d70));

    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = reinterpret_cast<fn_Serializer_Ctor_t>(
        *reinterpret_cast<uintptr_t*>(slot));

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = reinterpret_cast<fn_Serializer_Dtor_t>(
        *reinterpret_cast<uintptr_t*>(slot));

    slot = ResolveAddr(Module::Exe, 0x140152ac8);
    if (slot) s_fn_Serializer_SerializeInt = reinterpret_cast<fn_Serializer_SerializeInt_t>(
        *reinterpret_cast<uintptr_t*>(slot));

    slot = ResolveAddr(Module::Exe, 0x140152aa8);
    if (slot) s_fn_Time_Serialize = reinterpret_cast<fn_Time_Serialize_t>(
        *reinterpret_cast<uintptr_t*>(slot));

    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_d = reinterpret_cast<fn_IsDerivedFrom_dispatch_t>(
        *reinterpret_cast<uintptr_t*>(slot));

    s_initialized = true;
}

// ============================================================
// ImmigrationComponent::Shutdown (vtable slot [3], 0x14004e4b0)
// ============================================================
// Complex serialization: base + controller block + field50 + entity array + Time.

void ImmigrationComponent_Shutdown(ImmigrationComponent* self, void* stream) {
    EnsureHelpers();

    // Call base Component_Serialize
    if (s_fn_Component_Serialize) {
        s_fn_Component_Serialize(self, stream);
    }

    // Empty serializer block (controller compat)
    uint8_t serializer1[24];
    if (s_fn_Serializer_Ctor && s_fn_Serializer_Dtor) {
        s_fn_Serializer_Ctor(serializer1, stream, 0);
        s_fn_Serializer_Dtor(serializer1);
    }

    // Main serializer block
    uint8_t serializer2[24];
    if (s_fn_Serializer_Ctor) {
        s_fn_Serializer_Ctor(serializer2, stream, 0);
    }

    // Serialize field50 (int)
    if (s_fn_Serializer_SerializeInt) {
        s_fn_Serializer_SerializeInt(serializer2, (int*)&self->field50, nullptr);
    }

    // Serialize array count from field60
    int arrayCount = *(int*)&self->field60;
    if (s_fn_Serializer_SerializeInt) {
        s_fn_Serializer_SerializeInt(serializer2, &arrayCount, nullptr);
    }

    // If loading (serializer mode == 0 at serializer+8), resize the array
    if (*(int*)(serializer2 + 8) == 0) {
        if (s_fn_DynArray_Resize) {
            s_fn_DynArray_Resize((void*)&self->field58, arrayCount);
        }
    }

    // Serialize each InternalBasePtr element in the array
    // Array base at +0x58, count = arrayCount
    void* arrayBase = self->field58;
    int64_t remaining = (int64_t)arrayCount;
    if (remaining > 0) {
        uint8_t* elementPtr = (uint8_t*)arrayBase;
        do {
            // InternalBasePtr::Serialize(element, serializer)
            if (fn_InternalBasePtr_Serialize) {
                fn_InternalBasePtr_Serialize(elementPtr, serializer2);
            }

            // Validate: if ptr is non-null but not a ComponentEntity, clear it
            void* objPtr = *(void**)elementPtr;
            if (objPtr != nullptr) {
                if (s_fn_IsDerivedFrom_d && s_pTypeId_ComponentEntity) {
                    bool isEntity = s_fn_IsDerivedFrom_d(objPtr, *s_pTypeId_ComponentEntity);
                    if (!isEntity) {
                        *(uint64_t*)elementPtr = 0;
                    }
                }
            }

            elementPtr += 8;  // InternalBasePtr is 8 bytes
            remaining--;
        } while (remaining != 0);
    }

    // Serialize Time at +0x48
    if (s_fn_Time_Serialize) {
        s_fn_Time_Serialize((void*)&self->field48, serializer2);
    }

    // Destroy serializer
    if (s_fn_Serializer_Dtor) {
        s_fn_Serializer_Dtor(serializer2);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void ImmigrationComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[ImmigrationComponent] Funcs resolved: Serialize=%p DynResize=%p IBPSerialize=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_DynArray_Resize,
           (void*)fn_InternalBasePtr_Serialize);
#endif
}
