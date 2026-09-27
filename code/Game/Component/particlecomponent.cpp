// code/Game/Component/particlecomponent.cpp
// Clean reimplementation of ParticleComponent lifecycle methods from Application-x64.exe.
//
// ParticleComponent (0x48 bytes) manages particle effects attached to entities.
// Fields: +0x28=emitter flag array, +0x30=count, +0x38=geometry array, +0x40=total count
//
// Vtable at 0x14015a290. Six non-trivial overrides — all deeply coupled to
// Graphics::Scene, MeshInstance, and particle emitter subsystems.
//
// Due to deep Graphics subsystem dependencies (MeshInstance::GetNodeToWorld,
// Scene::RemoveObject, Geometry::SetTransform, etc.), several methods delegate
// to original exe functions for the complex Graphics work while the serialization
// and control flow logic is reimplemented.
//

// graphics/*.h FIRST: they pull runtime/types/volume/Volume.h, whose global-scope
// `struct Rectangle` is hidden by <wingdi.h>'s `Rectangle()` once <windows.h> has
// been seen through resolve.h.  ENDGAME B.3.5 caller batch I2.
#include "graphics/Scene.h"
#include "Component/ParticleDescription.h"
#include "graphics/Mesh.h"
#include "graphics/GraphicsMesh.h"
#include <new>
#include "Component/particlecomponent_funcs.h"
#include "Component/coll_bridge.h"
#include "scene_watch.h"
#include "engine_funcs.h"
#include "system/String.h"
#include "math/Math.h"
#include "resolve.h"
#include "perf_experimental.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include "io/Stream.h"      // ENDGAME B.3.4 callers batch H: IO::Stream::GetDirection
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <new>
#include "graphics/ParticleInstance.h"   // ENDGAME B.3.9 SS2: ParticleInstance::_frameIndex
#include "engine/MainLoop.h"   // ENDGAME B.3.9 SS2: Engine::MainLoop::_singleton

#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName (SA type-id fallback)
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch

// ============================================================
// Resolved helpers
// ============================================================

// FUN_14000a590 — resize bool array

// IsDerivedFrom
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* obj, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_d = nullptr;

// ParticleComponent_sub_2480 — particle setup helper
typedef void (*fn_ParticleSetup_t)(void* self);
static fn_ParticleSetup_t s_fn_ParticleSetup = nullptr;

// ParticleComponent_helper_2a50 — update emitter position
typedef void (*fn_UpdateEmitterPos_t)(void* self, int index);
static fn_UpdateEmitterPos_t s_fn_UpdateEmitterPos = nullptr;

// ParticleComponent_helper_2990 — emitter tick
typedef void (*fn_EmitterTick_t)(void* self, int index);
static fn_EmitterTick_t s_fn_EmitterTick = nullptr;

// Graphics::Scene::RemoveObject

// FUN_1400c3470 — entity flag update
typedef void (*fn_EntityFlagUpdate_t)(void* entity);
static fn_EntityFlagUpdate_t s_fn_EntityFlagUpdate = nullptr;

// FUN_140053660 — emitter startup
typedef void (*fn_EmitterStartup_t)(void* self, int index);
static fn_EmitterStartup_t s_fn_EmitterStartup = nullptr;

// The original OnRemove is too complex for clean reimpl — use direct call

static float* s_pUVScale = nullptr;  // g_fUVScale (constant 1.0)
static float PC_UVScale();  // defined with the helper ports below

static bool s_initialized = false;

// Entity re-bucket — FAITHFUL retail behavior, now the SOLE per-frame tick driver.
// Retail's StartEmitter/StopEmitter set the component's draw flag (self+0x20 bit 0x4)
// and call RefreshEntityFlags (c3470), which re-aggregates the OWNING entity's flags
// and registers it into the per-frame component-update tree; the tree walk then
// dispatches this component's +0x40 slot (PC_TickVSlot) every frame. Earlier this was
// suppressed in SA because re-bucketing a house woke its broken workplace/status
// evaluation (spurious "no workers"/?/cold icons); that eval is now fixed (the building
// "no workers" warning is raised only by WorkPlaceComponent_SetAccepting on an unstaffed
// active workplace), so the re-bucket is safe AND required to drive the tick. Always on.
static bool PC_RebucketEntity() { return true; }

// ParticleDescription type id — cell 0x1401c4758 was unregistered (ResolveAddr
// always returned NULL), so the raw read fell back to ParticleDescription::TypeId()
// unconditionally; converting by cell (D5) is mechanically equivalent to the old
// fallback path.
static uint32_t PC_DescTypeId() {
    const uint32_t id = ParticleDescription::TypeId();
    if (id != 0 && id != 0xFFFFFFFF) return id;
    return 0;
}

// Helper: get particle description from _unk18
static void* GetParticleDesc(void* self) {
    void* baseObj = *(void**)((uintptr_t)self + 0x18);
    if (!baseObj) return nullptr;
    // +0x18 is the description sub-block the component was instantiated from;
    // the SA factory tree can false-negative IsDerivedFrom, so trust non-null
    // (Object::IsDerivedFromOrTrust convention).
    return Object::IsDerivedFromOrTrust(baseObj, PC_DescTypeId()) ? baseObj : nullptr;
}







// ============================================================
// Full ports of the exe helper closure.
// Decompiled from Application-x64.exe:
//   0x140052480  Setup        — (re)build geometry+flag arrays, create one
//                               Graphics::ParticleSystemInstance per emitter
//                               via the referenced ParticleSystem resource's
//                               vtable+0x40 (CreateInstance, Runtime-side)
//   0x140052560  Place        — position emitter i: model transform / map-cell
//                               terrain pos / identity, then attach-point table
//                               (desc+0x20) or mesh-node lookup by name
//   0x140052990  StopEmitter  — Scene::RemoveObject + flag bookkeeping
//   0x140052a50  StartEmitter — Place + Update(0) + Scene::AddObject + flags
//   0x140053660  StartFade    — fade-in rate = 1/t1, opacity=0, StartEmitter
//   0x140052b30  UpdateTick   — per-frame (vtable+0x40): node-follow transform
//                               + opacity fade integration + geometry Update
//   0x140053090  Event        — event 2 = fade-in start, 3 = fade-out (b2 set)
// ============================================================

// Geometry-array entry (self+0x38 array, stride 0x18; count at self+0x40)
struct PCGeoEntry {
    void*   geo;       // +0x00 Graphics::ParticleSystemInstance (Geometry subclass)
    int32_t sceneId;   // +0x08 Graphics::Scene object id, -1 = not in scene
    int32_t nodeIdx;   // +0x0c mesh-node / attach-point index to follow, -1 = static
    float   fadeRate;  // +0x10 opacity units/sec (0 = steady)
    int32_t _pad14;    // +0x14
};
static_assert(sizeof(PCGeoEntry) == 0x18, "PCGeoEntry stride");

// Emitter entry in ParticleDescription (+0x28 array, stride 0x28) — layout per
// code/Game/Component/particledescription.cpp:
//   +0x00 External* particleSystem, +0x08 String name, +0x10 b0 followNode,
//   +0x11 b1 autoStart, +0x12 b2 eventToggled, +0x13 b3 noModelTransform,
//   +0x18 Time t0 prewarm, +0x20 Time t1 fadeDuration

// --- extra resolved deps for the ports ---
// FindComponent helpers (exe): 0x140003f60 = ModelComponent, 0x140004680 = MapComponent.
// (entity, retbuf IBP) — retbuf[0] receives the component pointer (or 0).
typedef void* (*fn_PCFindComponent_t)(uintptr_t entity, void* retIBP);
static fn_PCFindComponent_t s_fn_FindComp_Model = nullptr;
static fn_PCFindComponent_t s_fn_FindComp_Map = nullptr;


// g_fUVScale is the retail .rdata constant 1.0f. In SA the ResolveAddr for
// 0x140172234 lands inside an unrelated registered data blob (observed value
// 300.0 → fade rate 300/s, opacity 15+ → permanently opaque smoke), so use
// the known retail value directly.
static float PC_UVScale() { return 1.0f; }

// Owning entity from the component's +0x10 InternalBasePtr.
static uintptr_t PC_Entity(void* self) {
    void* ibp = (void*)((uintptr_t)self + 0x10);
    if (*(uintptr_t*)ibp == 0) return 0;
    return *(uintptr_t*)ibp;
}

static void* PC_FindComp(fn_PCFindComponent_t fn, uintptr_t entity) {
    if (!fn || !entity) return nullptr;
    uintptr_t buf[2] = {0, 0};
    fn(entity, buf);
    return (void*)buf[0];
}

// Geometry-array (re)size — port of FUN_14000a640 (stride 0x18).
// Header at self+0x38: {void* data(+0), int count(+8), int cap(+0xc)}.
// New entries init to {geo=0, sceneId=-1, nodeIdx=0, fadeRate=0}.
static void PC_GeoArrayResize(void* header, int newCount) {
    uint8_t* h = (uint8_t*)header;
    int oldCount = *(int32_t*)(h + 8);
    if (newCount == oldCount) return;

    uint8_t* oldData = *(uint8_t**)h;
    *(int32_t*)(h + 8) = newCount;
    if (*(int32_t*)(h + 0xc) < newCount) {
        int cap = (newCount == oldCount + 1 && oldCount != 0) ? newCount * 3 : newCount;
        *(int32_t*)(h + 0xc) = cap;
        *(void**)h = (fn_MemAlloc && g_pMemAllocSingleton)
                     ? fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)cap * 0x18, 8)
                     : nullptr;
    }
    uint8_t* data = *(uint8_t**)h;
    if (!data) { *(int32_t*)(h + 8) = 0; *(int32_t*)(h + 0xc) = 0; return; }
    int keep = (oldData == data) ? oldCount : 0;  // in-place: old entries stay valid
    for (int i = 0; i < newCount; i++) {
        PCGeoEntry* e = (PCGeoEntry*)(data + (size_t)i * 0x18);
        if (i < keep) continue;
        if (i < oldCount && oldData) {
            memcpy(e, oldData + (size_t)i * 0x18, 0x18);
        } else {
            e->geo = nullptr; e->sceneId = -1; e->nodeIdx = 0;
            e->fadeRate = 0.0f; e->_pad14 = 0;
        }
    }
    if (oldData && oldData != data && fn_MemFreeStatic) fn_MemFreeStatic(oldData);
}

// ============================================================
// ParticleComponent_helper_2560 — place emitter i (0x140052560)
// ============================================================
extern "C" void ParticleComponent_Place(ParticleComponent* self, int index) {
    uintptr_t s = (uintptr_t)self;
    void* desc = GetParticleDesc(self);
    if (!desc) return;
    uint8_t* elem = *(uint8_t**)((uintptr_t)desc + 0x28) + (size_t)index * 0x28;
    PCGeoEntry* geoArr = *(PCGeoEntry**)(s + 0x38);
    if (!geoArr) return;
    PCGeoEntry* ge = &geoArr[index];
    if (!ge->geo) return;   // resource failed to load — leave emitter inert

    uintptr_t entity = PC_Entity(self);
    void* model = PC_FindComp(s_fn_FindComp_Model, entity);
    void* mapc  = PC_FindComp(s_fn_FindComp_Map, entity);

    // Transform = quat(x,y,z,w) at +0x00, pos(x,y,z) at +0x10 — 7 floats.
    float tf[7] = {0, 0, 0, PC_UVScale(), 0, 0, 0};   // identity
    if (model && elem[0x13] == 0) {
        // Building/entity model transform (+0x148)
        memcpy(tf, (uint8_t*)model + 0x148, 7 * sizeof(float));
    } else if (mapc && g_pGameWorldSingleton && *g_pGameWorldSingleton && fn_MapGetTerrainPos) {
        // Centre cell of the entity's map footprint → terrain-height world pos
        void* mapDims = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0xf8);
        int W = mapDims ? *(int*)mapDims : 0;
        if (W > 0) {
            uintptr_t m = (uintptr_t)mapc;
            int idx = ((int)*(int16_t*)(m + 0x2a) + ((int)*(int16_t*)(m + 0x2e) >> 1)) * W
                    + ((int)*(int16_t*)(m + 0x2c) >> 1) + (int)*(int16_t*)(m + 0x28);
            float half = (float)(W >> 1) - 0.5f;
            float in[3]  = {(float)(idx % W) - half, (float)(idx / W) - half, 0.0f};
            float out[3] = {0, 0, 0};
            fn_MapGetTerrainPos(mapDims, out, in, '\0', nullptr);
            tf[4] = out[0]; tf[5] = out[1]; tf[6] = out[2];
        }
    }

    float pos[3] = {tf[4], tf[5], tf[6]};
    int followIdx = -1;
    void* attach = *(void**)((uintptr_t)desc + 0x20);
    if (attach) {
        // Named attach-point table: entries {String name(+0), Vector3 offset(+8)}, stride 0x18
        int n = *(int32_t*)((uintptr_t)attach + 0x38);
        uint8_t* arr = *(uint8_t**)((uintptr_t)attach + 0x30);
        for (int j = 0; j < n && arr; j++) {
            // System::String::operator==(const String&) — slot 0x140151438.
            if (*reinterpret_cast<const System::String*>(arr + (size_t)j * 0x18) ==
                *reinterpret_cast<const System::String*>(elem + 8)) {   // B.3.1 bridge
                const float zero = 0.0f;
                const Math::Transform ypo = Math::Transform::FromYawPitchOffset(
                    zero, zero,
                    *reinterpret_cast<const Math::Vector3*>(arr + (size_t)j * 0x18 + 8));   // B.3.1 bridge
                // Math::Transform::operator*(const Transform&) — slot 0x140151430.
                const Math::Transform prod =
                    ypo * *reinterpret_cast<const Math::Transform*>(tf);   // B.3.1 bridge
                pos[0] = prod.translation.x; pos[1] = prod.translation.y; pos[2] = prod.translation.z;
                followIdx = j;
                break;
            }
        }
    } else if (model) {
        // Mesh-node lookup by emitter name (e.g. a "smoke" node in the building mesh)
        void* mesh = *(void**)((uintptr_t)model + 0xe8);
        char* name = *(char**)(elem + 8);
        if (mesh && name && *(int32_t*)(name - 8) != 0) {
            int n = reinterpret_cast<const Graphics::GraphicsMesh*>(mesh)
                        ->GetNodeIndexFromName(name);   // B.3.5 bridge
            if (n >= 0) {
                // GetNodeToWorld is a hidden-retslot door in retail; the door body
                // moves to the call site as ONE construction into the same storage.
                float ntw[7] = {0, 0, 0, 1, 0, 0, 0};
                ::new (static_cast<void*>(ntw)) Math::Transform(
                    reinterpret_cast<const Graphics::MeshInstance*>((uint8_t*)model + 0x28)
                        ->GetNodeToWorld(n));   // B.3.5 bridge
                pos[0] = ntw[4]; pos[1] = ntw[5]; pos[2] = ntw[6];
                followIdx = n;
            }
        }
    }

    // Final placement: identity rotation + computed position
    float finalTf[7] = {0, 0, 0, PC_UVScale(), pos[0], pos[1], pos[2]};
    reinterpret_cast<Graphics::Geometry*>(ge->geo)
        ->SetTransform(*reinterpret_cast<const Math::Transform*>(finalTf));   // B.3.5 bridge

    // Configure the instance from its ParticleSystem resource (vtable+0x48)
    void* res = *(void**)(elem + 0);
    void** vt = *(void***)ge->geo;
    if (res && vt && vt[9]) {
        typedef void (*fn_setSystem_t)(void*, void*);
        ((fn_setSystem_t)vt[9])(ge->geo, res);
    }

    ge->sceneId = -1;
    ge->nodeIdx = (elem[0x10] != 0) ? followIdx : -1;


    // Prewarm: tick the emitter forward by t0 (elem+0x18) in engine fixed steps
    int64_t t0 = *(int64_t*)(elem + 0x18);
    if (t0 > 0 && ::Engine::MainLoop::_singleton && vt && vt[1]) {
        int64_t step = *(int64_t*)((uintptr_t)::Engine::MainLoop::_singleton + 0x48);
        if (step > 0) {
            typedef void (*fn_geoUpdate_t)(void*, void*);
            for (int64_t acc = step; ; acc += step) {
                ((fn_geoUpdate_t)vt[1])(ge->geo, &step);
                if (acc >= t0) break;
            }
        }
    }
}

// ============================================================
// ParticleComponent_helper_2990 — stop emitter i (0x140052990)
// ============================================================
extern "C" void ParticleComponent_StopEmitter(ParticleComponent* self, int index) {
    uintptr_t s = (uintptr_t)self;
    PCGeoEntry* geoArr = *(PCGeoEntry**)(s + 0x38);
    if (!geoArr) return;
    PCGeoEntry* ge = &geoArr[index];
    if (ge->sceneId >= 0) {
        if (g_pGameWorldSingleton && *g_pGameWorldSingleton) {
            void* scene = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0x60);
            if (scene)
                reinterpret_cast<Graphics::Scene*>(scene)
                    ->RemoveObject((uint32_t)ge->sceneId);   // B.3.5 bridge
        }
        ge->sceneId = -1;
    }
    uint8_t* flagArr = *(uint8_t**)(s + 0x28);
    if (flagArr) flagArr[index] = 0;

    // Any emitter still live? Keep the per-frame flag.
    int total = *(int32_t*)(s + 0x40);
    for (int j = 0; j < total; j++)
        if (geoArr[j].sceneId >= 0) return;

    if (!PC_RebucketEntity()) return;
    uint32_t oldFlags = *(uint32_t*)(s + 0x20);
    uint32_t newFlags = oldFlags & ~0x4u;
    *(uint32_t*)(s + 0x20) = newFlags;
    if (((newFlags ^ oldFlags) & 0xFFFF) != 0 && *(uintptr_t*)(s + 0x10) != 0) {
        if (s_fn_EntityFlagUpdate) s_fn_EntityFlagUpdate(*(void**)(s + 0x10));
    }
}

// ============================================================
// ParticleComponent_helper_2a50 — start emitter i (0x140052a50)
// ============================================================
extern "C" void ParticleComponent_StartEmitter(ParticleComponent* self, int index) {
    uintptr_t s = (uintptr_t)self;
    PCGeoEntry* geoArr = *(PCGeoEntry**)(s + 0x38);
    if (!geoArr) return;
    PCGeoEntry* ge = &geoArr[index];

    void* scene = (g_pGameWorldSingleton && *g_pGameWorldSingleton)
                  ? *(void**)((uintptr_t)*g_pGameWorldSingleton + 0x60) : nullptr;
    if (ge->sceneId >= 0 && scene)
        reinterpret_cast<Graphics::Scene*>(scene)
            ->RemoveObject((uint32_t)ge->sceneId);   // B.3.5 bridge

    ParticleComponent_Place(self, index);
    if (!ge->geo || !scene) return;

    // Initial Update(Time(0)) then add to the scene
    int64_t zeroTime = 0;
    void** vt = *(void***)ge->geo;
    if (vt && vt[1]) {
        typedef void (*fn_geoUpdate_t)(void*, void*);
        ((fn_geoUpdate_t)vt[1])(ge->geo, &zeroTime);
    }
    {
        ge->sceneId = (int32_t)reinterpret_cast<Graphics::Scene*>(scene)
            ->AddObject(reinterpret_cast<Graphics::Geometry*>(ge->geo));   // B.3.5 bridge
        SceneWatch_Note((uint32_t)ge->sceneId, ge->geo, "ParticleComponent");
    }

    if (PC_RebucketEntity()) {
        uint32_t oldFlags = *(uint32_t*)(s + 0x20);
        uint32_t newFlags = oldFlags | 0x4u;
        *(uint32_t*)(s + 0x20) = newFlags;
        if (((newFlags ^ oldFlags) & 0xFFFF) != 0 && *(uintptr_t*)(s + 0x10) != 0) {
            if (s_fn_EntityFlagUpdate) s_fn_EntityFlagUpdate(*(void**)(s + 0x10));
        }
    }
    uint8_t* flagArr = *(uint8_t**)(s + 0x28);
    if (flagArr) flagArr[index] = 1;

}

// ============================================================
// FUN_140053660 — start emitter i with fade-in (0x140053660)
// ============================================================
extern "C" void ParticleComponent_StartEmitterFade(ParticleComponent* self, int index) {
    uintptr_t s = (uintptr_t)self;
    void* desc = GetParticleDesc(self);
    if (!desc) return;
    PCGeoEntry* geoArr = *(PCGeoEntry**)(s + 0x38);
    if (!geoArr) return;
    PCGeoEntry* ge = &geoArr[index];

    uint8_t* elem = *(uint8_t**)((uintptr_t)desc + 0x28) + (size_t)index * 0x28;
    float t1 = reinterpret_cast<const System::Time*>(elem + 0x20)->AsSeconds();   // B.3.4b bridge
    ge->fadeRate = PC_UVScale() / t1;

    if (ge->geo) {
        uintptr_t g = (uintptr_t)ge->geo;
        if (*(float*)(g + 0x7c) != 0.0f) {
            *(uint32_t*)(g + 0x2c) |= 4;      // dirty
            *(uint64_t*)(g + 0x7c) = 0;       // opacity (both floats) = 0
        }
    }
    ParticleComponent_StartEmitter(self, index);
}

// ============================================================
// ParticleComponent_sub_2480 — setup (0x140052480)
// ============================================================
extern "C" void ParticleComponent_Setup(ParticleComponent* self) {
    uintptr_t s = (uintptr_t)self;
    void* desc = GetParticleDesc(self);
    if (!desc) {
        return;
    }
    int n = *(int32_t*)((uintptr_t)desc + 0x30);
    PC_GeoArrayResize((void*)(s + 0x38), n);
    // B.3.2 bridge: FUN_14000a590 is Collection::Array<uint8_t>::SetSize(n)
    GameColl::AsArray<GameColl::U8>((void*)(s + 0x28)).SetSize(n);

    uint8_t* emArr = *(uint8_t**)((uintptr_t)desc + 0x28);
    PCGeoEntry* geoArr = *(PCGeoEntry**)(s + 0x38);
    int total = *(int32_t*)(s + 0x40);
    if (!emArr || !geoArr) return;
    int nullRes = 0, nullInst = 0;
    for (int i = 0; i < total; i++) {
        void* res = *(void**)(emArr + (size_t)i * 0x28);
        void* inst = nullptr;
        if (res) {
            // ParticleSystem resource vtable+0x40 = CreateInstance (Runtime-side)
            void** rvt = *(void***)res;
            if (rvt && rvt[8]) {
                typedef void* (*fn_createInst_t)(void*);
                inst = ((fn_createInst_t)rvt[8])(res);
            }
        }
        // A null here is FATAL LATER, not here: ParticleComponent_Draw (slot 6 OnAdded)
        // reads geoArr[i].geo->+0x7C with no test, exactly as retail 0x140053029 does,
        // because in retail this slot is always populated. Say so at the site that
        // produced the null, or the only evidence is an AV reading address 0x7c.
        if (!res)  ++nullRes;
        if (!inst) ++nullInst;
        geoArr[i].geo = inst;
        ParticleComponent_Place(self, i);
    }
    if (nullRes || nullInst) {
        static int s_pcNullBudget = 0;
        if (s_pcNullBudget < 16) {
            s_pcNullBudget++;
            fprintf(stderr, "[Particle] EMPTY GEOMETRY self=%p emitters=%d "
                    "nullResource=%d nullInstance=%d -- slot 6 OnAdded (Draw) WILL fault "
                    "reading 0x7c on the first such emitter. nullResource!=0 means the "
                    "ParticleAttachment _particle external never resolved; nullInstance "
                    "with nullResource==0 means the resource resolved but its vtable+0x40 "
                    "CreateInstance returned null.\n",
                    (void*)self, total, nullRes, nullInst);
            fflush(stderr);
        }
    }
}



// ============================================================
// Init: resolve function pointers
// ============================================================
void ParticleComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_ParticleSetup = reinterpret_cast<fn_ParticleSetup_t>(
        ResolveAddr(Module::Exe, 0x140052480));
    s_fn_UpdateEmitterPos = reinterpret_cast<fn_UpdateEmitterPos_t>(
        ResolveAddr(Module::Exe, 0x140052a50));
    s_fn_EmitterTick = reinterpret_cast<fn_EmitterTick_t>(
        ResolveAddr(Module::Exe, 0x140052990));
    s_fn_EntityFlagUpdate = reinterpret_cast<fn_EntityFlagUpdate_t>(
        ResolveAddr(Module::Exe, 0x1400c3470));
    s_fn_EmitterStartup = reinterpret_cast<fn_EmitterStartup_t>(
        ResolveAddr(Module::Exe, 0x140053660));

    s_pUVScale = reinterpret_cast<float*>(
        ResolveAddr(Module::Exe, 0x140172234));  // g_fUVScale

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_d = *reinterpret_cast<fn_IsDerivedFrom_dispatch_t*>(slot);

    // --- deps for the full helper-closure ports ---
    s_fn_FindComp_Model = reinterpret_cast<fn_PCFindComponent_t>(
        ResolveAddr(Module::Exe, 0x140003f60));   // FindComponent(ModelComponent)
    s_fn_FindComp_Map = reinterpret_cast<fn_PCFindComponent_t>(
        ResolveAddr(Module::Exe, 0x140004680));   // FindComponent(MapComponent)


    s_initialized = true;
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// Building particle effects (chimney smoke etc). Gated on BANISHED_LIVE_PARTICLE
// (or global the global live-components switch); default (env off) stays inert.
//
// Live slot map (retail vtable 0x14015a290):
//   [4]  +0x20 Startup   (0x140053230) — Setup: create ParticleSystemInstances
//   [6]  +0x30 OnAdded   (0x140052f70) — fade-in start of autoStart(b1) emitters
//   [7]  +0x38 OnRemoved (0x140052e80) — remove all emitters from the scene
//   [8]  +0x40 per-frame (0x140052b30) — node-follow + fade + geometry Update
//   [11] +0x58 OnUpdate2 (0x1400531b0) — setup + restart flagged emitters
//   [12] +0x60 Event     (0x140053090) — event 2/3 fade-in/out (b2 emitters)
// Slot [3] (+0x18 save/load serialize) IS installed -- see
// ParticleComponent_FinishLiveVtable below, which sets it unconditionally,
// outside the live gate, because serializer wiring is not a feature flag.
// (This note previously claimed the slot stayed inert "because the bool array
// is rebuilt by Startup anyway". That was stale -- it outlived the a11d083
// fix 120 lines below -- and it was wrong in principle as well: a loader must
// CONSUME the bytes the writer emitted regardless of whether the data can be
// reconstructed. "Rebuilt by Startup" would justify DISCARDING a value after
// reading it, never SKIPPING the read.
// See [[reference_null_guard_on_a_serializer_call_is_a_desync]] and
// [[reference_stub_comment_outlived_the_port_that_replaced_it]].)
//
// ENDGAME B.3.8 CIT: ParticleComponent is a REAL class now (ParticleComponent.h) —
// the 48-slot ParticleComponent_LiveVtable, its per-create fills and
// Inert_ScalarDtor slot 0 are gone; the compiler's 16-slot vtable carries the
// slots above, with a real ctor/dtor chain (B38_RULES D1, D2).
#include "Object/factory_registration.h"
#include "Component/ParticleComponent.h"
#include "address_registry.h"

// ---- Per-frame frame-index un-stamp (Runtime-semantics fix, NOT a tick driver) ----
// The tick itself is now driven faithfully by the EntityList tree walk: StartEmitter's
// c3470 re-bucket registers the owning entity into the update tree, and the walk
// (EntityList_TreeNodeHandler) dispatches this component's +0x40 slot (PC_TickVSlot)
// every frame. The one thing SA still lacks is the per-render-frame advance of the
// shared Graphics::ParticleInstance::_frameIndex (Runtime static @0x1801fe77c) that
// retail's Renderer::Render performs; without it ParticleSystemInstance::Update dedups
// forever (emitState+0x2c == fi) and the sim freezes. Advancing the GLOBAL counter is
// not an option — the status-icon billboards share the mechanism and were built around
// the frozen counter (advancing it makes icons re-simulate as drifting particles).
// Instead, right before each tick, un-stamp THIS component's own live emitters so
// exactly one instance per state simulates. Blast radius = this component's smoke.
extern "C" int GameUpdateLoop_CurrentFrame();  // per-render-frame token (read-only)

static void PC_UnstampOwnEmitters(ParticleComponent* self) {
    if (!self) return;
    int32_t fi = ::Graphics::ParticleInstance::_frameIndex;

    // Per-frame emit dedup. The 6 founding houses share only ~3 POOLED emit-states
    // (verified: emit-diag showed one emit ref'd by 3 houses). Un-stamping a shared
    // emit once per referencing house would re-simulate it Nx/frame (smoke Nx too
    // fast). Instead un-stamp each DISTINCT emit at most once per render frame; the
    // first house's tick then simulates it and the rest dedup (emit+0x2c == fi). This
    // reproduces retail's once-per-render-frame advance without the deleted two-phase
    // driver. The seen-set resets when the GameUpdateLoop frame token changes.
    // Seen-set = a generation-stamped open-addressing table (2026-09-04). The old 64-entry
    // linear array was both a CAP (a town with more than 64 distinct emits per frame —
    // the 800-house stress town — silently stopped un-stamping the rest, freezing their
    // smoke) and a hot spot (2.2% of the EntityList phase at 800x10: ~400 calls per
    // iteration, each scanning up to 64 pointers per geo entry).
    struct SeenSlot { uintptr_t key; int gen; };
    constexpr int kSeenCap = 4096;   // power of two; ~5 emits per component worst case
    static SeenSlot s_seenTbl[kSeenCap];
    const int frame = GameUpdateLoop_CurrentFrame();
    // At game speed N the tree walk ticks this component N times per RENDER frame, and
    // every iteration after the first is a no-op here by construction (each emit is
    // un-stamped at most once per frame, and the first tick re-stamps it). Skip the
    // geo-entry walk for a component that already ran this frame — the same table
    // keyed by the component pointer (~400 particle components tick per iteration at
    // the 800-citizen stress town; this was 2.6% of the EntityList phase at 10x).
    constexpr int kCompCap = 8192;
    static SeenSlot s_compTbl[kCompCap];
    {
        const uintptr_t k = (uintptr_t)self;
        uint32_t h = (uint32_t)((k >> 4) * 2654435761u) & (kCompCap - 1);
        for (int probe = 0; probe < kCompCap; ++probe) {
            SeenSlot& sl = s_compTbl[(h + probe) & (kCompCap - 1)];
            if (sl.gen != frame) { sl.gen = frame; sl.key = k; break; }
            if (sl.key == k) return;   // already un-stamped this frame
        }
    }
    auto seenInsert = [&](uintptr_t k) -> bool {   // true = already un-stamped this frame
        uint32_t h = (uint32_t)((k >> 4) * 2654435761u) & (kSeenCap - 1);
        for (int probe = 0; probe < kSeenCap; ++probe) {
            SeenSlot& sl = s_seenTbl[(h + probe) & (kSeenCap - 1)];
            if (sl.gen != frame) { sl.gen = frame; sl.key = k; return false; }
            if (sl.key == k) return true;
        }
        return false;   // table full (cannot happen below kSeenCap distinct emits)
    };

    uintptr_t s = (uintptr_t)self;
    PCGeoEntry* arr = *(PCGeoEntry**)(s + 0x38);
    int total = *(int32_t*)(s + 0x40);
    for (int i = 0; arr && i < total; ++i) {
        if (!arr[i].geo || arr[i].sceneId < 0) continue;
        uintptr_t g = (uintptr_t)arr[i].geo;
        uintptr_t res  = *(uintptr_t*)(g + 0xc0);
        uintptr_t emit = *(uintptr_t*)(g + 0xc8);
        if (!res || !emit || *(uint8_t*)(res + 0x20) == 0) continue;
        if (seenInsert(emit)) continue;              // already un-stamped this frame
        if (*(int32_t*)(emit + 0x2c) == fi)
            *(int32_t*)(emit + 0x2c) = fi - 1;
    }
}

// [PerfX PARTICLE_CULL -- NON-RETAIL] a particle system nobody can see is FROZEN (not
// ticked) rather than simulated; when the camera reaches it the plume it had simply
// continues. Position = the first live emitter geometry's transform (+0x94).
static bool PC_Culled(ParticleComponent* self) {
    if (!PerfX::ParticleCull()) return false;
    uintptr_t s = (uintptr_t)self;
    int total = *(int32_t*)(s + 0x40);
    PCGeoEntry* geoArr = *(PCGeoEntry**)(s + 0x38);
    if (total <= 0 || !geoArr) return false;
    for (int i = 0; i < total; i++) {
        if (geoArr[i].geo && geoArr[i].sceneId >= 0)
            return PerfX::ParticleCullSkip(reinterpret_cast<const float*>((uintptr_t)geoArr[i].geo + 0x94));
    }
    return false;
}

// 0x1400533d0. Component() is the CALL 0x1401188b0; the 0x14015a290 stamp is
// the compiler's; +0x28..+0x40 = 0; flags |= 0xc3.
ParticleComponent::ParticleComponent()
    : field28(nullptr), field30(nullptr), field38(nullptr), field40(nullptr) {
    OrFlagsInCtor(0xc3);
}

// [0] 0x14000a560. PORT GAP (retail 0x140053310 destroys the geometry objects
// and frees both arrays first) — the tree's Inert_ScalarDtor never did; kept
// (ParticleComponent.h).
Object::Base* ParticleComponent::DeletingDtor(unsigned flags) {
    this->~ParticleComponent();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [3] 0x140053270. MUST resolve first: slot 3 is the FIRST call into this class
// on a load-from-save (PersistObject creates the object and dispatches +0x18
// before Startup or any tick has run). Until InitFuncs() has run every s_fn_*
// is null and the body null-guards EVERY stream call — it would consume ZERO
// bytes of a record the save leg wrote (a stream desync). Retail 0x140053270
// reads an emitter count and then that many bools.
// ============================================================
// ParticleComponent::Persist (0x140053270)
// ============================================================
void ParticleComponent::Persist(void* stream) {
    ParticleComponent_InitFuncs();
    uintptr_t selfAddr = reinterpret_cast<uintptr_t>(this);

    Component::Persist(stream);   // retail: CALL 0x1401189b0 (qualified)

    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    int localCount = *(int*)(selfAddr + 0x30);
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(localCount, nullptr);   // B.3.4 bridge

    // If loading, resize bool array
    // B.3.2 bridge: FUN_14000a590 is Collection::Array<uint8_t>::SetSize(n)
    if (static_cast<int>(reinterpret_cast<IO::Stream*>(stream)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Stream::GetDirection, retail 0x180006200)
        GameColl::AsArray<GameColl::U8>((void*)(selfAddr + 0x28)).SetSize(localCount);
    }

    // Serialize bool array
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(localCount, *(bool**)(selfAddr + 0x28), nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

// [4] 0x140053230 — setup: create the ParticleSystemInstances.
// ============================================================
// ParticleComponent::Startup (0x140053230) — actually setup/init
// ============================================================
void ParticleComponent::Startup() {
    ParticleComponent_InitFuncs();
    uintptr_t selfAddr = reinterpret_cast<uintptr_t>(this);

    if (s_fn_ParticleSetup)
        s_fn_ParticleSetup((void*)selfAddr);

    // Zero all emitter flags
    int count = *(int*)(selfAddr + 0x30);
    if (count > 0) {
        uint8_t* flags = *(uint8_t**)(selfAddr + 0x28);
        for (int i = 0; i < count; i++) {
            flags[i] = 0;
        }
    }
}

// [6] 0x140052f70 — fade-in start of the autoStart (b1) emitters.
// ============================================================
// ParticleComponent::OnAdded (0x140052f70)
// ============================================================
void ParticleComponent::OnAdded() {
    ParticleComponent_InitFuncs();
    uintptr_t selfAddr = reinterpret_cast<uintptr_t>(this);

    void* desc = GetParticleDesc(this);
    if (!desc) return;

    float uvScale = PC_UVScale();

    int emitterCount = *(int*)((uintptr_t)desc + 0x30);
    if (emitterCount <= 0) return;

    uintptr_t descArray = *(uintptr_t*)((uintptr_t)desc + 0x28);
    uintptr_t geoArray = *(uintptr_t*)(selfAddr + 0x38);

    for (int i = 0; i < emitterCount; i++) {
        uintptr_t descEntry = descArray + (uintptr_t)i * 0x28;

        // Check if emitter is active (byte at descEntry + 0x11)
        if (*(uint8_t*)(descEntry + 0x11) == 0) continue;

        // Re-check IsDerivedFrom for desc (original does this per iteration)
        void* descCheck = GetParticleDesc(this);
        if (!descCheck) continue;

        uintptr_t geoEntry = geoArray + (uintptr_t)i * 0x18;

        // Get timing from desc: entry array slot
        uintptr_t descArr2 = *(uintptr_t*)((uintptr_t)descCheck + 0x28);
        uintptr_t timeAddr = descArr2 + ((uintptr_t)i * 5 + 4) * 8;
        float timeSecs = reinterpret_cast<const System::Time*>((void*)timeAddr)->AsSeconds();   // B.3.4b bridge

        // Set emitter timing: uvScale / timeSecs
        *(float*)(geoEntry + 0x10) = uvScale / timeSecs;

        // Check/set geometry opacity
        uintptr_t geoObj = *(uintptr_t*)geoEntry;
        if (*(float*)(geoObj + 0x7C) != 0.0f) {
            *(uint32_t*)(geoObj + 0x2C) |= 4;
            *(uint64_t*)(geoObj + 0x7C) = 0;
        }

        if (s_fn_UpdateEmitterPos)
            s_fn_UpdateEmitterPos((void*)selfAddr, i);
    }
}

// [7] 0x140052e80 — remove every emitter from the scene.
// ============================================================
// ParticleComponent::OnRemove (0x140052e80)
// ============================================================
void ParticleComponent::OnRemove() {
    ParticleComponent_InitFuncs();
    uintptr_t selfAddr = reinterpret_cast<uintptr_t>(this);

    int totalCount = *(int*)(selfAddr + 0x40);
    if (totalCount <= 0) return;

    uintptr_t geoArray = *(uintptr_t*)(selfAddr + 0x38);
    uint8_t* flagArray = *(uint8_t**)(selfAddr + 0x28);

    for (int i = 0; i < totalCount; i++) {
        uintptr_t geoEntry = geoArray + (uintptr_t)i * 0x18;

        // Remove scene object if valid
        uint32_t objId = *(uint32_t*)(geoEntry + 8);
        if ((int)objId >= 0) {
            if (g_pGameWorldSingleton && *g_pGameWorldSingleton) {
                void* scene = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0x60);
                reinterpret_cast<Graphics::Scene*>(scene)->RemoveObject(objId);   // B.3.5 bridge
            }
            *(uint32_t*)(geoEntry + 8) = 0xFFFFFFFF;
        }

        // Clear emitter flag
        flagArray[i] = 0;

        // Check if any emitter still has a valid scene object
        bool anyValid = false;
        if (totalCount > 0) {
            for (int j = 0; j < totalCount; j++) {
                if ((int)*(uint32_t*)(*(uintptr_t*)(selfAddr + 0x38) + (uintptr_t)j * 0x18 + 8) >= 0) {
                    anyValid = true;
                    break;
                }
            }
        }

        if (!anyValid) {
            // Clear bit 2 of component flags
            uint32_t oldFlags = *(uint32_t*)(selfAddr + 0x20);
            uint32_t newFlags = oldFlags & ~0x4u;
            *(uint32_t*)(selfAddr + 0x20) = newFlags;

            if (((newFlags ^ oldFlags) & 0xFFFF) != 0 && *(uintptr_t*)(selfAddr + 0x10) != 0) {
                if (s_fn_EntityFlagUpdate)
                    s_fn_EntityFlagUpdate(*(void**)(selfAddr + 0x10));
            }
        }
    }
}

// [8] 0x140052b30 — the per-frame tick (node-follow + fade + geometry Update),
// dispatched by the EntityList tree walk (0x1400c2e5d: RDX = the update's time
// pointer, passed through).
// ============================================================
// ParticleComponent per-frame tick (vtable +0x40, 0x140052b30)
// ============================================================
void ParticleComponent::Tick(void* time) {
    if (PC_Culled(this)) return;   // frozen off-screen: neither the unstamp nor the tick
    PC_UnstampOwnEmitters(this);   // SA per-frame frame-index advance (see helper) before the tick
    uintptr_t s = reinterpret_cast<uintptr_t>(this);
    int total = *(int32_t*)(s + 0x40);
    if (total <= 0) return;
    PCGeoEntry* geoArr = *(PCGeoEntry**)(s + 0x38);
    if (!geoArr) return;
    float uvScale = PC_UVScale();

    for (int i = 0; i < total; i++) {
        PCGeoEntry* ge = &geoArr[i];

        // Node/attach-point follow: recompute transform every frame
        if (ge->nodeIdx >= 0 && ge->geo) {
            uintptr_t entity = PC_Entity(this);
            void* model = PC_FindComp(s_fn_FindComp_Model, entity);
            void* desc = GetParticleDesc(this);
            if (desc && model) {
                float tf[7] = {0, 0, 0, 1, 0, 0, 0};
                bool have = false;
                void* attach = *(void**)((uintptr_t)desc + 0x20);
                if (attach) {
                    uint8_t* arr = *(uint8_t**)((uintptr_t)attach + 0x30);
                    if (arr) {
                        // Transform copy-ctor (seven dwords) from the ModelComponent's +0x148.
                        const Math::Transform modelTf =
                            *reinterpret_cast<const Math::Transform*>((uint8_t*)model + 0x148);   // B.3.1 bridge
                        const float zero = 0.0f;
                        const Math::Transform ypo = Math::Transform::FromYawPitchOffset(
                            zero, zero,
                            *reinterpret_cast<const Math::Vector3*>(arr + (size_t)ge->nodeIdx * 0x18 + 8));   // B.3.1 bridge
                        // Math::Transform::operator*(const Transform&) — slot 0x140151430.
                        *reinterpret_cast<Math::Transform*>(tf) = ypo * modelTf;   // B.3.1 bridge
                        have = true;
                    }
                } else {
                    // Retail retslot door, body moved to the call site: exactly one
                    // construction into `tf`, the same storage the door wrote.
                    ::new (static_cast<void*>(tf)) Math::Transform(
                        reinterpret_cast<const Graphics::MeshInstance*>((uint8_t*)model + 0x28)
                            ->GetNodeToWorld(ge->nodeIdx));   // B.3.5 bridge
                    have = true;
                }
                if (have)
                    reinterpret_cast<Graphics::Geometry*>(ge->geo)
                        ->SetTransform(*reinterpret_cast<const Math::Transform*>(tf));   // B.3.5 bridge
            }
        }

        // Opacity fade integration + geometry tick (only while in the scene)
        if (ge->sceneId >= 0 && ge->geo) {
            uintptr_t g = (uintptr_t)ge->geo;
            float op = *(float*)(g + 0x7c);
            if (ge->fadeRate != 0.0f) {
                float dt = reinterpret_cast<const System::Time*>(time)->AsSeconds();   // B.3.4b bridge
                op += dt * ge->fadeRate;
                if (op > uvScale) {
                    ge->fadeRate = 0.0f;
                    op = uvScale;
                } else if (op < 0.0f) {
                    ge->fadeRate = 0.0f;
                    ParticleComponent_StopEmitter(this, i);
                    continue;
                } else if (op <= 0.0f) {
                    ParticleComponent_StopEmitter(this, i);
                    continue;
                }
            } else if (op <= 0.0f) {
                ParticleComponent_StopEmitter(this, i);
                continue;
            }
            if (op != *(float*)(g + 0x7c)) {
                *(uint32_t*)(g + 0x2c) |= 4;   // dirty
                *(float*)(g + 0x7c) = op;
                *(float*)(g + 0x80) = op;
            }
            void** vt = *(void***)ge->geo;
            if (vt && vt[1]) {
                typedef void (*fn_geoUpdate_t)(void*, void*);
                ((fn_geoUpdate_t)vt[1])(ge->geo, time);
            }
        }
    }
}

// [11] 0x1400531b0 — setup + restart the flagged emitters.
// ============================================================
// ParticleComponent::Slot11 (0x1400531b0)
// ============================================================
void ParticleComponent::Slot11() {
    ParticleComponent_InitFuncs();
    uintptr_t selfAddr = reinterpret_cast<uintptr_t>(this);

    void* desc = GetParticleDesc(this);

    if (s_fn_ParticleSetup)
        s_fn_ParticleSetup((void*)selfAddr);

    if (!desc) return;

    int emitterCount = *(int*)((uintptr_t)desc + 0x30);
    if (emitterCount <= 0) return;

    uint8_t* flagArray = *(uint8_t**)(selfAddr + 0x28);

    for (int i = 0; i < emitterCount; i++) {
        if (flagArray[i] != 0) {
            if (s_fn_EmitterStartup)
                s_fn_EmitterStartup((void*)selfAddr, i);
        }
    }
}

// [12] 0x140053090 — event 2/3 fade-in/out of the b2 emitters.
// ============================================================
// ParticleComponent_Slot12 — event handler (vtable +0x60, 0x140053090)
// event 2 = start with fade-in, event 3 = begin fade-out (b2 emitters only)
// ============================================================
void ParticleComponent::OnEvent(int eventCode) {
    ParticleComponent_InitFuncs();
    uintptr_t s = reinterpret_cast<uintptr_t>(this);
    void* desc = GetParticleDesc(this);
    if (!desc) return;
    uint8_t* emArr = *(uint8_t**)((uintptr_t)desc + 0x28);
    PCGeoEntry* geoArr = *(PCGeoEntry**)(s + 0x38);
    int total = *(int32_t*)(s + 0x40);
    if (!emArr || !geoArr) return;

    for (int i = 0; i < total; i++) {
        uint8_t* elem = emArr + (size_t)i * 0x28;
        if (elem[0x12] == 0) continue;   // b2: event-toggled emitters only
        if (eventCode == 2) {
            ParticleComponent_StartEmitterFade(this, i);
        } else if (eventCode == 3) {
            float t1 = reinterpret_cast<const System::Time*>(elem + 0x20)->AsSeconds();   // B.3.4b bridge
            geoArr[i].fadeRate = -(PC_UVScale() / t1);
        }
    }
}

// create 0x14000a360: MemAlloc(0x48, 8), then the ctor.
void* ParticleComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ParticleComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) ParticleComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* ParticleComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) ParticleComponent());
}

DEFINE_EXE_VTABLE(ParticleComponent, "_ZTV17ParticleComponent")
DEFINE_EXE_TYPE(ParticleComponent, "Component")

// Register the helper closure at its exe addresses so the shared bodies above
// (and any other reimpl referencing them) resolve in standalone.
extern "C" void RegisterParticleComponentAddrs() {
    RegisterExeAddr(0x140052480, reinterpret_cast<uintptr_t>(&ParticleComponent_Setup));
    RegisterExeAddr(0x140052990, reinterpret_cast<uintptr_t>(&ParticleComponent_StopEmitter));
    RegisterExeAddr(0x140052a50, reinterpret_cast<uintptr_t>(&ParticleComponent_StartEmitter));
    RegisterExeAddr(0x140053660, reinterpret_cast<uintptr_t>(&ParticleComponent_StartEmitterFade));
}
