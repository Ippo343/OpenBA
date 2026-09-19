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

#include "Component/immigrationcomponent_funcs.h"
#include "Component/coll_bridge.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.4 caller batch G

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

// FUN_1400032e0 -- dynamic array resize; this file DEFINES its body as
// ImmigrationComp_DynArrayResize below (kept, still registered for other
// callers) — this file's own call site goes straight through the bridge.

// Type ID for ComponentEntity (DAT_1401c5d70)
static uint32_t* s_pTypeId_ComponentEntity = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    uintptr_t slot;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));
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
    alignas(8) uint8_t serializer1[24];
    if (s_fn_Serializer_Ctor && s_fn_Serializer_Dtor) {
        ::new (static_cast<void*>(serializer1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(serializer1)->~Serializer();                                       // B.3.4 bridge
    }

    // Main serializer block
    alignas(8) uint8_t serializer2[24];
    if (s_fn_Serializer_Ctor) {
        ::new (static_cast<void*>(serializer2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    }

    // Serialize field50 (int)
    if (s_fn_Serializer_SerializeInt) {
        reinterpret_cast<IO::Serializer*>(serializer2)->Serialize(*reinterpret_cast<int*>(&self->field50), nullptr);   // B.3.4 bridge
    }

    // Serialize array count from field60
    int arrayCount = *(int*)&self->field60;
    if (s_fn_Serializer_SerializeInt) {
        reinterpret_cast<IO::Serializer*>(serializer2)->Serialize(arrayCount, nullptr);   // B.3.4 bridge
    }

    // "Am I loading?" is `*(int32*)(*(void**)serializer + 8) == 0` -- deref the
    // Serializer to reach its STREAM first, then read the stream's +8. This
    // tested the Serializer's OWN +8 (m_encoding), so on a real load the resize
    // below never ran and the element loop walked a NULL array with a non-zero
    // count straight off the stream. Same trap as interactcomponent.cpp and
    // reference_serializer_plus8_is_encoding_not_loading_flag.
    if (*(int32_t*)(*(uintptr_t*)serializer2 + 8) == 0) {
        // B.3.2 bridge: exe 0x1400032e0 IS Collection::Array<void*>::SetSize(n).
        GameColl::AsArray<GameColl::Ptr8>((void*)&self->field58).SetSize(arrayCount);
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
                reinterpret_cast<Object::InternalBasePtr*>(elementPtr)->Serialize(*reinterpret_cast<IO::Serializer*>(serializer2));   // B.3.4 bridge
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
        reinterpret_cast<IO::Serializer*>(serializer2)->~Serializer();   // B.3.4 bridge
    }
}

// ============================================================
// Reimpl of FUN_1400032e0 (Component reg wave "people") — shared DynArray<ptr> resize.
// ENDGAME B.3.2 step 2: this IS Collection::Array<void*>::SetSize(n) — the old
// hand-written body's copy path did `*slot = 0;` immediately before assigning
// the copied 8-byte value into the same slot; that default-construct-then-
// assign is exactly what SetSizeCore's byte-copy `assign` does for a
// zero-default element and is unobservable (the write is overwritten before
// anything reads it), so the bridge call below is byte-for-byte identical.
// Non-static: still registered by reg_component_people.cpp for other callers,
// so this function is KEPT (not deleted/unregistered) — only its body changed.
// ============================================================
void ImmigrationComp_DynArrayResize(int64_t* param_1, int newCount) {
    GameColl::AsArray<GameColl::Ptr8>(param_1).SetSize(newCount);
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void ImmigrationComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[ImmigrationComponent] Funcs resolved: Serialize=%p IBPSerialize=%p\n",
           (void*)s_fn_Component_Serialize,
           (void*)fn_InternalBasePtr_Serialize);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
#include "Component/controller_live_base.h"
#include "Component/controller_state_table.h"
// Immigrants wave 2026-09-05: the nomad-arrival state machine (states 2/0xf/0x10/0x11,
// immigrationcomponent_states.cpp) replaces the inert factory. Node at +0x28
// (CreateState 0x140004080 = LEA RAX,[RCX+0x28]), default state 2 (0x1400026c0 =
// MOV EAX,2). Opt-in BANISHED_LIVE_IMMIGRATION; off => inert, byte-unchanged.
// Slot 3 (stream Serialize) stays wired unconditionally, save/load wave 2026-08-28.
extern "C" void** ImmigrationCtrl_NodeVtable();
extern "C" void   ImmigrationCtrl_PopulateTable();
CONTROLLER_LIVE_FACTORY_STATES(ImmigrationComponent, 0x68, 0x80, 0x28, 2,
                               ImmigrationCtrl_NodeVtable, ImmigrationCtrl_PopulateTable)
CONTROLLER_INSTALL_SERIALIZE(ImmigrationComponent, ImmigrationComponent_Shutdown)
REGISTER_TYPE(ImmigrationComponent, "ControllerComponent",
              &ImmigrationComponent_Factory_Ser, &ImmigrationComponent_CreateInPlace_Ser);
