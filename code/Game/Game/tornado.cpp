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
//     *(qword p+0x74) = -1
//     System::Time::Time(p+0x80, g_fUVScale)
//     System::Time::Time(p+0x88, g_fUVScale)
//
// DORMANT-SAFE FACTORY (same policy as frustumoutlineelement.cpp): we run the
// real Graphics::Particle base ctor and fill the Tornado float block, but DO
// NOT install the exe subclass vtable (0x140160190 — exe->Runtime thunks that
// don't resolve standalone). The object keeps the working Particle base vtable
// installed by fn_Graphics_Particle_Ctor. Divergence: GetTypeId reports
// Particle and the Tornado-specific vtable overrides are not applied. This is
// acceptable for landmine removal — no current path drives a Tornado; it is
// registered so External::Load(Tornado.rsc) gets a valid object instead of the
// NullFactory -> IsDerivedFrom(NULL) crash at Runtime+0xC039A.

#include "Object/factory_registration.h"
#include "engine_funcs.h"
#include <cstdint>

static void* Tornado_Build(void* p) {
    if (!p) return nullptr;
    if (fn_Graphics_Particle_Ctor) fn_Graphics_Particle_Ctor(p);
    uint8_t* s = reinterpret_cast<uint8_t*>(p);
    // Tornado-specific float defaults (FUN_1400d23c0).
    *(float*)(s + 0x48) = 4.0f;    // 0x40800000
    *(float*)(s + 0x4c) = 20.0f;   // 0x41a00000
    *(float*)(s + 0x50) = 5.0f;    // 0x40a00000
    *(float*)(s + 0x54) = 45.0f;   // 0x42340000
    *(float*)(s + 0x58) = 1.0f;    // 0x3f800000
    *(float*)(s + 0x5c) = 1.0f;    // 0x3f800000
    *(float*)(s + 0x60) = 10.0f;   // 0x41200000 (low dword of the qword store; high = 0)
    *(float*)(s + 0x64) = 0.0f;
    *(float*)(s + 0x68) = 0.2f;    // 0x3e4ccccd
    *(float*)(s + 0x6c) = 3.0f;    // 0x40400000
    *(float*)(s + 0x70) = 30.0f;   // 0x41f00000
    *(int32_t*)(s + 0x74) = -1;    // qword -1 written as two dwords (unaligned)
    *(int32_t*)(s + 0x78) = -1;
    // Two System::Time fields seeded with g_fUVScale (= 1.0f).
    if (fn_Time_Ctor_Float) {
        float uv = g_fUVScale ? *g_fUVScale : 1.0f;
        fn_Time_Ctor_Float(s + 0x80, uv);
        fn_Time_Ctor_Float(s + 0x88, uv);
    }
    return p;
}
extern "C" void* Tornado_CreateInPlace_SA(void* mem) {
    return Tornado_Build(mem);
}
extern "C" void* Tornado_Factory_SA(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x90, 8);
    return Tornado_Build(p);
}
REGISTER_TYPE(Tornado, "Graphics::Particle",
              &Tornado_Factory_SA, &Tornado_CreateInPlace_SA);
