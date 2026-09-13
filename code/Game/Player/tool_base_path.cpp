// code/Game/Player/tool_base_path.cpp
// Player registration wave (batch "path") — shared exe sub-helpers referenced
// from code/Game/Player/removaltool.cpp (RemovalTool::Draw / per-tile checks).
//
// These are exe sub-helpers resolved via ResolveAddr(Module::Exe, 0xADDR) from
// the RemovalTool reimpl. They resolve to 0 (silently skipped) unless registered.
// This file provides faithful 1:1 reimplementations and a single registration point
// (RegisterPlayerPathBatch in standalone/reg_player_path.cpp) so the call sites
// fire real logic.
//
// Each function is a 1:1 port of the decompiled exe helper; do not "improve"
// behavior.
//
// Closure note: every exe sub-callee reached by the bodies here is either
// (a) registered elsewhere (FUN_1400052c0 area-self find, FUN_14009f3e0
//     CharComp height query, dispatch slot fn_InternalBasePtr_*),
// (b) registered in this batch bottom-up (the FindComponent leaves 5780/3ea0,
//     FUN_14011e710, CharComp f5d0),
// (c) a Runtime export resolved via engine_funcs.h.
//
// DEFERRED (NOT here): PathTool helper 0x1400dd710 (closure pulls in the
// 520-line AVL builder d770 + ~12 unregistered exe sub-callees), RemovalTool
// CalcPath 0x1401035e0 (~770 lines + ~15 unregistered sub-fns), and
// QuarryComponent_impl_a530 0x1400aa530 RaycastTerrain (closure IS satisfied but
// its float-march pointer-aliased body is too risky to port faithfully in this
// wave; a wrong port is silent corruption). See reg_player_path.cpp.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstdlib>

// ============================================================
// type-ID storage globals (exe .rdata, filled by RegisterType)
// ============================================================
static const uint32_t* s_pTid_75f0 = nullptr;  // 0x1401c75f0  (FUN_140005780)
static const uint32_t* s_pTid_7d98 = nullptr;  // 0x1401c7d98  (FUN_140003ea0)
static const uint32_t* s_pTid_7430 = nullptr;  // 0x1401c7430  (crop/field description, sub_48a0)
static bool s_tidInit = false;

static void EnsureTypeIds() {
    if (s_tidInit) return;
    s_pTid_75f0 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c75f0));
    s_pTid_7d98 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7d98));
    s_pTid_7430 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7430));
    s_tidInit = true;
}

// ============================================================
// FindComponent leaves — clones of the FUN_140005380 family. Each scans the
// entity's component list (ptr array at entity+0x30, count at entity+0x38),
// calls each component's GetTypeId (vtable +0x10), and on the first type match
// runs an IsDerivedFrom recheck before returning the component through an out
// InternalBasePtr.  Signature: void* fn(longlong entity, void* retIBP)
// ============================================================
static void* FindComponentByTypeId(uintptr_t entity, void* retIBP, const uint32_t* pTypeId) {
    void* found = nullptr;
    if (pTypeId) {
        int n = *reinterpret_cast<int*>(entity + 0x38);
        uintptr_t list = *reinterpret_cast<uintptr_t*>(entity + 0x30);
        int i = 0;
        for (; i < n; i++) {
            void** slot = reinterpret_cast<void**>(list + static_cast<int64_t>(i) * 8);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
            void* comp = *slot;
            uint32_t t = (*reinterpret_cast<uint32_t(**)(void*)>(*reinterpret_cast<uintptr_t*>(comp) + 0x10))(comp);
            if (t == *pTypeId) {
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
                void* c = *slot;
                found = (fn_IsDerivedFrom && fn_IsDerivedFrom(c, *pTypeId)) ? c : nullptr;
                break;
            }
        }
    }
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(retIBP, found);
    return retIBP;
}

// Reimpl of FUN_140005780 — FindComponent (type-ID DWORD_1401c75f0)
void* PlayerPath_FindComponent_5780(uintptr_t entity, void* retIBP) {
    EnsureTypeIds();
    return FindComponentByTypeId(entity, retIBP, s_pTid_75f0);
}

// Reimpl of FUN_140003ea0 — FindComponent (type-ID DWORD_1401c7d98)
void* PlayerPath_FindComponent_3ea0(uintptr_t entity, void* retIBP) {
    EnsureTypeIds();
    return FindComponentByTypeId(entity, retIBP, s_pTid_7d98);
}

// FUN_1400052c0 (FindComponent, type-ID DAT_1401c7b30) is already registered
// elsewhere. sub_48a0 needs it; call the registered exe function so we stay 1:1
// (do NOT duplicate the body).
typedef void* (*fn_find052c0_t)(uintptr_t, void*);
static fn_find052c0_t s_fn_find052c0 = nullptr;
static void* FindComponent_AreaSelf(uintptr_t entity, void* retIBP) {
    if (!s_fn_find052c0)
        s_fn_find052c0 = reinterpret_cast<fn_find052c0_t>(ResolveAddr(Module::Exe, 0x1400052c0));
    if (s_fn_find052c0) return s_fn_find052c0(entity, retIBP);
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(retIBP, nullptr);
    return retIBP;
}

// ============================================================
// FUN_14011e710 — "is removable child" check.
// CheckPtr(entity+0x10), FUN_140003ea0 on entity+0x10, then reads the result's
// state byte (+0x52, fallback +0x50 when negative) and returns 1 iff it is
// 3, 4, or 5.  Signature: undefined8 fn(longlong param_1)
// ============================================================
uint64_t PlayerPath_Sub_11e710(uintptr_t param_1) {
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)(param_1 + 0x10));
    uint64_t resIBP[4] = {0};
    PlayerPath_FindComponent_3ea0(*reinterpret_cast<uintptr_t*>(param_1 + 0x10), resIBP);

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(resIBP);
    char cVar1 = *reinterpret_cast<char*>(resIBP[0] + 0x52);
    if (cVar1 < 0) cVar1 = *reinterpret_cast<char*>(resIBP[0] + 0x50);
    if (cVar1 != 3) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(resIBP);
        cVar1 = *reinterpret_cast<char*>(resIBP[0] + 0x52);
        if (cVar1 < 0) cVar1 = *reinterpret_cast<char*>(resIBP[0] + 0x50);
        if (cVar1 != 4) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(resIBP);
            cVar1 = *reinterpret_cast<char*>(resIBP[0] + 0x52);
            if (cVar1 < 0) cVar1 = *reinterpret_cast<char*>(resIBP[0] + 0x50);
            if (cVar1 != 5) return 0;
        }
    }
    return 1;
}

// ============================================================
// CharacterComponent_impl_f5d0 (0x14009f5d0) — single-cell terrain/height test
// used by RemovalTool_sub_48a0. Validates cell (param_2,param_3) against
// pass-mask param_4, ownership param_10, override flags param_8/param_9, then
// runs CharacterComponent_impl_f3e0 (already registered) and compares the
// returned height to param_5/param_6/param_7.
//
// Faithful 1:1 port. DAT_140172114 = 0.5f. Math::Scalar::Abs(x) == fabsf(x).
// Cell stride 0x30; cell array base at byte +0x80 of the map grid (param_1).
// f3e0 writes its result Vector3 (x,y,z) into outPos; the decompile's
// 'local_28' (compared to param_5) is the y term, 'local_18' (compared with
// param_7) is the z term.
//
// Signature: bool fn(int* p1, int p2, int p3, uint p4, float p5, float p6,
//                    float p7, char p8, char p9, longlong p10)
// ============================================================
typedef float* (*fn_CharComp_f3e0_t)(int* p1, float* p2, float* p3, char p4, void* p5);
static fn_CharComp_f3e0_t s_fn_CharComp_f3e0 = nullptr;
static bool s_f3e0Init = false;

bool PlayerPath_CharComp_f5d0(int* param_1, int param_2, int param_3, uint32_t param_4,
                              float param_5, float param_6, float param_7,
                              char param_8, char param_9, int64_t param_10) {
    if (!s_f3e0Init) {
        s_fn_CharComp_f3e0 = reinterpret_cast<fn_CharComp_f3e0_t>(
            ResolveAddr(Module::Exe, 0x14009f3e0));
        s_f3e0Init = true;
    }
    if (!s_fn_CharComp_f3e0) return false;

    const float kHalf = 0.5f;  // DAT_140172114
    uintptr_t p1 = reinterpret_cast<uintptr_t>(param_1);

    if (!((-1 < param_2) && (param_2 < *param_1) && (-1 < param_3) && (param_3 < *param_1)))
        return false;

    int iVar2 = (*param_1) * param_3 + param_2;
    uintptr_t cell = static_cast<int64_t>(iVar2) * 0x30 + *reinterpret_cast<int64_t*>(p1 + 0x80);

    uint8_t passByte = *reinterpret_cast<uint8_t*>(cell + 0x29);
    if ((~param_4 & (1u << (passByte & 0x1f))) != 0)
        return false;

    int64_t owner = *reinterpret_cast<int64_t*>(cell);  // *self
    bool ownOk;
    if (param_10 != 0) {
        if (owner == 0) {
            ownOk = true;
        } else {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(cell));
            owner = *reinterpret_cast<int64_t*>(cell);
            ownOk = (*reinterpret_cast<int64_t*>(owner + 0x28) == param_10);
        }
    } else {
        ownOk = false;
    }
    if (!(ownOk || (param_8 != 0) || (*reinterpret_cast<int64_t*>(cell) == 0)))
        return false;

    // (char)self[5] -> byte at cell+0x28
    char cellFlag = *reinterpret_cast<char*>(cell + 0x28);
    if (!((param_9 != 0) || (cellFlag == 0)))
        return false;

    // Contiguous Vector3 locals. f3e0 writes a Vector3 to BOTH the main output
    // (2nd arg, = decompile local_20) AND the 5th-arg buffer (= local_30); the
    // 5th buffer must be a full Vector3 (3 floats), since local_28 (compared with
    // param_5) is its 3rd float ([RSP+0x50] = local_30+8). Verified from disasm
    // 0x14009f6ea-0x14009f725.
    float outPos[3]   = {0.0f, 0.0f, 0.0f};   // main output vec (local_20)
    float queryPos[2];                         // input pos (local_40/local_3c)
    float extraOut[3] = {0.0f, 0.0f, 0.0f};    // 5th-arg output vec (local_30)

    float base = (float)(*param_1 >> 1) - kHalf;
    queryPos[0] = (float)(iVar2 % *param_1) - base;
    queryPos[1] = (float)(iVar2 / *param_1) - base;

    s_fn_CharComp_f3e0(param_1, outPos, queryPos, '\0', extraOut);

    // param_5 ([RSP+0xa0]) <= extraOut.z ([RSP+0x50]); then outPos.z ([RSP+0x60])
    // - param_7, Abs, <= param_6 ([RSP+0xa8]).  (P4 draft used outPos[1] here — wrong lane.)
    if (param_5 <= extraOut[2]) {
        float diff = outPos[2] - param_7;
        return fabsf(diff) <= param_6;
    }
    return false;
}

// ============================================================
// RemovalTool_sub_4670 (0x140104670) — building-removal eligibility.
// self+0x38 -> removalDesc; checks flag desc[0x41]; if entity present and its
// owner (+0x28) differs from desc[0x38], finds the DestroyComponent
// (FUN_140005780) and the MapComponent variant (FUN_140003ea0), then reads the
// state byte (+0x52 / +0x50 fallback) and returns 1 when state is not 3/4/5.
// Signature: ulonglong fn(longlong param_1, InternalBasePtr* param_2)
// ============================================================
uint64_t PlayerPath_Sub_4670(uintptr_t param_1, void* param_2) {
    uintptr_t desc = *reinterpret_cast<uintptr_t*>(param_1 + 0x38);
    void* result = nullptr;  // pIVar2

    if ((*reinterpret_cast<uint8_t*>(desc + 0x41) != 0) &&
        (*reinterpret_cast<int64_t*>(param_2) != 0)) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_2);
        desc = *reinterpret_cast<uintptr_t*>(param_1 + 0x38);
        uintptr_t entity = *reinterpret_cast<uintptr_t*>(param_2);
        if (*reinterpret_cast<int64_t*>(entity + 0x28) != *reinterpret_cast<int64_t*>(desc + 0x38)) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_2);
            uint64_t ibp18[2] = {0};
            PlayerPath_FindComponent_5780(*reinterpret_cast<uintptr_t*>(param_2), ibp18);

            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_2);
            uint64_t ibpRes = 0;
            PlayerPath_FindComponent_3ea0(*reinterpret_cast<uintptr_t*>(param_2), &ibpRes);

            if (ibp18[0] != 0) {
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&ibpRes);
                char cVar1 = *reinterpret_cast<char*>(ibpRes + 0x52);
                if (cVar1 < 0) cVar1 = *reinterpret_cast<char*>(ibpRes + 0x50);
                if (cVar1 != 3) {
                    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&ibpRes);
                    cVar1 = *reinterpret_cast<char*>(ibpRes + 0x52);
                    if (cVar1 < 0) cVar1 = *reinterpret_cast<char*>(ibpRes + 0x50);
                    if (cVar1 != 4) {
                        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&ibpRes);
                        cVar1 = *reinterpret_cast<char*>(ibpRes + 0x52);
                        if (cVar1 < 0) cVar1 = *reinterpret_cast<char*>(ibpRes + 0x50);
                        if (cVar1 != 5) {
                            return 1;
                        }
                    }
                }
            }
        }
    }
    return reinterpret_cast<uint64_t>(result) & 0xffffffffffffff00ull;
}

// ============================================================
// RemovalTool_sub_4770 (0x140104770) — road/path-removal eligibility.
// self+0x38 -> removalDesc; checks flag desc[0x42]; owner-compare; if the
// entity's DestroyComponent (FUN_140005780) is present, runs FUN_14011e710 on
// it and returns 1 when that says removable (or owner matches).
// Signature: undefined8 fn(longlong param_1, InternalBasePtr* param_2)
// ============================================================
uint64_t PlayerPath_Sub_4770(uintptr_t param_1, void* param_2) {
    uintptr_t desc = *reinterpret_cast<uintptr_t*>(param_1 + 0x38);
    void* ret = nullptr;  // pIVar1

    if ((*reinterpret_cast<uint8_t*>(desc + 0x42) != 0) &&
        (*reinterpret_cast<int64_t*>(param_2) != 0)) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_2);
        desc = *reinterpret_cast<uintptr_t*>(param_1 + 0x38);
        uintptr_t entity = *reinterpret_cast<uintptr_t*>(param_2);
        if (*reinterpret_cast<int64_t*>(entity + 0x28) == *reinterpret_cast<int64_t*>(desc + 0x38)) {
            return 1;  // LAB_1401047aa
        }
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_2);
        uint64_t comp[8] = {0};
        PlayerPath_FindComponent_5780(*reinterpret_cast<uintptr_t*>(param_2), comp);
        if (comp[0] != 0) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_2);
            uint64_t comp2[8] = {0};
            PlayerPath_FindComponent_5780(*reinterpret_cast<uintptr_t*>(param_2), comp2);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(comp2);
            uint64_t r = PlayerPath_Sub_11e710(comp2[0]);
            if ((char)r != 0) return 1;  // goto LAB_1401047aa
            ret = reinterpret_cast<void*>(r);
        }
    }
    return reinterpret_cast<uint64_t>(ret) & 0xffffffffffffff00ull;
}

// ============================================================
// RemovalTool_sub_48a0 (0x1401048a0) — crop/field-area removability check.
// Finds the crop description component (DAT_1401c7430) on self+0x38; if present,
// the desc flag (+0x20) set, and the target entity (param_2) belongs to this
// field (its area-self component FUN_1400052c0 matches a slot at desc+0x28), it
// tests the single field cell (param_3,param_4) via CharacterComponent_impl_f5d0
// (0x14009f5d0). Returns 1 when the cell passes.
//
// Faithful 1:1 port. DAT_140172208 = 100.0f. The original's string-cleanup
// branch operates on zero-initialized locals; reproduced as null-safe frees.
// Signature: ulonglong fn(longlong p1, InternalBasePtr* p2, int p3, int p4)
// ============================================================
uint64_t PlayerPath_Sub_48a0(uintptr_t param_1, void* param_2, int param_3, int param_4) {
    EnsureTypeIds();
    const float kHundred = 100.0f;  // DAT_140172208

    // Ghidra's 'g_pGameWorldSingleton' is the GameWorld base pointer; our extern
    // is void** holding the slot, so deref once: *g_pGameWorldSingleton.
    int* local_78 = *reinterpret_cast<int**>((uintptr_t)*g_pGameWorldSingleton + 0xf8);

    // Scan self+0x38 entity's component list for the crop-desc type id.
    void* matched = nullptr;  // pvVar9
    {
        uintptr_t selfEnt = *reinterpret_cast<uintptr_t*>(param_1 + 0x38);
        if (s_pTid_7430 && *reinterpret_cast<int64_t*>(selfEnt + 0x38) != 0) {
            uintptr_t lst = *reinterpret_cast<uintptr_t*>(selfEnt + 0x38);
            int n = *reinterpret_cast<int*>(lst + 0x30);
            int idx = 0;
            uintptr_t cur = 0;
            while (idx < n) {
                void* comp = *reinterpret_cast<void**>(cur + *reinterpret_cast<int64_t*>(lst + 0x28));
                uint32_t t = (*reinterpret_cast<uint32_t(**)(void*)>(*reinterpret_cast<uintptr_t*>(comp) + 0x10))(comp);
                if (t == *s_pTid_7430) {
                    void* c = *reinterpret_cast<void**>(*reinterpret_cast<int64_t*>(lst + 0x28) + (int64_t)idx * 8);
                    if (fn_IsDerivedFrom && fn_IsDerivedFrom(c, *s_pTid_7430)) matched = c;
                    break;
                }
                idx++;
                cur += 8;
            }
        }
    }

    uintptr_t desc = *reinterpret_cast<uintptr_t*>(param_1 + 0x38);
    void* retFail = nullptr;  // pIVar4, masked low byte on fail

    if ((*reinterpret_cast<uint8_t*>(desc + 0x20) != 0) && (matched != nullptr) &&
        (*reinterpret_cast<int64_t*>(param_2) != 0)) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_2);
        uint64_t ibp88[4] = {0};
        FindComponent_AreaSelf(*reinterpret_cast<uintptr_t*>(param_2), ibp88);
        if (ibp88[0] != 0) {
            desc = *reinterpret_cast<uintptr_t*>(param_1 + 0x38);
            int compCount = *reinterpret_cast<int*>(desc + 0x30);
            int iVar12 = compCount;  // captured before the loop (decompile semantics)

            // Determine whether param_2's entity belongs to this field.
            bool atWorkBlock = false;
            {
                int idx = 0;
                uintptr_t cur = 0;
                while (idx < compCount) {
                    int64_t lv = *reinterpret_cast<int64_t*>(cur + *reinterpret_cast<int64_t*>(desc + 0x28));
                    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_2);
                    if (lv == *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(param_2) + 0x28)) {
                        atWorkBlock = true;  // LAB_140104a0f
                        break;
                    }
                    idx++;
                    desc = *reinterpret_cast<uintptr_t*>(param_1 + 0x38);
                    compCount = *reinterpret_cast<int*>(desc + 0x30);
                    cur += 8;
                }
            }
            if (!atWorkBlock && iVar12 == 0) atWorkBlock = true;  // 'if (iVar12 == 0) goto LAB_140104a0f'

            bool ok = false;
            if (atWorkBlock) {
                char cVar1 = *reinterpret_cast<char*>((uintptr_t)matched + 0x24);   // override flag
                uint32_t passMask = *reinterpret_cast<uint32_t*>((uintptr_t)matched + 0x20);

                // The y/x loops each run exactly once: y in [param_4,param_4],
                // x in [param_3,param_3].
                bool all = true;
                for (int y = param_4; y < param_4 + 1; y++) {
                    for (int x = param_3; x < param_3 + 1; x++) {
                        // param_9 (string '.' check) is always false: local_90==0.
                        bool cell = PlayerPath_CharComp_f5d0(
                            local_78, x, y, passMask, 0.0f, kHundred, 0.0f, cVar1, '\0', 0);
                        if (!cell) { all = false; break; }
                    }
                    if (!all) break;
                }
                ok = all;
            }

            // Original cleanup branch (string state) runs on zero locals.
            // local_a0 / local_88 / local_90 are all 0 -> null-safe.
            if (fn_MemFreeStatic) fn_MemFreeStatic(nullptr);

            if (ok) return 1;
        }
    }
    return reinterpret_cast<uint64_t>(retFail) & 0xffffffffffffff00ull;
}
