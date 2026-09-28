// ============================================================================
// tests/unit/graphics_animation_abi_test.cpp
//   ENDGAME B.3.5 step 5, block H1 — the REAL Graphics animation classes'
//   vtable order and layout.
// ============================================================================
//
// WHAT IT PINS, AND WHY IT HAS TO EXIST
// -------------------------------------
// Nothing else in the build compiles a real `Graphics::AnimationEvent` or
// `Graphics::AnimationGroup` vtable. Every live object of either class wears a
// MINTED table out of `standalone/rt_data_mint_tables.inc`, and every registered
// door calls QUALIFIED, so a slot declared one line out of place in
// `runtime/types/graphics/Animation.h` or `AnimationGroup.h` would stay
// invisible right through B.3.5 and surface only when B.3 retires the minted
// rows — as an `AnimationGroup` that serializes through `Compile`, with no
// recent change to blame.
//
// The shape is B34_BRIEFS section B.7's: a CONCRETE probe subclass of each real
// class, every slot overridden with its own marker, then a raw slot call at
// every retail byte offset asserting the marker equals offset/8. Retail slot 0
// (the deleting destructor) is, since B3X W1, the compiler's destructor pair
// (D1 at 0, D0 at 1): both are raw-called too, on objects destroyed exactly
// once (CheckDtorPair).
//
// THE OUT-OF-LINE BODIES THE PROBES NEED AT LINK TIME ARE DEFINED AT THE BOTTOM
// OF THIS FILE, not in a stubs file, and they are NOT duplicates. GCC emits a
// class's vtable in the translation unit that defines its KEY FUNCTION (the
// first non-inline, non-pure virtual in declaration order), and that vtable
// references every slot — so pinning `Graphics::AnimationEvent`'s table here
// requires `TypeName`, `Type` and `~AnimationEvent` to be linkable here. Their
// real bodies live in `runtime/src/graphics/animation.cpp` and
// `vtbatch7_animgroup_particlesystem.cpp`, which stand on `RegisterRuntimeExport`
// / `RT_DATA_SLOT` / `ResolveAddr` and cannot link into `make unit` at all
// (the same reason `external_class_doors.cpp` is kept out of `sources.mk`).
// Neither of those files is in `UNIT_TREE_SRCS`, so there is exactly one
// definition in each binary. The stubs answer with markers, never with data:
// this file tests the ABI, not the reflection records.
//
// `Graphics::Animation`, `AnimationState` and `AnimationManager` are
// NON-POLYMORPHIC (no `??_7` export, no `_typeInfo`), so their parts are
// layout-only: `sizeof`, `alignof`, every `offsetof`, the cross-asserts against
// the PODs that still describe the same bytes, and the class/POD write-through
// property that every `reinterpret_cast` in `runtime/src/graphics/` relies on.
// ============================================================================

#include "unit.h"
#include "slot_check.h"
#include "dtor_check.h"

#include "runtime/types/graphics/Animation.h"
#include "runtime/types/graphics/AnimationGroup.h"
#include "runtime/types/graphics/AnimationManager.h"
#include "runtime/types/graphics/AnimationState.h"

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <new>
#include <type_traits>

namespace {

// ---------------------------------------------------------------------------
// Observation channel.
// ---------------------------------------------------------------------------
const void* g_lastSelf = nullptr;
int         g_lastMark = -1;

void Note(const void* self, int mark) { g_lastSelf = self; g_lastMark = mark; }
void Reset() { g_lastSelf = nullptr; g_lastMark = -1; }

// The one instruction every raw slot caller in runtime/src compiles to.
// `byteOffset` is RETAIL's; T (the object's class) decides the compiler's.
template <typename Fn, class T>
Fn SlotAt(const void* obj, int byteOffset) {
    const uint8_t* const* vt = *reinterpret_cast<const uint8_t* const* const*>(obj);
    return reinterpret_cast<Fn>(*reinterpret_cast<void* const*>(
        reinterpret_cast<const uint8_t*>(vt) + RetailByteOffset<T>(byteOffset)));
}

// ---------------------------------------------------------------------------
// The nine `Object::External`-family byte offsets, spelled once. Copied from
// B35_SLOTS.md, not reconstructed.
// ---------------------------------------------------------------------------
constexpr int kDtorSlot   = 0x00;
constexpr int kTypeName       = 0x08;
constexpr int kType           = 0x10;
constexpr int kGetFileVersion = 0x18;
constexpr int kCompile        = 0x20;
constexpr int kSerialize      = 0x28;
constexpr int kReconstruct    = 0x30;
constexpr int kDestruct       = 0x38;
constexpr int kConstruct      = 0x40;   // introduced per branch — RULE B5-16

// ---------------------------------------------------------------------------
// PART A — Graphics::AnimationEvent (ABSTRACT, 9 slots, 0x20, Object::External)
// ---------------------------------------------------------------------------
struct AnimationEventProbe final : Graphics::AnimationEvent {
    ~AnimationEventProbe() override { Note(this, kDtorSlot / 8); }
    DTOR_CHECK_RECORDING_DELETE
    const char* TypeName() const override {
        Note(this, kTypeName / 8); return "AnimationEvent";
    }
    uint32_t Type() const override { Note(this, kType / 8); return 0u; }
    int32_t GetFileVersion() const override { Note(this, kGetFileVersion / 8); return 0; }
    CompileResult Compile(const System::Filename&) override {
        Note(this, kCompile / 8); return 1u;
    }
    bool Serialize(void*) override { Note(this, kSerialize / 8); return true; }
    Object::External* Reconstruct() override { Note(this, kReconstruct / 8); return this; }
    void Destruct() override { Note(this, kDestruct / 8); }
    Object::External* Construct() override { Note(this, kConstruct / 8); return this; }
};

// ---------------------------------------------------------------------------
// PART B — Graphics::AnimationGroup (9 slots, 0x58, Object::External)
// ---------------------------------------------------------------------------
struct AnimationGroupProbe final : Graphics::AnimationGroup {
    ~AnimationGroupProbe() override { Note(this, kDtorSlot / 8); }
    DTOR_CHECK_RECORDING_DELETE
};

// Retail slot 0 is the destructor pair, read raw: compiler [0] D1 runs ~P once
// and frees nothing (retail flags 0); [1] D0 runs it and hands the block to the
// class-scope operator delete (flags 1). D1 is called on a placement-new'd
// probe never destroyed again (on a stack probe it would run twice).
template <class P>
void CheckDtorPair() {
    alignas(P) unsigned char buf[sizeof(P)];
    P* p = ::new (static_cast<void*>(buf)) P();
    const void* const* vt = *reinterpret_cast<const void* const* const*>(p);
    CHECK(vt[0] != vt[1]);
    Reset(); dtor_check::ResetFrees();
    dtor_check::CallD1(p);
    CHECK_EQ(g_lastMark, kDtorSlot / 8); CHECK(g_lastSelf == p);
    CHECK_EQ(dtor_check::g_frees, 0);
    P* h = dtor_check::NewOnMalloc<P>();
    Reset(); dtor_check::ResetFrees();
    dtor_check::CallD0(h);
    CHECK_EQ(g_lastMark, kDtorSlot / 8); CHECK(g_lastSelf == h);
    CHECK_EQ(dtor_check::g_frees, 1); CHECK(dtor_check::g_lastFree == h);
}

// The shared five: every slot whose signature is plain machine words. `Compile`
// takes a `const System::Filename&` (one pointer) and returns a four-byte enum
// in EAX with NO hidden return slot (B35_RULES section 2.5 excludes `?AW4`), so
// it raw-calls safely too.
template <class T>
void CheckExternalFamilySlots(void* raw, bool checkConstruct,
                              const ::Object::BaseTypeInfo* record = nullptr) {
    using NameFn  = const char* (*)(const void*);
    using TypeFn  = uint32_t (*)(const void*);
    using VerFn   = int32_t (*)(const void*);
    using CompFn  = uint32_t (*)(void*, const void*);
    using SerFn   = bool (*)(void*, void*);
    using PtrFn   = void* (*)(void*);
    using VoidFn  = void (*)(void*);

    CheckDtorPair<T>();   // retail slot 0 (kDtorSlot): D1 / D0

    if (record) {   // DECLARE_TYPE's real slots 1/2 over the class's record (B.3.9 TI-REST)
        CHECK((SlotAt<NameFn, T>(raw, kTypeName)(raw)) == record->name);
        CHECK_EQ((SlotAt<TypeFn, T>(raw, kType)(raw)), record->typeId);
    } else {
        Reset(); SlotAt<NameFn, T>(raw, kTypeName)(raw);
        CHECK_EQ(g_lastMark, kTypeName / 8);

        Reset(); SlotAt<TypeFn, T>(raw, kType)(raw);
        CHECK_EQ(g_lastMark, kType / 8);
    }

    Reset(); SlotAt<VerFn, T>(raw, kGetFileVersion)(raw);
    CHECK_EQ(g_lastMark, kGetFileVersion / 8);

    Reset(); SlotAt<CompFn, T>(raw, kCompile)(raw, nullptr);
    CHECK_EQ(g_lastMark, kCompile / 8);

    Reset(); SlotAt<SerFn, T>(raw, kSerialize)(raw, nullptr);
    CHECK_EQ(g_lastMark, kSerialize / 8);

    Reset(); SlotAt<PtrFn, T>(raw, kReconstruct)(raw);
    CHECK_EQ(g_lastMark, kReconstruct / 8);

    Reset(); SlotAt<VoidFn, T>(raw, kDestruct)(raw);
    CHECK_EQ(g_lastMark, kDestruct / 8);

    if (checkConstruct) {
        Reset(); SlotAt<PtrFn, T>(raw, kConstruct)(raw);
        CHECK_EQ(g_lastMark, kConstruct / 8);
    }
}

UNIT_TEST(graphics_animationevent_slot_order_is_retails) {
    AnimationEventProbe probe;
    void* raw = static_cast<void*>(&probe);

    // The probe overrides all nine, so all nine raw-call onto their own marker.
    CheckExternalFamilySlots<AnimationEventProbe>(raw, /*checkConstruct=*/true);

    // Nine retail slots are TEN compiler entries (retail slot 0 = D1 + D0),
    // all distinct, D1 != D0 included.
    const void* const* vt = *reinterpret_cast<const void* const* const*>(raw);
    for (int i = 0; i < 10; ++i)
        for (int j = i + 1; j < 10; ++j)
            CHECK(vt[i] != vt[j]);

    // `this` is unadjusted at +0x00: single inheritance, no virtual bases.
    CHECK(static_cast<void*>(static_cast<Graphics::AnimationEvent*>(&probe)) == raw);
    CHECK(static_cast<void*>(static_cast<Object::External*>(&probe)) == raw);
    CHECK(static_cast<void*>(static_cast<Object::Base*>(&probe)) == raw);
}

UNIT_TEST(graphics_animationevent_layout) {
    // 0x20 — the `MOV EDX,0x20` in its own vector deleting destructor
    // (0x180011800), and `RT_DELDTOR(0x180011800, 0x1800a2120, 0x020, ...)`.
    CHECK_EQ((int)sizeof(Graphics::AnimationEvent), 0x20);
    CHECK_EQ((int)sizeof(RtGraphics::AnimationEvent), 0x20);
    CHECK_EQ((int)sizeof(Graphics::AnimationEvent), (int)sizeof(RtGraphics::AnimationEvent));
    CHECK_EQ((int)alignof(Graphics::AnimationEvent), 8);

    // AnimationEvent adds NO bytes to Object::External — the whole class is a
    // vtable and a new slot.
    CHECK_EQ((int)sizeof(Graphics::AnimationEvent), (int)sizeof(Object::External));

    // Abstract -- since ENDGAME B.3.9 GFX NOT by slot 0 (concrete now, the
    // RT_DELDTOR row retired) but by the pure External slots and `Construct`:
    // `Graphics::AnimationEvent e;` must still not compile.
    CHECK(std::is_abstract<Graphics::AnimationEvent>::value);
    // B3X W1: public virtual destructor (retail slot 0 = D1 + D0).
    CHECK(std::has_virtual_destructor<Graphics::AnimationEvent>::value);
    CHECK(std::is_destructible<Graphics::AnimationEvent>::value);
    CHECK(std::is_polymorphic<Graphics::AnimationEvent>::value);
    CHECK((!std::is_base_of<Object::CompiledExternal, Graphics::AnimationEvent>::value));
    CHECK((std::is_base_of<Object::External, Graphics::AnimationEvent>::value));
}

UNIT_TEST(graphics_animationgroup_slot_order_is_retails) {
    AnimationGroupProbe probe;
    void* raw = static_cast<void*>(&probe);

    // Slots 3-8 are AnimationGroup's OWN overrides, whose bodies are the marker
    // stubs at the bottom of this file; 1/2 are DECLARE_TYPE's over the class
    // record (ENDGAME B.3.9 TI-REST). Slot 0 is the probe's.
    CheckExternalFamilySlots<AnimationGroupProbe>(raw, /*checkConstruct=*/true,
                                                  &Graphics::AnimationGroup::GetTypeInfo());

    const void* const* vt = *reinterpret_cast<const void* const* const*>(raw);
    for (int i = 0; i < 10; ++i)          // nine retail slots, ten compiler entries
        for (int j = i + 1; j < 10; ++j)
            CHECK(vt[i] != vt[j]);

    CHECK(static_cast<void*>(static_cast<Object::External*>(&probe)) == raw);
}

UNIT_TEST(graphics_animationgroup_layout) {
    CHECK_EQ((int)sizeof(Graphics::AnimationGroup), 0x58);
    CHECK_EQ((int)alignof(Graphics::AnimationGroup), 8);
    CHECK_EQ((int)offsetof(Graphics::AnimationGroup, _clips),         0x20);
    CHECK_EQ((int)offsetof(Graphics::AnimationGroup, _defaultBlend),  0x30);
    CHECK_EQ((int)offsetof(Graphics::AnimationGroup, _tags),          0x38);
    CHECK_EQ((int)offsetof(Graphics::AnimationGroup, _blendTimes),    0x48);
    CHECK((std::is_base_of<Object::External, Graphics::AnimationGroup>::value));
    CHECK((!std::is_base_of<Object::CompiledExternal, Graphics::AnimationGroup>::value));

    // A field write through the CLASS is visible at the bytes every
    // `reinterpret_cast` in vtbatch7_animgroup_particlesystem.cpp reads.
    AnimationGroupProbe probe;
    probe._defaultBlend = 0.1f;
    probe._clips._size  = 7;
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(
        static_cast<Graphics::AnimationGroup*>(&probe));
    CHECK_EQ(*reinterpret_cast<const float*>(bytes + 0x30), 0.1f);
    CHECK_EQ(*reinterpret_cast<const int32_t*>(bytes + 0x28), 7);
}

// ---------------------------------------------------------------------------
// ENDGAME B.3.9 block GFX -- AnimationEvent's OWN slot 0 (B39_RULES D1, B9-16).
// A leaf that overrides every pure slot but NOT slot 0 reaches the class's
// vector deleting dtor: both arms as the retired RT_DELDTOR row 0x180011800
// ran them (element size 0x20, count at this-8, downward, free on &1).
// B3X W1: flags 0 is `~T()`, flags 1 `delete` (the free proven on a recording
// stand-in), and the vector arm 0x180011800 is retired: `delete[]` replaces it.
// ---------------------------------------------------------------------------
int g_aeDtorCount = 0;
const void* g_aeDtorOrder[4] = {};

struct AnimationEventLeaf final : Graphics::AnimationEvent {
    DTOR_CHECK_RECORDING_DELETE   // B3X W1: the flags-1 free, recorded
    int32_t GetFileVersion() const override { return 0; }
    bool Serialize(void*) override { return true; }
    Object::External* Reconstruct() override { return this; }
    void Destruct() override {}
    Object::External* Construct() override { return this; }
};
static_assert(sizeof(AnimationEventLeaf) == 0x20, "the leaf adds nothing");

UNIT_TEST(b39_gfx_animationevent_slot0_is_the_classs_own_both_arms) {
    alignas(8) uint8_t buf[0x20];
    std::memset(buf, 0xCD, sizeof buf);
    AnimationEventLeaf* e = ::new (static_cast<void*>(buf)) AnimationEventLeaf();
    CHECK_EQ(*reinterpret_cast<const uint64_t*>(buf + 0x08), (uint64_t)0);   // External()
    g_aeDtorCount = 0;
    e->~AnimationEventLeaf();
    CHECK_EQ(g_aeDtorCount, 1);

    void* m = Object::detail::InternalMemAlloc(0x20, 8);
    Graphics::AnimationEvent* h = ::new (m) AnimationEventLeaf();
    g_aeDtorCount = 0; dtor_check::ResetFrees();
    delete h;
    CHECK_EQ(g_aeDtorCount, 1);
    CHECK_EQ(dtor_check::g_frees, 1);
    CHECK(dtor_check::g_lastFree == m);


    CHECK_ARRAY_DELETE(AnimationEventLeaf, g_aeDtorCount, g_aeDtorOrder);
}

// ---------------------------------------------------------------------------
// ENDGAME B.3.9 block GFX -- AnimationGroup CONCRETE (B39_RULES D1, B9-16).
// ---------------------------------------------------------------------------
int g_agDtorCount = 0;
const void* g_agDtorOrder[4] = {};

UNIT_TEST(b39_gfx_animationgroup_concrete_slot_order_by_pmf) {
    using G = Graphics::AnimationGroup;
    CHECK(!std::is_abstract<G>::value);
    CHECK(std::is_destructible<G>::value);          // B3X W1: public virtual
    CHECK(std::has_virtual_destructor<G>::value);
    CHECK_DTOR_SLOT(G);   // 0x1800167f0
    CHECK_SLOT(G, TypeName,       0x08/8);   // 0x180016310
    CHECK_SLOT(G, Type,           0x10/8);   // 0x1800162f0
    CHECK_SLOT(G, GetFileVersion, 0x18/8);   // 0x1800fb680
    CHECK_SLOT(G, Compile,        0x20/8);   // 0x1800fb690
    CHECK_SLOT(G, Serialize,      0x28/8);   // 0x1800fb6a0
    CHECK_SLOT(G, Reconstruct,    0x30/8);   // 0x180016550
    CHECK_SLOT(G, Destruct,       0x38/8);   // 0x180016650
    CHECK_SLOT(G, Construct,      0x40/8);   // 0x1800165d0
}

// The real ctor makes the factory's stores (0x180016463..4ab): External's three
// zero qwords, the three Array headers zero, +0x30 = 0.1f, +0x34 untouched;
// slot 0 runs ~AnimationGroup once per element, both arms.
UNIT_TEST(b39_gfx_animationgroup_ctor_stores_and_both_slot0_arms) {
    alignas(8) uint8_t buf[0x58];
    std::memset(buf, 0xCD, sizeof buf);
    Graphics::AnimationGroup* g = ::new (static_cast<void*>(buf)) Graphics::AnimationGroup();
    for (size_t off = 0x08; off < 0x30; off += 8)
        CHECK_EQ(*reinterpret_cast<const uint64_t*>(buf + off), (uint64_t)0);
    CHECK_EQ(*reinterpret_cast<const uint32_t*>(buf + 0x30), 0x3dcccccdu);
    CHECK_EQ(*reinterpret_cast<const uint32_t*>(buf + 0x34), 0xCDCDCDCDu);
    for (size_t off = 0x38; off < 0x58; off += 8)
        CHECK_EQ(*reinterpret_cast<const uint64_t*>(buf + off), (uint64_t)0);
    g_agDtorCount = 0;
    g->~AnimationGroup();                            // retail flags 0
    CHECK_EQ(g_agDtorCount, 1);

    static_assert(dtor_check::HasClassDelete<Graphics::AnimationGroup>::value,
                  "D0 frees through the class's own operator delete (MemFree)");
    void* m = Object::detail::InternalMemAlloc(0x58, 8);
    Graphics::AnimationGroup* h = ::new (m) Graphics::AnimationGroup();
    g_agDtorCount = 0;
    delete h;                                        // retail flags 1
    CHECK_EQ(g_agDtorCount, 1);

    Graphics::AnimationGroup* r =
        dtor_check::NewOnMalloc<dtor_check::Recording<Graphics::AnimationGroup>>();
    g_agDtorCount = 0; dtor_check::ResetFrees();
    delete r;
    CHECK_EQ(g_agDtorCount, 1);
    CHECK_EQ(dtor_check::g_frees, 1);
    CHECK(dtor_check::g_lastFree == static_cast<const void*>(r));

    // The retired vector arm (0x1800167f0, stride 0x58): delete[], downward.
    CHECK_ARRAY_DELETE(Graphics::AnimationGroup, g_agDtorCount, g_agDtorOrder);
}


// ---------------------------------------------------------------------------
// PART C — Graphics::Animation, the 0x70 clip record (NON-polymorphic)
// ---------------------------------------------------------------------------
UNIT_TEST(graphics_animation_layout) {
    CHECK_EQ((int)sizeof(Graphics::Animation), 0x70);
    CHECK_EQ((int)alignof(Graphics::Animation), 8);
    CHECK_EQ((int)sizeof(Graphics::Animation::Event), 0x10);

    CHECK_EQ((int)offsetof(Graphics::Animation, _events),        0x00);
    CHECK_EQ((int)offsetof(Graphics::Animation, _name),          0x10);
    CHECK_EQ((int)offsetof(Graphics::Animation, _meshName),      0x18);
    CHECK_EQ((int)offsetof(Graphics::Animation, _sourceName),    0x20);
    CHECK_EQ((int)offsetof(Graphics::Animation, _instanceCount), 0x28);
    CHECK_EQ((int)offsetof(Graphics::Animation, _instanced),     0x2c);
    CHECK_EQ((int)offsetof(Graphics::Animation, _startFrame),    0x30);
    CHECK_EQ((int)offsetof(Graphics::Animation, _endFrame),      0x34);
    CHECK_EQ((int)offsetof(Graphics::Animation, _rate),          0x38);
    CHECK_EQ((int)offsetof(Graphics::Animation, _looping),       0x3c);
    CHECK_EQ((int)offsetof(Graphics::Animation, _loopFrame),     0x40);
    CHECK_EQ((int)offsetof(Graphics::Animation, _frameRate),     0x44);
    CHECK_EQ((int)offsetof(Graphics::Animation, _loopScale),     0x48);
    CHECK_EQ((int)offsetof(Graphics::Animation, _boneCount),     0x4c);
    CHECK_EQ((int)offsetof(Graphics::Animation, _frameCount),    0x50);
    CHECK_EQ((int)offsetof(Graphics::Animation, _duration),      0x58);
    CHECK_EQ((int)offsetof(Graphics::Animation, _keys),          0x60);

    // NEVER A VTABLE: retail exports no `??_7Animation@Graphics@@6B@` and both
    // its constructor and its destructor are `QEAA`.
    CHECK(!std::is_polymorphic<Graphics::Animation>::value);
    CHECK(std::is_standard_layout<Graphics::Animation>::value);

    // The clip is an ELEMENT of `AnimationGroup::_clips` at stride 0x70 — the
    // `imul $0x70` in `?GetAnimation@AnimationGroup@Graphics@@` (0x180016660).
    // This is the assert that would catch a field added to the class.
    CHECK_EQ((int)sizeof(Graphics::Animation), 0x70);

    // `AnimationState.h`'s `AnimationClipPrefix` is a PREFIX VIEW of the same
    // record and both headers must keep agreeing on every field they share.
    CHECK_EQ((int)offsetof(RtGraphics::AnimationClipPrefix, frameFirst),
             (int)offsetof(Graphics::Animation, _startFrame));
    CHECK_EQ((int)offsetof(RtGraphics::AnimationClipPrefix, frameLast),
             (int)offsetof(Graphics::Animation, _endFrame));
    CHECK_EQ((int)offsetof(RtGraphics::AnimationClipPrefix, looping),
             (int)offsetof(Graphics::Animation, _looping));
    CHECK_EQ((int)offsetof(RtGraphics::AnimationClipPrefix, loopFrame),
             (int)offsetof(Graphics::Animation, _loopFrame));
    CHECK_EQ((int)offsetof(RtGraphics::AnimationClipPrefix, loopScale),
             (int)offsetof(Graphics::Animation, _loopScale));
    CHECK_EQ((int)offsetof(RtGraphics::AnimationClipPrefix, frameCount),
             (int)offsetof(Graphics::Animation, _frameCount));
    CHECK_EQ((int)offsetof(RtGraphics::AnimationClipPrefix, duration),
             (int)offsetof(Graphics::Animation, _duration));
    CHECK_EQ((int)offsetof(RtGraphics::AnimationClipPrefix, eventCount),
             (int)offsetof(Graphics::Animation, _events) + 8);

    // And the `anim_clip::k*` constants every unconverted body still uses are
    // the same numbers. This is the cross-assert that stops the two drifting.
    CHECK_EQ(RtGraphics::anim_clip::kStride,     0x70);
    CHECK_EQ(RtGraphics::anim_clip::kInstanced,  (int)offsetof(Graphics::Animation, _instanced));
    CHECK_EQ(RtGraphics::anim_clip::kFrameFirst, (int)offsetof(Graphics::Animation, _startFrame));
    CHECK_EQ(RtGraphics::anim_clip::kFrameLast,  (int)offsetof(Graphics::Animation, _endFrame));
    CHECK_EQ(RtGraphics::anim_clip::kLooping,    (int)offsetof(Graphics::Animation, _looping));
    CHECK_EQ(RtGraphics::anim_clip::kLoopFrame,  (int)offsetof(Graphics::Animation, _loopFrame));
    CHECK_EQ(RtGraphics::anim_clip::kFrameRate,  (int)offsetof(Graphics::Animation, _frameRate));
    CHECK_EQ(RtGraphics::anim_clip::kBoneCount,  (int)offsetof(Graphics::Animation, _boneCount));
    CHECK_EQ(RtGraphics::anim_clip::kFrameCount, (int)offsetof(Graphics::Animation, _frameCount));
    CHECK_EQ(RtGraphics::anim_clip::kDuration,   (int)offsetof(Graphics::Animation, _duration));
    CHECK_EQ(RtGraphics::anim_clip::kKeys,       (int)offsetof(Graphics::Animation, _keys));
}

// ---------------------------------------------------------------------------
// PART D — Graphics::AnimationState, 0x48 (NON-polymorphic)
// ---------------------------------------------------------------------------
UNIT_TEST(graphics_animationstate_layout) {
    CHECK_EQ((int)sizeof(Graphics::AnimationState), 0x48);
    CHECK_EQ((int)alignof(Graphics::AnimationState), 8);
    CHECK_EQ((int)offsetof(Graphics::AnimationState, _needsUniqueCopy), 0x00);
    CHECK_EQ((int)offsetof(Graphics::AnimationState, _layers),          0x08);
    CHECK_EQ((int)offsetof(Graphics::AnimationState, _rootOffset),      0x18);
    CHECK_EQ((int)offsetof(Graphics::AnimationState, _pose),            0x28);
    CHECK_EQ((int)offsetof(Graphics::AnimationState, _nodes),           0x38);
    CHECK(!std::is_polymorphic<Graphics::AnimationState>::value);
    CHECK(std::is_standard_layout<Graphics::AnimationState>::value);

    // The nested Layer record — stride 0x38, measured three ways inside
    // `AnimationState::Update` (AnimationState.h's banner).
    CHECK_EQ((int)sizeof(Graphics::AnimationState::Layer), 0x38);
    CHECK_EQ((int)offsetof(Graphics::AnimationState::Layer, _clipIndex),     0x08);
    CHECK_EQ((int)offsetof(Graphics::AnimationState::Layer, _handle),        0x0c);
    CHECK_EQ((int)offsetof(Graphics::AnimationState::Layer, _speed),         0x10);
    CHECK_EQ((int)offsetof(Graphics::AnimationState::Layer, _time),          0x18);
    CHECK_EQ((int)offsetof(Graphics::AnimationState::Layer, _fadeLength),    0x20);
    CHECK_EQ((int)offsetof(Graphics::AnimationState::Layer, _fadeRemaining), 0x28);
    CHECK_EQ((int)offsetof(Graphics::AnimationState::Layer, _restarted),     0x30);
    CHECK_EQ((int)sizeof(Graphics::AnimationState::Layer),
             (int)sizeof(RtGraphics::AnimationLayer));

    // THE THREE SPELLINGS OF ONE RECORD must stay aligned: the class, lane
    // GFXBIG's `RtGraphics::AnimationState` POD (Update's view) and the
    // serializer's `RtGraphics::AnimStateRec`.
    CHECK_EQ((int)sizeof(Graphics::AnimationState), (int)sizeof(RtGraphics::AnimationState));
    CHECK_EQ((int)sizeof(Graphics::AnimationState), (int)sizeof(RtGraphics::AnimStateRec));
    CHECK_EQ((int)offsetof(Graphics::AnimationState, _layers),
             (int)offsetof(RtGraphics::AnimStateRec, _layers));
    CHECK_EQ((int)offsetof(Graphics::AnimationState, _rootOffset),
             (int)offsetof(RtGraphics::AnimStateRec, _rootOffset));
    CHECK_EQ((int)offsetof(Graphics::AnimationState, _pose),
             (int)offsetof(RtGraphics::AnimStateRec, _pose));
    CHECK_EQ((int)offsetof(Graphics::AnimationState, _nodes),
             (int)offsetof(RtGraphics::AnimStateRec, _nodes));
    CHECK_EQ((int)offsetof(Graphics::AnimationState, _layers),
             (int)offsetof(RtGraphics::AnimationState, layers));

    // THE 0x18-BYTE PREFIX `Mesh.h` EMBEDS IN `MeshInstance` as
    // `MeshAnimationState`. Block E owns that header; this assert is the
    // relationship stated as a test rather than only as a comment — the first
    // three members of the class occupy exactly the prefix's bytes.
    CHECK_EQ((int)offsetof(Graphics::AnimationState, _rootOffset), 0x18);

    // Written through a RAW BUFFER, not a local: `??0AnimationState@Graphics@@
    // QEAA@XZ` (0x1800fdaf0) is declared and deliberately undefined (nothing in
    // this tree calls it), so the class is not default-constructible on purpose.
    // Every live one is `MemAlloc`'d anyway.
    alignas(8) uint8_t buf[0x48] = {0};
    Graphics::AnimationState* st = reinterpret_cast<Graphics::AnimationState*>(buf);
    st->_needsUniqueCopy = 1;
    st->_layers._size    = 3;
    CHECK_EQ((int)buf[0x00], 1);
    CHECK_EQ((int)*reinterpret_cast<const int32_t*>(buf + 0x10), 3);
}

// ---------------------------------------------------------------------------
// PART E — Graphics::AnimationManager, 0x9d8 singleton (NON-polymorphic)
// ---------------------------------------------------------------------------
UNIT_TEST(graphics_animationmanager_layout) {
    CHECK_EQ((int)sizeof(Graphics::AnimationManager), 0x9d8);
    CHECK_EQ((int)alignof(Graphics::AnimationManager), 8);
    CHECK_EQ((int)offsetof(Graphics::AnimationManager, _instances), 0x00);
    CHECK_EQ((int)offsetof(Graphics::AnimationManager, _f10),       0x10);
    CHECK_EQ((int)offsetof(Graphics::AnimationManager, _rngState),  0x14);
    CHECK(!std::is_polymorphic<Graphics::AnimationManager>::value);
    CHECK(std::is_standard_layout<Graphics::AnimationManager>::value);

    // The two PODs that still describe these bytes.
    CHECK_EQ((int)sizeof(Graphics::AnimationManager),
             (int)sizeof(RtGraphics::AnimationManager));
    CHECK_EQ((int)offsetof(Graphics::AnimationManager, _instances),
             (int)offsetof(RtGraphics::AnimationManagerView, _instances));
    CHECK_EQ((int)offsetof(Graphics::AnimationManager, _rngState),
             (int)offsetof(RtGraphics::AnimationManager, _rngState));
    CHECK_EQ((int)offsetof(Graphics::AnimationManager, _f10),
             (int)offsetof(RtGraphics::AnimationManager, _f10));

    // The instance record the manager's array holds, stride 0x30.
    CHECK_EQ((int)sizeof(Graphics::AnimationManager::Instance), 0x30);
    CHECK_EQ((int)offsetof(Graphics::AnimationManager::Instance, _animIndex), 0x08);
    CHECK_EQ((int)offsetof(Graphics::AnimationManager::Instance, _refCount),  0x0c);
    CHECK_EQ((int)offsetof(Graphics::AnimationManager::Instance, _nodes),     0x10);
    CHECK_EQ((int)offsetof(Graphics::AnimationManager::Instance, _states),    0x20);
    CHECK_EQ((int)sizeof(Graphics::AnimationManager::Instance),
             (int)sizeof(RtGraphics::AnimInstanceRec));
}

}  // namespace

// ############################################################################
// THE OUT-OF-LINE BODIES THE TWO PROBES NEED AT LINK TIME.
//
// See the file banner: `runtime/src/graphics/animation.cpp` and
// `vtbatch7_animgroup_particlesystem.cpp` hold the real ones and neither is in
// `UNIT_TREE_SRCS`, so there is exactly one definition per binary. Every body
// here is a MARKER, deliberately not a reimplementation — this file tests slot
// order and layout, not the reflection records or the serializer.
// ############################################################################

namespace Graphics {

// ENDGAME B.3.9 GFX: the marker RECORDS each call (the real body is empty).
AnimationEvent::~AnimationEvent() {
    if (g_aeDtorCount < 4) g_aeDtorOrder[g_aeDtorCount] = this;
    ++g_aeDtorCount;
}

int32_t     AnimationGroup::GetFileVersion() const { Note(this, kGetFileVersion / 8); return 8; }
Object::External::CompileResult AnimationGroup::Compile(const System::Filename&) {
    Note(this, kCompile / 8); return 1u;
}
bool AnimationGroup::Serialize(void*) { Note(this, kSerialize / 8); return true; }
Object::External* AnimationGroup::Reconstruct() { Note(this, kReconstruct / 8); return this; }
void AnimationGroup::Destruct() { Note(this, kDestruct / 8); }
Object::External* AnimationGroup::Construct() { Note(this, kConstruct / 8); return this; }
// ENDGAME B.3.9 GFX: the marker RECORDS each call (the real body is the three
// by-address ~Array calls in vtbatch7_animgroup_particlesystem.cpp).
AnimationGroup::~AnimationGroup() {
    if (g_agDtorCount < 4) g_agDtorOrder[g_agDtorCount] = this;
    ++g_agDtorCount;
}

}  // namespace Graphics
