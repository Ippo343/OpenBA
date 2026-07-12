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
#include <cstdlib>
#include <cstring>

#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"   // Object::GetTypeIdByName
#endif

// Draw trace (BANISHED_BUILD_TRACE): confirms the per-frame Draw runs for build sites
// and reports the mesh-list (+0x58/+0x60) fill. Shares the construction trace env var.
static int s_bcDrawTrace = -1;
static bool BCDraw_Trace() {
    if (s_bcDrawTrace < 0) {
        const char* e = std::getenv("BANISHED_BUILD_TRACE");
        s_bcDrawTrace = (e && e[0] && std::strcmp(e, "0") != 0) ? 1 : 0;
    }
    return s_bcDrawTrace == 1;
}
extern "C" void* g_pConstructionDemoSite;

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
// Reimpl of FUN_140017550 (BuildComponent_helper_7550:
//   linear search of an InternalBasePtr array for a matching entity)
// ============================================================
// param_1 = {void* ptr (+0), int count (+8)}. param_2 = InternalBasePtr* to match.
// Returns the index of the slot whose pointer equals *param_2, or -1.
int BuildComponent_FindInQueue(void* arrayBase, void* entityPtr) {
    int64_t* arr = (int64_t*)arrayBase;
    int count = (int)arr[1];
    for (int i = 0; i < count; i++) {
        int64_t base = arr[0];
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(entityPtr);
        if (*(int64_t*)(base + (int64_t)i * 8) == *(int64_t*)entityPtr) {
            return i;
        }
    }
    return -1;
}

// ============================================================
// Reimpl of FUN_140108d40 / body 0x140108720 (BuildComponent::Draw:
//   rebuild the worker/storage child lists and refresh build progress)
// ============================================================
// Faithful port of the Ghidra decompilation. All exe-internal helpers are resolved
// guarded; if any is unavailable the corresponding step is skipped.
typedef void* (*fn_BC_FindComp_t)(int64_t entity, void* retbuf);   // returns retbuf
typedef void (*fn_BC_AddToList_t)(void* arrayField, int newCount, void* ibp);
typedef void (*fn_BC_Entity8f50_t)(int64_t* ptr);
static fn_BC_FindComp_t  s_fn_BC_FindMulti = nullptr;   // 0x140003f60 (FindComponent ModelComponent)
static fn_BC_FindComp_t  s_fn_BC_Find26a30 = nullptr;   // 0x140026a30 (FindComponent StorageComponent)
static fn_BC_FindComp_t  s_fn_BC_FindWork  = nullptr;   // 0x140004740 (Workplace)
static fn_BC_AddToList_t s_fn_BC_AddToList = nullptr;   // 0x140003110
static fn_BC_Entity8f50_t s_fn_BC_Entity8f50 = nullptr; // 0x140128f50
// Type-id VALUES. In the DLL build these come from raw .rdata; in standalone the
// .rdata globals are never seeded (ResolveAddr(Exe,.rdata)==NULL) so we fall back to
// the runtime type registry by name — otherwise block 1's IsDerivedFrom is guarded out
// and the site's ModelComponent never gets appended to +0x58 (empty mesh list).
//   0x1401c7a88 = ModelDescription (checked on the found ModelComponent's +0x18 desc)
//   0x1401c7a50 = ModelComponent   (block 2 collects these into +0x58)
//   0x1401c7c10 = StorageComponent (block 2 collects these into +0x68)
//   0x1401c7ac0 = WorkPlaceComponent (block 4 reads +0x58 build multiplier)
static uint32_t s_tid_ModelDesc  = 0;   // DAT_1401c7a88
static uint32_t s_tid_ModelComp  = 0;   // DWORD_1401c7a50
static uint32_t s_tid_Storage    = 0;   // DAT_1401c7c10
static uint32_t s_tid_Workplace  = 0;   // DWORD_1401c7ac0
static bool s_bcDrawResolved = false;
static uint32_t BC_ResolveTid(uintptr_t rdataGlobal, const char* name) {
#ifdef STANDALONE_EXE
    (void)rdataGlobal;
    return ::Object::GetTypeIdByName(name);
#else
    (void)name;
    uintptr_t g = ResolveAddr(Module::Exe, rdataGlobal);
    return g ? *reinterpret_cast<uint32_t*>(g) : 0;
#endif
}
static void EnsureDrawHelpers() {
    if (s_bcDrawResolved) return;
    s_fn_BC_FindMulti = reinterpret_cast<fn_BC_FindComp_t>(ResolveAddr(Module::Exe, 0x140003f60));
    s_fn_BC_Find26a30 = reinterpret_cast<fn_BC_FindComp_t>(ResolveAddr(Module::Exe, 0x140026a30));
    s_fn_BC_FindWork  = reinterpret_cast<fn_BC_FindComp_t>(ResolveAddr(Module::Exe, 0x140004740));
    s_fn_BC_AddToList = reinterpret_cast<fn_BC_AddToList_t>(ResolveAddr(Module::Exe, 0x140003110));
    s_fn_BC_Entity8f50 = reinterpret_cast<fn_BC_Entity8f50_t>(ResolveAddr(Module::Exe, 0x140128f50));
    s_tid_ModelDesc = BC_ResolveTid(0x1401c7a88, "ModelDescription");
    s_tid_ModelComp = BC_ResolveTid(0x1401c7a50, "ModelComponent");
    s_tid_Storage   = BC_ResolveTid(0x1401c7c10, "StorageComponent");
    s_tid_Workplace = BC_ResolveTid(0x1401c7ac0, "WorkPlaceComponent");
    s_bcDrawResolved = true;
}

// Scan an entity's component list (compHost+0x30 array, +0x38 count) for the
// first component whose GetTypeId (vtable+0x10) == typeId; verify with IsDerivedFrom.
static void* BC_FindComponentByType(int64_t compHost, uint32_t typeId) {
    if (!typeId) return nullptr;
    int count = *(int*)(compHost + 0x38);
    int64_t arr = *(int64_t*)(compHost + 0x30);
    for (int i = 0; i < count; i++) {
        int64_t* slot = (int64_t*)(arr + (int64_t)i * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
        void* comp = (void*)*slot;
        if (!comp) continue;
        typedef uint32_t (*GetTypeId_t)(void*);
        uint32_t tid = ((GetTypeId_t)(*(uintptr_t*)(*(uintptr_t*)comp + 0x10)))(comp);
        if (tid == typeId) {
            if (fn_IsDerivedFrom && fn_IsDerivedFrom(comp, typeId)) return comp;
            return nullptr;
        }
    }
    return nullptr;
}

void BuildComponent_DrawReimpl(void* selfP) {
    EnsureHelpers();
    EnsureDrawHelpers();
    int64_t* self = (int64_t*)selfP;

    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    uintptr_t gw = (uintptr_t)*g_pGameWorldSingleton;
    if (*(uint8_t*)(gw + 0xd0) & 1) return;   // debug-disabled

    bool demoTrace = BCDraw_Trace() && g_pConstructionDemoSite && selfP == g_pConstructionDemoSite;
    if (demoTrace)
        std::fprintf(stderr, "[BuildTrace] Draw ENTER demo site=%p meshCount(+0x60)=%d\n",
                     selfP, (int)self[0xc]);

    // --- Stage 1: append the site's own ModelComponent (whose +0x18 ModelDescription
    //     has a non-empty frame group) to the +0x58 mesh list / +0x68 storage list.
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self + 2);
    int64_t multiBuf = 0;
    if (s_fn_BC_FindMulti) s_fn_BC_FindMulti(self[2], &multiBuf);
    if (multiBuf != 0) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self + 2);
        if (s_fn_BC_FindMulti) s_fn_BC_FindMulti(self[2], &multiBuf);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&multiBuf);
        void* objBase = *(void**)(multiBuf + 0x18);
        void* multi = (fn_IsDerivedFrom && s_tid_ModelDesc &&
                       fn_IsDerivedFrom(objBase, s_tid_ModelDesc)) ? objBase : nullptr;
        if (multi && *(int*)((uintptr_t)multi + 0x28) > 0) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self + 2);
            int64_t b1 = 0;
            if (s_fn_BC_FindMulti) s_fn_BC_FindMulti(self[2], &b1);
            if (s_fn_BC_AddToList) s_fn_BC_AddToList(self + 0xb, (int)self[0xc] + 1, &b1);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self + 2);
            int64_t b2 = 0;
            if (s_fn_BC_Find26a30) s_fn_BC_Find26a30(self[2], &b2);
            if (s_fn_BC_AddToList) s_fn_BC_AddToList(self + 0xd, (int)self[0xe] + 1, &b2);
        }
    }

    // --- Stage 2: iterate the owner workplace's job hosts, collecting the
    //     Build component and Storage component of each into +0xb / +0xd lists.
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self + 2);
    int64_t workBuf = 0;
    if (s_fn_BC_FindWork) s_fn_BC_FindWork(self[2], &workBuf);
    if (workBuf != 0) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self + 2);
        if (s_fn_BC_FindWork) s_fn_BC_FindWork(self[2], &workBuf);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&workBuf);
        int jobCount = *(int*)(workBuf + 0x50);
        for (int j = 0; j < jobCount; j++) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&workBuf);
            int64_t host = *(int64_t*)((int64_t)j * 8 + *(int64_t*)(workBuf + 0x48));
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&host);
            void* buildComp = BC_FindComponentByType(host, s_tid_ModelComp);
            int64_t ib1 = 0;
            if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&ib1, buildComp);
            if (s_fn_BC_AddToList) s_fn_BC_AddToList(self + 0xb, (int)self[0xc] + 1, &ib1);

            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&workBuf);
            int64_t host2 = *(int64_t*)((int64_t)j * 8 + *(int64_t*)(workBuf + 0x48));
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&host2);
            void* storeComp = BC_FindComponentByType(host2, s_tid_Storage);
            int64_t ib2 = 0;
            if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&ib2, storeComp);
            if (s_fn_BC_AddToList) s_fn_BC_AddToList(self + 0xd, (int)self[0xe] + 1, &ib2);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&workBuf);
        }
    }

    if (demoTrace)
        std::fprintf(stderr, "[BuildTrace] Draw after fill: meshCount(+0x60)=%d (tid ModelDesc=%u ModelComp=%u)\n",
                     (int)self[0xc], s_tid_ModelDesc, s_tid_ModelComp);

    // --- Stage 3: walk our collected Build list (+0xb / count +0xc), driving each
    //     child's job hosts; finalise Storage list entries via Entity_impl_8f50.
    int buildCount = (int)self[0xc];
    for (int i = 0; i < buildCount; i++) {
        int64_t arrB = self[0xb];
        int64_t* slot = (int64_t*)(arrB + (int64_t)i * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
        int64_t* node = (int64_t*)*slot;
        int16_t mark = *(int16_t*)((uintptr_t)node + 0x166);
        if (mark == -1) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(node + 2);
            int64_t host = node[2];
            int hc = *(int*)(host + 0x38);
            for (int k = 0; k < hc; k++) {
                int64_t* h = *(int64_t**)(*(int64_t*)(host + 0x30) + (int64_t)k * 8);
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&h);
                typedef void (*VFn_t)(void*);
                ((VFn_t)(*(uintptr_t*)(*h + 0x60)))(h);
            }
        } else {
            if (mark >= 0) {
                typedef void (*VFn38_t)(int64_t*);
                ((VFn38_t)(*(uintptr_t*)(*node + 0x38)))(node);
            }
            *(int16_t*)((uintptr_t)node + 0x166) = (int16_t)0xffff;
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(node + 2);
            int64_t host = node[2];
            int hc = *(int*)(host + 0x38);
            for (int k = 0; k < hc; k++) {
                int64_t* h = *(int64_t**)(*(int64_t*)(host + 0x30) + (int64_t)k * 8);
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&h);
                typedef void (*VFn_t)(void*, int);
                ((VFn_t)(*(uintptr_t*)(*h + 0x60)))(h, 0);
            }
        }
        int64_t* sslot = (int64_t*)(self[0xd] + (int64_t)i * 8);
        if (*sslot != 0) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(sslot);
            if (s_fn_BC_Entity8f50) s_fn_BC_Entity8f50((int64_t*)*sslot);
        }
    }

    // --- Stage 4: progress = 1; then read the WorkplaceComponent's progress at +0xb*8.
    *(int*)((uintptr_t)self + 0x4c) = 1;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self + 2);
    void* wp = BC_FindComponentByType(self[2], s_tid_Workplace);
    int64_t wpIbp = 0;
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&wpIbp, wp);
    if (wpIbp != 0) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self + 2);
        void* wp2 = BC_FindComponentByType(self[2], s_tid_Workplace);
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&wpIbp, wp2);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&wpIbp);
        *(int*)((uintptr_t)self + 0x4c) = (int)((int64_t*)wpIbp)[0xb];
    }
}

// ============================================================
// Reimpl of FUN_140026a30 (BuildComponent find first StorageComponent child)
// ============================================================
// Scan the compHost's component list (+0x30 array of IBP, +0x38 count) for the
// first component whose GetTypeId (vtable+0x10) == StorageComponent type id
// (DAT_1401c7c10); verify with IsDerivedFrom; construct an IBP into retbuf.
// Closure: CheckPtr / IsDerivedFrom / InternalBasePtr_Ctor (all engine).
// Returns retbuf (caller passes &out).
void* BuildComponent_FindStorage26a30(int64_t compHost, void* retbuf) {
    void* found = nullptr;
    if (s_TypeId_StorageComp) {
        int count = *(int*)(compHost + 0x38);
        int64_t arr = *(int64_t*)(compHost + 0x30);
        for (int i = 0; i < count; i++) {
            int64_t* slot = (int64_t*)(arr + (int64_t)i * 8);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
            void* comp = (void*)*slot;
            typedef uint32_t (*GetTypeId_t)(void*);
            uint32_t tid = ((GetTypeId_t)(*(uintptr_t*)(*(uintptr_t*)comp + 0x10)))(comp);
            if (tid == *s_TypeId_StorageComp) {
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
                void* c = (void*)*slot;
                if (fn_IsDerivedFrom && fn_IsDerivedFrom(c, *s_TypeId_StorageComp))
                    found = c;
                break;
            }
        }
    }
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(retbuf, found);
    return retbuf;
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

// ===== Standalone factory (Foundation D — component instantiation) =====
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
#include "Component/controller_live_base.h"
#include "Component/controller_state_table.h"
// Phase 3 / Keystone D step 3 — live ControllerComponent with the REAL state-type
// table + dispatcher node virtuals. Embedded state node at +0x28 (CreateState
// 0x140004080 = LEA RAX,[RCX+0x28]); GetDefaultState (0x14002f2f0) returns 1. When
// BANISHED_LIVE_BUILDCTRL is set the entity's StateMachineComponent links this node
// and ticks it through the dispatcher vtable below.
//
// Build state graph (recovered from BuildDescription_Setup -> helper_9090 ->
// sub_f500/sub_f620, 0x140109090): three states 1 (default), 8, 2.
//   state 1: enter=helper_8290, setup=sub_81e0   ; transitions -> {8, 3}
//   state 8: enter=helper_7ae0, update=helper_7600, setup=sub_76e0
//                                                 ; transitions -> {2 (helper_75e0), 3}
//   state 2: (no handlers)                        ; transitions -> {3}
// (state-id 3 is a transition target only — not a registered state of this table;
//  it is resolved up the SM node chain.) Handler addresses are exe code unmapped in
// standalone, so they are populated as null (graceful no-op); the table structure
// (ids + transitions) is real, so Step returns real indices and the SM advances
// through the real state graph. Porting the real handlers is the next layer.
namespace { struct BuildCtrlTag {}; }
using BuildCtrlDispatch = ::ControllerLive::Dispatcher<BuildCtrlTag>;

// Ported state handlers (buildcomponent_state1.cpp / buildcomponent_state8.cpp).
extern "C" int  BuildComponent_State1Enter(void* buildComp, void* smNode);   // sub_8290 (slot +0x18)
extern "C" char BuildComponent_State1Update(void* buildComp, void* sm);      // sub_81e0 (slot +0x38, per-frame)
extern "C" char BuildComponent_State8Enter(void* buildComp, void* sm);       // sub_7ae0 (slot +0x18)
extern "C" char BuildComponent_State8Update(void* buildComp, void* sm);      // sub_76e0 (slot +0x38, per-frame)
extern "C" char BuildComponent_State8OneShot(void* buildComp, void* sm);     // sub_7600 (slot +0x20, exit-transition)
extern "C" char BuildComponent_State8To2Action(void* buildComp);             // sub_75e0 (slot +0x28, 8->2 action)

// Table field->node-slot mapping (see controller_state_table.h + buildcomponent_state8.cpp):
//   enterHandler (arg2) -> slot +0x18 Enter        (on state entry)
//   trans38Handler(arg4) -> slot +0x38 DrawAlt     (PER-FRAME; altDraw=1 every frame)
//   updateHandler(arg5) -> slot +0x20 Update       (only when a transition is pending)
//   transition handler   -> slot +0x28 OnState     (the 8->2 action)
// TableRegisterState(t, id, enterH, trans30H, trans38H, updateH):
static void BuildComponent_PopulateStateTable() {
    using namespace ::ControllerLive;
    StateTable& t = BuildCtrlDispatch::table;
    if (t.count != 0) return;  // idempotent
    // state 1: enter=State1Enter (spawner); per-frame=State1Update (materials -> req 8).
    TableRegisterState(t, 1, reinterpret_cast<void*>(&BuildComponent_State1Enter),
                       nullptr, reinterpret_cast<void*>(&BuildComponent_State1Update), nullptr);
    TableAddTransition(t, 8, nullptr);
    TableAddTransition(t, 3, nullptr);
    // state 8: enter=State8Enter (mesh reset); per-frame=State8Update (progress -> frames,
    //          complete -> req 2); exit-transition=State8OneShot; 8->2 action=Finalize.
    TableRegisterState(t, 8, reinterpret_cast<void*>(&BuildComponent_State8Enter),
                       nullptr, reinterpret_cast<void*>(&BuildComponent_State8Update),
                       reinterpret_cast<void*>(&BuildComponent_State8OneShot));
    TableAddTransition(t, 2, reinterpret_cast<void*>(&BuildComponent_State8To2Action));
    TableAddTransition(t, 3, nullptr);
    TableRegisterState(t, 2, nullptr, nullptr, nullptr, nullptr);
    TableAddTransition(t, 3, nullptr);
}
static void** BuildComponent_NodeVtable() { return BuildCtrlDispatch::Vtable(); }

CONTROLLER_LIVE_FACTORY_STATES(BuildComponent, 0x78, 0x83, 0x28, 1,
                               "BANISHED_LIVE_BUILDCTRL",
                               BuildComponent_NodeVtable,
                               BuildComponent_PopulateStateTable)
REGISTER_TYPE(BuildComponent, "ControllerComponent",
              &BuildComponent_Factory, &BuildComponent_CreateInPlace);
#endif // STANDALONE_EXE
