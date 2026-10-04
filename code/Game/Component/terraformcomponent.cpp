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
#include "object/Internal.h"
#include "object/TypeInfo.h"
#include "Game/exe_bodies.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/MemoryAllocator.h"
#include "math/Math.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include "io/Stream.h"      // ENDGAME B.3.4 callers batch H: IO::Stream::GetDirection
#include "picking/PickingMesh.h"   // ENDGAME B.3.7 callers (K2)
#include <cstdint>
#include <cstdio>
#include <new>
#include <cstring>
#include "Action/exe_bodies.h"      // Pf_AvlInsertA_a4f0
#include "Action/PathSearch.h"       // RegionGraph::MarkCellDirty
#include "Game/PathfinderThread.h"   // PathfinderThread::SetCellTerrain
#include "Component/TerraformComponent.h"
#include "Component/ModelComponent.h"
#include "Component/MapComponent.h"
#include "Entity/ComponentEntity.h"
#include "Game/WorldGeometry.h"
#include "Game/Map.h"
#include "Game/exe_constants.h"

// ============================================================
// Helpers
// ============================================================

// Terrain helpers for the OnRemove restore + the completion passes:
// CitizenAI_impl_b150 (0x1400ab150) — terrain region-dirty recalc around (x,y); the
// registered body is Pf_RegionDirty4_b150 (action_pf_road.cpp).
// NOTE: the literal was previously 0x1400a4b50 (a dropped/transposed digit) which
// lands mid-FUN_1400a4b30, an unrelated merchant-name binary search. The true callee
// in TerraformComponent OnRemove is CitizenAI_impl_b150 @ 0x1400ab150.
// CitizenAI_impl_a4f0 (0x14001a4f0, Pf_AvlInsertA_a4f0) — mark one region cell dirty.
// FUN_1400a8c90 (Pf_FineCellDirty_a8c90) — terrain tile visual update.
// FUN_1400a6fe0 (Pf_EnqueueCmd2_a6fe0) — terrain type set.
// DAT_140172230 == 256.0f, the heightmap scale.
static float TF_RdataF32(uint32_t bits) { float f; memcpy(&f, &bits, sizeof f); return f; }


// ============================================================
// Reimpl of FUN_1400ab150 (CitizenAI_impl_b150): mark the region cell(s) that
// contain world tile (x,y) dirty so the region/pathing graph is rebuilt there.
// The chunk size is WorldGeometry::_chunkSize; grid dimension = _gridSize / chunk.
// When (x,y) sits on a chunk boundary the adjacent chunks are flagged too. The
// dirty tree is at +0xE8. A second port of the body Pf_RegionDirty4_b150
// (action_pf_road.cpp) that differs by the zero-chunk guard below (the tree's):
// kept, listed.
void TerraformComponent_RegionRecalc(WorldGeometry* terrain, int x, int y) {
    // QuarryComponent calls this directly. A dropped dirty mark means the carve lands
    // in the int16 heightmap but the terrain VB never refreshes, so no pit shows and
    // the decal (which follows the carved, lower heights) is fully depth-rejected
    // behind the stale terrain.
    int cell = terrain->_chunkSize;
    if (cell == 0) return;
    int gx = x / cell;
    int gy = y / cell;
    int dim = terrain->_gridSize / cell;

    int64_t* regionMgr = reinterpret_cast<int64_t*>(&terrain->_dirtyTreeRoot);   // LEFT: SIG Pf_AvlInsertA_a4f0 (takes the root slot as int64_t*)
    uint64_t scratch = 0;
    uint32_t cellIdx;

    if ((x % cell == 0) && (-1 < gx - 1)) {
        cellIdx = (uint32_t)(dim * gy - 1 + gx);
        Pf_AvlInsertA_a4f0(regionMgr, &scratch, &cellIdx);   // 0x14001a4f0
    }
    if ((y % cell == 0) && (-1 < gy - 1)) {
        cellIdx = (uint32_t)((gy - 1) * dim + gx);
        Pf_AvlInsertA_a4f0(regionMgr, &scratch, &cellIdx);   // 0x14001a4f0
    }
    if ((x % cell == 0) && (y % cell == 0) && (-1 < gx - 1) && (-1 < gy - 1)) {
        cellIdx = (uint32_t)((gy - 1) * dim - 1 + gx);
        Pf_AvlInsertA_a4f0(regionMgr, &scratch, &cellIdx);   // 0x14001a4f0
    }
    cellIdx = (uint32_t)(dim * gy + gx);
    Pf_AvlInsertA_a4f0(regionMgr, &scratch, &cellIdx);   // 0x14001a4f0
}


// ============================================================
// Terrain-deformation driver
// ============================================================
// Retail dispatch chain for the tunnel mouth:
//
//   TerraformComponent vtable +0x60 (slot 12, 0x140057310) — the per-component
//   MESSAGE slot — fires on msgId == 4 (build complete) and runs three passes:
//
//     1. ApplyMesh(ceiling=true)    0x140056b50 with param_2 = 1
//     2. ApplyMesh(ceiling=false)   0x140056b50 with param_2 = 0
//     3. ApplyFootprint()           0x140057120
//
// ApplyMesh is the deformation itself. It walks every vertex of a Picking::PickingMesh
// carried by the TerraformDescription, transforms it into world space by the owner's
// ModelComponent transform, and writes the terrain HEIGHTMAP under it. That carve is
// what reads on screen as the dark recess at a tunnel entrance — there is no separate
// "hole" mesh or sprite; the depth is real geometry. (The flat overlay on top is the
// DecalComponent's Terrain/TiledDecals/TunnelMaterial, a different, already-live path.)
//
// The two mesh/mask pairs on TerraformDescription (from Template/TunnelStart.rsc):
//     _floorMesh / _floorModifyBits      _ceilingMesh / _ceilingModifyBits
// Floor pass lowers terrain where the mesh sits BELOW it (the trench the road runs in);
// ceiling pass raises terrain where the mesh sits ABOVE it (the hill over the mouth).
// Only the ceiling pass records an undo entry — faithful to retail.
//
// 0x14009f3e0 Reimpl_MapGetTerrainPos samples the terrain; slot 0x140152a98 is
// ?IsDerivedFrom@Base@Object@@, called on the object. Import slots 0x140152b60 /
// 0x140152b68 are System::MemoryAllocator::MemAlloc / MemFree (the static pair).
// .rdata 0x140172100 == 0.25f (vertex bias), 0x14017214c == 1.0f (g_fUVScale, cell step)

// Array push_back, byte-faithful to the inlined grow in 0x140056b50 / 0x140057120:
// growth is newCount*3 once the array is non-empty, exact-fit on the first element.
template <typename T>
static void TerraformArrayPush(T*& data, int32_t& count, int32_t& cap, const T& elem) {
    const int oldCount = count;
    const int newCount = oldCount + 1;
    T* oldData = data;

    count = newCount;
    if (cap < newCount) {
        cap = (oldCount != 0) ? newCount * 3 : newCount;
        data = static_cast<T*>(System::MemoryAllocator::MemAlloc(
            static_cast<uint64_t>(cap) * static_cast<uint64_t>(sizeof(T)), 4));
    }
    // Buffer reused -> only the new tail element is written; reallocated -> copy all.
    int i = (oldData == data) ? oldCount : 0;
    for (; i < newCount; ++i) {
        data[i] = (i < oldCount) ? oldData[i] : elem;
    }
    if (oldData != data) System::MemoryAllocator::MemFree(oldData);
}

// The terrain-type byte of map cell `idx` (MapCell::terrainType, +0x29).
static uint8_t* TerraformCellTypeAt(Map* map, int64_t idx) {
    return &map->cells[idx].terrainType;
}

// Terrain type byte of the map cell containing world point (wx, wy). Retail truncates
// toward zero (CVTTSS2SI) after biasing by half the grid, then clamps to the grid.
static uint8_t TerraformCellType(Map* map, float wx, float wy) {
    const int gw = map->size;
    int cx = static_cast<int>(wx + static_cast<float>(gw >> 1));
    int cy = static_cast<int>(wy + static_cast<float>(gw >> 1));
    if (cx < 0) cx = 0;
    if (cx >= gw) cx = gw - 1;
    if (cy < 0) cy = 0;
    if (cy >= gw) cy = gw - 1;
    const int64_t idx = static_cast<int64_t>(gw) * cy + cx;
    return *TerraformCellTypeAt(map, idx);
}

// The terrain heightmap sample at (mapX, mapY): WorldGeometry's +0xD8 int16 array,
// row stride +0xD0.
static int16_t& TerraformHeightAt(WorldGeometry* terrain, int mapX, int mapY) {
    int16_t* heights = terrain->_arrayD8;
    return heights[static_cast<int64_t>(terrain->_fieldD0) * mapY + mapX];
}

// ------------------------------------------------------------
// 0x140056b50 — one heightmap pass over a TerraformDescription picking mesh.
// ------------------------------------------------------------
void TerraformComponent::ApplyMesh(bool ceiling) {
    // Every way this pass can silently do nothing is reported once. A deformation that
    // does not happen is invisible in a log otherwise, and the failure modes here are all
    // "a pointer or type id resolved to zero" — the exact class of bug that reads as
    // "the feature is broken" when it is really "the feature never ran".
    auto bail = [&](const char* why) {
        static int n = 0;
        if (n < 8) { ++n;
            fprintf(stderr, "[Terraform] MeshPass(%s) SKIPPED self=%p — %s\n",
                    ceiling ? "ceiling" : "floor", static_cast<void*>(this), why);
            fflush(stderr);
        }
    };

    // The description, accepted only if it really is a TerraformDescription.
    // Retail derefs the description unguarded (it is always there); the checks below are
    // OUR resolution failures, not retail's, which is why they bail rather than fault.
    Description* descRaw = _description;
    if (!descRaw) { bail("component +0x18 description is null"); return; }
    if (!descRaw->IsDerivedFrom(TerraformDescription::TypeId())) {
        bail("description is not a TerraformDescription"); return;
    }
    TerraformDescription* desc = static_cast<TerraformDescription*>(descRaw);

    // The owner's ModelComponent carries the placed transform (FindComponent 0x140003f60).
    ModelComponent* model = _entity ? _entity->FindComponent<ModelComponent>() : nullptr;
    if (!model) { bail("owner has no ModelComponent (finder 0x140003f60 returned null)"); return; }
    // Retail copy-constructs a Transform on the stack (??0Transform@Math@@QEAA@AEBV01@@Z,
    // slot 0x140151418) from the ModelComponent's transform.
    const Math::Transform xform(model->transform);

    if (GameUpdateLoop::_singleton == nullptr) return;
    GameUpdateLoop* gameWorld = GameUpdateLoop::_singleton;
    WorldGeometry* terrain = gameWorld->m_worldGeometry;
    Map* map = gameWorld->m_game;
    if (!terrain || !map) { bail("gameWorld terrain/tile grid null"); return; }

    const Picking::PickingMesh* mesh = ceiling ? desc->_ceilingMesh : desc->_floorMesh;
    const uint32_t modifyBits = ceiling ? desc->_ceilingModifyBits : desc->_floorModifyBits;
    if (!mesh) { bail(ceiling ? "desc+0x28 _ceilingMesh is null" : "desc+0x20 _floorMesh is null"); return; }

    const float bias        = kRdata_140172100;
    const float cellStep    = kRdata_14017214c;
    const float heightScale = TF_RdataF32(kRdata_140172230);
    const uint32_t notMask  = ~modifyBits;

    int applied = 0;
    // Picking::PickingMesh::Vertices (import slot 0x140151478) — a reference return.
    const int vertCount = mesh->Vertices(0).GetSize();

    for (int i = 0; i < vertCount; ++i) {
        // Re-query per iteration exactly as retail does.
        const ::Vector3& localVtx = mesh->Vertices(0)[i];

        // Transform::operator*(const Vector3&) — slot 0x140151470. LEFT: the picking
        // mesh's element is the global engine Vector3, Math::Vector3's layout twin.
        const Math::Vector3 world =
            xform * *reinterpret_cast<const Math::Vector3*>(&localVtx);   // LEFT: POD ::Vector3 (PickingMesh.h) vs Math::Vector3 (wave X)

        // p = transformed vertex, nudged a quarter cell so it lands inside a cell
        // rather than exactly on the shared corner.
        const float p[3] = { world.x + bias, world.y + bias, world.z + 0.0f };

        // Original terrain point under p: (x, y, currentHeight). This is the undo record.
        Math::Vector3 original(0.0f, 0.0f, 0.0f);
        Reimpl_MapGetTerrainPos(map, &original.x, const_cast<float*>(p), '\0', nullptr);   // 0x14009f3e0  LEFT: SIG Reimpl_MapGetTerrainPos (takes float*)

        // Every cell sharing this heightmap vertex must be a terrain type the mask allows.
        // NOTE: retail samples (x,y), (x-1,y), (x-1,y-1) and then (x-1,y) AGAIN — the
        // fourth sample repeats the second instead of testing (x,y-1). Verified in the
        // disassembly at 0x140056e60..0x140056e77 (MOVSS [RSP+0x30]; SUBSS XMM1,XMM7),
        // identical to the second sample's 0x140056d3d. Replicated as-is: "fixing" it
        // would widen the mask test and deform cells retail leaves alone.
        uint32_t bits = 0;
        bits |= 1u << (TerraformCellType(map, p[0],            p[1])            & 0x1F);
        bits |= 1u << (TerraformCellType(map, p[0] - cellStep, p[1])            & 0x1F);
        bits |= 1u << (TerraformCellType(map, p[0] - cellStep, p[1] - cellStep) & 0x1F);
        bits |= 1u << (TerraformCellType(map, p[0] - cellStep, p[1])            & 0x1F);
        if ((notMask & bits) != 0) continue;

        // Ceiling raises terrain up to the mesh; floor lowers terrain down to it.
        const bool apply = ceiling ? (original.z < p[2]) : (p[2] < original.z);
        if (!apply) continue;

        const float half = static_cast<float>(terrain->_gridSize >> 1);
        const int mapX = static_cast<int>(p[0] + half);
        const int mapY = static_cast<int>(p[1] + half);

        TerraformHeightAt(terrain, mapX, mapY) =
            static_cast<int16_t>(static_cast<int>(p[2] * heightScale));

        terrain->MarkChunksDirty(mapX, mapY);   // 0x1400ab150 (Pf_RegionDirty4_b150's body)
        ++applied;

        if (ceiling) {
            TerraformArrayPush(heightRecord, heightCount, heightCap, original);
        }
    }

}

// ------------------------------------------------------------
// 0x140057120 — footprint terrain-type pass.
// Over the owner's MapComponent footprint rect, every cell currently of terrain type
// 10 becomes type 6, and its index is recorded so OnRemove can put it back.
// ------------------------------------------------------------
void TerraformComponent::ApplyFootprint() {
    MapComponent* mapComp = _entity ? _entity->FindComponent<MapComponent>() : nullptr;   // 0x140004680
    if (!mapComp) return;
    if (GameUpdateLoop::_singleton == nullptr) return;
    Map* map = GameUpdateLoop::_singleton->m_game;
    if (!map) return;

    const int x0 = mapComp->tileX0;
    const int y0 = mapComp->tileY0;
    const int w  = mapComp->tileX1;
    const int h  = mapComp->tileY1;

    int converted = 0;

    for (int y = y0; y < y0 + h; ++y) {
        for (int x = x0; x < x0 + w; ++x) {
            const int idx = map->size * y + x;
            uint8_t* type = TerraformCellTypeAt(map, idx);
            if (*type != 10) continue;

            map->field_0xA2 |= 1;
            *type = 6;
            map->regionGraph->MarkCellDirty(
                idx % map->size, idx / map->size);   // 0x1400a8c90 (Pf_FineCellDirty_a8c90's body)
            // 0x1400a6fe0 reads the cell and the type (EDX, R8D) only.
            map->pathfinder->SetCellTerrain(idx, 6);   // 0x1400a6fe0 (Pf_EnqueueCmd2_a6fe0's body)
            TerraformArrayPush(tileRecord, tileCount, tileCap, static_cast<int32_t>(idx));
            ++converted;
        }
    }

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
void TerraformComponent::Persist(IO::Stream& stream) {
    Component::Persist(stream);   // retail: CALL 0x1401189b0 (qualified)

    IO::Serializer ser(stream, 0);

    // The undo record's count
    int localCount = heightCount;
    ser.Serialize(localCount, nullptr);

    // If loading, resize the Vector3 array
    // B.3.2 bridge: FUN_14000d030 is Collection::Array<Vector3>::SetSize(n)
    if (static_cast<int>(stream.GetDirection()) == 0) {   // IO::Stream::GetDirection, retail 0x180006200
        GameColl::AsArray<GameColl::Vec3>(&heightRecord).SetSize(localCount);
    }

    // Serialize each Vector3
    Math::Vector3* vec = heightRecord;
    if (localCount > 0) {
        for (uint32_t i = 0; i < static_cast<uint32_t>(localCount); i++) {
            vec->Serialize(&ser);
            ++vec;
        }
    }
}
// [6] 0x1400574f0 — the class's own `RET` body.
void TerraformComponent::OnAdded() {}
// ============================================================
// TerraformComponent OnRemove — vtable slot 7 (0x140057350)
// ============================================================
// Ghidra labels this OnCreate; it is the TEARDOWN half. It REPLAYS the undo record
// this component built at build-complete: it writes the ORIGINAL terrain heights back
// from heightRecord, and puts every recorded map cell's terrain type back to 10 (the
// value the footprint pass converted to 6). Applying it at creation would restore
// terrain that was never deformed; the "OnCreate/OnRemove off by one" Ghidra mislabel
// is a known trap in this project, and the direction of the writes (10 here vs
// 10 -> 6 in the completion pass) is what settles it.
// [7] 0x140057350
void TerraformComponent::OnRemove() {
    if (GameUpdateLoop::_singleton == nullptr) return;
    GameUpdateLoop* gameWorld = GameUpdateLoop::_singleton;

    float heightScale = TF_RdataF32(kRdata_140172230);

    // Phase 1: Apply Vector3 terrain heights
    WorldGeometry* terrain = gameWorld->m_worldGeometry;
    int vecCount = heightCount;

    if (vecCount > 0 && terrain) {
        float halfSize = (float)(terrain->_gridSize >> 1);

        for (int i = 0; i < vecCount; i++) {
            const Math::Vector3& vec = heightRecord[i];

            // Offset by half grid size
            float mapX = halfSize + vec.x;
            float mapY = halfSize + vec.y;

            // Convert Z to height index
            uint32_t heightIdx = (uint32_t)(vec.z * heightScale);

            // Write height to terrain height array
            TerraformHeightAt(terrain, (int)mapX, (int)mapY) = (int16_t)heightIdx;

            // Recalculate terrain at this position
            terrain->MarkChunksDirty((int)mapX, (int)mapY);   // 0x1400ab150 (Pf_RegionDirty4_b150's body)
        }
    }

    // Phase 2: Update tile types from the recorded cell indices
    Map* map = gameWorld->m_game;
    int count = tileCount;

    if (count > 0 && map) {
        int gridWidth = map->size;

        for (int i = 0; i < count; i++) {
            int tileIndex = tileRecord[i];

            // Read current terrain type
            uint8_t* type = TerraformCellTypeAt(map, tileIndex);
            uint8_t oldType = *type;

            // Update buildable flag
            bool isBuildable = ((1 << (oldType & 0x1F)) & 0x1E0) != 0;
            map->field_0xA2 |= (isBuildable ? 1 : 0);

            // Set tile type to 10 (terraformed)
            *type = 10;

            // Update terrain visuals if was buildable
            if (isBuildable) {
                map->regionGraph->MarkCellDirty(
                    tileIndex % gridWidth, tileIndex / gridWidth);   // 0x1400a8c90 (Pf_FineCellDirty_a8c90's body)
            }

            // Set terrain type (0x1400a6fe0; args 3/4 cast as above)
            map->pathfinder->SetCellTerrain(static_cast<int>(tileIndex), 10);   // 0x1400a6fe0 (Pf_EnqueueCmd2_a6fe0's body)
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
    ApplyMesh(/*ceiling=*/true);
    ApplyMesh(/*ceiling=*/false);
    ApplyFootprint();
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

DEFINE_EXE_TYPE(TerraformComponent, "Component")
