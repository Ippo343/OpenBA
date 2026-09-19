// code/Game/Action/action_child_ctors2.cpp
// Keystone B3 (Phase 2) — leaf Action copy-ctors + the ConsumeRawMaterial list
// helper, shared by the larger Work action-queue builders (work_act_*.cpp). Faithful
// 1:1 ports, Ghidra-verified offset-by-offset against the disassembly.
//
//   0x1400c59d0  CompleteRoad_Action_CopyCtor          (trivial field-copy ctor)
//   0x1400c8400  HuntAnimal_Action_CopyCtor            (trivial field-copy ctor)
//   0x1400c7cd0  DropOffRawMaterial_Action_CopyCtor    (trivial field-copy ctor)
//   0x1400c64b0  Entity_impl_64b0 (ConsumeRawMaterial list append + storage reserve)
//   0x1400c63b0  ConsumeRawMaterial_Action_CopyCtor    (field-copy ctor + 64b0)
//
// Closure (all registered): vtables 0x14015fb70/0x14015fde0/0x14015fd80/0x14015fc20
// (action reimpls), Object::Internal::Internal (fn_ObjectInternal_Ctor),
// Entity_FindComponent_Storage 0x140006270 (reg_work_base), and the static
// MemAlloc/MemFree engine helpers.
//
// Registration: RegisterActionChildCtors2Batch() in standalone/reg_action_child_ctors2.cpp.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"      // ENDGAME B.3.4b System callers

namespace {
static uintptr_t s_vt_Action       = 0;  // 0x14015faa0
static uintptr_t s_vt_CompleteRoad = 0;  // 0x14015fb70
static uintptr_t s_vt_HuntAnimal   = 0;  // 0x14015fde0
static uintptr_t s_vt_DropOffRaw   = 0;  // 0x14015fd80
static uintptr_t s_vt_ConsumeRaw   = 0;  // 0x14015fc20
static uintptr_t s_vt_PickupStored = 0;  // 0x14015ff80

typedef void* (*fn_findStorage_t)(int64_t inner, void* retIBP);  // 0x140006270
typedef int   (*fn_calc116540_t)(int64_t comp);                  // 0x140116540
static fn_findStorage_t s_fn_findStorage = nullptr;
static fn_calc116540_t  s_fn_calc116540  = nullptr;
// RawMaterialDescription type-id global (0x1401c7cb8). Registered+filled (via
// GetTypeIdByName after Engine::Startup) in reg_action_child_ctors2.cpp; read here.
static const uint32_t* s_pTid_RawMatDesc = nullptr;
// StateMachineComponent type-id global (0x1401c7350) — for the bef70 component-find leaf.
static const uint32_t* s_pTid_StateMachine = nullptr;
// System::Random::RandFloat (Runtime export) — for the bef70 weighted-random leaf.
typedef float (*fn_randFloat_t)(void* random);
static fn_randFloat_t s_fn_randFloat = nullptr;

static bool s_init = false;
static void EnsureInit() {
    if (s_init) return;
    s_vt_Action       = ResolveAddr(Module::Exe, 0x14015faa0);
    s_vt_CompleteRoad = ResolveAddr(Module::Exe, 0x14015fb70);
    s_vt_HuntAnimal   = ResolveAddr(Module::Exe, 0x14015fde0);
    s_vt_DropOffRaw   = ResolveAddr(Module::Exe, 0x14015fd80);
    s_vt_ConsumeRaw   = ResolveAddr(Module::Exe, 0x14015fc20);
    s_vt_PickupStored = ResolveAddr(Module::Exe, 0x14015ff80);
    s_fn_findStorage  = reinterpret_cast<fn_findStorage_t>(ResolveAddr(Module::Exe, 0x140006270));
    s_fn_calc116540   = reinterpret_cast<fn_calc116540_t >(ResolveAddr(Module::Exe, 0x140116540));
    s_pTid_RawMatDesc = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7cb8));
    s_pTid_StateMachine = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7350));
    s_fn_randFloat = reinterpret_cast<fn_randFloat_t>(ResolveExport("?RandFloat@Random@System@@QEAAMXZ"));
    s_init = true;
}
} // namespace

// ============================================================
// FUN_1400c59d0 — CompleteRoad_Action_CopyCtor
//   RCX=self, EDX=p2(u32 @ +0x24), R8D=p3(u32 @ +0x20), R9B=p4(byte @ +0x28).
// ============================================================
void* ActChild_CompleteRoadCopyCtor_c59d0(void* self, uint32_t p2, uint32_t p3, uint8_t p4) {
    EnsureInit();
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (fn_ObjectInternal_Ctor) fn_ObjectInternal_Ctor(self);
    *reinterpret_cast<uintptr_t*>(s)       = s_vt_Action;
    *reinterpret_cast<uint64_t*>(s + 0x10) = 0;
    s[0x18]                                = 0;
    *reinterpret_cast<uintptr_t*>(s)       = s_vt_CompleteRoad;
    *reinterpret_cast<uint32_t*>(s + 0x20) = p3;
    *reinterpret_cast<uint32_t*>(s + 0x24) = p2;
    s[0x28]                                = p4;
    return self;
}

// ============================================================
// FUN_1400c8400 — HuntAnimal_Action_CopyCtor
//   RCX=self, RDX=p2, R8=p3.  +0x20 = *p3 ; +0x28 = *p2 ; +0x30 = 0 ; +0x38 = 0(byte).
// ============================================================
void* ActChild_HuntAnimalCopyCtor_c8400(void* self, uint64_t* p2, uint64_t* p3) {
    EnsureInit();
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (fn_ObjectInternal_Ctor) fn_ObjectInternal_Ctor(self);
    *reinterpret_cast<uintptr_t*>(s)       = s_vt_Action;
    *reinterpret_cast<uint64_t*>(s + 0x10) = 0;
    s[0x18]                                = 0;
    *reinterpret_cast<uintptr_t*>(s)       = s_vt_HuntAnimal;
    *reinterpret_cast<uint64_t*>(s + 0x20) = 0;
    *reinterpret_cast<uint64_t*>(s + 0x20) = *p3;
    *reinterpret_cast<uint64_t*>(s + 0x28) = 0;
    *reinterpret_cast<uint64_t*>(s + 0x28) = *p2;
    *reinterpret_cast<uint64_t*>(s + 0x30) = 0;
    s[0x38]                                = 0;
    return self;
}

// ============================================================
// FUN_1400c7cd0 — DropOffRawMaterial_Action_CopyCtor
//   RCX=self, RDX=p2, R8=p3, R9=p4(u64), [stack]=p5(u32), p6(byte).
//   +0x20=p6(byte) ; +0x24=p5(u32) ; +0x28=p4(u64) ; +0x30=*p2 ; +0x38=*p3.
// ============================================================
void* ActChild_DropOffRawCopyCtor_c7cd0(void* self, uint64_t* p2, uint64_t* p3,
                                        uint64_t p4, uint32_t p5, uint8_t p6) {
    EnsureInit();
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (fn_ObjectInternal_Ctor) fn_ObjectInternal_Ctor(self);
    *reinterpret_cast<uintptr_t*>(s)       = s_vt_Action;
    *reinterpret_cast<uint64_t*>(s + 0x10) = 0;
    s[0x18]                                = 0;
    *reinterpret_cast<uintptr_t*>(s)       = s_vt_DropOffRaw;
    s[0x20]                                = p6;
    *reinterpret_cast<uint32_t*>(s + 0x24) = p5;
    *reinterpret_cast<uint64_t*>(s + 0x28) = p4;
    *reinterpret_cast<uint64_t*>(s + 0x30) = 0;
    *reinterpret_cast<uint64_t*>(s + 0x30) = *p2;
    *reinterpret_cast<uint64_t*>(s + 0x38) = 0;
    *reinterpret_cast<uint64_t*>(s + 0x38) = *p3;
    return self;
}

// ============================================================
// FUN_1400c64b0 — Entity_impl_64b0 — ConsumeRawMaterial consume-list append.
// Adds a (resType, count) entry to the action's consume list (self+0x28 array,
// +0x30 count, +0x34 capacity, stride 0x10: [+0]=resType u64, [+8]=count int,
// [+0xc]=reserved int). If self+0x38 (the "reserve" flag) is set, first bumps the
// matching Storage-component inventory's reserved field (+0xc) by `count`.
//   param_1 = self (the ConsumeRawMaterial action), param_2 = resType, param_3 = count.
// ============================================================
void Entity_ConsumeListAppend_64b0(int64_t self, int64_t resType, int count) {
    EnsureInit();
    int64_t storageIbp = 0;
    if (s_fn_findStorage) s_fn_findStorage(*reinterpret_cast<int64_t*>(self + 0x20), &storageIbp);

    if (*reinterpret_cast<char*>(self + 0x38) != '\0') {
        int invCount = *reinterpret_cast<int*>(storageIbp + 0x30);
        if (0 < invCount) {
            int64_t* arr = *reinterpret_cast<int64_t**>(storageIbp + 0x28);
            int idx = 0;
            int64_t* p = arr;
            while (true) {
                if (*p == resType) {
                    if (idx >= 0) {
                        int64_t base = *reinterpret_cast<int64_t*>(storageIbp + 0x28);
                        int* reserved = reinterpret_cast<int*>(base + (int64_t)idx * 0x10 + 0xc);
                        *reserved += count;
                    }
                    break;
                }
                idx++;
                p += 2;
                if ((int64_t)idx >= invCount) break;
            }
        }
    }

    // append {resType, count} to self+0x28 (count +0x30, cap +0x34, stride 0x10)
    uint32_t oldCount = *reinterpret_cast<uint32_t*>(self + 0x30);
    uint32_t newCount = oldCount + 1;
    if (newCount == oldCount) return;  // overflow guard (never)

    // The new entry: [0]=resType, [8]=count, [0xc]=reserved(0).
    int64_t newEntry[2];
    newEntry[0] = resType;
    reinterpret_cast<int*>(&newEntry[1])[0] = count;
    reinterpret_cast<int*>(&newEntry[1])[1] = 0;

    void* oldArr = *reinterpret_cast<void**>(self + 0x28);
    *reinterpret_cast<uint32_t*>(self + 0x30) = newCount;
    if (*reinterpret_cast<int*>(self + 0x34) < (int)newCount) {
        *reinterpret_cast<uint32_t*>(self + 0x34) = newCount;
        if (oldCount != 0) *reinterpret_cast<uint32_t*>(self + 0x34) = newCount * 3;
        void* na = (fn_MemAlloc && g_pMemAllocSingleton)
            ? fn_MemAlloc(g_pMemAllocSingleton,
                          (uint64_t)((int64_t)*reinterpret_cast<int*>(self + 0x34) << 4), 8)
            : nullptr;
        *reinterpret_cast<void**>(self + 0x28) = na;
    }
    int copyStart = 0;
    if (oldArr == *reinterpret_cast<void**>(self + 0x28)) copyStart = (int)oldCount;
    int64_t i = copyStart;
    if (i < (int)newCount) {
        int64_t byteOff = i << 4;
        do {
            int64_t* dst = reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(self + 0x28) + byteOff);
            if (dst != nullptr) {
                int64_t* src = (i < (int)oldCount)
                    ? reinterpret_cast<int64_t*>((int64_t)i * 0x10 + reinterpret_cast<int64_t>(oldArr))
                    : newEntry;
                dst[0] = src[0];
                dst[1] = src[1];
            }
            i++;
            byteOff += 0x10;
        } while (i < (int)newCount);
    }
    if (oldArr != *reinterpret_cast<void**>(self + 0x28) && fn_MemFreeStatic) fn_MemFreeStatic(oldArr);
}

// ============================================================
// FUN_1400c63b0 — ConsumeRawMaterial_Action_CopyCtor
//   RCX=self, RDX=p2, R8=p3(resType), R9=p4(u64, unused), [stack]=p5(byte).
//   +0x20=*p2 ; +0x28=0 ; +0x30=0 ; +0x38=p5(byte); then 64b0(self, p3, 1).
// ============================================================
void* ActChild_ConsumeRawCopyCtor_c63b0(void* self, uint64_t* p2, int64_t p3,
                                        uint64_t /*p4*/, uint8_t p5) {
    EnsureInit();
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (fn_ObjectInternal_Ctor) fn_ObjectInternal_Ctor(self);
    *reinterpret_cast<uintptr_t*>(s)       = s_vt_Action;
    *reinterpret_cast<uint64_t*>(s + 0x10) = 0;
    s[0x18]                                = 0;
    *reinterpret_cast<uintptr_t*>(s)       = s_vt_ConsumeRaw;
    *reinterpret_cast<uint64_t*>(s + 0x20) = 0;
    *reinterpret_cast<uint64_t*>(s + 0x20) = *p2;
    *reinterpret_cast<uint64_t*>(s + 0x28) = 0;
    *reinterpret_cast<uint64_t*>(s + 0x30) = 0;
    s[0x38]                                = p5;
    Entity_ConsumeListAppend_64b0(reinterpret_cast<int64_t>(self), p3, 1);
    return self;
}

// ============================================================
// FUN_1400ca230 — Resource_TransferToCarrier
// Add `count` of resource `resType` to the carrier list at self+0x30 (count +0x38,
// cap +0x3c, stride 0x10: [+0]=resType u64, [+8]=count int, [+0xc]=reserved int),
// and bump the matching storage-inventory reserved field (+0xc) by `count`.
//   self = the PickupStored action; resType = resource type id; count = amount.
// ============================================================
void ActChild_TransferToCarrier_ca230(int64_t self, int64_t resType, int count) {
    EnsureInit();
    int64_t storageIbp = 0;
    if (s_fn_findStorage) s_fn_findStorage(*reinterpret_cast<int64_t*>(self + 0x28), &storageIbp);

    // bump storage inventory reserved(+0xc) for the matching resType
    int invCount = *reinterpret_cast<int*>(storageIbp + 0x30);
    if (0 < invCount) {
        int64_t* p = *reinterpret_cast<int64_t**>(storageIbp + 0x28);
        int idx = 0;
        while (true) {
            if (*p == resType) {
                if (idx >= 0) {
                    int64_t base = *reinterpret_cast<int64_t*>(storageIbp + 0x28);
                    *reinterpret_cast<int*>(base + (int64_t)idx * 0x10 + 0xc) += count;
                }
                break;
            }
            idx++; p += 2;
            if (idx >= invCount) break;
        }
    }

    // carrier-list find: if resType already present, add to its count(+8) and return
    int carCount = *reinterpret_cast<int*>(self + 0x38);
    if (0 < carCount) {
        int64_t* p = *reinterpret_cast<int64_t**>(self + 0x30);
        int idx = 0;
        while (true) {
            if (*p == resType) {
                int64_t base = *reinterpret_cast<int64_t*>(self + 0x30);
                *reinterpret_cast<int*>(base + (int64_t)idx * 0x10 + 8) += count;
                return;
            }
            idx++; p += 2;
            if (idx >= carCount) break;
        }
    }

    // not found: append {resType, count, reserved=0} to the carrier list (grow-if-full)
    uint32_t oldCount = *reinterpret_cast<uint32_t*>(self + 0x38);
    uint32_t newCount = oldCount + 1;
    if (newCount == oldCount) return;
    int64_t newEntry[2];
    newEntry[0] = resType;
    reinterpret_cast<int*>(&newEntry[1])[0] = count;
    reinterpret_cast<int*>(&newEntry[1])[1] = 0;
    void* oldArr = *reinterpret_cast<void**>(self + 0x30);
    *reinterpret_cast<uint32_t*>(self + 0x38) = newCount;
    if (*reinterpret_cast<int*>(self + 0x3c) < (int)newCount) {
        *reinterpret_cast<uint32_t*>(self + 0x3c) = newCount;
        if (oldCount != 0) *reinterpret_cast<uint32_t*>(self + 0x3c) = newCount * 3;
        void* na = (fn_MemAlloc && g_pMemAllocSingleton)
            ? fn_MemAlloc(g_pMemAllocSingleton,
                          (uint64_t)((int64_t)*reinterpret_cast<int*>(self + 0x3c) << 4), 8)
            : nullptr;
        *reinterpret_cast<void**>(self + 0x30) = na;
    }
    int copyStart = 0;
    if (oldArr == *reinterpret_cast<void**>(self + 0x30)) copyStart = (int)oldCount;
    int64_t i = copyStart;
    if (i < (int)newCount) {
        int64_t byteOff = i << 4;
        do {
            int64_t* dst = reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(self + 0x30) + byteOff);
            if (dst != nullptr) {
                int64_t* src = (i < (int)oldCount)
                    ? reinterpret_cast<int64_t*>((int64_t)i * 0x10 + reinterpret_cast<int64_t>(oldArr))
                    : newEntry;
                dst[0] = src[0];
                dst[1] = src[1];
            }
            i++;
            byteOff += 0x10;
        } while (i < (int)newCount);
    }
    if (oldArr != *reinterpret_cast<void**>(self + 0x30) && fn_MemFreeStatic) fn_MemFreeStatic(oldArr);
}

// ============================================================
// FUN_1400ca520 — PickupStoredRawMaterial_Action_CopyCtor
//   RCX=self, RDX=param_2, R8=param_3, R9=param_4(resType u64),
//   [stack]=param_5(int requested), param_6(byte @ +0x40).
// Field-init then, if param_4!=0: locate the source entity's RawMaterialDescription
// (component-array scan vs type-id 0x1401c7cb8) to get its per-unit size at +0x48;
// if requested(param_5)<=0 compute it from FUN_140116540(self+0x20)/perUnitSize;
// clamp to the storage availability (count-reserved) for resType; then transfer.
// ============================================================
void* ActChild_PickupStoredCopyCtor_ca520(void* self, uint64_t* param_2, uint64_t* param_3,
                                          int64_t param_4, int param_5, uint8_t param_6) {
    EnsureInit();
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (fn_ObjectInternal_Ctor) fn_ObjectInternal_Ctor(self);
    *reinterpret_cast<uintptr_t*>(s)       = s_vt_Action;
    *reinterpret_cast<uint64_t*>(s + 0x10) = 0;
    s[0x18]                                = 0;
    *reinterpret_cast<uintptr_t*>(s)       = s_vt_PickupStored;
    *reinterpret_cast<uint64_t*>(s + 0x20) = 0;
    *reinterpret_cast<uint64_t*>(s + 0x20) = *param_2;
    *reinterpret_cast<uint64_t*>(s + 0x28) = 0;
    *reinterpret_cast<uint64_t*>(s + 0x28) = *param_3;
    *reinterpret_cast<uint64_t*>(s + 0x30) = 0;
    *reinterpret_cast<uint64_t*>(s + 0x38) = 0;
    s[0x40]                                = param_6;

    int64_t storageIbp = 0;
    if (s_fn_findStorage) s_fn_findStorage(*reinterpret_cast<int64_t*>(s + 0x28), &storageIbp);

    if (param_4 == 0) return self;

    uint32_t wantTid = s_pTid_RawMatDesc ? *s_pTid_RawMatDesc : 0;

    // component-array scan of param_4 for child GetTypeId == wantTid, IsDerivedFrom-confirm
    void* rawMatDesc = nullptr;
    int srcCount = *reinterpret_cast<int*>(param_4 + 0x30);
    if (0 < srcCount) {
        int idx = 0;
        int64_t byteOff = 0;
        while (true) {
            int64_t arr = *reinterpret_cast<int64_t*>(param_4 + 0x28);
            void* child = *reinterpret_cast<void**>(arr + byteOff);
            uint32_t tid = (*reinterpret_cast<uint32_t(**)(void*)>(*reinterpret_cast<int64_t*>(child) + 0x10))(child);
            if (tid == wantTid) {
                int64_t arr2 = *reinterpret_cast<int64_t*>(param_4 + 0x28);
                void* match = *reinterpret_cast<void**>(arr2 + (int64_t)idx * 8);
                if (s_pTid_RawMatDesc && fn_IsDerivedFrom && fn_IsDerivedFrom(match, *s_pTid_RawMatDesc))
                    rawMatDesc = match;
                break;
            }
            idx++; byteOff += 8;
            if (idx >= srcCount) break;
        }
    }

    int requested = param_5;
    if (requested <= 0) {
        int total = s_fn_calc116540 ? s_fn_calc116540(*reinterpret_cast<int64_t*>(s + 0x20)) : 0;
        // perUnitSize = *(int*)(rawMatDesc+0x48); guard the null-find (original assumes found).
        int perUnit = rawMatDesc ? *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(rawMatDesc) + 0x48) : 0;
        requested = (perUnit != 0) ? total / perUnit : 0;
    }

    // storage availability for resType param_4 = count(+8) - reserved(+0xc); clamp requested down to it
    int avail = 0;
    {
        int invCount = *reinterpret_cast<int*>(storageIbp + 0x30);
        if (0 < invCount) {
            int64_t* inv = *reinterpret_cast<int64_t**>(storageIbp + 0x28);
            int idx = 0; int64_t* p = inv;
            while (true) {
                if (*p == param_4) {
                    if (idx >= 0)
                        avail = reinterpret_cast<int*>(inv)[(int64_t)idx * 4 + 2]
                              - reinterpret_cast<int*>(reinterpret_cast<int64_t>(inv) + (int64_t)idx * 0x10 + 0xc)[0];
                    break;
                }
                idx++; p += 2;
                if (idx >= invCount) break;
            }
        }
    }
    if (avail < requested) {
        // re-scan to set requested = avail (count - reserved)
        requested = 0;
        int invCount = *reinterpret_cast<int*>(storageIbp + 0x30);
        if (0 < invCount) {
            int64_t* inv = *reinterpret_cast<int64_t**>(storageIbp + 0x28);
            int idx = 0; int64_t* p = inv;
            while (true) {
                if (*p == param_4) {
                    if (idx >= 0)
                        requested = reinterpret_cast<int*>(inv)[(int64_t)idx * 4 + 2]
                                  - reinterpret_cast<int*>(reinterpret_cast<int64_t>(inv) + (int64_t)idx * 0x10 + 0xc)[0];
                    break;
                }
                idx++; p += 2;
                if (idx >= invCount) break;
            }
        }
    }

    ActChild_TransferToCarrier_ca230(reinterpret_cast<int64_t>(self), param_4, requested);
    return self;
}

// ============================================================
// FUN_1400c6440 — ConsumeRawMaterial_Action_Ctor3 (2-arg variant)
//   RCX=self, RDX=param_2.  vtable ConsumeRawMaterial; +0x20=*param_2, +0x28=0, +0x30=0.
// ============================================================
void* ActChild_ConsumeRawCtor3_c6440(void* self, uint64_t* param_2) {
    EnsureInit();
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (fn_ObjectInternal_Ctor) fn_ObjectInternal_Ctor(self);
    *reinterpret_cast<uintptr_t*>(s)       = s_vt_Action;
    *reinterpret_cast<uint64_t*>(s + 0x10) = 0;
    s[0x18]                                = 0;
    *reinterpret_cast<uintptr_t*>(s)       = s_vt_ConsumeRaw;
    *reinterpret_cast<uint64_t*>(s + 0x20) = 0;
    *reinterpret_cast<uint64_t*>(s + 0x20) = *param_2;
    *reinterpret_cast<uint64_t*>(s + 0x28) = 0;
    *reinterpret_cast<uint64_t*>(s + 0x30) = 0;
    return self;
}

// ============================================================
// FUN_14000b6f0 — find the StateMachineComponent on an entity's component list.
// Walks param_1's list (ptr +0x30, count +0x38, stride 8): child whose GetTypeId
// (vtable+0x10) == *0x1401c7350, IsDerivedFrom-confirmed → found; writes the IBP into
// param_2 via InternalBasePtr_Ctor and returns param_2.
// ============================================================
void* Entity_FindConsumeProduce_b6f0(int64_t param_1, void* param_2) {
    EnsureInit();
    void* found = nullptr;
    uint32_t wantTid = s_pTid_StateMachine ? *s_pTid_StateMachine : 0;
    int count = *reinterpret_cast<int*>(param_1 + 0x38);
    if (0 < count) {
        int idx = 0;
        int64_t byteOff = 0;
        while (true) {
            int64_t* slot = reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(param_1 + 0x30) + byteOff);
            void* child = reinterpret_cast<void*>(*slot);
            uint32_t tid = (*reinterpret_cast<uint32_t(**)(void*)>(*reinterpret_cast<int64_t*>(child) + 0x10))(child);
            if (tid == wantTid) {
                int64_t* mslot = reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(param_1 + 0x30) + (int64_t)idx * 8);
                void* match = reinterpret_cast<void*>(*mslot);
                if (s_pTid_StateMachine && fn_IsDerivedFrom && fn_IsDerivedFrom(match, *s_pTid_StateMachine))
                    found = match;
                break;
            }
            idx++; byteOff += 8;
            if (idx >= count) break;
        }
    }
    if (fn_InternalBasePtr_Ctor) new (param_2) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(found));   // B.3.3 bridge
    return param_2;
}

// ============================================================
// FUN_140119470 — weighted-random index over a float-weight array.
// If *(int*)(param_1+0x50) == -1: sum the weights[ +0x58 ] (count +0x60), draw
// r = RandFloat(world.random), walk the weights again returning the first index where
// the running sum exceeds r*total. Otherwise (or no weights) returns 0.
// (Returns ulonglong index; non-negative *(int*)(+0x50) returns that value.)
// ============================================================
uint64_t WeightedRandomIndex_119470(int64_t param_1) {
    EnsureInit();
    uint32_t sel = *reinterpret_cast<uint32_t*>(param_1 + 0x50);
    uint64_t result = sel;
    if ((int)sel < 0) {
        if (sel == 0xffffffff) {
            int n = *reinterpret_cast<int*>(param_1 + 0x60);
            float total = 0.0f;
            if (0 < n) {
                float* w = *reinterpret_cast<float**>(param_1 + 0x58);
                for (int k = 0; k < n; ++k) total += w[k];
            }
            void* random = g_pGameWorldSingleton
                ? *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xd8)
                : nullptr;
            float r = (s_fn_randFloat && random) ? reinterpret_cast<System::Random*>(random)->RandFloat() : 0.0f;   // B.3.4b bridge
            result = 0;
            if (0 < n) {
                float* w = *reinterpret_cast<float**>(param_1 + 0x58);
                float running = 0.0f;
                for (int k = 0; k < n; ++k) {
                    running += w[k];
                    if (r * total < running) return (uint64_t)k;
                }
            }
        }
        result = 0;
    }
    return result;
}
