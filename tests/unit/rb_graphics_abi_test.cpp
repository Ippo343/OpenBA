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
// MARKERS: Geometry's InitFields/ReleaseFields (scene.cpp), Particle's
// ReleaseFields (animation.cpp), RemoveState and the classes' virtuals do not
// link here; the inline InitFields of Particle / ParticleInstance are REAL.
// ============================================================================
#include "unit.h"

#include "runtime/types/graphics/Geometry.h"
#include "runtime/types/graphics/ParticleInstance.h"
#include "runtime/types/graphics/Particle.h"
#include "runtime/types/graphics/Animation.h"
#include "runtime/types/graphics/Mesh.h"
#include "runtime/types/graphics/SunLight.h"
#include "runtime/types/graphics/Material.h"

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

namespace {
const void* GeoVt() { return &g_rbGeoVtGroup[2]; }
const void* PiVt()  { return &g_rbPiVtGroup[2]; }
const void* PaVt()  { return &g_rbPaVtGroup[2]; }
const void* VptrOf(const void* p) { return *static_cast<const void* const*>(p); }

enum Ev { kGeoInit = 1, kGeoRel, kPiRemoveState, kPaRel, kMiInit, kMiRel, kSlInit, kSlRel };
struct Entry { int ev; const void* vptr; };
Entry g_log[16];
int   g_n = 0;
void Log(int ev, const void* self) { if (g_n < 16) g_log[g_n++] = Entry{ev, VptrOf(self)}; }

const void* const kFakeMinted[4] = {};
}  // namespace

// ===========================================================================
// Markers (see the banner).
// ===========================================================================
namespace Graphics {
void Geometry::InitFields()    { Log(kGeoInit, this); _attachNode = -1; _detailId = 2; }
void Geometry::ReleaseFields() { Log(kGeoRel, this); }
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

const char* Particle::TypeName() const { return "Particle"; }
uint32_t    Particle::Type() const { return 0; }
int32_t     Particle::GetFileVersion() const { return 0x1000; }
bool        Particle::Serialize(void*) { return true; }
void        Particle::ReleaseFields() { Log(kPaRel, this); }

void MeshInstance::InitFields()    { Log(kMiInit, this); _sortBias = 0; }
void MeshInstance::ReleaseFields() { Log(kMiRel, this); }
void MeshInstance::Update(const System::Time&) {}
void MeshInstance::Draw(Video::Context&, const Camera&, ExternalPtrHandle, float, int, int, int) const {}
void MeshInstance::AddDrawItems(const Scene&, Renderer&) const {}
Math::Transform* MeshInstance::GetSubTransform(Math::Transform* ret, int) const { return ret; }
Math::Matrix* MeshInstance::GetInstanceTransform(Math::Matrix* ret, int, float) const { return ret; }
int MeshInstance::SetSharedInstanceData(Video::Context&, int, float) const { return 0; }

Light::~Light() {}
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
    // ParticleInstance: ctor bytes == door bytes over +0xc0..+0xd7.
    alignas(8) unsigned char a[sizeof(RbTornadoInstance)];
    RbTornadoInstance* pi = BuildIn<RbTornadoInstance>(a);
    alignas(8) unsigned char b[sizeof(RbTornadoInstance)];
    std::memset(b, 0xCD, sizeof b);
    *reinterpret_cast<const void**>(b) = kFakeMinted;
    Graphics::ParticleInstance::DoorBody::Init(reinterpret_cast<Graphics::ParticleInstance*>(b));
    CHECK(std::memcmp(a + 0xc0, b + 0xc0, 0x18) == 0);
    pi->~RbTornadoInstance();

    // Particle: ctor bytes == door bytes over +0x20..+0x47.
    alignas(8) unsigned char c[sizeof(RbTornado)];
    RbTornado* p = BuildIn<RbTornado>(c);
    alignas(8) unsigned char d[sizeof(RbTornado)];
    std::memset(d, 0xCD, sizeof d);
    *reinterpret_cast<const void**>(d) = kFakeMinted;
    Graphics::Particle::DoorBody::Init(reinterpret_cast<Graphics::Particle*>(d));
    CHECK(std::memcmp(c + 0x20, d + 0x20, 0x28) == 0);
    p->~RbTornado();

    // Geometry: the door's calls run under the minted table.
    alignas(8) unsigned char g[sizeof(Graphics::Geometry)] = {};
    *reinterpret_cast<const void**>(g) = kFakeMinted;
    g_n = 0;
    Graphics::Geometry::DoorBody::Init(reinterpret_cast<Graphics::Geometry*>(g));
    Graphics::Geometry::DoorBody::Release(reinterpret_cast<Graphics::Geometry*>(g));
    CHECK_EQ(g_n, 2);
    CHECK(g_log[0].vptr == static_cast<const void*>(kFakeMinted));
    CHECK(g_log[1].vptr == static_cast<const void*>(kFakeMinted));
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

    // The MeshInstance doors reach the same bodies under the minted table.
    // (SunLight's doors are retired -- ENDGAME B.3.9 pilot c2.)
    alignas(8) unsigned char raw[sizeof(Graphics::MeshInstance)] = {};
    *reinterpret_cast<const void**>(raw) = kFakeMinted;
    g_n = 0;
    Graphics::MeshInstance::DoorBody::Init(reinterpret_cast<Graphics::MeshInstance*>(raw));
    Graphics::MeshInstance::DoorBody::Release(reinterpret_cast<Graphics::MeshInstance*>(raw));
    CHECK_EQ(g_n, 2);
    for (int i = 0; i < 2; ++i) CHECK(g_log[i].vptr == static_cast<const void*>(kFakeMinted));
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
    alignas(8) unsigned char raw[sizeof(Graphics::MaterialInstance)];
    std::memset(raw, 0xCD, sizeof raw);
    *reinterpret_cast<const void**>(raw) = kFakeMinted;
    Graphics::MaterialInstance::DoorBody::Init(reinterpret_cast<Graphics::MaterialInstance*>(raw));
    CHECK(std::memcmp(buf + 0x20, raw + 0x20, sizeof raw - 0x20) == 0);
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

}  // namespace
