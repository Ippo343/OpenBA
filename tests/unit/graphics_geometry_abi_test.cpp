// ============================================================================
// tests/unit/graphics_geometry_abi_test.cpp — ENDGAME B.3 step 5, block E
//   the `Graphics::Geometry` tree as real C++ classes
//
// The sibling of `io_vtable_abi_test.cpp` and `object_vtable_abi_test.cpp`, for
// the nine classes block E converts:
//
//   Graphics::Geometry        ROOT, abstract, 0xC0, 9 slots   (Geometry.h)
//   Graphics::MeshInstance    : Geometry, 0x120, 9 slots      (Mesh.h)
//   Graphics::EdgeMeshInstance: Geometry, 0xd0,  9 slots      (Mesh.h)
//   Graphics::ParticleInstance: Geometry, abstract, 0xd8, 10  (ParticleInstance.h)
//   Graphics::ParticleSystemInstance : ParticleInstance, 0xe0, 10
//   Graphics::Light           : Geometry, abstract, 0xC0, 11  (Light.h)
//   Graphics::AmbientLight    : Light, 0xd8, 11               (Light.h)
//   Graphics::SpotLight       : Light, 0x168, 11              (Light.h)
//   Graphics::SunLight        : Light, 0x108, 11              (SunLight.h)
//
// WHAT THIS FILE HAS TO ESTABLISH
// -------------------------------
// PART A — LAYOUT. Every size, alignment and field offset the headers claim,
//   measured here as well as `static_assert`ed there, so a header edit that
//   moves a field fails a TEST and not only a build somebody may be tempted to
//   "fix". It also pins the two facts the whole tree rests on: that the derived
//   classes' own fields begin at exactly `sizeof(base)` (no tail-padding reuse,
//   no second vptr) and that `Light` adds no bytes at all.
//
// PART B — SLOT ORDER. A local probe hierarchy declared in RETAIL SLOT ORDER,
//   whose GCC-generated vtable is read back slot by slot and compared against
//   the member each index is supposed to name. This is the claim that licenses
//   the conversion to coexist with the MINTED tables in
//   `standalone/rt_data_mint_tables.inc`: an object wearing a hand-built
//   `void*[]` at retail byte offsets and one wearing GCC's table are
//   interchangeable slot for slot, in BOTH directions.
//
//   It is also where RULE B5-17 is tested: retail names slot 2 and slot 10 both
//   `Draw`, EIGHT SLOTS APART, and the conversion gives slot 10 the distinct
//   name `DrawLight`. The probe declares both and checks that a call through
//   byte offset 0x10 reaches the slot-2 body and one through 0x50 reaches the
//   slot-10 body — i.e. that renaming did not reorder anything.
//
// PART C — THE TWO RETSLOT SLOTS (RULE B5-12, AND ITS CORRECTION). Slots 5
//   (`GetSubTransform` -> `Math::Transform`, 0x1c) and 6
//   (`GetInstanceTransform` -> `Math::Matrix`, 0x40) return a class BY VALUE
//   THROUGH A VTABLE SLOT. The 2026-09-13 perf-only crash (`1a4200c3`) was a
//   `void` port of one of them: the caller's storage arrives in RDX and must
//   come back in RAX at every exit, and at -O0 RAX happened to still hold it.
//
//   B5-12 says the fix is to declare them as C++ by-value returns, "because
//   GCC's Microsoft-ABI lowering produces the same hidden slot and the same RAX
//   return". *** THAT IS FALSE, AND PART C IS WHERE IT IS MEASURED. *** GCC
//   passes the hidden return pointer as the FIRST argument, BEFORE `this`, the
//   way the Itanium/SysV ABI does; MSVC — and therefore retail — passes `this`
//   first. `graphics_gcc_sret_precedes_this_unlike_msvc` below demonstrates the
//   swap on a throwaway class, which is why the real slots 5 and 6 are declared
//   with the out-parameter spelled out IN RETAIL ORDER, and
//   `graphics_slot5_and_slot6_carry_the_hidden_return_slot` then shows that a
//   retail-order caller — a minted row, a surviving `fn_*` typedef, one of the
//   118 hand-inlined `(*(void***)p)[N]` sites — reaches them correctly.
//
//   The first version of this test was written to B5-12 as published and
//   SEGFAULTED: the slot-5 call wrote a `Math::Transform` through the probe
//   object's `this`, over its vptr, and the next slot read faulted.
//
// PART D — THE QUALIFIED CALL. Doors delegate with
//   `reinterpret_cast<Ns::C*>(self)->Ns::C::Method(args)`, and the `Ns::C::` is
//   not decoration: a live object in this build wears a MINTED table whose slots
//   resolve back to those very doors, so a plain virtual call inside a door is
//   infinite recursion rather than delegation (B34_BRIEFS §A.1). PART D stamps a
//   probe with a hand-built table and shows that the unqualified call dispatches
//   through it while the qualified one does not.
//
// WHAT IS *NOT* PROVEN HERE
// -------------------------
// Nothing about MSVC, and nothing about the ported bodies. The retail DLL is
// gone; every caller and callee here is this same GCC. The binding binary facts
// are OBJECT LAYOUT and RETAIL SLOT ORDER, and slot order binds only because the
// tables it is compared against were transcribed from retail
// (`B35_SLOTS.md`, read as `.rdata` qwords). The bodies themselves stand on the
// port registry and the allocator and are verified by the real run: none of
// `runtime/src/graphics/*.cpp` can link into a host test, which is why the probe
// hierarchy below is local and the real classes are used for LAYOUT only.
// `Math::Transform` / `Math::Matrix` ARE the real classes (`runtime/src/math/`
// is in `tests/unit/sources.mk`), so PART C's retslot values are real ones.
// ============================================================================
#include "unit.h"

// The shipped headers. The tests measure THESE, not local copies, so a failure
// means the header and the ABI disagree.
#include "runtime/types/graphics/Geometry.h"
#include "runtime/types/graphics/Mesh.h"
#include "runtime/types/graphics/ParticleInstance.h"
#include "runtime/types/graphics/Light.h"
#include "runtime/types/graphics/SunLight.h"

#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace {

// ---------------------------------------------------------------------------
// Observation channel, same shape as io_vtable_abi_test.cpp: every probe body
// records WHICH one ran and on what `this`, so a test asserts the identity of
// the callee and not merely that something returned.
// ---------------------------------------------------------------------------
const void* g_lastSelf = nullptr;
int         g_lastMark = -1;
long long   g_lastArg  = 0;

void Note(const void* self, int mark, long long arg = 0) {
    g_lastSelf = self;
    g_lastMark = mark;
    g_lastArg  = arg;
}
void Reset() { g_lastSelf = nullptr; g_lastMark = -1; g_lastArg = 0; }

// The one instruction every hand-inlined `(*(void***)p)[N]` caller compiles to,
// spelled by BYTE OFFSET because that is how the tree spells it.
template <typename Fn>
Fn SlotAt(const void* obj, int byteOffset) {
    const uint8_t* const* vt = *reinterpret_cast<const uint8_t* const* const*>(obj);
    return reinterpret_cast<Fn>(
        *reinterpret_cast<void* const*>(reinterpret_cast<const uint8_t*>(vt) + byteOffset));
}

// Byte offset of a member measured from two live addresses, for the cases where
// `offsetof` on a polymorphic type would be the conditionally-supported form.
template <typename Obj, typename Field>
uintptr_t FieldOffset(const Obj* obj, const Field* field) {
    return reinterpret_cast<uintptr_t>(field) - reinterpret_cast<uintptr_t>(obj);
}

// ===========================================================================
// PART A — LAYOUT
// ===========================================================================

UNIT_TEST(graphics_geometry_sizes_match_retail) {
    // Each number is the `MOV EDX,imm` immediate in that class's OWN slot-0
    // vector deleting destructor, read as disassembly by block 0 (B35_RULES
    // §2.2). They are not derived from each other.
    CHECK_EQ(sizeof(Graphics::Geometry),               (size_t)0xC0);   // 0x180015e90
    CHECK_EQ(sizeof(Graphics::MeshInstance),           (size_t)0x120);  // 0x180017330
    CHECK_EQ(sizeof(Graphics::EdgeMeshInstance),       (size_t)0xd0);   // 0x1800175d0
    CHECK_EQ(sizeof(Graphics::ParticleInstance),       (size_t)0xd8);   // 0x180020310
    CHECK_EQ(sizeof(Graphics::ParticleSystemInstance), (size_t)0xe0);   // 0x180020ce0
    CHECK_EQ(sizeof(Graphics::Light),                  (size_t)0xC0);   // 0x180058dd0
    CHECK_EQ(sizeof(Graphics::AmbientLight),           (size_t)0xd8);   // 0x180059080
    CHECK_EQ(sizeof(Graphics::SpotLight),              (size_t)0x168);  // 0x1800642b0
    CHECK_EQ(sizeof(Graphics::SunLight),               (size_t)0x108);  // 0x180064620

    // Every one is qword aligned: the vptr is the first member and nothing in
    // the tree has a wider alignment requirement.
    CHECK_EQ(alignof(Graphics::Geometry),               (size_t)8);
    CHECK_EQ(alignof(Graphics::MeshInstance),           (size_t)8);
    CHECK_EQ(alignof(Graphics::EdgeMeshInstance),       (size_t)8);
    CHECK_EQ(alignof(Graphics::ParticleInstance),       (size_t)8);
    CHECK_EQ(alignof(Graphics::ParticleSystemInstance), (size_t)8);
    CHECK_EQ(alignof(Graphics::Light),                  (size_t)8);
    CHECK_EQ(alignof(Graphics::AmbientLight),           (size_t)8);
    CHECK_EQ(alignof(Graphics::SpotLight),              (size_t)8);
    CHECK_EQ(alignof(Graphics::SunLight),               (size_t)8);
}

UNIT_TEST(graphics_light_adds_no_bytes) {
    // ELEVEN slots against Geometry's NINE, and yet the same size. A `Light`
    // with even one data member would move AmbientLight's, SpotLight's and
    // SunLight's first field and break all three at once, so this is
    // load-bearing rather than cosmetic (B35_RULES §2.3 item 3).
    CHECK_EQ(sizeof(Graphics::Light), sizeof(Graphics::Geometry));
}

UNIT_TEST(graphics_derived_fields_begin_at_end_of_base) {
    // The whole tree rests on single inheritance with the base subobject at
    // offset zero and NO tail-padding reuse: retail's constructors all call the
    // base constructor and then write their own first field at exactly
    // `sizeof(base)` with no gap.
    CHECK_EQ(offsetof(Graphics::MeshInstance, _mesh),         (size_t)0xc0);
    CHECK_EQ(offsetof(Graphics::EdgeMeshInstance, _mesh),     (size_t)0xc0);
    CHECK_EQ(offsetof(Graphics::ParticleInstance, _resource), (size_t)0xc0);
    CHECK_EQ(offsetof(Graphics::AmbientLight, _colour),       (size_t)0xc0);
    CHECK_EQ(offsetof(Graphics::SpotLight, _frustum),         (size_t)0xc0);
    CHECK_EQ(offsetof(Graphics::SunLight, _dirX),             (size_t)0xc0);
    // and the one that is two levels deep
    CHECK_EQ(offsetof(Graphics::ParticleSystemInstance, _f_d8),
             sizeof(Graphics::ParticleInstance));
}

UNIT_TEST(graphics_geometry_field_offsets) {
    // Each offset is pinned by the named retail body in Geometry.h's banner.
    CHECK_EQ(offsetof(Graphics::Geometry, _attachParent),      (size_t)0x08);
    CHECK_EQ(offsetof(Graphics::Geometry, _attachNode),        (size_t)0x10);
    CHECK_EQ(offsetof(Graphics::Geometry, _attached),          (size_t)0x18);
    CHECK_EQ(offsetof(Graphics::Geometry, _detailId),          (size_t)0x28);
    CHECK_EQ(offsetof(Graphics::Geometry, _dirtyFlags),        (size_t)0x2c);
    CHECK_EQ(offsetof(Graphics::Geometry, _materialTypeFlags), (size_t)0x30);
    CHECK_EQ(offsetof(Graphics::Geometry, _localBounds),       (size_t)0x34);
    CHECK_EQ(offsetof(Graphics::Geometry, _worldBounds),       (size_t)0x4c);
    CHECK_EQ(offsetof(Graphics::Geometry, _prevWorldBounds),   (size_t)0x64);
    CHECK_EQ(offsetof(Graphics::Geometry, _opacity),           (size_t)0x7c);
    CHECK_EQ(offsetof(Graphics::Geometry, _opacityPrev),       (size_t)0x80);
    CHECK_EQ(offsetof(Graphics::Geometry, _transform),         (size_t)0x84);
    CHECK_EQ(offsetof(Graphics::Geometry, _prevTransform),     (size_t)0xa0);
    CHECK_EQ(offsetof(Graphics::Geometry, _padBC),             (size_t)0xbc);

    // The class and the POD must describe the same bytes: the POD still has
    // live users outside block E's file set, so a drift between the two is the
    // failure this pins.
    CHECK_EQ(sizeof(Graphics::Geometry), sizeof(RtGraphics::Geometry));
    CHECK_EQ(offsetof(Graphics::Geometry, _dirtyFlags),
             offsetof(RtGraphics::Geometry, _dirtyFlags));
    CHECK_EQ(offsetof(Graphics::Geometry, _transform),
             offsetof(RtGraphics::Geometry, _transform));
}

UNIT_TEST(graphics_instance_field_offsets) {
    CHECK_EQ(offsetof(Graphics::MeshInstance, _materials),           (size_t)0xc8);
    CHECK_EQ(offsetof(Graphics::MeshInstance, _animation),           (size_t)0xd8);
    CHECK_EQ(offsetof(Graphics::MeshInstance, _nodes),               (size_t)0xf0);
    CHECK_EQ(offsetof(Graphics::MeshInstance, _rootAnimationOffset), (size_t)0x100);
    CHECK_EQ(offsetof(Graphics::MeshInstance, _color),               (size_t)0x10c);
    // A WORD store, which is what fixes sizeof at 0x120 rather than 0x11c.
    CHECK_EQ(offsetof(Graphics::MeshInstance, _sortBias),            (size_t)0x11c);
    CHECK_EQ(sizeof(Graphics::MeshInstance), sizeof(RtGraphics::MeshInstance));

    CHECK_EQ(offsetof(Graphics::EdgeMeshInstance, _followed),        (size_t)0xc8);
    CHECK_EQ(sizeof(Graphics::EdgeMeshInstance), sizeof(RtGraphics::EdgeMeshInstance));

    CHECK_EQ(offsetof(Graphics::ParticleInstance, _boundsPtr),       (size_t)0xc8);
    CHECK_EQ(offsetof(Graphics::ParticleInstance, _index),           (size_t)0xd0);
    CHECK_EQ(sizeof(Graphics::ParticleInstance), sizeof(RtGraphics::ParticleInstance));
    CHECK_EQ(offsetof(Graphics::ParticleSystemInstance, _f_d8),      (size_t)0xd8);
}

UNIT_TEST(graphics_light_field_offsets_authored_from_retail) {
    // These three layouts did not exist anywhere in the tree before block E
    // (B35_RULES §13 item 3: the SIZES were measured, the FIELDS were not), so
    // this is the only test that pins them.

    // AmbientLight: `??0AmbientLight` zeroes two qwords at +0xc0/+0xc8 (the
    // Vector4) and stores 0x41200000 == 10.0f at +0xd0; `?Set@AmbientLight@`'s
    // mangled name is `(const Math::Vector4&, float)`, which fixes the types.
    CHECK_EQ(offsetof(Graphics::AmbientLight, _colour),    (size_t)0xc0);
    CHECK_EQ(offsetof(Graphics::AmbientLight, _intensity), (size_t)0xd0);
    CHECK_EQ(sizeof(Math::Vector4),                        (size_t)0x10);

    // SpotLight: the ctor zeroes exactly TWELVE qwords from +0xc0 to +0x118 —
    // the 0x60 bytes of a Frustum's six Vector4 planes — and then stores the
    // BYTE 1 at +0x120, which is `Frustum::isOrthographic`. Both ends of that
    // fit are checked, because it is the whole of the identification.
    CHECK_EQ(offsetof(Graphics::SpotLight, _frustum), (size_t)0xc0);
    CHECK_EQ(sizeof(::Frustum),                       (size_t)0x64);
    CHECK_EQ(offsetof(Graphics::SpotLight, _frustum) + offsetof(::Frustum, isOrthographic),
             (size_t)0x120);
    CHECK_EQ(offsetof(Graphics::SpotLight, _near),         (size_t)0x128);
    CHECK_EQ(offsetof(Graphics::SpotLight, _far),          (size_t)0x12c);
    CHECK_EQ(offsetof(Graphics::SpotLight, _spotScalarA),  (size_t)0x130);
    CHECK_EQ(offsetof(Graphics::SpotLight, _spotScalarB),  (size_t)0x134);
    CHECK_EQ(offsetof(Graphics::SpotLight, _colour),       (size_t)0x138);
    CHECK_EQ(offsetof(Graphics::SpotLight, _shadowMap),    (size_t)0x148);
    CHECK_EQ(offsetof(Graphics::SpotLight, _shadowDepth),  (size_t)0x150);
    CHECK_EQ(offsetof(Graphics::SpotLight, _shadowTarget), (size_t)0x158);
    CHECK_EQ(offsetof(Graphics::SpotLight, _boundImage),   (size_t)0x160);

    // SunLight: the eleven scalars `?Set@SunLight@` copies, the +0xEC hole no
    // body writes, and the three owned pointers.
    CHECK_EQ(offsetof(Graphics::SunLight, _dirZ),         (size_t)0xC8);
    CHECK_EQ(offsetof(Graphics::SunLight, _sunR),         (size_t)0xCC);
    CHECK_EQ(offsetof(Graphics::SunLight, _ambientR),     (size_t)0xDC);
    CHECK_EQ(offsetof(Graphics::SunLight, _ambientA),     (size_t)0xE8);
    CHECK_EQ(offsetof(Graphics::SunLight, _holdEC),       (size_t)0xEC);
    CHECK_EQ(offsetof(Graphics::SunLight, _colourTarget), (size_t)0xF0);
    CHECK_EQ(offsetof(Graphics::SunLight, _depthTarget),  (size_t)0xF8);
    CHECK_EQ(offsetof(Graphics::SunLight, _renderBuffer), (size_t)0x100);
    CHECK_EQ(sizeof(Graphics::SunLight), sizeof(RtGraphics::SunLight));
}

// ===========================================================================
// PART B / C / D — the probe hierarchy, declared in RETAIL SLOT ORDER.
//
// It mirrors the real classes' slot lists rather than deriving from them,
// because the real bodies live in `runtime/src/graphics/` and stand on the port
// registry and the allocator: none of those translation units can link into a
// host test (the same reason `external_class_doors.cpp` is kept out of
// `tests/unit/sources.mk`). What is under test is the SHAPE — that a class
// declared this way lays its slots out at the retail byte offsets — and the
// shape is a property of the declaration, not of the bodies.
//
// The declaration order below IS `B35_SLOTS.md`'s order for
// `??_7Geometry@Graphics@@6B@` (0x180154e60) and `??_7Light@Graphics@@6B@`
// (0x180158d30). Byte offset = slot index * 8.
// ===========================================================================

class ProbeGeometry {
public:
    // [0] +0x00  0x180015e90 — the vector deleting destructor, declared as an
    //            ordinary virtual (ENDGAME §B.1 decision 2: NO virtual dtor).
    virtual ProbeGeometry* DeletingDtor(unsigned flags)  { Note(this, 0, flags); return this; }
    // [1] +0x08  _purecall in Geometry itself
    virtual void Update(long long timeTicks)             { Note(this, 1, timeTicks); }
    // [2] +0x10  _purecall — the SLOT-2 `Draw` overload (see B5-17)
    virtual void Draw(int a) const                       { Note(this, 2, a); }
    // [3] +0x18  _purecall
    virtual void AddDrawItems(int a) const               { Note(this, 3, a); }
    // [4] +0x20  0x1800a4020  UpdateAttached
    virtual void UpdateAttached()                        { Note(this, 4); }
    // [5] +0x28  0x1800a43c0  GetSubTransform -> Math::Transform, hidden slot
    //            spelled out in RETAIL ORDER (`this`, then the caller's
    //            storage), returned again as the result. See PART C.
    virtual Math::Transform* GetSubTransform(Math::Transform* ret, int node) const {
        Note(this, 5, node);
        ret->translation = Math::Vector3(1.0f, 2.0f, 3.0f);
        return ret;
    }
    // [6] +0x30  0x1800154a0  GetInstanceTransform -> Math::Matrix, ditto
    virtual Math::Matrix* GetInstanceTransform(Math::Matrix* ret, int node, float t) const {
        Note(this, 6, node);
        for (int i = 0; i < 16; ++i) ret->m[i] = static_cast<float>(i) + t;
        return ret;
    }
    // [7] +0x38  0x180015590  SetSharedInstanceData
    virtual int  SetSharedInstanceData(void* ctx, int a, float t) const {
        (void)ctx; (void)t; Note(this, 7, a); return 0;
    }
    // [8] +0x40  0x1800a4420  Clean
    virtual bool Clean()                                 { Note(this, 8); return true; }

protected:
    ~ProbeGeometry() = default;   // NOT virtual — decision 2
};

// Graphics::MeshInstance's own table: it overrides 1, 2, 3, 5, 6, 7 and
// INHERITS 4 and 8 (`??_7MeshInstance@Graphics@@6B@`, 0x180154dc0).
class ProbeMeshInstance final : public ProbeGeometry {
public:
    void Update(long long t) override            { Note(this, 101, t); }
    void Draw(int a) const override              { Note(this, 102, a); }
    void AddDrawItems(int a) const override      { Note(this, 103, a); }
    Math::Transform* GetSubTransform(Math::Transform* ret, int node) const override {
        Note(this, 105, node);
        ret->translation = Math::Vector3(10.0f, 20.0f, 30.0f);
        return ret;
    }
    Math::Matrix* GetInstanceTransform(Math::Matrix* ret, int node, float f) const override {
        Note(this, 106, node);
        for (int i = 0; i < 16; ++i) ret->m[i] = 100.0f + static_cast<float>(i) + f;
        return ret;
    }
    int SetSharedInstanceData(void* ctx, int a, float t) const override {
        (void)ctx; (void)t; Note(this, 107, a); return 7;
    }
    ~ProbeMeshInstance() = default;
};

// Graphics::EdgeMeshInstance: overrides 1, 2, 3 and ONLY slot 6 of the two
// retslot slots — slot 5 stays Geometry's (`B35_RULES` §2.3 item 6).
class ProbeEdgeMeshInstance final : public ProbeGeometry {
public:
    void Update(long long t) override       { Note(this, 201, t); }
    void Draw(int a) const override         { Note(this, 202, a); }
    void AddDrawItems(int a) const override { Note(this, 203, a); }
    Math::Matrix* GetInstanceTransform(Math::Matrix* ret, int node, float f) const override {
        Note(this, 206, node);
        for (int i = 0; i < 16; ++i) ret->m[i] = 200.0f + static_cast<float>(i) + f;
        return ret;
    }
    ~ProbeEdgeMeshInstance() = default;
};

// Graphics::Light — adds slots 9 and 10 and NO fields. RULE B5-17: slot 10's
// retail name is also `Draw`; it is spelled `DrawLight`.
class ProbeLight : public ProbeGeometry {
public:
    // [9]  +0x48  DrawShadowMap
    virtual void DrawShadowMap(int a, int b) const        { Note(this, 9, (long long)a * 1000 + b); }
    // [10] +0x50  DrawLight  (retail `Draw`, the four-int overload)
    virtual void DrawLight(int a, int b, int c, int d) const {
        (void)b; (void)c; Note(this, 10, (long long)a * 1000 + d);
    }
protected:
    ~ProbeLight() = default;
};

class ProbeSunLight final : public ProbeLight {
public:
    void Update(long long t) override       { Note(this, 301, t); }
    void Draw(int a) const override          { Note(this, 302, a); }
    void AddDrawItems(int a) const override  { Note(this, 303, a); }
    void DrawShadowMap(int a, int b) const override { Note(this, 309, (long long)a * 1000 + b); }
    void DrawLight(int a, int b, int c, int d) const override {
        (void)b; (void)c; Note(this, 310, (long long)a * 1000 + d);
    }
    ~ProbeSunLight() = default;
};

// The slot byte offsets, as the tree spells them.
enum : int {
    kSlot0_DeletingDtor          = 0x00,
    kSlot1_Update                = 0x08,
    kSlot2_Draw                  = 0x10,
    kSlot3_AddDrawItems          = 0x18,
    kSlot4_UpdateAttached        = 0x20,
    kSlot5_GetSubTransform       = 0x28,
    kSlot6_GetInstanceTransform  = 0x30,
    kSlot7_SetSharedInstanceData = 0x38,
    kSlot8_Clean                 = 0x40,
    kSlot9_DrawShadowMap         = 0x48,
    kSlot10_DrawLight            = 0x50,
};

UNIT_TEST(graphics_geometry_slot_order_is_retail_order) {
    ProbeMeshInstance mi;
    ProbeGeometry* g = &mi;

    // Direction A: a raw `(*(void***)p)[N]` caller, exactly as 118 call sites
    // in the tree spell it, must land on the override with `this` UNADJUSTED.
    Reset();
    SlotAt<void (*)(void*, unsigned)>(g, kSlot0_DeletingDtor)(g, 3u);
    CHECK_EQ(g_lastMark, 0);            // MeshInstance does not override slot 0
    CHECK_EQ(g_lastSelf, (const void*)&mi);
    CHECK_EQ(g_lastArg, 3LL);

    Reset();
    SlotAt<void (*)(void*, long long)>(g, kSlot1_Update)(g, 42LL);
    CHECK_EQ(g_lastMark, 101);
    CHECK_EQ(g_lastSelf, (const void*)&mi);

    Reset();
    SlotAt<void (*)(const void*, int)>(g, kSlot2_Draw)(g, 5);
    CHECK_EQ(g_lastMark, 102);

    Reset();
    SlotAt<void (*)(const void*, int)>(g, kSlot3_AddDrawItems)(g, 6);
    CHECK_EQ(g_lastMark, 103);

    // Slots 4 and 8 are INHERITED: MeshInstance's table carries Geometry's own
    // body at both indices (0x1800a4020 and 0x1800a4420 in retail).
    Reset();
    SlotAt<void (*)(void*)>(g, kSlot4_UpdateAttached)(g);
    CHECK_EQ(g_lastMark, 4);

    Reset();
    SlotAt<bool (*)(void*)>(g, kSlot8_Clean)(g);
    CHECK_EQ(g_lastMark, 8);

    Reset();
    SlotAt<int (*)(const void*, void*, int, float)>(g, kSlot7_SetSharedInstanceData)(
        g, nullptr, 9, 0.5f);
    CHECK_EQ(g_lastMark, 107);
}

UNIT_TEST(graphics_edgemeshinstance_overrides_only_slot6_of_the_two_retslots) {
    ProbeEdgeMeshInstance emi;
    ProbeGeometry* g = &emi;

    // Slot 5 stays Geometry's own body …
    Reset();
    {
        Math::Transform out;
        using Fn = Math::Transform* (*)(const void*, Math::Transform*, int);
        Math::Transform* got = SlotAt<Fn>(g, kSlot5_GetSubTransform)(g, &out, 11);
        CHECK_EQ(got, &out);
        CHECK_EQ(g_lastMark, 5);            // Geometry's, NOT an override
    }
    // … while slot 6 is its own.
    Reset();
    {
        Math::Matrix out;
        using Fn = Math::Matrix* (*)(const void*, Math::Matrix*, int, float);
        Math::Matrix* got = SlotAt<Fn>(g, kSlot6_GetInstanceTransform)(g, &out, 12, 0.25f);
        CHECK_EQ(got, &out);
        CHECK_EQ(g_lastMark, 206);
    }
}

UNIT_TEST(graphics_light_branch_adds_slots_9_and_10) {
    ProbeSunLight sun;
    ProbeGeometry* g = &sun;

    // The first nine indices still mean what Geometry says they mean …
    Reset();
    SlotAt<void (*)(const void*, int)>(g, kSlot3_AddDrawItems)(g, 1);
    CHECK_EQ(g_lastMark, 303);

    // … and the two new ones sit at 0x48 and 0x50, in that order.
    Reset();
    SlotAt<void (*)(const void*, int, int)>(g, kSlot9_DrawShadowMap)(g, 4, 5);
    CHECK_EQ(g_lastMark, 309);
    CHECK_EQ(g_lastArg, 4005LL);

    Reset();
    SlotAt<void (*)(const void*, int, int, int, int)>(g, kSlot10_DrawLight)(g, 7, 0, 0, 8);
    CHECK_EQ(g_lastMark, 310);
    CHECK_EQ(g_lastArg, 7008LL);
}

UNIT_TEST(graphics_the_two_draw_overloads_stay_eight_slots_apart) {
    // RULE B5-17. Retail calls slot 2 and slot 10 both `Draw`; the conversion
    // renames slot 10 to `DrawLight`. This checks that the rename is a SPELLING
    // and not a reordering — the two bodies must still answer at 0x10 and 0x50
    // respectively, and a call through either must not reach the other.
    ProbeSunLight sun;
    ProbeGeometry* g = &sun;

    Reset();
    SlotAt<void (*)(const void*, int)>(g, kSlot2_Draw)(g, 77);
    CHECK_EQ(g_lastMark, 302);          // the slot-2 overload
    CHECK_EQ(g_lastArg, 77LL);

    Reset();
    SlotAt<void (*)(const void*, int, int, int, int)>(g, kSlot10_DrawLight)(g, 1, 0, 0, 2);
    CHECK_EQ(g_lastMark, 310);          // the slot-10 overload
    CHECK_EQ(g_lastArg, 1002LL);

    // And through the C++ names, which is what a converted caller writes.
    Reset();
    sun.Draw(88);
    CHECK_EQ(g_lastMark, 302);
    Reset();
    sun.DrawLight(3, 0, 0, 4);
    CHECK_EQ(g_lastMark, 310);
    CHECK_EQ(g_lastArg, 3004LL);
}

// ===========================================================================
// PART C — RULE B5-12: slots 5 and 6 carry a HIDDEN RETURN SLOT,
//          and GCC's order for a C++ by-value return is NOT retail's
// ===========================================================================

// A throwaway class, nothing to do with Graphics, whose only job is to show
// WHICH ARGUMENT the hidden return pointer is when GCC lowers a by-value class
// return from a virtual member. 28 bytes, so both ABIs must return it in memory.
struct AbiBig { float a[7]; };

struct AbiProbe {
    int tag;
    // Declared the way RULE B5-12 as published asks for: a plain C++ by-value
    // return. Slot 0 of this class's table.
    virtual AbiBig Get(int n) const {
        AbiBig b;
        for (int i = 0; i < 7; ++i) b.a[i] = static_cast<float>(tag * 100 + n + i);
        return b;
    }
    virtual void Other() {}
};

UNIT_TEST(graphics_gcc_sret_precedes_this_unlike_msvc) {
    // RETAIL / MSVC order: `this` in RCX, the caller's storage in RDX.
    using MsOrder  = AbiBig* (*)(const void* self, AbiBig* ret, int n);
    // GCC's actual order for the MS ABI: the storage FIRST, then `this`.
    using GccOrder = AbiBig* (*)(AbiBig* ret, const void* self, int n);

    // Called in MSVC order, GCC's body writes the 28 bytes through whatever it
    // was handed as the return slot — which is `this` — and leaves the caller's
    // buffer untouched. That is a WILD WRITE over a live object, the same family
    // as reference_hidden_retslot_wild_write.md with the arguments swapped.
    {
        AbiProbe p; p.tag = 3;
        void* slot0 = (*reinterpret_cast<void***>(&p))[0];
        AbiBig out; for (int i = 0; i < 7; ++i) out.a[i] = -1.0f;
        reinterpret_cast<MsOrder>(slot0)(static_cast<const void*>(&p), &out, 5);
        CHECK_EQ(out.a[0], -1.0f);        // the caller's buffer was NOT written
        CHECK(p.tag != 3);                // and the object WAS scribbled on
    }
    // Called in GCC's order it behaves: the caller's buffer is filled, returned
    // in RAX, and the object is intact.
    {
        AbiProbe p; p.tag = 4;
        void* slot0 = (*reinterpret_cast<void***>(&p))[0];
        AbiBig out; for (int i = 0; i < 7; ++i) out.a[i] = -1.0f;
        AbiBig* got = reinterpret_cast<GccOrder>(slot0)(&out, static_cast<const void*>(&p), 5);
        CHECK_EQ(got, &out);
        CHECK_EQ(out.a[0], 405.0f);
        CHECK_EQ(out.a[6], 411.0f);
        CHECK_EQ(p.tag, 4);
    }
    // THEREFORE: a vtable slot whose retail callers spell `(this, ret, …)` must
    // not be declared as a C++ by-value return. The real `Geometry` tree spells
    // slots 5 and 6 with the out-parameter explicit, in retail order, which the
    // next test exercises.
}

UNIT_TEST(graphics_slot5_and_slot6_carry_the_hidden_return_slot) {
    ProbeMeshInstance mi;
    ProbeGeometry* g = &mi;

    // Slot 5 -> Math::Transform (0x1c bytes). The caller's storage arrives as
    // the SECOND argument and the callee must hand it back as its RESULT. A
    // `void`-declared door is exactly the 2026-09-13 perf-only crash: at -O0
    // RAX happened to still hold the slot, at -O2 it held garbage.
    CHECK_EQ(sizeof(Math::Transform), (size_t)0x1c);
    Reset();
    {
        Math::Transform out;
        out.translation = Math::Vector3(-1.0f, -1.0f, -1.0f);
        using Fn = Math::Transform* (*)(const void*, Math::Transform*, int);
        Math::Transform* got = SlotAt<Fn>(g, kSlot5_GetSubTransform)(g, &out, 4);
        CHECK_EQ(g_lastMark, 105);
        CHECK_EQ(got, &out);                       // RAX == the caller's buffer
        CHECK_EQ(out.translation.x, 10.0f);        // and it holds the value
        CHECK_EQ(out.translation.y, 20.0f);
        CHECK_EQ(out.translation.z, 30.0f);
        CHECK_EQ(got->translation.z, 30.0f);       // reachable through the result
    }

    // Slot 6 -> Math::Matrix (0x40 bytes). Same rule, wider value.
    CHECK_EQ(sizeof(Math::Matrix), (size_t)0x40);
    Reset();
    {
        Math::Matrix out;
        using Fn = Math::Matrix* (*)(const void*, Math::Matrix*, int, float);
        Math::Matrix* got = SlotAt<Fn>(g, kSlot6_GetInstanceTransform)(g, &out, 5, 0.5f);
        CHECK_EQ(g_lastMark, 106);
        CHECK_EQ(got, &out);
        CHECK_EQ(out.m[0], 100.5f);
        CHECK_EQ(out.m[15], 115.5f);
        CHECK_EQ(got->m[15], 115.5f);
    }

    // The ordinary C++ virtual call has to agree with the hand-spelled one,
    // because a converted caller writes the C++ form while the surviving mint
    // rows and `fn_*` typedefs keep the hand-spelled one. It agrees BECAUSE the
    // declaration carries the out-parameter itself: there is no by-value return
    // for GCC to lower differently.
    Reset();
    {
        Math::Transform viaCpp;
        Math::Transform* got = g->GetSubTransform(&viaCpp, 4);
        CHECK_EQ(g_lastMark, 105);
        CHECK_EQ(got, &viaCpp);
        CHECK_EQ(viaCpp.translation.x, 10.0f);
    }
    Reset();
    {
        Math::Matrix viaCpp;
        Math::Matrix* got = g->GetInstanceTransform(&viaCpp, 5, 0.5f);
        CHECK_EQ(g_lastMark, 106);
        CHECK_EQ(got, &viaCpp);
        CHECK_EQ(viaCpp.m[0], 100.5f);
    }

    // And the REAL classes' two slots have that signature, not a by-value one —
    // pinned as a type, so a later block cannot "tidy" them back without this
    // test failing to compile.
    static_assert(
        std::is_same<
            decltype(std::declval<const Graphics::Geometry&>().GetSubTransform(
                         (Math::Transform*)nullptr, 0)),
            Math::Transform*>::value,
        "Geometry::GetSubTransform must keep the explicit retail-order return slot");
    static_assert(
        std::is_same<
            decltype(std::declval<const Graphics::Geometry&>().GetInstanceTransform(
                         (Math::Matrix*)nullptr, 0, 0.0f)),
            Math::Matrix*>::value,
        "Geometry::GetInstanceTransform must keep the explicit retail-order return slot");
}

// ===========================================================================
// PART D — the QUALIFIED delegation call (B34_BRIEFS §A.1)
//
// Direction B: an object whose vptr was stamped with a hand-built `void*[]` of
// FREE FUNCTIONS — which is what every object still wearing a row out of
// `standalone/rt_data_mint_tables.inc` looks like. A plain virtual call must
// dispatch into that array; a QUALIFIED call must not. That difference is why a
// door delegates with `reinterpret_cast<Ns::C*>(self)->Ns::C::Method(args)`: the
// minted slots resolve back to the doors themselves, so an unqualified call
// inside a door is infinite recursion.
// ===========================================================================

void MintedUpdate(void* self, long long t)   { Note(self, 904, t); }
void MintedDraw(const void* self, int a)     { Note(self, 902, a); }

UNIT_TEST(graphics_qualified_call_suppresses_minted_dispatch) {
    ProbeMeshInstance mi;

    // Build a table whose slot 1 and slot 2 are free functions, and stamp it.
    static void* table[11] = { nullptr };
    void** const gccTable = *reinterpret_cast<void***>(&mi);
    for (int i = 0; i < 9; ++i) table[i] = gccTable[i];
    table[kSlot1_Update / 8] = reinterpret_cast<void*>(&MintedUpdate);
    table[kSlot2_Draw   / 8] = reinterpret_cast<void*>(&MintedDraw);
    *reinterpret_cast<void***>(&mi) = table;

    // An ordinary virtual call goes through the minted array …
    Reset();
    static_cast<ProbeGeometry*>(&mi)->Update(11LL);
    CHECK_EQ(g_lastMark, 904);
    Reset();
    static_cast<ProbeGeometry*>(&mi)->Draw(12);
    CHECK_EQ(g_lastMark, 902);

    // … while the QUALIFIED call reaches the C++ member directly and never
    // touches the table. This is the delegation shape every door uses.
    Reset();
    mi.ProbeMeshInstance::Update(13LL);
    CHECK_EQ(g_lastMark, 101);
    Reset();
    mi.ProbeMeshInstance::Draw(14);
    CHECK_EQ(g_lastMark, 102);

    // A qualified call to the BASE's member is also non-virtual: that is how a
    // derived door reaches the inherited body without re-entering its own slot
    // (RULE B5-3's `self->Video::XData::~XData()` is the same shape).
    Reset();
    mi.ProbeGeometry::Draw(15);
    CHECK_EQ(g_lastMark, 2);

    // Put GCC's table back so the object is destroyed as itself.
    *reinterpret_cast<void***>(&mi) = gccTable;
}

// ===========================================================================
// The two claims the conversion makes about the class shape itself.
// ===========================================================================

UNIT_TEST(graphics_geometry_tree_has_no_virtual_destructor) {
    // ENDGAME §B.1 decision 2. Slot 0 is `DeletingDtor(unsigned)`, an ordinary
    // virtual; deletion is `Object::Destroy(p)` / the minted row, never
    // `delete`. If a virtual destructor were ever added, GCC would insert TWO
    // slots (the complete and deleting variants) and every index below would
    // shift — which is the failure this pins, cheaply.
    CHECK(!std::has_virtual_destructor<Graphics::Geometry>::value);
    CHECK(!std::has_virtual_destructor<Graphics::MeshInstance>::value);
    CHECK(!std::has_virtual_destructor<Graphics::EdgeMeshInstance>::value);
    CHECK(!std::has_virtual_destructor<Graphics::ParticleInstance>::value);
    CHECK(!std::has_virtual_destructor<Graphics::ParticleSystemInstance>::value);
    CHECK(!std::has_virtual_destructor<Graphics::Light>::value);
    CHECK(!std::has_virtual_destructor<Graphics::AmbientLight>::value);
    CHECK(!std::has_virtual_destructor<Graphics::SpotLight>::value);
    CHECK(!std::has_virtual_destructor<Graphics::SunLight>::value);

    // The probe hierarchy makes the same promise, and it is the one whose
    // vtable the tests above actually read.
    CHECK(!std::has_virtual_destructor<ProbeGeometry>::value);
    CHECK(!std::has_virtual_destructor<ProbeLight>::value);
}

UNIT_TEST(graphics_geometry_tree_is_abstract_and_polymorphic) {
    // B34_BRIEFS §A.4, amended: abstract where nothing holds the class BY VALUE
    // as a C++ member. Nothing in the tree does — the four by-value mirrors of
    // B35_RULES §3 hold the `RtGraphics` PODs — and slot 0's body is a minted
    // row that §11 forbids retiring in step 5, so slot 0 stays `= 0` on every
    // one of the nine. If a later block makes one concrete it must also give
    // slot 0 a delegating body and a protected `= default` constructor, and this
    // test is where that decision has to be recorded.
    CHECK(std::is_abstract<Graphics::Geometry>::value);
    CHECK(!std::is_abstract<Graphics::MeshInstance>::value);   // concrete since B.3.8 RB (exe table copy 0x14015e240)
    CHECK(!std::is_abstract<Graphics::EdgeMeshInstance>::value);   // concrete since B.3.9 GFX (rb_graphics_abi_test b39_gfx_*)
    CHECK(std::is_abstract<Graphics::ParticleInstance>::value);
    CHECK(!std::is_abstract<Graphics::ParticleSystemInstance>::value);   // concrete since B.3.9 GFX (rb_graphics_abi_test b39_gfx_*)
    CHECK(std::is_abstract<Graphics::Light>::value);
    CHECK(!std::is_abstract<Graphics::AmbientLight>::value);   // concrete since B.3.9 GFX (rb_graphics_abi_test b39_gfx_*)
    CHECK(!std::is_abstract<Graphics::SpotLight>::value);      // concrete since B.3.9 GFX
    CHECK(!std::is_abstract<Graphics::SunLight>::value);      // concrete since B.3.8 RB (exe table copy 0x14015e290)

    CHECK(std::is_polymorphic<Graphics::Geometry>::value);
    CHECK(std::is_polymorphic<Graphics::SunLight>::value);

    // RULE B5-10: `Geometry`'s copy constructor is a REAL, REGISTERED, EXPORTED
    // body (0x180015b60), never `= default`. A `= default` would make the class
    // trivially copyable, which is exactly the bitwise copy of the nested
    // `Array<Geometry*>` heap pointer the rule forbids — so the negative here is
    // the test.
    CHECK(!std::is_trivially_copyable<Graphics::Geometry>::value);
    CHECK(!std::is_trivially_copy_constructible<Graphics::Geometry>::value);
}

UNIT_TEST(graphics_base_subobject_sits_at_offset_zero) {
    // Single inheritance with the vptr shared: `reinterpret_cast` between a
    // derived pointer and its base must be the identity, because every door in
    // the tree casts a `void*` straight to the class it thinks it has.
    ProbeSunLight sun;
    ProbeLight*    asLight = &sun;
    ProbeGeometry* asGeom  = &sun;
    CHECK_EQ((const void*)asLight, (const void*)&sun);
    CHECK_EQ((const void*)asGeom,  (const void*)&sun);

    ProbeMeshInstance mi;
    CHECK_EQ((const void*)static_cast<ProbeGeometry*>(&mi), (const void*)&mi);

    // And one vptr, not two: a second base would show up as a size jump.
    CHECK_EQ(sizeof(ProbeLight), sizeof(ProbeGeometry));
}

}  // namespace
