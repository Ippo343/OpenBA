// code/Game/Game/worldgen_f390_shims.cpp
// Link shims for worldgen_f390_rivers.cpp (cd460/e910). That stage referenced a few
// engine helpers as GLOBAL extern "C" symbols rather than file-local resolved pointers
// (which the other f390 stages use). Defined here once so the standalone exe links.
//
// All semantics match how rivers.cpp actually calls them (verified at the call sites):
//   - fn_Vector2_Dot(v,&out)  is only ever self-dot  -> out = |v|^2
//   - FUN_140014c30           is only called on a null AVL root (dead path) -> no-op
//   - TradeComponent_helper_f9e0 copies a stride-4 {data,count,cap} int bucket
//   - fn_Random_RandFloatTol   advances the engine RNG -> must hit the real Runtime export

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cmath>

extern "C" {

// System::Random::RandFloatTol() — Runtime export; advances RNG state, returns ~[-1,1].
float fn_Random_RandFloatTol(void* self) {
    static auto p = reinterpret_cast<float (*)(void*)>(
        ResolveExport("?RandFloatTol@Random@System@@QEAAMXZ"));
    return p ? p(self) : 0.0f;
}

// rivers.cpp calls fn_Vector2_Dot(diff, &out) purely to get |diff|^2 (self-dot).
void fn_Vector2_Dot(void* a, void* out) {
    const float* v = static_cast<const float*>(a);
    *static_cast<float*>(out) = v[0] * v[0] + v[1] * v[1];
}

// Vector2::Length() — write length of {v[0],v[1]} into v[0]. (Only the connect pass,
// currently stubbed, would use this; defined for completeness.)
void fn_Vector2_Length(void* self_out) {
    float* v = static_cast<float*>(self_out);
    v[0] = sqrtf(v[0] * v[0] + v[1] * v[1]);
}

// Vector2::Normalize() — normalize in place, return self.
void* fn_Vector2_Normalize(void* self, void* /*scratch*/) {
    float* v = static_cast<float*>(self);
    float L = sqrtf(v[0] * v[0] + v[1] * v[1]);
    if (L > 0.0f) { v[0] /= L; v[1] /= L; }
    return self;
}

// vector<int> bucket deep-copy ({data@+0, count@+8, cap@+0xC}, stride-4 elements).
// Mirrors Reimpl_TradeComponent_helper_f9e0 in gameupdateloop_load.cpp.
void TradeComponent_helper_f9e0(void* dstElem, void* srcElem) {
    if (dstElem == srcElem) return;
    uint8_t* d = static_cast<uint8_t*>(dstElem);
    uint8_t* s = static_cast<uint8_t*>(srcElem);
    void* old = *reinterpret_cast<void**>(d + 0x00);
    if (old) fn_MemFreeStatic(old);
    *reinterpret_cast<int32_t*>(d + 0x08) = 0;
    *reinterpret_cast<void**>(d + 0x00) = nullptr;
    int32_t cnt = *reinterpret_cast<int32_t*>(s + 0x08);
    *reinterpret_cast<int32_t*>(d + 0x08) = cnt;
    if (cnt > 0) {
        *reinterpret_cast<int32_t*>(d + 0x0C) = cnt;
        *reinterpret_cast<void**>(d + 0x00) = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)cnt << 2, 4);
        int32_t* dp = *reinterpret_cast<int32_t**>(d + 0x00);
        int32_t* sp = *reinterpret_cast<int32_t**>(s + 0x00);
        for (int32_t i = 0; i < cnt; ++i) dp[i] = sp[i];
    }
}

// AVL recursive subtree free — rivers.cpp only calls this on a null root (the emulated
// cluster partition uses no AVL tree), so this is a no-op.
void FUN_140014c30(void** /*root*/, void* /*node*/) {}

}
