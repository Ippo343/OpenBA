// code/Game/Component/quarrycomponent_deform.cpp
// The quarry pit — terrain heightmap deformation driven by production progress.
//
// The pit a quarry digs is a real carve of the terrain HEIGHTMAP, not a mesh or a
// decal, and it DEEPENS as the quarry produces. Template/Quarry.rsc:
//
//     QuarryDescription quarry {
//         PickingMesh _floorMesh = "Models/Buildings/Quarry/QuarryFloor.rsc";  // desc +0x20
//         float _maxDepth     = -7.0;                                          // desc +0x2c
//         float _initialDepth = -1.4;                                          // desc +0x28
//     }
//
// Two entry points drive it, both previously unreachable because QuarryComponent was
// spawned by COMPONENT_INERT_FACTORY (shared stub vtable — nothing in it ran):
//
//   slot [12] 0x1400550f0, gated on msg 4 (build complete)
//        -> carve once at _initialDepth
//   slot [15] 0x140054b80, the ConsumeProduce message sink
//        (Quarry.rsc: `ObjectType _messageType = QuarryComponent; int _messageId = 1;`
//         — every completed production cycle sends it)
//        -> re-carve at lerp(_initialDepth, _maxDepth, produced/_maximumProduction)
//           then convert the dug cells' terrain type to 5 (quarried rock)
//
// Same shape as the tunnel TerraformComponent carve, and it reuses that wave's
// heightmap idiom deliberately (see code/Game/Component/terraformcomponent.cpp).
//
// Closure — every callee was already ported AND registered by an earlier wave:
//   0x14000b630 FindComponent<Decal>   (reg_destroy_sm)
//   0x14000b6f0 FindConsumeProduce     (reg_action_child_ctors2)
//   0x140003f60 FindComponent<Model>   (reg_action_cnr)
//   0x140004680 FindComponent<Map>     (reg_component_nature)
//   0x14009f3e0 MapGetTerrainPos       (mapquery)
//   0x1400aaa20 cell-quad ray          (reg_pf_road)   0x1400aa810 ray/tri (reg_pf_geom)
//   0x1400ab150 region recalc          (reg_component_gap_D)
//   0x1400a8c90 fine-cell dirty        (reg_pf_road)   0x1400a6fe0 pf cmd2 (reg_pf_astar)
//
// Decompiled from Application-x64.exe; the slope test and the cell-stamp tail are
// pinned against the disassembly (see the notes at each site).

#include "Component/quarrycomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// ------------------------------------------------------------
// Ported elsewhere; linked by name.
// ------------------------------------------------------------
void  TerraformComponent_RegionRecalc(void* terrainGridV, int x, int y);            // 0x1400ab150
void  Pf_FineCellDirty_a8c90(int64_t self, int x, int y);                           // 0x1400a8c90
void  Pf_EnqueueCmd2_a6fe0(int64_t self, uint32_t idx, void* type, void* p4);       // 0x1400a6fe0
int   Pf_CellQuadRay_aa20(int64_t self, int x, int y, void* p4, float* p5, float* p6,
                          float* p7, float* p8, uint32_t* p9);                      // 0x1400aaa20
extern "C" float* Reimpl_MapGetTerrainPos(void* game, float* outPos3, float* inXY2,
                                          char flag, void* extra);                  // 0x14009f3e0

namespace {

typedef void  (*fn_FindCompOut_t)(void* entity, void* outIbp);
typedef void* (*fn_TransformCopy_t)(void* self, const void* src);
typedef void* (*fn_TransformMul_t)(const void* self, void* retVec3, const void* vec);
typedef void* (*fn_MeshVertices_t)(void* mesh, int lod);
typedef bool  (*fn_IsDerivedFromId_t)(void* self, uint32_t typeId);
typedef void  (*fn_CheckPtrSlot_t)(void* ibp);

fn_FindCompOut_t     s_fn_FindComp_Model = nullptr;   // 0x140003f60
fn_FindCompOut_t     s_fn_FindComp_Map   = nullptr;   // 0x140004680
fn_FindCompOut_t     s_fn_FindComp_Decal = nullptr;   // 0x14000b630
fn_FindCompOut_t     s_fn_FindComp_CP    = nullptr;   // 0x14000b6f0
fn_TransformCopy_t   s_fn_TransformCopy  = nullptr;   // slot 0x140151418
fn_TransformMul_t    s_fn_TransformMul   = nullptr;   // slot 0x140151470
fn_MeshVertices_t    s_fn_MeshVertices   = nullptr;   // slot 0x140151478
fn_IsDerivedFromId_t s_fn_IsDerivedFrom  = nullptr;   // slot 0x140152a98
fn_CheckPtrSlot_t    s_fn_CheckPtr       = nullptr;   // slot 0x140152ab0
bool s_deformInit = false;

// .rdata constants, each verified with read_memory:
//   0x140172100 = 0.25   vertex bias      0x140172114 = 0.5    cell-centre bias
//   0x140172130 = 0.85   slope cutoff     0x140172230 = 256.0  heightmap scale
//   0x140172274 = -0.25  corner offset    0x140172280 = -1.0   ray direction (down)
// Raw .rdata reads are null in standalone unless registered, so each keeps its
// verified literal as the fallback.
float s_bias = 0.25f, s_centreBias = 0.5f, s_slopeCut = 0.85f;
float s_heightScale = 256.0f, s_cornerNeg = -0.25f, s_rayDown = -1.0f;

void QuarryDeformResolve() {
    if (s_deformInit) return;
    s_deformInit = true;
    s_fn_FindComp_Model = reinterpret_cast<fn_FindCompOut_t>(ResolveAddr(Module::Exe, 0x140003f60));
    s_fn_FindComp_Map   = reinterpret_cast<fn_FindCompOut_t>(ResolveAddr(Module::Exe, 0x140004680));
    s_fn_FindComp_Decal = reinterpret_cast<fn_FindCompOut_t>(ResolveAddr(Module::Exe, 0x14000b630));
    s_fn_FindComp_CP    = reinterpret_cast<fn_FindCompOut_t>(ResolveAddr(Module::Exe, 0x14000b6f0));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140151418); if (slot) s_fn_TransformCopy = *reinterpret_cast<fn_TransformCopy_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140151470); if (slot) s_fn_TransformMul  = *reinterpret_cast<fn_TransformMul_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140151478); if (slot) s_fn_MeshVertices  = *reinterpret_cast<fn_MeshVertices_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152a98); if (slot) s_fn_IsDerivedFrom = *reinterpret_cast<fn_IsDerivedFromId_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152ab0); if (slot) s_fn_CheckPtr      = *reinterpret_cast<fn_CheckPtrSlot_t*>(slot);

    struct RF { static void Get(uintptr_t a, float& dst) {
        const float* p = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, a));
        if (p) dst = *p;
    } };
    RF::Get(0x140172100, s_bias);       RF::Get(0x140172114, s_centreBias);
    RF::Get(0x140172130, s_slopeCut);   RF::Get(0x140172230, s_heightScale);
    RF::Get(0x140172274, s_cornerNeg);  RF::Get(0x140172280, s_rayDown);
}

bool QuarryTraceOn() {
    static int s = -1;
    if (s < 0) { const char* e = std::getenv("BANISHED_QUARRY_TRACE"); s = (e && *e && *e != '0') ? 1 : 0; }
    return s != 0;
}

// Every way the carve can silently do nothing gets named once. A deformation that
// never happens leaves no trace otherwise, and every failure mode here is "a pointer
// or type id resolved to zero" — the class of bug that reads as "the feature is
// broken" when it is really "the feature never ran".
void QuarryBail(const char* where, const char* why) {
    static int n = 0;
    if (n >= 12) return;
    ++n;
    std::fprintf(stderr, "[Quarry] %s SKIPPED — %s\n", where, why);
    std::fflush(stderr);
}

void* QuarryOwnerEntity(uintptr_t self) {
    if (s_fn_CheckPtr) s_fn_CheckPtr(reinterpret_cast<void*>(self + 0x10));
    return *reinterpret_cast<void**>(self + 0x10);
}

void* QuarryFindComp(fn_FindCompOut_t finder, void* entity) {
    if (!finder || !entity) return nullptr;
    uintptr_t out[2] = {0, 0};
    finder(entity, out);
    if (!out[0]) return nullptr;
    if (s_fn_CheckPtr) s_fn_CheckPtr(out);
    return reinterpret_cast<void*>(out[0]);
}

// The raw .rdata type-id globals (QuarryDescription 0x1401c4838, ConsumeProduce-
// Description 0x1401c7388) are UNFILLED in standalone — the documented trap where a
// registered-but-zero global makes IsDerivedFrom(x, 0) match nothing, so the find
// silently returns null and the whole carve no-ops. Resolve by NAME first and fall
// back to the raw global only if the name lookup fails.
uint32_t QuarryTypeId(const char* name, uintptr_t rdataAddr) {
    uint32_t id = ::Object::GetTypeIdByName(name);
    if (id != 0) return id;
    const uint32_t* p = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, rdataAddr));
    return p ? *p : 0u;
}

// Retail reaches the QuarryDescription through the owning entity's DESCRIPTION list
// (entity+0x28 -> array +0x28, count +0x30) — NOT the component array at +0x30/+0x38.
// The two lists are one deref apart and picking the wrong one finds nothing.
void* QuarryFindDescription(void* entity) {
    if (!entity || !s_fn_IsDerivedFrom) return nullptr;
    const uint32_t tid = QuarryTypeId("QuarryDescription", 0x1401c4838);
    if (tid == 0) return nullptr;
    const uintptr_t descList = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(entity) + 0x28);
    if (!descList) return nullptr;
    const int n = *reinterpret_cast<int*>(descList + 0x30);
    const uintptr_t arr = *reinterpret_cast<uintptr_t*>(descList + 0x28);
    if (n <= 0 || !arr) return nullptr;
    for (int i = 0; i < n; ++i) {
        void* d = *reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
        if (d && s_fn_IsDerivedFrom(d, tid)) return d;
    }
    return nullptr;
}

// The heightmap edit is bracketed by DecalComponent suspend/resume — the footprint
// decal is projected onto the terrain, so it has to be rebuilt after the ground moves.
// vtable +0x38 = suspend (index 7), +0x30 = resume (index 6).
void* QuarryDecalSuspend(void* entity) {
    void* decal = QuarryFindComp(s_fn_FindComp_Decal, entity);
    if (!decal) return nullptr;
    void** vt = *reinterpret_cast<void***>(decal);
    if (!vt) return nullptr;
    typedef void (*fn_t)(void*);
    fn_t f = reinterpret_cast<fn_t>(vt[7]);
    if (f) f(decal);
    return decal;
}
void QuarryDecalResume(void* decal) {
    if (!decal) return;
    void** vt = *reinterpret_cast<void***>(decal);
    if (!vt) return;
    typedef void (*fn_t)(void*);
    fn_t f = reinterpret_cast<fn_t>(vt[6]);
    if (f) f(decal);
}

}  // namespace

// ============================================================
// QuarryComponent::ApplyDepth (0x1400549c0)
// ============================================================
// Walks the QuarryDescription's _floorMesh, transforms every vertex by the owner's
// ModelComponent placed transform (+0x148), clamps each vertex Z up to `depth`, and
// writes the result into the terrain heightmap.
//
// The clamp is what makes the pit deepen: `depth` starts at _initialDepth (-1.4) and
// falls toward _maxDepth (-7.0), so as it drops, progressively more of the floor
// mesh's low geometry is allowed through and the floor sinks.
extern "C" void QuarryComponent_ApplyDepth(void* selfV, float depth) {
    QuarryDeformResolve();
    const uintptr_t self = reinterpret_cast<uintptr_t>(selfV);

    void* descRaw = *reinterpret_cast<void**>(self + 0x18);
    if (!s_fn_IsDerivedFrom) { QuarryBail("ApplyDepth", "IsDerivedFrom slot 0x140152a98 unresolved"); return; }
    const uint32_t tidDesc = QuarryTypeId("QuarryDescription", 0x1401c4838);
    if (tidDesc == 0) { QuarryBail("ApplyDepth", "QuarryDescription type id is 0"); return; }
    if (!descRaw || !s_fn_IsDerivedFrom(descRaw, tidDesc)) {
        QuarryBail("ApplyDepth", "component +0x18 is not a QuarryDescription"); return;
    }

    void* entity = QuarryOwnerEntity(self);
    void* model  = QuarryFindComp(s_fn_FindComp_Model, entity);
    if (!model) { QuarryBail("ApplyDepth", "owner has no ModelComponent (finder 0x140003f60 returned null)"); return; }
    if (!s_fn_TransformCopy) { QuarryBail("ApplyDepth", "slot 0x140151418 Transform copy-ctor unresolved"); return; }
    if (!s_fn_TransformMul)  { QuarryBail("ApplyDepth", "slot 0x140151470 Transform::operator* unresolved"); return; }
    if (!s_fn_MeshVertices)  { QuarryBail("ApplyDepth", "slot 0x140151478 PickingMesh::Vertices unresolved"); return; }

    // Oversized on purpose so a larger-than-expected Transform cannot clobber the next
    // local (the out-param-buffer-too-small lesson).
    alignas(16) uint8_t xform[0x80] = {};
    s_fn_TransformCopy(xform, reinterpret_cast<const void*>(
        reinterpret_cast<uintptr_t>(model) + 0x148));

    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    const uintptr_t gameWorld   = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
    const uintptr_t terrainGrid = *reinterpret_cast<uintptr_t*>(gameWorld + 0x110);
    if (!terrainGrid) { QuarryBail("ApplyDepth", "gameWorld+0x110 terrain grid null"); return; }

    void* mesh = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(descRaw) + 0x20);
    if (!mesh) { QuarryBail("ApplyDepth", "desc+0x20 _floorMesh is null"); return; }

    const uintptr_t heightArray = *reinterpret_cast<uintptr_t*>(terrainGrid + 0xD8);
    const int       stride      = *reinterpret_cast<int*>(terrainGrid + 0xD0);
    const float     half        = static_cast<float>(*reinterpret_cast<int*>(terrainGrid + 0xC0) >> 1);
    if (!heightArray) { QuarryBail("ApplyDepth", "terrainGrid+0xD8 heightmap null"); return; }

    void* verts = s_fn_MeshVertices(mesh, 0);
    const int vertCount = verts
        ? *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(verts) + 8) : 0;

    for (int i = 0; i < vertCount; ++i) {
        // Re-query per iteration exactly as retail does.
        void* arr = s_fn_MeshVertices(mesh, 0);
        if (!arr) break;
        const uint8_t* data = *reinterpret_cast<const uint8_t**>(arr);
        if (!data) break;
        const void* localVtx = data + static_cast<size_t>(i) * 0xC;

        float world[3] = {0.0f, 0.0f, 0.0f};
        s_fn_TransformMul(xform, world, localVtx);

        float z = world[2];
        if (z < depth) z = depth;          // floor clamp — this IS the progressive dig

        const int mapX = static_cast<int>(world[0] + s_bias + half);
        const int mapY = static_cast<int>(world[1] + s_bias + half);

        *reinterpret_cast<int16_t*>(heightArray +
            (static_cast<int64_t>(stride) * mapY + mapX) * 2) =
            static_cast<int16_t>(static_cast<int>(z * s_heightScale));

        TerraformComponent_RegionRecalc(reinterpret_cast<void*>(terrainGrid), mapX, mapY);
    }

    if (QuarryTraceOn()) {
        std::fprintf(stderr, "[Quarry] ApplyDepth self=%p depth=%.3f mesh=%p verts=%d\n",
                     selfV, depth, mesh, vertCount);
        // Coordinate ground truth: grid dim (+0xC0), stride (+0xD0), and where the
        // first/last vertex actually landed in heightmap cells.
        if (vertCount > 0) {
            void* arr0 = s_fn_MeshVertices(mesh, 0);
            const uint8_t* d0 = arr0 ? *reinterpret_cast<const uint8_t**>(arr0) : nullptr;
            if (d0) {
                const int diagIdx[2] = {0, vertCount - 1};
                for (int i : diagIdx) {
                    float w[3] = {0, 0, 0};
                    s_fn_TransformMul(xform, w, d0 + static_cast<size_t>(i) * 0xC);
                    std::fprintf(stderr,
                        "[Quarry]   diag v%d world=(%.2f,%.2f,%.2f) -> map=(%d,%d) dim(+0xC0)=%d stride=%d half=%.1f\n",
                        i, w[0], w[1], w[2],
                        static_cast<int>(w[0] + s_bias + half), static_cast<int>(w[1] + s_bias + half),
                        *reinterpret_cast<int*>(terrainGrid + 0xC0), stride, half);
                }
            }
        }
        std::fflush(stderr);
    }
}

// ============================================================
// QuarryComponent slot [12] (0x1400550f0) — message sink
// ============================================================
// msg 4 = build complete: carve the pit once at _initialDepth.
extern "C" void QuarryComponent_Slot12(void* selfV, int msgId) {
    if (msgId != 4) return;
    QuarryDeformResolve();
    void* entity = QuarryOwnerEntity(reinterpret_cast<uintptr_t>(selfV));

    void* desc = QuarryFindDescription(entity);
    if (!desc) { QuarryBail("Slot12(msg4)", "no QuarryDescription in the entity description list"); return; }

    const float initialDepth = *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(desc) + 0x28);

    void* decal = QuarryDecalSuspend(entity);
    QuarryComponent_ApplyDepth(selfV, initialDepth);
    QuarryDecalResume(decal);

    if (QuarryTraceOn()) {
        std::fprintf(stderr, "[Quarry] Slot12 msg=4 BUILT self=%p initialDepth=%.3f\n",
                     selfV, initialDepth);
        std::fflush(stderr);
    }
}

// ============================================================
// QuarryComponent slot [15] (0x140054b80) — ConsumeProduce message sink
// ============================================================
// Quarry.rsc wires this up: `_messageType = QuarryComponent; _messageId = 1;`, so every
// completed stone-production cycle lands here. Two halves:
//   1. re-carve the floor at lerp(_initialDepth, _maxDepth, produced/_maximumProduction)
//   2. walk the footprint and convert the dug cells' terrain type to 5 (quarried rock)
extern "C" void QuarryComponent_Slot15(void* selfV) {
    QuarryDeformResolve();
    void* entity = QuarryOwnerEntity(reinterpret_cast<uintptr_t>(selfV));

    // ---- production progress -> depth -------------------------------------------
    void* cp = QuarryFindComp(s_fn_FindComp_CP, entity);
    if (!cp) { QuarryBail("Slot15", "owner has no ConsumeProduceComponent"); return; }
    void* cpDescRaw = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(cp) + 0x18);
    const uint32_t tidCP = QuarryTypeId("ConsumeProduceDescription", 0x1401c7388);
    void* cpDesc = (cpDescRaw && s_fn_IsDerivedFrom && tidCP && s_fn_IsDerivedFrom(cpDescRaw, tidCP))
                 ? cpDescRaw : nullptr;
    if (!cpDesc) { QuarryBail("Slot15", "ConsumeProduce +0x18 is not a ConsumeProduceDescription"); return; }

    void* qDesc = QuarryFindDescription(entity);
    if (!qDesc) { QuarryBail("Slot15", "no QuarryDescription in the entity description list"); return; }

    const int produced = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(cp) + 0x68);
    const int maxProd  = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(cpDesc) + 0x48);
    // Retail divides unguarded; _maximumProduction is authored non-zero on every quarry
    // template. Guarded only because a zero here would be OUR resolution failure.
    if (maxProd == 0) { QuarryBail("Slot15", "cpDesc+0x48 _maximumProduction is 0"); return; }

    const float t            = static_cast<float>(produced) / static_cast<float>(maxProd);
    const float initialDepth = *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(qDesc) + 0x28);
    const float maxDepth     = *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(qDesc) + 0x2c);
    const float depth        = (1.0f - t) * initialDepth + t * maxDepth;

    void* decal = QuarryDecalSuspend(entity);
    QuarryComponent_ApplyDepth(selfV, depth);
    QuarryDecalResume(decal);

    // ---- footprint -> terrain type 5 --------------------------------------------
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    const uintptr_t gameWorld   = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
    const uintptr_t terrainGrid = *reinterpret_cast<uintptr_t*>(gameWorld + 0x110);
    int* tileGrid = *reinterpret_cast<int**>(gameWorld + 0xF8);
    if (!terrainGrid || !tileGrid) { QuarryBail("Slot15", "gameWorld terrain/tile grid null"); return; }

    void* map = QuarryFindComp(s_fn_FindComp_Map, entity);
    if (!map) { QuarryBail("Slot15", "owner has no MapComponent (finder 0x140004680 returned null)"); return; }
    const uintptr_t mapA = reinterpret_cast<uintptr_t>(map);
    const int baseX = *reinterpret_cast<int16_t*>(mapA + 0x28);
    const int baseY = *reinterpret_cast<int16_t*>(mapA + 0x2a);
    const int rectW = *reinterpret_cast<int16_t*>(mapA + 0x2c);
    const int rectH = *reinterpret_cast<int16_t*>(mapA + 0x2e);

    const uintptr_t tileGridA = reinterpret_cast<uintptr_t>(tileGrid);
    const uintptr_t tileData  = *reinterpret_cast<uintptr_t*>(tileGridA + 0x80);
    if (!tileData) { QuarryBail("Slot15", "tileGrid+0x80 cell array null"); return; }
    const int64_t entityPtr   = reinterpret_cast<int64_t>(entity);
    const float   halfTerrain = static_cast<float>(*reinterpret_cast<int*>(terrainGrid + 0xC0) >> 1);

    int converted = 0;
    for (int row = 0; row < rectH; ++row) {
        const int absRow = baseY + row;
        for (int col = 0; col < rectW; ++col) {
            for (int k = 0; k < 4; ++k) {
                // The four quarter-cell corner offsets: (-0.25,-0.25) (0.25,-0.25)
                // (-0.25,0.25) (0.25,0.25). Retail spells these out as a comma-chain
                // that Ghidra renders as one flattened condition.
                const float ox = (k == 1 || k == 3) ? -s_cornerNeg : s_cornerNeg;
                const float oy = (k == 2 || k == 3) ? -s_cornerNeg : s_cornerNeg;

                const int gw = *tileGrid;
                if (gw <= 0) break;
                const int   cellIdx = gw * absRow + baseX + col;
                const float centre  = static_cast<float>(gw >> 1) - s_centreBias;

                float cellPos[3] = { static_cast<float>(cellIdx % gw) - centre,
                                     static_cast<float>(cellIdx / gw) - centre, 0.0f };
                float terrain[3] = { 0.0f, 0.0f, 0.0f };
                Reimpl_MapGetTerrainPos(tileGrid, terrain, cellPos, '\0', nullptr);

                const float px = terrain[0] + ox;
                const float py = terrain[1] + oy;

                const int cellX = static_cast<int>(px + halfTerrain);
                const int cellY = static_cast<int>(py + halfTerrain);

                float dir[3]    = { 0.0f, 0.0f, s_rayDown };      // straight down
                float origin[3] = { px, py, 200.0f };
                float maxT      = 200.0f;
                float outHit[3] = { 0.0f, 0.0f, 0.0f };
                float outT      = 0.0f;
                // p9 is a 3-float out-param and the SLOPE TEST reads p9[2], not the hit
                // point. Pinned to the disassembly: the args set p9 = RBP-0x30, and the
                // test at 0x140054fb9 is `COMISS XMM13,[RBP-0x28] / JBE`, i.e.
                // 0.85 > p9[2]. Retail zeroes p9[2] before every call, so a ray that
                // misses leaves 0.0 and still passes — replicated exactly; do not
                // "improve" this into a hit check.
                float probe[3]  = { 0.0f, 0.0f, 0.0f };

                Pf_CellQuadRay_aa20(static_cast<int64_t>(terrainGrid), cellX, cellY,
                                    dir, origin, &maxT, outHit, &outT,
                                    reinterpret_cast<uint32_t*>(probe));

                if (!(probe[2] < s_slopeCut)) continue;

                uint8_t* cell = reinterpret_cast<uint8_t*>(
                    tileData + static_cast<int64_t>(cellIdx) * 0x30);
                const uint8_t  oldType = cell[0x29];
                const uint32_t oldMask = 1u << (oldType & 0x1f);
                // newType 5 always lands in 0x1e0, so f2f0's two-sided boundary rule
                // reduces to "was the old type in 0xe1f" — which is what retail inlines.
                const uint8_t dirty = ((oldMask & 0xe1f) != 0) ? 1u : 0u;
                *reinterpret_cast<uint8_t*>(tileGridA + 0xa2) |= dirty;
                cell[0x29] = 5;

                if (dirty) {
                    const int64_t regionObj = *reinterpret_cast<int64_t*>(tileGridA + 0x70);
                    if (regionObj) Pf_FineCellDirty_a8c90(regionObj, cellIdx % gw, cellIdx / gw);
                }
                const int64_t pfThread = *reinterpret_cast<int64_t*>(tileGridA + 0x68);
                if (pfThread)
                    Pf_EnqueueCmd2_a6fe0(pfThread, static_cast<uint32_t>(cellIdx),
                                         reinterpret_cast<void*>(static_cast<uintptr_t>(5)), nullptr);

                // cell+0x08 is the ENTITY raster (cell+0x00 is the ZONE raster) — the
                // quarry claims the dug cell.
                *reinterpret_cast<int64_t*>(cell + 8) = entityPtr;
                ++converted;
            }
        }
    }

    if (QuarryTraceOn()) {
        std::fprintf(stderr, "[Quarry] Slot15 self=%p produced=%d/%d t=%.3f depth=%.3f "
                     "rect=(%d,%d %dx%d) converted=%d\n",
                     selfV, produced, maxProd, t, depth, baseX, baseY, rectW, rectH, converted);
        std::fflush(stderr);
    }
}
