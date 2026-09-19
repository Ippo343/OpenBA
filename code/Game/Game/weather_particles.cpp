// code/Game/Game/weather_particles.cpp
//
// Faithful reimplementation of the weather PARTICLE ENGINE (rain/snow) —
// the game-side Graphics::Geometry subclass that Banished uses for weather
// precipitation. Decompiled from Application-x64.exe (vtable @0x140160240).
//
// A weather effect is a 0xf0-byte Graphics::Geometry whose +0xc8 points at a
// Weather descriptor (Environment/*.rsc, 0x80 bytes — see weather.cpp). The
// geometry owns a particle array (+0xe0, stride 0x24) and an index buffer
// (+0xd0). Each frame it integrates particle positions, wraps them into a box
// tiled around the camera, and submits instanced billboard quads.
//
// Vtable (0x140160240, 9 slots) — mirrors the WorldGeometry pattern in
// geometry_draw.cpp:
//   +0x00 Dtor        (exe 0x140025060 -> body FUN_1400d3c10)
//   +0x08 Update      (exe 0x1400d2e60)  particle integration + spatial wrap
//   +0x10 Draw        (exe 0x1400d3120)  instanced billboards -> weather_particle_draw.cpp
//   +0x18 AddDrawItem (exe 0x1400d30b0)  Renderer::AddDrawItem
//   +0x20..+0x40      inherited Graphics::Geometry base virtuals (reuse GeomDraw_*)
//
// In standalone the exe .text is not loaded, so this backs the vtable directly;
// the Runtime Graphics/Video primitives are resolved from Runtime-x64.dll.

// ENDGAME B.3.5 Video/Graphics caller batch: Graphics::Geometry and
// Graphics::Scene are real classes. Ahead of "resolve.h" — resolve.h pulls
// <windows.h>, whose <wingdi.h> declares a global `Rectangle` FUNCTION that
// hides `Volume::Rectangle`'s class name inside Volume.h's own static_asserts.
#include "graphics/Scene.h"     // also brings Graphics::Geometry
#include "engine_funcs.h"
#include "math/Math.h"
#include "resolve.h"
#include "collection/Collection.h"
#include "Component/coll_bridge.h"
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include "system/Value.h"      // ENDGAME B.3.4b System caller batch (Random/Time/Color)

// ============================================================
// Exe-resident float constants (bit-exact, embedded — .rdata not present in
// our standalone PE). Verified via Ghidra read of 0x14017xxxx.
// ============================================================
namespace {
constexpr float kHalf     = 0.5f;      // DAT_140172114
constexpr float kTickDt   = 0.016f;    // DAT_1401720c8 (prevPos seed delta)
constexpr float kBoundHi  =  1024.0f;  // DAT_140172250 (SetBounds max)
constexpr float kBoundLo  = -1024.0f;  // DAT_1401722ac (SetBounds min)
constexpr float kUVScale  = 1.0f;      // g_fUVScale

// Local lazy-resolved Runtime helpers not in engine_funcs.h.
// ENDGAME B.3.5: the `?SetBounds@Geometry@Graphics@@QEAAXAEBVBox@Volume@@@Z`
// cache is GONE — its one caller calls `Graphics::Geometry::SetBounds` directly.
// `??1Geometry@Graphics@@UEAA@XZ` STAYS a door: it is a DESTRUCTOR, and no
// Video/Graphics class has a real ctor/dtor (B4b-2, B35_RULES section 11).
typedef void  (*fn_Geo_BaseDtor_t)(void* geo);                    // ??1Geometry@Graphics@@UEAA@XZ  (Runtime 0x1800a41c0)

fn_Geo_BaseDtor_t       s_fnGeoBaseDtor  = nullptr;
bool s_resolved = false;

void EnsureResolved() {
    if (s_resolved) return;
    s_resolved = true;
    s_fnGeoBaseDtor = reinterpret_cast<fn_Geo_BaseDtor_t>(
        ResolveExport("??1Geometry@Graphics@@UEAA@XZ"));
}

inline uint8_t SettingsByte(int off) {
    if (!g_pGameSettingsPtr || !*g_pGameSettingsPtr) return 0;
    return reinterpret_cast<const uint8_t*>(*g_pGameSettingsPtr)[off];
}
}  // namespace

// The weather Geometry vtable address (exe .rdata PTR_FUN_140160240).
static const uintptr_t kWeatherGeometryVtable = 0x140160240;

// ============================================================
// Particle-array (re)alloc — port of FUN_140025090 (stride 0x24).
// Weather config calls this once on a fresh geometry (count 0 -> particleCount),
// so the grow/copy machinery collapses to: alloc, zero, set count. array header
// = { void* data(+0), int count(+8), int cap(+0xc) } at geo+0xe0.
//
// ENDGAME B.3.2 step 2: NOT converted. Retail 0x140025090 (re-decompiled
// 2026-09-13) is a plain rtcoll::SetSizeCore whose new-tail arm zeroes the
// 0x24-byte element (four qword stores + one dword) and whose prefix arm copies
// nine dwords. The body below instead memsets [0, newCount) of the buffer
// AFTER the grow — on an in-place grow or shrink from a non-zero count that
// also wipes the surviving prefix, which the reference never touches. Only
// the element row (`WeatherParticleElem`, below) is retail's; it is used by the
// geometry destructor. Reconciling this body with retail is a separate,
// deliberate change.
// ============================================================
namespace {
// ENDGAME B.3.2 closing pass: the row said only what a type's traits say — a
// whole-stride byte copy, a whole-stride zero tail, no destroy, no
// deliberately-untouched extent — so it is the trait-derived
// `Collection::Array<T>` over the 0x24-byte zeroing blob. `SpecFor<T>()`
// re-derives the identical row; exe 0x140025090 stays here as the provenance.
using WeatherParticleElem = GameColl::Bytes<0x24, 4>;   // exe 0x140025090
static_assert(sizeof(WeatherParticleElem)  == 0x24, "WeatherParticleElem stride");
static_assert(alignof(WeatherParticleElem) == 4,    "WeatherParticleElem align");
using WeatherParticleArray = Collection::Array<WeatherParticleElem>;
}  // namespace

static void WeatherParticle_ArrayAlloc(void* header, int newCount) {
    uint8_t* h = reinterpret_cast<uint8_t*>(header);
    int oldCount = *reinterpret_cast<int*>(h + 8);
    if (newCount == oldCount) return;

    void* oldPtr = *reinterpret_cast<void**>(h + 0);
    *reinterpret_cast<int*>(h + 8) = newCount;
    if (*reinterpret_cast<int*>(h + 0xc) < newCount) {
        int cap = newCount;
        if (newCount == oldCount + 1 && oldCount != 0) cap = newCount * 3;  // faithful grow rule
        *reinterpret_cast<int*>(h + 0xc) = cap;
        void* p = (fn_MemAlloc && g_pMemAllocSingleton)
                      ? fn_MemAlloc(g_pMemAllocSingleton, (size_t)cap * 0x24, 4)
                      : nullptr;
        *reinterpret_cast<void**>(h + 0) = p;
    }
    void* data = *reinterpret_cast<void**>(h + 0);
    if (data && newCount > 0)
        std::memset(data, 0, (size_t)newCount * 0x24);
    if (oldPtr && oldPtr != data && fn_MemFreeStatic)
        fn_MemFreeStatic(oldPtr);
}

// ============================================================
// Geometry ctor — port of FUN_1400d3f10 (0xf0 bytes).
// ============================================================
extern "C" void* WeatherParticle_GeoCtor(void* mem) {
    if (!mem) return nullptr;
    if (fn_Graphics_Geometry_Ctor) fn_Graphics_Geometry_Ctor(mem);   // base Graphics::Geometry
    uint8_t* g = reinterpret_cast<uint8_t*>(mem);
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = reinterpret_cast<void*>(ResolveAddr(Module::Exe, kWeatherGeometryVtable));
    *reinterpret_cast<void**>(g) = s_vtable;
    *reinterpret_cast<float*>(g + 0xc0) = 0.0f;     // intensity
    *reinterpret_cast<float*>(g + 0xc4) = 1.0f;     // fade direction (+1 = fade in)
    *reinterpret_cast<uint64_t*>(g + 0xc8) = 0;     // descriptor
    *reinterpret_cast<uint64_t*>(g + 0xd0) = 0;     // index buffer
    *reinterpret_cast<uint64_t*>(g + 0xd8) = 0;
    *reinterpret_cast<uint64_t*>(g + 0xe0) = 0;     // particle array
    *reinterpret_cast<uint64_t*>(g + 0xe8) = 0;
    return mem;
}

// ============================================================
// Geometry dtor — port of FUN_1400d3c10 (+ scalar-deleting wrapper 0x140025060).
// ============================================================
extern "C" void* WeatherParticle_GeoDtor(void* self, uint32_t flags) {
    if (!self) return self;
    EnsureResolved();
    uint8_t* g = reinterpret_cast<uint8_t*>(self);
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = reinterpret_cast<void*>(ResolveAddr(Module::Exe, kWeatherGeometryVtable));
    *reinterpret_cast<void**>(g) = s_vtable;
    // ENDGAME B.3.2 step 2: the particle array at +0xe0 is torn down by
    // `~Array` (MemFree the block, zero data and the count/capacity qword —
    // the same three stores this body made by hand; trivial element, no
    // destroy). The +0xd0 index buffer is not an Array (its +0xd8 is a bare
    // count) and keeps its hand-written free. B.3.2 bridge: +0xe0 is still
    // raw bytes in the untyped geometry.
    reinterpret_cast<WeatherParticleArray*>(g + 0xe0)->~Array();
    if (fn_MemFreeStatic) {
        if (*reinterpret_cast<void**>(g + 0xd0)) fn_MemFreeStatic(*reinterpret_cast<void**>(g + 0xd0));
    }
    *reinterpret_cast<uint64_t*>(g + 0xd0) = 0;
    *reinterpret_cast<uint64_t*>(g + 0xd8) = 0;
    if (s_fnGeoBaseDtor) s_fnGeoBaseDtor(self);      // Graphics::Geometry::~Geometry
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

// ============================================================
// Config — port of TradeWork_impl_3f80(geo, Weather* descriptor).
// Allocates + seeds the particle array and index buffer from the descriptor.
// ============================================================
extern "C" void WeatherParticle_Config(void* geo, void* descriptor) {
    if (!geo || !descriptor) return;
    EnsureResolved();
    uint8_t* g = reinterpret_cast<uint8_t*>(geo);
    uint8_t* d = reinterpret_cast<uint8_t*>(descriptor);

    *reinterpret_cast<void**>(g + 0xc8) = descriptor;
    *reinterpret_cast<float*>(g + 0xc0) = 0.0f;

    int particleCount = *reinterpret_cast<int*>(d + 0x28);   // Weather.particleCount

    WeatherParticle_ArrayAlloc(g + 0xe0, particleCount);

    // Index buffer: 6 shorts (2 tris) per particle. Header {ptr(+0), count(+8)} at geo+0xd0.
    {
        uint8_t* ib = g + 0xd0;
        int idxCount = particleCount * 6;
        void* p = (fn_MemAlloc && g_pMemAllocSingleton)
                      ? fn_MemAlloc(g_pMemAllocSingleton, (size_t)idxCount * 2, 2)
                      : nullptr;
        *reinterpret_cast<void**>(ib + 0) = p;
        *reinterpret_cast<int*>(ib + 8) = idxCount;
    }

    // Normalized velocity direction (Weather.velocityDir @ +0x2c).
    float dir[3] = { *reinterpret_cast<float*>(d + 0x2c),
                     *reinterpret_cast<float*>(d + 0x30),
                     *reinterpret_cast<float*>(d + 0x34) };
    {
        Math::Vector3 nd = reinterpret_cast<const Math::Vector3*>(dir)->Normalize();
        dir[0] = nd.x; dir[1] = nd.y; dir[2] = nd.z;
    }

    float boxSize = *reinterpret_cast<float*>(d + 0x48);   // Weather.size
    float speedBase = *reinterpret_cast<float*>(d + 0x38); // Weather.velocity
    float speedTol  = *reinterpret_cast<float*>(d + 0x3c); // Weather.velocityTolerance

    float* particles = *reinterpret_cast<float**>(g + 0xe0);
    uint16_t* indices = *reinterpret_cast<uint16_t**>(g + 0xd0);
    void* rng = (g_pGameWorldSingleton && *g_pGameWorldSingleton)
                    ? *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) + 0xd8)
                    : nullptr;

    for (int i = 0; i < particleCount; ++i) {
        float* p = particles + (size_t)i * 9;   // stride 0x24 = 9 floats

        // Random position within the box: randTol per axis * size * 0.5.
        float r0 = rng ? reinterpret_cast<System::Random*>(rng)->RandFloatTol() : 0.0f;   // B.3.4b bridge
        float r1 = rng ? reinterpret_cast<System::Random*>(rng)->RandFloatTol() : 0.0f;   // B.3.4b bridge
        float r2 = rng ? reinterpret_cast<System::Random*>(rng)->RandFloatTol() : 0.0f;   // B.3.4b bridge
        // Original order: Vector3(r2*size, size*r1, size*r0) then *0.5.
        p[0] = r2 * boxSize * kHalf;
        p[1] = r1 * boxSize * kHalf;
        p[2] = r0 * boxSize * kHalf;

        // Velocity = dir * (randTol*speedTol + speedBase).
        float rs = rng ? reinterpret_cast<System::Random*>(rng)->RandFloatTol() : 0.0f;   // B.3.4b bridge
        float speed = rs * speedTol + speedBase;
        p[6] = dir[0] * speed;
        p[7] = dir[1] * speed;
        p[8] = dir[2] * speed;

        // prevPos = pos - velocity * tickDt.
        p[3] = p[0] - p[6] * kTickDt;
        p[4] = p[1] - p[7] * kTickDt;
        p[5] = p[2] - p[8] * kTickDt;

        // Quad indices {base+0,+1,+2, +0,+2,+3}.
        if (indices) {
            uint16_t base = (uint16_t)(i * 4);
            uint16_t* q = indices + (size_t)i * 6;
            q[0] = base;     q[1] = base + 1; q[2] = base + 2;
            q[3] = base;     q[4] = base + 2; q[5] = base + 3;
        }
    }

    // Identity transform + world bounds [-1024, 1024]^3.
    if (fn_Geometry_SetTransform) {
        Math::Transform tf = Math::Transform::Identity();
        fn_Geometry_SetTransform(geo, &tf);
    }
    {
        float box[6] = { kBoundLo, kBoundLo, kBoundLo, kBoundHi, kBoundHi, kBoundHi };
        // B.3.5 bridge — ?SetBounds@Geometry@Graphics@@QEAAXAEBVBox@Volume@@@Z.
        reinterpret_cast<Graphics::Geometry*>(geo)->SetBounds(
            *reinterpret_cast<const ::Box*>(box));   // B.3.5 bridge
    }

    // Material layer bitmask: geo+0x30 = 1 << (material+0x20 & 0x1f).
    void* material = *reinterpret_cast<void**>(d + 0x20);
    if (material) {
        uint32_t layer = *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(material) + 0x20);
        *reinterpret_cast<uint32_t*>(g + 0x30) = 1u << (layer & 0x1f);
    }
}

// ============================================================
// Update (vtable +0x08) — port of FUN_1400d2e60(geo, Time* dt).
// Integrates particle positions and wraps them into the [-half, half] box,
// carrying prevPos so motion-blur streaks stay continuous.
// ============================================================
extern "C" void WeatherParticle_Update(void* geo, void* time) {
    if (!geo) return;
    uint8_t* g = reinterpret_cast<uint8_t*>(geo);
    void* desc = *reinterpret_cast<void**>(g + 0xc8);
    if (!desc) return;
    uint8_t* d = reinterpret_cast<uint8_t*>(desc);

    float sec = reinterpret_cast<const System::Time*>(time)->AsSeconds();   // B.3.4b bridge

    // Intensity fade toward the fade-direction target, clamped [0,1].
    float transition = *reinterpret_cast<float*>(d + 0x5c);   // Weather.transitionTime
    float intensity = (transition != 0.0f)
        ? (sec / transition) * *reinterpret_cast<float*>(g + 0xc4) + *reinterpret_cast<float*>(g + 0xc0)
        : *reinterpret_cast<float*>(g + 0xc0);
    if (intensity > kUVScale) intensity = kUVScale;
    else if (intensity < 0.0f) intensity = 0.0f;
    *reinterpret_cast<float*>(g + 0xc0) = intensity;

    float box = *reinterpret_cast<float*>(d + 0x48);   // Weather.size
    float half = box * kHalf;
    float negHalf = -half;

    int count = *reinterpret_cast<int*>(g + 0xe8);
    float* particles = *reinterpret_cast<float**>(g + 0xe0);
    if (!particles) return;

    for (int i = 0; i < count; ++i) {
        float* p = particles + (size_t)i * 9;
        // prevPos = pos (before integrating), then pos += vel*dt.
        p[3] = p[0]; p[4] = p[1]; p[5] = p[2];
        p[0] += sec * p[6];
        p[1] += sec * p[7];
        p[2] += sec * p[8];
        // Wrap each axis into [-half, half], shifting prevPos identically.
        for (int a = 0; a < 3; ++a) {
            while (p[a] > half)     { p[a] -= box; p[a + 3] -= box; }
            while (p[a] < negHalf)  { p[a] += box; p[a + 3] += box; }
        }
    }
}

// ============================================================
// AddDrawItem (vtable +0x18) — port of FUN_1400d30b0(geo, unused, Renderer*).
// ============================================================
extern "C" void WeatherParticle_AddDrawItem(void* geo, void* /*unused*/, void* renderer) {
    if (!geo || !renderer || !fn_Renderer_AddDrawItem) return;
    if (SettingsByte(0x16) == 0) return;   // weather effects disabled in settings
    uint8_t* g = reinterpret_cast<uint8_t*>(geo);
    void* desc = *reinterpret_cast<void**>(g + 0xc8);
    if (!desc) return;
    void* material = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(desc) + 0x20);
    if (!material) return;
    fn_Renderer_AddDrawItem(renderer, geo, material, 0xffff, 0, 0.0f, kUVScale, 1, nullptr, 0);
}

// Draw (vtable +0x10) — big instanced-billboard port, in weather_particle_draw.cpp.
extern "C" void WeatherParticle_Draw(void* geo, void* context, void* camera);

// ============================================================
// WeatherEffect_SetEnabled — port of ComponentEntity_impl_42c0(sim, slot, enable).
// Enables/disables one weather effect slot (0/1). ENABLE creates a particle
// Geometry from the envRes descriptor and adds it to the Graphics scene;
// DISABLE flags it to fade out. Audio-emitter spawn (the +8 template) is
// EXCLUDED per scope — the visible particle half is fully wired.
//   sim   = weather sim struct (gameWorld+0x130)
//   slot  = 0 or 1
//   enable= build effect if 1, fade out if 0
// ============================================================
extern "C" void WeatherEffect_SetEnabled(void* sim, int slot, int enable) {
    if (!sim) return;
    uint8_t* s = reinterpret_cast<uint8_t*>(sim);
    void** activeArray = *reinterpret_cast<void***>(s + 0x50);   // effect geometry array
    if (!activeArray) return;

    if (enable == 0) {
        // DISABLE: mark the effect to fade out (Update ramps intensity toward 0).
        void* g = activeArray[slot];
        if (g) {
            *reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(g) + 0xc4) = -1.0f;
            *reinterpret_cast<int*>(s + 0x10) = 3;   // sim state = fading
        }
        return;
    }

    // ENABLE: only build if this slot has no live effect yet.
    if (activeArray[slot] != nullptr) return;

    void* envRes = *reinterpret_cast<void**>(s + 0x00);
    if (!envRes) return;
    void* arrayBase = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(envRes) + 0x28);
    if (!arrayBase) return;
    void* entry = reinterpret_cast<uint8_t*>(arrayBase) + (size_t)slot * 0x10;
    void* descriptor = *reinterpret_cast<void**>(entry);
    if (!descriptor) return;

    if (!fn_MemAlloc || !g_pMemAllocSingleton) return;
    void* mem = fn_MemAlloc(g_pMemAllocSingleton, 0xf0, 8);
    if (!mem) return;
    void* geo = WeatherParticle_GeoCtor(mem);
    activeArray[slot] = geo;

    WeatherParticle_Config(geo, descriptor);

    // Add to the Graphics scene global object list (gameWorld+0x60).
    // B.3.5: the `fn_Graphics_Scene_AddGlobalObject` readiness term is gone; the
    // two singleton tests are object tests and stay.
    if (g_pGameWorldSingleton && *g_pGameWorldSingleton) {
        void* scene = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) + 0x60);
        if (scene)
            reinterpret_cast<Graphics::Scene*>(scene)->AddGlobalObject(
                reinterpret_cast<Graphics::Geometry*>(geo));   // B.3.5 bridge
    }
    *reinterpret_cast<int*>(s + 0x10) = 0;   // sim state = active
    // NOTE: the audio ambient entity (entry+8 template) is intentionally not
    // spawned here — audio is out of scope for this task.
}

// Inherited Graphics::Geometry base virtuals (+0x20..+0x40) — reuse the
// forwarders from geometry_draw.cpp (identical base vtable tail for all
// Graphics::Geometry subclasses).
extern "C" void  GeomDraw_UpdateAttached(void* self);
extern "C" void* GeomDraw_GetSubTransform(void* self, void* retval, int idx);
extern "C" void* GeomDraw_GetInstanceTransform(void* self, void* retval, int idx, float t);
extern "C" int   GeomDraw_SetSharedInstanceData(void* self, void* context, int idx, float t);
extern "C" bool  GeomDraw_Clean(void* self);

// ============================================================
// WeatherResize — port of FUN_1400198a0. Stride-0x10 array resize (16-byte
// {Weather* descriptor, ExternalPtr audioTemplate} elements) used by
// EnvironmentDescription::Serialize to size the weather-descriptor array on
// load. Header {ptr(+0), count(+8), cap(+0xc)}. Copies existing, zero-fills new.
//
// ENDGAME B.3.2 step 2: now `Collection::Array<>::SetSize` over the
// `kSpec_WeatherEntry` row. Compared arm for arm against rtcoll::SetSizeCore
// and the retail body 0x1400198a0 (re-decompiled 2026-09-13): exact-equal early
// return, `_size` first, treble-on-single-append-from-non-empty, cap<<4 at
// align 8, two-qword copy of the prefix only when the block moved, two-qword
// zero init for the tail, old block freed only when it moved, no destroy.
// Every arm matched; only the `if (dst)` out-of-memory guard is gone.
// ============================================================
namespace {
// ENDGAME B.3.2 closing pass: as for the particle row above, the hand-derived
// row said exactly what `SpecFor<Bytes<0x10,8>>()` derives — whole-stride byte
// copy, whole-stride zero tail, no destroy, no untouched extent.
using WeatherEntryElem = GameColl::Bytes<0x10, 8>;   // exe 0x1400198a0
static_assert(sizeof(WeatherEntryElem)  == 0x10, "WeatherEntryElem stride");
static_assert(alignof(WeatherEntryElem) == 8,    "WeatherEntryElem align");
using WeatherEntryArray = Collection::Array<WeatherEntryElem>;
}  // namespace

extern "C" void Reimpl_WeatherArray_Resize(void* hdr, int newCount) {
    // B.3.2 bridge: EnvironmentDescription's weather array is still a raw triple.
    reinterpret_cast<WeatherEntryArray*>(hdr)->SetSize(newCount);
}

// ============================================================
// STANDALONE vtable + registration
// ============================================================
#include "address_registry.h"

static void* g_WeatherParticle_Vtable[9] = {
    reinterpret_cast<void*>(&WeatherParticle_GeoDtor),         // +0x00
    reinterpret_cast<void*>(&WeatherParticle_Update),          // +0x08
    reinterpret_cast<void*>(&WeatherParticle_Draw),            // +0x10
    reinterpret_cast<void*>(&WeatherParticle_AddDrawItem),     // +0x18
    reinterpret_cast<void*>(&GeomDraw_UpdateAttached),         // +0x20
    reinterpret_cast<void*>(&GeomDraw_GetSubTransform),        // +0x28
    reinterpret_cast<void*>(&GeomDraw_GetInstanceTransform),   // +0x30
    reinterpret_cast<void*>(&GeomDraw_SetSharedInstanceData),  // +0x38
    reinterpret_cast<void*>(&GeomDraw_Clean),                  // +0x40
};

extern "C" void RegisterWeatherParticleVtable() {
    RegisterExeAddr(kWeatherGeometryVtable,
                    reinterpret_cast<uintptr_t>(g_WeatherParticle_Vtable));

    // The debug-UI draw combo (DebugUI_OnDrawComboChanged) constructs the very
    // same weather-particle Geometry via exe helpers DrawGeometry_Ctor
    // (0x1400d3f10) and DrawGeometry_Init (0x1400d3f80). Those are exactly our
    // WeatherParticle_GeoCtor / WeatherParticle_Config (same alloc->ctor->config
    // sequence used in-game at weather_particles.cpp SpawnEffect). Register them
    // so ResolveAddr(Module::Exe, ...) returns non-null and the combo works.
    RegisterExeAddr(0x1400d3f10, reinterpret_cast<uintptr_t>(&WeatherParticle_GeoCtor));
    RegisterExeAddr(0x1400d3f80, reinterpret_cast<uintptr_t>(&WeatherParticle_Config));
}
