// code/Game/Component/terraformcomponent.cpp
// Clean reimplementation of TerraformComponent lifecycle methods from Application-x64.exe.
//
// TerraformComponent (0x48 bytes) — manages terrain modification during construction.
// Fields: +0x28/+0x30/+0x34 = original-terrain undo record, +0x38/+0x40/+0x44 = retyped cells
//
// Vtable at 0x14015a880. Two non-trivial overrides:
//   Shutdown (0x140057500) — Serialize Vector3 array
//   OnRemove (0x140057350) — Restore original terrain heights + tile types
//

#include "Component/terraformcomponent_funcs.h"
#include "Component/TerraformDescription.h"
#include "Component/coll_bridge.h"
#include "engine_funcs.h"
#include "math/Math.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include "io/Stream.h"      // ENDGAME B.3.4 callers batch H: IO::Stream::GetDirection
#include "picking/PickingMesh.h"   // ENDGAME B.3.7 callers (K2)
#include <cstdint>
#include <cstdio>
#include <new>

// ============================================================
// Resolved helpers
// ============================================================

// Terrain helpers for the OnRemove restore + the completion passes
// CitizenAI_impl_b150 (0x1400ab150) — terrain region-dirty recalc around (x,y).
// NOTE: the literal was previously 0x1400a4b50 (a dropped/transposed digit) which
// lands mid-FUN_1400a4b30, an unrelated merchant-name binary search. The true callee
// in TerraformComponent OnRemove is CitizenAI_impl_b150 @ 0x1400ab150.
typedef void (*fn_TerrainRecalc_t)(void* terrainGrid, int x, int y);
static fn_TerrainRecalc_t s_fn_TerrainRecalc = nullptr;

// CitizenAI_impl_a4f0 (0x14001a4f0) — mark one region cell dirty. Sole callee of
// CitizenAI_impl_b150; already registered, so b150 is closure-safe.
typedef void (*fn_RegionDirty_t)(int64_t* regionMgr, void* scratch, uint32_t* cellIdx);
static fn_RegionDirty_t s_fn_RegionDirty = nullptr;

// FUN_1400a8c90 — terrain tile visual update
typedef void (*fn_TerrainVisualUpdate_t)(void* ptr, int x, int y);
static fn_TerrainVisualUpdate_t s_fn_TerrainVisualUpdate = nullptr;

// FUN_1400a6fe0 — terrain type set
typedef void (*fn_TerrainTypeSet_t)(void* thread, int index, int type, uint64_t param4);
static fn_TerrainTypeSet_t s_fn_TerrainTypeSet = nullptr;

static float* s_pHeightScale = nullptr;  // DAT_140172230 == 256.0f

static bool s_initialized = false;

// Resolves the deformation-pass helpers (defined with those passes further down).
static void TerraformDeformResolve();

// 0x140057500 door: registered by address (standalone/reg_terraform_live.cpp).
void TerraformComponent_Shutdown(TerraformComponent* self, void* stream) {
    self->TerraformComponent::Persist(stream);
}


// 0x140057350 door: registered by address (standalone/reg_terraform_live.cpp).
void TerraformComponent_OnRemove(TerraformComponent* self) {
    self->TerraformComponent::OnRemove();
}


// ============================================================
// Init: resolve function pointers
// ============================================================
void TerraformComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_TerrainRecalc = reinterpret_cast<fn_TerrainRecalc_t>(
        ResolveAddr(Module::Exe, 0x1400ab150));
    s_fn_RegionDirty = reinterpret_cast<fn_RegionDirty_t>(
        ResolveAddr(Module::Exe, 0x14001a4f0));
    s_fn_TerrainVisualUpdate = reinterpret_cast<fn_TerrainVisualUpdate_t>(
        ResolveAddr(Module::Exe, 0x1400a8c90));
    s_fn_TerrainTypeSet = reinterpret_cast<fn_TerrainTypeSet_t>(
        ResolveAddr(Module::Exe, 0x1400a6fe0));
    s_pHeightScale = reinterpret_cast<float*>(
        ResolveAddr(Module::Exe, 0x140172230));

    TerraformDeformResolve();

    s_initialized = true;
}

// ============================================================
// Reimpl of FUN_1400ab150 (CitizenAI_impl_b150): mark the region cell(s) that
// contain world tile (x,y) dirty so the region/pathing graph is rebuilt there.
// Region cell size is at terrainGrid+0xc8; grid dimension = (terrainGrid+0xc0)/cell.
// When (x,y) sits on a cell boundary the adjacent cells are flagged too. The region
// manager is at terrainGrid+0xe8. Faithful 1:1 port; closure = CitizenAI_impl_a4f0
// (0x14001a4f0, registered).
void TerraformComponent_RegionRecalc(void* terrainGridV, int x, int y) {
    // Self-resolving: QuarryComponent calls this without going through any other
    // terraform entry point, and an unresolved s_fn_RegionDirty would silently drop
    // every dirty mark — the carve lands in the int16 heightmap but the terrain VB
    // never refreshes, so no pit shows and the decal (which follows the carved,
    // lower heights) is fully depth-rejected behind the stale terrain.
    TerraformComponent_InitFuncs();
    int64_t grid = (int64_t)terrainGridV;
    int cell = *(int*)(grid + 0xc8);
    if (cell == 0) return;
    int gx = x / cell;
    int gy = y / cell;
    int dim = *(int*)(grid + 0xc0) / cell;

    int64_t* regionMgr = (int64_t*)(grid + 0xe8);
    uint64_t scratch = 0;
    uint32_t cellIdx;

    if ((x % cell == 0) && (-1 < gx - 1)) {
        cellIdx = (uint32_t)(dim * gy - 1 + gx);
        if (s_fn_RegionDirty) s_fn_RegionDirty(regionMgr, &scratch, &cellIdx);
    }
    if ((y % cell == 0) && (-1 < gy - 1)) {
        cellIdx = (uint32_t)((gy - 1) * dim + gx);
        if (s_fn_RegionDirty) s_fn_RegionDirty(regionMgr, &scratch, &cellIdx);
    }
    if ((x % cell == 0) && (y % cell == 0) && (-1 < gx - 1) && (-1 < gy - 1)) {
        cellIdx = (uint32_t)((gy - 1) * dim - 1 + gx);
        if (s_fn_RegionDirty) s_fn_RegionDirty(regionMgr, &scratch, &cellIdx);
    }
    cellIdx = (uint32_t)(dim * gy + gx);
    if (s_fn_RegionDirty) s_fn_RegionDirty(regionMgr, &scratch, &cellIdx);
}


// ============================================================
// Terrain-deformation driver (previously unported)
// ============================================================
// Retail dispatch chain for the tunnel mouth:
//
//   TerraformComponent vtable +0x60 (slot 12, 0x140057310) — the per-component
//   MESSAGE slot — fires on msgId == 4 (build complete) and runs three passes:
//
//     1. MeshPass(self, ceiling=true)   0x140056b50 with param_2 = 1
//     2. MeshPass(self, ceiling=false)  0x140056b50 with param_2 = 0
//     3. FootprintPass(self)            0x140057120
//
// MeshPass is the deformation itself. It walks every vertex of a Picking::PickingMesh
// carried by the TerraformDescription, transforms it into world space by the owner's
// ModelComponent transform, and writes the terrain HEIGHTMAP under it. That carve is
// what reads on screen as the dark recess at a tunnel entrance — there is no separate
// "hole" mesh or sprite; the depth is real geometry. (The flat overlay on top is the
// DecalComponent's Terrain/TiledDecals/TunnelMaterial, a different, already-live path.)
//
// The two mesh/mask pairs on TerraformDescription (from Template/TunnelStart.rsc):
//     desc +0x20 = _floorMesh      desc +0x30 = _floorModifyBits
//     desc +0x28 = _ceilingMesh    desc +0x34 = _ceilingModifyBits
// Floor pass lowers terrain where the mesh sits BELOW it (the trench the road runs in);
// ceiling pass raises terrain where the mesh sits ABOVE it (the hill over the mouth).
// Only the ceiling pass records an undo entry — faithful to retail.

#include <cstring>
#include <cstdlib>

typedef void  (*fn_FindCompOut_t)(void* entity, void* outIbp);
typedef float* (*fn_TerrainSample_t)(int* tileGrid, float* out, const float* in,
                                     char flag, void* extra);
typedef void* (*fn_TfMemAlloc_t)(uint64_t size, int align);
typedef void  (*fn_TfMemFree_t)(void* p);
typedef bool  (*fn_IsDerivedFromId_t)(void* self, uint32_t typeId);

static fn_FindCompOut_t     s_fn_FindComp_Model  = nullptr;  // 0x140003f60 (ModelComponent)
static fn_FindCompOut_t     s_fn_FindComp_Map    = nullptr;  // 0x140004680 (MapComponent)
static fn_TerrainSample_t   s_fn_TerrainSample   = nullptr;  // 0x14009f3e0
static fn_TfMemAlloc_t      s_fn_MemAlloc        = nullptr;  // slot 0x140152b60
static fn_TfMemFree_t       s_fn_MemFree         = nullptr;  // slot 0x140152b68
static fn_IsDerivedFromId_t s_fn_IsDerivedFrom   = nullptr;  // slot 0x140152a98
static const float*         s_pVertexBias        = nullptr;  // 0x140172100 == 0.25f
static const float*         s_pCellStep          = nullptr;  // 0x14017214c == 1.0f (g_fUVScale)

static void TerraformDeformResolve() {
    s_fn_FindComp_Model = reinterpret_cast<fn_FindCompOut_t>(ResolveAddr(Module::Exe, 0x140003f60));
    s_fn_FindComp_Map   = reinterpret_cast<fn_FindCompOut_t>(ResolveAddr(Module::Exe, 0x140004680));
    s_fn_TerrainSample  = reinterpret_cast<fn_TerrainSample_t>(ResolveAddr(Module::Exe, 0x14009f3e0));
    s_pVertexBias       = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172100));
    s_pCellStep         = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x14017214c));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152b60);
    if (slot) s_fn_MemAlloc = *reinterpret_cast<fn_TfMemAlloc_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152b68);
    if (slot) s_fn_MemFree = *reinterpret_cast<fn_TfMemFree_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom = *reinterpret_cast<fn_IsDerivedFromId_t*>(slot);
}


// Resolve the component's owning entity (component +0x10 is an InternalBasePtr).
static void* TerraformOwnerEntity(uintptr_t self) {
    return *reinterpret_cast<void**>(self + 0x10);
}

// Both finders return their result through an InternalBasePtr out-param.
static void* TerraformFindComp(fn_FindCompOut_t finder, void* entity) {
    if (!finder || !entity) return nullptr;
    uintptr_t out[2] = {0, 0};
    finder(entity, out);
    if (!out[0]) return nullptr;
    return reinterpret_cast<void*>(out[0]);
}

// Array push_back, byte-faithful to the inlined grow in 0x140056b50 / 0x140057120:
// growth is newCount*3 once the array is non-empty, exact-fit on the first element.
static void TerraformArrayPush(void** pData, int32_t* pCount, int32_t* pCap,
                               const void* elem, size_t elemSize) {
    if (!s_fn_MemAlloc) return;
    const int oldCount = *pCount;
    const int newCount = oldCount + 1;
    void* oldData = *pData;

    *pCount = newCount;
    if (*pCap < newCount) {
        *pCap = (oldCount != 0) ? newCount * 3 : newCount;
        *pData = s_fn_MemAlloc(static_cast<uint64_t>(*pCap) * static_cast<uint64_t>(elemSize), 4);
    }
    // Buffer reused -> only the new tail element is written; reallocated -> copy all.
    int i = (oldData == *pData) ? oldCount : 0;
    for (; i < newCount; ++i) {
        uint8_t* dst = reinterpret_cast<uint8_t*>(*pData) + static_cast<size_t>(i) * elemSize;
        const uint8_t* src = (i < oldCount)
            ? reinterpret_cast<const uint8_t*>(oldData) + static_cast<size_t>(i) * elemSize
            : reinterpret_cast<const uint8_t*>(elem);
        memcpy(dst, src, elemSize);
    }
    if (oldData != *pData && s_fn_MemFree) s_fn_MemFree(oldData);
}

// Terrain type byte of the map cell containing world point (wx, wy). Retail truncates
// toward zero (CVTTSS2SI) after biasing by half the grid, then clamps to the grid.
static uint8_t TerraformCellType(int* tileGrid, float wx, float wy) {
    const int gw = *tileGrid;
    int cx = static_cast<int>(wx + static_cast<float>(gw >> 1));
    int cy = static_cast<int>(wy + static_cast<float>(gw >> 1));
    if (cx < 0) cx = 0;
    if (cx >= gw) cx = gw - 1;
    if (cy < 0) cy = 0;
    if (cy >= gw) cy = gw - 1;
    const uintptr_t tileData = *reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uintptr_t>(tileGrid) + 0x80);
    const int64_t idx = static_cast<int64_t>(gw) * cy + cx;
    return *reinterpret_cast<uint8_t*>(tileData + 0x29 + idx * 0x30);
}

// ------------------------------------------------------------
// 0x140056b50 — one heightmap pass over a TerraformDescription picking mesh.
// ------------------------------------------------------------
void TerraformComponent_MeshPass(TerraformComponent* self, bool ceiling) {
    const uintptr_t selfA = reinterpret_cast<uintptr_t>(self);

    // Every way this pass can silently do nothing is reported once. A deformation that
    // does not happen is invisible in a log otherwise, and the failure modes here are all
    // "a pointer or type id resolved to zero" — the exact class of bug that reads as
    // "the feature is broken" when it is really "the feature never ran".
    auto bail = [&](const char* why) {
        static int n = 0;
        if (n < 8) { ++n;
            fprintf(stderr, "[Terraform] MeshPass(%s) SKIPPED self=%p — %s\n",
                    ceiling ? "ceiling" : "floor", reinterpret_cast<void*>(self), why);
            fflush(stderr);
        }
    };

    // desc = component +0x18, accepted only if it really is a TerraformDescription.
    // Retail derefs the description unguarded (it is always there); the checks below are
    // OUR resolution failures, not retail's, which is why they bail rather than fault.
    void* descRaw = *reinterpret_cast<void**>(selfA + 0x18);
    if (!s_fn_IsDerivedFrom)    { bail("IsDerivedFrom slot 0x140152a98 unresolved"); return; }
    if (!descRaw) { bail("component +0x18 description is null"); return; }
    if (!s_fn_IsDerivedFrom(descRaw, TerraformDescription::TypeId())) {
        bail("description is not a TerraformDescription"); return;
    }
    void* desc = descRaw;

    // The owner's ModelComponent carries the placed transform at +0x148.
    void* model = TerraformFindComp(s_fn_FindComp_Model, TerraformOwnerEntity(selfA));
    if (!model) { bail("owner has no ModelComponent (finder 0x140003f60 returned null)"); return; }
    // Retail copy-constructs a Transform on the stack (??0Transform@Math@@QEAA@AEBV01@@Z,
    // slot 0x140151418) from the ModelComponent's +0x148.
    const Math::Transform xform(*reinterpret_cast<const Math::Transform*>(
        reinterpret_cast<uintptr_t>(model) + 0x148));   // B.3.1 bridge

    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    const uintptr_t gameWorld = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
    const uintptr_t terrainGrid = *reinterpret_cast<uintptr_t*>(gameWorld + 0x110);
    int* tileGrid = *reinterpret_cast<int**>(gameWorld + 0xF8);
    if (!terrainGrid || !tileGrid) { bail("gameWorld terrain/tile grid null"); return; }

    void* mesh = *reinterpret_cast<void**>(
        reinterpret_cast<uintptr_t>(desc) + (ceiling ? 0x28 : 0x20));
    const uint32_t modifyBits = *reinterpret_cast<uint32_t*>(
        reinterpret_cast<uintptr_t>(desc) + (ceiling ? 0x34 : 0x30));
    if (!mesh) { bail(ceiling ? "desc+0x28 _ceilingMesh is null" : "desc+0x20 _floorMesh is null"); return; }

    const float bias        = s_pVertexBias  ? *s_pVertexBias  : 0.25f;
    const float cellStep    = s_pCellStep    ? *s_pCellStep    : 1.0f;
    const float heightScale = s_pHeightScale ? *s_pHeightScale : 256.0f;
    const uint32_t notMask  = ~modifyBits;

    int applied = 0;
    // Picking::PickingMesh::Vertices (import slot 0x140151478) — a reference return.
    void* verts = const_cast<void*>(static_cast<const void*>(
        &reinterpret_cast<const Picking::PickingMesh*>(mesh)->Vertices(0)));   // B.3.7 bridge
    const int vertCount = verts
        ? *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(verts) + 8) : 0;

    for (int i = 0; i < vertCount; ++i) {
        // Re-query per iteration exactly as retail does.
        void* arr = const_cast<void*>(static_cast<const void*>(
            &reinterpret_cast<const Picking::PickingMesh*>(mesh)->Vertices(0)));   // B.3.7 bridge
        if (!arr) break;
        const uint8_t* data = *reinterpret_cast<const uint8_t**>(arr);
        const void* localVtx = data + static_cast<size_t>(i) * 0xC;

        // Transform::operator*(const Vector3&) — slot 0x140151470.
        const Math::Vector3 world =
            xform * *reinterpret_cast<const Math::Vector3*>(localVtx);   // B.3.1 bridge

        // p = transformed vertex, nudged a quarter cell so it lands inside a cell
        // rather than exactly on the shared corner.
        const float p[3] = { world.x + bias, world.y + bias, world.z + 0.0f };

        // Original terrain point under p: (x, y, currentHeight). This is the undo record.
        float original[3] = {0.0f, 0.0f, 0.0f};
        if (s_fn_TerrainSample) s_fn_TerrainSample(tileGrid, original, p, '\0', nullptr);

        // Every cell sharing this heightmap vertex must be a terrain type the mask allows.
        // NOTE: retail samples (x,y), (x-1,y), (x-1,y-1) and then (x-1,y) AGAIN — the
        // fourth sample repeats the second instead of testing (x,y-1). Verified in the
        // disassembly at 0x140056e60..0x140056e77 (MOVSS [RSP+0x30]; SUBSS XMM1,XMM7),
        // identical to the second sample's 0x140056d3d. Replicated as-is: "fixing" it
        // would widen the mask test and deform cells retail leaves alone.
        uint32_t bits = 0;
        bits |= 1u << (TerraformCellType(tileGrid, p[0],            p[1])            & 0x1F);
        bits |= 1u << (TerraformCellType(tileGrid, p[0] - cellStep, p[1])            & 0x1F);
        bits |= 1u << (TerraformCellType(tileGrid, p[0] - cellStep, p[1] - cellStep) & 0x1F);
        bits |= 1u << (TerraformCellType(tileGrid, p[0] - cellStep, p[1])            & 0x1F);
        if ((notMask & bits) != 0) continue;

        // Ceiling raises terrain up to the mesh; floor lowers terrain down to it.
        const bool apply = ceiling ? (original[2] < p[2]) : (p[2] < original[2]);
        if (!apply) continue;

        const float half = static_cast<float>(*reinterpret_cast<int*>(terrainGrid + 0xC0) >> 1);
        const int mapX = static_cast<int>(p[0] + half);
        const int mapY = static_cast<int>(p[1] + half);

        const uintptr_t heightArray = *reinterpret_cast<uintptr_t*>(terrainGrid + 0xD8);
        const int stride = *reinterpret_cast<int*>(terrainGrid + 0xD0);
        *reinterpret_cast<int16_t*>(heightArray +
            (static_cast<int64_t>(stride) * mapY + mapX) * 2) =
            static_cast<int16_t>(static_cast<int>(p[2] * heightScale));

        if (s_fn_TerrainRecalc)
            s_fn_TerrainRecalc(reinterpret_cast<void*>(terrainGrid), mapX, mapY);
        ++applied;

        if (ceiling) {
            TerraformArrayPush(&self->heightRecord, &self->heightCount, &self->heightCap,
                               original, 0xC);
        }
    }

}

// ------------------------------------------------------------
// 0x140057120 — footprint terrain-type pass.
// Over the owner's MapComponent footprint rect, every cell currently of terrain type
// 10 becomes type 6, and its index is recorded so OnRemove can put it back.
// ------------------------------------------------------------
void TerraformComponent_FootprintPass(TerraformComponent* self) {
    const uintptr_t selfA = reinterpret_cast<uintptr_t>(self);

    void* mapComp = TerraformFindComp(s_fn_FindComp_Map, TerraformOwnerEntity(selfA));
    if (!mapComp) return;
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    int* tileGrid = *reinterpret_cast<int**>(
        reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xF8);
    if (!tileGrid) return;

    const uintptr_t m = reinterpret_cast<uintptr_t>(mapComp);
    const int x0 = *reinterpret_cast<int16_t*>(m + 0x28);
    const int y0 = *reinterpret_cast<int16_t*>(m + 0x2A);
    const int w  = *reinterpret_cast<int16_t*>(m + 0x2C);
    const int h  = *reinterpret_cast<int16_t*>(m + 0x2E);

    const uintptr_t tileData = *reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uintptr_t>(tileGrid) + 0x80);
    int converted = 0;

    for (int y = y0; y < y0 + h; ++y) {
        for (int x = x0; x < x0 + w; ++x) {
            const int idx = *tileGrid * y + x;
            uint8_t* type = reinterpret_cast<uint8_t*>(
                tileData + 0x29 + static_cast<int64_t>(idx) * 0x30);
            if (*type != 10) continue;

            *reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(tileGrid) + 0xA2) |= 1;
            *type = 6;
            if (s_fn_TerrainVisualUpdate) {
                s_fn_TerrainVisualUpdate(*reinterpret_cast<void**>(
                        reinterpret_cast<uintptr_t>(tileGrid) + 0x70),
                    idx % *tileGrid, idx / *tileGrid);
            }
            if (s_fn_TerrainTypeSet) {
                s_fn_TerrainTypeSet(*reinterpret_cast<void**>(
                        reinterpret_cast<uintptr_t>(tileGrid) + 0x68),
                    idx, 6, 0);
            }
            TerraformArrayPush(&self->tileRecord, &self->tileCount, &self->tileCap, &idx, 4);
            ++converted;
        }
    }

}

// 0x140057310 door: registered by address (standalone/reg_terraform_live.cpp).
extern "C" void TerraformComponent_Slot12(void* selfV, int msgId) {
    if (!selfV) return;
    reinterpret_cast<TerraformComponent*>(selfV)->TerraformComponent::OnEvent(msgId);   // B.3.8 bridge
}


// ============================================================================
// ENDGAME B.3.8 VIS — TerraformComponent as a REAL class (TerraformComponent.h).
//
// Before: a 48-slot TerraformComponent_LiveVtable re-filled at every create
// (InitInertTyped + InstallLive), Inert_ScalarDtor in slot 0. After: the
// compiler's 16-slot vtable and a real ctor/dtor chain (B38_RULES D1, D2).
//   [3]  Shutdown/Serialize  0x140057500 — persists the undo record
//   [7]  OnRemove            0x140057350 — replays the undo record
//   [12] Message             0x140057310 — applies the deformation on msg 4
// ============================================================================
#include "Object/factory_registration.h"
#include "Component/TerraformComponent.h"

// 0x140057620: Component() is the CALL 0x1401188b0; the 0x14015a880 stamp is
// the compiler's; +0x28/+0x30/+0x38/+0x40 = 0; flags |= 3 (0x83).
TerraformComponent::TerraformComponent()
    : heightRecord(nullptr), heightCount(0), heightCap(0),
      tileRecord(nullptr), tileCount(0), tileCap(0) {
    OrFlagsInCtor(0x3);
}

// ============================================================
// TerraformComponent::Persist (0x140057500)
// ============================================================

// [3] 0x140057500 — the body resolves its helpers first (slot 3 is the first
// call into the class on a load-from-save).
void TerraformComponent::Persist(void* stream) {
    TerraformComponent_InitFuncs();
    uintptr_t selfAddr = reinterpret_cast<uintptr_t>(this);

    Component::Persist(stream);   // retail: CALL 0x1401189b0 (qualified)

    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize Vector3 count from +0x30
    uint32_t count = *(uint32_t*)(selfAddr + 0x30);
    uint32_t localCount = count;
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(&localCount), nullptr);   // B.3.4 bridge

    // If loading, resize the Vector3 array
    // B.3.2 bridge: FUN_14000d030 is Collection::Array<Vector3>::SetSize(n)
    if (static_cast<int>(reinterpret_cast<IO::Stream*>(stream)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Stream::GetDirection, retail 0x180006200)
        GameColl::AsArray<GameColl::Vec3>((void*)(selfAddr + 0x28))
            .SetSize(static_cast<int>(localCount));
    }

    // Serialize each Vector3 (12 bytes each)
    void* vecArray = *(void**)(selfAddr + 0x28);
    if ((int)localCount > 0) {
        uint8_t* pVec = (uint8_t*)vecArray;
        for (uint32_t i = 0; i < localCount; i++) {
            reinterpret_cast<Math::Vector3*>(pVec)->Serialize(serializer);   // B.3.1 bridge
            pVec += 0xC;  // sizeof(Vector3) = 12
        }
    }

    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}
// [6] 0x1400574f0 — the class's own `RET` body.
void TerraformComponent::OnAdded() {}
// ============================================================
// TerraformComponent OnRemove — vtable slot 7 (0x140057350)
// ============================================================
// Ghidra labels this OnCreate; it is the TEARDOWN half. It REPLAYS the undo record
// this component built at build-complete: it writes the ORIGINAL terrain heights back
// from the Vector3 array at +0x28, and puts every recorded map cell's terrain type
// back to 10 (the value the footprint pass converted to 6). Applying it at creation
// would restore terrain that was never deformed; the "OnCreate/OnRemove off by one"
// Ghidra mislabel is a known trap in this project, and the direction of the writes
// (10 here vs 10 -> 6 in the completion pass) is what settles it.
// [7] 0x140057350
void TerraformComponent::OnRemove() {
    TerraformComponent_InitFuncs();
    uintptr_t selfAddr = reinterpret_cast<uintptr_t>(this);

    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;

    float heightScale = s_pHeightScale ? *s_pHeightScale : 256.0f;

    // Phase 1: Apply Vector3 terrain heights
    uintptr_t terrainGrid = *(uintptr_t*)(gameWorld + 0x110);
    int vecCount = *(int*)(selfAddr + 0x30);

    if (vecCount > 0 && terrainGrid) {
        uintptr_t vecArray = *(uintptr_t*)(selfAddr + 0x28);
        float halfSize = (float)(*(int*)(terrainGrid + 0xC0) >> 1);

        for (int i = 0; i < vecCount; i++) {
            uintptr_t vec = vecArray + (uintptr_t)i * 0xC;
            float vx = *(float*)vec;
            float vy = *(float*)(vec + 4);
            float vz = *(float*)(vec + 8);

            // Offset by half grid size
            float mapX = halfSize + vx;
            float mapY = halfSize + vy;

            // Convert Z to height index
            uint32_t heightIdx = (uint32_t)(vz * heightScale);

            // Write height to terrain height array
            uintptr_t heightArray = *(uintptr_t*)(terrainGrid + 0xD8);
            int stride = *(int*)(terrainGrid + 0xD0);
            int64_t offset = ((int64_t)stride * (int64_t)(int)mapY + (int64_t)(int)mapX) * 2;
            *(int16_t*)(heightArray + offset) = (int16_t)heightIdx;

            // Recalculate terrain at this position
            if (s_fn_TerrainRecalc)
                s_fn_TerrainRecalc((void*)terrainGrid, (int)mapX, (int)mapY);
        }
    }

    // Phase 2: Update tile types from tile index array at +0x38/+0x40
    int* tileGrid = *(int**)(gameWorld + 0xF8);
    int tileCount = *(int*)(selfAddr + 0x40);

    if (tileCount > 0 && tileGrid) {
        uintptr_t tileArray = *(uintptr_t*)(selfAddr + 0x38);
        int gridWidth = *tileGrid;

        for (int i = 0; i < tileCount; i++) {
            int tileIndex = *(int*)(tileArray + (uintptr_t)i * 4);

            // Read current terrain type
            uintptr_t tileData = *(uintptr_t*)((uintptr_t)tileGrid + 0x80);
            uint8_t oldType = *(uint8_t*)(tileData + 0x29 + (int64_t)tileIndex * 0x30);

            // Update buildable flag
            bool isBuildable = ((1 << (oldType & 0x1F)) & 0x1E0) != 0;
            *(uint8_t*)((uintptr_t)tileGrid + 0xA2) |= (isBuildable ? 1 : 0);

            // Set tile type to 10 (terraformed)
            *(uint8_t*)(tileData + 0x29 + (int64_t)tileIndex * 0x30) = 10;

            // Update terrain visuals if was buildable
            if (isBuildable && s_fn_TerrainVisualUpdate) {
                uintptr_t visualPtr = *(uintptr_t*)((uintptr_t)tileGrid + 0x70);
                s_fn_TerrainVisualUpdate((void*)visualPtr, tileIndex % gridWidth, tileIndex / gridWidth);
            }

            // Set terrain type
            if (s_fn_TerrainTypeSet) {
                uintptr_t threadPtr = *(uintptr_t*)((uintptr_t)tileGrid + 0x68);
                s_fn_TerrainTypeSet((void*)threadPtr, tileIndex, 10, 0);
            }
        }
    }
}
// ------------------------------------------------------------
// 0x140057310 — vtable slot 12, the per-component MESSAGE slot.
// Message id 4 is the build-complete broadcast (the same one PastureComponent's
// slot 12 gates on). Everything else is ignored.
// ------------------------------------------------------------
void TerraformComponent::OnEvent(int msgId) {
    if (msgId != 4) return;
    TerraformComponent_InitFuncs();
    TerraformComponent_MeshPass(this, /*ceiling=*/true);
    TerraformComponent_MeshPass(this, /*ceiling=*/false);
    TerraformComponent_FootprintPass(this);
}

// [0] 0x14000ce90: ~TerraformComponent (0x1400575b0; its array frees are a PORT GAP, see the header), MemFree on &1.
Object::Base* TerraformComponent::DeletingDtor(unsigned flags) {
    this->~TerraformComponent();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x14000cc90: MemAlloc(0x48, 8), then the ctor.
void* TerraformComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(TerraformComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) TerraformComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* TerraformComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) TerraformComponent());
}

DEFINE_EXE_VTABLE(TerraformComponent, "_ZTV18TerraformComponent")
DEFINE_EXE_TYPE(TerraformComponent, "Component")
