// code/Game/Game/tornado.cpp
// Standalone factory for Tornado (factory cascade — last concrete misc landmine).
//
// Tornado (size 0x90) extends Graphics::Particle. The exe factory
// (Toolbar_StaticInit_sub_4e90 @ 0x140024e90):
//     p = MemAlloc(0x90, 8); return FUN_1400d23c0(p);
// and the ctor FUN_1400d23c0 @ 0x1400d23c0:
//     Graphics::Particle::Particle(p)        // base ctor, sets base vtable + [0x00,0x48)
//     *(p)        = &vtable_Tornado(0x140160190)
//     <float block at +0x48 .. +0x70>
//     *(qword p+0x74) = -1                   // = two System::Colors, both opaque white
//     System::Time::Time(p+0x80, g_fUVScale)
//     System::Time::Time(p+0x88, g_fUVScale)
//
// vtable_Tornado at 0x140160190 is TEN slots (0x140160190..0x1401601d8). What looks
// like slots 10-19 is the separate TornadoInstance vtable starting at 0x1401601e0 —
// the two share an identical 0x14003aXXX Runtime-thunk tail, which is exactly what
// makes them read as one 20-slot table. Slots, from the BYTES:
//     3 = 0x1400d2370  GetFileVersion | 1
//     5 = 0x1400d2210  Serialize        <- reads the .rsc tornado fields
//     8 = 0x1400d21b0  CreateInstance   -> TornadoInstance (vtable 0x1401601e0,
//                                          whose slot 1 = 0x1400d2470 is the swirl sim)
// the rest are Object boilerplate or Runtime thunks.
//
// The Tornado particle is PURELY VISUAL. All tornado gameplay — the path, the
// movement, and the destruction — lives in TornadoComponent
// (code/Game/Component/tornadocomponent.cpp, vtable 0x14015aa80), which does not
// touch this class at all.
//
// DORMANT-SAFE FACTORY (same policy as frustumoutlineelement.cpp): we run the
// real Graphics::Particle base ctor and fill the Tornado float block, but DO
// NOT install the exe subclass vtable (0x140160190 — exe->Runtime thunks that
// don't resolve standalone). The object keeps the working Particle base vtable
// installed by fn_Graphics_Particle_Ctor. Divergence: GetTypeId reports
// Particle and the Tornado-specific vtable overrides are not applied. It is still
// registered so External::Load(Tornado.rsc) gets a valid object instead of the
// NullFactory -> IsDerivedFrom(NULL) crash at Runtime+0xC039A.
//
// COST OF THE DIVERGENCE, now that TornadoComponent is live (2026-08-22 wave):
//   * slot 5 (Serialize) not installed => the .rsc's tornado-specific fields are
//     never read, so the block below stays at these ctor defaults forever;
//   * slot 8 (CreateInstance) not installed => the base Particle::CreateInstance
//     runs and yields a plain ParticleInstance, so there is no funnel to look at.
// Neither stops the component: it moves and destroys regardless. It does, however,
// feed the component's LIFETIME gate — Update kills the tornado when the entity's
// ParticleComponent reports no live instance — which is why that gate is traced.
// Deliberately left dormant for this wave; see the [Tornado] spawn trace line.

#include "Object/factory_registration.h"
#include "engine_funcs.h"
#include <cstdint>
#include <cstdio>

// Discriminator for the tornado ParticleComponent null (see particlecomponent.cpp
// EMPTY GEOMETRY). Every OTHER ParticleAttachment in the game points at a
// `ParticleSystem` resource -- a complete Runtime class -- and resolves. The tornado
// alone points at THIS class, whose registration is deliberately dormant. Two very
// different failures produce the same null ExternalPtr, and only this trace separates
// them: factory never called => External::Load found no type at all; factory called
// but the pointer still null => the object was built and then REJECTED (the dormant
// slot 5 Serialize cannot read Tornado.rsc bodies).
static void TornadoFactoryTrace(const char* who, void* p) {
    static int budget = 0;
    if (budget >= 8) return;
    budget++;
    fprintf(stderr, "[Tornado/particle] %s -> %p (dormant-safe: vtable stays Particle, "
            "slot5 Serialize + slot8 CreateInstance NOT installed)\n", who, p);
    fflush(stderr);
}

static void* Tornado_Build(void* p) {
    if (!p) return nullptr;
    if (fn_Graphics_Particle_Ctor) fn_Graphics_Particle_Ctor(p);
    uint8_t* s = reinterpret_cast<uint8_t*>(p);
    // Tornado-specific defaults (FUN_1400d23c0). Field meanings are from
    // Tornado::Serialize (0x1400d2210, which names the types) cross-read against
    // TornadoInstance::Update (0x1400d2470, which consumes them).
    *(float*)(s + 0x48) = 4.0f;    // 0x40800000  radial swirl amplitude
    *(float*)(s + 0x4c) = 20.0f;   // 0x41a00000  vertical rise over a particle's life
    *(float*)(s + 0x50) = 5.0f;    // 0x40a00000  outer sweep amplitude
    *(float*)(s + 0x54) = 45.0f;   // 0x42340000  spin rate, degrees/sec
    *(float*)(s + 0x58) = 1.0f;    // 0x3f800000  per-particle angular frequency
    *(float*)(s + 0x5c) = 1.0f;    // 0x3f800000  emission RATE (interval = 1/x seconds)
    *(float*)(s + 0x60) = 10.0f;   // 0x41200000  particle lifetime, base
    *(float*)(s + 0x64) = 0.0f;    //             particle lifetime, random tolerance
    *(float*)(s + 0x68) = 0.2f;    // 0x3e4ccccd  min size clamp (also the initial size)
    *(float*)(s + 0x6c) = 3.0f;    // 0x40400000  max size clamp
    *(float*)(s + 0x70) = 30.0f;   // 0x41f00000  size scale, clamped into [+0x68,+0x6c]
    // +0x74 and +0x78 are two System::Colors — START and END tint — NOT "a qword -1".
    // Serialize reads them with System::Color::Serialize, and the instance update
    // divides their bytes by 255.0 (DAT_14017222c). All-0xFF = opaque white at both
    // ends. The bytes written here are unchanged; only the description was wrong.
    *(int32_t*)(s + 0x74) = -1;    // start colour RGBA = 0xFFFFFFFF
    *(int32_t*)(s + 0x78) = -1;    // end   colour RGBA = 0xFFFFFFFF
    // Two System::Time fields seeded with g_fUVScale (= 1.0f): fade-in and fade-out.
    if (fn_Time_Ctor_Float) {
        float uv = g_fUVScale ? *g_fUVScale : 1.0f;
        fn_Time_Ctor_Float(s + 0x80, uv);
        fn_Time_Ctor_Float(s + 0x88, uv);
    }
    return p;
}
extern "C" void* Tornado_CreateInPlace_SA(void* mem) {
    TornadoFactoryTrace("CreateInPlace", mem);
    return Tornado_Build(mem);
}
extern "C" void* Tornado_Factory_SA(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x90, 8);
    void* built = Tornado_Build(p);
    TornadoFactoryTrace("Factory", built);
    return built;
}
REGISTER_TYPE(Tornado, "Graphics::Particle",
              &Tornado_Factory_SA, &Tornado_CreateInPlace_SA);
