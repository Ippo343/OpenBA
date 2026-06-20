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
// (action reimpls), Object::Internal::Internal (fn_ObjectInternal_Ctor), CheckPtr,
// Entity_FindComponent_Storage 0x140006270 (reg_work_base), and the static
// MemAlloc/MemFree engine helpers.
//
// Registration: RegisterActionChildCtors2Batch() in standalone/reg_action_child_ctors2.cpp.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

namespace {
static uintptr_t s_vt_Action       = 0;  // 0x14015faa0
static uintptr_t s_vt_CompleteRoad = 0;  // 0x14015fb70
static uintptr_t s_vt_HuntAnimal   = 0;  // 0x14015fde0
static uintptr_t s_vt_DropOffRaw   = 0;  // 0x14015fd80
static uintptr_t s_vt_ConsumeRaw   = 0;  // 0x14015fc20

typedef void* (*fn_findStorage_t)(int64_t inner, void* retIBP);  // 0x140006270
static fn_findStorage_t s_fn_findStorage = nullptr;

static bool s_init = false;
static void EnsureInit() {
    if (s_init) return;
    s_vt_Action       = ResolveAddr(Module::Exe, 0x14015faa0);
    s_vt_CompleteRoad = ResolveAddr(Module::Exe, 0x14015fb70);
    s_vt_HuntAnimal   = ResolveAddr(Module::Exe, 0x14015fde0);
    s_vt_DropOffRaw   = ResolveAddr(Module::Exe, 0x14015fd80);
    s_vt_ConsumeRaw   = ResolveAddr(Module::Exe, 0x14015fc20);
    s_fn_findStorage  = reinterpret_cast<fn_findStorage_t>(ResolveAddr(Module::Exe, 0x140006270));
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
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(self + 0x20));
    if (s_fn_findStorage) s_fn_findStorage(*reinterpret_cast<int64_t*>(self + 0x20), &storageIbp);

    if (*reinterpret_cast<char*>(self + 0x38) != '\0') {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&storageIbp);
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
