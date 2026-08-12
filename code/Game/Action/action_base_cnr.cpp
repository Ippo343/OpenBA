// code/Game/Action/action_base_cnr.cpp
// CreateNaturalResource / AnimateNaturalResource / RemoveNaturalResource
// shared exe sub-helpers (Wave 3, Action).
//
// These exe helpers are resolved via ResolveAddr(Module::Exe, 0xADDR) from the
// per-class Action .cpp files (createnaturalresource.cpp, animatenaturalresource.cpp,
// removenaturalresource.cpp). They resolve to 0 (silently skipped) unless registered.
// This file provides faithful 1:1 ports and a single registration point
// (RegisterActionCnrBatch in standalone/reg_action_cnr.cpp).
//
// Each function is a literal port of the decompiled exe helper — no behavior is
// invented or "improved". Every exe sub-call is mirrored through a guarded
// ResolveAddr typedef'd pointer (closure-safe: an unregistered sub-callee simply
// turns the call into a no-op).
//

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// ============================================================
// Lazily-resolved exe sub-helpers and type-ID globals (shared by this batch).
// ============================================================
namespace {

// Type-ID storage globals (exe .rdata, filled by RegisterType at startup).
const uint32_t* s_pTid_c7a50 = nullptr;  // 0x1401c7a50  (FindComponent in 0x140003f60)
const uint32_t* s_pTid_c77b0 = nullptr;  // 0x1401c77b0  (FindComponent in 0x14000c460)
const uint32_t* s_pTid_c7a88 = nullptr;  // 0x1401c7a88  (ModelDescription)
const uint32_t* s_pTid_c7b68 = nullptr;  // 0x1401c7b68  (NaturalResourceDescription)

// Exe sub-helpers used by the complex ports.
typedef void* (*fn_FindComp_t)(uintptr_t entity, void* retIBP);   // member ABI RCX,RDX
typedef void  (*fn_ModelComp2b70_t)(void* comp, uint32_t arg);
typedef void  (*fn_NotifyChanged_t)(void* internal);              // FUN_1400c3470
typedef void* (*fn_FindComp_3050_t)(uintptr_t entity, void* retIBP);

fn_FindComp_t       s_fn_FindComp_3f60     = nullptr;  // 0x140003f60 (this file)
fn_FindComp_t       s_fn_FindComp_3050     = nullptr;  // 0x140003050
fn_FindComp_t       s_fn_FindComp_4680     = nullptr;  // 0x140004680
fn_ModelComp2b70_t  s_fn_ModelComp_2b70    = nullptr;  // 0x1400c2b70
fn_NotifyChanged_t  s_fn_NotifyChanged     = nullptr;  // 0x1400c3470

// 0x140125320 helper (subvariant count) — used by 0x1401260f0? no; standalone use of FUN_140006330.
typedef void* (*fn_GetMeshDesc_6330_t)(uintptr_t descObj, void* retbuf);  // FUN_140006330
fn_GetMeshDesc_6330_t s_fn_GetMeshDesc_6330 = nullptr;

// 0x1401260f0 closure sub-helpers.
typedef void  (*fn_c2ea0_t)(uintptr_t internal);                                 // FUN_1400c2ea0
typedef void  (*fn_003110_t)(int64_t* tree, int key, void* ibp);                 // FUN_140003110
typedef void  (*fn_8d50_t)(int64_t map, void* retIBP, int cell);                 // RemovalTool_helper_8d50
typedef void* (*fn_c3720_t)(void* mem, void* desc);                              // ComponentEntity_CopyCtor
typedef void  (*fn_0223c0_t)(int64_t entity, void* retIBP);                      // FUN_1400223c0
typedef int   (*fn_RandTol_t)(void* random, int n);                             // Random::RandTol
typedef void  (*fn_9760_t)(int64_t comp, int cell, float amount);              // CreateRawMaterial_helper_9760
typedef void  (*fn_022480_t)(int64_t target, void* ibp);                        // FUN_140022480
typedef void  (*fn_88ef0_t)(int64_t mgr, void* retIBP, char flag, int64_t desc, int amount); // CreateRawMaterial_helper_8ef0
typedef void  (*fn_c48a0_t)(void** list, void* ibp, int group);                 // FUN_1400c48a0
typedef void  (*fn_022520_t)(int64_t self, void* retIBP);                        // FUN_140022520
typedef void  (*fn_044810_t)(int64_t a, uint32_t b);                            // FUN_140044810

fn_c2ea0_t   s_fn_c2ea0   = nullptr;
fn_003110_t  s_fn_003110  = nullptr;
fn_8d50_t    s_fn_8d50    = nullptr;
fn_c3720_t   s_fn_c3720   = nullptr;
fn_0223c0_t  s_fn_0223c0  = nullptr;
fn_RandTol_t s_fn_RandTol = nullptr;
fn_9760_t    s_fn_9760    = nullptr;
fn_022480_t  s_fn_022480  = nullptr;
fn_88ef0_t   s_fn_88ef0   = nullptr;
fn_c48a0_t   s_fn_c48a0   = nullptr;
fn_022520_t  s_fn_022520  = nullptr;
fn_044810_t  s_fn_044810  = nullptr;

// 0x140126540 closure sub-helpers.
typedef float* (*fn_f3e0_t)(int* mapGrid, float* outA, float* inCell, char flag, void* extra); // CharacterComponent_impl_f3e0
typedef float  (*fn_RandFloatTol_t)(void* random);                              // Random::RandFloatTol
fn_f3e0_t        s_fn_f3e0        = nullptr;
fn_RandFloatTol_t s_fn_RandFloatTol = nullptr;

// 0x140126440 closure sub-helpers.
typedef void (*fn_50f0_t)(int64_t comp, uint32_t animIdx, float a, float b, float c); // AnimalComponent_helper_50f0
fn_50f0_t s_fn_50f0 = nullptr;

// Runtime dispatch-slot helpers (resolved through the .rdata slots, matching the
// way the existing call sites read IAT slots).
typedef void  (*fn_Vec4_t)(void* outVec4, const void* scalar);                  // Math::Vector4::Vector4 (slot 0x140152958)
typedef void  (*fn_Time_t)(void* outTime, float seconds);                       // System::Time::Time (slot 0x140152b00)
typedef void  (*fn_SetMesh_t)(void* meshInst, void* externalPtr);               // Graphics::MeshInstance::SetMesh (slot 0x140151498)
fn_Vec4_t    s_fn_Vec4    = nullptr;
fn_Time_t    s_fn_Time    = nullptr;
fn_SetMesh_t s_fn_SetMesh = nullptr;

// Float constants from the exe .rdata (read as globals).
const float* s_p_DAT_1401720f4 = nullptr;  // divisor in FUN_140120db0
const float* s_p_DAT_140172234 = nullptr;  // time scale in FUN_140120db0
const float* s_p_DAT_140172114 = nullptr;  // half-bias in Entity_impl_6540
const float* s_p_DAT_140172220 = nullptr;  // yaw scale in Entity_impl_6540 / 6440
const float* s_p_DAT_140172204 = nullptr;  // amount scale in FUN_1401260f0

bool s_resolved = false;

template <class T>
static inline T R(uintptr_t a) { return reinterpret_cast<T>(ResolveAddr(Module::Exe, a)); }

// Resolve a runtime function pointer stored in an exe .rdata dispatch slot.
static inline void* RSlot(uintptr_t slotAddr) {
    uintptr_t slot = ResolveAddr(Module::Exe, slotAddr);
    return slot ? *reinterpret_cast<void**>(slot) : nullptr;
}

void EnsureResolved() {
    if (s_resolved) return;
    s_resolved = true;

    s_pTid_c7a50 = R<const uint32_t*>(0x1401c7a50);
    s_pTid_c77b0 = R<const uint32_t*>(0x1401c77b0);
    s_pTid_c7a88 = R<const uint32_t*>(0x1401c7a88);
    s_pTid_c7b68 = R<const uint32_t*>(0x1401c7b68);

    s_fn_FindComp_3f60  = R<fn_FindComp_t>(0x140003f60);
    s_fn_FindComp_3050  = R<fn_FindComp_t>(0x140003050);
    s_fn_FindComp_4680  = R<fn_FindComp_t>(0x140004680);
    s_fn_ModelComp_2b70 = R<fn_ModelComp2b70_t>(0x1400c2b70);
    s_fn_NotifyChanged  = R<fn_NotifyChanged_t>(0x1400c3470);
    s_fn_GetMeshDesc_6330 = R<fn_GetMeshDesc_6330_t>(0x140006330);

    s_fn_c2ea0   = R<fn_c2ea0_t>(0x1400c2ea0);
    s_fn_003110  = R<fn_003110_t>(0x140003110);
    s_fn_8d50    = R<fn_8d50_t>(0x1400f8d50);
    s_fn_c3720   = R<fn_c3720_t>(0x1400c3720);
    s_fn_0223c0  = R<fn_0223c0_t>(0x1400223c0);
    s_fn_9760    = R<fn_9760_t>(0x140129760);
    s_fn_022480  = R<fn_022480_t>(0x140022480);
    s_fn_88ef0   = R<fn_88ef0_t>(0x140088ef0);
    s_fn_c48a0   = R<fn_c48a0_t>(0x1400c48a0);
    s_fn_022520  = R<fn_022520_t>(0x140022520);
    s_fn_044810  = R<fn_044810_t>(0x140044810);

    s_fn_f3e0    = R<fn_f3e0_t>(0x14009f3e0);
    s_fn_50f0    = R<fn_50f0_t>(0x1401250f0);

    // Random::RandTol / RandFloatTol via .rdata slots used by these helpers.
    s_fn_RandTol      = reinterpret_cast<fn_RandTol_t>(RSlot(0x1401512f0));
    s_fn_RandFloatTol = reinterpret_cast<fn_RandFloatTol_t>(RSlot(0x140151400));
    s_fn_Vec4         = reinterpret_cast<fn_Vec4_t>(RSlot(0x140152958));
    s_fn_Time         = reinterpret_cast<fn_Time_t>(RSlot(0x140152b00));
    s_fn_SetMesh      = reinterpret_cast<fn_SetMesh_t>(RSlot(0x140151498));

    s_p_DAT_1401720f4 = R<const float*>(0x1401720f4);
    s_p_DAT_140172234 = R<const float*>(0x140172234);
    s_p_DAT_140172114 = R<const float*>(0x140172114);
    s_p_DAT_140172220 = R<const float*>(0x140172220);
    s_p_DAT_140172204 = R<const float*>(0x140172204);
}

// Generic FindComponent body (matches WorkBase_FindComponent / the exe decompile):
// scan entity's component list (ptr array at entity+0x30, count int at entity+0x38),
// call each component's GetTypeId (vtable +0x10); on the first type match run an
// IsDerivedFrom gate before returning through the out InternalBasePtr.
void* FindComponentByTypeId(uintptr_t entity, void* retIBP, const uint32_t* pTypeId) {
    void* found = nullptr;
    if (pTypeId) {
        int n = *reinterpret_cast<int*>(entity + 0x38);
        uintptr_t list = *reinterpret_cast<uintptr_t*>(entity + 0x30);
        for (int i = 0; i < n; i++) {
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

}  // namespace

// Forward decl: ActCNR_SetAnimIndex_126440 tail-calls RemoveNatRes (defined below).
void ActCNR_RemoveNatRes_1260f0(int64_t self, void* citizenIBP, char harvestFlag);

// ============================================================
// Reimpl of Entity_ClearPathQueue (FUN_140003f60) — FindComponent, type 0x1401c7a50.
// (Called from createnaturalresource.cpp as FindComponent_NR and from
// animatenaturalresource.cpp as GetModelComp.)
// ============================================================
void* ActCNR_FindComponent_3f60(uintptr_t entity, void* retIBP) {
    EnsureResolved();
    return FindComponentByTypeId(entity, retIBP, s_pTid_c7a50);
}

// ============================================================
// Reimpl of FUN_14000c460 — FindComponent, type 0x1401c77b0 (GrowthComponent).
// ============================================================
void* ActCNR_FindComponent_c460(uintptr_t entity, void* retIBP) {
    EnsureResolved();
    return FindComponentByTypeId(entity, retIBP, s_pTid_c77b0);
}

// ============================================================
// Reimpl of CreateNaturalResource_helper_cb00 (FUN_14009cb00) — cell terrain
// availability check. Returns 1 (true) if the cell is unavailable (occupied /
// wrong pass type / pending in the +0xe8 BST), else 0.
// Cell record: array at mapGrid+0x80, stride 0x30; pass byte +0x29, status byte +0x2a.
// The +0xe8 field is the root of a BST of "pending" cell ints (key at node[0],
// children at node[4]=+0x10 and node[6]=+0x18).
// ============================================================
uint64_t ActCNR_CheckTerrain_cb00(void* mapGrid, int cellIndex) {
    EnsureResolved();
    uintptr_t mg = reinterpret_cast<uintptr_t>(mapGrid);
    uintptr_t rec = static_cast<int64_t>(cellIndex) * 0x30 + *reinterpret_cast<int64_t*>(mg + 0x80);
    uint32_t passBits = 1u << (*reinterpret_cast<uint8_t*>(rec + 0x29) & 0x1f);
    int* node = reinterpret_cast<int*>(static_cast<uintptr_t>(passBits));
    if (((passBits & 2) == 0) && ((passBits & 1) == 0) &&
        (*reinterpret_cast<int8_t*>(rec + 0x2a) == -1)) {
        node = *reinterpret_cast<int**>(mg + 0xe8);
        while (true) {
            while (true) {
                if (node == nullptr) return 0;
                if (*node <= cellIndex) break;
                node = *reinterpret_cast<int**>(node + 4);   // +0x10
            }
            if (cellIndex <= *node) break;
            node = *reinterpret_cast<int**>(node + 6);       // +0x18
        }
    }
    // CONCAT71(node>>8, 1): top 7 bytes = node>>8, low byte = 1 — i.e. node with its
    // low byte replaced by 1 (callers read only the low byte as the bool).
    return (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(node)) & ~0xffull) | 1u;
}

// ============================================================
// Reimpl of FUN_140120db0 — GrowthComponent::SetGrowthProgress(progress).
// self (param_1) = GrowthComponent; entity-IBPs at +0x10 (parent), +0x28, +0x38.
// ============================================================
void ActCNR_SetGrowthProgress_120db0(int64_t self, float progress) {
    EnsureResolved();
    uintptr_t s = static_cast<uintptr_t>(self);
    float growth = progress * *reinterpret_cast<float*>(s + 0x4c);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(s + 0x28));
    float uvScale = g_fUVScale ? *g_fUVScale : 1.0f;
    *reinterpret_cast<float*>(*reinterpret_cast<int64_t*>(s + 0x28) + 0x28) = growth;
    float clamped = (uvScale < growth) ? uvScale : growth;
    *reinterpret_cast<float*>(s + 0x48) = clamped;

    if (*reinterpret_cast<int8_t*>(*reinterpret_cast<int64_t*>(s + 0x38) + 100) == 0) {
        float g = (growth <= uvScale) ? growth : uvScale;
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(s + 0x30));
        int64_t target = *reinterpret_cast<int64_t*>(s + 0x30);
        float scalar = g;
        uint8_t vec4[64];
        if (s_fn_Vec4) {
            s_fn_Vec4(vec4, &scalar);
            const uint32_t* pv = reinterpret_cast<const uint32_t*>(vec4);
            *reinterpret_cast<uint32_t*>(target + 0x134) = pv[0];
            *reinterpret_cast<uint32_t*>(target + 0x138) = pv[1];
            *reinterpret_cast<uint32_t*>(target + 0x13c) = pv[2];
            *reinterpret_cast<uint32_t*>(target + 0x140) = pv[3];
        }
    } else {
        float divisor = s_p_DAT_1401720f4 ? *s_p_DAT_1401720f4 : 1.0f;
        float scaled = clamped / divisor;
        float lo = 0.0f;
        if ((0.0f <= scaled) && (lo = scaled, uvScale < scaled)) lo = uvScale;
        float hi = growth;
        if (uvScale < growth) hi = uvScale;
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(s + 0x30));
        int64_t target = *reinterpret_cast<int64_t*>(s + 0x30);
        *reinterpret_cast<float*>(target + 0x134) = lo;
        *reinterpret_cast<float*>(target + 0x138) = lo;
        *reinterpret_cast<float*>(target + 0x13c) = hi;
        *reinterpret_cast<float*>(target + 0x140) = uvScale;
    }

    int64_t desc = *reinterpret_cast<int64_t*>(s + 0x38);
    float rf = 0.0f;
    if (fn_Random_RandFloat) {
        void* rng = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xd8);
        rf = fn_Random_RandFloat(rng);
    }
    float timeScale = s_p_DAT_140172234 ? *s_p_DAT_140172234 : 1.0f;
    uint8_t timeObj[8] = {0};
    if (s_fn_Time) s_fn_Time(timeObj, rf * static_cast<float>(*reinterpret_cast<int*>(desc + 0x50)) * timeScale);

    uint32_t flagsOld = *reinterpret_cast<uint32_t*>(s + 0x20);
    *reinterpret_cast<uint64_t*>(s + 0x40) = *reinterpret_cast<uint64_t*>(timeObj);
    uint32_t flagsNew = flagsOld & 0xffffffef;
    *reinterpret_cast<uint32_t*>(s + 0x20) = flagsNew;
    if ((((flagsNew ^ flagsOld) & 0xffff) != 0) && (*reinterpret_cast<int64_t*>(s + 0x10) != 0)) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(s + 0x10));
        if (s_fn_NotifyChanged) s_fn_NotifyChanged(*reinterpret_cast<void**>(s + 0x10));
    }
}

// ============================================================
// Reimpl of CreateNaturalResource_helper_52f0 (FUN_1401252f0) — NR variant count.
// Reads description at comp+0x18, type-gates against 0x1401c7a88, returns desc+0x28.
// ============================================================
uint32_t ActCNR_NR_GetVariantCount_1252f0(int64_t comp) {
    EnsureResolved();
    void* desc = *reinterpret_cast<void**>(static_cast<uintptr_t>(comp) + 0x18);
    void* d = (s_pTid_c7a88 && fn_IsDerivedFrom && fn_IsDerivedFrom(desc, *s_pTid_c7a88)) ? desc : nullptr;
    return *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(d) + 0x28);
}

// ============================================================
// Reimpl of FUN_140125320 — NR sub-variant count for a given variant index.
// desc at comp+0x18, type-gated; reads *(desc+0x20) + 0x10 + variant*0x18.
// ============================================================
uint32_t ActCNR_NR_GetSubVariantCount_125320(int64_t comp, int variant) {
    EnsureResolved();
    void* desc = *reinterpret_cast<void**>(static_cast<uintptr_t>(comp) + 0x18);
    void* d = (s_pTid_c7a88 && fn_IsDerivedFrom && fn_IsDerivedFrom(desc, *s_pTid_c7a88)) ? desc : nullptr;
    int64_t arr = *reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(d) + 0x20);
    return *reinterpret_cast<uint32_t*>(arr + 0x10 + static_cast<int64_t>(variant) * 0x18);
}

// ============================================================
// Reimpl of FUN_1401253d0 — NaturalResourceComponent::SetVariant(variant, subVariant).
// comp (param_1) = int64_t* base. Writes variant @+0x164, subVariant @+0x166, and on
// transitions (de)activates the model: vtable +0x38 (hide) when an old subVariant was
// active; sets the mesh + vtable +0x30 (show) when the new subVariant is valid;
// finally refreshes the model child at comp[2] via ModelComponent_helper_2b70(.,0).
// ============================================================
void ActCNR_NR_SetVariant_1253d0(int64_t* comp, uint16_t variant, int16_t subVariant) {
    EnsureResolved();
    uintptr_t c = reinterpret_cast<uintptr_t>(comp);
    // type-gate the description at comp+0x18 (RDI[0x18]); return value unused here.
    void* desc = *reinterpret_cast<void**>(c + 0x18);
    if (s_pTid_c7a88 && fn_IsDerivedFrom) fn_IsDerivedFrom(desc, *s_pTid_c7a88);

    *reinterpret_cast<uint16_t*>(c + 0x164) = variant;
    if (*reinterpret_cast<int16_t*>(c + 0x166) >= 0) {
        // vtable +0x38 — hide/deactivate current model (RCX = comp)
        (*reinterpret_cast<void(**)(int64_t*)>(*comp + 0x38))(comp);
    }
    *reinterpret_cast<int16_t*>(c + 0x166) = subVariant;
    if (subVariant >= 0) {
        // Re-gate the description and resolve the mesh ExternalPtr for (variant, subVariant):
        //   d   = IsDerivedFrom(desc, c7a88) ? desc : 0
        //   arr = *(d + 0x20)
        //   sub = *(arr + variant*0x18 + 8)   (per-variant sub-array)
        //   mesh= *(sub + subVariant*8)
        void* descMesh = *reinterpret_cast<void**>(c + 0x18);
        void* d = (s_pTid_c7a88 && fn_IsDerivedFrom && fn_IsDerivedFrom(descMesh, *s_pTid_c7a88)) ? descMesh : nullptr;
        int16_t sub = *reinterpret_cast<int16_t*>(c + 0x166);
        int16_t var = *reinterpret_cast<int16_t*>(c + 0x164);
        int64_t arr = *reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(d) + 0x20);
        int64_t subArr = *reinterpret_cast<int64_t*>(arr + static_cast<int64_t>(var) * 0x18 + 8);
        void* mesh = *reinterpret_cast<void**>(subArr + static_cast<int64_t>(sub) * 8);
        // Graphics::MeshInstance::SetMesh(this = comp+0x28, mesh)
        if (s_fn_SetMesh) s_fn_SetMesh(reinterpret_cast<void*>(c + 0x28), mesh);
        // vtable +0x30 — show/activate model (RCX = comp)
        (*reinterpret_cast<void(**)(int64_t*)>(*comp + 0x30))(comp);
    }
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(c + 0x10));
    if (s_fn_ModelComp_2b70) s_fn_ModelComp_2b70(*reinterpret_cast<void**>(c + 0x10), 0);
}

// ============================================================
// Reimpl of AnimateNaturalResource_helper_6440 (FUN_140126440) — NaturalResource
// SetAnimIndex(animIdx). self (param_1) = NaturalResourceComponent.
// If the description (self+0x18, type-gated 0x1401c7b68) byte +100 == 0:
//   if "removed" bit (+0x20 bit 21) set, run RemoveNatRes (FUN_1401260f0) with a null IBP.
// else (byte +100 != 0): get model component (Entity_ClearPathQueue on parent at +0x10),
//   AnimalComponent_helper_50f0(model, animIdx, 0,0, g_fUVScale), set bit 2 (+0x20 |= 4),
//   and on a low-16-bit flag change with non-null parent, notify changed.
// ============================================================
void ActCNR_SetAnimIndex_126440(int64_t self, uint32_t animIdx) {
    EnsureResolved();
    uintptr_t s = static_cast<uintptr_t>(self);
    void* desc = *reinterpret_cast<void**>(s + 0x18);
    void* d = (s_pTid_c7b68 && fn_IsDerivedFrom && fn_IsDerivedFrom(desc, *s_pTid_c7b68)) ? desc : nullptr;

    if (*reinterpret_cast<int8_t*>(reinterpret_cast<uintptr_t>(d) + 100) == 0) {
        if (((*reinterpret_cast<uint32_t*>(s + 0x20) >> 0x15) & 1) != 0) {
            int64_t nullIBP = 0;
            if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&nullIBP, nullptr);
            ActCNR_RemoveNatRes_1260f0(self, &nullIBP, 0);
        }
    } else {
        int64_t modelIBP = 0;
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(s + 0x10));
        if (s_fn_FindComp_3f60) s_fn_FindComp_3f60(*reinterpret_cast<uintptr_t*>(s + 0x10), &modelIBP);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&modelIBP);
        float uvScale = g_fUVScale ? *g_fUVScale : 1.0f;
        if (s_fn_50f0) s_fn_50f0(modelIBP, animIdx, 0.0f, 0.0f, uvScale);
        uint32_t flagsOld = *reinterpret_cast<uint32_t*>(s + 0x20);
        uint32_t flagsNew = flagsOld | 4;
        *reinterpret_cast<uint32_t*>(s + 0x20) = flagsNew;
        if ((((flagsNew ^ flagsOld) & 0xffff) != 0) && (*reinterpret_cast<int64_t*>(s + 0x10) != 0)) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(s + 0x10));
            if (s_fn_NotifyChanged) s_fn_NotifyChanged(*reinterpret_cast<void**>(s + 0x10));
        }
    }
}

// ============================================================
// Reimpl of Entity_impl_6540 (FUN_140126540) — CreateNaturalResource model placement.
// self (param_1) = NaturalResourceComponent; param_2 = cell index. Positions the
// model + transform component at the cell, with per-resource jitter.
// ============================================================
uint64_t ActCNR_SetupModelAtCell_126540(int64_t self, int cellIndex) {
    EnsureResolved();
    uintptr_t s = static_cast<uintptr_t>(self);
    uintptr_t gameWorld = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
    void* desc = *reinterpret_cast<void**>(s + 0x18);
    int* mapGrid = *reinterpret_cast<int**>(gameWorld + 0xf8);
    void* rng = *reinterpret_cast<void**>(gameWorld + 0xd8);
    int gridW = *mapGrid;

    void* d = (s_pTid_c7b68 && fn_IsDerivedFrom && fn_IsDerivedFrom(desc, *s_pTid_c7b68)) ? desc : nullptr;
    float jitter = *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(d) + 0x60);

    float halfBias = s_p_DAT_140172114 ? *s_p_DAT_140172114 : 0.0f;
    float half = static_cast<float>(*mapGrid >> 1) - halfBias;
    float cellX = static_cast<float>(cellIndex % *mapGrid) - half;
    float cellY = static_cast<float>(cellIndex / *mapGrid) - half;

    // CharacterComponent_impl_f3e0 fills a Vector3 (local_a8/a4/a0) for the cell.
    float inCell[2] = { cellX, cellY };
    float outPos[3] = { 0.0f, 0.0f, 0.0f };
    if (s_fn_f3e0) s_fn_f3e0(mapGrid, outPos, inCell, '\0', nullptr);

    float r1 = s_fn_RandFloatTol ? s_fn_RandFloatTol(rng) : 0.0f;
    float r2 = s_fn_RandFloatTol ? s_fn_RandFloatTol(rng) : 0.0f;
    float worldX = r1 * jitter + outPos[1];
    float worldY = r2 * jitter + outPos[0];
    float worldZ = outPos[2] + 0.0f;
    float yawScale = s_p_DAT_140172220 ? *s_p_DAT_140172220 : 1.0f;
    float yaw = (s_fn_RandFloatTol ? s_fn_RandFloatTol(rng) : 0.0f) * yawScale;

    int64_t posIBP = 0;
    int64_t modelIBP = 0;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(s + 0x10));
    if (s_fn_FindComp_4680) s_fn_FindComp_4680(*reinterpret_cast<uintptr_t*>(s + 0x10), &posIBP);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(s + 0x10));
    if (s_fn_FindComp_3f60) s_fn_FindComp_3f60(*reinterpret_cast<uintptr_t*>(s + 0x10), &modelIBP);

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&posIBP);
    *reinterpret_cast<int16_t*>(posIBP + 0x28) = static_cast<int16_t>(cellIndex % gridW);
    *reinterpret_cast<int16_t*>(posIBP + 0x2a) = static_cast<int16_t>(cellIndex / gridW);
    *reinterpret_cast<uint64_t*>(posIBP + 0x2c) = 0x10001;

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&modelIBP);
    // Math::Transform::FromYawPitchOffset(out, &yaw, &pitch(=0), &offset(worldX,worldY,worldZ))
    float pitch = 0.0f;
    float offset[3] = { worldY, worldX, worldZ };  // FromYawPitchOffset offset = {local_98, local_94, local_90}
    // NOTE: the original passes local_res8[0]=yaw as the Scalar and local_res10 (Vector3
    // zero-ish) — see disasm; we mirror the documented FromYawPitchOffset ABI.
    uint8_t xform[80] = {0};
    if (fn_Transform_FromYawPitchOffset) {
        fn_Transform_FromYawPitchOffset(xform, &yaw, &pitch, offset);
        const uint32_t* pv = reinterpret_cast<const uint32_t*>(xform);
        *reinterpret_cast<uint32_t*>(modelIBP + 0x148) = pv[0];
        *reinterpret_cast<uint32_t*>(modelIBP + 0x14c) = pv[1];
        *reinterpret_cast<uint32_t*>(modelIBP + 0x150) = pv[2];
        *reinterpret_cast<uint32_t*>(modelIBP + 0x154) = pv[3];
        *reinterpret_cast<uint32_t*>(modelIBP + 0x158) = pv[4];
        *reinterpret_cast<uint32_t*>(modelIBP + 0x15c) = pv[5];
        *reinterpret_cast<uint32_t*>(modelIBP + 0x160) = pv[6];
    }
    *reinterpret_cast<float*>(modelIBP + 0x168) = yaw;
    if (fn_Geometry_SetTransform)
        fn_Geometry_SetTransform(reinterpret_cast<void*>(modelIBP + 0x28),
                                 reinterpret_cast<void*>(modelIBP + 0x148));
    return 1;
}

// ============================================================
// Reimpl of FUN_1401260f0 — RemoveNaturalResource core removal/harvest.
// self (param_1) = NaturalResourceComponent, param_2 = citizen/work IBP, param_3 = harvest flag.
// Detaches the model from the build queue, then (if harvestable, byte +0x2d) spawns a
// raw-material entity at the cell, transfers the harvest amount to the citizen, and
// registers the new entity.
// ============================================================
void ActCNR_RemoveNatRes_1260f0(int64_t self, void* citizenIBP, char harvestFlag) {
    // Removal census. Crops vanish from their tiles between planting and harvest --
    // measured tiles==growing exactly, with the difference showing up as slots that
    // ValidateEntityRefs demoted to state 2. This is the only path that removes a
    // natural resource, so counting it (and its harvest flag) says whether something is
    // actively clearing the field's own crops and whether it produces anything.
    {
        static int s_t = -1;
        if (s_t < 0) { const char* e = std::getenv("BANISHED_CROPFIELD_TRACE");
                       s_t = (e && e[0] && e[0] != '0') ? 1 : 0; }
        static long long s_harv = 0, s_clear = 0, s_n = 0;
        if (harvestFlag) ++s_harv; else ++s_clear;
        if (s_t && ((s_n++ % 10) == 0)) {
            std::fprintf(stderr, "[RemoveNatRes] harvestFlag=1 -> %lld, harvestFlag=0 (CLEAR) -> %lld\n",
                         s_harv, s_clear);
            std::fflush(stderr);
        }
    }
    EnsureResolved();
    uintptr_t s = static_cast<uintptr_t>(self);
    uintptr_t gameWorld = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
    int64_t parent = *reinterpret_cast<int64_t*>(s + 0x10);
    int64_t entityList = *reinterpret_cast<int64_t*>(gameWorld + 0xe8);

    // [standalone] Entity-lifecycle gate (BANISHED_PRODUCE_SPAWN_ENTITY, default ON since
    // economy brick M-B; set =0 to opt out for A/B). The historical fault ("next-frame
    // EntityList PreUpdate/removal faults in Runtime") was the NULL ComponentEntity vtable —
    // 0x14015fa78 was unregistered, so InternalBasePtr::DeletePtr's first-ever virtual
    // dispatch (the scalar dtor) called through NULL. With the vtable materialized
    // (componententity.cpp g_ComponentEntity_StandaloneVtable) and the removal-side grid
    // occupancy un-mark wired (mapcomponent.cpp MapComponent_DetachOccupancy_4750), the
    // full lifecycle — tree removal via the entityList+0xd0 dirty list + ground-pile item
    // clone/position/register — runs crash-free (soak-verified: 37 organic produces).
    static int s_spawnItem = -1;
    if (s_spawnItem < 0) {
        const char* e = std::getenv("BANISHED_PRODUCE_SPAWN_ENTITY");
        s_spawnItem = (e && e[0] == '0') ? 0 : 1;
    }

    // Grid claim for the spawned pile (see the long note at the write below). Default ON,
    // matching starttown.cpp's BANISHED_RES_OCCUPANCY collapse for worldgen resources;
    // only "0" disables.
    static int s_pileAttach = -1;
    if (s_pileAttach < 0) {
        const char* e = std::getenv("BANISHED_PILE_GRID_ATTACH");
        s_pileAttach = (e && e[0] == '0') ? 0 : 1;
    }

    // Detach model from the build queue when not already queued (bit 10 of +0x40).
    if (s_spawnItem && parent != 0) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&parent);
        if (((*reinterpret_cast<uint32_t*>(parent + 0x40) >> 10) & 1) == 0) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&parent);
            if (s_fn_c2ea0) s_fn_c2ea0(parent);
            if (s_fn_003110)
                s_fn_003110(reinterpret_cast<int64_t*>(entityList + 0xd0),
                            *reinterpret_cast<int*>(entityList + 0xd8) + 1, &parent);
        }
    }

    // [diag] Gate trace for the harvest-yield investigation (BANISHED_FORESTER_SCAN_TRACE).
    // 1260f0 has three early-outs before the pile spawn; report which one fires.
    const bool yTrace = std::getenv("BANISHED_FORESTER_SCAN_TRACE") != nullptr;

    if (*reinterpret_cast<int8_t*>(s + 0x2d) == 0) {   // not harvestable
        if (yTrace) {
            fprintf(stderr, "[ORGANIC-PRODUCE] RemoveNatRes GATE-1 harvestable(+0x2d)=0 -> no yield "
                            "(nrComp=%p mature(+0x2c)=%d frac(+0x28)=%f)\n",
                    (void*)s, (int)*reinterpret_cast<int8_t*>(s + 0x2c),
                    (double)*reinterpret_cast<float*>(s + 0x28));
            fflush(stderr);
        }
        return;
    }

    void* desc = *reinterpret_cast<void**>(s + 0x18);
    void* d = (s_pTid_c7b68 && fn_IsDerivedFrom && fn_IsDerivedFrom(desc, *s_pTid_c7b68)) ? desc : nullptr;
    int64_t descArr = *reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(d) + 0x20);
    if (descArr == 0) {
        if (yTrace) {
            fprintf(stderr, "[ORGANIC-PRODUCE] RemoveNatRes GATE-2 descArr(+0x20)=0 -> no yield "
                            "(desc=%p isNatResDesc=%d)\n", desc, (int)(d != nullptr));
            fflush(stderr);
        }
        return;
    }

    float uvScale = g_fUVScale ? *g_fUVScale : 1.0f;
    float frac = uvScale;
    if (*reinterpret_cast<float*>(s + 0x28) <= uvScale) frac = *reinterpret_cast<float*>(s + 0x28);

    int amount;
    if (*reinterpret_cast<int8_t*>(s + 0x2c) == 0) {
        void* meshDesc[4] = {0};
        void** pd = s_fn_GetMeshDesc_6330
                    ? reinterpret_cast<void**>(s_fn_GetMeshDesc_6330(descArr, meshDesc)) : nullptr;
        amount = pd ? *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(*pd) + 0x40) : 0;
    } else {
        int64_t tmp = 0;
        void** pd = s_fn_GetMeshDesc_6330
                    ? reinterpret_cast<void**>(s_fn_GetMeshDesc_6330(descArr, &tmp)) : nullptr;
        amount = pd ? *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(*pd) + 0x44) : 0;
    }
    int rawAmount = amount;
    amount = static_cast<int>(static_cast<float>(amount) * frac);
    if (amount <= 0) {
        if (yTrace) {
            fprintf(stderr, "[ORGANIC-PRODUCE] RemoveNatRes GATE-3 amount=0 -> no yield "
                            "(rawAmount=%d frac=%f mature(+0x2c)=%d descArr=%p GetMeshDesc=%p)\n",
                    rawAmount, (double)frac, (int)*reinterpret_cast<int8_t*>(s + 0x2c),
                    (void*)descArr, (void*)s_fn_GetMeshDesc_6330);
            fflush(stderr);
        }
        return;
    }

    // Compute the cell index from the model's grid position.
    int64_t posIBP = 0;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(s + 0x10));
    if (s_fn_FindComp_4680) s_fn_FindComp_4680(*reinterpret_cast<uintptr_t*>(s + 0x10), &posIBP);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&posIBP);
    int cell = static_cast<int>(*reinterpret_cast<int16_t*>(posIBP + 0x2a)) *
                   **reinterpret_cast<int**>(gameWorld + 0xf8) +
               static_cast<int>(*reinterpret_cast<int16_t*>(posIBP + 0x28));
    // Mirrors the original's reused stack slot (local_res8 low32 = cell). 022480 reads
    // this slot by pointer; keep it live in a dedicated 8-byte local.
    int64_t cellSlot = static_cast<int64_t>(static_cast<uint32_t>(cell));

    // ── ECONOMY ACCOUNTING (always) ─────────────────────────────────────────
    // Increment the town raw-material stock — the essential produce effect the economy loop
    // consumes. RegisterRawMaterial needs only the citizen/work IBP, the resource description
    // and the yield amount; NOT the spawned physical item entity, so it runs unconditionally.
    int64_t regIBP = *reinterpret_cast<int64_t*>(citizenIBP);
    if (std::getenv("BANISHED_FORESTER_SCAN_TRACE")) {
        int64_t descEnt = *reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(d) + 0x20);
        fprintf(stderr, "[ORGANIC-PRODUCE] RemoveNatRes harvest: amount=%d cell=%d harvestFlag=%d "
                        "descEnt=%p RegisterRawMaterial=%p\n",
                amount, cell, (int)harvestFlag, (void*)descEnt, (void*)s_fn_88ef0);
        fflush(stderr);
    }
    // ATTRIBUTED DEPOSIT METER. This call is the town-stock increment itself, so summing
    // `amount` here credits food to the harvest that produced it. Net town stock cannot do
    // that: material 5 is the aggregate Food pool, citizens eat from it continuously, and a
    // control run with NO food building still showed +700 gross. A monotonic rise in the
    // pool is therefore NOT evidence that a particular class deposited -- measure at the
    // source instead of inferring from the total.
    {
        static int s_dt = -1;
        if (s_dt < 0) { const char* e = std::getenv("BANISHED_CROPFIELD_TRACE");
                        s_dt = (e && e[0] && e[0] != '0') ? 1 : 0; }
        static long long s_units = 0, s_calls = 0;
        if (harvestFlag) { s_units += amount; ++s_calls; }
        if (s_dt && s_calls && (s_calls % 10) == 0)
            fprintf(stderr, "[DepositMeter] harvest deposits: %lld calls, %lld units "
                            "(last amount=%d)\n", s_calls, s_units, amount);
    }
    if (s_fn_88ef0)
        s_fn_88ef0(*reinterpret_cast<int64_t*>(gameWorld + 0xf0), &regIBP, harvestFlag,
                   *reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(d) + 0x20), amount);

    // ── PHYSICAL ITEM ENTITY (opt-in: BANISHED_PRODUCE_SPAWN_ENTITY) ─────────
    // Clone/position/register the ground-pile item entity. In standalone the clone is only
    // partially materialized (inert component factories); once it joins the live world entity
    // list the next frame's EntityList_PreUpdate → Serialize faults in Runtime-x64.dll. Gate
    // the whole visual/lifecycle block until entity materialization is live — the stock
    // accounting above is what the economy loop reads.
    if (!s_spawnItem) return;

    // Query the existing surface tile at the cell (for its material id at +0x24).
    int64_t surfIBP = 0;
    if (s_fn_8d50) s_fn_8d50(*reinterpret_cast<int64_t*>(gameWorld + 0x118), &surfIBP, cell);
    uint32_t surfMatId = 0xffffffff;
    if (surfIBP != 0) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&surfIBP);
        surfMatId = *reinterpret_cast<uint32_t*>(surfIBP + 0x24);
    }

    // Spawn a new raw-material entity cloned from the resource description's template.
    void* mem = fn_MemAlloc ? fn_MemAlloc(g_pMemAllocSingleton, 0x48, 8) : nullptr;
    void* newEntity = (mem && s_fn_c3720)
                      ? s_fn_c3720(mem, *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(d) + 0x20))
                      : nullptr;
    int64_t newIBP = 0;
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&newIBP, newEntity);

    int64_t rawMatIBP = 0;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&newIBP);
    if (s_fn_0223c0) s_fn_0223c0(newIBP, &rawMatIBP);
    void* rng = *reinterpret_cast<void**>(gameWorld + 0xd8);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&rawMatIBP);
    int rt = s_fn_RandTol ? s_fn_RandTol(rng, 2) : 0;
    // DAT_140172204 = 90.0f (rotation scale). It resolves NULL in standalone (raw .rdata
    // constant) — use the real value, not 0 (see reference_standalone_rdata_null_fallback).
    float amtScale = s_p_DAT_140172204 ? *s_p_DAT_140172204 : 90.0f;
    if (s_fn_9760 && rawMatIBP != 0) s_fn_9760(rawMatIBP, cell, static_cast<float>(rt) * amtScale);

    if (*reinterpret_cast<int64_t*>(citizenIBP) != 0) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(citizenIBP);
        int64_t cit28 = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(citizenIBP) + 0x28);
        // Original: FUN_140022480(*(citizen+0x28), &local_res8) where local_res8 = cell.
        if (s_fn_022480) s_fn_022480(cit28, &cellSlot);
    }

    if (rawMatIBP != 0) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&rawMatIBP);
        *reinterpret_cast<int*>(rawMatIBP + 0x28) = amount;
        *reinterpret_cast<int64_t*>(rawMatIBP + 0x30) = 0;
    }

    if (surfIBP != 0) {
        int64_t nrIBP = 0;
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&newIBP);
        void* p = s_fn_FindComp_3050 ? s_fn_FindComp_3050(newIBP, &nrIBP) : nullptr;
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(p);
        if (p) *reinterpret_cast<uint32_t*>(*reinterpret_cast<int64_t*>(p) + 0x2c) = surfMatId;
    }

    int64_t listIBP = newIBP;
    if (s_fn_c48a0)
        s_fn_c48a0(*reinterpret_cast<void***>(gameWorld + 0xe8), &listIBP,
                   *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(d) + 0x20) + 0x20));

    // ── GRID CLAIM (dc20's net effect, scoped to this pile) ──────────────────
    // Retail claims the cell via c48a0 -> ComponentEntity_OnAddedPass -> the pile
    // MapComponent's vtable+0x30 (MapComponent::Draw 0x140124370) -> sub_41b0
    // (0x1401241b0) -> dc20 (0x14009dc20), whose net effect is cell+0x08 = the owner
    // entity plus comp+0x20 bit 0x100000 ("occupancy currently written"). Standalone's
    // MapComponent is inert, so the pile spawned with cell+0x08 still null — and
    // GetTilesInArea (FUN_14009fc00) only reports a cell whose +0x08 is non-null, so the
    // pile was invisible to every tile scan, including the ClearArea::Execute Phase 3
    // gather that posts the haul work. That is why piles were never picked up.
    //
    // This is the same spawn-site collapse starttown.cpp already applies to worldgen
    // resources (StartTown_MarkResourceCellGrid), kept SCOPED to the pile: driving the
    // attach from MapComponent's slot for every entity regressed the RemovalTool, because
    // dc20 also marks any pre-existing cell occupant for removal. That hazard cannot fire
    // here — 1260f0 marks the tree at its top via c2ea0, which dispatches the tree
    // MapComponent's vtable+0x38 (-> impl_4750 -> dee0) IMMEDIATELY, nulling cell+0x08
    // before we get here. So the cell is already empty and there is no occupant to mark.
    // For the same reason the tree's later removal-drain cannot wipe this write.
    //
    // Only cell+0x08 is written: dc20 would also stamp the terrain type from
    // MapDescription+0x20, but the tree's dee0 detach has already reclassified the cell to
    // its natural (passable) type, which is what a walkable pile wants — the same
    // deliberate divergence documented at the starttown.cpp collapse. Bit 0x100000 is set
    // so the removal-side un-mark (MapComponent_DetachOccupancy_4750) stays symmetric and
    // the grid cannot keep pointing at a freed pile.
    if (s_pileAttach && newIBP != 0) {
        uintptr_t mapIBP = 0;
        if (s_fn_FindComp_4680) s_fn_FindComp_4680(newIBP, &mapIBP);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&mapIBP);
        uint8_t* mc = reinterpret_cast<uint8_t*>(mapIBP);
        uintptr_t mapGrid = *reinterpret_cast<uintptr_t*>(gameWorld + 0xf8);
        uint8_t* cells = mapGrid ? *reinterpret_cast<uint8_t**>(mapGrid + 0x80) : nullptr;
        const int dim = mapGrid ? *reinterpret_cast<int32_t*>(mapGrid) : 0;
        // 41b0's own gate: skip when occupancy is already written for this component.
        if (mc && cells && dim > 0 && cell >= 0 && cell < dim * dim &&
            ((*reinterpret_cast<uint32_t*>(mc + 0x20) >> 20) & 1) == 0) {
            *reinterpret_cast<uintptr_t*>(cells + static_cast<size_t>(cell) * 0x30 + 0x08) = newIBP;
            *reinterpret_cast<uint32_t*>(mc + 0x20) |= 0x100000u;
        }
    }

    // [diag] Report the cell slots so the claim above is observable: cell+0x08 non-null
    // means the pile is now visible to tile scans.
    if (yTrace) {
        uintptr_t mapGrid = *reinterpret_cast<uintptr_t*>(gameWorld + 0xf8);
        uintptr_t cellArr = *reinterpret_cast<uintptr_t*>(mapGrid + 0x80);
        uintptr_t rec = cellArr + static_cast<uintptr_t>(cell) * 0x30;
        fprintf(stderr, "[ORGANIC-PRODUCE] pile REGISTERED cell=%d newEntity=%p | "
                        "grid cell+0x00=%p cell+0x08=%p (0x08 must == newEntity; null => "
                        "invisible to tile scans)\n",
                cell, (void*)newEntity,
                *reinterpret_cast<void**>(rec + 0x00), *reinterpret_cast<void**>(rec + 0x08));
        fflush(stderr);
    }

    if ((harvestFlag != 0) && (*reinterpret_cast<int64_t*>(citizenIBP) != 0)) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(citizenIBP);
        int64_t out = 0;
        if (s_fn_022520) s_fn_022520(*reinterpret_cast<int64_t*>(citizenIBP), &out);
        if (out != 0) {
            int64_t nrIBP2 = 0;
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&newIBP);
            void* p = s_fn_FindComp_3050 ? s_fn_FindComp_3050(newIBP, &nrIBP2) : nullptr;
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(p);
            uint32_t v = p ? *reinterpret_cast<uint32_t*>(*reinterpret_cast<int64_t*>(p) + 0x28) : 0;
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&out);
            if (s_fn_044810) s_fn_044810(out, v);
        }
    }
}
