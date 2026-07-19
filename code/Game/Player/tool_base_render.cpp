// code/Game/Player/tool_base_render.cpp
// Entity SELECTION-OUTLINE render chain — faithful clean-room port of the
// HighlightComponent EdgeMesh tessellation family (Application-x64.exe).
//
// This is the "yellow selection outline" that retail draws around a selected/
// followed entity. The select->highlight hookup already exists
// (code/Game/Dialog/edittool_select.cpp resolves s_fn_c9e0 = 0x1400fc9e0 and
// s_fn_cc90 = 0x1400fcc90); previously those were unregistered in standalone so
// the calls no-op'd. This file provides the whole closure so, when the outline
// is enabled (BANISHED_SELECTION_OUTLINE, default OFF — see
// standalone/reg_player_render.cpp), the select path rebuilds and submits a
// terrain-conforming Graphics::EdgeMesh to the live Graphics::Scene.
//
// Functions ported (bottom-up), each verified against the Ghidra decompile AND
// the disassembly (the EdgeMesh::Set 25-arg call + the inlined array-growth
// strides were confirmed instruction-by-instruction):
//   thunk_helper_d250_sub_7080  0x140007080   FindComponent(HighlightComponent) loop
//   TradeWork_impl_sub_74a0     0x1400074a0   16-byte EdgeVertex array grow
//   FUN_1400b10e0               0x1400b10e0   6-corner terrain tile sample (18 floats)
//   TradeWork_impl_b440         0x14004b440   one edge "ribbon" segment (4 EdgeVertex + 6 idx)
//   TradeWork_impl_b930         0x14004b930   the full EdgeMesh builder (outline + fill)
//   Entity_impl_c4a0            0x14004c4a0   rebuild EdgeMeshInstance + (re)add to Scene
//   Entity_impl_cb40            0x1400fcb40   clear highlight (+0x11c=0) over UIComponents
//   Entity_impl_cc90            0x1400fcc90   deselect: cb40 + close the info dialog
//   Entity_impl_c9e0            0x1400fc9e0   select: cb40 + set highlight (+0x11c=1) + repaint
//
// The four inlined growth loops share ONE canonical helper (GrowTo) that is a
// 1:1 transcription of TradeWork_impl_sub_74a0's grow idiom (append-one with the
// count==old+1 -> capacity*3 doubling rule). This deliberately avoids restating
// the decompiler's CONCAT44-packed count/capacity locals as ad-hoc pointer math,
// which was the transposition hazard that got the whole chain DEFERRED before.
//
// Runtime methods go through engine_funcs fn_* pointers (resolved by mangled
// name from Runtime-x64.dll — see engine/funcs/engine_funcs.cpp): EdgeMesh
// ctor/Set/SetMaterials, TriangleVertex/EdgeVertex ctors, EdgeMeshInstance
// SetMesh/ctor, Box ctor/Extend/operator=, Quaternion ctor, Vector3 ctors,
// static MemAlloc/MemFree, Scene Add/RemoveObject, Geometry::SetTransform, Time.
// Exe helpers (already registered) go through direct ResolveAddr(Module::Exe).
//
// STANDALONE liveness notes (both guarded/lazy, so nothing here runs on the
// default byte-clean path — the chain is only registered when the env is set):
//   * The exe overrides the new EdgeMesh's vtable with an exe .rdata vtable
//     (0x140159a60, an EdgeMesh subclass). That address is not valid in the
//     standalone image, so we KEEP the Runtime base EdgeMesh vtable set by the
//     ctor (#ifndef STANDALONE_EXE guards the override). The base vtable is a
//     complete, valid Runtime vtable — adequate for live rendering (we never
//     serialize the mesh, which is the only thing the subclass overrides).
//   * The HighlightComponent is created via COMPONENT_INERT_FACTORY (zeroed), so
//     its embedded EdgeMeshInstance at +0x28 is unconstructed. c4a0 lazily runs
//     the EdgeMeshInstance ctor the first time (when its vtable ptr is null)
//     before any virtual dispatch on it.
//   * c4a0's ModelComponent lookup (retail: Entity_ClearPathQueue 0x140003f60)
//     yields the model-variant indices (+0x164/+0x166) that pick the per-variant
//     prebuilt outline mesh — the path retail uses for CITIZENS (which have no
//     terrain footprint, so the build branch can't fire). The shared registered
//     3f60 reads its type-id from .rdata (DWORD_1401c7a50), NULL in standalone,
//     so it always returned null there -> variant read as (-1,-1) -> build branch
//     -> bail. We do that FindComponent inline (FindModelComp) with the
//     GetTypeIdByName fallback so the citizen's ModelComponent is actually found.
//     (Still gated: whether the citizen HighlightDescription's +0x20 per-variant
//     mesh table loaded in SA is reported by BANISHED_OUTLINE_TRACE's
//     "PREBUILT-branch ... cellMesh=" line.)

#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>

// Gated diagnostic (BANISHED_OUTLINE_TRACE) — reports where the selection-outline
// chain reaches/bails; off unless the env is set.
static int s_outlineTrace = -1;
static inline bool OTrace() {
    if (s_outlineTrace < 0) { const char* e = std::getenv("BANISHED_OUTLINE_TRACE");
                              s_outlineTrace = (e && e[0] && e[0] != '0') ? 1 : 0; }
    return s_outlineTrace != 0;
}
#define OT(...) do { if (OTrace()) { std::fprintf(stderr, "[Outline] " __VA_ARGS__); std::fprintf(stderr, "\n"); } } while(0)

// The whole outline chain is opt-in (BANISHED_SELECTION_OUTLINE) — normally
// gated by NOT registering the exe addresses (standalone/reg_player_render.cpp).
// The per-frame follow re-sync (Impl_HighlightFollow) is instead reached by a
// direct cross-TU call from EditTool_Startup_Impl, so it self-gates on the same
// env var to stay a no-op on the shipping default path.
namespace ComponentFactory { bool LiveEnabled(const char* envVar); }
static int s_outlineEnabled = -1;
static inline bool OutlineEnabled() {
    if (s_outlineEnabled < 0) { s_outlineEnabled = ::ComponentFactory::LiveEnabled("BANISHED_SELECTION_OUTLINE") ? 1 : 0; }
    return s_outlineEnabled != 0;
}

namespace {

// ---- exe .rdata float constants (NULL in standalone -> real-value fallback) ----
constexpr float kCenterBias = 0.5f;      // DAT_140172114 = 0x3f000000

// ---- exe helper functions (registered; resolved as direct exe pointers) -------
typedef float* (*fn_f3e0_t)(void* grid, float* out, const float* in, char flag, void* extra);
typedef void*  (*fn_ibp_out_t)(int64_t obj, void* outIBP);   // FUN_140004680 / _140004740 / ClearPathQueue
typedef void   (*fn_void1_t)(void* self);                    // FUN_1400c3470

fn_f3e0_t     s_f3e0  = nullptr;   // 0x14009f3e0 CharacterComponent_impl_f3e0 (cell->world+height)
fn_ibp_out_t  s_4680  = nullptr;   // 0x140004680 -> entity footprint rect (IBP)
fn_ibp_out_t  s_4740  = nullptr;   // 0x140004740 -> entity component container (IBP)
fn_ibp_out_t  s_3f60  = nullptr;   // 0x140003f60 Entity_ClearPathQueue (misnomer: FindComponent)
fn_void1_t    s_c3470 = nullptr;   // 0x1400c3470 (flag-change notify)

uint32_t s_tid_Highlight = 0;      // DAT_1401c4410 HighlightComponent
uint32_t s_tid_HighlightDesc = 0;  // DAT_1401c4448 HighlightDescription
uint32_t s_tid_Model = 0;          // DWORD_1401c7a50 ModelComponent (c4a0 variant find)
bool     s_resolved = false;

uint32_t ResolveTid(uintptr_t rdataAddr, const char* name, uint32_t* cache) {
    if (*cache) return *cache;
    uint32_t* p = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, rdataAddr));
    if (p && *p) { *cache = *p; return *cache; }
    *cache = ::Object::GetTypeIdByName(name);
    return *cache;
}

void EnsureFuncs() {
    if (s_resolved) return;
    s_f3e0  = reinterpret_cast<fn_f3e0_t>(ResolveAddr(Module::Exe, 0x14009f3e0));
    s_4680  = reinterpret_cast<fn_ibp_out_t>(ResolveAddr(Module::Exe, 0x140004680));
    s_4740  = reinterpret_cast<fn_ibp_out_t>(ResolveAddr(Module::Exe, 0x140004740));
    s_3f60  = reinterpret_cast<fn_ibp_out_t>(ResolveAddr(Module::Exe, 0x140003f60));
    s_c3470 = reinterpret_cast<fn_void1_t>(ResolveAddr(Module::Exe, 0x1400c3470));
    s_resolved = true;
}

inline void CheckPtr(void* p) { if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(p); }

// A growable array with the exact field layout the exe passes by pointer to the
// grow helpers: { void* base @+0, int count @+8, int cap @+0xC }.
struct GrowArr {
    void* base;
    int   count;
    int   cap;
    int   _pad;
    GrowArr() : base(nullptr), count(0), cap(0), _pad(0) {}
};

// GrowTo — 1:1 transcription of TradeWork_impl_sub_74a0 (0x1400074a0), generalized
// over element stride. Appends up to `newCount` elements; new slots [oldCount,
// newCount) are filled from `newElem` (stride bytes), existing slots copied on
// realloc. Capacity rule: when newCount == oldCount+1 and oldCount != 0, grow to
// newCount*3 (amortized). Uses the static MemoryAllocator (fn_MemAllocStatic /
// fn_MemFreeStatic), exactly like the original.
void GrowTo(GrowArr* a, int newCount, const void* newElem, int stride) {
    int oldCount = a->count;
    if (newCount == oldCount) return;
    void* old = a->base;
    a->count = newCount;
    if (a->cap < newCount) {
        a->cap = newCount;
        if (newCount == oldCount + 1 && oldCount != 0) a->cap = newCount * 3;
        a->base = fn_MemAllocStatic ? fn_MemAllocStatic((int64_t)a->cap * stride, 2) : nullptr;
    }
    int start = (old == a->base) ? oldCount : 0;
    for (int i = start; i < newCount; i++) {
        char* dst = static_cast<char*>(a->base) + (int64_t)i * stride;
        if (!dst) continue;
        const void* src = (i < oldCount)
            ? static_cast<const void*>(static_cast<char*>(old) + (int64_t)i * stride)
            : newElem;
        std::memcpy(dst, src, stride);
    }
    if (old != a->base && fn_MemFreeStatic) fn_MemFreeStatic(old);
}

// ===========================================================================
// thunk_helper_d250_sub_7080 (0x140007080)
//   Find the HighlightComponent (type DAT_1401c4410) in the component list at
//   entity+0x30 (array) / +0x38 (count). Writes it (or null) into *outIBP.
// ===========================================================================
void Sub7080(int64_t container, void* outIBP) {
    const uint32_t tid = ResolveTid(0x1401c4410, "HighlightComponent", &s_tid_Highlight);
    void* found = nullptr;
    int count = *reinterpret_cast<int32_t*>(container + 0x38);
    for (int i = 0; i < count; i++) {
        uint64_t* slot = reinterpret_cast<uint64_t*>(
            *reinterpret_cast<uintptr_t*>(container + 0x30) + (int64_t)i * 8);
        CheckPtr(slot);
        void* comp = *reinterpret_cast<void**>(slot);
        uint32_t ct = reinterpret_cast<uint32_t(*)(void*)>(
            *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(comp) + 0x10))(comp);
        if (ct == tid) {
            uint64_t* slot2 = reinterpret_cast<uint64_t*>(
                *reinterpret_cast<uintptr_t*>(container + 0x30) + (int64_t)i * 8);
            CheckPtr(slot2);
            void* comp2 = *reinterpret_cast<void**>(slot2);
            if (fn_IsDerivedFrom && fn_IsDerivedFrom(comp2, tid)) found = comp2;
            break;
        }
    }
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(outIBP, found);
}

// ===========================================================================
// FindComponent(ModelComponent) — the lookup Entity_ClearPathQueue (0x140003f60)
//   performs inside c4a0 to fetch the entity's model-variant indices (+0x164 /
//   +0x166). Ported inline here (mirrors Sub7080) instead of calling the shared
//   registered 3f60 (reg_action_cnr.cpp), because that reads the ModelComponent
//   type-id straight from .rdata (DWORD_1401c7a50), which is NULL in the
//   standalone image -> the shared helper always returns null there. With the
//   GetTypeIdByName fallback the citizen's ModelComponent is actually found, so
//   c4a0 can take the per-variant prebuilt-mesh path (how retail outlines
//   citizens / other model-variant entities that have no terrain footprint).
// ===========================================================================
void FindModelComp(int64_t container, void* outIBP) {
    const uint32_t tid = ResolveTid(0x1401c7a50, "ModelComponent", &s_tid_Model);
    void* found = nullptr;
    int count = *reinterpret_cast<int32_t*>(container + 0x38);
    for (int i = 0; i < count; i++) {
        uint64_t* slot = reinterpret_cast<uint64_t*>(
            *reinterpret_cast<uintptr_t*>(container + 0x30) + (int64_t)i * 8);
        CheckPtr(slot);
        void* comp = *reinterpret_cast<void**>(slot);
        uint32_t ct = reinterpret_cast<uint32_t(*)(void*)>(
            *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(comp) + 0x10))(comp);
        if (ct == tid) {
            CheckPtr(slot);
            void* comp2 = *reinterpret_cast<void**>(slot);
            if (fn_IsDerivedFrom && fn_IsDerivedFrom(comp2, tid)) found = comp2;
            break;
        }
    }
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(outIBP, found);
}

// ===========================================================================
// FUN_1400b10e0 (0x1400b10e0)
//   Sample a 6-corner terrain tile from the heightfield into param_4 (18 floats:
//   6 corners x {x,y,z}). Two parity branches (checkerboard tile orientation).
//   hf fields: +0xc0 dimA, +0xc4 dimB, +0xd0 clampMax, +0xd8 height-sample table.
// ===========================================================================
void SampleTile(int64_t hf, int col, int row, float* out) {
    const float hscale = 0.00390625f;  // DAT_1401720bc = 1/256
    int base = (*reinterpret_cast<int32_t*>(hf + 0xc0) - *reinterpret_cast<int32_t*>(hf + 0xc4)) >> 1;
    int cx = base + col;
    int cy = base + row;
    int cx1 = cx + 1;
    int cy1 = cy + 1;
    float half = (float)(*reinterpret_cast<int32_t*>(hf + 0xc0) >> 1);
    int dim = *reinterpret_cast<int32_t*>(hf + 0xd0);
    float fx  = (float)cx  - half;
    float fy  = (float)cy  - half;
    float fx1 = (float)cx1 - half;
    float fy1 = (float)cy1 - half;
    int16_t* tbl = *reinterpret_cast<int16_t**>(hf + 0xd8);

    auto sample = [&](int a, int b) -> float {
        int ca = a < 0 ? 0 : a; if (dim <= ca) ca = dim - 1;
        int cb = b < 0 ? 0 : b; if (dim <= cb) cb = dim - 1;
        return (float)(int)tbl[(int64_t)dim * cb + ca] * hscale;
    };

    if (((cx + cy) & 1U) == 0) {
        // parity 0: full Vector3 corners (x,y,height)
        out[0]  = fx;  out[1]  = fy;  out[2]  = sample(cx,  cy);
        out[3]  = fx;  out[4]  = fy1; out[5]  = sample(cx,  cy1);
        out[6]  = fx1; out[7]  = fy;  out[8]  = sample(cx1, cy);
        out[9]  = fx1; out[10] = fy;  out[11] = sample(cx1, cy);   // (matches decompile dup)
        out[12] = fx;  out[13] = fy1; out[14] = sample(cx,  cy1);
        out[15] = fx1; out[16] = fy1; out[17] = sample(cx1, cy1);
    } else {
        out[0]  = fx;  out[1]  = fy;  out[2]  = sample(cx,  cy);
        out[3]  = fx1; out[4]  = fy1; out[5]  = sample(cx1, cy1);
        out[6]  = fx1; out[7]  = fy;  out[8]  = sample(cx1, cy);
        out[9]  = fx;  out[10] = fy;  out[11] = sample(cx,  cy);
        out[12] = fx;  out[13] = fy1; out[14] = sample(cx,  cy1);
        out[15] = fx1; out[16] = fy1; out[17] = sample(cx1, cy1);
    }
}

// ===========================================================================
// TradeWork_impl_b440 (0x14004b440)
//   One edge "ribbon" segment between two world points v0,v1: appends 4
//   EdgeVertex (the two endpoints extruded +/-0x200) to `evtx`, then 6 triangle
//   indices {0,1,2,0,2,3} (base-offset by the pre-append vertex count) to `idx`.
// ===========================================================================
void EdgeSegment(const float* v0, const float* v1, GrowArr* idx, GrowArr* evtx) {
    if (!fn_EdgeMesh_TriangleVertex_Ctor || !fn_EdgeMesh_EdgeVertex_Ctor) return;
    int16_t baseIdx = (int16_t)evtx->count;   // sVar2 = (short)param_5[1]

    uint8_t tvA[8], tvB[8], ev[16];
    // edge 1: EdgeVertex(tv(v0), tv(v1), +0x200)
    fn_EdgeMesh_TriangleVertex_Ctor(tvA, v1);
    fn_EdgeMesh_TriangleVertex_Ctor(tvB, v0);
    fn_EdgeMesh_EdgeVertex_Ctor(ev, tvB, tvA, 0x200);
    GrowTo(evtx, evtx->count + 1, ev, 0x10);
    // edge 2: same, -0x200
    fn_EdgeMesh_TriangleVertex_Ctor(tvA, v1);
    fn_EdgeMesh_TriangleVertex_Ctor(tvB, v0);
    fn_EdgeMesh_EdgeVertex_Ctor(ev, tvB, tvA, -0x200);
    GrowTo(evtx, evtx->count + 1, ev, 0x10);
    // edge 3: swapped endpoints, +0x200
    fn_EdgeMesh_TriangleVertex_Ctor(tvA, v0);
    fn_EdgeMesh_TriangleVertex_Ctor(tvB, v1);
    fn_EdgeMesh_EdgeVertex_Ctor(ev, tvB, tvA, 0x200);
    GrowTo(evtx, evtx->count + 1, ev, 0x10);
    // edge 4: swapped, -0x200
    fn_EdgeMesh_TriangleVertex_Ctor(tvA, v0);
    fn_EdgeMesh_TriangleVertex_Ctor(tvB, v1);
    fn_EdgeMesh_EdgeVertex_Ctor(ev, tvB, tvA, -0x200);
    GrowTo(evtx, evtx->count + 1, ev, 0x10);

    static const int16_t offs[6] = {0, 1, 2, 0, 2, 3};
    for (int k = 0; k < 6; k++) {
        int16_t v = (int16_t)(baseIdx + offs[k]);
        GrowTo(idx, idx->count + 1, &v, 2);
    }
}

// convenience: build a world-relative Vector3 (p - origin) into `dst`
inline void RelVec(float* dst, const float* p, float ox, float oy) {
    if (fn_Vector3_Ctor) fn_Vector3_Ctor(dst, p[0] - ox, p[1] - oy, p[2] - 0.0f);
}

// ===========================================================================
// TradeWork_impl_b930 (0x14004b930)
//   Rebuild the HighlightComponent's EdgeMesh (at self+0xf8): a terrain-conforming
//   outline "ribbon" around the entity footprint (loops A/B via EdgeSegment) plus
//   a filled top-face (per-tile 6-corner triangles), then EdgeMesh::Set + materials.
//   self = HighlightComponent*.
// ===========================================================================
void Impl_b930(int64_t self) {
    EnsureFuncs();
    int64_t gw = reinterpret_cast<int64_t>(*g_pGameWorldSingleton);
    int* mapGrid = *reinterpret_cast<int**>(gw + 0xf8);

    // Entity footprint rect (short fields at +0x28 x, +0x2a y, +0x2c w, +0x2e h).
    CheckPtr(reinterpret_cast<void*>(self + 0x10));
    uint64_t rectIBP = 0;
    if (s_4680) s_4680(*reinterpret_cast<int64_t*>(self + 0x10), &rectIBP);
    CheckPtr(&rectIBP);
    int rx = (int)*reinterpret_cast<int16_t*>(rectIBP + 0x28);
    int ry = (int)*reinterpret_cast<int16_t*>(rectIBP + 0x2a);
    int rw = (int)*reinterpret_cast<int16_t*>(rectIBP + 0x2c);
    int rh = (int)*reinterpret_cast<int16_t*>(rectIBP + 0x2e);
    // Defensive (standalone): a malformed footprint rect would make the tessellation
    // loops runaway and freeze. Real building footprints are small; bail on garbage.
    if (rw <= 0 || rh <= 0 || rw > 64 || rh > 64) { OT("b930 SKIP bad rect w=%d h=%d", rw, rh); return; }

    int g = *mapGrid;
    // origin = terrain-world position of the footprint centre cell
    int centerIdx = g * ((rh >> 1) + ry) + (rw >> 1) + rx;
    float bias = (float)(g >> 1) - kCenterBias;
    float centerCell[3], centerWorld[3];
    if (fn_Vector3_Ctor) fn_Vector3_Ctor(centerCell,
        (float)(centerIdx % g) - bias, (float)(centerIdx / g) - bias, 0.0f);
    if (s_f3e0) s_f3e0(mapGrid, centerWorld, centerCell, 0, nullptr);
    float ox = centerWorld[0];
    float oy = centerWorld[1];

    // Quaternion(0,0,0,uv) rotation + centre position -> Transform at self+0x100.
    float uv = g_fUVScale ? *g_fUVScale : 1.0f;
    uint8_t quat[16];
    if (fn_Quaternion_Ctor) fn_Quaternion_Ctor(quat, 0.0f, 0.0f, 0.0f, uv);
    std::memcpy(reinterpret_cast<void*>(self + 0x100), quat, 16);
    *reinterpret_cast<float*>(self + 0x110) = ox;
    *reinterpret_cast<float*>(self + 0x114) = oy;
    *reinterpret_cast<uint32_t*>(self + 0x118) = 0;

    GrowArr edgeIdx;   // local_208 / local_200  (short indices)
    GrowArr edgeVtx;   // local_218 / local_210  (16-byte EdgeVertex)

    auto cellToRel = [&](int idx, float* rel) {
        float cell[3], world[3];
        if (fn_Vector3_Ctor) fn_Vector3_Ctor(cell,
            (float)(idx % g) - (float)(g >> 1), (float)(idx / g) - (float)(g >> 1), 0.0f);
        if (s_f3e0) s_f3e0(mapGrid, world, cell, 0, nullptr);
        RelVec(rel, world, ox, oy);
    };

    // Loop A — top & bottom edges (iterate columns).
    for (int col = rx, n = 0; n < rw; n++, col++) {
        float p1[3], p2[3], p3[3], p4[3];
        cellToRel(g * ry + (col + 1), p1);       // top, col+1
        cellToRel(g * ry + col,       p2);       // top, col
        EdgeSegment(p2, p1, &edgeIdx, &edgeVtx);
        cellToRel(g * (ry + rh) + (col + 1), p3); // bottom, col+1
        cellToRel(g * (ry + rh) + col,       p4); // bottom, col
        EdgeSegment(p4, p3, &edgeIdx, &edgeVtx);
    }
    // Loop B — left & right edges (iterate rows).
    for (int row = ry, n = 0; n < rh; n++, row++) {
        float p1[3], p2[3], p3[3], p4[3];
        cellToRel(g * (row + 1) + rx, p1);        // left, row+1
        cellToRel(g * row + rx,       p2);        // left, row
        EdgeSegment(p2, p1, &edgeIdx, &edgeVtx);
        cellToRel(g * (row + 1) + rx + rw, p3);   // right, row+1
        cellToRel(g * row + rx + rw,       p4);   // right, row
        EdgeSegment(p4, p3, &edgeIdx, &edgeVtx);
    }

    // Fill section — per-tile top-face triangles from the heightfield.
    int64_t hf = *reinterpret_cast<int64_t*>(gw + 0x110);
    GrowArr fillIdx;   // local_1f0 / local_1e8 (short indices)
    GrowArr fillVtx;   // local_268 / local_260 (8-byte TriangleVertex)
    uint8_t box[24];
    if (fn_Box_Ctor) fn_Box_Ctor(box);

    int bottomRow = ry + rh;
    int rightCol  = rx + rw;
    for (int row = ry; row < bottomRow; row++) {
        for (int col = rx; col < rightCol; col++) {
            float scratch[18];
            SampleTile(hf, col, row, scratch);
            const float* pf = &scratch[1];      // pfVar23 = local_c4
            for (int corner = 0; corner < 6; corner++) {
                // 1. append index = current vertex count (before this vertex)
                int16_t idxVal = (int16_t)fillVtx.count;
                GrowTo(&fillIdx, fillIdx.count + 1, &idxVal, 2);
                // 2. build corner Vector3 + TriangleVertex (8 bytes)
                float cv[3];
                if (fn_Vector3_Ctor) fn_Vector3_Ctor(cv, pf[-1] - ox, pf[0] - oy, pf[1] - 0.0f);
                uint8_t tvtx[8];
                if (fn_EdgeMesh_TriangleVertex_Ctor) fn_EdgeMesh_TriangleVertex_Ctor(tvtx, cv);
                // 3. append the TriangleVertex
                GrowTo(&fillVtx, fillVtx.count + 1, tvtx, 8);
                // 4. extend bounding box by the corner
                if (fn_Box_Extend && fn_Box_Assign) {
                    uint8_t tmp[24];
                    fn_Box_Extend(box, tmp, cv);
                    fn_Box_Assign(box, tmp);
                }
                pf += 3;
            }
        }
    }

    // Release the previous EdgeMesh (vtable slot 0 = scalar deleting dtor, arg 1 = free).
    void** oldMesh = *reinterpret_cast<void***>(self + 0xf8);
    if (oldMesh) {
        reinterpret_cast<void(*)(void*, int)>((*reinterpret_cast<void***>(oldMesh))[0])(oldMesh, 1);
        *reinterpret_cast<void**>(self + 0xf8) = nullptr;
    }

    // Allocate + construct a fresh EdgeMesh.
    void* mesh = fn_MemAllocStatic ? fn_MemAllocStatic(0x180, 8) : nullptr;
    if (mesh) {
        if (fn_EdgeMesh_Ctor) fn_EdgeMesh_Ctor(mesh);
    }
    *reinterpret_cast<void**>(self + 0xf8) = mesh;

    // Two GraphicsMesh::TriangleList descriptors { pad, vtxCount@+4, pad, idxCount@+0xc, pad, pad }.
    uint32_t triList0[6] = {0, (uint32_t)fillVtx.count, 0, (uint32_t)fillIdx.count, 0, 0};
    uint32_t triList1[6] = {0, (uint32_t)edgeVtx.count, 0, (uint32_t)edgeIdx.count, 0, 0};

    if (mesh && fn_EdgeMesh_Set) {
        fn_EdgeMesh_Set(
            mesh,
            1, triList0,
            1, triList1,
            // fill stream: short indices + 8-byte TriangleVertex, stride 8
            fillIdx.count, fillIdx.base, fillVtx.count, fillVtx.base, 8,
            0, nullptr, 0, nullptr, 0,
            // edge stream: short indices + 16-byte EdgeVertex, stride 0x10
            edgeIdx.count, edgeIdx.base, edgeVtx.count, edgeVtx.base, 0x10,
            0, nullptr, 0, nullptr, 0,
            box);
    }

    // Materials from the HighlightDescription (self+0x18): +0x30, +0x38 (yellow).
    void* obj = *reinterpret_cast<void**>(self + 0x18);
    const uint32_t tidDesc = ResolveTid(0x1401c4448, "HighlightDescription", &s_tid_HighlightDesc);
    void* desc = (fn_IsDerivedFrom && fn_IsDerivedFrom(obj, tidDesc)) ? obj : nullptr;
    OT("b930 fillVtx=%d fillIdx=%d edgeVtx=%d edgeIdx=%d mesh=%p desc=%p",
       fillVtx.count, fillIdx.count, edgeVtx.count, edgeIdx.count, mesh, desc);
    if (mesh && desc && fn_EdgeMesh_SetMaterials) {
        void* m0 = *reinterpret_cast<void**>(reinterpret_cast<int64_t>(desc) + 0x30);
        void* m1 = *reinterpret_cast<void**>(reinterpret_cast<int64_t>(desc) + 0x38);
        fn_EdgeMesh_SetMaterials(*reinterpret_cast<void**>(self + 0xf8), m0, m1, m0, m1);
    }

    if (fn_MemFreeStatic) {
        fn_MemFreeStatic(fillIdx.base);
        fn_MemFreeStatic(fillVtx.base);
        fn_MemFreeStatic(edgeIdx.base);
        fn_MemFreeStatic(edgeVtx.base);
    }
}

// ===========================================================================
// Entity_impl_c4a0 (0x14004c4a0)
//   Rebuild the HighlightComponent's EdgeMeshInstance and (re)add it to the live
//   Graphics::Scene (gameWorld+0x60). Removes the old scene object first, decides
//   between the built EdgeMesh (self+0xf8, when no per-cell mesh applies) and a
//   per-cell prebuilt mesh from the description table. self = HighlightComponent*.
// ===========================================================================
void Impl_c4a0(int64_t self) {
    EnsureFuncs();
    int64_t gw = reinterpret_cast<int64_t>(*g_pGameWorldSingleton);
    void* scene = *reinterpret_cast<void**>(gw + 0x60);

    // Standalone: the HighlightComponent is created via COMPONENT_INERT_FACTORY
    // (all zeroed), so its embedded EdgeMeshInstance at +0x28 is unconstructed AND
    // its scene-object id at +0x120 is 0 instead of the real ctor's -1. A null +0x28
    // vtable is the "fresh/inert" tell — construct the sub-object and reset +0x120 to
    // -1. (Without the -1, the RemoveObject block below would delete live scene
    // object id 0 — e.g. the storage barn — and corrupt the scene / freeze.)
    if (*reinterpret_cast<void**>(self + 0x28) == nullptr) {
        if (fn_EdgeMeshInstance_Ctor) fn_EdgeMeshInstance_Ctor(reinterpret_cast<void*>(self + 0x28));
        *reinterpret_cast<int32_t*>(self + 0x120) = -1;
    }

    // If we already have a scene object, remove it and clear the "in-scene" flag.
    if ((int)*reinterpret_cast<uint32_t*>(self + 0x120) >= 0) {
        if (fn_SceneRemoveObject) fn_SceneRemoveObject(scene, *reinterpret_cast<uint32_t*>(self + 0x120));
        uint32_t f0 = *reinterpret_cast<uint32_t*>(self + 0x20);
        uint32_t f1 = f0 & 0xfffffffb;
        *reinterpret_cast<uint32_t*>(self + 0x20) = f1;
        if (((f1 ^ f0) & 0xffff) != 0 && *reinterpret_cast<int64_t*>(self + 0x10) != 0) {
            CheckPtr(reinterpret_cast<void*>(self + 0x10));
            if (s_c3470) s_c3470(*reinterpret_cast<void**>(self + 0x10));
        }
    }
    // Release the built EdgeMesh (vtable slot 0).
    {
        void** m = *reinterpret_cast<void***>(self + 0xf8);
        *reinterpret_cast<uint32_t*>(self + 0x120) = 0xffffffff;
        if (m) {
            reinterpret_cast<void(*)(void*, int)>((*reinterpret_cast<void***>(m))[0])(m, 1);
            *reinterpret_cast<void**>(self + 0xf8) = nullptr;
        }
    }
    OT("c4a0 self=%p flag11c=%d id120=%d", (void*)self,
       (int)*reinterpret_cast<char*>(self + 0x11c), *reinterpret_cast<int32_t*>(self + 0x120));
    if (*reinterpret_cast<char*>(self + 0x11c) == 0) return;   // highlight not active

    // Look up per-cell prebuilt mesh indices (charComp +0x164/+0x166) + the
    // "any neighbour already highlighting" test over sibling components.
    uint64_t charIBP = 0, compIBP = 0;
    CheckPtr(reinterpret_cast<void*>(self + 0x10));
    FindModelComp(*reinterpret_cast<int64_t*>(self + 0x10), &charIBP);   // was s_3f60 (null tid in SA)
    CheckPtr(reinterpret_cast<void*>(self + 0x10));
    if (s_4740) s_4740(*reinterpret_cast<int64_t*>(self + 0x10), &compIBP);

    void* ownerObj = *reinterpret_cast<void**>(self + 0x18);
    const uint32_t tidDesc = ResolveTid(0x1401c4448, "HighlightDescription", &s_tid_HighlightDesc);
    void* desc = (fn_IsDerivedFrom && fn_IsDerivedFrom(ownerObj, tidDesc)) ? ownerObj : nullptr;

    int iVar10 = -1, iVar11 = -1;
    if (charIBP != 0) {
        CheckPtr(&charIBP);
        iVar10 = (int)*reinterpret_cast<int16_t*>(charIBP + 0x164);
        CheckPtr(&charIBP);
        iVar11 = (int)*reinterpret_cast<int16_t*>(charIBP + 0x166);
    }

    bool neighbourActive = false;
    if (compIBP != 0) {
        CheckPtr(&compIBP);
        int cnt = *reinterpret_cast<int32_t*>(compIBP + 0x50);
        if (cnt > 0) {
            int64_t off = 0;
            int i = 0;
            do {
                CheckPtr(&compIBP);
                int64_t sib = *reinterpret_cast<int64_t*>(off + *reinterpret_cast<int64_t*>(compIBP + 0x48));
                CheckPtr(&sib);
                uint64_t sibHi = 0;
                Sub7080(sib, &sibHi);
                if (sibHi != 0) {
                    CheckPtr(&sibHi);
                    bool active = !(*reinterpret_cast<char*>(sibHi + 0x11c) == 0 ||
                                    *reinterpret_cast<int32_t*>(sibHi + 0x120) < 0 ||
                                    *reinterpret_cast<int64_t*>(sibHi + 0xf8) != 0);
                    neighbourActive = neighbourActive || active;
                }
                i++; off += 8;
                CheckPtr(&compIBP);
            } while (i < *reinterpret_cast<int32_t*>(compIBP + 0x50));
            if (neighbourActive) goto add_prebuilt;
        }
    }

    OT("c4a0 cellIdx=(%d,%d) desc=%p neighbourActive=%d", iVar10, iVar11, desc, (int)neighbourActive);
    // No per-cell mesh (both indices < 0) -> build+submit our own EdgeMesh.
    if (iVar10 < 0 || iVar11 < 0) {
        CheckPtr(reinterpret_cast<void*>(self + 0x10));
        uint64_t footprintIBP = 0;
        void* r = s_4680 ? s_4680(*reinterpret_cast<int64_t*>(self + 0x10), &footprintIBP) : nullptr;
        void* mat = (r && desc) ? *reinterpret_cast<void**>(reinterpret_cast<int64_t>(desc) + 0x30) : nullptr;
        OT("c4a0 BUILD-branch r=%p rInner=%p desc+0x30(mat)=%p", r,
           r ? *reinterpret_cast<void**>(r) : nullptr, mat);
        if (r && *reinterpret_cast<int64_t*>(r) != 0 &&
            desc && *reinterpret_cast<int64_t*>(reinterpret_cast<int64_t>(desc) + 0x30) != 0) {
            Impl_b930(self);
            if (fn_EdgeMeshInstance_SetMesh)
                fn_EdgeMeshInstance_SetMesh(reinterpret_cast<void*>(self + 0x28),
                                            *reinterpret_cast<void**>(self + 0xf8), nullptr);
            if (fn_Geometry_SetTransform)
                fn_Geometry_SetTransform(reinterpret_cast<void*>(self + 0x28),
                                         reinterpret_cast<void*>(self + 0x100));
            uint8_t time[8];
            if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(time, 0.0f);
            void* inst = reinterpret_cast<void*>(self + 0x28);
            reinterpret_cast<void(*)(void*, void*)>((*reinterpret_cast<void***>(inst))[1])(inst, time);
            if (fn_SceneAddObject)
                *reinterpret_cast<uint32_t*>(self + 0x120) = fn_SceneAddObject(scene, inst);
            OT("c4a0 SUBMITTED own EdgeMesh, sceneId=%u", *reinterpret_cast<uint32_t*>(self + 0x120));
            return;
        }
    }

add_prebuilt:
    // Per-variant prebuilt mesh path (how retail outlines citizens / model-variant
    // entities): mesh = descTable[variantIdx][modelVariant], transform from the
    // ModelComponent (charIBP+0x28). descTable = HighlightDescription+0x20.
    if (iVar10 >= 0 && iVar11 >= 0 && desc) {
        CheckPtr(&charIBP);
        int64_t table = *reinterpret_cast<int64_t*>(reinterpret_cast<int64_t>(desc) + 0x20);
        // Standalone guard: if the HighlightDescription's prebuilt-mesh table (or the
        // selected row/entry) never loaded, bail instead of dereferencing null / OOB.
        // Retail guarantees the table is populated + consistent with the model's
        // variant range; a fresh/inert description in SA may leave it empty.
        int64_t row = table ? *reinterpret_cast<int64_t*>(table + (int64_t)iVar10 * 0x10) : 0;
        void* cellMesh = row ? *reinterpret_cast<void**>(row + (int64_t)iVar11 * 8) : nullptr;
        OT("c4a0 PREBUILT-branch variant=(%d,%d) desc=%p table=%p row=%p cellMesh=%p",
           iVar10, iVar11, desc, (void*)table, (void*)row, cellMesh);
        if (cellMesh == nullptr) return;   // nothing loaded to draw
        if (fn_EdgeMeshInstance_SetMesh)
            fn_EdgeMeshInstance_SetMesh(reinterpret_cast<void*>(self + 0x28), cellMesh,
                                        reinterpret_cast<void*>(charIBP + 0x28));
        uint8_t time[8];
        if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(time, 0.0f);
        void* inst = reinterpret_cast<void*>(self + 0x28);
        reinterpret_cast<void(*)(void*, void*)>((*reinterpret_cast<void***>(inst))[1])(inst, time);
        uint32_t id = fn_SceneAddObject ? fn_SceneAddObject(scene, inst) : 0xffffffff;
        OT("c4a0 PREBUILT SUBMITTED sceneId=%u instVtbl=%p modelXform(charIBP+0x28)=%p",
           id, *reinterpret_cast<void**>(inst), reinterpret_cast<void*>(charIBP + 0x28));
        uint32_t f0 = *reinterpret_cast<uint32_t*>(self + 0x20);
        *reinterpret_cast<uint32_t*>(self + 0x120) = id;
        uint32_t f1 = f0 | 4;
        *reinterpret_cast<uint32_t*>(self + 0x20) = f1;
        if (((f1 ^ f0) & 0xffff) != 0 && *reinterpret_cast<int64_t*>(self + 0x10) != 0) {
            CheckPtr(reinterpret_cast<void*>(self + 0x10));
            if (s_c3470) s_c3470(*reinterpret_cast<void**>(self + 0x10));
        }
    }
}

// ===========================================================================
// Entity_impl_cb40 (0x1400fcb40) — clear highlight on all of the entity's
//   HighlightComponents (set +0x11c = 0, then repaint via c4a0). self = EditTool*.
// ===========================================================================
void Impl_cb40(int64_t self) {
    EnsureFuncs();
    if (*reinterpret_cast<int64_t*>(self + 0x30) == 0) return;

    CheckPtr(reinterpret_cast<void*>(self + 0x30));
    uint64_t contIBP = 0;
    if (s_4740) s_4740(*reinterpret_cast<int64_t*>(self + 0x30), &contIBP);
    if (contIBP != 0) {
        CheckPtr(&contIBP);
        int cnt = *reinterpret_cast<int32_t*>(contIBP + 0x50);
        int64_t off = 0;
        for (int i = 0; i < cnt; i++) {
            CheckPtr(&contIBP);
            int64_t sib = *reinterpret_cast<int64_t*>(off + *reinterpret_cast<int64_t*>(contIBP + 0x48));
            CheckPtr(&sib);
            uint64_t hi = 0;
            Sub7080(sib, &hi);
            if (hi != 0) {
                CheckPtr(&hi);
                if (*reinterpret_cast<char*>(hi + 0x11c) != 0) {
                    *reinterpret_cast<char*>(hi + 0x11c) = 0;
                    Impl_c4a0((int64_t)hi);
                }
            }
            off += 8;
            CheckPtr(&contIBP);
            cnt = *reinterpret_cast<int32_t*>(contIBP + 0x50);
        }
    }
    CheckPtr(reinterpret_cast<void*>(self + 0x30));
    uint64_t hi2 = 0;
    Sub7080(*reinterpret_cast<int64_t*>(self + 0x30), &hi2);
    if (hi2 != 0) {
        CheckPtr(&hi2);
        if (*reinterpret_cast<char*>(hi2 + 0x11c) != 0) {
            *reinterpret_cast<char*>(hi2 + 0x11c) = 0;
            Impl_c4a0((int64_t)hi2);
        }
    }
}

// ===========================================================================
// Entity_impl_c9e0 (0x1400fc9e0) — select finalize: cb40 (clear), then set the
//   highlight (+0x11c = 1) on the entity's HighlightComponents and repaint.
//   self = EditTool*.
// ===========================================================================
void Impl_c9e0(int64_t self) {
    EnsureFuncs();
    OT("c9e0 ENTER self=%p selectedEntity=%p", (void*)self, *reinterpret_cast<void**>(self + 0x30));
    Impl_cb40(self);
    if (*reinterpret_cast<int64_t*>(self + 0x30) == 0) return;

    CheckPtr(reinterpret_cast<void*>(self + 0x30));
    uint64_t contIBP = 0;
    if (s_4740) s_4740(*reinterpret_cast<int64_t*>(self + 0x30), &contIBP);
    if (contIBP != 0) {
        CheckPtr(&contIBP);
        int cnt = *reinterpret_cast<int32_t*>(contIBP + 0x50);
        int64_t off = 0;
        for (int i = 0; i < cnt; i++) {
            CheckPtr(&contIBP);
            int64_t sib = *reinterpret_cast<int64_t*>(off + *reinterpret_cast<int64_t*>(contIBP + 0x48));
            CheckPtr(&sib);
            uint64_t hi = 0;
            Sub7080(sib, &hi);
            if (hi != 0) {
                CheckPtr(&hi);
                if (*reinterpret_cast<char*>(hi + 0x11c) != 1) {
                    *reinterpret_cast<char*>(hi + 0x11c) = 1;
                    Impl_c4a0((int64_t)hi);
                }
            }
            off += 8;
            CheckPtr(&contIBP);
            cnt = *reinterpret_cast<int32_t*>(contIBP + 0x50);
        }
    }
    CheckPtr(reinterpret_cast<void*>(self + 0x30));
    uint64_t hi2 = 0;
    Sub7080(*reinterpret_cast<int64_t*>(self + 0x30), &hi2);
    if (hi2 != 0) {
        CheckPtr(&hi2);
        if (*reinterpret_cast<char*>(hi2 + 0x11c) != 1) {
            *reinterpret_cast<char*>(hi2 + 0x11c) = 1;
            Impl_c4a0((int64_t)hi2);
        }
    }
}

// ===========================================================================
// Entity_impl_cc90 (0x1400fcc90) — DESELECT.
//
// Retail cc90 does THREE things: (1) clear the highlight on the currently-
// selected entity (cb40, reads self+0x30), (2) null the tool's selected-dialog
// (self+0x28) and selected-entity (self+0x30) refs, and (3) close the open
// DialogInstance if its controller derives from ToolUI (DWORD_1401c5788) and
// isn't "pinned" (controller+0x40 == 0). d3b0 (EditTool::SelectEntity) calls it
// BEFORE overwriting self+0x30 with the newly-clicked entity, so cb40 clears the
// PREVIOUS entity's outline — which is exactly the "clear on deselect" fix.
//
// *** STANDALONE: we do ONLY step (1), the highlight-clear. ***
// Steps (2)+(3) — nulling self+0x30 and calling UI::CloseDialog — regress the SA
// select path. Our EditTool picking handler (edittool.cpp:629) re-fires d3b0 as
// the cursor/entities move and guards redundant selects with
// `self->lastEntityRef(+0x30) != foundEntity`. Nulling +0x30 defeats that guard,
// and CloseDialog re-enters the deselect path (dialog OnRemove -> EditTool
// deactivate vtable slot 0x1401ee710 -> cc90) while +0x30 is null — so the
// just-opened info panel gets torn down every frame (observed: citizen dialog
// never stays up; [Outline] trace shows cc90->c9e0 looping on one entity). In SA
// the dialog lifecycle is owned by d3b0/OnKey (step 4 reuses/replaces the panel),
// so the retail teardown here is both redundant and harmful. Keeping just cb40
// gives the outline-clear without disturbing the panel. (This body is reached via
// the registered 0x1400fcc90 under BANISHED_SELECTION_OUTLINE; the original exe cc90
// runs the full teardown unmodified.)
// ===========================================================================
void Impl_cc90(int64_t self) {
    EnsureFuncs();
    OT("cc90 CLEAR-only selectedEntity=%p", *reinterpret_cast<void**>(self + 0x30));
    Impl_cb40(self);   // clear the previous entity's outline; leave dialog/refs alone
}

// ===========================================================================
// HighlightFollow — per-frame transform re-sync so a MOVING selected entity's
//   outline TRACKS it (walking citizens). self = EditTool* (its +0x30 = the
//   currently-selected entity, set by d3b0/edittool_select.cpp:258).
//
// NOT a retail function. Retail relies on the Graphics::Scene ticking
// EdgeMeshInstance::Update (vtable slot [1]) on every scene geometry each frame;
// that Update re-reads the transform from the instance's bound MeshInstance
// (EdgeMeshInstance+0xc8, set by SetMesh's 3rd arg = the ModelComponent's
// MeshInstance at ModelComp+0x28) at +0x84 and Geometry::SetTransform's it — so
// the outline stays glued to the model. In standalone the Graphics::Scene is a
// passive render container: geometry is repositioned EXPLICITLY per frame (the
// citizen move-tick calls Geometry::SetTransform on its own MeshInstance, see
// citizenai_executor.cpp:582). c4a0 calls EdgeMeshInstance::Update only ONCE at
// select, so the outline freezes at the click location while the citizen walks
// away — its skinning still animates because the bone palette lives in the
// shared MeshInstance, but the world transform never re-syncs. This re-runs that
// same Update every frame over the selected entity's active HighlightComponent,
// so the outline follows.
//
// For BUILDING highlights (the c4a0 "build" branch) the instance's bound
// MeshInstance (+0xc8) is null — SetMesh's 3rd arg was 0 — so
// EdgeMeshInstance::Update early-outs; buildings don't move, so this is a
// harmless no-op for them.
// ===========================================================================
void Impl_HighlightFollow(int64_t self) {
    if (!OutlineEnabled()) return;
    EnsureFuncs();
    if (*reinterpret_cast<int64_t*>(self + 0x30) == 0) return;      // nothing selected
    CheckPtr(reinterpret_cast<void*>(self + 0x30));                 // resolve the entity IBP
    int64_t entity = *reinterpret_cast<int64_t*>(self + 0x30);
    // Dead / non-canonical ref (a torn-down entity leaves the -1 sentinel) — Sub7080
    // would deref it. Treat as "nothing to follow" (mirrors edittool.cpp:328).
    if (static_cast<uint64_t>(entity) < 0x10000 ||
        static_cast<uint64_t>(entity) > 0x00007FFFFFFFFFFFULL) return;

    uint64_t hi = 0;
    Sub7080(entity, &hi);                                           // find HighlightComponent
    if (hi == 0) return;
    CheckPtr(&hi);
    if (*reinterpret_cast<char*>(hi + 0x11c) == 0) return;          // highlight not active
    if (*reinterpret_cast<int32_t*>(hi + 0x120) < 0) return;        // not currently in the scene

    void* inst = reinterpret_cast<void*>(hi + 0x28);                // embedded EdgeMeshInstance
    if (*reinterpret_cast<void**>(inst) == nullptr) return;         // unconstructed (inert) — skip

    uint8_t time[8];
    if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(time, 0.0f);
    // vtable slot [1] = EdgeMeshInstance::Update(time): pulls the current transform
    // from the bound MeshInstance (+0xc8 -> +0x84) and Geometry::SetTransform's it.
    reinterpret_cast<void(*)(void*, void*)>((*reinterpret_cast<void***>(inst))[1])(inst, time);

    if (OTrace()) {
        static long s_n = 0;
        void* bound = *reinterpret_cast<void**>(reinterpret_cast<int64_t>(inst) + 0xc8);
        if ((s_n++ % 120) == 0 && bound) {
            // Transform copied by Update starts at boundMesh+0x84; world pos is the
            // trailing xyz (+0x94/+0x98/+0x9c). Confirms the outline is re-syncing.
            float* p = reinterpret_cast<float*>(reinterpret_cast<int64_t>(bound) + 0x94);
            OT("FOLLOW hi=%p boundMesh=%p pos=(%.3f,%.3f,%.3f)", (void*)hi, bound, p[0], p[1], p[2]);
        }
    }
}

}  // namespace

// ===========================================================================
// Exported entry points (registered in standalone/reg_player_render.cpp when
// BANISHED_SELECTION_OUTLINE is set). c9e0/cc90 are what edittool_select.cpp
// resolves: c9e0 sets+paints the outline on the newly-selected entity, cc90
// clears the previous entity's outline and closes its dialog (deselect).
// ===========================================================================
extern "C" void HighlightSelect_Impl(void* self)   { Impl_c9e0(reinterpret_cast<int64_t>(self)); }  // 0x1400fc9e0
extern "C" void HighlightClear_Impl(void* self)     { Impl_cb40(reinterpret_cast<int64_t>(self)); }  // 0x1400fcb40
extern "C" void HighlightRepaint_Impl(void* self)   { Impl_c4a0(reinterpret_cast<int64_t>(self)); }  // 0x14004c4a0
extern "C" void HighlightDeselect_Impl(void* self)  { Impl_cc90(reinterpret_cast<int64_t>(self)); }  // 0x1400fcc90

// Per-frame follow re-sync (NOT an exe address). Called directly from
// EditTool_Startup_Impl (edittool.cpp) every frame; self-gates on
// BANISHED_SELECTION_OUTLINE so it is a no-op unless the outline is enabled.
extern "C" void HighlightFollow_Impl(void* self)    { Impl_HighlightFollow(reinterpret_cast<int64_t>(self)); }
