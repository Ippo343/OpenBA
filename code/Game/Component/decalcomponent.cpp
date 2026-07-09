// code/Game/Component/decalcomponent.cpp
// Clean reimplementation of DecalComponent lifecycle methods.
//
// DecalComponent (0x38 bytes) manages ground decals for buildings (shadows, footprints).
// Fields: +0x28=decalState (uint32), +0x2C=decalId (int32, -1=none), +0x30=decalScale (float, 1.0)
//
// Vtable at 0x140162fd0. Non-trivial overrides:
//   Shutdown  (0x14011b820) -- Serializes decalScale and decalState
//   Serialize (0x14011b7d0) -- Reads building data, copies scale from data+0x20
//   Draw      (0x14011b560) -- Complex terrain decal rendering (111 instrs)
//   OnCreate  (0x14011b510) -- Removes decal from terrain if decalId >= 0
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- portable for standalone exe.

#include "Component/decalcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName (SA type-id fallback)
#endif

// ============================================================
// Resolved helpers
// ============================================================

typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

typedef void (*fn_Serialize_Int_t)(void* serializer, int* value, const char* name);
static fn_Serialize_Int_t s_fn_Serialize_Int = nullptr;

// IO::Serializer::Serialize(float*, name) -- dispatch slot at 0x140152a60
typedef void (*fn_Serialize_Float_t)(void* serializer, float* value, const char* name);
static fn_Serialize_Float_t s_fn_Serialize_Float = nullptr;

// IO::Serializer::GetVersion -- dispatch slot at 0x140152a48
typedef int (*fn_Serializer_GetVersion_t)(void* serializer);
static fn_Serializer_GetVersion_t s_fn_Serializer_GetVersion = nullptr;

// IsDerivedFrom dispatch slot at 0x140152a98
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* obj, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_dispatch = nullptr;

// Type ID for DecalDescription at DAT_1401c75b8
static uint32_t* s_TypeId_DecalDesc = nullptr;

// Original Draw function (too complex: 111 instrs, terrain decal rendering)
typedef void (*fn_DecalDraw_t)(void* self);
static fn_DecalDraw_t s_fn_OriginalDraw = nullptr;

// Terrain decal remove: FUN_1400b0d00(terrainDecalSys, decalId)
typedef void (*fn_TerrainDecalRemove_t)(void* terrainDecalSys, int decalId);
static fn_TerrainDecalRemove_t s_fn_TerrainDecalRemove = nullptr;

// Sub-callees of the terrain-decal-slot release (FUN_1400b0d00):
//   FUN_14001ee40 — resize the per-slot 0x14-byte-element list (stride 0x14, align 2)
//   FUN_14001c890 — resize the per-slot 2-byte-element list (stride 2, align 2)
// Both are already registered exe leaves; we resolve and forward to them.
typedef void (*fn_ResizeStride14_t)(void* arrayField, int newCount);
static fn_ResizeStride14_t s_fn_ResizeStride14 = nullptr;
typedef void (*fn_ResizeStride2_t)(void* arrayField, int newCount);
static fn_ResizeStride2_t s_fn_ResizeStride2 = nullptr;

static bool s_initialized = false;

// ============================================================
// Reimpl of FUN_1400b0d00 (TerrainDecalSystem release one decal slot)
// ============================================================
// Shared terrain-decal helper (also called from RadiusUI/PlacementTool).
// The decal system at terrainDecalSys+0x1F0 is an array of 0x60-byte slots.
// If slot[decalId].id (+0x28) still equals decalId, mark it free (-1), clear
// the slot's owner ptr (+0), and shrink the two sub-lists embedded at +8 and
// +0x18 to zero length. 1:1 port of the exe body.
void DecalComponent_TerrainDecalRemove(void* terrainDecalSys, int decalId) {
    uintptr_t base = *(uintptr_t*)((uintptr_t)terrainDecalSys + 0x1f0);
    uintptr_t slot = base + (uintptr_t)decalId * 0x60;
    if (*(int*)(slot + 0x28) == decalId) {
        *(uint32_t*)(slot + 0x28) = 0xffffffff;
        *(uint64_t*)(base + (uintptr_t)decalId * 0x60) = 0;
        if (s_fn_ResizeStride14)
            s_fn_ResizeStride14((void*)(slot + 8), 0);
        if (s_fn_ResizeStride2)
            s_fn_ResizeStride2((void*)(slot + 0x18), 0);
    }
}

// ============================================================
// Helper: Get DecalDescription config component
// ============================================================
static void* GetDecalDescription(void* self) {
    void* obj = *(void**)((uintptr_t)self + 0x18);
    if (!obj || !s_fn_IsDerivedFrom_dispatch || !s_TypeId_DecalDesc)
        return nullptr;
    bool derived = s_fn_IsDerivedFrom_dispatch(obj, *s_TypeId_DecalDesc);
    return derived ? obj : nullptr;
}

// ============================================================
// DecalComponent::Shutdown (vtable slot [3], 0x14011b820)
// ============================================================
// Serializes:
//   1. Component_Serialize base
//   2. Serializer(version=1): serialize decalScale (float at +0x30)
//   3. If version > 0: serialize decalState (int at +0x28)
void DecalComponent_Shutdown(DecalComponent* self, void* stream) {
    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    uint8_t serializer[24];
    if (!s_fn_Serializer_Ctor) return;
    s_fn_Serializer_Ctor(serializer, stream, 1);

    // Serialize decalScale (float at +0x30)
    if (s_fn_Serialize_Float)
        s_fn_Serialize_Float(serializer, &self->decalScale, nullptr);

    // Version-gated: if version > 0, also serialize decalState
    if (s_fn_Serializer_GetVersion) {
        int version = s_fn_Serializer_GetVersion(serializer);
        if (version > 0) {
            if (s_fn_Serialize_Int)
                s_fn_Serialize_Int(serializer, (int*)&self->decalState, nullptr);
        }
    }

    if (s_fn_Serializer_Dtor)
        s_fn_Serializer_Dtor(serializer);
}

// ============================================================
// DecalComponent::Serialize (vtable slot [4], 0x14011b7d0)
// ============================================================
// Gets DecalDescription via IsDerivedFrom(+0x18, DAT_1401c75b8),
// reads scale from building data+0x20 and stores in decalScale (+0x30).
void DecalComponent_Serialize(DecalComponent* self) {
    void* config = GetDecalDescription(self);
    if (config) {
        self->decalScale = *(float*)((uintptr_t)config + 0x20);
    }
}

// ============================================================
// DecalComponent::Draw (vtable slot [6], 0x14011b560)
// ============================================================
// Real terrain-decal rendering (was a delegate to the exe body). Faithful 1:1 port of
// the 0x14011b560 disassembly: read the building footprint (MapComponent tile bbox +
// rotation), the model variant index (ModelComponent+0x164 → per-variant UV rect), and
// the DecalDescription (material list + tiled/uv params), then submit one decal to the
// terrain-decal system (gameWorld+0x110) via AddDecal (TradeWork_impl_0d80 @ b0d80).
// AddDecal fills a slot in the +0x1f0 array and tessellates it (ace40); the slot is then
// rendered every frame by GeomDraw_CollectDraw's +0x1f0 loop (geometry_draw.cpp). The
// resulting decalId is stored at self+0x2c so OnCreate can release it.
//
// This is the brown ground foundation under a StorageYard (and any building carrying a
// DecalDescription). STANDALONE: reached via the live vtable / driver below when
// BANISHED_LIVE_DECAL is set; the b0d80 → ace40 closure must be registered
// (reg_player_highlight.cpp) and the Transform2/Vector2 slots wired.

// AddDecal (0x1400b0d80, tool_base_highlight.cpp) — 19-arg tile-slot manager + tessellate.
extern "C" int TradeWork_impl_0d80(int64_t sys, int minX, int minY, int maxX, int maxY,
                                   uint8_t alpha, uint32_t rot, int64_t material,
                                   uint8_t tiledFlag, uint32_t p10, uint32_t uv0, uint32_t uv1,
                                   uint32_t uv2, uint32_t uv3, char p15, int p16, int p17,
                                   uint64_t p18, uint8_t p19);

// Draw-path helpers (resolved independently of DecalComponent_InitFuncs, which is only
// called from the DLL hook — in standalone the driver calls Draw without InitFuncs).
typedef void  (*fn_CheckPtr_draw_t)(void* ptr);
typedef void* (*fn_FindComp_t)(void* entityInner, void* retIBP);
static fn_CheckPtr_draw_t s_fn_CheckPtr_draw   = nullptr;
static fn_FindComp_t      s_fn_FindComp_Map    = nullptr;   // 0x140004680 (MapComponent)
static fn_FindComp_t      s_fn_FindComp_Model  = nullptr;   // 0x140003f60 (ModelComponent)
static void**             s_pGameStateDraw      = nullptr;   // DAT_1401c3dd8
// RefreshEntityFlags (0x1400c3470) — re-aggregate the owning entity's component flags
// and (un)register it in the update tree; used to bucket the decal's owner via the draw
// flag so the tree walk's +0x40 slot drives the build-once, then un-bucket once placed.
typedef void (*fn_EntityFlagUpdate_t)(void* entity);
static fn_EntityFlagUpdate_t s_fn_EntityFlagUpdate = nullptr;
static bool s_drawHelpersInit = false;

static bool DecalTraceOn() {
    static int v = -1;
    if (v < 0) { const char* e = getenv("BANISHED_DECAL_TRACE");
                 v = (e && *e && e[0] != '0') ? 1 : 0; }
    return v == 1;
}

// DecalDescription type-id (DAT_1401c75b8). In DLL mode the raw .rdata slot holds the id;
// in standalone that .rdata address resolves NULL, so fall back to the runtime type
// registry (valid after Object::Factory::Startup) — same split as StorageDisplayComponent.
static uint32_t DecalDescTypeId() {
    if (s_TypeId_DecalDesc && *s_TypeId_DecalDesc != 0 && *s_TypeId_DecalDesc != 0xFFFFFFFF)
        return *s_TypeId_DecalDesc;
#ifdef STANDALONE_EXE
    static uint32_t cached = 0;
    if (cached == 0 || cached == 0xFFFFFFFF) cached = ::Object::GetTypeIdByName("DecalDescription");
    if (cached != 0xFFFFFFFF) return cached;
#endif
    return 0;
}

static void EnsureDrawHelpers() {
    if (s_drawHelpersInit) return;
    uintptr_t slot = ResolveAddr(Module::Exe, 0x140152ab0);   // InternalBasePtr::CheckPtr
    if (slot) s_fn_CheckPtr_draw = *reinterpret_cast<fn_CheckPtr_draw_t*>(slot);
    s_fn_FindComp_Map   = reinterpret_cast<fn_FindComp_t>(ResolveAddr(Module::Exe, 0x140004680));
    s_fn_FindComp_Model = reinterpret_cast<fn_FindComp_t>(ResolveAddr(Module::Exe, 0x140003f60));
    s_pGameStateDraw    = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x1401c3dd8));
    s_fn_EntityFlagUpdate = reinterpret_cast<fn_EntityFlagUpdate_t>(ResolveAddr(Module::Exe, 0x1400c3470));
    if (!s_TypeId_DecalDesc)
        s_TypeId_DecalDesc = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c75b8));
    if (!s_fn_IsDerivedFrom_dispatch) {
        uintptr_t s = ResolveAddr(Module::Exe, 0x140152a98);
        if (s) s_fn_IsDerivedFrom_dispatch = *reinterpret_cast<fn_IsDerivedFrom_dispatch_t*>(s);
    }
    s_drawHelpersInit = true;
}

// FindComponent(ModelComponent) — SA-safe. The exe helper 0x140003f60 matches on the
// raw .rdata type-id constant DWORD_1401c7a50, which is NEVER seeded in standalone
// (same class of gap as the DecalDescription type-id — see DecalDescTypeId()). With
// that constant null, the exe FindComponent's `if (typeId)` guard fails and it returns
// a null component, so the decal read variant=0 and always sampled atlas cell (0,0)
// (transparent). We therefore resolve the ModelComponent type-id from the runtime
// registry (Object::GetTypeIdByName, valid after Object::Factory::Startup) and walk the
// owner entity's component list ourselves — the same manual find StartTown already uses
// successfully (FindComponentOnEntity). In DLL mode the exe constant is live, so we keep
// delegating to the faithful 0x140003f60. Returns the ModelComponent ptr or nullptr.
static void* FindModelComponentOnOwner(void* ownerEnt) {
#ifdef STANDALONE_EXE
    // Preferred faithful path: the exe FindComponent(ModelComponent) (0x140003f60) now
    // works in standalone because the ModelComponent type-id .rdata constant
    // (DWORD_1401c7a50) is seeded by reg_pipeline_typeids.cpp (gated on BANISHED_LIVE_DECAL).
    // Fall back to the manual GetTypeIdByName walk if the seeding is absent (e.g. the exe
    // helper unresolved). One-shot log records which path resolved the model.
    if (ownerEnt && s_fn_FindComp_Model) {
        uintptr_t exeBuf[2] = {0, 0};
        s_fn_FindComp_Model(ownerEnt, exeBuf);
        static int s_pathLog = 0;
        if (DecalTraceOn() && s_pathLog < 4) { ++s_pathLog;
            fprintf(stderr, "[Decal::Find] exe 0x140003f60 -> %p (seeded-typeid path %s)\n",
                    (void*)exeBuf[0], exeBuf[0] ? "OK" : "FAILED->fallback"); fflush(stderr); }
        if (exeBuf[0]) {
            if (s_fn_CheckPtr_draw) s_fn_CheckPtr_draw(exeBuf);
            return reinterpret_cast<void*>(exeBuf[0]);
        }
    }
    static uint32_t s_tidModel = 0;
    if (s_tidModel == 0 || s_tidModel == 0xFFFFFFFF)
        s_tidModel = ::Object::GetTypeIdByName("ModelComponent");
    if (!ownerEnt || s_tidModel == 0 || s_tidModel == 0xFFFFFFFF) return nullptr;
    uint8_t* e = reinterpret_cast<uint8_t*>(ownerEnt);
    int count = *reinterpret_cast<int32_t*>(e + 0x38);
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(e + 0x30);
    for (int i = 0; i < count; ++i) {
        void** slot = reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
        if (s_fn_CheckPtr_draw) s_fn_CheckPtr_draw(slot);
        void* comp = *slot;
        if (!comp) continue;
        uintptr_t vt = *reinterpret_cast<uintptr_t*>(comp);
        typedef uint32_t (*fn_GetTypeId_t)(void*);
        uint32_t id = (*reinterpret_cast<fn_GetTypeId_t*>(vt + 0x10))(comp);
        if (id == s_tidModel) {
            if (s_fn_CheckPtr_draw) s_fn_CheckPtr_draw(slot);
            return *slot;
        }
    }
    return nullptr;
#else
    if (!s_fn_FindComp_Model || !ownerEnt) return nullptr;
    uintptr_t modelBuf[2] = {0, 0};
    s_fn_FindComp_Model(ownerEnt, modelBuf);
    if (modelBuf[0] && s_fn_CheckPtr_draw) s_fn_CheckPtr_draw(modelBuf);
    return reinterpret_cast<void*>(modelBuf[0]);
#endif
}

void DecalComponent_Draw(DecalComponent* self) {
    EnsureDrawHelpers();
    static int s_dbg = 0;
    bool trc = DecalTraceOn() && (s_dbg < 30);
    if (trc) { ++s_dbg;
        fprintf(stderr, "[Decal] Draw ENTER self=%p gs=%p findMap=%p findModel=%p\n",
                (void*)self, s_pGameStateDraw ? *s_pGameStateDraw : nullptr,
                (void*)s_fn_FindComp_Map, (void*)s_fn_FindComp_Model); fflush(stderr); }
    if (!self || !s_pGameStateDraw || !*s_pGameStateDraw) {
        if (trc) { fprintf(stderr, "[Decal]   bail: no gamestate\n"); fflush(stderr); }
        return;
    }
    uintptr_t selfA = reinterpret_cast<uintptr_t>(self);

    // CheckPtr(owner) then FindComponent(MapComponent) for the tile footprint.
    if (s_fn_CheckPtr_draw) s_fn_CheckPtr_draw(reinterpret_cast<void*>(selfA + 0x10));
    uintptr_t mapBuf[2] = {0, 0};
    if (s_fn_FindComp_Map) s_fn_FindComp_Map(*reinterpret_cast<void**>(selfA + 0x10), mapBuf);
    if (s_fn_CheckPtr_draw) s_fn_CheckPtr_draw(mapBuf);
    uintptr_t mapComp = mapBuf[0];
    if (!mapComp) {
        if (trc) { fprintf(stderr, "[Decal]   bail: no MapComponent\n"); fflush(stderr); }
        return;
    }

    int minX   = *reinterpret_cast<int16_t*>(mapComp + 0x28);
    int minY   = *reinterpret_cast<int16_t*>(mapComp + 0x2a);
    int width  = *reinterpret_cast<int16_t*>(mapComp + 0x2c);
    int height = *reinterpret_cast<int16_t*>(mapComp + 0x2e);
    uint32_t rotation = *reinterpret_cast<uint32_t*>(mapComp + 0x30);

    // FindComponent(ModelComponent) → per-variant UV rect index (+0x164). This is the
    // per-building visual style StartTown randomises (ComponentEntity_impl_1fc0 in
    // retail); it selects which atlas cell of the tiled-decal material this building's
    // foundation samples. Defaults to 0 if no ModelComponent is found.
    int variant = 0;
    float mdlPos[3] = {0, 0, 0};   // building's rendered world pos (ModelComponent+0x148 Transform +0x10..0x18)
    int mdlVar166 = -1;
    void* mdlDesc = nullptr;
    if (s_fn_CheckPtr_draw) s_fn_CheckPtr_draw(reinterpret_cast<void*>(selfA + 0x10));
    void* modelComp = FindModelComponentOnOwner(*reinterpret_cast<void**>(selfA + 0x10));
    if (modelComp != nullptr) {
        uintptr_t mc = reinterpret_cast<uintptr_t>(modelComp);
        variant   = *reinterpret_cast<int16_t*>(mc + 0x164);
        mdlVar166 = *reinterpret_cast<int16_t*>(mc + 0x166);
        mdlPos[0] = *reinterpret_cast<float*>(mc + 0x148 + 0x10);
        mdlPos[1] = *reinterpret_cast<float*>(mc + 0x148 + 0x14);
        mdlPos[2] = *reinterpret_cast<float*>(mc + 0x148 + 0x18);
        mdlDesc   = *reinterpret_cast<void**>(mc + 0x18);
    }

    // DecalDescription = IsDerivedFrom(self+0x18, DecalDescription typeId) ? self+0x18 : null.
    void* obj = *reinterpret_cast<void**>(selfA + 0x18);
    uint32_t decalTid = DecalDescTypeId();
    void* decalDesc = nullptr;
    if (s_fn_IsDerivedFrom_dispatch && decalTid && obj &&
        s_fn_IsDerivedFrom_dispatch(obj, decalTid))
        decalDesc = obj;
    if (!decalDesc) {
        if (trc) { fprintf(stderr, "[Decal]   bail: obj=%p not DecalDescription (tid=0x%08x)\n",
                           obj, decalTid); fflush(stderr); }
        return;
    }
    uintptr_t dd = reinterpret_cast<uintptr_t>(decalDesc);

    // Decal alpha (+0x30 decalScale, used as decalScale*255 -> the vertex alpha /
    // shader lightFogAlpha.w). Retail lifecycle:
    //   * Serialize seeds it from DecalDescription._initialAlpha (dd+0x20 = 0.5) — the
    //     DURING-CONSTRUCTION (half-built, translucent foundation) value.
    //   * When the building COMPLETES, DecalComponent Slot12(msg=1) sets +0x30 = 1.0
    //     (0x3f800000) and re-adds the decal -> a fully-opaque finished foundation.
    // Neither lifecycle slot runs on our inert standalone components, and start-town
    // buildings spawn ALREADY COMPLETE, so seed the completed alpha (1.0) directly.
    // (Using _initialAlpha=0.5 drew every finished foundation at half opacity — the
    // washed-out look vs retail. Confirmed against retail RenderDoc event 1230:
    // vsout lightFogAlpha.w = 1.0.) A real construction pipeline would instead start
    // this at 0.5 and drive the Slot12(msg=1) ramp to 1.0 on completion.
    if (self->decalScale == 0.0f) self->decalScale = 1.0f;

    // Per-variant UV rect (defaults {0,0,uvScale,uvScale} when variant >= count).
    float uvScale = g_fUVScale ? *g_fUVScale : 1.0f;
    float uv[4] = {0.0f, 0.0f, uvScale, uvScale};
    int variantCount = *reinterpret_cast<int*>(dd + 0x40);
    if (variant < variantCount) {
        uintptr_t varr = *reinterpret_cast<uintptr_t*>(dd + 0x38);
        if (varr) {
            float* e = reinterpret_cast<float*>(varr + static_cast<uintptr_t>(variant) * 0x10);
            uv[0] = e[0]; uv[1] = e[1]; uv[2] = e[2]; uv[3] = e[3];
        }
    }

    uintptr_t gs = reinterpret_cast<uintptr_t>(*s_pGameStateDraw);
    uintptr_t decalSys = *reinterpret_cast<uintptr_t*>(gs + 0x110);
    if (!decalSys) {
        if (trc) { fprintf(stderr, "[Decal]   bail: no decalSys (gs+0x110)\n"); fflush(stderr); }
        return;
    }

    // material = DecalDescription material-array[decalState].
    uintptr_t matArray = *reinterpret_cast<uintptr_t*>(dd + 0x28);
    if (!matArray) {
        if (trc) { fprintf(stderr, "[Decal]   bail: no material array (dd+0x28) state=%u\n",
                           self->decalState); fflush(stderr); }
        return;
    }
    int64_t material = *reinterpret_cast<int64_t*>(matArray + static_cast<uintptr_t>(self->decalState) * 8);

    uint8_t  tiledFlag = *reinterpret_cast<uint8_t*>(dd + 0x24);
    uint8_t  p19       = *reinterpret_cast<uint8_t*>(dd + 0x25);
    uint32_t p10       = *reinterpret_cast<uint32_t*>(dd + 0x48);
    int alpha = static_cast<int>(self->decalScale * 255.0f);   // DAT_14017222c = 255.0f

    int id = TradeWork_impl_0d80(
        static_cast<int64_t>(decalSys), minX, minY,
        width - 1 + minX, height - 1 + minY,
        static_cast<uint8_t>(alpha), rotation, material, tiledFlag,
        p10,
        *reinterpret_cast<uint32_t*>(&uv[0]), *reinterpret_cast<uint32_t*>(&uv[1]),
        *reinterpret_cast<uint32_t*>(&uv[2]), *reinterpret_cast<uint32_t*>(&uv[3]),
        '\0', 0, 0, 0, p19);
    self->decalId = id;

    // Diagnostic (BANISHED_DECAL_TRACE) — throttled. Confirms the decal path fires and
    // whether AddDecal placed a slot (id>=0) or bailed (id==-1, e.g. null material).
    static int s_traceOn = -1;
    if (s_traceOn < 0) { const char* e = getenv("BANISHED_DECAL_TRACE");
                         s_traceOn = (e && *e && e[0] != '0') ? 1 : 0; }
    if (s_traceOn) {
        static int s_budget = 0;
        if (s_budget < 40) {
            ++s_budget;
            // Read back the tessellated slot geometry (slot = decalSys+0x1f0 base + id*0x60):
            //   +0x08 vtx base, +0x10 vtxCount, +0x18 idx base, +0x20 idxCount.
            // First vertex position shorts at vtx+0x00/+0x02/+0x04 (from caa0).
            // Also read the DIFFUSE UV int16 pair (vtx+0x08/+0x0A) for the first 3 verts — this
            // is what the tessellator T_1f0 transform writes; it must span ~0..4096 across the
            // footprint (VS scales by 1/4096). If it reads (0,0/1) the UV collapsed (the bug);
            // non-zero spread confirms the ace40 chain fix (uvRect + 4096 scale restored).
            long vtxCount = -1, idxCount = -1; int px = 0, py = 0, pz = 0; void* vb = nullptr;
            int uv0u = -1, uv0v = -1, uv1u = -1, uv1v = -1, uv2u = -1, uv2v = -1; int stride = 0x14;
            // texcoord.zw (vtx +0xC/+0xE) — the tiled-mask atlas rect written by cc10 from the
            // DAT_1401c23d0 LUT. For a tiled decal (tiled=1) these must be non-zero (~128..3968);
            // if 0 the LUT fallback didn't fire and the TiledDecal mask samples empty -> invisible.
            int zw0z = -1, zw0w = -1, zw1z = -1, zw1w = -1;
            // Objective geometry-health scan (confirms the ace40 dedup-index fix): the
            // maximum index must be < vtxCount (no out-of-range fetch) and the vertex
            // position bbox must hug the footprint (no stray vertex at the origin, which
            // was the "sliver to map center" signature).
            int maxIdx = -1, oobCount = -1; int vminX = 0, vmaxX = 0, vminY = 0, vmaxY = 0;
            if (id >= 0) {
                uintptr_t sbase = *reinterpret_cast<uintptr_t*>(decalSys + 0x1f0);
                if (sbase) {
                    uintptr_t slot = sbase + static_cast<uintptr_t>(id) * 0x60;
                    vb       = *reinterpret_cast<void**>(slot + 0x08);
                    vtxCount = *reinterpret_cast<int32_t*>(slot + 0x10);
                    idxCount = *reinterpret_cast<int32_t*>(slot + 0x20);
                    if (vb) { px = *reinterpret_cast<int16_t*>((uintptr_t)vb + 0x00);
                              py = *reinterpret_cast<int16_t*>((uintptr_t)vb + 0x02);
                              pz = *reinterpret_cast<int16_t*>((uintptr_t)vb + 0x04);
                              uv0u = *reinterpret_cast<int16_t*>((uintptr_t)vb + 0x08);
                              uv0v = *reinterpret_cast<int16_t*>((uintptr_t)vb + 0x0A);
                              uv1u = *reinterpret_cast<int16_t*>((uintptr_t)vb + stride + 0x08);
                              uv1v = *reinterpret_cast<int16_t*>((uintptr_t)vb + stride + 0x0A);
                              uv2u = *reinterpret_cast<int16_t*>((uintptr_t)vb + 2*stride + 0x08);
                              uv2v = *reinterpret_cast<int16_t*>((uintptr_t)vb + 2*stride + 0x0A);
                              zw0z = *reinterpret_cast<int16_t*>((uintptr_t)vb + 0x0C);
                              zw0w = *reinterpret_cast<int16_t*>((uintptr_t)vb + 0x0E);
                              zw1z = *reinterpret_cast<int16_t*>((uintptr_t)vb + stride + 0x0C);
                              zw1w = *reinterpret_cast<int16_t*>((uintptr_t)vb + stride + 0x0E);
                              vminX = vmaxX = px; vminY = vmaxY = py;
                              for (long vi = 0; vi < vtxCount && vi < 4096; ++vi) {
                                  int x = *reinterpret_cast<int16_t*>((uintptr_t)vb + vi*stride + 0x00);
                                  int y = *reinterpret_cast<int16_t*>((uintptr_t)vb + vi*stride + 0x02);
                                  if (x < vminX) vminX = x; if (x > vmaxX) vmaxX = x;
                                  if (y < vminY) vminY = y; if (y > vmaxY) vmaxY = y;
                              } }
                    uint16_t* ib = *reinterpret_cast<uint16_t**>(slot + 0x18);
                    if (ib && idxCount > 0) { maxIdx = 0; oobCount = 0;
                        for (long ii = 0; ii < idxCount && ii < 4096; ++ii) {
                            int v = ib[ii]; if (v > maxIdx) maxIdx = v;
                            if (v >= vtxCount) ++oobCount;
                        } }
                    // stored slot footprint (b0d80 wrote iVar2+tile here) + the decal
                    // system's centering fields (0xc0 = map dim, 0xc4 = ?). caa0 renders
                    // each vertex at storedTile - (0xc0>>1); if 0xc4 != 0xc0 the decal is
                    // shifted off its building by iVar2 = (0xc0-0xc4)>>1.
                    int c0 = *reinterpret_cast<int32_t*>(decalSys + 0xc0);
                    int c4 = *reinterpret_cast<int32_t*>(decalSys + 0xc4);
                    fprintf(stderr, "[Decal]   slotFP min(%d,%d) max(%d,%d) | f0xc0=%d f0xc4=%d "
                            "iVar2=%d  (tile min was %d,%d) | mdlPos=(%.1f,%.1f,%.1f) "
                            "decalV0world=(%d,%d) [expect ~mdlPos.x/z]\n",
                            *reinterpret_cast<int32_t*>(slot + 0x2c), *reinterpret_cast<int32_t*>(slot + 0x30),
                            *reinterpret_cast<int32_t*>(slot + 0x34), *reinterpret_cast<int32_t*>(slot + 0x38),
                            c0, c4, (c0 - c4) >> 1, minX, minY,
                            mdlPos[0], mdlPos[1], mdlPos[2], px, py);
                }
            }
            // p10 = dd+0x48 is the tessellator's UV *divisor* (ace40 cVar1==0 branch,
            // fVar_div): the per-tile UV is (uvRect / p10) * 4096. If p10 is large (or the
            // uvRect is the normalized {0,0,1,1} default while p10 expects texel units) the
            // UV collapses toward texel (0,0) -> the decal samples the empty corner of the
            // dirt-splat atlas -> transparent black. Logged to pin the exact bad input.
            float p10f = *reinterpret_cast<float*>(&p10);
            fprintf(stderr, "[Decal] Draw self=%p tile=(%d,%d %dx%d) rot=%d tiled=%d "
                    "mat=%p variant=%d/%d alpha=%d uv=(%.3f,%.3f,%.3f,%.3f) p10=%g(0x%08x) sys=%p id=%d "
                    "| MDL model=%p desc=%p var166=%d "
                    "| vtx=%ld idx=%ld vb=%p v0pos=(%d,%d,%d) uvINT16=[(%d,%d)(%d,%d)(%d,%d)] "
                    "zwINT16=[(%d,%d)(%d,%d)] "
                    "maxIdx=%d oob=%d posBBox=x[%d,%d]y[%d,%d]\n",
                    (void*)self, minX, minY, width, height, (int)rotation, (int)tiledFlag,
                    (void*)material, variant, variantCount, alpha,
                    uv[0], uv[1], uv[2], uv[3], p10f, p10, (void*)decalSys, id,
                    modelComp, mdlDesc, mdlVar166,
                    vtxCount, idxCount, vb, px, py, pz,
                    uv0u, uv0v, uv1u, uv1v, uv2u, uv2v,
                    zw0z, zw0w, zw1z, zw1w,
                    maxIdx, oobCount, vminX, vmaxX, vminY, vmaxY);
            fflush(stderr);
        }
    }
}

// ============================================================
// DecalComponent::OnCreate (vtable slot [7], 0x14011b510)
// ============================================================
// If decalId >= 0, removes the decal from terrain, resets decalId to -1.
void DecalComponent_OnCreate(DecalComponent* self) {
    if (self->decalId >= 0) {
        // Remove decal from terrain decal subsystem
        if (s_fn_TerrainDecalRemove && g_pGameWorldSingleton && *g_pGameWorldSingleton) {
            void* terrainDecalSys = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0x110);
            if (terrainDecalSys)
                s_fn_TerrainDecalRemove(terrainDecalSys, self->decalId);
        }
    }
    self->decalId = -1;
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void DecalComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    uintptr_t slot;

    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ac8);
    if (slot) s_fn_Serialize_Int = *reinterpret_cast<fn_Serialize_Int_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152a60);
    if (slot) s_fn_Serialize_Float = *reinterpret_cast<fn_Serialize_Float_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152a48);
    if (slot) s_fn_Serializer_GetVersion = *reinterpret_cast<fn_Serializer_GetVersion_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_dispatch = *reinterpret_cast<fn_IsDerivedFrom_dispatch_t*>(slot);

    s_TypeId_DecalDesc = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c75b8));

    s_fn_OriginalDraw = reinterpret_cast<fn_DecalDraw_t>(
        ResolveAddr(Module::Exe, 0x14011b560));

    s_fn_TerrainDecalRemove = reinterpret_cast<fn_TerrainDecalRemove_t>(
        ResolveAddr(Module::Exe, 0x1400b0d00));

    // Sub-callees for our DecalComponent_TerrainDecalRemove reimpl (registered leaves).
    s_fn_ResizeStride14 = reinterpret_cast<fn_ResizeStride14_t>(
        ResolveAddr(Module::Exe, 0x14001ee40));
    s_fn_ResizeStride2 = reinterpret_cast<fn_ResizeStride2_t>(
        ResolveAddr(Module::Exe, 0x14001c890));

    s_initialized = true;

#ifdef _DEBUG
    printf("[DecalComponent] Funcs resolved: CompSerialize=%p Draw=%p DecalRemove=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_OriginalDraw,
           (void*)s_fn_TerrainDecalRemove);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// size 0x38 (DecalComponent_Factory MemAlloc), flags 0xc3 (Ctor 0x14011b8f0:
// Component base sets +0x20=0x80, leaf OR 0xc3 => 0xc3).
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

// Building ground-decal rendering (BANISHED_LIVE_DECAL, default OFF — like
// BANISHED_LIVE_STORAGEDISPLAY). A building's DecalDescription (e.g. StorageYard's
// brown dirt "foundation" — StorageYard.rsc `decal` -> Terrain/TiledDecals/
// StorageYardMaterial.rsc) is painted onto the terrain by DecalComponent::Draw, which
// AddDecal's it into the terrain-decal system at gameWorld+0x110. That slot array is
// walked + submitted every frame by GeomDraw_CollectDraw (geometry_draw.cpp), so the
// decal only needs to be BUILT ONCE.
//
// DRIVER NOTE (tree-driven, commit 85845b9): the retail decal build is driven by the
// component lifecycle (OnCreate/OnAdded via the Fourth UpdatePriority bucket). In
// standalone that bucket IS reachable once the owner entity is bucketed, so the build is
// driven through the faithful tree walk (no explicit per-frame drain):
//   * Startup slot (index 4 / +0x20) — dispatched by ComponentEntity::StartupComponents
//     at spawn — sets the draw flag (+0x20 bit 0x4) + c3470-rebuckets the owner, letting
//     the OnAdded pass dispatch slot 6 (Draw = AddDecal).
//   * The new +0x40 slot (index 8) retries the build while decalId < 0 (a still-loading
//     material leaves decalId == -1), then un-buckets once placed (clears bit 0x4 + c3470)
//     — build-once. Once AddDecal returns a slot index, decalId >= 0 and it is skipped.
// Default (env off) => nothing wired => the default sa-run stays byte-identical.
static uint32_t DecalComponent_LiveGetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("DecalComponent");
    return cached;
}
static void* DecalComponent_LiveVtable[::ComponentFactory::kInertVtableSlots];

// Bucket (or un-bucket) this decal's owning entity into the update tree via the draw flag
// (component +0x20 bit 0x4), so the EntityList tree walk dispatches the component's +0x40
// slot. c3470 (RefreshEntityFlags) re-aggregates the owner's flags and (de)registers it;
// it no-ops safely if the entityList isn't set yet (the flag itself is then picked up by
// the later EntityList registration). Setting the flag ⇒ eventual bucketing either way.
static void DecalComponent_SetDrawFlag(void* s, bool on) {
    EnsureDrawHelpers();
    uintptr_t c = reinterpret_cast<uintptr_t>(s);
    uint32_t oldFlags = *reinterpret_cast<uint32_t*>(c + 0x20);
    uint32_t newFlags = on ? (oldFlags | 0x4u) : (oldFlags & ~0x4u);
    if (newFlags == oldFlags) return;
    *reinterpret_cast<uint32_t*>(c + 0x20) = newFlags;
    if (((newFlags ^ oldFlags) & 0xFFFF) != 0 && *reinterpret_cast<void**>(c + 0x10)) {
        if (s_fn_CheckPtr_draw) s_fn_CheckPtr_draw(reinterpret_cast<void*>(c + 0x10));
        if (s_fn_EntityFlagUpdate) s_fn_EntityFlagUpdate(*reinterpret_cast<void**>(c + 0x10));
    }
}

// index 4 — Startup(self): bucket the owner so the tree walk drives the build-once.
static void DecalComponent_StartupVSlot(void* s) {
    DecalComponent_SetDrawFlag(s, true);
    if (DecalTraceOn()) {
        static int n = 0;
        if (n < 8) { ++n; fprintf(stderr, "[Decal] Startup bucketed self=%p\n", s); fflush(stderr); }
    }
}
// index 6 — Draw(self): build + submit the terrain decal.
static void DecalComponent_DrawVSlot(void* s) {
    DecalComponent_Draw(reinterpret_cast<DecalComponent*>(s));
}
// index 7 — OnCreate(self): release the decal slot (already reimplemented).
static void DecalComponent_OnCreateVSlot(void* s) {
    DecalComponent_OnCreate(reinterpret_cast<DecalComponent*>(s));
}
// index 8 (+0x40) — per-frame draw-tick, dispatched by the EntityList tree walk once the
// owner is bucketed. Build-once: attempt the build until placed (Draw self-guards if the
// scene/material aren't ready), then un-bucket so the tree stops ticking this component
// (the placed decal is thereafter rendered by GeomDraw_CollectDraw, not the update tree).
static void DecalComponent_TickVSlot(void* s, void* /*time*/) {
    DecalComponent* cc = reinterpret_cast<DecalComponent*>(s);
    if (cc->decalId < 0) DecalComponent_Draw(cc);   // retry the build until the material is ready
    if (cc->decalId >= 0) DecalComponent_SetDrawFlag(s, false);   // placed → un-bucket
}
static void DecalComponent_FinishLiveVtable() {
    if (::ComponentFactory::LiveEnabled("BANISHED_LIVE_DECAL")) {
        DecalComponent_LiveVtable[4] = reinterpret_cast<void*>(&DecalComponent_StartupVSlot);
        DecalComponent_LiveVtable[6] = reinterpret_cast<void*>(&DecalComponent_DrawVSlot);
        DecalComponent_LiveVtable[7] = reinterpret_cast<void*>(&DecalComponent_OnCreateVSlot);
        DecalComponent_LiveVtable[8] = reinterpret_cast<void*>(&DecalComponent_TickVSlot);
    }
}
extern "C" void* DecalComponent_Factory(void) {
    void* p = ::ComponentFactory::MakeInertTyped(0x38, 0xc3,
        DecalComponent_LiveVtable,
        reinterpret_cast<void*>(&DecalComponent_LiveGetTypeId));
    DecalComponent_FinishLiveVtable();
    return p;
}
static void* DecalComponent_CreateInPlace(void* mem) {
    if (mem) ::ComponentFactory::InitInertTyped(mem, 0x38, 0xc3,
        DecalComponent_LiveVtable,
        reinterpret_cast<void*>(&DecalComponent_LiveGetTypeId));
    DecalComponent_FinishLiveVtable();
    return mem;
}
REGISTER_TYPE(DecalComponent, "Component",
              &DecalComponent_Factory, &DecalComponent_CreateInPlace);

#endif // STANDALONE_EXE
