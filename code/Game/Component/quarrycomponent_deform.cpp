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

#include "Component/QuarryComponent.h"
#include "Component/ConsumeProduceDescription.h"
#include "Component/QuarryDescription.h"
#include "object/TypeInfo.h"
#include "Game/exe_bodies.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "math/Math.h"
#include "perf_terrain.h"
#include "Object/factory_registration.h"
#include "picking/PickingMesh.h"   // ENDGAME B.3.7 callers (K2)
#include "Component/exe_bodies.h"   // Destroy_FindDecalComponent_b630, FloorComponent_FindResData
#include "Action/exe_bodies.h"      // ActCNR_FindComponent_3f60, Entity_FindConsumeProduce_b6f0
#include "Game/exe_constants.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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

// The four component finders (entity, outIbp) this file uses, by retail address.
void FindModel(void* e, void* out) { ActCNR_FindComponent_3f60(reinterpret_cast<uintptr_t>(e), out); }        // 0x140003f60
void FindMap(void* e, void* out)   { FloorComponent_FindResData(e, out); }                                       // 0x140004680
void FindDecal(void* e, void* out) { Destroy_FindDecalComponent_b630(reinterpret_cast<uintptr_t>(e), out); }  // 0x14000b630
void FindCP(void* e, void* out)    { Entity_FindConsumeProduce_b6f0(reinterpret_cast<int64_t>(e), out); }     // 0x14000b6f0

// ?IsDerivedFrom@Base@Object@@ (import slot 0x140152a98).
bool IsDerived(void* p, uint32_t tid) {
    return reinterpret_cast<const Object::Base*>(p)->IsDerivedFrom(tid);   // B.3.x bridge
}

float RdataF32(uint32_t bits) { float f; std::memcpy(&f, &bits, sizeof f); return f; }

// .rdata constants:
//   0x140172100 = 0.25   vertex bias      0x140172114 = 0.5    cell-centre bias
//   0x140172230 = 256.0  heightmap scale  0x140172280 = -1.0   ray direction (down)
const float s_bias = kRdata_140172100, s_centreBias = kRdata_140172114;
const float s_heightScale = RdataF32(kRdata_140172230), s_rayDown = kRdata_140172280;
// PORT GAP 0x140172130 slope cutoff: not registered; retail reads the .rdata float
// here. The literal below is the value this file has always used (0.85, noted as
// read with read_memory, not re-checked against the image).
const float s_slopeCut = 0.85f;
// PORT GAP 0x140172274 corner offset: not registered; retail reads the .rdata float
// here. The literal below is the value this file has always used (-0.25, same note).
const float s_cornerNeg = -0.25f;


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
    return *reinterpret_cast<void**>(self + 0x10);
}

void* QuarryFindComp(void (*finder)(void*, void*), void* entity) {
    if (!entity) return nullptr;
    uintptr_t out[2] = {0, 0};
    finder(entity, out);
    if (!out[0]) return nullptr;
    return reinterpret_cast<void*>(out[0]);
}


// Retail reaches the QuarryDescription through the owning entity's DESCRIPTION list
// (entity+0x28 -> array +0x28, count +0x30) — NOT the component array at +0x30/+0x38.
// The two lists are one deref apart and picking the wrong one finds nothing.
void* QuarryFindDescription(void* entity) {
    if (!entity) return nullptr;
    const uint32_t tid = QuarryDescription::TypeId();
    if (tid == 0) return nullptr;
    const uintptr_t descList = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(entity) + 0x28);
    if (!descList) return nullptr;
    const int n = *reinterpret_cast<int*>(descList + 0x30);
    const uintptr_t arr = *reinterpret_cast<uintptr_t*>(descList + 0x28);
    if (n <= 0 || !arr) return nullptr;
    for (int i = 0; i < n; ++i) {
        void* d = *reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
        if (d && IsDerived(d, tid)) return d;
    }
    return nullptr;
}

// The heightmap edit is bracketed by DecalComponent suspend/resume — the footprint
// decal is projected onto the terrain, so it has to be rebuilt after the ground moves.
// vtable +0x38 = suspend (index 7), +0x30 = resume (index 6).
void* QuarryDecalSuspend(void* entity) {
    void* decal = QuarryFindComp(FindDecal, entity);
    if (!decal) return nullptr;
    reinterpret_cast<Component*>(decal)->OnRemove();   // B.3.8 bridge (slot 7; the DecalComponent)
    return decal;
}
void QuarryDecalResume(void* decal) {
    if (!decal) return;
    reinterpret_cast<Component*>(decal)->OnAdded();   // B.3.8 bridge (slot 6; the DecalComponent)
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
    const uintptr_t self = reinterpret_cast<uintptr_t>(selfV);

    void* descRaw = *reinterpret_cast<void**>(self + 0x18);
    const uint32_t tidDesc = QuarryDescription::TypeId();
    if (tidDesc == 0) { QuarryBail("ApplyDepth", "QuarryDescription type id is 0"); return; }
    if (!descRaw || !IsDerived(descRaw, tidDesc)) {
        QuarryBail("ApplyDepth", "component +0x18 is not a QuarryDescription"); return;
    }

    void* entity = QuarryOwnerEntity(self);
    void* model  = QuarryFindComp(FindModel, entity);
    if (!model) { QuarryBail("ApplyDepth", "owner has no ModelComponent (finder 0x140003f60 returned null)"); return; }

    // Retail copy-constructs a Transform on the stack (??0Transform@Math@@QEAA@AEBV01@@Z,
    // slot 0x140151418) from the ModelComponent's +0x148.
    const Math::Transform xform(*reinterpret_cast<const Math::Transform*>(
        reinterpret_cast<uintptr_t>(model) + 0x148));   // B.3.1 bridge

    if (GameUpdateLoop::_singleton == nullptr) return;
    const uintptr_t gameWorld   = reinterpret_cast<uintptr_t>(GameUpdateLoop::_singleton);
    const uintptr_t terrainGrid = *reinterpret_cast<uintptr_t*>(gameWorld + 0x110);
    if (!terrainGrid) { QuarryBail("ApplyDepth", "gameWorld+0x110 terrain grid null"); return; }

    void* mesh = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(descRaw) + 0x20);
    if (!mesh) { QuarryBail("ApplyDepth", "desc+0x20 _floorMesh is null"); return; }

    const uintptr_t heightArray = *reinterpret_cast<uintptr_t*>(terrainGrid + 0xD8);
    const int       stride      = *reinterpret_cast<int*>(terrainGrid + 0xD0);
    const float     half        = static_cast<float>(*reinterpret_cast<int*>(terrainGrid + 0xC0) >> 1);
    if (!heightArray) { QuarryBail("ApplyDepth", "terrainGrid+0xD8 heightmap null"); return; }

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
        if (!data) break;
        const void* localVtx = data + static_cast<size_t>(i) * 0xC;

        // Transform::operator*(const Vector3&) — slot 0x140151470.
        const Math::Vector3 world =
            xform * *reinterpret_cast<const Math::Vector3*>(localVtx);   // B.3.1 bridge

        float z = world.z;
        if (z < depth) z = depth;          // floor clamp — this IS the progressive dig

        const int mapX = static_cast<int>(world.x + s_bias + half);
        const int mapY = static_cast<int>(world.y + s_bias + half);

        *reinterpret_cast<int16_t*>(heightArray +
            (static_cast<int64_t>(stride) * mapY + mapX) * 2) =
            static_cast<int16_t>(static_cast<int>(z * s_heightScale));

        TerraformComponent_RegionRecalc(reinterpret_cast<void*>(terrainGrid), mapX, mapY);
    }

}

// ============================================================
// QuarryComponent slot [12] (0x1400550f0) — message sink
// ============================================================
// msg 4 = build complete: carve the pit once at _initialDepth.
void QuarryComponent::OnEvent(int msgId) {
    if (msgId != 4) return;
    void* entity = QuarryOwnerEntity(reinterpret_cast<uintptr_t>(this));

    void* desc = QuarryFindDescription(entity);
    if (!desc) { QuarryBail("Slot12(msg4)", "no QuarryDescription in the entity description list"); return; }

    const float initialDepth = *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(desc) + 0x28);

    void* decal = QuarryDecalSuspend(entity);
    QuarryComponent_ApplyDepth(this, initialDepth);
    QuarryDecalResume(decal);

}

// 0x1400550f0 door: registered by address (standalone/reg_quarry_live.cpp).
extern "C" void QuarryComponent_Slot12(void* selfV, int msgId) {
    reinterpret_cast<QuarryComponent*>(selfV)->QuarryComponent::OnEvent(msgId);   // B.3.8 bridge
}

// ============================================================
// QuarryComponent slot [15] (0x140054b80) — ConsumeProduce message sink
// ============================================================
// Quarry.rsc wires this up: `_messageType = QuarryComponent; _messageId = 1;`, so every
// completed stone-production cycle lands here. Two halves:
//   1. re-carve the floor at lerp(_initialDepth, _maxDepth, produced/_maximumProduction)
//   2. walk the footprint and convert the dug cells' terrain type to 5 (quarried rock)
void QuarryComponent::OnMessage(int /*message*/, void* /*data*/, uint32_t /*arg0*/, uint32_t /*arg1*/) {
    void* entity = QuarryOwnerEntity(reinterpret_cast<uintptr_t>(this));

    // ---- production progress -> depth -------------------------------------------
    void* cp = QuarryFindComp(FindCP, entity);
    if (!cp) { QuarryBail("Slot15", "owner has no ConsumeProduceComponent"); return; }
    void* cpDescRaw = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(cp) + 0x18);
    const uint32_t tidCP = ConsumeProduceDescription::TypeId();
    void* cpDesc = (cpDescRaw && tidCP && IsDerived(cpDescRaw, tidCP))
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
    QuarryComponent_ApplyDepth(this, depth);
    QuarryDecalResume(decal);

    // ---- footprint -> terrain type 5 --------------------------------------------
    if (GameUpdateLoop::_singleton == nullptr) return;
    const uintptr_t gameWorld   = reinterpret_cast<uintptr_t>(GameUpdateLoop::_singleton);
    const uintptr_t terrainGrid = *reinterpret_cast<uintptr_t*>(gameWorld + 0x110);
    int* tileGrid = *reinterpret_cast<int**>(gameWorld + 0xF8);
    if (!terrainGrid || !tileGrid) { QuarryBail("Slot15", "gameWorld terrain/tile grid null"); return; }

    void* map = QuarryFindComp(FindMap, entity);
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
                PerfX::TerrainNoteWrite(cell);   // [PerfX] mirror

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

}

// 0x140054b80 door: registered by address (standalone/reg_quarry_live.cpp).
extern "C" void QuarryComponent_Slot15(void* selfV) {
    reinterpret_cast<QuarryComponent*>(selfV)->QuarryComponent::OnMessage(0, nullptr, 0, 0);   // B.3.8 bridge
}
