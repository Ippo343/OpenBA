// code/Game/Component/buildcomponent.cpp
// Clean reimplementation of BuildComponent vtable methods.
//
// BuildComponent (0x78 bytes) — construction management component.
// Vtable: 0x140162260
//
// Fields (from Ghidra decompilation):
//   +0x48: int32  laborProgress accumulator (init 0) — raised +1 per delivered
//          build-labor SendComponentMessage (Slot15/+0x78); read by State8Update
//   +0x4C: int32  costMultiplier (init 1) — target = matComp[0x30] * this
//   +0x50: byte   buildFlag (serialized)
//   +0x58: void*  entityArray1 (InternalBasePtr array ptr)
//   +0x60: int    entityArray1Count
//   +0x68: void*  entityArray2 (InternalBasePtr array ptr)
//   +0x70: int    entityArray2Count
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/buildcomponent_funcs.h"
#include "Component/coll_bridge.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "Object/factory_registration.h"   // Object::GetTypeIdByName
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

extern "C" void* g_pConstructionDemoSite;


// ============================================================
// Type ID globals needed by BuildComponent
// ============================================================

// DWORD_1401c7a50 — ModelComponent type ID (for IsDerivedFrom check on array1 elements)
static uint32_t* s_TypeId_ModelComp2 = nullptr;
// DAT_1401c7c10 — PickingComponent type ID (for IsDerivedFrom check on array2 elements)
static uint32_t* s_TypeId_PickingComp = nullptr;
// DWORD_1401c7ac0 — MultiObjectComponent type ID (for Draw function)
static uint32_t* s_TypeId_MultiObjComp = nullptr;
// DWORD_1401c7a88 — ModelDescription type ID
static uint32_t* s_TypeId_ModelDesc2 = nullptr;

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

    s_fn_IBP_Serialize = (fn_IBP_Serialize_t)ResolveExport(
        "?Serialize@InternalBasePtr@Object@@QEAAXAEAVSerializer@IO@@@Z");

    // Type IDs from exe .rdata
    s_TypeId_ModelComp2 = (uint32_t*)ResolveAddr(Module::Exe, 0x1401c7a50);
    s_TypeId_PickingComp = (uint32_t*)ResolveAddr(Module::Exe, 0x1401c7c10);
    s_TypeId_MultiObjComp = (uint32_t*)ResolveAddr(Module::Exe, 0x1401c7ac0);
    s_TypeId_ModelDesc2 = (uint32_t*)ResolveAddr(Module::Exe, 0x1401c7a88);

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
    alignas(8) uint8_t ser0[24];
    ::new (static_cast<void*>(ser0)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser0)->~Serializer();                                       // B.3.4 bridge

    // Version 1 block
    alignas(8) uint8_t ser1[24];
    ::new (static_cast<void*>(ser1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 1);   // B.3.4 bridge

    // Serialize buildState (+0x48), buildProgress (+0x4C), buildFlag (+0x50)
    reinterpret_cast<IO::Serializer*>(ser1)->Serialize(*reinterpret_cast<int*>(s + 0x48), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser1)->Serialize(*reinterpret_cast<int*>(s + 0x4C), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser1)->Serialize(*reinterpret_cast<bool*>(s + 0x50), nullptr);   // B.3.4 bridge

    // Serialize entityArray1 count (+0x60)
    int count1 = *(int*)(s + 0x60);
    reinterpret_cast<IO::Serializer*>(ser1)->Serialize(count1, nullptr);   // B.3.4 bridge

    // On load: resize array
    int64_t serFirst = *(int64_t*)ser1;
    if (*(int32_t*)(serFirst + 8) == 0) {
        // B.3.2: Collection::Array<T>::SetSize — retail 0x1400032e0, stride 8 align 8, new tail zeroed.
        // Element is the 8-byte InternalBasePtr slot serialized below (byte-copy, zeroing).
        GameColl::ArrayAt<GameColl::Ptr8>(s, 0x58).SetSize(count1);
    }

    // Serialize each IBP in array1, validate with IsDerivedFrom
    void** arr1 = *(void***)(s + 0x58);
    if (s_fn_IBP_Serialize && count1 > 0) {
        for (int i = 0; i < count1; i++) {
            void* ibp = (void*)((uintptr_t)arr1 + i * 8);
            s_fn_IBP_Serialize(ibp, ser1);
            // Validate: if non-null but wrong type, clear
            void* val = *(void**)ibp;
            if (val != nullptr && s_TypeId_ModelComp2) {
                bool valid = fn_IsDerivedFrom(val, *s_TypeId_ModelComp2);
                if (!valid) {
                    *(uint64_t*)ibp = 0;
                }
            }
        }
    }

    // Serialize entityArray2 count (+0x70)
    int count2 = *(int*)(s + 0x70);
    reinterpret_cast<IO::Serializer*>(ser1)->Serialize(count2, nullptr);   // B.3.4 bridge

    // On load: resize array2
    serFirst = *(int64_t*)ser1;
    if (*(int32_t*)(serFirst + 8) == 0) {
        // B.3.2: Collection::Array<T>::SetSize — retail 0x1400032e0, stride 8 align 8, new tail zeroed.
        // Element is the 8-byte InternalBasePtr slot serialized below (byte-copy, zeroing).
        GameColl::ArrayAt<GameColl::Ptr8>(s, 0x68).SetSize(count2);
    }

    // Serialize each IBP in array2, validate with IsDerivedFrom
    void** arr2 = *(void***)(s + 0x68);
    if (s_fn_IBP_Serialize && count2 > 0) {
        for (int i = 0; i < count2; i++) {
            void* ibp = (void*)((uintptr_t)arr2 + i * 8);
            s_fn_IBP_Serialize(ibp, ser1);
            // Validate with PickingComponent type (DAT_1401c7c10)
            void* val = *(void**)ibp;
            if (val != nullptr && s_TypeId_PickingComp) {
                bool valid = fn_IsDerivedFrom(val, *s_TypeId_PickingComp);
                if (!valid) {
                    *(uint64_t*)ibp = 0;
                }
            }
        }
    }

    reinterpret_cast<IO::Serializer*>(ser1)->~Serializer();                                       // B.3.4 bridge
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
    new (ptrBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>((void*)self));   // B.3.3 bridge

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
// type coverage.

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
        if (*(int64_t*)(base + (int64_t)i * 8) == *(int64_t*)entityPtr) {
            return i;
        }
    }
    return -1;
}

// ============================================================
// Reimpl of FUN_140108d40 / body 0x140108720 — BuildComponent::OnAdded
//   (rebuild the worker/storage child lists and refresh build progress)
// ============================================================
// SLOT: vtable INDEX 6 (+0x30) — the one-shot OnAdded broadcast dispatched by
// ComponentEntity_OnAddedPass (componententity.cpp:701) at EntityList_RegisterEntity
// time, for components whose +0x20 bit 0 is set (BuildComponent's flags are 0x83, so
// it qualifies). It is NOT a per-frame draw-tick: verified 2026-07-27, the function's
// only xref is 0x140162290 == vtable_BuildComponent(0x140162260) + 0x30, and +0x40
// (index 8) is 0x140118a70, a shared empty `return;` stub. The name below is kept as
// *_DrawReimpl only because callers and the address registry already use it; Ghidra's
// "Draw" label is the familiar slot-6 mislabel.
//
// Wiring is DEFAULT-ON via BANISHED_LIVE_BUILD_ONADDED (kill-switch =0) — see
// BuildComponent_InstallMsgSlot.
// Faithful port of the Ghidra decompilation. All exe-internal helpers are resolved
// guarded; if any is unavailable the corresponding step is skipped.
typedef void* (*fn_BC_FindComp_t)(int64_t entity, void* retbuf);   // returns retbuf
typedef void (*fn_BC_Entity8f50_t)(int64_t* ptr);
static fn_BC_FindComp_t  s_fn_BC_FindMulti = nullptr;   // 0x140003f60 (FindComponent ModelComponent)
static fn_BC_FindComp_t  s_fn_BC_Find26a30 = nullptr;   // 0x140026a30 (FindComponent StorageComponent)
static fn_BC_FindComp_t  s_fn_BC_FindWork  = nullptr;   // 0x140004740 (Workplace)
// 0x140003110 was Collection::Array<void*>::SetSize(n, fill) monomorphised
// into the exe (ENDGAME B.3.2 step 2) — converted at its call sites below.
static fn_BC_Entity8f50_t s_fn_BC_Entity8f50 = nullptr; // 0x140128f50
// Type-id VALUES. The .rdata globals are never seeded (ResolveAddr(Exe,.rdata)==NULL)
// so we fall back to
// the runtime type registry by name — otherwise block 1's IsDerivedFrom is guarded out
// and the site's ModelComponent never gets appended to +0x58 (empty mesh list).
//   0x1401c7a88 = ModelDescription (checked on the found ModelComponent's +0x18 desc)
//   0x1401c7a50 = ModelComponent   (block 2 collects these into +0x58)
//   0x1401c7c10 = PickingComponent (block 2 collects these into +0x68) — CORRECTED
//                 2026-07-19: writer is PickingComponent_RegisterType 0x14014f6f5; was
//                 wrongly resolved by name as "StorageComponent" (wrong CRC → block-2
//                 scan matched the wrong component type)
//   0x1401c7ac0 = MultiObjectComponent (block 4 reads +0x58 build multiplier) —
//                 CORRECTED 2026-07-19: writer MultiObjectComponent_RegisterType
//                 0x14014f475; was wrongly resolved as "WorkPlaceComponent"
static uint32_t s_tid_ModelDesc  = 0;   // DAT_1401c7a88
static uint32_t s_tid_ModelComp  = 0;   // DWORD_1401c7a50
static uint32_t s_tid_Picking    = 0;   // DAT_1401c7c10
static uint32_t s_tid_MultiObj   = 0;   // DWORD_1401c7ac0
static bool s_bcDrawResolved = false;
static uint32_t BC_ResolveTid(uintptr_t rdataGlobal, const char* name) {
    (void)rdataGlobal;
    return ::Object::GetTypeIdByName(name);
}
static void EnsureDrawHelpers() {
    if (s_bcDrawResolved) return;
    s_fn_BC_FindMulti = reinterpret_cast<fn_BC_FindComp_t>(ResolveAddr(Module::Exe, 0x140003f60));
    s_fn_BC_Find26a30 = reinterpret_cast<fn_BC_FindComp_t>(ResolveAddr(Module::Exe, 0x140026a30));
    s_fn_BC_FindWork  = reinterpret_cast<fn_BC_FindComp_t>(ResolveAddr(Module::Exe, 0x140004740));
    s_fn_BC_Entity8f50 = reinterpret_cast<fn_BC_Entity8f50_t>(ResolveAddr(Module::Exe, 0x140128f50));
    s_tid_ModelDesc = BC_ResolveTid(0x1401c7a88, "ModelDescription");
    s_tid_ModelComp = BC_ResolveTid(0x1401c7a50, "ModelComponent");
    s_tid_Picking   = BC_ResolveTid(0x1401c7c10, "PickingComponent");
    s_tid_MultiObj  = BC_ResolveTid(0x1401c7ac0, "MultiObjectComponent");
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

void BuildComponent_DrawReimpl(void* selfP);   // body, 0x140108720

// The thunk at 0x140108d40 itself, instruction for instruction:
//     MOV RAX,[0x1401c3dd8]; TEST byte [RAX+0xd0],0x1; JZ 0x140108720; RET
// Registered at 0x140108d40; the ungated body is registered separately at 0x140108720.
// Keeping them distinct matters because State8Enter calls the BODY directly (0x140107bb0),
// precisely when the instant bit is set — routing it through the gate would no-op it.
extern "C" void BuildComponent_OnAddedThunk(void* selfP) {
    if (!selfP) return;
    if (g_pGameWorldSingleton && *g_pGameWorldSingleton) {
        uintptr_t gw = (uintptr_t)*g_pGameWorldSingleton;
        if (*(uint8_t*)(gw + 0xd0) & 1) return;   // instant build -> OnAdded does nothing
    }
    BuildComponent_DrawReimpl(selfP);
}

void BuildComponent_DrawReimpl(void* selfP) {
    EnsureHelpers();
    EnsureDrawHelpers();
    int64_t* self = (int64_t*)selfP;

    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    // NOTE: the instant-build gate that used to sit here has MOVED to
    // BuildComponent_OnAddedVSlot, because that is where retail keeps it. 0x140108d40 is
    // only a 4-instruction thunk:
    //     MOV RAX,[0x1401c3dd8]; TEST byte [RAX+0xd0],0x1; JZ 0x140108720; RET
    // The gate is in the THUNK; the body at 0x140108720 is ungated and has two callers —
    // the thunk (slot 6 / OnAdded) and 0x140107bb0 inside BuildDescription_helper_7ae0,
    // i.e. State8Enter's instant branch (ported 2026-07-27). Keeping the gate in the body
    // would wrongly suppress that second caller, since it runs precisely when the bit IS
    // set — and that caller is the whole insta-build completion path.



    // --- Stage 1: append the site's own ModelComponent (whose +0x18 ModelDescription
    //     has a non-empty frame group) to the +0x58 mesh list / +0x68 storage list.
    int64_t multiBuf = 0;
    if (s_fn_BC_FindMulti) s_fn_BC_FindMulti(self[2], &multiBuf);
    if (multiBuf != 0) {
        if (s_fn_BC_FindMulti) s_fn_BC_FindMulti(self[2], &multiBuf);
        void* objBase = *(void**)(multiBuf + 0x18);
        void* multi = (fn_IsDerivedFrom && s_tid_ModelDesc &&
                       fn_IsDerivedFrom(objBase, s_tid_ModelDesc)) ? objBase : nullptr;
        if (multi && *(int*)((uintptr_t)multi + 0x28) > 0) {
            int64_t b1 = 0;
            if (s_fn_BC_FindMulti) s_fn_BC_FindMulti(self[2], &b1);
            // B.3.2 bridge: exe 0x140003110 IS Collection::Array<void*>::SetSize(n, fill).
            GameColl::AsArray<GameColl::Ptr8>(self + 0xb).SetSizeFill((int)self[0xc] + 1, &b1);
            int64_t b2 = 0;
            if (s_fn_BC_Find26a30) s_fn_BC_Find26a30(self[2], &b2);
            GameColl::AsArray<GameColl::Ptr8>(self + 0xd).SetSizeFill((int)self[0xe] + 1, &b2);
        }
    }

    // --- Stage 2: iterate the owner workplace's job hosts, collecting the
    //     Build component and Storage component of each into +0xb / +0xd lists.
    int64_t workBuf = 0;
    if (s_fn_BC_FindWork) s_fn_BC_FindWork(self[2], &workBuf);
    if (workBuf != 0) {
        if (s_fn_BC_FindWork) s_fn_BC_FindWork(self[2], &workBuf);
        int jobCount = *(int*)(workBuf + 0x50);
        for (int j = 0; j < jobCount; j++) {
            int64_t host = *(int64_t*)((int64_t)j * 8 + *(int64_t*)(workBuf + 0x48));
            void* buildComp = BC_FindComponentByType(host, s_tid_ModelComp);
            int64_t ib1 = 0;
            if (fn_InternalBasePtr_Ctor) new (&ib1) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(buildComp));   // B.3.3 bridge
            GameColl::AsArray<GameColl::Ptr8>(self + 0xb).SetSizeFill((int)self[0xc] + 1, &ib1);

            int64_t host2 = *(int64_t*)((int64_t)j * 8 + *(int64_t*)(workBuf + 0x48));
            void* storeComp = BC_FindComponentByType(host2, s_tid_Picking);
            int64_t ib2 = 0;
            if (fn_InternalBasePtr_Ctor) new (&ib2) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(storeComp));   // B.3.3 bridge
            GameColl::AsArray<GameColl::Ptr8>(self + 0xd).SetSizeFill((int)self[0xe] + 1, &ib2);
        }
    }


    // --- Stage 3: walk our collected Build list (+0xb / count +0xc), driving each
    //     child's job hosts; finalise Storage list entries via Entity_impl_8f50.
    //
    // NOTE (2026-07-27): the `mark >= 0` branch below is retail's "reset this model to
    // build-controlled" — hide it via the model's +0x38, stamp modelVariant = -1 — so the
    // state-8 ramp can drive its construction frames. Running it on an already-finished
    // building un-frames it permanently (nothing restores a frame; it never enters state
    // 8). Retail does not special-case that here; it suppresses this whole function via
    // the instant-build bit at the top (gw+0xd0 bit 0), which the start-town spawner sets
    // across each building's setup + RegisterEntity. See StartTown_SpawnStructures.
    int buildCount = (int)self[0xc];
    for (int i = 0; i < buildCount; i++) {
        int64_t arrB = self[0xb];
        int64_t* slot = (int64_t*)(arrB + (int64_t)i * 8);
        int64_t* node = (int64_t*)*slot;
        int16_t mark = *(int16_t*)((uintptr_t)node + 0x166);
        if (mark == -1) {
            int64_t host = node[2];
            int hc = *(int*)(host + 0x38);
            for (int k = 0; k < hc; k++) {
                int64_t* h = *(int64_t**)(*(int64_t*)(host + 0x30) + (int64_t)k * 8);
                typedef void (*VFn_t)(void*);
                ((VFn_t)(*(uintptr_t*)(*h + 0x60)))(h);
            }
        } else {
            if (mark >= 0) {
                typedef void (*VFn38_t)(int64_t*);
                ((VFn38_t)(*(uintptr_t*)(*node + 0x38)))(node);
            }
            *(int16_t*)((uintptr_t)node + 0x166) = (int16_t)0xffff;
            int64_t host = node[2];
            int hc = *(int*)(host + 0x38);
            for (int k = 0; k < hc; k++) {
                int64_t* h = *(int64_t**)(*(int64_t*)(host + 0x30) + (int64_t)k * 8);
                typedef void (*VFn_t)(void*, int);
                ((VFn_t)(*(uintptr_t*)(*h + 0x60)))(h, 0);
            }
        }
        int64_t* sslot = (int64_t*)(self[0xd] + (int64_t)i * 8);
        if (*sslot != 0) {
            if (s_fn_BC_Entity8f50) s_fn_BC_Entity8f50((int64_t*)*sslot);
        }
    }

    // --- Stage 4: progress = 1; then read the WorkplaceComponent's progress at +0xb*8.
    *(int*)((uintptr_t)self + 0x4c) = 1;
    void* wp = BC_FindComponentByType(self[2], s_tid_MultiObj);
    int64_t wpIbp = 0;
    if (fn_InternalBasePtr_Ctor) new (&wpIbp) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(wp));   // B.3.3 bridge
    if (wpIbp != 0) {
        void* wp2 = BC_FindComponentByType(self[2], s_tid_MultiObj);
        if (fn_InternalBasePtr_Ctor) new (&wpIbp) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(wp2));   // B.3.3 bridge
        // Retail: bc+0x4c = WorkPlaceComponent+0x58 (the build-effort multiplier). In the
        // standalone exe the WorkPlaceComponent lifecycle is inert, so +0x58 stays 0 — which
        // would clobber the default 1 set above and collapse the build target
        // (matComp[0x30] * bc[0x4c]) to 0. Two downstream failures follow: (1) BC8_Target has
        // to clamp it back to 1 for the mesh ramp, and (2) BuildComplete_Impl (the EntityUI
        // page-routing predicate, retail 0x140109390) reads `totalRequired * bc[0x4c] <=
        // bc[0x48]` == `0 <= 0` and reports the build COMPLETE the instant materials arrive
        // (1->8) — flipping the dialog from the build page to the operational page while the
        // building is still under construction (labor not yet done). Only adopt the workplace
        // value when it's a valid positive multiplier (same clamp State1Enter applies); keep
        // the default 1 otherwise, so the target stays matComp[0x30] and routing stays "under
        // construction" until the labor accumulator (bc+0x48) actually reaches it.
        int wpMult = (int)((int64_t*)wpIbp)[0xb];
        if (wpMult > 0)
            *(int*)((uintptr_t)self + 0x4c) = wpMult;
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
    if (s_TypeId_PickingComp) {
        int count = *(int*)(compHost + 0x38);
        int64_t arr = *(int64_t*)(compHost + 0x30);
        for (int i = 0; i < count; i++) {
            int64_t* slot = (int64_t*)(arr + (int64_t)i * 8);
            void* comp = (void*)*slot;
            typedef uint32_t (*GetTypeId_t)(void*);
            uint32_t tid = ((GetTypeId_t)(*(uintptr_t*)(*(uintptr_t*)comp + 0x10)))(comp);
            if (tid == *s_TypeId_PickingComp) {
                void* c = (void*)*slot;
                if (fn_IsDerivedFrom && fn_IsDerivedFrom(c, *s_TypeId_PickingComp))
                    found = c;
                break;
            }
        }
    }
    if (fn_InternalBasePtr_Ctor) new (retbuf) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(found));   // B.3.3 bridge
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
           (void*)s_TypeId_ModelComp2, (void*)s_TypeId_PickingComp);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
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

// ============================================================
// BuildComponent message-receive handler — port of BuildComponent_Slot15
// (0x140108710), the real vtable_BuildComponent slot +0x78 ("OnMessage").
//
// This is THE build-labor +0x48 progress credit. The build action chain
// (FUN_1400c0690 / BuildStructure_SetupBuildActions) pushes a SendComponentMessage
// targeting the BuildComponent (type-id c7008) with message-code 0. When the
// citizen's PerformWork(3s) completes and the executor advances to that node,
// SendComponentMessage::AbstractImpl (sendcomponentmessage.cpp) scans the entity's
// components, matches this BuildComponent by GetTypeId, and calls its vtable+0x78.
// The handler credits ONE unit of labor into BuildComponent+0x48; State8Update
// ramps the build meshes as +0x48 climbs toward target (matComp[0x30] * bc[0x4c])
// and requests 8->2 completion once it is reached.
//
// In standalone the live-factory (CONTROLLER_LIVE_FACTORY_STATES) fills the whole
// vtable with inert no-op stubs and only overrides GetTypeId/GetDefaultState/
// CreateState — leaving +0x78 a no-op, so the credit never fired. We install this
// real handler into slot 15 (+0x78) from InstallLive (BuildComponent_InstallMsgSlot).
//   Slot15 ABI (verified vs disasm): (self /*RCX*/, int msgCode /*RDX*/, trailing
//   args unused). SendComponentMessage passes msgCode = sendMsg+0x34 (= 0 here).
static void BuildComponent_MsgReceive(void* self, int msgCode,
                                      void* /*srcEntityRef*/,
                                      uint32_t /*p1*/, uint32_t /*p2*/) {
    if (msgCode == 0) {
        int* prog = reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(self) + 0x48);
        *prog += 1;
    }
}

// Installs the real +0x78 message-receive handler into the live-factory vtable.
// Defined AFTER the CONTROLLER_LIVE_FACTORY_STATES macro (which declares the
// BuildComponent_LiveVtable array). Called from InstallLive via PopulateStateTable
// on EVERY instance creation, because InitInertTyped re-stubs the shared vtable each
// time, so real slots must be re-installed after that re-fill.
static void BuildComponent_InstallMsgSlot();

// Table field->node-slot mapping (see controller_state_table.h + buildcomponent_state8.cpp):
//   enterHandler (arg2) -> slot +0x18 Enter        (on state entry)
//   trans38Handler(arg4) -> slot +0x38 DrawAlt     (PER-FRAME; altDraw=1 every frame)
//   updateHandler(arg5) -> slot +0x20 Update       (only when a transition is pending)
//   transition handler   -> slot +0x28 OnState     (the 8->2 action)
// TableRegisterState(t, id, enterH, trans30H, trans38H, updateH):
static void BuildComponent_PopulateStateTable() {
    // (Re)install the +0x78 message-receive credit handler every InstallLive:
    // InitInertTyped re-fills the shared LiveVtable with inert stubs on each instance
    // creation, so this must run before (and independent of) the idempotent table guard.
    BuildComponent_InstallMsgSlot();
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
                               BuildComponent_NodeVtable,
                               BuildComponent_PopulateStateTable)
REGISTER_TYPE(BuildComponent, "ControllerComponent",
              &BuildComponent_Factory, &BuildComponent_CreateInPlace);

// Real vtable slot +0x78 = index 15 (15*8). BuildComponent_LiveVtable is the static
// array declared by CONTROLLER_LIVE_FACTORY_STATES above; visible from here on.
static void BuildComponent_OnRemoveVSlot(void* s) {
    if (s) BuildComponent_OnCreate(reinterpret_cast<BuildComponent*>(s));
}

// Slot 6 (+0x30) — the real OnAdded broadcast, retail 0x140108d40. See the header block
// above BuildComponent_DrawReimpl for the slot proof.
//
// DEFAULT-ON via the default-on table (graduated 2026-07-27); the env var is a kill-switch (=0).
// Wiring it un-dormants the whole body for EVERY build site at registration time rather
// than lazily at state-8 entry:
//   * Stage 1/2 fill the mesh (+0x58/+0x60) and picking (+0x68/+0x70) lists from the
//     site's own ModelComponent and from each MultiObjectComponent child. Measured: the
//     bridge master collects all of its segments, and the lazy BC8_EnsureMeshList
//     fallback drops to 0 calls — the two paths are cleanly idempotent.
//   * Stage 3 HIDES each collected model: for a model whose modelVariant (+0x166) is
//     >= 0 it calls the model's own vtable +0x38, stamps +0x166 = -1, and re-adds the
//     entity's draw children via +0x60. That is retail's "this model is now
//     build-controlled" transition — the finished mesh is replaced by nothing until
//     State8Enter/State8Update sets a construction frame, which is why a placed
//     foundation carries no building mesh until construction starts (faithful). For a
//     PRE-BUILT building that would be permanent, and retail's answer is not to
//     special-case it here: the spawner sets the instant-build bit (gw+0xd0 bit 0) across
//     SetModel + RegisterEntity, which makes the thunk above return before Stage 3 —
//     see StartTown_SpawnStructures.
//   * Stage 4 writes +0x4c from MultiObjectComponent+0x58 (clamped to keep the
//     default 1 — see the comment at the write). NOT a behaviour change on its own:
//     State1Enter reads the same finder AFTER registration, so every site that enters
//     state 1 gets that multiplier with this slot dormant too (measured: a bridge site
//     reports mult=15/21 with the flag off).
// Ordering caveat, checked and clear: at RegisterEntity time the MultiObjectComponent
// child array IS filled — the bridge master's OnAdded fires after all its segments are
// created and appended, so Stage 2 collects every one.
static void BuildComponent_OnAddedVSlot(void* s) {
    if (!s) return;
    BuildComponent_OnAddedThunk(s);
}
// Slot 3 = Shutdown/Serialize (0x140108d80). The body has been ported in this file
// since the original wave (BuildComponent_Shutdown, above) but nothing ever wrote it
// into the live vtable, so a loaded save's BuildComponent kept the FillTypedVtable
// base-Component serialize only — buildState/buildProgress/buildFlag (+0x48/+0x4c/
// +0x50) and both entity-IBP arrays (+0x60/+0x70) never round-tripped, resetting every
// construction site to its ctor default on the first post-load tick. Wired
// here, inside InstallLive (save/load wave 2026-08-27) — like the two OnAdded/OnRemove
// slots below it rides BANISHED_LIVE_BUILDCTRL, which is DEFAULT-ON (the default-on table),
// so on the default path this is effectively unconditional; the two slots below stay
// individually opt-in on their OWN env vars regardless.
static void BuildComponent_ShutdownVSlot(void* self, void* stream) {
    BuildComponent_Shutdown(reinterpret_cast<BuildComponent*>(self), stream);
}
static void BuildComponent_InstallMsgSlot() {
    BuildComponent_LiveVtable[15] = reinterpret_cast<void*>(&BuildComponent_MsgReceive);
    BuildComponent_LiveVtable[3] = reinterpret_cast<void*>(&BuildComponent_ShutdownVSlot);
    BuildComponent_LiveVtable[6] = reinterpret_cast<void*>(&BuildComponent_OnAddedVSlot);

    // Slot 7 (+0x38) is the removed-from-world broadcast, NOT OnCreate — 0x140108cb0 is
    // misnamed in Ghidra exactly like CitizenComponent's 0x140113ca0 was. It removes the
    // site from the town's pending-build registry (townMgr+0xd0), and BuildComponent's
    // capability flags (0x83) have bit 1 set, so the MarkForRemoval broadcast already
    // reaches the slot — it has just been landing on the inert stub.
    //
    // buildcomponent_state8.cpp removes the site on COMPLETION, so the hole only opens
    // for a site torn down before state 8: a cancelled/demolished foundation leaves a
    // dangling entry that BuilderPool_Tick then walks. Same bug class as the citizen
    // list. (Was opt-in until 2026-09-13; graduated with the rest of ENDGAME C.2.)
    BuildComponent_LiveVtable[7] = reinterpret_cast<void*>(&BuildComponent_OnRemoveVSlot);

}
