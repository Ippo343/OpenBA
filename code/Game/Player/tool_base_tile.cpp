// code/Game/Player/tool_base_tile.cpp
// Player wave (round 2) — "tile" sub-batch. Shared PlacementTool tile/footprint
// helpers resolved via ResolveAddr(Module::Exe, 0xADDR) from code/Game/Player/*.cpp.
//
// These resolve to 0 (silently skipped) unless registered. This file provides
// faithful 1:1 reimplementations and a single registration point
// (RegisterPlayerTileBatch in standalone/reg_player_tile.cpp) so the call sites
// fire real logic.
//
// Each function is a 1:1 port of the decompiled+disassembled exe helper; do not
// "improve" behavior.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName (scoped ModelComponent fallback)
#include "Component/component_factory_base.h" // ComponentFactory::LiveEnabled (default-on gates)
#include "Component/pickingcomponent_funcs.h" // PickingComponent_RegisterMeshForEntity (mesh-precise picking)
#include <cstdio>                            // BANISHED_TOOL_DIAG place-model trace
#include <cstdlib>                           // std::getenv
#include <cstring>                           // std::strcmp (BANISHED_CLEAR_PLACE_OVER)

// ============================================================
// 0x140003870 — SetTransformFromYaw
// ============================================================
// Builds a Math::Transform from (yaw=param_3, pitch=0, offset=param_2) into
// self+0x148, stores yaw at self+0x168, then Graphics::Geometry::SetTransform on
// the Geometry sub-object at self+0x28.
//
// Disasm-verified ABI (RCX=self, RDX=param_2 (offset Vector3*), XMM2=param_3 (yaw float)):
//   MOV R9,RDX        ; R9 = param_2 -> the OFFSET (4th) arg of FromYawPitchOffset
//   yaw local  = param_3 (XMM6)
//   pitch local = 0.0
//   CALL FromYawPitchOffset(out=local_38, &yaw, &pitch, offset=param_2)
//   copy 7 dwords (0x1c bytes, the Transform) result -> self+0x148
//   MOVSS self+0x168 = yaw
//   JMP Geometry::SetTransform(self+0x28, self+0x148)
//
// NOTE: the Ghidra decompile dropped R9/param_2 and showed a zeroed local as the
// offset; the disasm passes param_2 through as the offset. Ported per the disasm.
void ToolTile_SetTransformFromYaw_3870(uintptr_t self, const void* offset, float yaw) {
    float pitch = 0.0f;
    float yawLocal = yaw;
    // FromYawPitchOffset writes a Transform (0x1c bytes here) to outTransform.
    // Local out buffer sized like the exe stack slot (0x30 bytes / 12 floats).
    float out[12];
    if (fn_Transform_FromYawPitchOffset) {
        fn_Transform_FromYawPitchOffset(out, &yawLocal, &pitch, offset);
        // Copy 7 dwords (0x148..0x160 inclusive of dword at +0x18 = byte 0x1c).
        const uint32_t* src = reinterpret_cast<const uint32_t*>(out);
        uint32_t* dst = reinterpret_cast<uint32_t*>(self + 0x148);
        for (int i = 0; i < 7; i++) dst[i] = src[i];
    }
    *reinterpret_cast<float*>(self + 0x168) = yaw;
    if (fn_Geometry_SetTransform) {
        fn_Geometry_SetTransform(reinterpret_cast<void*>(self + 0x28),
                                 reinterpret_cast<void*>(self + 0x148));
    }
}

// ============================================================
// 0x14009d580 — PlacementTool footprint tile-validity over a rotated rectangle
// ============================================================
// Iterates a (param_4 x param_5) rectangle anchored at (param_2,param_3). For each
// cell it computes, per the rotation code in param_14 (0x5a=90, 0xb4=180,
// 0x10e=270, else 0=identity), an index into two string buffers:
//   - param_13[0]  : a char map; '.' at the index sets the "isRoad"(?) flag (R10B)
//   - param_8[0]   : a char map indexing into param_7 to override param_6 (the
//                    placement code), when param_7[1] (count) > 0.
// then calls CharacterComponent_impl_f5d0 (0x14009f5d0, registered round 1) per
// cell; returns 0 on the first f5d0 == false, else 1.
//
// 1:1 port of the Ghidra decompile (disasm-verified faithful). The index math and
// the rotation switch are reproduced exactly.
extern "C" {
// f5d0 is registered (round 1); call via ResolveAddr.
typedef bool (*ToolTile_f5d0_t)(int* param_1, int col, int row, unsigned int code,
                                float p5, float p6, float p7, char p8, char p9,
                                long long p10);
}
static ToolTile_f5d0_t s_fn_f5d0 = nullptr;
static bool s_f5d0Init = false;

// ============================================================
// Track A refinement #2 — allow SA placement over CLEARABLE resource cells.
// ============================================================
// In retail, a building's description clearable-filter (desc+0x50) matches the
// tree/rock/iron resource descriptions, so f5d0's occupant check passes for those
// cells and the building's ClearAreaComponent then harvests them. In the standalone
// exe that filter isn't satisfied, so f5d0 rejects ANY occupied cell and a building
// can never be anchored over resources — the ClearAreaComponent harvest mechanic can
// only be exercised via the footprint-override harness, never through normal
// placement. When BANISHED_CLEAR_PLACE_OVER is set, a cell that f5d0 rejects is
// re-checked with allowOverlap=1 (f5d0 param_8, which bypasses ONLY the occupant
// condition — terrain-mask, road flag, and height/slope rejections all still apply);
// the cell is accepted only if its occupant is a clearable NaturalResourceComponent.
// Default (env off) => the verified placement behaviour is byte-for-byte unchanged.
static int s_clearPlaceOver = -1;
static bool ToolTile_ClearPlaceOver() {
    if (s_clearPlaceOver < 0) {
        const char* e = std::getenv("BANISHED_CLEAR_PLACE_OVER");
        s_clearPlaceOver = (e && e[0] && std::strcmp(e, "0") != 0) ? 1 : 0;
    }
    return s_clearPlaceOver == 1;
}
// True iff the tile at (col,row) is occupied by an entity carrying a
// NaturalResourceComponent (type-id 0x1401c7b30) — i.e. a clearable tree/rock/iron.
static bool ToolTile_CellHasClearableResource(int* grid, int col, int row) {
    static uint32_t s_tid = 0;
    if (s_tid == 0) s_tid = ::Object::GetTypeIdByName("NaturalResourceComponent");
    if (!grid || !s_tid) return false;
    int width = *grid;
    if (col < 0 || col >= width || row < 0 || row >= width) return false;
    uintptr_t tiles = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(grid) + 0x80);
    int cell = width * row + col;
    uintptr_t slot = tiles + static_cast<uintptr_t>(cell) * 0x30 + 8;
    if (!*reinterpret_cast<void**>(slot)) return false;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(slot));
    void* occ = *reinterpret_cast<void**>(slot);
    if (!occ) return false;
    uintptr_t e = reinterpret_cast<uintptr_t>(occ);
    int n = *reinterpret_cast<int*>(e + 0x38);
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(e + 0x30);
    for (int i = 0; i < n && arr; ++i) {
        void* c = *reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
        if (!c) continue;
        void** vt = *reinterpret_cast<void***>(c);
        if (!vt || !vt[2]) continue;
        if (reinterpret_cast<uint32_t(*)(void*)>(vt[2])(c) == s_tid)
            return fn_IsDerivedFrom && fn_IsDerivedFrom(c, s_tid);
    }
    return false;
}

unsigned long long ToolTile_FootprintValidity_d580(
        int* param_1, int param_2, int param_3, int param_4, int param_5,
        unsigned int param_6, long long* param_7, long long* param_8,
        float param_9, float param_10, float param_11, char param_12,
        long long* param_13, int param_14, long long param_15) {
    if (!s_f5d0Init) {
        s_fn_f5d0 = reinterpret_cast<ToolTile_f5d0_t>(ResolveAddr(Module::Exe, 0x14009f5d0));
        s_f5d0Init = true;
    }

    int iVar1 = param_3 + param_5;        // row end
    int iVar4 = iVar1;
    if (param_3 < iVar1) {
        iVar4 = param_2 + param_4;        // col end
        int iVar14 = param_3;             // current row
        int iVar3 = param_5;
        do {
            iVar3 = iVar3 + -1;
            if (param_2 < iVar4) {
                int iVar10 = param_2;     // current col
                int iVar11 = param_4;
                do {
                    iVar11 = iVar11 + -1;
                    bool bVar13 = false;
                    int iVar9 = iVar14 - param_3;
                    int iVar5 = iVar11;
                    int iVar7, iVar8, iVar6, iVar12;
                    if ((param_14 == 0x5a) ||
                        ((iVar7 = param_5, iVar8 = iVar11, iVar6 = iVar3, iVar12 = param_4,
                          param_14 != 0xb4 &&
                          (iVar8 = iVar10 - param_2, iVar9 = iVar3, iVar6 = iVar14 - param_3,
                           iVar5 = iVar10 - param_2, param_14 == 0x10e)))) {
                        iVar7 = param_4;
                        iVar8 = iVar9;
                        iVar6 = iVar5;
                        iVar12 = param_5;
                    }
                    iVar8 = ((iVar7 - iVar6) + -1) * (iVar12 + 1) + iVar8;
                    long long lVar2 = *param_13;
                    if ((lVar2 != 0) && (*reinterpret_cast<int*>(lVar2 + -8) != 0)) {
                        bVar13 = *reinterpret_cast<char*>(iVar8 + lVar2) == '.';
                    }
                    if (((0 < static_cast<int>(param_7[1])) &&
                         (lVar2 = *param_8, lVar2 != 0)) &&
                        (*reinterpret_cast<int*>(lVar2 + -8) != 0)) {
                        param_6 = *reinterpret_cast<unsigned int*>(
                            *param_7 - 0xc0 +
                            static_cast<long long>(*reinterpret_cast<char*>(iVar8 + lVar2)) * 4);
                    }
                    bool ok = false;
                    if (s_fn_f5d0) {
                        ok = s_fn_f5d0(param_1, iVar10, iVar14, param_6, param_9, param_10,
                                       param_11, param_12, bVar13 ? 1 : 0, param_15);
                    }
                    // Refinement #2: accept a clearable-resource cell that f5d0 rejected
                    // only on its occupant condition. Retry with allowOverlap=1 (keeps
                    // terrain/height/slope rejection) and require a NaturalResourceComponent
                    // occupant, so buildings/water/mountains stay rejected.
                    if (!ok && s_fn_f5d0 && ToolTile_ClearPlaceOver() &&
                        s_fn_f5d0(param_1, iVar10, iVar14, param_6, param_9, param_10,
                                  param_11, /*allowOverlap=*/1, bVar13 ? 1 : 0, param_15) &&
                        ToolTile_CellHasClearableResource(param_1, iVar10, iVar14)) {
                        ok = true;
                    }
                    if (!ok) {
                        return 0;
                    }
                    iVar10 = iVar10 + 1;
                } while (iVar10 < iVar4);
            }
            iVar14 = iVar14 + 1;
        } while (iVar14 < iVar1);
    }
    return 1;
}

// ============================================================
// 0x1400fd650 — PlacementTool validate + create placed entity
// ============================================================
// Validates the rectangle via ToolTile_FootprintValidity_d580; on success spawns a
// placed ComponentEntity (CopyCtor), writes its footprint cell rect (+0x28..+0x30),
// orientation (+0x30 normalised into [0,0x168)), sets its transform via
// ToolTile_SetTransformFromYaw_3870, fixes up its model component, registers it in
// the world entity list, and runs the post-create hook.
//
// Whole exe-code closure registered: d580 + 003870 (this batch); 1018d0, c3720
// (CopyCtor), 003f60 (ClearPathQueue), 004680, c48a0, c2b70 (ModelComponent helper),
// fd960 (sub_d960) all registered in prior waves. Runtime deps: MemAlloc,
// fn_InternalBasePtr_Ctor, fn_InternalBasePtr_CheckPtr, fn_IsDerivedFrom.
//
// 1:1 port of the Ghidra decompile.
typedef long long* (*ToolTile_resolveIBP_t)(long long self, void* outIBP); // 003f60 / 004680
typedef void* (*ToolTile_copyCtor_t)(void* mem, void* src);                 // c3720
typedef void (*ToolTile_1018d0_t)(long long a, int b, int c, int d, int e, long long f);
typedef void* (*ToolTile_004680_t)(long long self, void* outIBP);
typedef void (*ToolTile_c48a0_t)(void* entityList, void* ibp, int idx);
typedef void (*ToolTile_c2b70_t)(long long modelComp, int flag);
typedef unsigned long long (*ToolTile_subd960_t)(long long self);

static ToolTile_1018d0_t  s_fn_1018d0  = nullptr;
static ToolTile_copyCtor_t s_fn_copyCtor = nullptr;
static ToolTile_004680_t  s_fn_004680  = nullptr;
static ToolTile_resolveIBP_t s_fn_clearPathQ = nullptr; // 003f60
static ToolTile_c48a0_t   s_fn_c48a0   = nullptr;
static ToolTile_c2b70_t   s_fn_c2b70   = nullptr;
static ToolTile_subd960_t s_fn_subd960 = nullptr;
static const uint32_t*    s_pTid_c7a88 = nullptr; // DAT_1401c7a88
static bool s_d650Init = false;

static void EnsureD650Deps() {
    if (s_d650Init) return;
    s_fn_1018d0    = reinterpret_cast<ToolTile_1018d0_t>(ResolveAddr(Module::Exe, 0x1401018d0));
    s_fn_copyCtor  = reinterpret_cast<ToolTile_copyCtor_t>(ResolveAddr(Module::Exe, 0x1400c3720));
    s_fn_004680    = reinterpret_cast<ToolTile_004680_t>(ResolveAddr(Module::Exe, 0x140004680));
    s_fn_clearPathQ= reinterpret_cast<ToolTile_resolveIBP_t>(ResolveAddr(Module::Exe, 0x140003f60));
    s_fn_c48a0     = reinterpret_cast<ToolTile_c48a0_t>(ResolveAddr(Module::Exe, 0x1400c48a0));
    s_fn_c2b70     = reinterpret_cast<ToolTile_c2b70_t>(ResolveAddr(Module::Exe, 0x1400c2b70));
    s_fn_subd960   = reinterpret_cast<ToolTile_subd960_t>(ResolveAddr(Module::Exe, 0x1400fd960));
    s_pTid_c7a88   = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7a88));
    s_d650Init = true;
}

// Scoped ModelComponent finder. The shared component finder 003f60 resolves its
// target type-id from raw .rdata 0x1401c7a50 (= ModelComponent), which is NULL in
// the standalone exe, so it returns a null component — and helper_d650 derefs that
// null twice (SetTransformFromYaw target + the model-variant fixup). Mirror the
// M-D pf_road scoped fallback: find the ModelComponent on the entity ourselves via
// GetTypeIdByName. Kept SCOPED here (do NOT fill the shared c7a50 global — that
// regresses the verified produce/animation callers of 003f60).
static void* ToolTile_FindModelComponentScoped(long long entity) {
    static uint32_t s_tidModel = 0;
    if (s_tidModel == 0) s_tidModel = ::Object::GetTypeIdByName("ModelComponent");
    if (!entity || !s_tidModel) return nullptr;
    int n = *reinterpret_cast<int*>(entity + 0x38);
    long long arr = *reinterpret_cast<long long*>(entity + 0x30);
    for (int k = 0; k < n && arr; ++k) {
        void* c = *reinterpret_cast<void**>(arr + static_cast<long long>(k) * 8);
        if (!c) continue;
        typedef uint32_t (*getTid_t)(void*);
        uintptr_t vt = *reinterpret_cast<uintptr_t*>(c);
        if ((*reinterpret_cast<getTid_t*>(vt + 0x10))(c) == s_tidModel) return c;
    }
    return nullptr;
}

// Standalone-only: show the COMPLETED building model on a freshly placed entity.
// helper_d650 faithfully leaves the placed building's ModelComponent at "no
// finished model" (+0x166 = -1) — the retail construction-site start state, which
// retail advances to the completed sub-mesh when the BuildComponent finishes. That
// component is inert in standalone, so a player-placed building would sit model-less
// forever (rendered as the "(?)" status icon with no mesh). Mirror StartTown's
// "show the completed building, not its foundation" fix (starttown.cpp ~L671 /
// ApplyResourceModelVariant): keep the visual style helper_d650 chose (+0x164),
// force the LAST sub-mesh = finished build (+0x166), SetMesh it, so the register-
// time OnAddedPass -> ModelComponent::Draw submits the finished mesh to the scene.
// modelDesc layout: +0x20 group array, +0x28 group count; each group is 0x18 bytes
// with sub-count at +0x10 and sub-mesh array ptr at +0x08 (indexed by modelVariant).
static void (*s_fn_tileSetMesh)(void*, void*) = nullptr;   // MeshInstance::SetMesh (slot 0x140151498)
static bool s_tileMeshInit = false;
// Set the placed building's ModelComponent build frame (+0x166) and SetMesh it so the
// register-time OnAddedPass renders it. `foundation`:
//   false -> LAST sub-mesh (finished building) — used when the live construction pipeline
//            is OFF, so a placed building isn't stuck model-less forever.
//   true  -> FIRST sub-mesh (variant 0 = construction-start foundation) — used when
//            BANISHED_LIVE_BUILDCTRL is on: the BuildComponent state machine then drives
//            the model up through the build stages to completion (organic construction).
static void ToolTile_SetPlacedModel(long long modelComp, bool foundation) {
    if (!s_tileMeshInit) {
        s_tileMeshInit = true;
        uintptr_t sl = ResolveAddr(Module::Exe, 0x140151498);
        if (sl) s_fn_tileSetMesh = *reinterpret_cast<void(**)(void*, void*)>(sl);
    }
    if (!modelComp) return;
    uint8_t* m = reinterpret_cast<uint8_t*>(modelComp);
    void* modelDesc = *reinterpret_cast<void**>(m + 0x18);
    if (!modelDesc) return;
    uint8_t* d = reinterpret_cast<uint8_t*>(modelDesc);
    uint8_t* groupArray = *reinterpret_cast<uint8_t**>(d + 0x20);
    uint32_t groupCount = *reinterpret_cast<uint32_t*>(d + 0x28);
    if (!groupArray || groupCount == 0) return;

    // Keep the variant style helper_d650 wrote to +0x164; clamp to a valid group.
    int variantIdx = *reinterpret_cast<int16_t*>(m + 0x164);
    if (variantIdx < 0 || static_cast<uint32_t>(variantIdx) >= groupCount) variantIdx = 0;
    uint8_t* group = groupArray + static_cast<int64_t>(variantIdx) * 0x18;
    int subCount = *reinterpret_cast<int*>(group + 0x10);
    // foundation -> first sub-mesh (0); else last sub-mesh (complete). A single-mesh group
    // has no construction stages, so 0 == complete either way (harmless).
    int modelVariant = foundation ? 0 : ((subCount > 0) ? subCount - 1 : 0);

    *reinterpret_cast<int16_t*>(m + 0x164) = static_cast<int16_t>(variantIdx);
    *reinterpret_cast<int16_t*>(m + 0x166) = static_cast<int16_t>(modelVariant);

    void* mesh = nullptr;
    uint8_t* subArr = *reinterpret_cast<uint8_t**>(group + 0x08);
    if (subArr) mesh = *reinterpret_cast<void**>(subArr + static_cast<int64_t>(modelVariant) * 8);
    if (mesh && s_fn_tileSetMesh) {
        s_fn_tileSetMesh(reinterpret_cast<void*>(m + 0x28), mesh);
        // Re-apply the placement yaw after the mesh swap. Retail's model-variant
        // setters (FUN_1401253d0/FUN_140125240) run a detach(+0x38)→SetMesh→Draw(+0x30)
        // cycle, and ModelComponent::Draw (0x140125xxx) re-applies the +0x148 Transform
        // via Graphics::Geometry::SetTransform — but ONLY when the model isn't already
        // in the scene (+0x16c < 0). This bare SetMesh skips that cycle, and the placed
        // entity is copy-constructed from a template (fd650), so its +0x16c can be a
        // stale scene id → the register-time OnAdded→Draw skips the transform re-apply
        // → the built building loses the rotation the ghost showed. +0x148 already holds
        // the yaw (SetTransformFromYaw_3870 wrote it at fd650 before this call), so push
        // it onto the MeshInstance geometry directly — exactly what Draw would do.
        if (fn_Geometry_SetTransform)
            fn_Geometry_SetTransform(reinterpret_cast<void*>(m + 0x28),
                                     reinterpret_cast<void*>(m + 0x148));
    }

    if (const char* e = std::getenv("BANISHED_TOOL_DIAG"); e && e[0] && e[0] != '0') {
        std::fprintf(stderr, "[place-model] modelComp=%p desc=%p groups=%u variantIdx=%d "
                     "subCount=%d modelVariant=%d (%s) mesh=%p setMesh=%p\n",
                     (void*)modelComp, modelDesc, groupCount, variantIdx, subCount,
                     modelVariant, foundation ? "foundation" : "complete", mesh, (void*)s_fn_tileSetMesh);
        std::fflush(stderr);
    }
}

// Organic construction is available when the live BuildComponent state machine is enabled.
// Default-ON via kDefaultOnFlags (graduated 2026-07-22); env var is a kill-switch (=0).
static bool ToolTile_OrganicConstruction() {
    return ::ComponentFactory::LiveEnabled("BANISHED_LIVE_BUILDCTRL");
}

// (The placement path deliberately performs no occupancy write — see the note at the tail
// of ValidateCreate_fd650. That belongs to MapComponent slot 12, driven by ClearArea's
// state-2 event-1 broadcast.)

// Diagnostic for the placed-building click-pick question: report both pick routes for
// the entity we just created. cellEntity != 0 => EditTool's cell pick resolves it;
// pickingId >= 0 => the Picking::Scene mesh route registered a mesh. BANISHED_PLACE_PICK_DIAG=1.
static void ToolTile_ProbePlacedPick(void* entity, int tileX, int tileY) {
    static bool s_on = [](){ const char* e = std::getenv("BANISHED_PLACE_PICK_DIAG");
                             return e && e[0] && e[0] != '0'; }();
    if (!s_on || !entity || !g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    uint8_t* gw = reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton);
    int* grid = *reinterpret_cast<int**>(gw + 0xf8);

    // Walk the entity's component array for the Model/Picking/Map trio (entity+0x30 array,
    // +0x38 count; each slot an IBP) — same shape as PC_FindModelComponent.
    int modelVariant = -999, pickingId = -999;
    uint8_t* mapComp = nullptr;
    // Array INDEX of each: ComponentEntity_OnAddedPass dispatches slot 6 in array order, so
    // whether PickingComponent sees a framed model (variant 0 = the foundation decal) or an
    // un-framed one (-1, stamped by BuildComponent's own OnAdded Stage 3) is decided purely
    // by which index comes first. That ordering is the whole question for the mesh route.
    int idxModel = -1, idxPick = -1, idxBuild = -1, idxMap = -1, idxZone = -1;
    uint32_t tidModel = ::Object::GetTypeIdByName("ModelComponent");
    uint32_t tidPick  = ::Object::GetTypeIdByName("PickingComponent");
    uint32_t tidMap   = ::Object::GetTypeIdByName("MapComponent");
    uint32_t tidBuild = ::Object::GetTypeIdByName("BuildComponent");
    uint32_t tidZone  = ::Object::GetTypeIdByName("ZoneComponent");
    int count = *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(entity) + 0x38);
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uint8_t*>(entity) + 0x30);
    for (int i = 0; i < count; ++i) {
        void** slot = reinterpret_cast<void**>(arr + (uintptr_t)i * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
        void* comp = *slot;
        if (!comp) continue;
        typedef uint32_t (*gt_t)(void*);
        uint32_t tid = (*reinterpret_cast<gt_t*>(*reinterpret_cast<uintptr_t*>(comp) + 0x10))(comp);
        if (tid == tidModel) { modelVariant = *reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(comp) + 0x166); idxModel = i; }
        else if (tid == tidPick) { pickingId = *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(comp) + 0x28); idxPick = i; }
        else if (tid == tidMap) { mapComp = reinterpret_cast<uint8_t*>(comp); idxMap = i; }
        else if (tid == tidBuild) idxBuild = i;
        else if (tid == tidZone) idxZone = i;
    }

    // Sweep the WHOLE footprint, not one corner: dc20 clears '.' cells (the ground in
    // front of a building) and only stamps '#'/'_' ones, so sampling the rect origin
    // reads a legitimately-empty cell and libels the fix. Report coverage instead, plus
    // what else is holding cells (a foreign occupant = the trees this port does not remove).
    // Both slots: FUN_1400a0280 returns cell+0x00 first and only falls back to cell+0x08,
    // so a building that is only ever written to +0x08 can be masked by whatever holds +0x00.
    int fpCells = 0, mine = 0, foreign = 0, empty = 0;
    int s0_mine = 0, s0_other = 0, s0_empty = 0;
    if (grid && mapComp) {
        int dim = *grid;
        uint8_t* cells = *reinterpret_cast<uint8_t**>(reinterpret_cast<uint8_t*>(grid) + 0x80);
        int x0 = *reinterpret_cast<int16_t*>(mapComp + 0x28), y0 = *reinterpret_cast<int16_t*>(mapComp + 0x2a);
        int w  = *reinterpret_cast<int16_t*>(mapComp + 0x2c), h  = *reinterpret_cast<int16_t*>(mapComp + 0x2e);
        for (int iy = 0; cells && iy < h; ++iy)
            for (int ix = 0; ix < w; ++ix) {
                int cx = x0 + ix, cy = y0 + iy;
                if (cx < 0 || cx >= dim || cy < 0 || cy >= dim) continue;
                ++fpCells;
                uint8_t* cell = cells + (size_t)(dim * cy + cx) * 0x30;
                void* occ0 = *reinterpret_cast<void**>(cell + 0x00);
                void* occ8 = *reinterpret_cast<void**>(cell + 0x08);
                if (!occ8) ++empty; else if (occ8 == entity) ++mine; else ++foreign;
                if (!occ0) ++s0_empty; else if (occ0 == entity) ++s0_mine; else ++s0_other;
            }
    }

    // The retail dispatch is MapComponent::Draw -> (gate: MapDescription+0x30) -> 41b0 ->
    // (gate: desc+0x31) -> dc20. If +0x30 is 0 for a building template, retail NEVER runs
    // the occupancy write at register time and any call we make to it is a deviation.
    const uint8_t* mdesc = mapComp ? *reinterpret_cast<const uint8_t* const*>(mapComp + 0x18) : nullptr;
    std::fprintf(stderr, "[PlacePick] entity=%p tile=(%d,%d) fp=%d cells | slot8: mine=%d foreign=%d empty=%d %s"
                 " | slot0: mine=%d other=%d empty=%d | modelVariant=%d pickingId=%d %s"
                 " | mapDesc=%p +0x30(drawGate)=%d +0x31(skipOccupancy)=%d +0x32(minimap)=%d | pickmap=%s\n",
                 entity, tileX, tileY, fpCells, mine, foreign, empty,
                 (mine || s0_mine) ? "CELL-PICK OK" : "CELL-PICK DEAD",
                 s0_mine, s0_other, s0_empty,
                 modelVariant, pickingId,
                 (pickingId > 0) ? "MESH-PICK OK" : "MESH-PICK DEAD",
                 (const void*)mdesc,
                 mdesc ? mdesc[0x30] : -1, mdesc ? mdesc[0x31] : -1, mdesc ? mdesc[0x32] : -1,
                 ::ComponentFactory::LiveEnabled("BANISHED_LIVE_MAP_SOLIDIFY") ? "on" : "off");
    std::fprintf(stderr, "[PlacePick]   component order: Model=[%d] Picking=[%d] Build=[%d] Map=[%d] Zone=[%d] -> "
                 "Picking %s Build (retail OnAddedPass runs them in this order)\n",
                 idxModel, idxPick, idxBuild, idxMap, idxZone,
                 (idxPick >= 0 && idxBuild >= 0) ? (idxPick < idxBuild ? "BEFORE" : "AFTER") : "?");
    std::fflush(stderr);
}

unsigned long long ToolTile_ValidateCreate_fd650(
        long long param_1, int param_2, int param_3, int param_4, int param_5,
        const void* param_6, float param_7) {
    EnsureD650Deps();

    // The exe global g_pGameWorldSingleton holds the gameWorld pointer value; in our
    // type system that value is *g_pGameWorldSingleton (engine_funcs declares it void**).
    uintptr_t gameWorld = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
    // DAT_140172248 == 0x447a0000 == 250.0f (read from Ghidra).
    const float DAT_140172248 = 250.0f;

    int* piVar10 = *reinterpret_cast<int**>(gameWorld + 0xf8);
    int iVar12 = (param_4 - param_2) + 1;   // width
    int iVar13 = (param_5 - param_3) + 1;   // height
    long long lVar3 = *reinterpret_cast<long long*>(param_1 + 0x28);
    int iVar11 = static_cast<int>(param_7); // orientation

    unsigned long long uVar7 = ToolTile_FootprintValidity_d580(
        piVar10, param_2, param_3, iVar12, iVar13,
        *reinterpret_cast<unsigned int*>(lVar3 + 0x20),
        reinterpret_cast<long long*>(lVar3 + 0x30),
        reinterpret_cast<long long*>(lVar3 + 0x28),
        0.0f, DAT_140172248, 0.0f, '\0',
        reinterpret_cast<long long*>(*reinterpret_cast<long long*>(param_1 + 0x30) + 0x28),
        iVar11,
        *reinterpret_cast<long long*>(lVar3 + 0x50));

    if (static_cast<char>(uVar7) == '\0') {
        return uVar7 & 0xffffffffffffff00ULL;
    }

    long long lvCfg = *reinterpret_cast<long long*>(param_1 + 0x10);
    if (*reinterpret_cast<char*>(lvCfg + 0x80) != '\0') {
        int iVar1 = **reinterpret_cast<int**>(gameWorld + 0xf8);
        long long c98 = *reinterpret_cast<int*>(lvCfg + 0x98);
        long long c9c = *reinterpret_cast<int*>(lvCfg + 0x9c);
        int col0 = static_cast<int>(c98 % iVar1);
        int row0 = static_cast<int>(c98 / iVar1);
        int col1 = static_cast<int>(c9c % iVar1);
        int row1 = static_cast<int>(c9c / iVar1);
        if ((param_2 != col0) || (param_3 != row0) ||
            (col1 != param_4) || (row1 != param_5) ||
            (*reinterpret_cast<int*>(lvCfg + 0xa0) != iVar11)) {
            return uVar7 & 0xffffffffffffff00ULL;
        }
        // piVar10 in the original is reused as (int*)col0 here, but it is only ever
        // passed as the first arg of 1018d0 (a longlong); pass col0 to mirror it.
        piVar10 = reinterpret_cast<int*>(static_cast<long long>(col0));
    }

    if (s_fn_1018d0) {
        s_fn_1018d0(reinterpret_cast<long long>(piVar10), param_2, param_3, param_4, param_5,
                    *reinterpret_cast<long long*>(
                        *reinterpret_cast<long long*>(param_1 + 0x28) + 0x50));
    }

    // Allocate + copy-construct the placed ComponentEntity.
    void* mem = fn_MemAlloc ? fn_MemAlloc(g_pMemAllocSingleton, 0x48, 8) : nullptr;
    void* ptr = nullptr;
    if (mem != nullptr && s_fn_copyCtor) {
        ptr = s_fn_copyCtor(mem, *reinterpret_cast<void**>(param_1 + 0x20));
    }

    long long local_res8 = 0;
    void* local_48 = mem; // doubles as the out-IBP scratch (matches the exe stack reuse)
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&local_res8, ptr);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&local_res8);

    // Resolve the placed-entity sub-component (004680) and write the footprint rect.
    void* pIVar8 = s_fn_004680 ? s_fn_004680(local_res8, &local_48) : nullptr;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(pIVar8);
    lVar3 = *reinterpret_cast<long long*>(pIVar8);
    *reinterpret_cast<short*>(lVar3 + 0x28) = static_cast<short>(param_2);
    *reinterpret_cast<short*>(lVar3 + 0x2a) = static_cast<short>(param_3);
    *reinterpret_cast<short*>(lVar3 + 0x2c) = static_cast<short>(iVar12);
    *reinterpret_cast<short*>(lVar3 + 0x2e) = static_cast<short>(iVar13);
    *reinterpret_cast<int*>(lVar3 + 0x30) = iVar11;
    if (iVar11 < 0) {
        do { iVar11 = iVar11 + 0x168; } while (iVar11 < 0);
        *reinterpret_cast<int*>(lVar3 + 0x30) = iVar11;
    }

    // Resolve the model/geometry sub-component (ClearPathQueue == 003f60) and set its transform.
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&local_res8);
    pIVar8 = s_fn_clearPathQ ? reinterpret_cast<void*>(s_fn_clearPathQ(local_res8, &local_48)) : nullptr;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(pIVar8);
    long long modelComp = pIVar8 ? *reinterpret_cast<long long*>(pIVar8) : 0;
    // 003f60's c7a50 type-id is NULL in standalone → null component. Scoped fallback.
    if (modelComp == 0) modelComp = reinterpret_cast<long long>(ToolTile_FindModelComponentScoped(local_res8));
    if (modelComp != 0) ToolTile_SetTransformFromYaw_3870(modelComp, param_6, param_7);

    // Resolve again (different out-IBP) and fix up the model component.
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&local_res8);
    unsigned char local_38[16];
    pIVar8 = s_fn_clearPathQ ? reinterpret_cast<void*>(s_fn_clearPathQ(local_res8, local_38)) : nullptr;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(pIVar8);
    long long* plVar4 = pIVar8 ? *reinterpret_cast<long long**>(pIVar8) : nullptr;
    // Same c7a50-NULL fallback as the transform site above.
    if (plVar4 == nullptr) plVar4 = reinterpret_cast<long long*>(ToolTile_FindModelComponentScoped(local_res8));
    if (plVar4 != nullptr) {
        int uVar2 = *reinterpret_cast<int*>(param_1 + 100);
        if (fn_IsDerivedFrom && s_pTid_c7a88) {
            fn_IsDerivedFrom(reinterpret_cast<void*>(plVar4[3]), *s_pTid_c7a88);
        }
        *reinterpret_cast<short*>(reinterpret_cast<long long>(plVar4) + 0x164) = static_cast<short>(uVar2);
        if (-1 < *reinterpret_cast<short*>(reinterpret_cast<long long>(plVar4) + 0x166)) {
            // vtable +0x38 call
            (*reinterpret_cast<void(**)(long long*)>(*plVar4 + 0x38))(plVar4);
        }
        *reinterpret_cast<unsigned short*>(reinterpret_cast<long long>(plVar4) + 0x166) = 0xffff;
        // With the live construction pipeline (BANISHED_LIVE_BUILDCTRL) the placed building
        // is left at its FOUNDATION frame so the BuildComponent state machine drives it
        // through organic gather -> build -> complete: BuildComponent_DemoGateSkip picks up
        // any incomplete-model building when no demo foundation is seeded (BuildDemoIndex
        // default = -1). Without the pipeline, force the finished mesh so the placed building
        // renders (the original "no completion flow -> model-less forever" guard).
        ToolTile_SetPlacedModel(reinterpret_cast<long long>(plVar4), ToolTile_OrganicConstruction());
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(plVar4 + 2);
        if (s_fn_c2b70) s_fn_c2b70(plVar4[2], 0);
    }

    // Register the new entity in the world entity list.
    local_48 = reinterpret_cast<void*>(local_res8);
    if (s_fn_c48a0) {
        s_fn_c48a0(*reinterpret_cast<void**>(gameWorld + 0xe8), &local_48,
                   *reinterpret_cast<int*>(*reinterpret_cast<long long*>(param_1 + 0x20) + 0x20));
    }

    // Mesh-precise picking: register the placed building's (now-completed, forced above)
    // model mesh into Picking::Scene so it's clickable anywhere on the model — the same
    // fix start-town buildings get. Retail would drive this via PickingComponent::OnAdded
    // during the build lifecycle, which is inert in standalone. Respects LIVE_PICKMESH.
    //
    // NB this is a NO-OP for a foundation, faithfully: PickingComponent::OnAdded bails on
    // `modelVariant < 0`, and a just-registered building has -1 there (retail stamps it at
    // fd650 above; ours is re-stamped by BuildComponent's OnAdded Stage 3). So the mesh
    // route cannot select a foundation in retail either — the MAP-CELL route below is what
    // does, which is why it is the load-bearing half.
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&local_res8);
    PickingComponent_RegisterMeshForEntity(reinterpret_cast<void*>(local_res8));

    // Cell-precise picking + navigation blocking: write the placed building's footprint
    // into the region-map cell array (cell+0x08 = the entity), the half retail gets from
    // RegisterEntity -> OnAddedPass -> MapComponent's vtable+0x30 -> sub_41b0 -> dc20.
    // Standalone's MapComponent slot 6 only paints the minimap and the occupancy half is
    // collapsed into per-site helpers (starttown.cpp) — the START-TOWN spawn and the
    // upgrade-replacement path each call one, and the PLAYER PLACEMENT path had no
    // equivalent. Consequence measured 2026-07-28: a hand-placed building holds 0 of its
    // 16 footprint cells, so it is unclickable (EditTool_OnKey's cell pick reads cell+0x08
    // and finds nothing, and the mesh pick above legitimately registered nothing), it does
    // not block navigation, and its removal-side cell clear stays disarmed.
    //
    // NB there is deliberately NO map-cell occupancy write here. An earlier pass added one
    // and it was wrong: retail's chain is MapComponent::Draw (slot 6) -> gate
    // `MapDescription+0x30 != 0` -> 41b0 -> dc20, and +0x30 is **0** for a WoodHouse, so
    // retail never writes cells at register time. Forcing it ran dc20's stale-occupant
    // branch and MarkForRemoval'd 11 trees under one footprint — the opposite of the
    // shipped game, where the trees stay and the building's dialog reads "remove X of Y".
    //
    // A building claims its cells LATER, when its ClearArea reaches state 2: that handler
    // broadcasts event 1 (`MOV EDX,1; CALL 0x1400c2b70`) and MapComponent's slot 12 turns
    // that into the 41b0 call. See MapComponent_Slot12VSlot (mapcomponent.cpp), which is
    // where the occupancy write belongs.
    ToolTile_ProbePlacedPick(reinterpret_cast<void*>(local_res8), param_2, param_3);

    unsigned long long uVar9 = s_fn_subd960 ? s_fn_subd960(param_1) : 0;
    return (uVar9 & ~0xffULL) | 1ULL;
}
