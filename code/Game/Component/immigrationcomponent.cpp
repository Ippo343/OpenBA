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
// Reimpl of FUN_1400032e0 (Component reg wave "people") — shared DynArray<ptr> resize.
// Grows a {ptr, count, cap} array (param[0]=buffer, param[1] low32=count,
// param[1] high32 / +0xc=capacity) to `newCount` 8-byte slots, copying existing
// elements and zeroing new ones (3x growth policy). Used by several controller
// components' Shutdown serialize paths. Non-static for registration.
// ============================================================
void ImmigrationComp_DynArrayResize(int64_t* param_1, int newCount) {
    int oldCount = static_cast<int>(param_1[1]);
    if (newCount == oldCount) return;

    void* oldBuf = reinterpret_cast<void*>(*param_1);
    *reinterpret_cast<int*>(&param_1[1]) = newCount;
    if (*reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(param_1) + 0xc) < newCount) {
        *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(param_1) + 0xc) = newCount;
        if (newCount == oldCount + 1 && oldCount != 0) {
            *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(param_1) + 0xc) = newCount * 3;
        }
        void* nb = fn_MemAlloc(g_pMemAllocSingleton,
                               (uint64_t)*reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(param_1) + 0xc) << 3, 8);
        *param_1 = reinterpret_cast<int64_t>(nb);
    }
    int start = (oldBuf == reinterpret_cast<void*>(*param_1)) ? oldCount : 0;
    for (int64_t i = start; i < newCount; i++) {
        int64_t* slot = reinterpret_cast<int64_t*>(*param_1 + i * 8);
        if (i < oldCount) {
            if (slot) {
                *slot = 0;
                *slot = *reinterpret_cast<int64_t*>(reinterpret_cast<uint8_t*>(oldBuf) + i * 8);
            }
        } else if (slot) {
            *slot = 0;
        }
    }
    if (oldBuf != reinterpret_cast<void*>(*param_1)) {
        if (oldBuf) fn_MemFreeStatic(oldBuf);
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

// ===== Standalone factory (Foundation D — component instantiation) =====
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
COMPONENT_INERT_FACTORY(ImmigrationComponent, 0x68, 0x80)
REGISTER_TYPE(ImmigrationComponent, "ControllerComponent",
              &ImmigrationComponent_Factory, &ImmigrationComponent_CreateInPlace);
#endif // STANDALONE_EXE
