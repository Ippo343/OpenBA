// code/Game/Component/cemeterycomponent.cpp
// Clean reimplementation of CemeteryComponent lifecycle methods.
//
// CemeteryComponent (0x58 bytes) -- ControllerComponent subclass.
// Fields: +0x48=array ptr, +0x50=count. Manages cemetery plot grid for tombstone placement.
//
// Vtable at 0x1401590a0. Non-trivial overrides:
//   Shutdown  (0x1400475C0) -- serialize array of tile indices
//   Draw      (0x140047200) -- complex grid rendering (233 instrs), delegates to original
//   OnCreate  (0x1400471C0) -- register entity in cemetery list
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- portable for standalone exe.

#include "Component/cemeterycomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

typedef void (*fn_Serialize_Int_t)(void* serializer, int* value, const char* name);
static fn_Serialize_Int_t s_fn_Serialize_Int = nullptr;

// Bulk int array serialization -- dispatch slot at 0x140151278
typedef void (*fn_Serialize_IntArray_t)(void* serializer, int count, int* array, const char* name);
static fn_Serialize_IntArray_t s_fn_Serialize_IntArray = nullptr;

// FUN_1400021d0 -- resize int array
typedef void (*fn_ResizeIntArray_t)(void* arrayField, int newCount);
static fn_ResizeIntArray_t s_fn_ResizeIntArray = nullptr;

// FUN_140002cc0 -- add InternalBasePtr to linked list
typedef void (*fn_ListAdd_t)(void* listBase, void* ptrValue);
static fn_ListAdd_t s_fn_ListAdd = nullptr;

// Original Draw function (too complex to reimplement: 233 instrs, grid rendering)
typedef void (*fn_CemeteryDraw_t)(void* self);
static fn_CemeteryDraw_t s_fn_OriginalDraw = nullptr;

static void** s_g_pGameState = nullptr;

static bool s_initialized = false;

// ============================================================
// CemeteryComponent::Shutdown (vtable slot [3], 0x1400475C0)
// ============================================================
// Serializes:
//   1. Component_Serialize base
//   2. Empty serializer block (controller compat stub)
//   3. Serializer(version=0): count from +0x50, resize on load, bulk serialize int array
void CemeteryComponent_Shutdown(CemeteryComponent* self, void* stream) {
    uintptr_t selfAddr = (uintptr_t)self;

    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    // Empty serializer block (controller compat stub)
    uint8_t serializer1[24];
    if (s_fn_Serializer_Ctor && s_fn_Serializer_Dtor) {
        s_fn_Serializer_Ctor(serializer1, stream, 0);
        s_fn_Serializer_Dtor(serializer1);
    }

    // Serialize tile index array
    uint8_t serializer2[24];
    if (!s_fn_Serializer_Ctor) return;
    s_fn_Serializer_Ctor(serializer2, stream, 0);

    int localCount = *(int*)(selfAddr + 0x50);
    if (s_fn_Serialize_Int)
        s_fn_Serialize_Int(serializer2, &localCount, nullptr);

    // If reading, resize array
    uintptr_t serializerObj = *(uintptr_t*)serializer2;
    if (*(int*)(serializerObj + 8) == 0) {
        if (s_fn_ResizeIntArray)
            s_fn_ResizeIntArray((void*)(selfAddr + 0x48), localCount);
    }

    // Bulk-serialize all int elements
    int* array = *(int**)(selfAddr + 0x48);
    if (s_fn_Serialize_IntArray)
        s_fn_Serialize_IntArray(serializer2, localCount, array, nullptr);

    if (s_fn_Serializer_Dtor)
        s_fn_Serializer_Dtor(serializer2);
}

// ============================================================
// CemeteryComponent::Draw (vtable slot [6], 0x140047200)
// ============================================================
// Complex cemetery grid rendering: 233 instructions. Iterates grid cells,
// checks orientation/rotation, allocates memory for plot indices, builds
// plot grid based on entity rotation. Delegates to original function.
void CemeteryComponent_Draw(CemeteryComponent* self) {
    if (s_fn_OriginalDraw)
        s_fn_OriginalDraw(self);
}

// ============================================================
// CemeteryComponent::OnCreate (vtable slot [7], 0x1400471C0)
// ============================================================
// Creates InternalBasePtr from owner entity, adds it to the cemetery list
// at GameState+0xF0+0x40.
void CemeteryComponent_OnCreate(CemeteryComponent* self) {
    uint8_t ptrBuf[8];
    if (fn_InternalBasePtr_Ctor)
        fn_InternalBasePtr_Ctor(ptrBuf, self->owner);

    if (s_g_pGameState && *s_g_pGameState && s_fn_ListAdd) {
        void* gameState = *s_g_pGameState;
        void* subObj = *(void**)((uintptr_t)gameState + 0xF0);
        void* cemeteryListBase = (void*)((uintptr_t)subObj + 0x40);
        s_fn_ListAdd(cemeteryListBase, ptrBuf);
    }
}

// ============================================================
// Reimpl of FUN_140002cc0 (CemeteryComponent list erase-by-pointer)
// ============================================================
// Scans the InternalBasePtr array {ptr@list+0, count@list+8} for an entry equal to
// the resolved value of `ptrValue` and removes it, shifting the trailing entries
// down by one slot and decrementing the count. Calls CheckPtr on the search key
// before comparing (resolves lazy/weak pointers, mirroring the original). The exe
// shifts via a quirky `*p=0; *p=p[1]` (effectively `p[0]=p[1]`). 1:1 port.
extern "C" void CemeteryListErase_Reimpl(void* listBase, void* ptrValue) {
    int64_t* list = reinterpret_cast<int64_t*>(listBase);
    int i = 0;
    while (i < (int)list[1]) {
        int64_t arr = list[0];
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(ptrValue);
        if (*(int64_t*)(arr + (int64_t)i * 8) == *(int64_t*)ptrValue) {
            for (int j = i + 1; j < (int)list[1]; ++j) {
                int64_t* slot = (int64_t*)(list[0] + (int64_t)(j - 1) * 8);
                if (slot) slot[0] = slot[1];
            }
            *(int*)(list + 1) = (int)list[1] - 1;
            --i;
        }
        ++i;
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void CemeteryComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    uintptr_t slot;

    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ac8);
    if (slot) s_fn_Serialize_Int = *reinterpret_cast<fn_Serialize_Int_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140151278);
    if (slot) s_fn_Serialize_IntArray = *reinterpret_cast<fn_Serialize_IntArray_t*>(slot);

    s_fn_ResizeIntArray = reinterpret_cast<fn_ResizeIntArray_t>(
        ResolveAddr(Module::Exe, 0x1400021d0));

    s_fn_ListAdd = reinterpret_cast<fn_ListAdd_t>(
        ResolveAddr(Module::Exe, 0x140002cc0));

    s_fn_OriginalDraw = reinterpret_cast<fn_CemeteryDraw_t>(
        ResolveAddr(Module::Exe, 0x140047200));

    s_g_pGameState = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));

    s_initialized = true;

#ifdef _DEBUG
    printf("[CemeteryComponent] Funcs resolved: CompSerialize=%p ListAdd=%p Draw=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_ListAdd,
           (void*)s_fn_OriginalDraw);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// In standalone the original exe is not mapped, so Object::Factory::CreateObject
// must resolve to a C++ factory here. Produces an inert-but-valid CemeteryComponent
// (size 0x58, +0x20 capability flags 0x83, Object::Internal base + shared stub
// vtable, zeroed body). Size from CemeteryComponent_Factory MemAlloc; flags from the
// Component_Ctor (+0x20=0x80) chain OR'd with the leaf ctor bits.
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

COMPONENT_INERT_FACTORY(CemeteryComponent, 0x58, 0x83)
REGISTER_TYPE(CemeteryComponent, "ControllerComponent",
              &CemeteryComponent_Factory, &CemeteryComponent_CreateInPlace);
