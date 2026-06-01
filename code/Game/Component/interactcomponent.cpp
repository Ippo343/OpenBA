// code/Game/Component/interactcomponent.cpp
// Clean reimplementation of InteractComponent lifecycle methods.
//
// InteractComponent (0x38 bytes) -- direct Component subclass.
// Manages interactive object selection (clickable entities).
// Holds a dynamic array of entity references at +0x28 (ptr) / +0x30 (count).
//
// Vtable at 0x1401638b0:
//   [3] Shutdown = 0x140123340 -- serialization with entity array + IsDerivedFrom
//   [6] Draw     = 0x140123110 -- 140-instruction rebuild of interact list from scene
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- portable for standalone exe.

#include "Component/interactcomponent_funcs.h"
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

// IO::Serializer::Serializer(stream, version)
typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

// IO::Serializer::~Serializer()
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

// IO::Serializer::Serialize(int*)
typedef void (*fn_Serialize_Int_t)(void* serializer, int32_t* field, const char* name);
static fn_Serialize_Int_t s_fn_Serialize_Int = nullptr;

// Object::InternalBasePtr::Serialize(this, Serializer*)
typedef void (*fn_IBP_Serialize_t)(void* self, void* serializer);
static fn_IBP_Serialize_t s_fn_IBP_Serialize = nullptr;

// IsDerivedFrom -- dispatch slot
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* obj, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_dispatch = nullptr;

// CheckPtr
typedef void (*fn_CheckPtr_t)(void* ptrField);
static fn_CheckPtr_t s_fn_CheckPtr = nullptr;

// InternalBasePtr::Ctor(retbuf, rawPtr)
typedef void (*fn_IBP_Ctor_t)(void* retbuf, void* rawPtr);
static fn_IBP_Ctor_t s_fn_IBP_Ctor = nullptr;

// FUN_1400032e0 -- dynamic array resize for IBP arrays
typedef void (*fn_IBPArray_Resize_t)(void* arrayPtr, int newCount);
static fn_IBPArray_Resize_t s_fn_IBPArray_Resize = nullptr;

// FUN_140004740 -- FindComponent (MultiObject)
typedef void (*fn_FindMultiObjComp_t)(uintptr_t entity, void* retBuf);
static fn_FindMultiObjComp_t s_fn_FindMultiObjComp = nullptr;

// Type ID: DAT_1401c7900 -- InteractComponent type for IsDerivedFrom
static uint32_t* s_TypeId_Interact = nullptr;

static bool s_helpersResolved = false;

static void EnsureHelpers() {
    if (s_helpersResolved) return;

    uintptr_t slot;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ac8);
    if (slot) s_fn_Serialize_Int = *reinterpret_cast<fn_Serialize_Int_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_dispatch = *reinterpret_cast<fn_IsDerivedFrom_dispatch_t*>(slot);

    s_fn_IBP_Serialize = reinterpret_cast<fn_IBP_Serialize_t>(
        ResolveAddr(Module::Runtime, 0x1800c0210));

    s_fn_CheckPtr = reinterpret_cast<fn_CheckPtr_t>(
        ResolveAddr(Module::Runtime, 0x1800c0290));

    s_fn_IBP_Ctor = reinterpret_cast<fn_IBP_Ctor_t>(
        reinterpret_cast<uintptr_t>(fn_InternalBasePtr_Ctor));

    s_fn_IBPArray_Resize = reinterpret_cast<fn_IBPArray_Resize_t>(
        ResolveAddr(Module::Exe, 0x1400032e0));

    s_fn_FindMultiObjComp = reinterpret_cast<fn_FindMultiObjComp_t>(
        ResolveAddr(Module::Exe, 0x140004740));

    s_TypeId_Interact = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7900));

    s_helpersResolved = true;
}

// ============================================================
// InteractComponent::Shutdown (vtable slot [3], 0x140123340)
// ============================================================
// Serializes Component base + dynamic array of InternalBasePtr elements.
//
// Original pseudocode:
//   Component_Serialize(self, stream)
//   Serializer local(stream, 0)
//   int count = self->field30
//   Serialize(&count, nullptr)
//   if (serializer loading): IBPArray_Resize(&self->field28, count)
//   for each element in array:
//     IBP::Serialize(element, serializer)
//     if element != null && !IsDerivedFrom(element, InteractTypeId):
//       element = 0
//   ~Serializer()

void InteractComponent_Shutdown(InteractComponent* self, void* stream) {
    EnsureHelpers();

    // Call base Component_Serialize
    if (s_fn_Component_Serialize) {
        s_fn_Component_Serialize(self, stream);
    }

    // Create local IO::Serializer (24 bytes on stack)
    uint8_t serializer[24];
    if (s_fn_Serializer_Ctor) {
        s_fn_Serializer_Ctor(serializer, stream, 0);
    }

    // Serialize count
    int32_t count = *reinterpret_cast<int32_t*>(
        reinterpret_cast<uint8_t*>(self) + 0x30);
    if (s_fn_Serialize_Int) {
        s_fn_Serialize_Int(serializer, &count, nullptr);
    }

    // On load (serializer mode check: serializer+0x8 == 0 means loading)
    int serMode = *reinterpret_cast<int*>(
        reinterpret_cast<uint8_t*>(serializer) + 8);
    if (serMode == 0) {
        // Resize array for loading
        if (s_fn_IBPArray_Resize) {
            s_fn_IBPArray_Resize(
                reinterpret_cast<uint8_t*>(self) + 0x28, count);
        }
    }

    // Serialize each InternalBasePtr element and validate
    uint8_t* arrayBase = *reinterpret_cast<uint8_t**>(
        reinterpret_cast<uint8_t*>(self) + 0x28);
    for (int i = 0; i < count; i++) {
        uint8_t* element = arrayBase + i * 8;

        // Serialize InternalBasePtr
        if (s_fn_IBP_Serialize) {
            s_fn_IBP_Serialize(element, serializer);
        }

        // Validate with IsDerivedFrom
        void* ptr = *reinterpret_cast<void**>(element);
        if (ptr != nullptr) {
            if (s_fn_IsDerivedFrom_dispatch && s_TypeId_Interact) {
                bool valid = s_fn_IsDerivedFrom_dispatch(ptr, *s_TypeId_Interact);
                if (!valid) {
                    *reinterpret_cast<uint64_t*>(element) = 0;
                }
            }
        }
    }

    // Destroy serializer
    if (s_fn_Serializer_Dtor) {
        s_fn_Serializer_Dtor(serializer);
    }
}

// ============================================================
// InteractComponent::Draw (vtable slot [6], 0x140123110)
// ============================================================
// Rebuilds the interact entity list from the scene.
// Complex scene traversal with dynamic array management.
//
// Due to extreme complexity (140 instructions, nested iteration with
// vtable dispatch, dynamic array growth with MemAlloc/MemFree),
// this delegates to the original function via ResolveAddr.

// Original function pointer
typedef void (*fn_InteractDraw_Original_t)(void* self);
static fn_InteractDraw_Original_t s_fn_InteractDraw_Original = nullptr;

void InteractComponent_Draw(InteractComponent* self) {
    EnsureHelpers();

    if (!s_fn_InteractDraw_Original) {
        s_fn_InteractDraw_Original = reinterpret_cast<fn_InteractDraw_Original_t>(
            ResolveAddr(Module::Exe, 0x140123110));
    }

    if (s_fn_InteractDraw_Original) {
        s_fn_InteractDraw_Original(self);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void InteractComponent_InitFuncs() {
    EnsureHelpers();

    // Pre-resolve the Draw original
    if (!s_fn_InteractDraw_Original) {
        s_fn_InteractDraw_Original = reinterpret_cast<fn_InteractDraw_Original_t>(
            ResolveAddr(Module::Exe, 0x140123110));
    }

#ifdef _DEBUG
    printf("[InteractComponent] Funcs resolved: Serialize=%p IBP_Serialize=%p "
           "DrawOrig=%p TypeId=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_IBP_Serialize,
           (void*)s_fn_InteractDraw_Original,
           (void*)s_TypeId_Interact);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// In standalone the original exe is not mapped, so Object::Factory::CreateObject
// must resolve to a C++ factory here. Produces an inert-but-valid InteractComponent
// (size 0x38, +0x20 capability flags 0x81, Object::Internal base + shared stub
// vtable, zeroed body). Size from InteractComponent_Factory MemAlloc; flags from the
// Component_Ctor (+0x20=0x80) chain OR'd with the leaf ctor bits.
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

COMPONENT_INERT_FACTORY(InteractComponent, 0x38, 0x81)
REGISTER_TYPE(InteractComponent, "Component",
              &InteractComponent_Factory, &InteractComponent_CreateInPlace);
#endif // STANDALONE_EXE
