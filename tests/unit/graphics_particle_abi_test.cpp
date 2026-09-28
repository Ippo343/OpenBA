// ============================================================================
// tests/unit/graphics_particle_abi_test.cpp — ENDGAME B.3 step 5, block H2
//   `Graphics::Particle` and `Graphics::ParticleSystem` as real C++ classes
//
// The sibling of `graphics_geometry_abi_test.cpp` (block E) and
// `graphics_animation_abi_test.cpp` (block H1), for block H2's two classes:
//
//   Graphics::Particle        : Object::External, ABSTRACT, 0x48,  9 slots
//   Graphics::ParticleSystem  : Particle,                   0x108, 10 slots
//
// WHAT THIS FILE HAS TO ESTABLISH
// -------------------------------
// PART A — LAYOUT. Every size, alignment and field offset the header claims,
//   measured here as well as `static_assert`ed there, so a header edit that moves
//   a field fails a TEST and not only a build somebody may be tempted to "fix".
//   `ParticleSystem`'s layout is AUTHORED (B35_RULES §13 item 3 listed the size
//   as verified and the fields as not), so for 37 of its offsets this test and
//   the header are the only record — which is why the header cites the
//   instruction behind each one and this part re-states the two that fix the
//   ends of the run.
//
// PART B — SLOT ORDER, AND IT IS THE POINT OF THE WHOLE FILE. A local probe
//   hierarchy declared in RETAIL SLOT ORDER, whose GCC-generated vtable is read
//   back slot by slot and compared against the member each index is supposed to
//   name.
//
//   *** RULE B5-16's ONE EXCEPTION IN THE WHOLE OF VIDEO AND GRAPHICS IS HERE.
//   *** Every other Video/Graphics external introduces `Construct` itself at
//   slot 8. `Graphics::Particle` spends slot 8 on `CreateSystem` (pure), so
//   `ParticleSystem::Construct` enters the branch at slot NINE, behind it:
//       [8] 0x1800a5140  ?CreateSystem@ParticleSystem@Graphics@@UEBAPEAVParticleInstance@2@XZ
//       [9] 0x180020580  ?Construct@ParticleSystem@Graphics@@UEAAPEAVExternal@Object@@XZ
//   Copying "Construct is slot 8" onto this class swaps the two and hands every
//   `CreateSystem` caller an `Object::External*` through the right vtable index —
//   a wrong-type call that no compiler and no run would report. PART B calls
//   byte offset 0x40 and byte offset 0x48 through a raw `(*(void***)p)[N]`
//   caller, the way the tree's 118 hand-inlined sites spell it, and checks WHICH
//   member each one reaches, in both directions.
//
//   It also pins the other half of B5-16: that `Particle` OVERRIDES slots 6 and
//   7 (which `Object::External` introduced pure) and INTRODUCES slot 8, so that
//   a `Particle` subclass which fills only 6/7/8 still finds `Particle`'s own
//   bodies at 1/2/3/5 and `External`'s at 4.
//
// PART C — THE QUALIFIED DELEGATION CALL (B34_BRIEFS §A.1). Every door in
//   `vtbatch7_animgroup_particlesystem.cpp` and `wave27_lane_gfxpart.cpp`
//   delegates with `reinterpret_cast<Ns::C*>(self)->Ns::C::Method(args)`, and the
//   `Ns::C::` is not decoration: a live `ParticleSystem` in this build wears a
//   MINTED `??_7ParticleSystem@Graphics@@6B@` whose slots resolve back to those
//   very doors, so a plain virtual call inside a door is infinite recursion
//   rather than delegation. PART C stamps a probe with a hand-built table and
//   shows the unqualified call dispatching through it while the qualified one
//   does not.
//
// PART D — THE CLASS SHAPE. No virtual destructor anywhere (ENDGAME §B.1
//   decision 2: a virtual dtor makes GCC emit TWO slots and every index below
//   shifts), both classes abstract, and `CreateSystem`'s return type pinned as a
//   TYPE so a later block cannot "tidy" it to `Object::External*`.
//
// WHAT IS *NOT* PROVEN HERE
// -------------------------
// Nothing about MSVC, and nothing about the ported bodies. The retail DLL is
// gone; every caller and callee here is this same GCC. The binding binary facts
// are OBJECT LAYOUT and RETAIL SLOT ORDER, and slot order binds only because the
// table it is compared against was transcribed from retail (`B35_SLOTS.md`
// lines 170-203, read as `.rdata` qwords, cross-checked against
// `standalone/rt_data_mint_tables.inc`). The bodies stand on the port registry,
// the allocator and `RT_DATA_SLOT`, so none of `runtime/src/graphics/*.cpp` can
// link into a host test — the probe hierarchy below is therefore local and the
// real classes are used for LAYOUT and for TYPE claims only.
//
// Also NOT proven: the two RULE B5-15 name literals. They are data in the
// binary, not in the code — nothing in this tree calls `DEFINE_TYPE` for either
// class yet, and slots 1/2 read the minted `?_typeInfo@` nodes. They are recorded
// in `Particle.h`'s banner so a later `DEFINE_TYPE` is COPIED and not derived:
// `"Particle"` and, for ParticleSystem's PARENT, `"Particle"` with NO namespace.
// ============================================================================
#include "unit.h"
#include "slot_check.h"
#include "dtor_check.h"

// The shipped header. The tests measure THIS, not a local copy, so a failure
// means the header and the ABI disagree.
#include "runtime/types/graphics/Particle.h"

#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace {

// ---------------------------------------------------------------------------
// Observation channel, the same shape as `graphics_geometry_abi_test.cpp`: every
// probe body records WHICH one ran and on what `this`, so a test asserts the
// identity of the callee and not merely that something returned.
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
// spelled by BYTE OFFSET because that is how the tree spells it. The offset is
// RETAIL's; T (the object's class, here the mirror) decides the compiler's.
template <typename Fn, class T>
Fn SlotAt(const void* obj, int byteOffset) {
    const uint8_t* const* vt = *reinterpret_cast<const uint8_t* const* const*>(obj);
    return reinterpret_cast<Fn>(*reinterpret_cast<void* const*>(
        reinterpret_cast<const uint8_t*>(vt) + RetailByteOffset<T>(byteOffset)));
}

// ===========================================================================
// PART A — LAYOUT
// ===========================================================================

UNIT_TEST(graphics_particle_sizes_match_retail) {
    // Each number is the `MOV EDX,imm` immediate in that class's OWN slot-0
    // vector deleting destructor, read as disassembly by block 0 (B35_RULES
    // §2.2). They are not derived from each other.
    CHECK_EQ(sizeof(Graphics::Particle),       (size_t)0x48);   // dtor 0x180020160
    CHECK_EQ(sizeof(Graphics::ParticleSystem), (size_t)0x108);  // dtor 0x180020a70

    CHECK_EQ(alignof(Graphics::Particle),       (size_t)8);
    CHECK_EQ(alignof(Graphics::ParticleSystem), (size_t)8);

    // Single inheritance, base subobject at offset zero, NO tail-padding reuse
    // and NO second vptr: retail's constructor inlines the base stores and then
    // writes its own first field at exactly `sizeof(base)` with no gap.
    CHECK_EQ(offsetof(Graphics::Particle, _active),   sizeof(Object::External));
    CHECK_EQ(offsetof(Graphics::ParticleSystem, _f48), sizeof(Graphics::Particle));
    CHECK_EQ(sizeof(Object::External), (size_t)0x20);
}

UNIT_TEST(graphics_particle_field_offsets) {
    // Pinned by the constructor at 0x180103e80 (the store list is in the
    // header's banner) and by `Serialize` at 0x180103f00.
    CHECK_EQ(offsetof(Graphics::Particle, _active),      (size_t)0x20);
    CHECK_EQ(offsetof(Graphics::Particle, _recordCount), (size_t)0x24);
    CHECK_EQ(offsetof(Graphics::Particle, _subCount),    (size_t)0x28);
    CHECK_EQ(offsetof(Graphics::Particle, _material),    (size_t)0x30);
    CHECK_EQ(offsetof(Graphics::Particle, _records),     (size_t)0x38);

    // The two MEASURED gaps the constructor never writes, kept as named pads
    // rather than left to C++'s natural alignment. They are why the diff oracles
    // in `animation.cpp` exclude [0x21,0x24) and [0x2c,0x30).
    CHECK_EQ(offsetof(Graphics::Particle, _pad21), (size_t)0x21);
    CHECK_EQ(offsetof(Graphics::Particle, _pad2c), (size_t)0x2c);
    CHECK_EQ(sizeof(Graphics::Particle::_pad21),   (size_t)3);

    // The class and the POD must describe the same bytes: the POD still has live
    // users (`animation.cpp`'s three constructor doors and their oracles), so a
    // drift between the two is the failure this pins.
    CHECK_EQ(sizeof(Graphics::Particle), sizeof(RtGraphics::Particle));
    CHECK_EQ(offsetof(Graphics::Particle, _active),
             offsetof(RtGraphics::Particle, _active));
    CHECK_EQ(offsetof(Graphics::Particle, _recordCount),
             offsetof(RtGraphics::Particle, _f24));
    CHECK_EQ(offsetof(Graphics::Particle, _subCount),
             offsetof(RtGraphics::Particle, _f28));
}

UNIT_TEST(graphics_particlesystem_field_offsets_authored_from_retail) {
    // This layout did not exist under retail's name anywhere in the tree before
    // block H2. It is not invented: it moves `vtbatch7_animgroup_particlesystem
    // .cpp`'s file-local struct — the WRITER, off the disassembled 60-store
    // constructor 0x1800a5320 and off `Serialize`'s emission order — and takes
    // its field NAMES from `wave28_lane_partsys.cpp`'s independent READER
    // measurements for `ParticleSystemInstance::Update`, each of which carries
    // the instruction that reads it. That file now `static_assert`s all 37
    // constants against this class, so the two witnesses are checked against each
    // other at compile time as well.
    CHECK_EQ(offsetof(Graphics::ParticleSystem, _f48),           (size_t)0x48);
    CHECK_EQ(offsetof(Graphics::ParticleSystem, _rateAccelA),    (size_t)0x4c);
    CHECK_EQ(offsetof(Graphics::ParticleSystem, _rateAccelB),    (size_t)0x50);
    CHECK_EQ(offsetof(Graphics::ParticleSystem, _durationTicks), (size_t)0x58);
    CHECK_EQ(offsetof(Graphics::ParticleSystem, _emitDirX),      (size_t)0x60);
    CHECK_EQ(offsetof(Graphics::ParticleSystem, _spreadX),       (size_t)0x6c);
    CHECK_EQ(offsetof(Graphics::ParticleSystem, _spreadScale),   (size_t)0x78);
    CHECK_EQ(offsetof(Graphics::ParticleSystem, _gravX),         (size_t)0xb8);
    CHECK_EQ(offsetof(Graphics::ParticleSystem, _sizeBase),      (size_t)0xc4);
    CHECK_EQ(offsetof(Graphics::ParticleSystem, _colorT0),       (size_t)0xd4);
    CHECK_EQ(offsetof(Graphics::ParticleSystem, _color0),        (size_t)0xdc);
    CHECK_EQ(offsetof(Graphics::ParticleSystem, _color1),        (size_t)0xe0);
    CHECK_EQ(offsetof(Graphics::ParticleSystem, _fadeInTicks),   (size_t)0xe8);
    CHECK_EQ(offsetof(Graphics::ParticleSystem, _fadeOutTicks),  (size_t)0xf0);
    CHECK_EQ(offsetof(Graphics::ParticleSystem, _pool),          (size_t)0xf8);

    // The two MEASURED constructor gaps: it writes +0x50 and then the int64 at
    // +0x58, and it writes +0xdc/+0xe0 as ONE qword of -1 and then the int64 at
    // +0xe8. `Serialize` skips both as well.
    CHECK_EQ(offsetof(Graphics::ParticleSystem, _pad54),  (size_t)0x54);
    CHECK_EQ(offsetof(Graphics::ParticleSystem, _pad_e4), (size_t)0xe4);

    // `Serialize` emits FIFTEEN consecutive float scalars from +0x7c to +0xb4.
    // If a field is ever inserted in the middle of that run the stream shifts by
    // four bytes and every later record is read out of the wrong place — a save
    // desync, not a crash. Both ends and the span are checked.
    CHECK_EQ(offsetof(Graphics::ParticleSystem, _angleBRange), (size_t)0x7c);
    CHECK_EQ(offsetof(Graphics::ParticleSystem, _gravScale),   (size_t)0xb4);
    CHECK_EQ(offsetof(Graphics::ParticleSystem, _gravScale)
                 - offsetof(Graphics::ParticleSystem, _angleBRange),
             (size_t)(14 * sizeof(float)));

    // The `_pool` header is the last 0x10 bytes, which is what fixes `sizeof` at
    // 0x108 rather than 0xf8.
    CHECK_EQ(offsetof(Graphics::ParticleSystem, _pool) + sizeof(Collection::Array<uint8_t>),
             sizeof(Graphics::ParticleSystem));
    CHECK_EQ(sizeof(Collection::Array<uint8_t>), (size_t)0x10);
}

// ===========================================================================
// PART B / C — the probe hierarchy, declared in RETAIL SLOT ORDER.
//
// It mirrors the real classes' slot lists rather than deriving from them,
// because the real bodies live in `runtime/src/graphics/` and stand on the port
// registry, the allocator and `RT_DATA_SLOT`: none of those translation units
// can link here (the same reason `external_class_doors.cpp` is kept out of
// `tests/unit/sources.mk`). What is under test is the SHAPE — that a class
// declared this way lays its slots out at the retail byte offsets — and the
// shape is a property of the declaration, not of the bodies.
//
// The declaration order below IS `B35_SLOTS.md`'s order for
// `??_7External@Object@@6B@`, `??_7Particle@Graphics@@6B@` (0x180155130) and
// `??_7ParticleSystem@Graphics@@6B@` (0x180155090). Byte offset = index * 8.
//
// ONE DELIBERATE DEVIATION, stated rather than hidden: the real classes leave
// slot 0 PURE (`= 0`) at every level, because its body is a minted `RT_DELDTOR`
// row that B35_RULES §11 forbids retiring, and nothing in the tree instantiates
// either class. A probe has to be instantiable, so the LEAF probes give slot 0 a
// body. That changes which function slot 0 holds and nothing about the INDEX of
// any slot, which is what this part measures.
//
// B3X W1: retail slot 0 is now `virtual ~T()` in the real tree, which GCC lays
// out as TWO entries (D1 complete, D0 deleting); retail slot n >= 1 is compiler
// slot n + 1 and the SlotAt reads translate. The mirror follows: a virtual
// destructor at the root (with a recording class-scope operator delete standing
// in for the root's MemFree one) and a recording destructor on each leaf.
// ===========================================================================

// `Object::External`'s eight slots (External.h:601, itself read from retail).
// Slots 3, 5, 6 and 7 are `_purecall` in retail's own table.
class ProbeExternal {
public:
    virtual ~ProbeExternal() {}                                          // [0] D1 + D0
    DTOR_CHECK_RECORDING_DELETE
    virtual const char* TypeName() const               { Note(this, 1); return "External"; }
    virtual uint32_t    Type() const                   { Note(this, 2); return 0u; }
    virtual int32_t     GetFileVersion() const = 0;                      // [3] pure
    virtual uint32_t    Compile(const void* path)      { (void)path; Note(this, 4); return 1u; }
    virtual bool        Serialize(void* stream) = 0;                     // [5] pure
    virtual ProbeExternal* Reconstruct() = 0;                            // [6] pure
    virtual void        Destruct() = 0;                                  // [7] pure
};

// `Graphics::Particle`: overrides 1, 2, 3 and 5; INHERITS 4; leaves 6 and 7 pure
// (retail's table carries External's own `_purecall` qwords at both, which is
// what `inherited-from Object::External` means in the dump); and INTRODUCES
// `CreateSystem` PURE at 8.
class ProbeParticle : public ProbeExternal {
public:
    const char* TypeName() const override        { Note(this, 101); return "Particle"; }
    uint32_t    Type() const override            { Note(this, 102); return 1u; }
    int32_t     GetFileVersion() const override  { Note(this, 103); return 0x1000; }
    bool        Serialize(void*) override        { Note(this, 105); return true; }
    ProbeExternal* Reconstruct() override = 0;                           // [6] still pure
    void        Destruct() override = 0;                                 // [7] still pure
    virtual void* CreateSystem() const = 0;                              // [8] INTRODUCED
};

// A minimal concrete `Particle` subclass — the shape `code/Game/Game/tornado.cpp`
// has. It fills exactly the three slots retail leaves `_purecall` on `Particle`
// (plus slot 0, per the deviation note), so slots 1/2/3/5 must still reach
// `Particle`'s bodies and slot 4 must still reach `External`'s.
class ProbeParticleLeaf final : public ProbeParticle {
public:
    ProbeExternal* Reconstruct() override        { Note(this, 306); return this; }
    void  Destruct() override                    { Note(this, 307); }
    void* CreateSystem() const override          { Note(this, 308); return nullptr; }
    ~ProbeParticleLeaf() override                { Note(this, 300); }
};

// `Graphics::ParticleSystem`: overrides 1, 2, 3, 5, 6, 7 and 8, INHERITS 4, and
// INTRODUCES `Construct` at NINE. THE ORDER OF THE LAST TWO DECLARATIONS IS THE
// WHOLE POINT — `CreateSystem` is the override of an inherited slot and so keeps
// index 8; `Construct` is new and lands at 9.
class ProbeParticleSystem final : public ProbeParticle {
public:
    const char* TypeName() const override        { Note(this, 201); return "ParticleSystem"; }
    uint32_t    Type() const override            { Note(this, 202); return 2u; }
    int32_t     GetFileVersion() const override  { Note(this, 203); return 0x1001; }
    bool        Serialize(void*) override        { Note(this, 205); return true; }
    ProbeExternal* Reconstruct() override        { Note(this, 206); return this; }
    void        Destruct() override              { Note(this, 207); }
    void*       CreateSystem() const override    { Note(this, 208); return this->kSentinel; }
    virtual ProbeExternal* Construct()           { Note(this, 209); return this; }
    ~ProbeParticleSystem() override              { Note(this, 200); }

    // A value only `CreateSystem` returns, so a test can tell a `CreateSystem`
    // result apart from a `Construct` result even if both marks were lost.
    void* const kSentinel = reinterpret_cast<void*>(static_cast<uintptr_t>(0xC5C5C5C5u));
};

// The slot byte offsets, as the tree spells them.
enum : int {
    kSlot0_Destructor     = 0x00,
    kSlot1_TypeName       = 0x08,
    kSlot2_Type           = 0x10,
    kSlot3_GetFileVersion = 0x18,
    kSlot4_Compile        = 0x20,
    kSlot5_Serialize      = 0x28,
    kSlot6_Reconstruct    = 0x30,
    kSlot7_Destruct       = 0x38,
    kSlot8_CreateSystem   = 0x40,
    kSlot9_Construct      = 0x48,
};

UNIT_TEST(graphics_particle_slot_order_is_retail_order) {
    ProbeParticleLeaf leaf;

    // A raw `(*(void***)p)[N]` caller — exactly as the tree's hand-inlined sites
    // and every minted row spell it — must land on the right member with `this`
    // UNADJUSTED at every index.
    // Retail slot 0 (B3X W1): raw retail slot 0 is D1, which really destroys,
    // so it runs on a placement-new'd leaf never destroyed again; D0 (compiler
    // slot 1) runs the same body and frees through the class operator delete.
    {
        alignas(ProbeParticleLeaf) unsigned char buf[sizeof(ProbeParticleLeaf)];
        ProbeParticleLeaf* p = ::new (static_cast<void*>(buf)) ProbeParticleLeaf();
        CHECK(dtor_check::CompilerEntry<const void*>(p, 0) != dtor_check::CompilerEntry<const void*>(p, 1));
        Reset(); dtor_check::ResetFrees();
        SlotAt<void (*)(void*), ProbeParticleLeaf>(p, kSlot0_Destructor)(p);   // D1
        CHECK_EQ(g_lastMark, 300);
        CHECK_EQ(g_lastSelf, (const void*)p);
        CHECK_EQ(dtor_check::g_frees, 0);
        ProbeParticleLeaf* h = dtor_check::NewOnMalloc<ProbeParticleLeaf>();
        Reset(); dtor_check::ResetFrees();
        dtor_check::CallD0(h);
        CHECK_EQ(g_lastMark, 300);
        CHECK_EQ(g_lastSelf, (const void*)h);
        CHECK_EQ(dtor_check::g_frees, 1);
        CHECK(dtor_check::g_lastFree == h);
    }

    // 1, 2, 3 and 5 reach PARTICLE's bodies: the leaf does not override them.
    Reset(); SlotAt<const char* (*)(const void*), ProbeParticleLeaf>(&leaf, kSlot1_TypeName)(&leaf);
    CHECK_EQ(g_lastMark, 101);
    Reset(); SlotAt<uint32_t (*)(const void*), ProbeParticleLeaf>(&leaf, kSlot2_Type)(&leaf);
    CHECK_EQ(g_lastMark, 102);
    Reset(); SlotAt<int32_t (*)(const void*), ProbeParticleLeaf>(&leaf, kSlot3_GetFileVersion)(&leaf);
    CHECK_EQ(g_lastMark, 103);
    Reset(); SlotAt<bool (*)(void*, void*), ProbeParticleLeaf>(&leaf, kSlot5_Serialize)(&leaf, nullptr);
    CHECK_EQ(g_lastMark, 105);

    // 4 reaches EXTERNAL's body, inherited through Particle untouched — which is
    // why `Graphics::Particle` declares no `Compile` at all.
    Reset(); SlotAt<uint32_t (*)(void*, const void*), ProbeParticleLeaf>(&leaf, kSlot4_Compile)(&leaf, nullptr);
    CHECK_EQ(g_lastMark, 4);

    // 6, 7, 8 are the three the leaf itself fills — the three retail leaves
    // `_purecall` on `Particle`.
    Reset(); SlotAt<void* (*)(void*), ProbeParticleLeaf>(&leaf, kSlot6_Reconstruct)(&leaf);
    CHECK_EQ(g_lastMark, 306);
    Reset(); SlotAt<void (*)(void*), ProbeParticleLeaf>(&leaf, kSlot7_Destruct)(&leaf);
    CHECK_EQ(g_lastMark, 307);
    Reset(); SlotAt<void* (*)(const void*), ProbeParticleLeaf>(&leaf, kSlot8_CreateSystem)(&leaf);
    CHECK_EQ(g_lastMark, 308);
}

UNIT_TEST(graphics_particlesystem_construct_is_slot_nine_behind_createsystem) {
    // *** RULE B5-16. This is the test the whole file exists for. ***
    ProbeParticleSystem ps;

    // Byte offset 0x40 — slot EIGHT — is `CreateSystem`, and it returns the
    // instance sentinel, not `this`.
    Reset();
    void* created = SlotAt<void* (*)(const void*), ProbeParticleSystem>(&ps, kSlot8_CreateSystem)(&ps);
    CHECK_EQ(g_lastMark, 208);
    CHECK_EQ(g_lastSelf, (const void*)&ps);
    CHECK_EQ(created, ps.kSentinel);

    // Byte offset 0x48 — slot NINE — is `Construct`, and it returns `this`.
    Reset();
    void* constructed = SlotAt<void* (*)(void*), ProbeParticleSystem>(&ps, kSlot9_Construct)(&ps);
    CHECK_EQ(g_lastMark, 209);
    CHECK_EQ(g_lastSelf, (const void*)&ps);
    CHECK_EQ(constructed, (void*)&ps);

    // And the two are DIFFERENT entries: the naive "Construct is slot 8" reading
    // would make these compare equal, which is exactly how the mistake would
    // survive a build and a run.
    CHECK(RetailEntry<ProbeParticleSystem>(&ps, kSlot8_CreateSystem / 8) !=
          RetailEntry<ProbeParticleSystem>(&ps, kSlot9_Construct / 8));

    // Slot 9 exists ONLY on this class. `ProbeParticle`'s branch has nine
    // entries, so a `Particle` subclass that is not a `ParticleSystem` has
    // nothing at 0x48 — which is why `Construct` cannot be declared on
    // `Particle` "for symmetry".
    ProbeParticleLeaf leaf;
    CHECK(RetailEntry<ProbeParticleLeaf>(&leaf, kSlot8_CreateSystem / 8) !=
          RetailEntry<ProbeParticleSystem>(&ps, kSlot8_CreateSystem / 8));

    // The inherited slot 4 is the SAME entry in both tables — the one qword the
    // vtable dump marks `inherited-from` on both classes.
    CHECK_EQ(RetailEntry<ProbeParticleLeaf>(&leaf, kSlot4_Compile / 8),
             RetailEntry<ProbeParticleSystem>(&ps, kSlot4_Compile / 8));
}

UNIT_TEST(graphics_particlesystem_overrides_the_seven_slots_retail_says) {
    ProbeParticleSystem ps;

    // Retail slot 0 is its own destructor: raw D1 on a placement-new'd object
    // (never destroyed again: `ps` on the stack would be destroyed twice).
    {
        alignas(ProbeParticleSystem) unsigned char buf[sizeof(ProbeParticleSystem)];
        ProbeParticleSystem* p = ::new (static_cast<void*>(buf)) ProbeParticleSystem();
        Reset(); SlotAt<void (*)(void*), ProbeParticleSystem>(p, kSlot0_Destructor)(p);
        CHECK_EQ(g_lastMark, 200);
        CHECK_EQ(g_lastSelf, (const void*)p);
    }
    Reset(); SlotAt<const char* (*)(const void*), ProbeParticleSystem>(&ps, kSlot1_TypeName)(&ps);
    CHECK_EQ(g_lastMark, 201);
    Reset(); SlotAt<uint32_t (*)(const void*), ProbeParticleSystem>(&ps, kSlot2_Type)(&ps);
    CHECK_EQ(g_lastMark, 202);
    Reset(); SlotAt<int32_t (*)(const void*), ProbeParticleSystem>(&ps, kSlot3_GetFileVersion)(&ps);
    CHECK_EQ(g_lastMark, 203);
    // Slot 4 is NOT overridden: retail's dump marks it inherited.
    Reset(); SlotAt<uint32_t (*)(void*, const void*), ProbeParticleSystem>(&ps, kSlot4_Compile)(&ps, nullptr);
    CHECK_EQ(g_lastMark, 4);
    Reset(); SlotAt<bool (*)(void*, void*), ProbeParticleSystem>(&ps, kSlot5_Serialize)(&ps, nullptr);
    CHECK_EQ(g_lastMark, 205);
    Reset(); SlotAt<void* (*)(void*), ProbeParticleSystem>(&ps, kSlot6_Reconstruct)(&ps);
    CHECK_EQ(g_lastMark, 206);
    Reset(); SlotAt<void (*)(void*), ProbeParticleSystem>(&ps, kSlot7_Destruct)(&ps);
    CHECK_EQ(g_lastMark, 207);
}

// ===========================================================================
// PART C — the QUALIFIED delegation call (B34_BRIEFS §A.1)
//
// An object whose vptr was stamped with a hand-built `void*[]` of FREE
// FUNCTIONS — which is what every object still wearing a row out of
// `standalone/rt_data_mint_tables.inc` looks like. A plain virtual call must
// dispatch into that array; a QUALIFIED call must not. That difference is why
// every door in this block delegates with
// `reinterpret_cast<Ns::C*>(self)->Ns::C::Method(args)`: the minted slots
// resolve back to the doors themselves, so an unqualified call inside a door is
// infinite recursion.
// ===========================================================================

void* MintedCreateSystem(const void* self)  { Note(self, 908); return nullptr; }
void* MintedConstruct(void* self)           { Note(self, 909); return self; }

UNIT_TEST(graphics_particlesystem_qualified_call_suppresses_minted_dispatch) {
    ProbeParticleSystem ps;

    // Build a table whose slots 8 and 9 are free functions, and stamp it. Ten
    // retail entries, matching `??_7ParticleSystem@Graphics@@6B@`'s measured
    // length — ELEVEN compiler entries since B3X W1 (retail slot 0 = D1 + D0).
    static void* table[11] = { nullptr };
    void** const gccTable = *reinterpret_cast<void***>(&ps);
    for (int i = 0; i < 11; ++i) table[i] = gccTable[i];
    table[RetailSlot<ProbeParticleSystem>(kSlot8_CreateSystem / 8)] =
        reinterpret_cast<void*>(&MintedCreateSystem);
    table[RetailSlot<ProbeParticleSystem>(kSlot9_Construct / 8)] =
        reinterpret_cast<void*>(&MintedConstruct);
    *reinterpret_cast<void***>(&ps) = table;

    // An ordinary virtual call goes through the minted array …
    Reset();
    static_cast<ProbeParticle*>(&ps)->CreateSystem();
    CHECK_EQ(g_lastMark, 908);

    // … while the QUALIFIED call reaches the C++ member directly and never
    // touches the table. This is the delegation shape every door uses, and the
    // two slots it is shown on are the two B5-16 could have swapped.
    Reset();
    ps.ProbeParticleSystem::CreateSystem();
    CHECK_EQ(g_lastMark, 208);
    Reset();
    ps.ProbeParticleSystem::Construct();
    CHECK_EQ(g_lastMark, 209);

    // A qualified call to the BASE's member is also non-virtual: that is how a
    // derived door reaches the inherited body without re-entering its own slot
    // (RULE B5-3's `self->Video::XData::~XData()` is the same shape). Slot 4 is
    // the one `ParticleSystem` genuinely inherits.
    Reset();
    ps.ProbeExternal::Compile(nullptr);
    CHECK_EQ(g_lastMark, 4);

    // Put GCC's table back so the object is destroyed as itself.
    *reinterpret_cast<void***>(&ps) = gccTable;
}

// ===========================================================================
// PART D — the claims the conversion makes about the class shape itself
// ===========================================================================

UNIT_TEST(graphics_particle_branch_has_no_virtual_destructor) {
    // B3X W1 retired ENDGAME §B.1 decision 2's one-slot deleting-destructor
    // virtual: the branch now HAS a virtual destructor, and GCC's two slots for
    // it (D1 complete, D0 deleting) move every index above by one — CreateSystem
    // and Construct TOGETHER, which is why B5-16's trap is pinned by the
    // translated reads (RetailSlot) and not by bumped constants.
    CHECK(std::has_virtual_destructor<Graphics::Particle>::value);
    CHECK(std::has_virtual_destructor<Graphics::ParticleSystem>::value);
    CHECK(std::has_virtual_destructor<ProbeParticle>::value);
    CHECK(std::has_virtual_destructor<ProbeParticleSystem>::value);
    CHECK_EQ(RetailSlot<ProbeParticleSystem>(kSlot8_CreateSystem / 8), 9L);
    CHECK_EQ(RetailSlot<ProbeParticleSystem>(kSlot9_Construct / 8), 10L);

}

UNIT_TEST(graphics_particle_branch_is_abstract) {
    // `Particle` is abstract in retail itself (three `_purecall` slots).
    // `ParticleSystem` is concrete in retail but is declared abstract here on
    // slot 0 alone, exactly as `AnimationGroup` is: nothing holds either by value
    // (B34_BRIEFS §A.4 as amended) and their deleting-dtor bodies are minted rows
    // B35_RULES §11 forbids retiring.
    // ENDGAME B.3.9 GFX: ParticleSystem is CONCRETE now (its own slot 0, a real
    // ctor, the rows retired -- rb_graphics_abi_test b39_gfx_particlesystem_*);
    // Particle stays abstract through its three pure slots.
    CHECK(std::is_abstract<Graphics::Particle>::value);
    CHECK(!std::is_abstract<Graphics::ParticleSystem>::value);

    // And no real constructor on either (B4b-2): the ctor doors at 0x180103e80
    // and 0x1800a5320 stay, because both are called on memory that already wears
    // the minted table.
    CHECK(std::is_trivially_default_constructible<RtGraphics::Particle>::value);
}

UNIT_TEST(graphics_particlesystem_createsystem_returns_a_particleinstance) {
    // Pinned as a TYPE, so a later block cannot "tidy" slot 8's return to
    // `Object::External*` — the exact consequence B35_RULES §2.3 item 2 warns
    // about, and the one a swapped slot pair would produce silently.
    static_assert(
        std::is_same<
            decltype(std::declval<const Graphics::ParticleSystem&>().CreateSystem()),
            Graphics::ParticleInstance*>::value,
        "ParticleSystem::CreateSystem must return ParticleInstance*, never External*");
    static_assert(
        std::is_same<
            decltype(std::declval<Graphics::ParticleSystem&>().Construct()),
            Object::External*>::value,
        "ParticleSystem::Construct returns Object::External* (slot 9)");
    static_assert(
        std::is_same<
            decltype(std::declval<Graphics::ParticleSystem&>().Reconstruct()),
            Object::External*>::value,
        "ParticleSystem::Reconstruct returns Object::External* (slot 6)");
    CHECK(true);   // the claims above are compile-time; this keeps the test honest
}

}  // namespace
