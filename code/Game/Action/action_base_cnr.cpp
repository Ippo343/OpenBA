// code/Game/Action/action_base_cnr.cpp
// CreateNaturalResource / AnimateNaturalResource / RemoveNaturalResource
// shared exe sub-helpers (Wave 3, Action).
//
// These exe helpers are resolved via ResolveAddr(Module::Exe, 0xADDR) from the
// per-class Action .cpp files (createnaturalresource.cpp, animatenaturalresource.cpp,
// removenaturalresource.cpp). In DLL mode they resolve to the original exe code;
// in standalone mode they resolve to 0 (silently skipped) unless registered.
// This file provides faithful 1:1 ports and a single registration point
// (RegisterActionCnrBatch in standalone/reg_action_cnr.cpp).
//
// Each function is a literal port of the decompiled exe helper — no behavior is
// invented or "improved". Every exe sub-call is mirrored through a guarded
// ResolveAddr typedef'd pointer (closure-safe: an unregistered sub-callee simply
// turns the call into a no-op, exactly like the original passthrough behavior).
//
// This file must NOT depend on hooks.h — it is portable for the standalone exe.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

// ============================================================
// Lazily-resolved exe sub-helpers and type-ID globals (shared by this batch).
// ============================================================
namespace {

// Type-ID storage globals (exe .rdata, filled by RegisterType at startup).
const uint32_t* s_pTid_c7a50 = nullptr;  // 0x1401c7a50  (FindComponent in 0x140003f60)
const uint32_t* s_pTid_c77b0 = nullptr;  // 0x1401c77b0  (FindComponent in 0x14000c460)
const uint32_t* s_pTid_c7a88 = nullptr;  // 0x1401c7a88  (NaturalResourceDescription? variant)
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
    EnsureResolved();
    uintptr_t s = static_cast<uintptr_t>(self);
    uintptr_t gameWorld = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
    int64_t parent = *reinterpret_cast<int64_t*>(s + 0x10);
    int64_t entityList = *reinterpret_cast<int64_t*>(gameWorld + 0xe8);

    // Detach model from the build queue when not already queued (bit 10 of +0x40).
    if (parent != 0) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&parent);
        if (((*reinterpret_cast<uint32_t*>(parent + 0x40) >> 10) & 1) == 0) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&parent);
            if (s_fn_c2ea0) s_fn_c2ea0(parent);
            if (s_fn_003110)
                s_fn_003110(reinterpret_cast<int64_t*>(entityList + 0xd0),
                            *reinterpret_cast<int*>(entityList + 0xd8) + 1, &parent);
        }
    }

    if (*reinterpret_cast<int8_t*>(s + 0x2d) == 0) return;  // not harvestable

    void* desc = *reinterpret_cast<void**>(s + 0x18);
    void* d = (s_pTid_c7b68 && fn_IsDerivedFrom && fn_IsDerivedFrom(desc, *s_pTid_c7b68)) ? desc : nullptr;
    int64_t descArr = *reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(d) + 0x20);
    if (descArr == 0) return;

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
    amount = static_cast<int>(static_cast<float>(amount) * frac);
    if (amount <= 0) return;

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
    float amtScale = s_p_DAT_140172204 ? *s_p_DAT_140172204 : 0.0f;
    if (s_fn_9760) s_fn_9760(rawMatIBP, cell, static_cast<float>(rt) * amtScale);

    if (*reinterpret_cast<int64_t*>(citizenIBP) != 0) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(citizenIBP);
        int64_t cit28 = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(citizenIBP) + 0x28);
        // Original: FUN_140022480(*(citizen+0x28), &local_res8) where local_res8 = cell.
        if (s_fn_022480) s_fn_022480(cit28, &cellSlot);
    }

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&rawMatIBP);
    *reinterpret_cast<int*>(rawMatIBP + 0x28) = amount;
    *reinterpret_cast<int64_t*>(rawMatIBP + 0x30) = 0;

    if (surfIBP != 0) {
        int64_t nrIBP = 0;
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&newIBP);
        void* p = s_fn_FindComp_3050 ? s_fn_FindComp_3050(newIBP, &nrIBP) : nullptr;
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(p);
        if (p) *reinterpret_cast<uint32_t*>(*reinterpret_cast<int64_t*>(p) + 0x2c) = surfMatId;
    }

    // Register the new entity + hand it to the storage manager.
    int64_t regIBP = *reinterpret_cast<int64_t*>(citizenIBP);
    if (s_fn_88ef0)
        s_fn_88ef0(*reinterpret_cast<int64_t*>(gameWorld + 0xf0), &regIBP, harvestFlag,
                   *reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(d) + 0x20), amount);
    int64_t listIBP = newIBP;
    if (s_fn_c48a0)
        s_fn_c48a0(*reinterpret_cast<void***>(gameWorld + 0xe8), &listIBP,
                   *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(d) + 0x20) + 0x20));

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
