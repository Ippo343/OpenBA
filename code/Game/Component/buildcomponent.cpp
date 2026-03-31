// code/Game/Component/buildcomponent.cpp
// Clean reimplementation of BuildComponent vtable methods.
//
// BuildComponent (0x78 bytes) — construction management component.
// Vtable: 0x140162260
//
// Fields (from Ghidra decompilation):
//   +0x48: uint32 buildState (serialized)
//   +0x4C: uint32 buildProgress (init 1, serialized)
//   +0x50: byte   buildFlag (serialized)
//   +0x58: void*  entityArray1 (InternalBasePtr array ptr)
//   +0x60: int    entityArray1Count
//   +0x68: void*  entityArray2 (InternalBasePtr array ptr)
//   +0x70: int    entityArray2Count
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Component/buildcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Type ID globals needed by BuildComponent
// ============================================================

// DWORD_1401c7a50 — BuildComponent type ID (for IsDerivedFrom check on array1 elements)
static uint32_t* s_TypeId_BuildComp = nullptr;
// DAT_1401c7c10 — StorageComponent type ID (for IsDerivedFrom check on array2 elements)
static uint32_t* s_TypeId_StorageComp = nullptr;
// DWORD_1401c7ac0 — WorkplaceComponent type ID (for Draw function)
static uint32_t* s_TypeId_WorkplaceComp = nullptr;
// DWORD_1401c7a88 — MultiObjectComponent type ID
static uint32_t* s_TypeId_MultiObj = nullptr;

// ============================================================
// Helper function pointers (resolved once)
// ============================================================

// Object::InternalBasePtr::Serialize(this, Serializer*)
typedef void (*fn_IBP_Serialize_t)(void* self, void* serializer);
static fn_IBP_Serialize_t s_fn_IBP_Serialize = nullptr;

// Original Draw function — delegated because Draw is 300+ instructions with
// multi-object hierarchy iteration, worker assignment, and storage management
typedef void (*fn_BuildComponent_Draw_Original_t)(void*);
static fn_BuildComponent_Draw_Original_t s_fn_Draw_Original = nullptr;

static bool s_helpersResolved = false;

static void EnsureHelpers() {
    if (s_helpersResolved) return;

    // Resolve IBP::Serialize from Runtime DLL
    s_fn_IBP_Serialize = (fn_IBP_Serialize_t)ResolveExport(
        "?Serialize@InternalBasePtr@Object@@QEAAXAEAVSerializer@IO@@@Z");
    if (!s_fn_IBP_Serialize) {
        s_fn_IBP_Serialize = (fn_IBP_Serialize_t)ResolveOrdinal(2990);
    }

    // Type IDs from exe .rdata
    s_TypeId_BuildComp = (uint32_t*)ResolveAddr(Module::Exe, 0x1401c7a50);
    s_TypeId_StorageComp = (uint32_t*)ResolveAddr(Module::Exe, 0x1401c7c10);
    s_TypeId_WorkplaceComp = (uint32_t*)ResolveAddr(Module::Exe, 0x1401c7ac0);
    s_TypeId_MultiObj = (uint32_t*)ResolveAddr(Module::Exe, 0x1401c7a88);

    // Original Draw (0x140108D40) — delegated due to extreme complexity
    s_fn_Draw_Original = (fn_BuildComponent_Draw_Original_t)
        ResolveAddr(Module::Exe, 0x140108D40);

    s_helpersResolved = true;
}

// ============================================================
// BuildComponent::Shutdown (vtable slot [3], 0x140108D80)
// ============================================================
// Serializes BuildComponent fields via IO::Serializer.
// Calls Component_Serialize for base class, then two versioned blocks:
//   - Version 0: empty (just ctor/dtor)
//   - Version 1: buildState, buildProgress, buildFlag,
//     entityArray1 (IBP array with IsDerivedFrom check),
//     entityArray2 (IBP array with IsDerivedFrom check)

void BuildComponent_Shutdown(BuildComponent* self, void* stream) {
    EnsureHelpers();
    uintptr_t s = (uintptr_t)self;

    // Base class serialize
    fn_ComponentBase_Serialize(self, stream);

    // Version 0 block (empty — just opens and closes)
    uint8_t ser0[24];
    g_fn_Serializer_Ctor(ser0, stream, 0);
    g_fn_Serializer_Dtor(ser0);

    // Version 1 block
    uint8_t ser1[24];
    g_fn_Serializer_Ctor(ser1, stream, 1);

    // Serialize buildState (+0x48), buildProgress (+0x4C), buildFlag (+0x50)
    g_fn_Serialize_Int(ser1, (int*)(s + 0x48), nullptr);
    g_fn_Serialize_Int(ser1, (int*)(s + 0x4C), nullptr);
    g_fn_Serialize_Bool(ser1, (bool*)(s + 0x50), nullptr);

    // Serialize entityArray1 count (+0x60)
    int count1 = *(int*)(s + 0x60);
    g_fn_Serialize_Int(ser1, &count1, nullptr);

    // On load: resize array
    int64_t serFirst = *(int64_t*)ser1;
    if (*(int32_t*)(serFirst + 8) == 0) {
        fn_ExeArrayResize((void*)(s + 0x58), count1);
    }

    // Serialize each IBP in array1, validate with IsDerivedFrom
    void** arr1 = *(void***)(s + 0x58);
    if (s_fn_IBP_Serialize && count1 > 0) {
        for (int i = 0; i < count1; i++) {
            void* ibp = (void*)((uintptr_t)arr1 + i * 8);
            s_fn_IBP_Serialize(ibp, ser1);
            // Validate: if non-null but wrong type, clear
            void* val = *(void**)ibp;
            if (val != nullptr && s_TypeId_BuildComp) {
                bool valid = fn_IsDerivedFrom(val, *s_TypeId_BuildComp);
                if (!valid) {
                    *(uint64_t*)ibp = 0;
                }
            }
        }
    }

    // Serialize entityArray2 count (+0x70)
    int count2 = *(int*)(s + 0x70);
    g_fn_Serialize_Int(ser1, &count2, nullptr);

    // On load: resize array2
    serFirst = *(int64_t*)ser1;
    if (*(int32_t*)(serFirst + 8) == 0) {
        fn_ExeArrayResize((void*)(s + 0x68), count2);
    }

    // Serialize each IBP in array2, validate with IsDerivedFrom
    void** arr2 = *(void***)(s + 0x68);
    if (s_fn_IBP_Serialize && count2 > 0) {
        for (int i = 0; i < count2; i++) {
            void* ibp = (void*)((uintptr_t)arr2 + i * 8);
            s_fn_IBP_Serialize(ibp, ser1);
            // Validate with StorageComponent type
            void* val = *(void**)ibp;
            if (val != nullptr && s_TypeId_StorageComp) {
                bool valid = fn_IsDerivedFrom(val, *s_TypeId_StorageComp);
                if (!valid) {
                    *(uint64_t*)ibp = 0;
                }
            }
        }
    }

    g_fn_Serializer_Dtor(ser1);
}

// ============================================================
// BuildComponent::OnCreate (vtable slot [7], 0x140108CB0)
// ============================================================
// Removes component from the removal queue. Creates InternalBasePtr
// from self, searches the removal queue at GameState+0xF0+0xD0.

// Forward-declared: fn_FindInQueue at 0x140017550
typedef int (*fn_FindInQueue_t)(void* arrayBase, void* entityPtr);
static fn_FindInQueue_t s_fn_FindInQueue = nullptr;
static void** s_pGameState = nullptr;

void BuildComponent_OnCreate(BuildComponent* self) {
    if (!s_fn_FindInQueue) {
        s_fn_FindInQueue = (fn_FindInQueue_t)ResolveAddr(Module::Exe, 0x140017550);
        s_pGameState = (void**)ResolveAddr(Module::Exe, 0x1401c3dd8);
    }
    if (!s_fn_FindInQueue || !s_pGameState || !fn_InternalBasePtr_Ctor)
        return;

    // Create InternalBasePtr on stack — original passes self (component ptr)
    uint8_t ptrBuf[8];
    fn_InternalBasePtr_Ctor(ptrBuf, (void*)self);

    void* gameState = *s_pGameState;
    void* worldData = *(void**)((uintptr_t)gameState + 0xF0);

    // Find in removal queue
    int idx = s_fn_FindInQueue((void*)((uintptr_t)worldData + 0xD0), ptrBuf);

    if (idx >= 0) {
        int count = *(int*)((uintptr_t)worldData + 0xD8);

        // Shift remaining elements down (each is 8 bytes / InternalBasePtr)
        if (idx < count) {
            int64_t offset = (int64_t)idx * 8;
            do {
                int nextIdx = idx + 1;
                if (nextIdx < count) {
                    uint64_t* pSlot = (uint64_t*)(*(uintptr_t*)((uintptr_t)worldData + 0xD0) + offset);
                    if (pSlot != nullptr) {
                        *pSlot = 0;
                        *pSlot = pSlot[1];
                    }
                }
                idx = nextIdx;
                offset += 8;
            } while (idx < count);
        }

        // Decrement count
        *(int*)((uintptr_t)worldData + 0xD8) = count - 1;
    }
}

// ============================================================
// BuildComponent::Draw (vtable slot [6], 0x140108D40)
// ============================================================
// Extremely complex function (300+ Ghidra instructions) that:
// 1. Checks game debug flags (bit 0 at game+0xD0) — returns if set
// 2. Gets owner entity, finds MultiObjectComponent children
// 3. For each child entity, finds BuildComponent and StorageComponent
// 4. Appends to internal arrays (+0x58/+0x60 and +0x68/+0x70)
// 5. Iterates BuildComponent array: manages worker assignments via vtable calls
// 6. Iterates StorageComponent array: calls Entity_impl_8f50
// 7. Sets buildProgress (+0x4C) to 1, finds WorkplaceComponent, updates progress
//
// Delegated to original exe code — too many subsystem interactions and vtable
// dispatches to safely reimplement without full MultiObjectComponent/WorkplaceComponent
// type coverage. The hook uses InstallHookVTable so the original code is intact.

void BuildComponent_Draw(BuildComponent* self) {
    EnsureHelpers();
    if (s_fn_Draw_Original) {
        s_fn_Draw_Original(self);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void BuildComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[BuildComponent] Funcs resolved: IBP_Serialize=%p DrawOriginal=%p "
           "TypeId_Build=%p TypeId_Storage=%p\n",
           (void*)s_fn_IBP_Serialize, (void*)s_fn_Draw_Original,
           (void*)s_TypeId_BuildComp, (void*)s_TypeId_StorageComp);
#endif
}
