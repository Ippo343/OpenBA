// code/Game/Game/gameupdateloop_load.cpp
// GameUpdateLoop::Load (exe 0x140096b00) — the in-game WORLD BUILD (M5).
//
// This is the Stage-C cliff from project_newgame_path_roadmap: the loader chain
// reaches GameUpdateLoop::Load on "New Game → OK", and this function constructs
// the ~28 in-game subsystems that GameUpdateLoop::{Startup,Update,Draw} operate on.
// Until it ran, m_game (+0xF8) was null and those methods held behind a guard.
//
// FIRST SLICE (milestone 5a): build every subsystem object so m_game is non-null
// and the loop runs. DEFERRED to later slices:
//   - the 8 Object::ExternalList::Find resource loads (Diseases/Profession/Material/
//     Help/Environment/MusicList/SkyMesh/StringTables) — left null; consumers guarded.
//     (exe-typed ones are factory-cascade landmines; Runtime-typed ones are cheap to
//     re-add once the skeleton is proven.) See project_gameupdateloop_load_map.
//   - the SkyMesh MeshInstance (+0x108, step 28) — needs vtable 0x14015e240 registered.
//   - the deep worldgen (step 31): FUN_1400b0230 / CreateNaturalResource_impl_ef20 /
//     FUN_1400a3300 — real terrain + citizens; milestone 5b.
//
// All subsystem ctor bodies below were verified against Ghidra disassembly
// (m5-load-ctor-drafts workflow, 2026-05-30) — exact field offsets in comments.

#include "GameUpdateLoop.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>

// ============================================================
// Externs from sibling reimpl TUs (already registered/defined)
// ============================================================
extern "C" void* Reimpl_Geometry_Init(void* self);            // FUN_1400b00e0 (serialize_arrays.cpp)
extern "C" void  Reimpl_Array_Resize_Stride04(void*, int);    // FUN_1400021d0 (serialize_resize_helpers.cpp)
extern "C" void  WorldGen_BuildTerrain(void* wg, void* gen, int sizeIdx, uint32_t seed); // worldgen.cpp (M5 5b)

// ============================================================
// Load-specific Runtime resolves not yet in engine_funcs.h
// ============================================================
typedef void* (*fn_Random_Seeded_Ctor_t)(void* self, uint32_t seed);
static fn_Random_Seeded_Ctor_t s_fn_Random_Seeded_Ctor = nullptr;   // ??0Random@System@@QEAA@I@Z
typedef void* (*fn_Picking_Scene_Ctor_t)(void* self);
static fn_Picking_Scene_Ctor_t s_fn_Picking_Scene_Ctor = nullptr;   // ??0Scene@Picking@@QEAA@XZ
typedef void* (*fn_UI_Ctor_t)(void* self);
static fn_UI_Ctor_t s_fn_UI_Ctor = nullptr;                         // ??0UserInterface@UI@@QEAA@XZ
typedef void* (*fn_AnimMgr_Ctor_t)(void* self);
static fn_AnimMgr_Ctor_t s_fn_AnimMgr_Ctor = nullptr;               // ??0AnimationManager@Graphics@@QEAA@XZ
typedef void* (*fn_SaveLoad_Ctor_t)(void* self);
static fn_SaveLoad_Ctor_t s_fn_SaveLoad_Ctor = nullptr;             // ??0SaveLoad@Save@@QEAA@XZ
typedef uint32_t (*fn_Random_RandFull_t)(void* self);
static fn_Random_RandFull_t s_fn_Random_RandFull = nullptr;         // ?Rand@Random@System@@QEAAIXZ (no-arg)

static bool s_loadResolved = false;
static void LoadEnsureInit() {
    if (s_loadResolved) return;
    s_loadResolved = true;
    s_fn_Random_Seeded_Ctor = (fn_Random_Seeded_Ctor_t)ResolveExport("??0Random@System@@QEAA@I@Z");
    s_fn_Picking_Scene_Ctor = (fn_Picking_Scene_Ctor_t)ResolveExport("??0Scene@Picking@@QEAA@XZ");
    s_fn_UI_Ctor            = (fn_UI_Ctor_t)ResolveExport("??0UserInterface@UI@@QEAA@XZ");
    s_fn_AnimMgr_Ctor       = (fn_AnimMgr_Ctor_t)ResolveExport("??0AnimationManager@Graphics@@QEAA@XZ");
    s_fn_SaveLoad_Ctor      = (fn_SaveLoad_Ctor_t)ResolveExport("??0SaveLoad@Save@@QEAA@XZ");
    s_fn_Random_RandFull    = (fn_Random_RandFull_t)ResolveExport("?Rand@Random@System@@QEAAIXZ");
    fprintf(stderr, "[GUL::Load] resolves: RandSeed=%p Picking=%p UI=%p Anim=%p SaveLoad=%p RandFull=%p\n",
            (void*)s_fn_Random_Seeded_Ctor, (void*)s_fn_Picking_Scene_Ctor, (void*)s_fn_UI_Ctor,
            (void*)s_fn_AnimMgr_Ctor, (void*)s_fn_SaveLoad_Ctor, (void*)s_fn_Random_RandFull);
    fflush(stderr);
}

// ============================================================
// Array-resize leaf helpers (verified, m5-load-ctor-drafts)
// Header layout: +0x00 ptr, +0x08 int count, +0x0C int capacity.
// NB: the stride-8 ZERO-FILL variant (FUN_1400032e0) already exists as
// fn_ExeArrayResize (engine_funcs.h) — reused, not redefined here.
// ============================================================

// FUN_1400191f0 — stride-8 pointer-array resize; copies existing, does NOT zero new slots.
static void* Reimpl_FUN_1400191f0(void* hdr, int n) {
    uint8_t* p = (uint8_t*)hdr;
    int32_t oldCount = *(int32_t*)(p + 0x08);
    if (n == oldCount) return hdr;
    void* oldPtr = *(void**)(p + 0x00);
    *(int32_t*)(p + 0x08) = n;
    if (*(int32_t*)(p + 0x0C) < n) {
        *(int32_t*)(p + 0x0C) = n;
        if (n == oldCount + 1 && oldCount != 0) *(int32_t*)(p + 0x0C) = n * 3;
        int32_t cap = *(int32_t*)(p + 0x0C);
        *(void**)(p + 0x00) = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)cap << 3, 8);
    }
    int start = 0;
    if (oldPtr == *(void**)(p + 0x00)) start = oldCount;
    void** dst = *(void***)(p + 0x00);
    void** src = (void**)oldPtr;
    for (long long i = start; i < n; ++i) if (i < oldCount) dst[i] = src[i];
    if (oldPtr != *(void**)(p + 0x00) && oldPtr) fn_MemFreeStatic(oldPtr);
    return hdr;
}

// FUN_14002e000 — stride-0x1c (28-byte) struct-array resize; copies existing, zero-fills new.
static void* Reimpl_FUN_14002e000(void* hdr, int n) {
    uint8_t* p = (uint8_t*)hdr;
    int32_t oldCount = *(int32_t*)(p + 0x08);
    if (n == oldCount) return hdr;
    void* oldPtr = *(void**)(p + 0x00);
    *(int32_t*)(p + 0x08) = n;
    if (*(int32_t*)(p + 0x0C) < n) {
        *(int32_t*)(p + 0x0C) = n;
        if (n == oldCount + 1 && oldCount != 0) *(int32_t*)(p + 0x0C) = n * 3;
        int32_t cap = *(int32_t*)(p + 0x0C);
        *(void**)(p + 0x00) = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)cap * 0x1c, 4);
    }
    int start = 0;
    if (oldPtr == *(void**)(p + 0x00)) start = oldCount;
    uint8_t* dstBase = *(uint8_t**)(p + 0x00);
    uint8_t* srcBase = (uint8_t*)oldPtr;
    for (long long i = start; i < n; ++i) {
        uint8_t* d = dstBase + i * 0x1c;
        if (i < oldCount) {
            uint8_t* s = srcBase + i * 0x1c;
            for (int b = 0; b < 0x18; b += 4) *(uint32_t*)(d + b) = *(uint32_t*)(s + b);
            *(uint8_t*)(d + 0x18) = *(uint8_t*)(s + 0x18);
        } else {
            *(uint64_t*)(d + 0x00) = 0; *(uint64_t*)(d + 0x08) = 0;
            *(uint64_t*)(d + 0x10) = 0; *(uint8_t*)(d + 0x18) = 0;
        }
    }
    if (oldPtr != *(void**)(p + 0x00) && oldPtr) fn_MemFreeStatic(oldPtr);
    return hdr;
}

// helper @ 0x14000f9e0 — deep-copy of an inner stride-4 (uint32) array header (16 bytes).
static void* Reimpl_TradeComponent_helper_f9e0(void* dstElem, void* srcElem) {
    if (dstElem == srcElem) return dstElem;
    uint8_t* d = (uint8_t*)dstElem;
    uint8_t* s = (uint8_t*)srcElem;
    void* old = *(void**)(d + 0x00);
    if (old) fn_MemFreeStatic(old);
    *(int32_t*)(d + 0x08) = 0;
    *(void**)(d + 0x00) = 0;
    int32_t cnt = *(int32_t*)(s + 0x08);
    *(int32_t*)(d + 0x08) = cnt;
    if (cnt > 0) {
        *(int32_t*)(d + 0x0C) = cnt;
        *(void**)(d + 0x00) = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)cnt << 2, 4);
    }
    int32_t newCnt = *(int32_t*)(d + 0x08);
    if (newCnt > 0) {
        int32_t* dp = *(int32_t**)(d + 0x00);
        int32_t* sp = *(int32_t**)(s + 0x00);
        for (int32_t i = 0; i < newCnt; ++i) dp[i] = sp[i];
    }
    return dstElem;
}

// FUN_140017640 (TradeComponent_impl_f760_sub_7640) — stride-0x10 array of inner-array headers.
static void* Reimpl_FUN_140017640(void* hdr, int n) {
    uint8_t* p = (uint8_t*)hdr;
    int32_t oldCount = *(int32_t*)(p + 0x08);
    if (n == oldCount) return hdr;
    uint8_t* oldPtr = *(uint8_t**)(p + 0x00);
    *(int32_t*)(p + 0x08) = n;
    if (*(int32_t*)(p + 0x0C) < n) {
        *(int32_t*)(p + 0x0C) = n;
        if (n == oldCount + 1 && oldCount != 0) *(int32_t*)(p + 0x0C) = n * 3;
        int32_t cap = *(int32_t*)(p + 0x0C);
        *(void**)(p + 0x00) = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)cap << 4, 8);
    }
    int start = 0;
    if (oldPtr == *(uint8_t**)(p + 0x00)) start = oldCount;
    uint8_t* dstBase = *(uint8_t**)(p + 0x00);
    for (long long i = start; i < n; ++i) {
        uint8_t* dstElem = dstBase + i * 0x10;
        *(uint64_t*)(dstElem + 0x00) = 0;
        *(uint64_t*)(dstElem + 0x08) = 0;
        if (i < oldCount) Reimpl_TradeComponent_helper_f9e0(dstElem, oldPtr + i * 0x10);
    }
    if (oldPtr == *(uint8_t**)(p + 0x00)) {
        if (n < oldCount) {
            uint8_t* e = oldPtr + (long long)n * 0x10;
            for (long long k = oldCount - n; k > 0; --k) {
                fn_MemFreeStatic(*(void**)(e + 0x00));
                *(uint64_t*)(e + 0x00) = 0; *(uint64_t*)(e + 0x08) = 0; e += 0x10;
            }
        }
    } else {
        if (oldCount > 0) {
            uint8_t* e = oldPtr;
            for (long long k = oldCount; k > 0; --k) {
                fn_MemFreeStatic(*(void**)(e + 0x00));
                *(uint64_t*)(e + 0x00) = 0; *(uint64_t*)(e + 0x08) = 0; e += 0x10;
            }
        }
        if (oldPtr) fn_MemFreeStatic(oldPtr);
    }
    return hdr;
}

// TradeComponent_impl_f760_sub_77c0 @ 0x1400177c0 — stride-0x20 resize; new slots {0,100,0,0}.
static void* Reimpl_TradeComponent_sub_77c0(void* hdr, int n) {
    uint8_t* p = (uint8_t*)hdr;
    int32_t oldCount = *(int32_t*)(p + 0x08);
    if (n == oldCount) return hdr;
    void* oldPtr = *(void**)(p + 0x00);
    *(int32_t*)(p + 0x08) = n;
    if (*(int32_t*)(p + 0x0C) < n) {
        *(int32_t*)(p + 0x0C) = n;
        if (n == oldCount + 1 && oldCount != 0) *(int32_t*)(p + 0x0C) = n * 3;
        int32_t cap = *(int32_t*)(p + 0x0C);
        *(void**)(p + 0x00) = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)cap << 5, 8);
    }
    int start = 0;
    if (oldPtr == *(void**)(p + 0x00)) start = oldCount;
    uint8_t* dstBase = *(uint8_t**)(p + 0x00);
    uint8_t* srcBase = (uint8_t*)oldPtr;
    for (long long i = start; i < n; ++i) {
        uint8_t* d = dstBase + i * 0x20;
        if (i < oldCount) {
            uint8_t* s = srcBase + i * 0x20;
            *(uint64_t*)(d + 0x00) = *(uint64_t*)(s + 0x00);
            *(uint64_t*)(d + 0x08) = *(uint64_t*)(s + 0x08);
            *(uint64_t*)(d + 0x10) = *(uint64_t*)(s + 0x10);
            *(uint64_t*)(d + 0x18) = *(uint64_t*)(s + 0x18);
        } else {
            *(uint64_t*)(d + 0x00) = 0;
            *(uint64_t*)(d + 0x08) = 100;   // +0x08 default = 100
            *(uint32_t*)(d + 0x10) = 0;
            *(uint64_t*)(d + 0x18) = 0;
        }
    }
    if (oldPtr != *(void**)(p + 0x00) && oldPtr) fn_MemFreeStatic(oldPtr);
    return hdr;
}

// FUN_14000f090 — stride-0x10 POD resize; copies 2x u64, zero-fills new.
static void* Reimpl_FUN_14000f090(void* hdr, int n) {
    uint8_t* p = (uint8_t*)hdr;
    int32_t oldCount = *(int32_t*)(p + 0x08);
    if (n == oldCount) return hdr;
    void* oldPtr = *(void**)(p + 0x00);
    *(int32_t*)(p + 0x08) = n;
    if (*(int32_t*)(p + 0x0C) < n) {
        *(int32_t*)(p + 0x0C) = n;
        if (n == oldCount + 1 && oldCount != 0) *(int32_t*)(p + 0x0C) = n * 3;
        int32_t cap = *(int32_t*)(p + 0x0C);
        *(void**)(p + 0x00) = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)cap << 4, 8);
    }
    int start = 0;
    if (oldPtr == *(void**)(p + 0x00)) start = oldCount;
    uint8_t* dstBase = *(uint8_t**)(p + 0x00);
    uint8_t* srcBase = (uint8_t*)oldPtr;
    for (long long i = start; i < n; ++i) {
        uint8_t* d = dstBase + i * 0x10;
        if (i < oldCount) {
            uint8_t* s = srcBase + i * 0x10;
            *(uint64_t*)(d + 0x00) = *(uint64_t*)(s + 0x00);
            *(uint64_t*)(d + 0x08) = *(uint64_t*)(s + 0x08);
        } else {
            *(uint64_t*)(d + 0x00) = 0; *(uint64_t*)(d + 0x08) = 0;
        }
    }
    if (oldPtr != *(void**)(p + 0x00) && oldPtr) fn_MemFreeStatic(oldPtr);
    return hdr;
}

// ============================================================
// Subsystem constructors (verified bodies)
// ============================================================

// FUN_1400983f0 — PlayerController/Camera lookAt-transform builder (helper_83f0).
// Exposed (extern "C") + registered at 0x1400983f0 so gameupdateloop.cpp's
// fn_PlayerController_CalcTransform (resolved via ResolveAddr) finds it in standalone.
extern "C" void* Reimpl_FUN_1400983f0(void* self, void* outTransform) {
    uint8_t* p = (uint8_t*)self;
    if (!fn_Math_Vector3_FromYawPitch || !fn_Math_Transform_FromLookAt) return outTransform;
    float yaw = *(float*)(p + 0x28);
    float pitch = *(float*)(p + 0x2c);
    float distance = *(float*)(p + 0x30);
    float dir[4] = {};
    fn_Math_Vector3_FromYawPitch(dir, &yaw, &pitch);
    float* pos = (float*)(p + 0x1c);
    float eye[4] = {};
    eye[0] = pos[0] - dir[0] * distance;
    eye[1] = pos[1] - dir[1] * distance;
    eye[2] = pos[2] - dir[2] * distance;
    uint8_t lookAt[32] = {};
    void* res = fn_Math_Transform_FromLookAt(lookAt, eye, pos);
    if (!res) res = lookAt;
    const uint32_t* src = (const uint32_t*)res;
    uint32_t* dstSelf = (uint32_t*)p;
    uint32_t* dstOut = (uint32_t*)outTransform;
    for (int i = 0; i < 7; ++i) { dstSelf[i] = src[i]; dstOut[i] = dstSelf[i]; }
    return outTransform;
}

// FUN_1400985b0 — PlayerController camera-system ctor (+0xE0, 0x10c).
static void* Reimpl_FUN_1400985b0(void* selfMem, void* frustum, float yaw, float pitch, float distance) {
    uint8_t* p = (uint8_t*)selfMem;
    if (!fn_Math_Transform_Ctor || !fn_Transform_AsTransform3 || !fn_Graphics_Camera_Ctor)
        return selfMem;
    fn_Math_Transform_Ctor(selfMem);
    *(uint64_t*)(p + 0x1c) = 0;
    *(uint32_t*)(p + 0x24) = 0;
    *(float*)(p + 0x28) = yaw;
    *(float*)(p + 0x2c) = pitch;
    *(float*)(p + 0x30) = distance;
    uint8_t localTransform[32] = {};
    void* lt = Reimpl_FUN_1400983f0(selfMem, localTransform);
    if (!lt) lt = localTransform;
    uint8_t t3[64] = {};
    void* t3p = fn_Transform_AsTransform3(lt, t3);
    if (!t3p) t3p = t3;
    fn_Graphics_Camera_Ctor((void*)(p + 0x34), frustum, t3p, true);
    return selfMem;
}

// FUN_1400c3f90 — EntityList ctor (+0xE8, 0xe8). memset + the 5 non-zero defaults.
static void* Reimpl_FUN_1400c3f90(void* selfMem) {
    uint8_t* p = (uint8_t*)selfMem;
    memset(p, 0, 0xe8);                       // zeroes header + the 5x16 array @+0x18 + two {ptr,count} @+0x68/+0x78
    *(int32_t*)(p + 0xa8) = 0x32;             // 50
    *(int32_t*)(p + 0xac) = 0xf;              // 15
    *(int32_t*)(p + 0xb0) = 8;
    *(int32_t*)(p + 0xb4) = 5;
    *(uint64_t*)(p + 0xb8) = 2;
    return selfMem;
}

// TradeComponent_impl_0a30 @ 0x140090a30 — town/trade manager ctor (+0xF0, 0x1c0).
static void* Reimpl_TradeComponent_impl_0a30(void* selfMem) {
    uint8_t* p = (uint8_t*)selfMem;
    for (int i = 0; i < 0x1b4; ++i) p[i] = 0;
    Reimpl_TradeComponent_sub_77c0(p + 0x170, 0x15);
    uint8_t* prices = *(uint8_t**)(p + 0x170);
    if (prices) {
        static const int32_t kDefaultPrices[21] = {
            200, 200, 200, 200, 50, 5000, 100, 50, 500, 200, 200,
            1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000
        };
        for (int i = 0; i < 21; ++i)
            *(int32_t*)(prices + (size_t)i * 0x20 + 0x8) = kDefaultPrices[i];
    }
    Reimpl_FUN_14000f090(p + 0x180, 2);
    Reimpl_FUN_140017640(p + 0x150, 0x47);
    *(int32_t*)(p + 0x1a8) = 0;
    void* sub = fn_MemAlloc(g_pMemAllocSingleton, 0x28, 4);
    if (sub) {
        uint8_t* s = (uint8_t*)sub;
        *(void**)(s + 0x00) = nullptr;
        *(void**)(s + 0x08) = nullptr;
        *(int32_t*)(s + 0x10) = -1;
        *(void**)(s + 0x18) = nullptr;
        *(int32_t*)(s + 0x20) = 0;
    }
    *(void**)(p + 0x1b8) = sub;
    return selfMem;
}

// FUN_14009ee70 — REAL Game ctor (+0xF8, 0xf8). Embedded Picking::Scene @ +8.
static void* Reimpl_FUN_14009ee70(void* selfMem) {
    uint8_t* p = (uint8_t*)selfMem;
    if (s_fn_Picking_Scene_Ctor) s_fn_Picking_Scene_Ctor((void*)(p + 8));
    memset(p + 0x68, 0, 0xf8 - 0x68);         // zero +0x68..+0xf8 (preserves the embedded Scene at +8)
    *(uint8_t*)(p + 0xa2) = 1;
    *(uint64_t*)(p + 0xd0) = 8;
    return selfMem;
}

// MainMenuDialog_helper_9180 @ 0x1400f9180 — Tutorial-ish ctor (+0x118, 0x138).
static void* Reimpl_FUN_1400f9180(void* selfMem) {
    uint8_t* p = (uint8_t*)selfMem;
    memset(p, 0, 0x138);                      // zeroes header + the 15x16 array @+0x30
    *(int32_t*)(p + 0x08) = 1;
    return selfMem;
}

// FUN_140093f60 — in-game Weather SIMULATION ctor (+0x130, 0x78). PerlinNoise-driven.
static void* Reimpl_FUN_140093f60(void* selfMem) {
    uint8_t* p = (uint8_t*)selfMem;
    *(uint64_t*)(p + 0x00) = 0;
    *(uint64_t*)(p + 0x08) = 0;
    *(uint32_t*)(p + 0x10) = 0;
    *(uint32_t*)(p + 0x14) = 4;               // default count = 4
    *(uint32_t*)(p + 0x18) = 0;
    *(uint64_t*)(p + 0x1c) = 0;
    *(uint64_t*)(p + 0x24) = 0;
    *(uint64_t*)(p + 0x2c) = 0;
    uint8_t rng1[2520];
    uint8_t rng2[2520];
    uint32_t seed1 = 0, seed2 = 0;
    if (fn_Random_Default_Ctor) {
        fn_Random_Default_Ctor(rng1);
        seed1 = s_fn_Random_RandFull ? s_fn_Random_RandFull(rng1)
                : (fn_Random_Rand ? fn_Random_Rand(rng1, 0xffffffffu) : 0u);
        fn_Random_Default_Ctor(rng2);
        seed2 = s_fn_Random_RandFull ? s_fn_Random_RandFull(rng2)
                : (fn_Random_Rand ? fn_Random_Rand(rng2, 0xffffffffu) : 0u);
    }
    if (fn_PerlinNoise_Ctor)
        fn_PerlinNoise_Ctor((void*)(p + 0x34), 0.25f, 4.0f, (int)seed2, (int)seed1);
    *(uint64_t*)(p + 0x44) = 0;
    *(uint32_t*)(p + 0x4c) = 0xffffffffu;
    *(uint64_t*)(p + 0x50) = 0;
    *(uint64_t*)(p + 0x58) = 0;
    *(uint64_t*)(p + 0x60) = 0;
    *(uint64_t*)(p + 0x68) = 0;
    *(uint64_t*)(p + 0x70) = 0;
    return selfMem;
}

// FUN_140093e30 — weather env-config helper. (envRes may be null on the deferred path.)
static void* Reimpl_FUN_140093e30(void* weatherObj, void* envRes, void* climateGen) {
    uint8_t* p = (uint8_t*)weatherObj;
    *(void**)(p + 0x00) = envRes;
    *(void**)(p + 0x08) = climateGen;
    if (envRes) {
        int count = *(int32_t*)((uint8_t*)envRes + 0x30);
        Reimpl_FUN_1400191f0((void*)(p + 0x50), count);
        fn_ExeArrayResize((void*)(p + 0x60), count);   // FUN_1400032e0 (stride8 zero-fill)
        int n0 = *(int32_t*)(p + 0x58);
        void** arr0 = *(void***)(p + 0x50);
        for (int i = 0; i < n0; ++i) arr0[i] = nullptr;
    }
    if (climateGen) {
        float v = *(float*)((uint8_t*)climateGen + 0x34);
        *(float*)(p + 0x2c) = v;
        *(float*)(p + 0x30) = v;
    }
    return weatherObj;
}

// FUN_1400182b0 — SkyMesh transform / quadtree-LOD init (+0x88, 0x60).
static void* Reimpl_FUN_1400182b0(void* selfMem, float param2) {
    uint8_t* p = (uint8_t*)selfMem;
    memset(p, 0, 0x60);
    *(float*)(p + 0x00) = param2;
    *(int32_t*)(p + 0x0C) = 8;
    float scale = param2 * 0.5f;
    *(float*)(p + 0x04) = scale;
    *(float*)(p + 0x08) = scale;
    Reimpl_Array_Resize_Stride04((void*)(p + 0x40), 8);
    {
        int32_t* lod = *(int32_t**)(p + 0x40);
        if (lod) {
            lod[0] = 0x0; lod[1] = 0x1; lod[2] = 0x5; lod[3] = 0x15;
            lod[4] = 0x55; lod[5] = 0x155; lod[6] = 0x555; lod[7] = 0x1555;
        }
    }
    Reimpl_Array_Resize_Stride04((void*)(p + 0x30), 0x5555);
    Reimpl_Array_Resize_Stride04((void*)(p + 0x50), 0x5555);
    {
        int32_t* aFF = *(int32_t**)(p + 0x30);
        int32_t* aZ = *(int32_t**)(p + 0x50);
        if (aFF && aZ) {
            for (int idx = 0; idx < 0x5555; ++idx) { aFF[idx] = (int32_t)0xffffffff; aZ[idx] = 0; }
        }
    }
    return selfMem;
}

// FUN_140098660 — PlayerController::UpdateCamera (builds projection frustum on camera+0x34).
// Exposed + registered at 0x140098660 (gameupdateloop.cpp Draw + Load step 33 call it).
extern "C" void* Reimpl_FUN_140098660(void* camera, float fovY, float nearZ, float farZ) {
    int width = 1920, height = 1080;          // default 16:9 if viewport singleton unavailable
    if (g_pSingletonExref_151a80 && *g_pSingletonExref_151a80) {
        uint8_t* vp = *(uint8_t**)(*g_pSingletonExref_151a80);
        if (vp) { width = *(int32_t*)(vp + 4); height = *(int32_t*)(vp + 8); }
    }
    const float kAspectThresh = 1.7777778f, kHalf = 0.5f, kDeg2Rad = 0.017453292f,
                kTwo = 2.0f, kRad2Deg = 57.29578f;
    float aspect = (height != 0) ? (float)width / (float)height : kAspectThresh;
    float fovX = aspect * fovY;
    if (aspect > kAspectThresh) {
        float t = tanf(fovY * kAspectThresh * kHalf * kDeg2Rad);
        float den = (height * kAspectThresh * kHalf) / t;
        float halfW = (float)(width / 2);
        fovX = atanf(halfW / den) * kTwo * kRad2Deg;
    }
    uint8_t frustum[0xC0];
    void* fr = fn_Volume_Frustum_Ctor ? fn_Volume_Frustum_Ctor(frustum, &nearZ, &farZ, &fovX, &fovY) : nullptr;
    if (fn_Graphics_Camera_SetFrustum)
        fn_Graphics_Camera_SetFrustum((void*)((uint8_t*)camera + 0x34), fr ? fr : frustum);
    return camera;
}

// FUN_1401025c0 (helper_25c0) — in-game EntityUI-style ctor (+0x120, 0xb0).
static void* Reimpl_FUN_1401025c0(void* selfMem) {
    uint8_t* p = (uint8_t*)selfMem;
    fn_InternalBasePtr_Ctor((void*)(p + 0x00), nullptr);
    *(int64_t*)(p + 0x10) = 0;
    *(int32_t*)(p + 0x18) = 0;
    *(int64_t*)(p + 0x20) = 0;
    fn_InternalBasePtr_Ctor((void*)(p + 0x28), nullptr);
    *(uint8_t*)(p + 0x30) = 0;
    *(int32_t*)(p + 0x34) = 1;
    *(uint8_t*)(p + 0x38) = 0;
    *(int64_t*)(p + 0x3c) = 0;
    *(float*)(p + 0x44) = 0.75f;
    *(int64_t*)(p + 0x48) = 0;
    *(int64_t*)(p + 0x50) = 0;
    *(int32_t*)(p + 0x58) = 0;
    *(int64_t*)(p + 0x60) = 0;
    *(int32_t*)(p + 0x68) = 0;
    *(int64_t*)(p + 0x70) = 0;
    *(int64_t*)(p + 0x78) = 0;
    *(uint8_t*)(p + 0x80) = 0;
    *(int64_t*)(p + 0x88) = 0;
    *(int64_t*)(p + 0x90) = 0;
    *(int64_t*)(p + 0x98) = -1;
    *(int32_t*)(p + 0xa0) = -1;
    *(uint8_t*)(p + 0xa4) = 0;
    Reimpl_FUN_14002e000((void*)(p + 0x70), 5);
    return selfMem;
}

// ============================================================
// Small allocation helper
// ============================================================
static inline void* AllocSub(uint64_t size, int align) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    return fn_MemAlloc(g_pMemAllocSingleton, size, align);
}

// ============================================================
// GameUpdateLoop::Load (0x140096b00) — orchestration
// ============================================================
extern "C" void GameUpdateLoop_Load(void* loopVoid) {
    LoadEnsureInit();
    uint8_t* p = (uint8_t*)loopVoid;

    fprintf(stderr, "[GUL::Load] ENTER self=%p seed=%d mapSize=%d climateGen=%p terrainGen=%p saved=%p\n",
            loopVoid, *(int32_t*)(p + 0x30), *(int32_t*)(p + 0x34),
            *(void**)(p + 0x50), *(void**)(p + 0x48), *(void**)(p + 0x58));
    fflush(stderr);

    // g_pGameWorldSingleton = this loop (read by AddGlobalObject / Audio::Pause at the tail).
    if (g_pGameWorldSingleton) *g_pGameWorldSingleton = loopVoid;

    // Steps 1-4: resource Finds (Diseases/Profession/Material/Help) — DEFERRED (leave null).
    *(void**)(p + 0xA8) = nullptr;  // diseases
    *(void**)(p + 0xB0) = nullptr;  // professions
    *(void**)(p + 0xB8) = nullptr;  // ui material
    *(void**)(p + 0xA0) = nullptr;  // help topics

    // Step 5 — Random (+0xD8, 0x9c4) seeded with mapSizeIndex (+0x30).
    {
        void* mem = AllocSub(0x9c4, 4);
        if (mem && s_fn_Random_Seeded_Ctor) s_fn_Random_Seeded_Ctor(mem, *(uint32_t*)(p + 0x30));
        *(void**)(p + 0xD8) = mem;
    }

    // Step 6 — PlayerController/Camera (+0xE0, 0x10c). Initial frustum (overwritten by step 33).
    {
        int width = 1920, height = 1080;
        if (g_pSingletonExref_151a80 && *g_pSingletonExref_151a80) {
            uint8_t* vp = *(uint8_t**)(*g_pSingletonExref_151a80);
            if (vp) { width = *(int32_t*)(vp + 8); height = *(int32_t*)(vp + 4); }  // step6 reads +8/+4
        }
        float aspect = (height != 0) ? (float)width / (float)height : 1.7777778f;
        void* mem = AllocSub(0x10c, 4);
        if (mem) {
            float nearZ = 2.0f, farZ = 70.0f, fovX = 60.0f, fovY = aspect * 60.0f;
            uint8_t frustum[0xC0] = {};
            if (fn_Volume_Frustum_Ctor) fn_Volume_Frustum_Ctor(frustum, &nearZ, &farZ, &fovX, &fovY);
            Reimpl_FUN_1400985b0(mem, frustum, 0.0f, -40.0f, 25.0f);
        }
        *(void**)(p + 0xE0) = mem;
    }

    // Step 7 — Renderer (+0x90, 0x50).
    {
        void* mem = AllocSub(0x50, 4);
        if (mem && fn_Graphics_Renderer_Ctor) fn_Graphics_Renderer_Ctor(mem);
        *(void**)(p + 0x90) = mem;
    }

    // Step 8 — EntityList (+0xE8, 0xe8).
    {
        void* mem = AllocSub(0xe8, 4);
        if (mem) Reimpl_FUN_1400c3f90(mem);
        *(void**)(p + 0xE8) = mem;
    }

    // Step 9 — UserInterface (+0x98, 0x128).
    {
        void* mem = AllocSub(0x128, 4);
        if (mem && s_fn_UI_Ctor) s_fn_UI_Ctor(mem);
        *(void**)(p + 0x98) = mem;
    }

    // Step 10 — town/trade manager (+0xF0, 0x1c0) — exe ctor 0x140090a30.
    {
        void* mem = AllocSub(0x1c0, 4);
        if (mem) Reimpl_TradeComponent_impl_0a30(mem);
        *(void**)(p + 0xF0) = mem;
    }

    // Step 11 — Game (+0xF8, 0xf8) — CRITICAL: clears the m_game==null Stage-C guards.
    {
        void* mem = AllocSub(0xf8, 8);
        if (mem) Reimpl_FUN_14009ee70(mem);
        *(void**)(p + 0xF8) = mem;
    }

    // Step 12 — Tutorial-ish (+0x118, 0x138).
    {
        void* mem = AllocSub(0x138, 4);
        if (mem) Reimpl_FUN_1400f9180(mem);
        *(void**)(p + 0x118) = mem;
    }

    // Step 13 — EntityUI (+0x120, 0xb0).
    {
        void* mem = AllocSub(0xb0, 4);
        if (mem) Reimpl_FUN_1401025c0(mem);
        *(void**)(p + 0x120) = mem;
    }

    // Step 14 — DayCounter Time struct (+0x128, 0x18): flag=1, Time(+8,0.0), Time(+0x10,3600.0).
    {
        void* mem = AllocSub(0x18, 8);
        if (mem) {
            uint8_t* d = (uint8_t*)mem;
            *(uint32_t*)d = 1;
            if (fn_Time_Ctor_Float) {
                fn_Time_Ctor_Float(d + 8, 0.0f);
                fn_Time_Ctor_Float(d + 0x10, 3600.0f);
            }
        }
        *(void**)(p + 0x128) = mem;
    }

    // Step 15 — Weather sim (+0x130, 0x78).
    {
        void* mem = AllocSub(0x78, 8);
        if (mem) Reimpl_FUN_140093f60(mem);
        *(void**)(p + 0x130) = mem;
    }

    // Step 16 — env config: bind (null env, climateGen) into the weather sim.
    if (*(void**)(p + 0x130))
        Reimpl_FUN_140093e30(*(void**)(p + 0x130), /*envRes*/nullptr, /*climateGen*/*(void**)(p + 0x50));

    // Step 17 — StringTable-pair struct (+0x138, 0x28) zero + flag bytes.
    {
        void* mem = AllocSub(0x28, 4);
        if (mem) {
            uint8_t* d = (uint8_t*)mem;
            memset(d, 0, 0x28);
            *(uint32_t*)(d + 0x10) = 0x01010101;
            *(uint16_t*)(d + 0x14) = 0x0101;
            *(uint8_t*)(d + 0x10) = 0;   // clear low byte (matches exe)
        }
        *(void**)(p + 0x138) = mem;
    }

    // Step 18 — Graphics::Scene (+0x60, 0x78).
    {
        void* mem = AllocSub(0x78, 8);
        if (mem && fn_Graphics_Scene_Ctor) fn_Graphics_Scene_Ctor(mem);
        *(void**)(p + 0x60) = mem;
    }

    // Step 19 — Audio::Scene (+0x78, 0x38).
    {
        void* mem = AllocSub(0x38, 8);
        if (mem && fn_AudioScene_Ctor) fn_AudioScene_Ctor(mem);
        *(void**)(p + 0x78) = mem;
    }

    // Step 20 — Picking::Scene (+0x70, 0x60).
    {
        void* mem = AllocSub(0x60, 8);
        if (mem && s_fn_Picking_Scene_Ctor) s_fn_Picking_Scene_Ctor(mem);
        *(void**)(p + 0x70) = mem;
    }

    // Step 21 — SkyMesh transform / LOD (+0x88, 0x60).
    {
        void* mem = AllocSub(0x60, 8);
        if (mem) Reimpl_FUN_1400182b0(mem, 2048.0f);
        *(void**)(p + 0x88) = mem;
    }

    // Step 22 — AnimationManager (+0x68, 0x9d8).
    {
        void* mem = AllocSub(0x9d8, 8);
        if (mem && s_fn_AnimMgr_Ctor) s_fn_AnimMgr_Ctor(mem);
        *(void**)(p + 0x68) = mem;
    }

    // Step 23 — WorldGeometry (+0x110, 0x230) via the registered Reimpl_Geometry_Init.
    {
        void* mem = AllocSub(0x230, 8);
        if (mem) Reimpl_Geometry_Init(mem);
        *(void**)(p + 0x110) = mem;
    }

    // Step 24 — SunLight (+0x100, 0x108): ctor + Set(dir,col1,col2). No exe vtable override
    // (matches the proven menu path, which uses the Runtime default SunLight vtable).
    {
        void* mem = AllocSub(0x108, 8);
        if (mem && fn_Graphics_SunLight_Ctor) {
            fn_Graphics_SunLight_Ctor(mem);
            // dir = normalize(0.5, 0.33, 1.33) then negate
            float dir[3] = { 0.5f, 0.33f, 1.33f };
            float len = sqrtf(dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2]);
            if (len > 0.0f) { dir[0] = -dir[0]/len; dir[1] = -dir[1]/len; dir[2] = -dir[2]/len; }
            float col1[4] = { 0.8f, 0.8f, 0.8f, 0.85f };
            float col2[4] = { 0.37f, 0.37f, 0.4f, 1.0f };
            if (fn_Graphics_SunLight_Set) fn_Graphics_SunLight_Set(mem, dir, col1, col2);
            *(uint32_t*)((uint8_t*)mem + 0x28) = 0;
        }
        *(void**)(p + 0x100) = mem;
    }
    if (*(void**)(p + 0x110)) *(uint32_t*)(*(uint8_t**)(p + 0x110) + 0x28) = 1;  // worldGeom +0x28 = 1

    // Step 25 — SaveLoad (+0x140, 0x90).
    {
        void* mem = AllocSub(0x90, 4);
        if (mem && s_fn_SaveLoad_Ctor) s_fn_SaveLoad_Ctor(mem);
        *(void**)(p + 0x140) = mem;
    }

    // Step 26 — KeyState/map-input struct (+0x148, 0x38) zero-init.
    {
        void* mem = AllocSub(0x38, 4);
        if (mem) memset(mem, 0, 0x38);
        *(void**)(p + 0x148) = mem;
    }

    // Step 27 — MusicTracks struct (+0x80, 0x18): {idx=-1, res=null (Find deferred), 0}.
    {
        void* mem = AllocSub(0x18, 4);
        if (mem) {
            uint8_t* d = (uint8_t*)mem;
            *(int32_t*)(d + 0x00) = -1;
            *(void**)(d + 0x08) = nullptr;     // music list resource — DEFERRED
            *(uint64_t*)(d + 0x10) = 0;
        }
        *(void**)(p + 0x80) = mem;
    }

    // Step 28 — SkyMesh MeshInstance (+0x108) — DEFERRED (needs vtable 0x14015e240).
    *(void**)(p + 0x108) = nullptr;

    // Step 29 — Seasons/Events StringTable Finds — DEFERRED (leave +0x138 struct slots null).

    // Step 30 — AchievementManager::SetEventHandler — GUARDED (slot 0x1401514f0 not yet
    // registered in standalone, so g_ppAchievementManager is null → skip cleanly).

    // Step 31 — worldgen (new-game branch, savedState==0). M5 5b first-light:
    // build the terrain mesh (base perlin island) + the gameplay tile-grid map
    // arrays. DEFERRED to later 5b slices: TradeComponent trade-map resize,
    // CreateNaturalResource (trees/stone — spawns worker threads), and
    // FUN_1400a3300 start-town + initial-citizen spawn (which also sets the
    // camera position — until then the camera stays at the ctor default, framing
    // the map centre at the origin). See code/Game/Game/worldgen.cpp.
    static const bool kEnableWorldgen = true;
    if (kEnableWorldgen && *(void**)(p + 0x58) == nullptr /*new game*/) {
        void* worldGeom  = *(void**)(p + 0x110);
        void* terrainGen = *(void**)(p + 0x48);
        int   sizeIdx    = *(int32_t*)(p + 0x34);
        uint32_t seed    = *(uint32_t*)(p + 0x30);
        if (worldGeom && terrainGen) {
            WorldGen_BuildTerrain(worldGeom, terrainGen, sizeIdx, seed);

            // Map dimension from the terrain size-table row (sizeRow[0]).
            int32_t* sizeTable = *(int32_t**)((uint8_t*)terrainGen + 0x80);
            uint32_t dim = sizeTable ? *(uint32_t*)((uint8_t*)sizeTable + (size_t)sizeIdx * 0x44) : 0;

            // Gameplay tile-grid arrays at the map struct (+0x148): store dim,
            // allocate two dim*dim arrays (+0x18/+0x28), zero the +0x28 one.
            void* mapStruct = *(void**)(p + 0x148);
            if (mapStruct && dim > 0) {
                *(uint32_t*)((uint8_t*)mapStruct + 4) = dim;
                int n = (int)(dim * dim);
                Reimpl_Array_Resize_Stride04((uint8_t*)mapStruct + 0x28, n);
                Reimpl_Array_Resize_Stride04((uint8_t*)mapStruct + 0x18, n);
                void* arr = *(void**)((uint8_t*)mapStruct + 0x28);
                if (arr) memset(arr, 0, (size_t)n * 4);
            }
        }
    }

    // Step 32 — AchievementInstance — SKIPPED on new game (achievementId == -1).
    *(void**)(p + 0x150) = nullptr;

    // Step 33 — finalize camera + add scene globals + unpause audio channels.
    if (*(void**)(p + 0xE0))
        Reimpl_FUN_140098660(*(void**)(p + 0xE0), 40.0f, 2.0f, 512.0f);
    {
        void* gScene = *(void**)(p + 0x60);   // g_pGameWorld+0x60
        if (gScene && fn_Graphics_Scene_AddGlobalObject) {
            if (*(void**)(p + 0x110)) fn_Graphics_Scene_AddGlobalObject(gScene, *(void**)(p + 0x110)); // WorldGeom
            // M5 5b lighting: add the SunLight so the directional light + ambient
            // light the terrain in the main render pass (Renderer::Draw dispatches the
            // light's vtable+0x50 SunLight::Draw → Context::SetLight + DrawItemList).
            // The crashing shadow pass (SunLight::DrawShadowMap, vtable+0x48) is gated by
            // renderer+0x3c, which GameUpdateLoop::Draw step 6 forces to 0 in standalone
            // ("Shadows: Off") — the shadow draw never runs, so the terrain renders lit
            // without the unwired shadow shader/render-target. Verified against the
            // proven menu path (which adds its SunLight the same way) + the Runtime
            // decompiles (Renderer::Draw 0x180104bb0, SunLight::Draw 0x1801067b0,
            // DrawShadowMap 0x180106bc0, AddScene 0x180105700).
            static const bool kAddSunLight = true;
            if (kAddSunLight && *(void**)(p + 0x100))
                fn_Graphics_Scene_AddGlobalObject(gScene, *(void**)(p + 0x100)); // SunLight
            // SkyMesh deferred → not added
        }
        void* aScene = *(void**)(p + 0x78);   // g_pGameWorld+0x78
        if (aScene && fn_Audio_Scene_Pause) {
            fn_Audio_Scene_Pause(aScene, 4, false);
            fn_Audio_Scene_Pause(aScene, 3, false);
            fn_Audio_Scene_Pause(aScene, 2, false);
        }
    }

    fprintf(stderr, "[GUL::Load] EXIT — world built: game=%p ui=%p entityList=%p scene=%p sun=%p\n",
            *(void**)(p + 0xF8), *(void**)(p + 0x98), *(void**)(p + 0xE8),
            *(void**)(p + 0x60), *(void**)(p + 0x100));
    fflush(stderr);
}

// ============================================================
// STANDALONE_EXE: register the Load-owned exe addresses so gameupdateloop.cpp's
// EnsureInit (which resolves these via ResolveAddr) finds real bodies/values.
// ============================================================
#ifdef STANDALONE_EXE
#include "address_registry.h"

// .rdata float constants the in-game loop dereferences (*g_fXxx). Verified via
// read_memory: 1.0 / 6.0 (audio time scale) / 40.0 / 2.0 / 512.0 (camera params).
static float s_fConst_1_0      = 1.0f;    // 0x14017214c
static float s_fAudioTimeScale = 6.0f;    // 0x14017219c
static float s_fCamParam1      = 40.0f;   // 0x1401721dc
static float s_fCamParam2      = 2.0f;    // 0x140172174
static float s_fCamParam3      = 512.0f;  // 0x140172240

extern "C" void RegisterGameUpdateLoopLoadAddrs() {
    RegisterExeAddr(0x1400983f0, reinterpret_cast<uintptr_t>(&Reimpl_FUN_1400983f0)); // CalcTransform
    RegisterExeAddr(0x140098660, reinterpret_cast<uintptr_t>(&Reimpl_FUN_140098660)); // UpdateCamera
    RegisterExeAddr(0x14017214c, reinterpret_cast<uintptr_t>(&s_fConst_1_0));
    RegisterExeAddr(0x14017219c, reinterpret_cast<uintptr_t>(&s_fAudioTimeScale));
    RegisterExeAddr(0x1401721dc, reinterpret_cast<uintptr_t>(&s_fCamParam1));
    RegisterExeAddr(0x140172174, reinterpret_cast<uintptr_t>(&s_fCamParam2));
    RegisterExeAddr(0x140172240, reinterpret_cast<uintptr_t>(&s_fCamParam3));
}
#endif // STANDALONE_EXE
