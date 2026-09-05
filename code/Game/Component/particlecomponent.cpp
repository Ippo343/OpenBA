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

#include "Component/particlecomponent_funcs.h"
#include "scene_watch.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName (SA type-id fallback)

// ============================================================
// Resolved helpers
// ============================================================

typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

typedef void* (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

typedef void (*fn_SerializeInt_t)(void* self, int* value, const char* name);
static fn_SerializeInt_t s_fn_SerializeInt = nullptr;

// Serialize(serializer, count, boolArray, name) — overload for bool arrays
typedef void (*fn_SerializeBoolArray_t)(void* self, int count, bool* array, const char* name);
static fn_SerializeBoolArray_t s_fn_SerializeBoolArray = nullptr;

// FUN_14000a590 — resize bool array
typedef void (*fn_ResizeBoolArray_t)(void* arrayPtr, int newCount);
static fn_ResizeBoolArray_t s_fn_ResizeBoolArray = nullptr;

// IsDerivedFrom
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* obj, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_d = nullptr;

// CheckPtr
typedef void (*fn_CheckPtr_t)(void* ptr);
static fn_CheckPtr_t s_fn_CheckPtr = nullptr;

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
typedef void (*fn_SceneRemoveObject_t)(void* scene, uint32_t objectId);
static fn_SceneRemoveObject_t s_fn_SceneRemoveObject = nullptr;

// FUN_1400c3470 — entity flag update
typedef void (*fn_EntityFlagUpdate_t)(void* entity);
static fn_EntityFlagUpdate_t s_fn_EntityFlagUpdate = nullptr;

// System::Time::AsSeconds
typedef float (*fn_TimeAsSeconds_t)(void* time);
static fn_TimeAsSeconds_t s_fn_TimeAsSeconds = nullptr;

// FUN_140053660 — emitter startup
typedef void (*fn_EmitterStartup_t)(void* self, int index);
static fn_EmitterStartup_t s_fn_EmitterStartup = nullptr;

// The original OnRemove is too complex for clean reimpl — use direct call
typedef void (*fn_OnRemove_Orig_t)(void* self, void* time);
static fn_OnRemove_Orig_t s_fn_OnRemove_Orig = nullptr;

static uint32_t* s_pParticleDescTypeId = nullptr;  // DAT_1401c4758
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

// ParticleDescription type id — the exe .rdata slot ResolveAddr returns NULL
// (raw .rdata is not registered), so fall back to
// the runtime type registry (valid after Object::Factory::Startup).
static uint32_t PC_DescTypeId() {
    if (s_pParticleDescTypeId && *s_pParticleDescTypeId != 0 &&
        *s_pParticleDescTypeId != 0xFFFFFFFF)
        return *s_pParticleDescTypeId;
    static uint32_t cached = 0;
    if (cached == 0 || cached == 0xFFFFFFFF)
        cached = ::Object::GetTypeIdByName("ParticleDescription");
    if (cached != 0 && cached != 0xFFFFFFFF) return cached;
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
// ParticleComponent::Shutdown (0x140053270)
// ============================================================
void ParticleComponent_Shutdown(ParticleComponent* self, void* stream) {
    uintptr_t selfAddr = (uintptr_t)self;

    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    uint8_t serializer[24];
    if (s_fn_Serializer_Ctor) s_fn_Serializer_Ctor(serializer, stream, 0);

    int localCount = *(int*)(selfAddr + 0x30);
    if (s_fn_SerializeInt)
        s_fn_SerializeInt(serializer, &localCount, nullptr);

    // If loading, resize bool array
    if (*(int*)((uintptr_t)stream + 8) == 0 && s_fn_ResizeBoolArray) {
        s_fn_ResizeBoolArray((void*)(selfAddr + 0x28), localCount);
    }

    // Serialize bool array
    if (s_fn_SerializeBoolArray)
        s_fn_SerializeBoolArray(serializer, localCount, *(bool**)(selfAddr + 0x28), nullptr);

    if (s_fn_Serializer_Dtor) s_fn_Serializer_Dtor(serializer);
}

// ============================================================
// ParticleComponent::Serialize (0x140053230) — actually setup/init
// ============================================================
void ParticleComponent_Serialize(ParticleComponent* self) {
    uintptr_t selfAddr = (uintptr_t)self;

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

// ============================================================
// ParticleComponent::Draw (0x140052f70)
// ============================================================
void ParticleComponent_Draw(ParticleComponent* self) {
    uintptr_t selfAddr = (uintptr_t)self;

    void* desc = GetParticleDesc(self);
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
        void* descCheck = GetParticleDesc(self);
        if (!descCheck) continue;

        uintptr_t geoEntry = geoArray + (uintptr_t)i * 0x18;

        // Get timing from desc: entry array slot
        uintptr_t descArr2 = *(uintptr_t*)((uintptr_t)descCheck + 0x28);
        uintptr_t timeAddr = descArr2 + ((uintptr_t)i * 5 + 4) * 8;
        float timeSecs = 0.0f;
        if (s_fn_TimeAsSeconds)
            timeSecs = s_fn_TimeAsSeconds((void*)timeAddr);

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

// ============================================================
// ParticleComponent::OnCreate (0x140052e80)
// ============================================================
void ParticleComponent_OnCreate(ParticleComponent* self) {
    uintptr_t selfAddr = (uintptr_t)self;

    int totalCount = *(int*)(selfAddr + 0x40);
    if (totalCount <= 0) return;

    uintptr_t geoArray = *(uintptr_t*)(selfAddr + 0x38);
    uint8_t* flagArray = *(uint8_t**)(selfAddr + 0x28);

    for (int i = 0; i < totalCount; i++) {
        uintptr_t geoEntry = geoArray + (uintptr_t)i * 0x18;

        // Remove scene object if valid
        uint32_t objId = *(uint32_t*)(geoEntry + 8);
        if ((int)objId >= 0) {
            if (s_fn_SceneRemoveObject && g_pGameWorldSingleton && *g_pGameWorldSingleton) {
                void* scene = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0x60);
                s_fn_SceneRemoveObject(scene, objId);
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
                if (s_fn_CheckPtr)
                    s_fn_CheckPtr((void*)(selfAddr + 0x10));
                if (s_fn_EntityFlagUpdate)
                    s_fn_EntityFlagUpdate(*(void**)(selfAddr + 0x10));
            }
        }
    }
}

// ============================================================
// ParticleComponent::OnRemove (0x140052b30)
// ============================================================
// This is the most complex method (200 instructions) — positions particles
// relative to model using MeshInstance::GetNodeToWorld, Transform operations.
// Too many Graphics dependencies to cleanly reimplement. We call the original.

void ParticleComponent_OnRemove(ParticleComponent* self, void* time) {
    if (s_fn_OnRemove_Orig)
        s_fn_OnRemove_Orig(self, time);
}

// ============================================================
// ParticleComponent::OnUpdate2 (0x1400531b0)
// ============================================================
void ParticleComponent_OnUpdate2(ParticleComponent* self) {
    uintptr_t selfAddr = (uintptr_t)self;

    void* desc = GetParticleDesc(self);

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
// MeshInstance::GetNodeToWorld — member returning Transform by value:
// RCX=this, RDX=retbuf, R8D=nodeIndex (dispatch slot 0x140152998).
typedef void* (*fn_GetNodeToWorld_t)(void* meshInstance, void* retTransform, int nodeIndex);
static fn_GetNodeToWorld_t s_fn_GetNodeToWorld = nullptr;

// Math::Transform::operator* — member: RCX=this, RDX=retbuf, R8=&rhs
// (dispatch slot 0x140151430).
typedef void* (*fn_TransformMul_t)(const void* self, void* retTransform, const void* rhs);
static fn_TransformMul_t s_fn_TransformMul = nullptr;

// System::String::operator==(const String&) — RCX=&lhs field, RDX=&rhs field
// (dispatch slot 0x140151438).
typedef bool (*fn_StringEqStr_t)(const void* lhs, const void* rhs);
static fn_StringEqStr_t s_fn_StringEqStr = nullptr;

// Graphics::GraphicsMesh::GetNodeIndexFromName(mesh, nameChars) — slot 0x140152970.
typedef int (*fn_GetNodeIdxFromName_t)(void* mesh, const char* name);
static fn_GetNodeIdxFromName_t s_fn_GetNodeIdxFromName = nullptr;

// FindComponent helpers (exe): 0x140003f60 = ModelComponent, 0x140004680 = MapComponent.
// (entity, retbuf IBP) — retbuf[0] receives the component pointer (or 0).
typedef void* (*fn_PCFindComponent_t)(uintptr_t entity, void* retIBP);
static fn_PCFindComponent_t s_fn_FindComp_Model = nullptr;
static fn_PCFindComponent_t s_fn_FindComp_Map = nullptr;

// Throttled diagnostic trace (BANISHED_PARTICLE_TRACE)
static bool PCTraceOn() {
    static int v = -1;
    if (v < 0) { const char* e = getenv("BANISHED_PARTICLE_TRACE");
                 v = (e && *e && e[0] != '0') ? 1 : 0; }
    return v == 1;
}
static int s_pcTraceBudget = 0;

// g_fUVScale is the retail .rdata constant 1.0f. In SA the ResolveAddr for
// 0x140172234 lands inside an unrelated registered data blob (observed value
// 300.0 → fade rate 300/s, opacity 15+ → permanently opaque smoke), so use
// the known retail value directly.
static float PC_UVScale() { return 1.0f; }

// Owning entity from the component's +0x10 InternalBasePtr.
static uintptr_t PC_Entity(void* self) {
    void* ibp = (void*)((uintptr_t)self + 0x10);
    if (*(uintptr_t*)ibp == 0) return 0;
    if (s_fn_CheckPtr) s_fn_CheckPtr(ibp);
    return *(uintptr_t*)ibp;
}

static void* PC_FindComp(fn_PCFindComponent_t fn, uintptr_t entity) {
    if (!fn || !entity) return nullptr;
    uintptr_t buf[2] = {0, 0};
    fn(entity, buf);
    if (buf[0] && s_fn_CheckPtr) s_fn_CheckPtr(buf);
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
            if (s_fn_StringEqStr && s_fn_StringEqStr(arr + (size_t)j * 0x18, elem + 8)) {
                float zero = 0.0f, ypo[7], prod[7];
                if (fn_Transform_FromYawPitchOffset && s_fn_TransformMul) {
                    fn_Transform_FromYawPitchOffset(ypo, &zero, &zero, arr + (size_t)j * 0x18 + 8);
                    s_fn_TransformMul(ypo, prod, tf);
                    pos[0] = prod[4]; pos[1] = prod[5]; pos[2] = prod[6];
                }
                followIdx = j;
                break;
            }
        }
    } else if (model) {
        // Mesh-node lookup by emitter name (e.g. a "smoke" node in the building mesh)
        void* mesh = *(void**)((uintptr_t)model + 0xe8);
        char* name = *(char**)(elem + 8);
        if (mesh && name && *(int32_t*)(name - 8) != 0 && s_fn_GetNodeIdxFromName) {
            int n = s_fn_GetNodeIdxFromName(mesh, name);
            if (n >= 0) {
                if (s_fn_GetNodeToWorld) {
                    float ntw[7] = {0, 0, 0, 1, 0, 0, 0};
                    s_fn_GetNodeToWorld((uint8_t*)model + 0x28, ntw, n);
                    pos[0] = ntw[4]; pos[1] = ntw[5]; pos[2] = ntw[6];
                }
                followIdx = n;
            }
        }
    }

    // Final placement: identity rotation + computed position
    float finalTf[7] = {0, 0, 0, PC_UVScale(), pos[0], pos[1], pos[2]};
    if (fn_Geometry_SetTransform) fn_Geometry_SetTransform(ge->geo, finalTf);

    // Configure the instance from its ParticleSystem resource (vtable+0x48)
    void* res = *(void**)(elem + 0);
    void** vt = *(void***)ge->geo;
    if (res && vt && vt[9]) {
        typedef void (*fn_setSystem_t)(void*, void*);
        ((fn_setSystem_t)vt[9])(ge->geo, res);
    }

    ge->sceneId = -1;
    ge->nodeIdx = (elem[0x10] != 0) ? followIdx : -1;

    if (PCTraceOn() && s_pcTraceBudget < 64) {
        s_pcTraceBudget++;
        char* nm = *(char**)(elem + 8);
        printf("[Particle] place self=%p i=%d name=%s follow=%d pos=(%.1f,%.1f,%.1f) res=%p geo=%p\n",
               (void*)self, index, nm ? nm : "(null)", ge->nodeIdx, pos[0], pos[1], pos[2], res, ge->geo);
        fflush(stdout);
    }

    // Prewarm: tick the emitter forward by t0 (elem+0x18) in engine fixed steps
    int64_t t0 = *(int64_t*)(elem + 0x18);
    if (t0 > 0 && g_pEngineSingleton && *g_pEngineSingleton && vt && vt[1]) {
        int64_t step = *(int64_t*)((uintptr_t)*g_pEngineSingleton + 0x48);
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
        if (s_fn_SceneRemoveObject && g_pGameWorldSingleton && *g_pGameWorldSingleton) {
            void* scene = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0x60);
            if (scene) s_fn_SceneRemoveObject(scene, (uint32_t)ge->sceneId);
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
        if (s_fn_CheckPtr) s_fn_CheckPtr((void*)(s + 0x10));
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
    if (ge->sceneId >= 0 && scene && s_fn_SceneRemoveObject)
        s_fn_SceneRemoveObject(scene, (uint32_t)ge->sceneId);

    ParticleComponent_Place(self, index);
    if (!ge->geo || !scene) return;

    // Initial Update(Time(0)) then add to the scene
    int64_t zeroTime = 0;
    void** vt = *(void***)ge->geo;
    if (vt && vt[1]) {
        typedef void (*fn_geoUpdate_t)(void*, void*);
        ((fn_geoUpdate_t)vt[1])(ge->geo, &zeroTime);
    }
    if (fn_Graphics_Scene_AddObject)
    {
        ge->sceneId = (int32_t)fn_Graphics_Scene_AddObject(scene, ge->geo);
        SceneWatch_Note((uint32_t)ge->sceneId, ge->geo, "ParticleComponent");
    }

    if (PC_RebucketEntity()) {
        uint32_t oldFlags = *(uint32_t*)(s + 0x20);
        uint32_t newFlags = oldFlags | 0x4u;
        *(uint32_t*)(s + 0x20) = newFlags;
        if (((newFlags ^ oldFlags) & 0xFFFF) != 0 && *(uintptr_t*)(s + 0x10) != 0) {
            if (s_fn_CheckPtr) s_fn_CheckPtr((void*)(s + 0x10));
            if (s_fn_EntityFlagUpdate) s_fn_EntityFlagUpdate(*(void**)(s + 0x10));
        }
    }
    uint8_t* flagArr = *(uint8_t**)(s + 0x28);
    if (flagArr) flagArr[index] = 1;

    if (PCTraceOn() && s_pcTraceBudget < 96) {
        s_pcTraceBudget++;
        printf("[Particle] start self=%p i=%d sceneId=%d\n", (void*)self, index, ge->sceneId);
        fflush(stdout);
    }
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
    float t1 = s_fn_TimeAsSeconds ? s_fn_TimeAsSeconds(elem + 0x20) : 1.0f;
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
        if (PCTraceOn() && s_pcTraceBudget < 64) {
            s_pcTraceBudget++;
            printf("[Particle] setup self=%p: no ParticleDescription (+0x18=%p)\n",
                   (void*)self, *(void**)(s + 0x18));
            fflush(stdout);
        }
        return;
    }
    int n = *(int32_t*)((uintptr_t)desc + 0x30);
    PC_GeoArrayResize((void*)(s + 0x38), n);
    if (s_fn_ResizeBoolArray) s_fn_ResizeBoolArray((void*)(s + 0x28), n);

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
    if (PCTraceOn() && s_pcTraceBudget < 64) {
        s_pcTraceBudget++;
        printf("[Particle] setup self=%p emitters=%d nullResource=%d nullInstance=%d\n",
               (void*)self, total, nullRes, nullInst);
        fflush(stdout);
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
// ParticleComponent per-frame tick (vtable +0x40, 0x140052b30)
// ============================================================
extern "C" void ParticleComponent_UpdateTick(ParticleComponent* self, void* time) {
    uintptr_t s = (uintptr_t)self;
    int total = *(int32_t*)(s + 0x40);
    if (total <= 0) return;
    PCGeoEntry* geoArr = *(PCGeoEntry**)(s + 0x38);
    if (!geoArr) return;
    float uvScale = PC_UVScale();

    for (int i = 0; i < total; i++) {
        PCGeoEntry* ge = &geoArr[i];

        // Node/attach-point follow: recompute transform every frame
        if (ge->nodeIdx >= 0 && ge->geo) {
            uintptr_t entity = PC_Entity(self);
            void* model = PC_FindComp(s_fn_FindComp_Model, entity);
            void* desc = GetParticleDesc(self);
            if (desc && model) {
                float tf[7] = {0, 0, 0, 1, 0, 0, 0};
                bool have = false;
                void* attach = *(void**)((uintptr_t)desc + 0x20);
                if (attach) {
                    uint8_t* arr = *(uint8_t**)((uintptr_t)attach + 0x30);
                    if (arr && fn_Transform_FromYawPitchOffset && s_fn_TransformMul) {
                        float modelTf[7];
                        memcpy(modelTf, (uint8_t*)model + 0x148, 7 * sizeof(float));
                        float zero = 0.0f, ypo[7];
                        fn_Transform_FromYawPitchOffset(ypo, &zero, &zero,
                                                        arr + (size_t)ge->nodeIdx * 0x18 + 8);
                        s_fn_TransformMul(ypo, tf, modelTf);
                        have = true;
                    }
                } else if (s_fn_GetNodeToWorld) {
                    s_fn_GetNodeToWorld((uint8_t*)model + 0x28, tf, ge->nodeIdx);
                    have = true;
                }
                if (have && fn_Geometry_SetTransform)
                    fn_Geometry_SetTransform(ge->geo, tf);
            }
        }

        // Opacity fade integration + geometry tick (only while in the scene)
        if (ge->sceneId >= 0 && ge->geo) {
            uintptr_t g = (uintptr_t)ge->geo;
            float op = *(float*)(g + 0x7c);
            if (ge->fadeRate != 0.0f) {
                float dt = s_fn_TimeAsSeconds ? s_fn_TimeAsSeconds(time) : 0.0f;
                op += dt * ge->fadeRate;
                if (op > uvScale) {
                    ge->fadeRate = 0.0f;
                    op = uvScale;
                } else if (op < 0.0f) {
                    ge->fadeRate = 0.0f;
                    ParticleComponent_StopEmitter(self, i);
                    continue;
                } else if (op <= 0.0f) {
                    ParticleComponent_StopEmitter(self, i);
                    continue;
                }
            } else if (op <= 0.0f) {
                ParticleComponent_StopEmitter(self, i);
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

// ============================================================
// ParticleComponent_Slot12 — event handler (vtable +0x60, 0x140053090)
// event 2 = start with fade-in, event 3 = begin fade-out (b2 emitters only)
// ============================================================
extern "C" void ParticleComponent_Event(ParticleComponent* self, int eventCode) {
    uintptr_t s = (uintptr_t)self;
    void* desc = GetParticleDesc(self);
    if (!desc) return;
    uint8_t* emArr = *(uint8_t**)((uintptr_t)desc + 0x28);
    PCGeoEntry* geoArr = *(PCGeoEntry**)(s + 0x38);
    int total = *(int32_t*)(s + 0x40);
    if (!emArr || !geoArr) return;

    for (int i = 0; i < total; i++) {
        uint8_t* elem = emArr + (size_t)i * 0x28;
        if (elem[0x12] == 0) continue;   // b2: event-toggled emitters only
        if (eventCode == 2) {
            ParticleComponent_StartEmitterFade(self, i);
        } else if (eventCode == 3) {
            float t1 = s_fn_TimeAsSeconds ? s_fn_TimeAsSeconds(elem + 0x20) : 1.0f;
            geoArr[i].fadeRate = -(PC_UVScale() / t1);
        }
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void ParticleComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));
    s_fn_ParticleSetup = reinterpret_cast<fn_ParticleSetup_t>(
        ResolveAddr(Module::Exe, 0x140052480));
    s_fn_UpdateEmitterPos = reinterpret_cast<fn_UpdateEmitterPos_t>(
        ResolveAddr(Module::Exe, 0x140052a50));
    s_fn_EmitterTick = reinterpret_cast<fn_EmitterTick_t>(
        ResolveAddr(Module::Exe, 0x140052990));
    s_fn_EntityFlagUpdate = reinterpret_cast<fn_EntityFlagUpdate_t>(
        ResolveAddr(Module::Exe, 0x1400c3470));
    s_fn_ResizeBoolArray = reinterpret_cast<fn_ResizeBoolArray_t>(
        ResolveAddr(Module::Exe, 0x14000a590));
    s_fn_EmitterStartup = reinterpret_cast<fn_EmitterStartup_t>(
        ResolveAddr(Module::Exe, 0x140053660));

    // OnRemove original exe function.
    s_fn_OnRemove_Orig = reinterpret_cast<fn_OnRemove_Orig_t>(
        ResolveAddr(Module::Exe, 0x140052b30));

    s_pParticleDescTypeId = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c4758));
    s_pUVScale = reinterpret_cast<float*>(
        ResolveAddr(Module::Exe, 0x140172234));  // g_fUVScale

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152ac8);
    if (slot) s_fn_SerializeInt = *reinterpret_cast<fn_SerializeInt_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_d = *reinterpret_cast<fn_IsDerivedFrom_dispatch_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152ab0);
    if (slot) s_fn_CheckPtr = *reinterpret_cast<fn_CheckPtr_t*>(slot);

    s_fn_SceneRemoveObject = reinterpret_cast<fn_SceneRemoveObject_t>(
        ResolveExport("?RemoveObject@Scene@Graphics@@QEAAXI@Z"));
    s_fn_TimeAsSeconds = reinterpret_cast<fn_TimeAsSeconds_t>(
        ResolveExport("?AsSeconds@Time@System@@QEBAMXZ"));
    // IO::Serializer::Serialize(serializer, count, boolArray, name) — the ARRAY
    // overload, distinct from the single-bool one.
    //
    // Two bugs lived on this line and they compounded (fixed 2026-09-04):
    //   1. WRONG SLOT. Retail's call site is `CALL qword ptr [0x140151458]`
    //      (disassembly of ParticleComponent::Shutdown 0x140053270). This read
    //      0x140152a58 — a digit transposition of a completely different slot.
    //   2. ONE DEREF SHORT. 0x14015xxxx addresses are .rdata DISPATCH SLOTS, and
    //      ResolveAddr returns the address OF the slot, not the function pointer
    //      inside it. Every other serializer binding here derefs (see the `slot =
    //      ResolveAddr(...); *reinterpret_cast<T*>(slot)` pairs above); this one
    //      used the raw result as the callee, so the CALL jumped into .rdata
    //      storage. See [[reference_one_deref_short_vcall]].
    //
    // Symptom: every SAVE crashed with `EXEC of <data address>` in
    // ParticleComponent_Shutdown, via ComponentEntity_Serialize <- EntityList_Serialize
    // <- GameUpdateLoop_SaveRestore. Any world containing a particle emitter — i.e.
    // every world — could not be saved. Proven independent of that session's other
    // work by an A/B (identical crash with the state-machine fix reverted).
    slot = ResolveAddr(Module::Exe, 0x140151458);
    if (slot) s_fn_SerializeBoolArray = *reinterpret_cast<fn_SerializeBoolArray_t*>(slot);
    if (!s_fn_SerializeBoolArray) {
        // Skipping this call is NOT a safe fallback: it is a stream DESYNC that our
        // own round trip can never detect, because both legs would skip the same
        // bytes. Say so loudly rather than write a short record.
        // See [[reference_null_guard_on_a_serializer_call_is_a_desync]].
        fprintf(stderr, "[ParticleComp] FATAL-ISH: bool-array Serialize slot 0x140151458 "
                        "is unregistered — every particle record will be short by "
                        "count bytes and the save stream will desync.\n");
        fflush(stderr);
    }

    // --- deps for the full helper-closure ports ---
    slot = ResolveAddr(Module::Exe, 0x140152970);   // GraphicsMesh::GetNodeIndexFromName
    if (slot) s_fn_GetNodeIdxFromName = *reinterpret_cast<fn_GetNodeIdxFromName_t*>(slot);
    if (!s_fn_GetNodeIdxFromName)                    // SA: slot unregistered — use the export
        s_fn_GetNodeIdxFromName = reinterpret_cast<fn_GetNodeIdxFromName_t>(
            ResolveExport("?GetNodeIndexFromName@GraphicsMesh@Graphics@@QEBAHPEBD@Z"));
    s_fn_GetNodeToWorld = reinterpret_cast<fn_GetNodeToWorld_t>(
        ResolveExport("?GetNodeToWorld@MeshInstance@Graphics@@QEBA?AVTransform@Math@@H@Z"));
    s_fn_TransformMul = reinterpret_cast<fn_TransformMul_t>(
        ResolveExport("??DTransform@Math@@QEBA?AV01@AEBV01@@Z"));
    s_fn_StringEqStr = reinterpret_cast<fn_StringEqStr_t>(
        ResolveExport("??8String@System@@QEBA_NAEBV01@@Z"));
    s_fn_FindComp_Model = reinterpret_cast<fn_PCFindComponent_t>(
        ResolveAddr(Module::Exe, 0x140003f60));   // FindComponent(ModelComponent)
    s_fn_FindComp_Map = reinterpret_cast<fn_PCFindComponent_t>(
        ResolveAddr(Module::Exe, 0x140004680));   // FindComponent(MapComponent)

    if (PCTraceOn()) {
        printf("[Particle] InitFuncs: nodeIdxFromName=%p nodeToWorld=%p tfMul=%p strEq=%p "
               "findModel=%p findMap=%p setup=%p descTid=%08x\n",
               (void*)s_fn_GetNodeIdxFromName, (void*)s_fn_GetNodeToWorld,
               (void*)s_fn_TransformMul, (void*)s_fn_StringEqStr,
               (void*)s_fn_FindComp_Model, (void*)s_fn_FindComp_Map,
               (void*)s_fn_ParticleSetup, PC_DescTypeId());
        fflush(stdout);
    }

    s_initialized = true;
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// Building particle effects (chimney smoke etc). Gated on BANISHED_LIVE_PARTICLE
// (or global BANISHED_LIVE_COMPONENTS); default (env off) stays inert.
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
#include "Component/component_factory_base.h"
#include "address_registry.h"

static uint32_t ParticleComponent_LiveGetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("ParticleComponent");
    return cached;
}
static void* ParticleComponent_LiveVtable[::ComponentFactory::kInertVtableSlots];

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
    static int32_t* s_pFrameIndex = nullptr;
    static bool s_resolved = false;
    if (!s_resolved) {
        s_pFrameIndex = reinterpret_cast<int32_t*>(
            ResolveAddr(Module::Runtime, 0x1801fe77c));
        s_resolved = true;
    }
    if (!s_pFrameIndex || !self) return;
    int32_t fi = *s_pFrameIndex;

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

static void PC_StartupVSlot(void* s) {
    ParticleComponent_InitFuncs();
    ParticleComponent_Serialize(reinterpret_cast<ParticleComponent*>(s));
}
static void PC_OnAddedVSlot(void* s) {
    ParticleComponent_InitFuncs();
    ParticleComponent_Draw(reinterpret_cast<ParticleComponent*>(s));
}
static void PC_OnRemovedVSlot(void* s) {
    ParticleComponent_InitFuncs();
    ParticleComponent_OnCreate(reinterpret_cast<ParticleComponent*>(s));
}
static void PC_TickVSlot(void* s, void* time) {
    ParticleComponent* self = reinterpret_cast<ParticleComponent*>(s);
    PC_UnstampOwnEmitters(self);   // SA per-frame frame-index advance (see helper) before the tick
    ParticleComponent_UpdateTick(self, time);
}
static void PC_OnUpdate2VSlot(void* s) {
    ParticleComponent_InitFuncs();
    ParticleComponent_OnUpdate2(reinterpret_cast<ParticleComponent*>(s));
}
static void PC_EventVSlot(void* s, int eventCode) {
    ParticleComponent_InitFuncs();
    ParticleComponent_Event(reinterpret_cast<ParticleComponent*>(s), eventCode);
}

static void PC_SerSlot3(void* s, void* stream) {
    // MUST resolve first, exactly like the five sibling wrappers below.
    // Slot 3 is the FIRST call into this class on a load-from-save:
    // PersistObject creates the object and dispatches vt[0x18] immediately,
    // before Startup or any tick has run. Until InitFuncs() has been called
    // every s_fn_* here is null, and the body null-guards EVERY stream call --
    // so it consumed ZERO bytes and the record was just the 13-byte outer IBP
    // header, against 71 bytes on the save leg (where 1200 frames of ticks had
    // already resolved everything). Symmetric code, asymmetric state.
    // The rule is already written down in uicomponent_serialize.cpp: resolve
    // unconditionally at the top of every call, because this can be the very
    // first call into the class on load-from-save.
    ParticleComponent_InitFuncs();
    ParticleComponent_Shutdown(reinterpret_cast<ParticleComponent*>(s), stream);
}
static void ParticleComponent_FinishLiveVtable() {
    // Slot 3 (+0x18), OUTSIDE the live gate — serializer wiring is not a feature
    // flag. Retail 0x140053270 reads an emitter count and then that many bools;
    // the base-only fill read none of it, so every ParticleComponent record came
    // up short and desynced the stream for every object after it. The count makes
    // the shortfall VARIABLE, which is why it did not look like a fixed offset.
    ParticleComponent_LiveVtable[3]  = reinterpret_cast<void*>(&PC_SerSlot3);
    if (::ComponentFactory::LiveEnabled("BANISHED_LIVE_PARTICLE")) {
        ParticleComponent_LiveVtable[4]  = reinterpret_cast<void*>(&PC_StartupVSlot);
        ParticleComponent_LiveVtable[6]  = reinterpret_cast<void*>(&PC_OnAddedVSlot);
        ParticleComponent_LiveVtable[7]  = reinterpret_cast<void*>(&PC_OnRemovedVSlot);
        ParticleComponent_LiveVtable[8]  = reinterpret_cast<void*>(&PC_TickVSlot);
        ParticleComponent_LiveVtable[11] = reinterpret_cast<void*>(&PC_OnUpdate2VSlot);
        ParticleComponent_LiveVtable[12] = reinterpret_cast<void*>(&PC_EventVSlot);
    }
}
extern "C" void* ParticleComponent_Factory(void) {
    void* p = ::ComponentFactory::MakeInertTyped(0x48, 0xC3,
        ParticleComponent_LiveVtable,
        reinterpret_cast<void*>(&ParticleComponent_LiveGetTypeId));
    ParticleComponent_FinishLiveVtable();
    return p;
}
static void* ParticleComponent_CreateInPlace(void* mem) {
    if (mem) ::ComponentFactory::InitInertTyped(mem, 0x48, 0xC3,
        ParticleComponent_LiveVtable,
        reinterpret_cast<void*>(&ParticleComponent_LiveGetTypeId));
    ParticleComponent_FinishLiveVtable();
    return mem;
}
REGISTER_TYPE(ParticleComponent, "Component",
              &ParticleComponent_Factory, &ParticleComponent_CreateInPlace);

// Register the helper closure at its exe addresses so the shared bodies above
// (and any other reimpl referencing them) resolve in standalone.
extern "C" void RegisterParticleComponentAddrs() {
    RegisterExeAddr(0x140052480, reinterpret_cast<uintptr_t>(&ParticleComponent_Setup));
    RegisterExeAddr(0x140052560, reinterpret_cast<uintptr_t>(&ParticleComponent_Place));
    RegisterExeAddr(0x140052990, reinterpret_cast<uintptr_t>(&ParticleComponent_StopEmitter));
    RegisterExeAddr(0x140052a50, reinterpret_cast<uintptr_t>(&ParticleComponent_StartEmitter));
    RegisterExeAddr(0x140053660, reinterpret_cast<uintptr_t>(&ParticleComponent_StartEmitterFade));
    RegisterExeAddr(0x140052b30, reinterpret_cast<uintptr_t>(&ParticleComponent_UpdateTick));
    RegisterExeAddr(0x140053090, reinterpret_cast<uintptr_t>(&ParticleComponent_Event));
}
