// ============================================================================
// tests/unit/graphics_material_abi_test.cpp — ENDGAME B.3 step 5, block G
//   the four `Graphics` material/mesh resource classes as real C++ classes
//
//   Graphics::Material      : Object::External,         abstract, 0x38,  9 slots
//   Graphics::MaterialInstance : Object::External,       abstract, 0x60,  9 slots
//   Graphics::GraphicsMesh  : Object::CompiledExternal,  abstract, 0x98,  9 slots
//   Graphics::EdgeMesh      : Object::External,          abstract, 0x180, 9 slots
//
// The sibling of `graphics_animation_abi_test.cpp` (block H1) and
// `graphics_geometry_abi_test.cpp` (block E), and it is built to the same
// contract, so only what is DIFFERENT here is spelled out:
//
// PART A — LAYOUT. Every size, alignment and field offset the four headers
//   claim, measured as well as `static_assert`ed, plus the cross-asserts against
//   the PODs that still describe the same bytes (`RtGraphics::MaterialInstance`,
//   `RtGraphics::EdgeMesh`, `RtGraphics::GraphicsMeshView`, `RtGraphics::MeshNode`).
//   It also pins the one structural fact the conversion rests on: each class's
//   own fields begin at exactly `sizeof(base)` — no tail-padding reuse, no
//   second vptr — and that `GraphicsMesh`'s base is `Object::CompiledExternal`
//   while the other three sit directly on `Object::External`. That split is the
//   exact mistake B35_RULES section 2.1 warns about for `Video::VideoProgram`,
//   and here it is asserted in both directions for all four.
//
// PART B — SLOT ORDER. Probe subclasses declared in retail slot order, whose
//   GCC vtable is read back by BYTE OFFSET and compared against the member each
//   index is supposed to name. This is the claim that licenses the conversion to
//   coexist with the MINTED tables in `standalone/rt_data_mint_tables.inc`: an
//   object wearing a hand-built `void*[]` at retail byte offsets and one wearing
//   GCC's table are interchangeable slot for slot, in BOTH directions.
//
//   *** SLOT 4 IS THE INTERESTING ONE, AND IT IS SHARED, NOT OVERRIDDEN. ***
//   All four classes INHERIT `?Compile@External@Object@@` (0x1800066e0) at slot
//   4 — B35_SLOTS.md marks it `inherited-from` on every one of the four — except
//   `EdgeMesh`, which has its OWN `?Compile@EdgeMesh@Graphics@@` (0x1800a21c0,
//   `MOV EAX,1 ; RET`). The probes check that asymmetry directly: three tables
//   must carry the SAME pointer at +0x20 as a bare `Object::External` subclass
//   does, and EdgeMesh's must carry a different one. Getting that backwards is
//   silent — both bodies return 1 — which is exactly why it is a test.
//
// PART C — RULE B5-16, `Construct` AT SLOT 8. `Object::External` and
//   `Object::CompiledExternal` are EIGHT-entry tables; `Construct` is introduced
//   per branch, by the Video/Graphics leaf, at slot 8. Each probe's table is
//   therefore checked to have NINE distinct entries with `Construct` at +0x40,
//   and `Object::External`'s own table to have no such slot.
//
// PART D — THE QUALIFIED CALL (B34_BRIEFS A.1), on the same probe shape the
//   other two block tests use: a door delegates with
//   `reinterpret_cast<Ns::C*>(self)->Ns::C::Method(args)` and the `Ns::C::` is
//   load-bearing, because a live object in this build wears a MINTED table whose
//   slots resolve back to those very doors.
//
// WHAT IS *NOT* PROVEN HERE. Nothing about MSVC and nothing about the ported
// bodies. `runtime/src/graphics/material.cpp`, `graphicsmesh_vtable.cpp`,
// `edgemesh_vtable.cpp` and `mesh.cpp` all stand on `RegisterRuntimeExport` /
// `RT_DATA_SLOT` / `ResolveAddr` and cannot link into a host test, so the
// classes' out-of-line key-function bodies are defined AS MARKERS at the bottom
// of this file — which is also what makes GCC emit their vtables in this
// translation unit so the probes can raw-call every slot. The serializers and
// the reflection records are verified by the real run.
//
// The two retail prologues quoted in `Material.h`'s banner — the finding that
// `ExternalPtr<T>` getters really ARE hidden-retslot, against block E's doubt —
// are a fact about the RETAIL DLL and cannot be tested here; what IS tested is
// that `Object::ExternalPtr<T>` is the eight trivially-copyable bytes the
// layouts depend on.
// ============================================================================
#include "unit.h"

#include "runtime/types/graphics/Material.h"

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <type_traits>

namespace {

// ---------------------------------------------------------------------------
// Observation channel, same shape as the two sibling block tests.
// ---------------------------------------------------------------------------
const void* g_lastSelf = nullptr;
int         g_lastMark = -1;

void Note(const void* self, int mark) { g_lastSelf = self; g_lastMark = mark; }
void Reset() { g_lastSelf = nullptr; g_lastMark = -1; }

// The one instruction every hand-inlined `(*(void***)p)[N]` caller compiles to,
// spelled by BYTE OFFSET because that is how the tree spells it.
template <typename Fn>
Fn SlotAt(void* obj, int byteOffset) {
    const uint8_t* vt = *reinterpret_cast<const uint8_t* const*>(obj);
    Fn f = nullptr;
    const void* raw = *reinterpret_cast<const void* const*>(vt + byteOffset);
    std::memcpy(&f, &raw, sizeof f);
    return f;
}
const void* const* VTableOf(void* obj) {
    return *reinterpret_cast<const void* const* const*>(obj);
}

// The nine retail byte offsets, shared by every Object::External leaf.
constexpr int kDeletingDtor   = 0x00;
constexpr int kTypeName       = 0x08;
constexpr int kType           = 0x10;
constexpr int kGetFileVersion = 0x18;
constexpr int kCompile        = 0x20;
constexpr int kSerialize      = 0x28;
constexpr int kReconstruct    = 0x30;
constexpr int kDestruct       = 0x38;
constexpr int kConstruct      = 0x40;   // introduced per branch — RULE B5-16

// ---------------------------------------------------------------------------
// A bare `Object::External` leaf, existing only so PART B has something to read
// slot 4 out of: whatever pointer THIS table carries at +0x20 is
// `?Compile@External@Object@@` (0x1800066e0), and the three inheriting classes
// must carry the very same one.
// ---------------------------------------------------------------------------
struct BareExternalProbe final : Object::External {
    Object::Base* DeletingDtor(unsigned) override { return this; }
    int32_t GetFileVersion() const override { return 0; }
    bool Serialize(void*) override { return true; }
    Object::External* Reconstruct() override { return this; }
    void Destruct() override {}
};

struct MaterialProbe final : Graphics::Material {
    Object::Base* DeletingDtor(unsigned) override {
        Note(this, kDeletingDtor / 8); return this;
    }
};

struct MaterialInstanceProbe final : Graphics::MaterialInstance {
    Object::Base* DeletingDtor(unsigned) override {
        Note(this, kDeletingDtor / 8); return this;
    }
};

// The eight slots whose signature is plain machine words. `Compile` takes a
// `const System::Filename&` (one pointer) and returns a four-byte `?AW4` enum in
// EAX with NO hidden return slot (B35_RULES section 2.5 excludes `?AW4`), so it
// raw-calls safely too — but it is NOT marked, because three of the four classes
// inherit `Object::External`'s body, which does not Note().
void CheckExternalFamilySlots(void* raw) {
    using DtorFn = void* (*)(void*, unsigned);
    using NameFn = const char* (*)(const void*);
    using TypeFn = uint32_t (*)(const void*);
    using VerFn  = int32_t (*)(const void*);
    using SerFn  = bool (*)(void*, void*);
    using PtrFn  = void* (*)(void*);
    using VoidFn = void (*)(void*);

    Reset(); SlotAt<DtorFn>(raw, kDeletingDtor)(raw, 0u);
    CHECK_EQ(g_lastMark, kDeletingDtor / 8);
    CHECK(g_lastSelf == raw);

    Reset(); SlotAt<NameFn>(raw, kTypeName)(raw);
    CHECK_EQ(g_lastMark, kTypeName / 8);

    Reset(); SlotAt<TypeFn>(raw, kType)(raw);
    CHECK_EQ(g_lastMark, kType / 8);

    Reset(); SlotAt<VerFn>(raw, kGetFileVersion)(raw);
    CHECK_EQ(g_lastMark, kGetFileVersion / 8);

    Reset(); SlotAt<SerFn>(raw, kSerialize)(raw, nullptr);
    CHECK_EQ(g_lastMark, kSerialize / 8);

    Reset(); SlotAt<PtrFn>(raw, kReconstruct)(raw);
    CHECK_EQ(g_lastMark, kReconstruct / 8);

    Reset(); SlotAt<VoidFn>(raw, kDestruct)(raw);
    CHECK_EQ(g_lastMark, kDestruct / 8);

    Reset(); SlotAt<PtrFn>(raw, kConstruct)(raw);
    CHECK_EQ(g_lastMark, kConstruct / 8);
}

// NINE distinct entries and no tenth that aliases one of them — the check that
// a hand-built minted row of nine and GCC's table describe the same shape.
void CheckNineDistinctSlots(void* raw) {
    const void* const* vt = VTableOf(raw);
    for (int i = 0; i < 9; ++i)
        for (int j = i + 1; j < 9; ++j)
            CHECK(vt[i] != vt[j]);
}

// ---------------------------------------------------------------------------
// PART B/C — slot order
// ---------------------------------------------------------------------------
UNIT_TEST(graphics_material_slot_order_is_retails) {
    MaterialProbe probe;
    void* raw = static_cast<void*>(&probe);

    CheckExternalFamilySlots(raw);
    CheckNineDistinctSlots(raw);

    // *** SLOT 4 IS INHERITED. *** B35_SLOTS.md:141 marks Material's [4] as
    // `0x1800066e0 ?Compile@External@Object@@ … inherited-from Object::External`,
    // so the entry at +0x20 must be byte-identical to a bare External leaf's.
    BareExternalProbe bare;
    CHECK(VTableOf(raw)[kCompile / 8] ==
          VTableOf(static_cast<void*>(&bare))[kCompile / 8]);

    // `this` is unadjusted at +0x00: single inheritance, no virtual bases.
    CHECK(static_cast<void*>(static_cast<Graphics::Material*>(&probe)) == raw);
    CHECK(static_cast<void*>(static_cast<Object::External*>(&probe)) == raw);
    CHECK(static_cast<void*>(static_cast<Object::Base*>(&probe)) == raw);
}

UNIT_TEST(graphics_materialinstance_slot_order_is_retails) {
    MaterialInstanceProbe probe;
    void* raw = static_cast<void*>(&probe);

    CheckExternalFamilySlots(raw);
    CheckNineDistinctSlots(raw);

    BareExternalProbe bare;
    CHECK(VTableOf(raw)[kCompile / 8] ==
          VTableOf(static_cast<void*>(&bare))[kCompile / 8]);

    CHECK(static_cast<void*>(static_cast<Object::External*>(&probe)) == raw);
}

// RULE B5-16 — `Construct` is NOT in `Object::External`'s own table. If it ever
// moves into the base, every leaf's slots 8.. shift and the minted nine-entry
// rows start calling the wrong body, so this is asserted rather than assumed.
UNIT_TEST(graphics_material_construct_is_the_leafs_own_slot8) {
    BareExternalProbe bare;
    MaterialProbe mat;
    MaterialInstanceProbe inst;

    // The leaf tables carry a distinct `Construct` at +0x40; the base leaf's
    // eighth entry (+0x38) is its last declared slot, `Destruct`.
    CHECK(VTableOf(static_cast<void*>(&mat))[kConstruct / 8] !=
          VTableOf(static_cast<void*>(&inst))[kConstruct / 8]);

    // And slots 0..7 of each leaf are its own or the base's, never Construct's.
    for (int i = 0; i < 8; ++i) {
        CHECK(VTableOf(static_cast<void*>(&mat))[i] !=
              VTableOf(static_cast<void*>(&mat))[kConstruct / 8]);
        CHECK(VTableOf(static_cast<void*>(&inst))[i] !=
              VTableOf(static_cast<void*>(&inst))[kConstruct / 8]);
    }
    (void)bare;
}

// ---------------------------------------------------------------------------
// PART D — the qualified call. A door reached through a MINTED table must
// delegate NON-virtually or it re-enters itself (B34_BRIEFS A.1).
// ---------------------------------------------------------------------------
int g_qualifiedDoorEntries = 0;

int32_t MintedGetFileVersionDoor(const void* self) {
    ++g_qualifiedDoorEntries;
    // THE QUALIFIED FORM. Without `Graphics::Material::` this is a virtual call
    // that lands back in this same function through the stamped table below.
    return reinterpret_cast<const Graphics::Material*>(self)      // B.3.5 bridge
               ->Graphics::Material::GetFileVersion();
}

UNIT_TEST(graphics_material_qualified_call_does_not_reenter_the_door) {
    MaterialProbe probe;
    void* raw = static_cast<void*>(&probe);

    // A hand-built nine-entry table, the shape `rt_data_mint_tables.inc` stamps,
    // with slot 3 pointing at the door.
    const void* minted[9] = {};
    const void* const* real = VTableOf(raw);
    for (int i = 0; i < 9; ++i) minted[i] = real[i];
    minted[kGetFileVersion / 8] = reinterpret_cast<const void*>(&MintedGetFileVersionDoor);

    const void* const* saved = real;
    *reinterpret_cast<const void* const**>(raw) = minted;

    g_qualifiedDoorEntries = 0;
    Reset();
    using VerFn = int32_t (*)(const void*);
    const int32_t v = SlotAt<VerFn>(raw, kGetFileVersion)(raw);

    // Entered ONCE — the qualified call went to the member, not back through
    // the minted slot — and the member's marker ran.
    CHECK_EQ(g_qualifiedDoorEntries, 1);
    CHECK_EQ(g_lastMark, kGetFileVersion / 8);
    CHECK_EQ(v, 5);                       // Material::GetFileVersion's literal

    *reinterpret_cast<const void* const**>(raw) = saved;
}

// ---------------------------------------------------------------------------
// PART A — layout
// ---------------------------------------------------------------------------
UNIT_TEST(graphics_material_layout) {
    CHECK_EQ((int)sizeof(Graphics::Material), 0x38);
    CHECK_EQ((int)alignof(Graphics::Material), 8);
    CHECK_EQ((int)offsetof(Graphics::Material, _type),   0x20);
    CHECK_EQ((int)offsetof(Graphics::Material, _passes), 0x28);

    // The derived fields begin exactly at the end of the base — no tail-padding
    // reuse, no second vptr.
    CHECK_EQ((int)offsetof(Graphics::Material, _type), (int)sizeof(Object::External));

    // `Object::External`, NOT `Object::CompiledExternal`
    // (?_ParentTypeName@Material@Graphics@@ @0x1800118a0 -> "Object::External").
    CHECK((std::is_base_of<Object::External, Graphics::Material>::value));
    CHECK((!std::is_base_of<Object::CompiledExternal, Graphics::Material>::value));

    // Abstract by slot 0, which is what keeps the deleting-dtor row alive
    // (B35_RULES section 11): `Graphics::Material m;` must not compile.
    CHECK(std::is_abstract<Graphics::Material>::value);
    CHECK(std::is_polymorphic<Graphics::Material>::value);

    // The pass array's element is eight bytes — the `MOV RAX,[RAX+RDX*8]` at
    // 0x1801027a0 inside ?IsPassValid@.
    CHECK_EQ((int)sizeof(Object::ExternalPtr<Video::VideoProgram>), 8);
    CHECK(std::is_trivially_copyable<Object::ExternalPtr<Video::VideoProgram> >::value);

    // A field write through the CLASS is visible at the bytes every
    // `reinterpret_cast` in material.cpp reads.
    MaterialProbe probe;
    probe._type = 3;
    probe._passes._size = 2;
    const uint8_t* bytes =
        reinterpret_cast<const uint8_t*>(static_cast<Graphics::Material*>(&probe));
    CHECK_EQ(*reinterpret_cast<const int32_t*>(bytes + 0x20), 3);
    CHECK_EQ(*reinterpret_cast<const int32_t*>(bytes + 0x30), 2);
}

UNIT_TEST(graphics_materialinstance_layout) {
    CHECK_EQ((int)sizeof(Graphics::MaterialInstance), 0x60);
    CHECK_EQ((int)alignof(Graphics::MaterialInstance), 8);
    CHECK_EQ((int)offsetof(Graphics::MaterialInstance, _shaderClass),        0x20);
    CHECK_EQ((int)offsetof(Graphics::MaterialInstance, _hasRenderTexture),   0x24);
    CHECK_EQ((int)offsetof(Graphics::MaterialInstance, _hasRenderedTexture), 0x25);
    CHECK_EQ((int)offsetof(Graphics::MaterialInstance, _material),           0x28);
    CHECK_EQ((int)offsetof(Graphics::MaterialInstance, _parameters),         0x30);
    CHECK_EQ((int)offsetof(Graphics::MaterialInstance, _constants),          0x40);
    CHECK_EQ((int)offsetof(Graphics::MaterialInstance, _textures),           0x50);

    CHECK_EQ((int)offsetof(Graphics::MaterialInstance, _shaderClass),
             (int)sizeof(Object::External));
    CHECK((std::is_base_of<Object::External, Graphics::MaterialInstance>::value));
    CHECK((!std::is_base_of<Object::CompiledExternal, Graphics::MaterialInstance>::value));
    CHECK(std::is_abstract<Graphics::MaterialInstance>::value);

    // The POD that still describes the same bytes, and has live users outside
    // block G's file set, so the two must not drift.
    CHECK_EQ((int)sizeof(Graphics::MaterialInstance),
             (int)sizeof(RtGraphics::MaterialInstance));
    CHECK_EQ((int)offsetof(Graphics::MaterialInstance, _material),
             (int)offsetof(RtGraphics::MaterialInstance, material));
    CHECK_EQ((int)offsetof(Graphics::MaterialInstance, _parameters),
             (int)offsetof(RtGraphics::MaterialInstance, parameters));
    CHECK_EQ((int)offsetof(Graphics::MaterialInstance, _constants),
             (int)offsetof(RtGraphics::MaterialInstance, constants));
    CHECK_EQ((int)offsetof(Graphics::MaterialInstance, _textures),
             (int)offsetof(RtGraphics::MaterialInstance, textures));

    // The four element types the arrays hold, unchanged by block G.
    CHECK_EQ((int)sizeof(RtGraphics::MaterialTexture),      0x10);
    CHECK_EQ((int)sizeof(RtGraphics::MaterialBinding),      0x08);
    CHECK_EQ((int)sizeof(RtGraphics::MaterialParameterSet), 0x20);
    CHECK_EQ((int)sizeof(RtGraphics::MaterialConstant),     0x18);

    // `RendererDrawItem::material` is this +0x28 handle BY VALUE (B35_RULES
    // section 4), so the handle has to be the eight trivially-copyable bytes an
    // array element bulk-copy moves.
    CHECK_EQ((int)sizeof(Object::ExternalPtr<Graphics::Material>), 8);
    CHECK(std::is_trivially_copyable<Object::ExternalPtr<Graphics::Material> >::value);

    MaterialInstanceProbe probe;
    probe._shaderClass        = 4;
    probe._hasRenderTexture   = 1;
    probe._hasRenderedTexture = 1;
    probe._textures._size     = 6;
    const uint8_t* bytes =
        reinterpret_cast<const uint8_t*>(static_cast<Graphics::MaterialInstance*>(&probe));
    CHECK_EQ(*reinterpret_cast<const uint32_t*>(bytes + 0x20), 4u);
    CHECK_EQ((int)bytes[0x24], 1);
    CHECK_EQ((int)bytes[0x25], 1);
    CHECK_EQ(*reinterpret_cast<const int32_t*>(bytes + 0x58), 6);
}

}  // namespace

// ############################################################################
// THE OUT-OF-LINE BODIES THE PROBES NEED AT LINK TIME.
//
// See the file banner: `runtime/src/graphics/material.cpp` holds the real ones
// and is NOT in `UNIT_TREE_SRCS`, so there is exactly one definition per binary.
// Every body here is a MARKER, deliberately not a reimplementation — this file
// tests slot order and layout, not the reflection records or the serializer.
// `GetFileVersion` returns retail's literal 5 / 4 because PART D reads it back.
// ############################################################################

namespace Graphics {

const char* Material::TypeName() const { Note(this, kTypeName / 8); return "Material"; }
uint32_t    Material::Type() const { Note(this, kType / 8); return 0u; }
int32_t     Material::GetFileVersion() const { Note(this, kGetFileVersion / 8); return 5; }
bool        Material::Serialize(void*) { Note(this, kSerialize / 8); return true; }
Object::External* Material::Reconstruct() { Note(this, kReconstruct / 8); return this; }
void        Material::Destruct() { Note(this, kDestruct / 8); }
Object::External* Material::Construct() { Note(this, kConstruct / 8); return this; }
Material::~Material() {}

const char* MaterialInstance::TypeName() const { Note(this, kTypeName / 8); return "MaterialInstance"; }
uint32_t    MaterialInstance::Type() const { Note(this, kType / 8); return 0u; }
int32_t     MaterialInstance::GetFileVersion() const { Note(this, kGetFileVersion / 8); return 4; }
bool        MaterialInstance::Serialize(void*) { Note(this, kSerialize / 8); return true; }
Object::External* MaterialInstance::Reconstruct() { Note(this, kReconstruct / 8); return this; }
void        MaterialInstance::Destruct() { Note(this, kDestruct / 8); }
Object::External* MaterialInstance::Construct() { Note(this, kConstruct / 8); return this; }
MaterialInstance::~MaterialInstance() {}

}  // namespace Graphics
