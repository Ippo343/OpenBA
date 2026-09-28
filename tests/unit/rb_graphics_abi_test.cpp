// ============================================================================
// tests/unit/rb_graphics_abi_test.cpp — ENDGAME B.3.8 block RB, the Graphics
// Runtime bases the exe derives from: Geometry (the StatusIcon member and two
// LOOP geometries), ParticleInstance (the Tornado instance), Particle
// (Tornado), AnimationEvent (AnimAudioEvent).
//
// B38_RULES D1 / RULE B8-1: real ctors whose bodies are ONE body with the ctor
// doors (`InitFields`; reached by the doors through `X::DoorBody`), and real
// dtors — Geometry's shared with its door, ParticleInstance's (RemoveState,
// 0x1801041f0) and Particle's (0x180103c70, ported by RB) with no door.
//
// PART A — STAMP ORDER + RETAIL BYTES through test subclasses over a
//   0xCD-poisoned buffer: Geometry's shared body runs under Geometry's table,
//   the inline bodies leave exactly retail's bytes and gaps, and teardown runs
//   ParticleInstance's body under its own table BEFORE Geometry's.
// PART B — the doors reach the same bodies with a stand-in minted table.
//
// MARKERS: Geometry's ctor / dtor (scene.cpp), Particle's
// ReleaseFields (animation.cpp), RemoveState and the classes' virtuals do not
// link here; the inline InitFields of Particle / ParticleInstance are REAL.
// ============================================================================
#include "unit.h"
#include "slot_check.h"

#include "runtime/types/graphics/Geometry.h"
#include "runtime/types/graphics/ParticleInstance.h"
#include "runtime/types/graphics/Particle.h"
#include "runtime/types/graphics/Animation.h"
#include "runtime/types/graphics/Mesh.h"
#include "runtime/types/graphics/SunLight.h"
#include "runtime/types/graphics/Material.h"
#include "runtime/types/graphics/GeneratedImageBuffer.h"
#include "runtime/types/graphics/FrameCopyImageBuffer.h"   // B.3.9 GFX

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <new>
#include <type_traits>

extern const void* const g_rbGeoVtGroup[] __asm__("_ZTVN8Graphics8GeometryE");
extern const void* const g_rbPiVtGroup[]  __asm__("_ZTVN8Graphics16ParticleInstanceE");
extern const void* const g_rbPaVtGroup[]  __asm__("_ZTVN8Graphics8ParticleE");
extern const void* const g_rbMiVtGroup[]  __asm__("_ZTVN8Graphics12MeshInstanceE");
extern const void* const g_rbSlVtGroup[]  __asm__("_ZTVN8Graphics8SunLightE");
extern const void* const g_rbMatVtGroup[] __asm__("_ZTVN8Graphics16MaterialInstanceE");
extern const void* const g_rbRiVtGroup[]  __asm__("_ZTVN8Graphics21ReflectionImageBufferE");

namespace {
const void* GeoVt() { return &g_rbGeoVtGroup[2]; }
const void* PiVt()  { return &g_rbPiVtGroup[2]; }
const void* PaVt()  { return &g_rbPaVtGroup[2]; }
const void* VptrOf(const void* p) { return *static_cast<const void* const*>(p); }

enum Ev { kGeoInit = 1, kGeoRel, kPiRemoveState, kPaRel, kMiInit, kMiRel, kSlInit, kSlRel,
          kAlRel, kSpRel, kSpTargets, kPsRel };
uint32_t g_spKindAtTargets = 0;
struct Entry { int ev; const void* vptr; };
Entry g_log[16];
int   g_n = 0;
void Log(int ev, const void* self) { if (g_n < 16) g_log[g_n++] = Entry{ev, VptrOf(self)}; }

}  // namespace

// ===========================================================================
// Markers (see the banner).
// ===========================================================================
namespace Graphics {
Geometry::Geometry()  { Log(kGeoInit, this); _attachNode = -1; _detailId = 2; }
Geometry::~Geometry() { Log(kGeoRel, this); }
void Geometry::UpdateAttached() {}
Math::Transform* Geometry::GetSubTransform(Math::Transform* ret, int) const { return ret; }
Math::Matrix* Geometry::GetInstanceTransform(Math::Matrix* ret, int, float) const { return ret; }
int  Geometry::SetSharedInstanceData(Video::Context&, int, float) const { return 0; }
bool Geometry::Clean() { return false; }

void ParticleInstance::Draw(Video::Context&, const Camera&, ExternalPtrHandle,
                            float, int, int, int) const {}
void ParticleInstance::AddDrawItems(const Scene&, Renderer&) const {}
Math::Matrix* ParticleInstance::GetInstanceTransform(Math::Matrix* ret, int, float) const { return ret; }
void ParticleInstance::SetParticle(ExternalPtrHandle) {}
void ParticleInstance::RemoveState() { Log(kPiRemoveState, this); }

int32_t     Particle::GetFileVersion() const { return 0x1000; }
bool        Particle::Serialize(void*) { return true; }
void        Particle::ReleaseFields() { Log(kPaRel, this); }

// ENDGAME B.3.9 block GFX: ParticleSystem is concrete; its out-of-line members
// are markers here (its vtable emits in this TU). The ctor marker makes one
// store so the test can see it ran after Particle's.
ParticleSystem::ParticleSystem() : Particle() { _f48 = 5.0f; }
ParticleSystem::~ParticleSystem() { Log(kPsRel, this); }
int32_t     ParticleSystem::GetFileVersion() const { return 0x1001; }
bool        ParticleSystem::Serialize(void*) { return true; }
Object::External* ParticleSystem::Reconstruct() { return this; }
void        ParticleSystem::Destruct() {}
ParticleInstance* ParticleSystem::CreateSystem() const { return nullptr; }
Object::External* ParticleSystem::Construct() { return this; }

// ENDGAME B.3.9 GFX: the ctor / dtor are the members themselves (mesh.cpp).
MeshInstance::MeshInstance()  { Log(kMiInit, this); _sortBias = 0; }
MeshInstance::~MeshInstance() { Log(kMiRel, this); }
void MeshInstance::Update(const System::Time&) {}
void MeshInstance::Draw(Video::Context&, const Camera&, ExternalPtrHandle, float, int, int, int) const {}
void MeshInstance::AddDrawItems(const Scene&, Renderer&) const {}
Math::Transform* MeshInstance::GetSubTransform(Math::Transform* ret, int) const { return ret; }
Math::Matrix* MeshInstance::GetInstanceTransform(Math::Matrix* ret, int, float) const { return ret; }
int MeshInstance::SetSharedInstanceData(Video::Context&, int, float) const { return 0; }

Light::~Light() {}

// ENDGAME B.3.9 block GFX: AmbientLight / SpotLight are concrete; their
// out-of-line members are markers here (the vtables emit in this TU).
AmbientLight::~AmbientLight() { Log(kAlRel, this); }
void AmbientLight::Update(const System::Time&) {}
void AmbientLight::Draw(Video::Context&, const Camera&, ExternalPtrHandle, float, int, int, int) const {}
void AmbientLight::AddDrawItems(const Scene&, Renderer&) const {}
void AmbientLight::DrawShadowMap(Video::Context&, Renderer&, int, int) const {}
void AmbientLight::DrawLight(Video::Context&, Renderer&, int, int, int, int) const {}
SpotLight::~SpotLight() { Log(kSpRel, this); }
void SpotLight::CreateTargets() { Log(kSpTargets, this); g_spKindAtTargets = _materialTypeFlags; }
void SpotLight::Update(const System::Time&) {}
void SpotLight::Draw(Video::Context&, const Camera&, ExternalPtrHandle, float, int, int, int) const {}
void SpotLight::AddDrawItems(const Scene&, Renderer&) const {}
void SpotLight::DrawShadowMap(Video::Context&, Renderer&, int, int) const {}
void SpotLight::DrawLight(Video::Context&, Renderer&, int, int, int, int) const {}

void SunLight::InitFields()    { Log(kSlInit, this); _dirZ = -1.0f; }
void SunLight::ReleaseFields() { Log(kSlRel, this); }
void SunLight::Update(const System::Time&) {}
void SunLight::Draw(Video::Context&, const Camera&, ExternalPtrHandle, float, int, int, int) const {}
void SunLight::AddDrawItems(const Scene&, Renderer&) const {}
void SunLight::DrawShadowMap(Video::Context&, Renderer&, int, int) const {}
void SunLight::DrawLight(Video::Context&, Renderer&, int, int, int, int) const {}
}  // namespace Graphics

namespace {

// ---- stand-ins for the exe subclasses -------------------------------------
struct RbGeometry final : Graphics::Geometry {   // StatusIcon member / LOOP geometry
    Graphics::Geometry* DeletingDtor(unsigned) override { return this; }
    void Update(const System::Time&) override {}
    void Draw(Video::Context&, const Graphics::Camera&, Graphics::ExternalPtrHandle,
              float, int, int, int) const override {}
    void AddDrawItems(const Graphics::Scene&, Graphics::Renderer&) const override {}
};
struct RbTornadoInstance final : Graphics::ParticleInstance {
    Graphics::Geometry* DeletingDtor(unsigned) override { return this; }
    void Update(const System::Time&) override {}
};
struct RbTornado final : Graphics::Particle {
    Object::Base* DeletingDtor(unsigned) override { return this; }
    Object::External* Reconstruct() override { return this; }
    void Destruct() override {}
    Graphics::ParticleInstance* CreateSystem() const override { return nullptr; }
};
struct RbAnimAudioEvent final : Graphics::AnimationEvent {
    Object::Base* DeletingDtor(unsigned) override { return this; }
    int32_t GetFileVersion() const override { return 0; }
    bool Serialize(void*) override { return true; }
    Object::External* Reconstruct() override { return this; }
    void Destruct() override {}
    Object::External* Construct() override { return this; }
};

template <class T> T* BuildIn(unsigned char* buf) {
    std::memset(buf, 0xCD, sizeof(T));
    return ::new (static_cast<void*>(buf)) T;
}
uint64_t Q(const void* p, size_t off) { uint64_t v; std::memcpy(&v, static_cast<const unsigned char*>(p) + off, 8); return v; }
uint32_t D(const void* p, size_t off) { uint32_t v; std::memcpy(&v, static_cast<const unsigned char*>(p) + off, 4); return v; }
uint8_t  By(const void* p, size_t off) { return static_cast<const unsigned char*>(p)[off]; }

// ===========================================================================
// PART A
// ===========================================================================
UNIT_TEST(rb_geometry_ctor_dtor_run_shared_bodies_under_base_table) {
    g_n = 0;
    alignas(8) unsigned char buf[sizeof(RbGeometry)];
    RbGeometry* g = BuildIn<RbGeometry>(buf);
    CHECK_EQ(g_n, 1);
    CHECK_EQ(g_log[0].ev, (int)kGeoInit);
    CHECK(g_log[0].vptr == GeoVt());
    CHECK(VptrOf(g) != GeoVt());
    g->~RbGeometry();
    CHECK_EQ(g_n, 2);
    CHECK_EQ(g_log[1].ev, (int)kGeoRel);
    CHECK(g_log[1].vptr == GeoVt());
}

UNIT_TEST(rb_particleinstance_ctor_is_retail_0x180104130_and_dtor_0x1801041f0) {
    g_n = 0;
    alignas(8) unsigned char buf[sizeof(RbTornadoInstance)];
    RbTornadoInstance* pi = BuildIn<RbTornadoInstance>(buf);
    CHECK_EQ(g_n, 1);
    CHECK(g_log[0].vptr == GeoVt());            // Geometry's body under Geometry's table
    CHECK_EQ(Q(pi, 0xc0), (uint64_t)0);         // _resource
    CHECK_EQ(Q(pi, 0xc8), (uint64_t)0);         // _boundsPtr
    CHECK_EQ(D(pi, 0xd0), 0xFFFFFFFFu);         // _index = -1
    CHECK_EQ(D(pi, 0xd4), 0xCDCDCDCDu);         // not written
    pi->~RbTornadoInstance();
    CHECK_EQ(g_n, 3);
    CHECK_EQ(g_log[1].ev, (int)kPiRemoveState); CHECK(g_log[1].vptr == PiVt());
    CHECK_EQ(g_log[2].ev, (int)kGeoRel);        CHECK(g_log[2].vptr == GeoVt());
}

UNIT_TEST(rb_particle_ctor_is_retail_0x180103e80_and_dtor_0x180103c70) {
    g_n = 0;
    alignas(8) unsigned char buf[sizeof(RbTornado)];
    RbTornado* p = BuildIn<RbTornado>(buf);
    CHECK_EQ(Q(p, 0x08), (uint64_t)0);          // External()
    CHECK_EQ(Q(p, 0x10), (uint64_t)0);
    CHECK_EQ(Q(p, 0x18), (uint64_t)0);
    CHECK_EQ((int)By(p, 0x20), 1);              // _active
    CHECK_EQ((int)By(p, 0x21), 0xCD);           // gap
    CHECK_EQ((int)By(p, 0x23), 0xCD);
    CHECK_EQ(D(p, 0x24), 1u);                   // _recordCount
    CHECK_EQ(D(p, 0x28), 0xfu);                 // _subCount
    CHECK_EQ(D(p, 0x2c), 0xCDCDCDCDu);          // gap
    CHECK_EQ(Q(p, 0x30), (uint64_t)0);          // _material
    CHECK_EQ(Q(p, 0x38), (uint64_t)0);          // _records
    CHECK_EQ(Q(p, 0x40), (uint64_t)0);
    p->~RbTornado();
    CHECK_EQ(g_n, 1);
    CHECK_EQ(g_log[0].ev, (int)kPaRel);
    CHECK(g_log[0].vptr == PaVt());
}

UNIT_TEST(rb_animationevent_ctor_is_retail_0x1800a2150) {
    alignas(8) unsigned char buf[sizeof(RbAnimAudioEvent)];
    RbAnimAudioEvent* e = BuildIn<RbAnimAudioEvent>(buf);
    CHECK_EQ(Q(e, 0x08), (uint64_t)0);
    CHECK_EQ(Q(e, 0x10), (uint64_t)0);
    CHECK_EQ(Q(e, 0x18), (uint64_t)0);
    e->~RbAnimAudioEvent();
}

// ===========================================================================
// PART B
// ===========================================================================
UNIT_TEST(rb_graphics_doors_reach_the_same_bodies) {
    // ParticleInstance: (ENDGAME B.3.9 GFX) the ??0 door and its DoorBody are
    // retired; the real ctor is the only builder
    // (rb_particleinstance_ctor_is_retail_* above).

    // Particle: (ENDGAME B.3.9 GFX) the ??0 door and its DoorBody are retired;
    // the real ctor is the only builder (rb_particle_ctor_is_retail_* above).

    // Geometry: (ENDGAME B.3.9 GFX) the ??0 / ??1 doors and DoorBody are
    // retired; the real ctor / dtor are the only bodies
    // (b39_gfx_geometry_slot0_is_the_classs_own_both_arms below).
}

// ===========================================================================
// PART C — the two Geometry leaves the exe instantiates itself (B38_SLOTS
// section 4: 0x14015e240 MeshInstance, 0x14015e290 SunLight): public real
// ctors, and slot 0 is the exe's scalar deleting dtor (0x140019e80 /
// 0x140019e40: ~X, then MemFree on flags & 1).
// ===========================================================================
UNIT_TEST(rb_meshinstance_and_sunlight_are_concrete_with_the_exe_slot0) {
    using Slot0 = void* (*)(void*, unsigned);
    g_n = 0;
    void* m = Object::detail::InternalMemAlloc(sizeof(Graphics::MeshInstance), 8);
    Graphics::MeshInstance* mi = ::new (m) Graphics::MeshInstance();
    CHECK(VptrOf(mi) == &g_rbMiVtGroup[2]);
    CHECK_EQ(g_n, 2);
    CHECK_EQ(g_log[0].ev, (int)kGeoInit); CHECK(g_log[0].vptr == GeoVt());
    CHECK_EQ(g_log[1].ev, (int)kMiInit);  CHECK(g_log[1].vptr == &g_rbMiVtGroup[2]);
    Slot0 s0 = reinterpret_cast<Slot0>(const_cast<void*>(g_rbMiVtGroup[2]));
    CHECK(s0(mi, 1u) == m);
    CHECK_EQ(g_n, 4);
    CHECK_EQ(g_log[2].ev, (int)kMiRel);   CHECK(g_log[2].vptr == &g_rbMiVtGroup[2]);
    CHECK_EQ(g_log[3].ev, (int)kGeoRel);  CHECK(g_log[3].vptr == GeoVt());

    g_n = 0;
    void* sm = Object::detail::InternalMemAlloc(sizeof(Graphics::SunLight), 8);
    Graphics::SunLight* sl = ::new (sm) Graphics::SunLight();
    CHECK(VptrOf(sl) == &g_rbSlVtGroup[2]);
    CHECK_EQ(g_n, 2);
    CHECK_EQ(g_log[1].ev, (int)kSlInit);  CHECK(g_log[1].vptr == &g_rbSlVtGroup[2]);
    Slot0 s1 = reinterpret_cast<Slot0>(const_cast<void*>(g_rbSlVtGroup[2]));
    CHECK(s1(sl, 1u) == sm);
    CHECK_EQ(g_n, 4);
    CHECK_EQ(g_log[2].ev, (int)kSlRel);   CHECK(g_log[2].vptr == &g_rbSlVtGroup[2]);
    CHECK_EQ(g_log[3].ev, (int)kGeoRel);  CHECK(g_log[3].vptr == GeoVt());

    // (MeshInstance's and SunLight's doors are retired -- ENDGAME B.3.9.)
}

// ENDGAME B.3.9 pilot c2: with the ??0/??1 doors gone, the exe's builder
// `::new (p) Graphics::SunLight()` and the table's slot 0 are the only way in
// and out. Slot 0 with flags 0 runs ~SunLight and frees nothing.
UNIT_TEST(b39_sunlight_real_ctor_is_the_only_builder) {
    using Slot0 = void* (*)(void*, unsigned);
    alignas(8) unsigned char buf[sizeof(Graphics::SunLight)] = {};
    g_n = 0;
    Graphics::SunLight* sl = ::new (static_cast<void*>(buf)) Graphics::SunLight();
    CHECK(VptrOf(sl) == &g_rbSlVtGroup[2]);
    CHECK_EQ(g_n, 2);
    CHECK_EQ(g_log[0].ev, (int)kGeoInit); CHECK(g_log[0].vptr == GeoVt());
    CHECK_EQ(g_log[1].ev, (int)kSlInit);  CHECK(g_log[1].vptr == &g_rbSlVtGroup[2]);
    Slot0 s0 = reinterpret_cast<Slot0>(const_cast<void*>(g_rbSlVtGroup[2]));
    CHECK(s0(sl, 0u) == static_cast<void*>(buf));
    CHECK_EQ(g_n, 4);
    CHECK_EQ(g_log[2].ev, (int)kSlRel);   CHECK(g_log[2].vptr == &g_rbSlVtGroup[2]);
    CHECK_EQ(g_log[3].ev, (int)kGeoRel);  CHECK(g_log[3].vptr == GeoVt());
}

UNIT_TEST(rb_materialinstance_ctor_is_retail_0x180103980_and_concrete) {
    // InitFields is inline (the REAL body); the virtuals and ~MaterialInstance
    // are graphics_material_abi_test.cpp's markers.
    alignas(8) unsigned char buf[sizeof(Graphics::MaterialInstance)];
    std::memset(buf, 0xCD, sizeof buf);
    Graphics::MaterialInstance* m = ::new (static_cast<void*>(buf)) Graphics::MaterialInstance();
    CHECK(VptrOf(m) == &g_rbMatVtGroup[2]);
    CHECK_EQ(D(m, 0x20), 0u);
    CHECK_EQ((int)By(m, 0x24), 0);
    CHECK_EQ((int)By(m, 0x25), 0);
    CHECK_EQ((int)By(m, 0x26), 0xCD);           // never written
    for (size_t off = 0x28; off < 0x60; off += 8) CHECK_EQ(Q(m, off), (uint64_t)0);
    // (ENDGAME B.3.9 GFX: the ??0 door and DoorBody are retired; the real ctor
    // is the only builder. Retail's External() zeroes run under the base.)
    CHECK_EQ(Q(m, 0x08), (uint64_t)0);
    CHECK_EQ(Q(m, 0x10), (uint64_t)0);
    CHECK_EQ(Q(m, 0x18), (uint64_t)0);
    CHECK(!std::is_destructible<Graphics::MaterialInstance>::value);
    CHECK(!std::has_virtual_destructor<Graphics::MaterialInstance>::value);
    m->DeletingDtor(0);

    void* mem = Object::detail::InternalMemAlloc(sizeof(Graphics::MaterialInstance), 8);
    Graphics::MaterialInstance* h = ::new (mem) Graphics::MaterialInstance();
    using Slot0 = Object::Base* (*)(void*, unsigned);
    CHECK(static_cast<void*>(reinterpret_cast<Slot0>(const_cast<void*>(g_rbMatVtGroup[2]))(h, 1u)) == mem);
}

UNIT_TEST(rb_graphics_bases_shape) {
    CHECK(std::is_abstract<Graphics::Geometry>::value);
    CHECK(std::is_abstract<Graphics::ParticleInstance>::value);
    CHECK(std::is_abstract<Graphics::Particle>::value);
    CHECK(std::is_abstract<Graphics::AnimationEvent>::value);
    CHECK(!std::has_virtual_destructor<Graphics::Geometry>::value);
    CHECK(!std::has_virtual_destructor<Graphics::Particle>::value);
    CHECK_EQ(sizeof(RbGeometry), (size_t)0xc0);
    CHECK_EQ(sizeof(RbTornadoInstance), (size_t)0xd8);
    CHECK_EQ(sizeof(RbTornado), (size_t)0x48);
    CHECK_EQ(sizeof(RbAnimAudioEvent), (size_t)0x20);
    CHECK(!std::is_abstract<Graphics::MeshInstance>::value);
    CHECK(!std::is_abstract<Graphics::SunLight>::value);
    CHECK(!std::has_virtual_destructor<Graphics::MeshInstance>::value);
    CHECK(!std::has_virtual_destructor<Graphics::SunLight>::value);
}

// ===========================================================================
// ENDGAME B.3.9 block GFX -- the Light family concrete (B39_RULES D1, B9-16).
// ===========================================================================

UNIT_TEST(b39_gfx_lights_shape_and_slot_order) {
    CHECK(!std::is_abstract<Graphics::AmbientLight>::value);
    CHECK(!std::is_abstract<Graphics::SpotLight>::value);
    CHECK(std::is_abstract<Graphics::Light>::value);
    CHECK(!std::is_destructible<Graphics::AmbientLight>::value);
    CHECK(!std::is_destructible<Graphics::SpotLight>::value);
    CHECK(!std::has_virtual_destructor<Graphics::AmbientLight>::value);
    CHECK(!std::has_virtual_destructor<Graphics::SpotLight>::value);
    using A = Graphics::AmbientLight;
    using S = Graphics::SpotLight;
    CHECK_DTOR_SLOT(A);   // 0x180059080
    CHECK_SLOT(A, Update,        0x08/8);   // 0x1800fa7a0
    CHECK_SLOT(A, Draw,          0x10/8);   // 0x1800fa790
    CHECK_SLOT(A, AddDrawItems,  0x18/8);   // 0x1800fa890
    CHECK_SLOT(A, DrawShadowMap, 0x48/8);   // 0x1800fa880
    CHECK_SLOT(A, DrawLight,     0x50/8);   // 0x1800fa7b0
    CHECK_DTOR_SLOT(S);   // 0x1800642b0
    CHECK_SLOT(S, Update,        0x08/8);   // 0x180105950
    CHECK_SLOT(S, Draw,          0x10/8);   // 0x180105940
    CHECK_SLOT(S, AddDrawItems,  0x18/8);   // 0x180105e30
    CHECK_SLOT(S, DrawShadowMap, 0x48/8);   // 0x180105d00
    CHECK_SLOT(S, DrawLight,     0x50/8);   // 0x180105960
}

// ??0AmbientLight 0x1800fa940: Geometry(), the colour zeroed, 10.0f at +0xd0,
// the kind word 0x8000 at +0x30; +0xd4 untouched.
UNIT_TEST(b39_gfx_ambientlight_ctor_stores_and_both_slot0_arms) {
    alignas(8) unsigned char buf[sizeof(Graphics::AmbientLight)];
    std::memset(buf, 0xCD, sizeof buf);
    g_n = 0;
    Graphics::AmbientLight* a = ::new (static_cast<void*>(buf)) Graphics::AmbientLight();
    CHECK_EQ(g_n, 1);
    CHECK_EQ(g_log[0].ev, (int)kGeoInit);
    CHECK_EQ(Q(a, 0xc0), (uint64_t)0);
    CHECK_EQ(Q(a, 0xc8), (uint64_t)0);
    CHECK_EQ(D(a, 0xd0), 0x41200000u);
    CHECK_EQ(D(a, 0xd4), 0xCDCDCDCDu);          // never written
    CHECK_EQ(D(a, 0x30), 0x8000u);
    const void* vt = VptrOf(a);
    CHECK(vt != GeoVt());
    CHECK(a->DeletingDtor(0) == a);             // destroy, no free
    CHECK_EQ(g_n, 3);
    CHECK_EQ(g_log[1].ev, (int)kAlRel); CHECK(g_log[1].vptr == vt);
    CHECK_EQ(g_log[2].ev, (int)kGeoRel); CHECK(g_log[2].vptr == GeoVt());

    void* m = Object::detail::InternalMemAlloc(sizeof(Graphics::AmbientLight), 8);
    Graphics::AmbientLight* h = ::new (m) Graphics::AmbientLight();
    CHECK(h->DeletingDtor(1) == h);             // destroy AND free

    // The vector arm: int32 count at this-8, elements destroyed DOWNWARD with
    // stride 0xd8, block freed, this-8 returned.
    uint8_t* block = static_cast<uint8_t*>(
        Object::detail::InternalMemAlloc(8 + 3 * sizeof(Graphics::AmbientLight), 8));
    const int64_t cookie = 3;
    std::memcpy(block, &cookie, sizeof cookie);
    Graphics::AmbientLight* e[3];
    for (int i = 0; i < 3; ++i)
        e[i] = ::new (block + 8 + i * sizeof(Graphics::AmbientLight)) Graphics::AmbientLight();
    g_n = 0;
    CHECK(reinterpret_cast<uint8_t*>(e[0]->DeletingDtor(3)) == block);
    CHECK_EQ(g_n, 6);
    CHECK(g_log[0].vptr == vt);
}

// ??0SpotLight 0x180106690: the frustum's planes zero, isOrthographic 1, 0.1f /
// 10.0f, +0x130/+0x134/+0x158 NOT written, colour and the three pointers zero,
// CreateTargets BEFORE the kind word; ~SpotLight runs its body then ~Geometry.
UNIT_TEST(b39_gfx_spotlight_ctor_stores_and_dtor_chain) {
    alignas(8) unsigned char buf[sizeof(Graphics::SpotLight)];
    std::memset(buf, 0xCD, sizeof buf);
    g_n = 0;
    g_spKindAtTargets = 0;
    Graphics::SpotLight* s = ::new (static_cast<void*>(buf)) Graphics::SpotLight();
    CHECK_EQ(g_n, 2);
    CHECK_EQ(g_log[0].ev, (int)kGeoInit);
    CHECK_EQ(g_log[1].ev, (int)kSpTargets);
    CHECK(g_spKindAtTargets != 0x8000u);        // the kind word comes AFTER the call
    for (size_t off = 0xc0; off <= 0x118; off += 8) CHECK_EQ(Q(s, off), (uint64_t)0);
    CHECK_EQ((int)By(s, 0x120), 1);
    CHECK_EQ(D(s, 0x128), 0x3dcccccdu);
    CHECK_EQ(D(s, 0x12c), 0x41200000u);
    CHECK_EQ(D(s, 0x130), 0xCDCDCDCDu);
    CHECK_EQ(D(s, 0x134), 0xCDCDCDCDu);
    CHECK_EQ(Q(s, 0x138), (uint64_t)0);
    CHECK_EQ(Q(s, 0x140), (uint64_t)0);
    CHECK_EQ(Q(s, 0x148), (uint64_t)0);
    CHECK_EQ(Q(s, 0x150), (uint64_t)0);
    CHECK_EQ(Q(s, 0x158), 0xCDCDCDCDCDCDCDCDull);   // CreateTargets' to fill
    CHECK_EQ(Q(s, 0x160), (uint64_t)0);
    CHECK_EQ(D(s, 0x30), 0x8000u);
    s->_shadowTarget = nullptr;                      // the marker CreateTargets built none
    const void* vt = VptrOf(s);
    CHECK(s->DeletingDtor(0) == s);
    CHECK_EQ(g_n, 4);
    CHECK_EQ(g_log[2].ev, (int)kSpRel); CHECK(g_log[2].vptr == vt);
    CHECK_EQ(g_log[3].ev, (int)kGeoRel); CHECK(g_log[3].vptr == GeoVt());

    void* m = Object::detail::InternalMemAlloc(sizeof(Graphics::SpotLight), 8);
    Graphics::SpotLight* h = ::new (m) Graphics::SpotLight();
    h->_shadowTarget = nullptr;
    CHECK(h->DeletingDtor(1) == h);
}

// ---- Particle / ParticleSystem (vtable rows 97 / 99) ----------------------
struct RbParticleLeaf final : Graphics::Particle {   // slot 0 NOT overridden
    Object::External* Reconstruct() override { return this; }
    void Destruct() override {}
    Graphics::ParticleInstance* CreateSystem() const override { return nullptr; }
};

UNIT_TEST(b39_gfx_particle_slot0_is_the_classs_own_both_arms) {
    // Particle stays abstract (three pure slots); its slot 0 is concrete now.
    CHECK(std::is_abstract<Graphics::Particle>::value);
    CHECK(!std::is_destructible<Graphics::Particle>::value);
    CHECK_DTOR_SLOT(Graphics::Particle);   // 0x180020160
    CHECK_SLOT(Graphics::Particle, CreateSystem, 0x40/8);   // _purecall, B5-16

    void* m = Object::detail::InternalMemAlloc(sizeof(RbParticleLeaf), 8);
    RbParticleLeaf* p = ::new (m) RbParticleLeaf();
    g_n = 0;
    CHECK(static_cast<void*>(p->DeletingDtor(1)) == m);      // ~Particle, freed
    CHECK_EQ(g_n, 1);
    CHECK_EQ(g_log[0].ev, (int)kPaRel);

    uint8_t* block = static_cast<uint8_t*>(Object::detail::InternalMemAlloc(8 + 2 * 0x48, 8));
    const int64_t cookie = 2;
    std::memcpy(block, &cookie, sizeof cookie);
    RbParticleLeaf* e0 = ::new (block + 8) RbParticleLeaf();
    ::new (block + 8 + 0x48) RbParticleLeaf();
    g_n = 0;
    CHECK(reinterpret_cast<uint8_t*>(e0->DeletingDtor(3)) == block);   // stride 0x48, downward
    CHECK_EQ(g_n, 2);
}

UNIT_TEST(b39_gfx_particlesystem_concrete_slot_order_and_both_slot0_arms) {
    using PS = Graphics::ParticleSystem;
    CHECK(!std::is_abstract<PS>::value);
    CHECK(!std::is_destructible<PS>::value);
    CHECK(!std::has_virtual_destructor<PS>::value);
    CHECK_DTOR_SLOT(PS);   // 0x180020a70
    CHECK_SLOT(PS, TypeName,       0x08/8);   // 0x1800203e0
    CHECK_SLOT(PS, Type,           0x10/8);   // 0x1800203c0
    CHECK_SLOT(PS, GetFileVersion, 0x18/8);   // 0x1800a4d10
    CHECK_SLOT(PS, Serialize,      0x28/8);   // 0x1800a4d20
    CHECK_SLOT(PS, Reconstruct,    0x30/8);   // 0x180020560
    CHECK_SLOT(PS, Destruct,       0x38/8);   // 0x180020590
    CHECK_SLOT(PS, CreateSystem,   0x40/8);   // 0x1800a5140
    CHECK_SLOT(PS, Construct,      0x48/8);   // 0x180020580

    // Particle's six stores run under the base ctor (the marker ctor adds _f48).
    alignas(8) unsigned char buf[sizeof(PS)];
    std::memset(buf, 0xCD, sizeof buf);
    PS* ps = ::new (static_cast<void*>(buf)) PS();
    CHECK_EQ((int)By(ps, 0x20), 1);
    CHECK_EQ(D(ps, 0x24), 1u);
    CHECK_EQ(D(ps, 0x28), 0xfu);
    CHECK_EQ(D(ps, 0x2c), 0xCDCDCDCDu);
    const void* vt = VptrOf(ps);
    g_n = 0;
    CHECK(static_cast<void*>(ps->DeletingDtor(0)) == static_cast<void*>(buf));
    CHECK_EQ(g_n, 2);                                         // ~ParticleSystem, then ~Particle
    CHECK_EQ(g_log[0].ev, (int)kPsRel); CHECK(g_log[0].vptr == vt);
    CHECK_EQ(g_log[1].ev, (int)kPaRel); CHECK(g_log[1].vptr == PaVt());

    void* m = Object::detail::InternalMemAlloc(sizeof(PS), 8);
    PS* h = ::new (m) PS();
    CHECK(static_cast<void*>(h->DeletingDtor(1)) == m);

    uint8_t* block = static_cast<uint8_t*>(Object::detail::InternalMemAlloc(8 + 2 * 0x108, 8));
    const int64_t cookie = 2;
    std::memcpy(block, &cookie, sizeof cookie);
    PS* e0 = ::new (block + 8) PS();
    PS* e1 = ::new (block + 8 + 0x108) PS();
    g_n = 0;
    CHECK(reinterpret_cast<uint8_t*>(e0->DeletingDtor(3)) == block);
    CHECK_EQ(g_n, 4);
    CHECK_EQ(g_log[0].ev, (int)kPsRel);
    CHECK(g_log[0].vptr == vt);
    (void)e1;
}

}  // namespace

// ===========================================================================
// ENDGAME B.3.9 block GFX -- ReflectionImageBuffer CONCRETE (D1).
// Its out-of-line members are MARKERS (imagebuffer_derived.cpp does not link
// here; Video::ImageBuffer's and GeneratedImageBuffer's are rb_video_abi_test's
// markers); ~ReflectionImageBuffer records each call, so the real inline
// DeletingDtor can be seen running it.
// ===========================================================================
namespace {
int         g_riRel = 0;
const void* g_riRelVptr = nullptr;
const void* g_riRelOrder[4] = {};
const void* RiVptrOf(const void* p) { return *static_cast<const void* const*>(p); }
}  // namespace
namespace Graphics {
int32_t           ReflectionImageBuffer::GetFileVersion() const { return 0x1007; }
bool              ReflectionImageBuffer::Serialize(void*) { return true; }
Object::External* ReflectionImageBuffer::Reconstruct() { return this; }
void              ReflectionImageBuffer::Destruct() {}
Object::External* ReflectionImageBuffer::Construct() { return this; }
void ReflectionImageBuffer::Generate(void*, int32_t) const {}
void ReflectionImageBuffer::Render(void*, const void*, const void*, int32_t) const {}
ReflectionImageBuffer::~ReflectionImageBuffer() {
    if (g_riRel < 4) g_riRelOrder[g_riRel] = this;
    ++g_riRel; g_riRelVptr = RiVptrOf(this);
}
}  // namespace Graphics

namespace {

// The ctor is ImageBuffer() -> GeneratedImageBuffer() -> retail 0x1800a7130's
// stores (+0x70 / +0x78 null, plane (0, 0, 1, 0)); slot 0 is the class's own
// VECTOR deleting dtor (0x180022600, `MOV EDX,0x90`), both arms.
UNIT_TEST(b39_gfx_reflectionimagebuffer_ctor_stores_and_both_slot0_arms) {
    using R = Graphics::ReflectionImageBuffer;
    CHECK(!std::is_abstract<R>::value);
    CHECK(!std::is_destructible<R>::value);            // ~R protected
    CHECK(!std::has_virtual_destructor<R>::value);
    CHECK_EQ(sizeof(R), (size_t)0x90);

    g_riRel = 0;
    void* mem = Object::detail::InternalMemAlloc(sizeof(R), 8);
    std::memset(mem, 0xCD, sizeof(R));
    R* r = ::new (mem) R();
    const uint8_t* b = static_cast<const uint8_t*>(mem);
    auto D = [&](size_t off) { uint32_t v; std::memcpy(&v, b + off, 4); return v; };
    auto Q = [&](size_t off) { uint64_t v; std::memcpy(&v, b + off, 8); return v; };
    CHECK(RiVptrOf(r) == &g_rbRiVtGroup[2]);           // the compiler's table
    CHECK_EQ(D(0x68), 0u);                             // GeneratedImageBuffer()
    CHECK_EQ(D(0x6c), 0xCDCDCDCDu);                    // never written
    CHECK_EQ(Q(0x70), (uint64_t)0);
    CHECK_EQ(Q(0x78), (uint64_t)0);
    CHECK_EQ(Q(0x80), (uint64_t)0);
    CHECK_EQ(Q(0x88), (uint64_t)0x3f800000ull);        // retail's one imm64 store

    // scalar arm, flags 1: ~R under R's table, then the chain, then free.
    CHECK(static_cast<void*>(r->DeletingDtor(1u)) == mem);
    CHECK_EQ(g_riRel, 1);
    CHECK(g_riRelVptr == &g_rbRiVtGroup[2]);

    // vector arm, flags 3: count at block-8, destroyed downward, block freed.
    uint8_t* block = static_cast<uint8_t*>(Object::detail::InternalMemAlloc(8 + 3 * 0x90, 8));
    const int64_t cookie = 3;
    std::memcpy(block, &cookie, sizeof cookie);
    R* e[3];
    for (int i = 0; i < 3; ++i) e[i] = ::new (block + 8 + i * 0x90) R();
    g_riRel = 0;
    CHECK(reinterpret_cast<uint8_t*>(e[0]->DeletingDtor(3u)) == block);
    CHECK_EQ(g_riRel, 3);
    CHECK(g_riRelOrder[0] == e[2]);
    CHECK(g_riRelOrder[1] == e[1]);
    CHECK(g_riRelOrder[2] == e[0]);
}

}  // namespace

// ===========================================================================
// ENDGAME B.3.9 block GFX -- FrameCopyImageBuffer CONCRETE (its real ctor / dtor
// are the only builders now; moved here from rb_video_abi_test.cpp's PART D) and
// GeneratedImageBuffer's own VECTOR slot 0 (the class stays abstract).
// Video::ImageBuffer's and GeneratedImageBuffer's markers are
// rb_video_abi_test's; FrameCopy's out-of-line members are markers here.
// ===========================================================================
namespace {
int         g_fcCtor = 0, g_fcDtor = 0;
const void* g_fcCtorVptr = nullptr;
const void* g_fcDtorVptr = nullptr;
int32_t     g_fcGenSeen = -1;
const void* FcVptrOf(const void* p) { return *static_cast<const void* const*>(p); }
}  // namespace
namespace Graphics {
int32_t           FrameCopyImageBuffer::GetFileVersion() const { return 0x1007; }
bool              FrameCopyImageBuffer::Serialize(void*) { return true; }
Object::External* FrameCopyImageBuffer::Reconstruct() { return this; }
void              FrameCopyImageBuffer::Destruct() {}
Object::External* FrameCopyImageBuffer::Construct() { return this; }
bool FrameCopyImageBuffer::Create(ImageType, Usage, Format, uint32_t, int32_t, int32_t,
                                  FilterMode, AddressMode, AddressMode, AddressMode, LevelArray*) {
    return false;
}
void FrameCopyImageBuffer::Generate(void*, int32_t) const {}
void FrameCopyImageBuffer::Render(void*, const void*, const void*, int32_t) const {}
FrameCopyImageBuffer::FrameCopyImageBuffer() {
    ++g_fcCtor; g_fcCtorVptr = FcVptrOf(this); g_fcGenSeen = _generation;
    _backBufferImage = nullptr; _sourceTarget = nullptr; _destTarget = nullptr;
}
FrameCopyImageBuffer::~FrameCopyImageBuffer() { ++g_fcDtor; g_fcDtorVptr = FcVptrOf(this); }
}  // namespace Graphics

extern const void* const g_rbFcVtGroup[] __asm__("_ZTVN8Graphics20FrameCopyImageBufferE");

namespace {

struct RbGeneratedLeaf final : Graphics::GeneratedImageBuffer {   // slot 0 NOT overridden
    void Generate(void*, int32_t) const override {}
    void Render(void*, const void*, const void*, int32_t) const override {}
};

UNIT_TEST(b39_gfx_framecopy_concrete_ctor_dtor_and_exe_scalar_slot0) {
    using F = Graphics::FrameCopyImageBuffer;
    CHECK(!std::is_abstract<F>::value);
    CHECK(!std::is_destructible<F>::value);            // ~F protected
    CHECK(!std::has_virtual_destructor<F>::value);
    CHECK_DTOR_SLOT(F);   // exe copy 0x140019d50
    CHECK_SLOT(F, TypeName,       0x08/8);   // 0x18001efb0
    CHECK_SLOT(F, Type,           0x10/8);   // 0x18001ef90
    CHECK_SLOT(F, GetFileVersion, 0x18/8);   // 0x1800a3c70
    CHECK_SLOT(F, Serialize,      0x28/8);   // 0x1800a3c40
    CHECK_SLOT(F, Reconstruct,    0x30/8);   // 0x18001f130
    CHECK_SLOT(F, Destruct,       0x38/8);   // 0x18001f160
    CHECK_SLOT(F, Construct,      0x40/8);   // 0x18001f150
    CHECK_SLOT(F, Create,         0x48/8);   // 0x1800a3b70
    CHECK_SLOT(F, Generate,       0x50/8);   // 0x1800a3b20
    CHECK_SLOT(F, Render,         0x58/8);   // 0x1800a3b10

    // The chain: ImageBuffer() -> GeneratedImageBuffer() (`_generation = 0`)
    // -> the FrameCopy body under FrameCopy's (compiler) table.
    g_fcCtor = g_fcDtor = 0; g_fcGenSeen = -1;
    void* mem = Object::detail::InternalMemAlloc(sizeof(F), 8);
    std::memset(mem, 0xCD, sizeof(F));
    F* f = ::new (mem) F();
    CHECK_EQ(g_fcCtor, 1);
    CHECK(g_fcCtorVptr == &g_rbFcVtGroup[2]);
    CHECK_EQ(g_fcGenSeen, 0);
    CHECK(FcVptrOf(f) == &g_rbFcVtGroup[2]);

    // Slot 0 is the exe copy's SCALAR deleting dtor: ~F, then free on flags & 1.
    CHECK(static_cast<void*>(f->DeletingDtor(1u)) == mem);
    CHECK_EQ(g_fcDtor, 1);
    CHECK(g_fcDtorVptr == &g_rbFcVtGroup[2]);

    alignas(8) unsigned char buf[sizeof(F)];
    F* g = ::new (static_cast<void*>(buf)) F();
    CHECK(static_cast<void*>(g->DeletingDtor(0u)) == static_cast<void*>(buf));   // no free
    CHECK_EQ(g_fcDtor, 2);
}

// GeneratedImageBuffer: abstract (slots 10 / 11 `_purecall`), protected
// non-virtual dtor, slot 0 its own VECTOR deleting dtor (`MOV EDX,0x70`).
UNIT_TEST(b39_gfx_generatedimagebuffer_slot0_is_the_classs_own_both_arms) {
    using G = Graphics::GeneratedImageBuffer;
    CHECK(std::is_abstract<G>::value);
    CHECK(!std::is_destructible<G>::value);
    CHECK(!std::has_virtual_destructor<G>::value);
    CHECK_EQ(sizeof(RbGeneratedLeaf), (size_t)0x70);
    CHECK_DTOR_SLOT(G);   // 0x18001eee0
    CHECK_SLOT(G, Generate, 0x50/8);
    CHECK_SLOT(G, Render,   0x58/8);

    alignas(8) unsigned char buf[sizeof(RbGeneratedLeaf)];
    std::memset(buf, 0xCD, sizeof buf);
    RbGeneratedLeaf* g = ::new (static_cast<void*>(buf)) RbGeneratedLeaf;   // no value-init
    uint32_t gen; std::memcpy(&gen, buf + 0x68, 4);
    CHECK_EQ(gen, 0u);                                             // the ctor's one store
    uint32_t pad; std::memcpy(&pad, buf + 0x6c, 4);
    CHECK_EQ(pad, 0xCDCDCDCDu);                                    // never written
    CHECK(static_cast<void*>(g->DeletingDtor(0u)) == static_cast<void*>(buf));

    void* m = Object::detail::InternalMemAlloc(sizeof(RbGeneratedLeaf), 8);
    RbGeneratedLeaf* h = ::new (m) RbGeneratedLeaf();
    CHECK(static_cast<void*>(h->DeletingDtor(1u)) == m);

    uint8_t* block = static_cast<uint8_t*>(Object::detail::InternalMemAlloc(8 + 2 * 0x70, 8));
    const int64_t cookie = 2;
    std::memcpy(block, &cookie, sizeof cookie);
    RbGeneratedLeaf* e0 = ::new (block + 8) RbGeneratedLeaf();
    ::new (block + 8 + 0x70) RbGeneratedLeaf();
    CHECK(reinterpret_cast<uint8_t*>(e0->DeletingDtor(3u)) == block);   // stride 0x70, freed
}

}  // namespace

// ===========================================================================
// ENDGAME B.3.9 block GFX -- ParticleSystemInstance CONCRETE, ParticleInstance
// slot 0 concrete (the class stays abstract: slot 1 `_purecall`). PSI's
// out-of-line members are markers here (wave28 / wave27 / batchc do not link).
// ===========================================================================
namespace {
int         g_psiDtor = 0;
const void* g_psiDtorVptr = nullptr;
const void* g_psiDtorOrder[4] = {};
}  // namespace
namespace Graphics {
void ParticleSystemInstance::Update(const System::Time&) {}
void ParticleSystemInstance::SetParticle(ExternalPtrHandle) {}
ParticleSystemInstance::~ParticleSystemInstance() {
    if (g_psiDtor < 4) g_psiDtorOrder[g_psiDtor] = this;
    ++g_psiDtor; g_psiDtorVptr = *static_cast<const void* const*>(static_cast<const void*>(this));
}
}  // namespace Graphics

extern const void* const g_rbPsiVtGroup[] __asm__("_ZTVN8Graphics22ParticleSystemInstanceE");

namespace {

struct RbPiLeaf final : Graphics::ParticleInstance {   // slot 0 NOT overridden
    void Update(const System::Time&) override {}
};

UNIT_TEST(b39_gfx_particlesysteminstance_concrete_ctor_and_both_slot0_arms) {
    using P = Graphics::ParticleSystemInstance;
    CHECK(!std::is_abstract<P>::value);
    CHECK(!std::is_destructible<P>::value);
    CHECK(!std::has_virtual_destructor<P>::value);
    CHECK_DTOR_SLOT(P);   // 0x180020ce0
    CHECK_PMF_SLOT(static_cast<void (P::*)(const System::Time&)>(&P::Update),
                   0x08/8);                                               // 0x1800a5600
    CHECK_SLOT(P, GetInstanceTransform, 0x30/8);   // PI 0x1801047f0
    CHECK_SLOT(P, SetParticle,          0x48/8);   // 0x1800a6a80

    // Retail 0x1800a55a0: Geometry(), +0xc0/+0xc8 = 0, +0xd0 = -1, +0xd8 = 0.
    g_n = 0;
    alignas(8) unsigned char buf[sizeof(P)];
    std::memset(buf, 0xCD, sizeof buf);
    P* p = ::new (static_cast<void*>(buf)) P();
    CHECK_EQ(g_n, 1);
    CHECK_EQ(g_log[0].ev, (int)kGeoInit);
    CHECK(g_log[0].vptr == GeoVt());
    CHECK(VptrOf(p) == &g_rbPsiVtGroup[2]);
    CHECK_EQ(Q(p, 0xc0), (uint64_t)0);
    CHECK_EQ(Q(p, 0xc8), (uint64_t)0);
    CHECK_EQ(D(p, 0xd0), 0xFFFFFFFFu);
    CHECK_EQ(D(p, 0xd4), 0xCDCDCDCDu);          // not written
    CHECK_EQ(Q(p, 0xd8), (uint64_t)0);

    // Slot 0, flags 0: ~PSI under PSI's table, then RemoveState under PI's,
    // then Geometry's release under Geometry's; nothing freed.
    g_n = 0; g_psiDtor = 0;
    CHECK(static_cast<void*>(p->DeletingDtor(0u)) == static_cast<void*>(buf));
    CHECK_EQ(g_psiDtor, 1);
    CHECK(g_psiDtorVptr == &g_rbPsiVtGroup[2]);
    CHECK_EQ(g_n, 2);
    CHECK_EQ(g_log[0].ev, (int)kPiRemoveState); CHECK(g_log[0].vptr == PiVt());
    CHECK_EQ(g_log[1].ev, (int)kGeoRel);        CHECK(g_log[1].vptr == GeoVt());

    void* m = Object::detail::InternalMemAlloc(sizeof(P), 8);
    P* h = ::new (m) P();
    CHECK(static_cast<void*>(h->DeletingDtor(1u)) == m);

    // Vector arm: stride 0xe0, downward, block freed.
    uint8_t* block = static_cast<uint8_t*>(Object::detail::InternalMemAlloc(8 + 3 * 0xe0, 8));
    const int64_t cookie = 3;
    std::memcpy(block, &cookie, sizeof cookie);
    P* e[3];
    for (int i = 0; i < 3; ++i) e[i] = ::new (block + 8 + i * 0xe0) P();
    g_psiDtor = 0;
    CHECK(reinterpret_cast<uint8_t*>(e[0]->DeletingDtor(3u)) == block);
    CHECK_EQ(g_psiDtor, 3);
    CHECK(g_psiDtorOrder[0] == e[2]);
    CHECK(g_psiDtorOrder[2] == e[0]);
}

UNIT_TEST(b39_gfx_particleinstance_slot0_is_the_classs_own_both_arms) {
    using PI = Graphics::ParticleInstance;
    CHECK(std::is_abstract<PI>::value);
    CHECK(!std::is_destructible<PI>::value);
    CHECK_DTOR_SLOT(PI);   // 0x180020310
    CHECK_EQ(sizeof(RbPiLeaf), (size_t)0xd8);

    void* m = Object::detail::InternalMemAlloc(sizeof(RbPiLeaf), 8);
    RbPiLeaf* p = ::new (m) RbPiLeaf();
    g_n = 0;
    CHECK(static_cast<void*>(p->DeletingDtor(1u)) == m);       // ~PI, freed
    CHECK_EQ(g_n, 2);
    CHECK_EQ(g_log[0].ev, (int)kPiRemoveState);

    uint8_t* block = static_cast<uint8_t*>(Object::detail::InternalMemAlloc(8 + 2 * 0xd8, 8));
    const int64_t cookie = 2;
    std::memcpy(block, &cookie, sizeof cookie);
    RbPiLeaf* e0 = ::new (block + 8) RbPiLeaf();
    ::new (block + 8 + 0xd8) RbPiLeaf();
    g_n = 0;
    CHECK(reinterpret_cast<uint8_t*>(e0->DeletingDtor(3u)) == block);   // stride 0xd8
    CHECK_EQ(g_n, 4);
}

}  // namespace

// ===========================================================================
// ENDGAME B.3.9 block GFX -- EdgeMeshInstance CONCRETE (retail ctor 0x1800a3ab0,
// slot 0 0x1800175d0 vector, element 0xd0). Its out-of-line members are
// markers here (mesh.cpp / meshinstance_draw.cpp do not link).
// ===========================================================================
namespace {
int         g_emiDtor = 0;
const void* g_emiDtorVptr = nullptr;
const void* g_emiDtorOrder[4] = {};
}  // namespace
namespace Graphics {
void EdgeMeshInstance::Update(const System::Time&) {}
void EdgeMeshInstance::Draw(Video::Context&, const Camera&, ExternalPtrHandle,
                            float, int, int, int) const {}
void EdgeMeshInstance::AddDrawItems(const Scene&, Renderer&) const {}
Math::Matrix* EdgeMeshInstance::GetInstanceTransform(Math::Matrix* ret, int, float) const {
    return ret;
}
EdgeMeshInstance::~EdgeMeshInstance() {
    if (g_emiDtor < 4) g_emiDtorOrder[g_emiDtor] = this;
    ++g_emiDtor; g_emiDtorVptr = *static_cast<const void* const*>(static_cast<const void*>(this));
}
}  // namespace Graphics

extern const void* const g_rbEmiVtGroup[] __asm__("_ZTVN8Graphics16EdgeMeshInstanceE");

namespace {

UNIT_TEST(b39_gfx_edgemeshinstance_concrete_ctor_and_both_slot0_arms) {
    using E = Graphics::EdgeMeshInstance;
    CHECK(!std::is_abstract<E>::value);
    CHECK(!std::is_destructible<E>::value);
    CHECK(!std::has_virtual_destructor<E>::value);
    CHECK_DTOR_SLOT(E);   // 0x1800175d0
    CHECK_SLOT(E, GetInstanceTransform, 0x30/8);   // 0x1800a3840 RETSLOT

    // Retail 0x1800a3ab0: Geometry(), +0xc0 = 0; +0xc8 NOT written.
    g_n = 0;
    alignas(8) unsigned char buf[sizeof(E)];
    std::memset(buf, 0xCD, sizeof buf);
    E* p = ::new (static_cast<void*>(buf)) E();
    CHECK_EQ(g_n, 1);
    CHECK_EQ(g_log[0].ev, (int)kGeoInit);
    CHECK(g_log[0].vptr == GeoVt());
    CHECK(VptrOf(p) == &g_rbEmiVtGroup[2]);
    CHECK_EQ(Q(p, 0xc0), (uint64_t)0);
    CHECK_EQ(Q(p, 0xc8), (uint64_t)0xCDCDCDCDCDCDCDCDull);   // not written

    // Slot 0, flags 0: ~EMI under EMI's table, then Geometry's release; nothing freed.
    g_n = 0; g_emiDtor = 0;
    CHECK(static_cast<void*>(p->DeletingDtor(0u)) == static_cast<void*>(buf));
    CHECK_EQ(g_emiDtor, 1);
    CHECK(g_emiDtorVptr == &g_rbEmiVtGroup[2]);
    CHECK_EQ(g_n, 1);
    CHECK_EQ(g_log[0].ev, (int)kGeoRel); CHECK(g_log[0].vptr == GeoVt());

    void* m = Object::detail::InternalMemAlloc(sizeof(E), 8);
    E* h = ::new (m) E();
    CHECK(static_cast<void*>(h->DeletingDtor(1u)) == m);

    // Vector arm: stride 0xd0, downward, block freed.
    uint8_t* block = static_cast<uint8_t*>(Object::detail::InternalMemAlloc(8 + 3 * 0xd0, 8));
    const int64_t cookie = 3;
    std::memcpy(block, &cookie, sizeof cookie);
    E* e[3];
    for (int i = 0; i < 3; ++i) e[i] = ::new (block + 8 + i * 0xd0) E();
    g_emiDtor = 0;
    CHECK(reinterpret_cast<uint8_t*>(e[0]->DeletingDtor(3u)) == block);
    CHECK_EQ(g_emiDtor, 3);
    CHECK(g_emiDtorOrder[0] == e[2]);
    CHECK(g_emiDtorOrder[2] == e[0]);
}

}  // namespace

// ===========================================================================
// ENDGAME B.3.9 block GFX -- Graphics::Geometry slot 0 is the class's own
// VECTOR deleting destructor (retail 0x180015e90, element 0xc0, dtor
// 0x1800a41c0). The class stays abstract; a leaf that does not override
// slot 0 reaches it.
// ===========================================================================
namespace {

struct RbGeoLeaf final : Graphics::Geometry {   // slot 0 NOT overridden
    void Update(const System::Time&) override {}
    void Draw(Video::Context&, const Graphics::Camera&, Graphics::ExternalPtrHandle,
              float, int, int, int) const override {}
    void AddDrawItems(const Graphics::Scene&, Graphics::Renderer&) const override {}
};

UNIT_TEST(b39_gfx_geometry_slot0_is_the_classs_own_both_arms) {
    using G = Graphics::Geometry;
    CHECK(std::is_abstract<G>::value);
    CHECK(!std::is_destructible<G>::value);
    CHECK(!std::has_virtual_destructor<G>::value);
    CHECK_DTOR_SLOT(G);   // 0x180015e90
    CHECK_EQ(sizeof(RbGeoLeaf), (size_t)0xc0);

    // Scalar arm, flags 1: ~Geometry under Geometry's table, freed, returns this.
    void* m = Object::detail::InternalMemAlloc(sizeof(RbGeoLeaf), 8);
    RbGeoLeaf* p = ::new (m) RbGeoLeaf();
    g_n = 0;
    CHECK(static_cast<void*>(p->DeletingDtor(1u)) == m);
    CHECK_EQ(g_n, 1);
    CHECK_EQ(g_log[0].ev, (int)kGeoRel); CHECK(g_log[0].vptr == GeoVt());

    // Scalar arm, flags 0: nothing freed.
    alignas(8) unsigned char buf[sizeof(RbGeoLeaf)];
    RbGeoLeaf* q = ::new (static_cast<void*>(buf)) RbGeoLeaf();
    g_n = 0;
    CHECK(static_cast<void*>(q->DeletingDtor(0u)) == static_cast<void*>(buf));
    CHECK_EQ(g_n, 1);

    // Vector arm: stride 0xc0, count at block, block freed, returns block.
    uint8_t* block = static_cast<uint8_t*>(Object::detail::InternalMemAlloc(8 + 2 * 0xc0, 8));
    const int64_t cookie = 2;
    std::memcpy(block, &cookie, sizeof cookie);
    RbGeoLeaf* e0 = ::new (block + 8) RbGeoLeaf();
    ::new (block + 8 + 0xc0) RbGeoLeaf();
    g_n = 0;
    CHECK(reinterpret_cast<uint8_t*>(e0->DeletingDtor(3u)) == block);
    CHECK_EQ(g_n, 2);
    CHECK_EQ(g_log[0].ev, (int)kGeoRel);
    CHECK_EQ(g_log[1].ev, (int)kGeoRel);
}

}  // namespace
