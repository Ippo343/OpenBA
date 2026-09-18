// ============================================================================
// tests/unit/object_vtable_abi_test.cpp — ENDGAME B.3.3 step 2.
//
// THE PREMISE THIS FILE PROVES
// ----------------------------
// B.3.3 converts `Object::Base` and its subclasses to real C++ classes ONE AT A
// TIME. Every class that has not been converted yet keeps dispatching through a
// hand-built / minted `void*[]` table at retail slot indices, and keeps being
// reached from ported C bodies as `(*(void***)obj)[N](obj, …)`. That only works
// if GCC's compiler-generated vtable for a class declared per ENDGAME §B.1
// decision 2 is INTERCHANGEABLE, slot for slot, with such an array.
//
// "Interchangeable" has two directions and this file tests both:
//
//   A. A C body that reads `(*(void***)obj)[N]` off a REAL C++ object must land
//      on that object's override, with `this` unadjusted, for N = the retail
//      slot index (direction 1: old caller -> new class).
//   B. A REAL C++ virtual call on an object whose vptr has been stamped with a
//      HAND-BUILT array of free functions must land on those free functions
//      (direction 2: new class -> old minted table). This is the direction that
//      matters for `rt_data_mint`/`Register*Vtable` objects handed to converted
//      code, and it is the one an ABI with a hidden prologue slot would break.
//
// WHAT IS *NOT* PROVEN HERE
// -------------------------
// Nothing about MSVC. The retail DLL is gone (B31_RULES "the one fact that
// changes the rules"): every caller and callee is this same GCC now. The only
// binary facts still binding are OBJECT LAYOUT and RETAIL SLOT ORDER, and slot
// order binds only because our own unconverted tables were transcribed from
// retail. So these tests compare GCC against GCC — which is exactly the
// question B.3.3 has to answer — and the retail slot list they mirror is cited
// from Ghidra in the comments below, not re-derived here.
//
// THE RETAIL SLOT LIST BEING MIRRORED (Runtime-x64.dll, 2026-09-18, Ghidra)
// ------------------------------------------------------------------------
// `??_7Base@Object@@6B@` @ 0x180154788 — THREE slots, no more:
//   [0] 0x1800065b0  vector deleting dtor (elem size 0x8, real dtor
//                    ??1Base@Object@@UEAA@XZ @ 0x1800c0360)
//   [1] 0x18007ec28  _purecall  -> `virtual const char* TypeName() const = 0`
//   [2] 0x1800c0380  `XOR EAX,EAX; RET` -> `?Type@Base@Object@@UEBAIXZ`
// `??_7External@Object@@6B@` @ 0x1801547a0 starts immediately after it, which
// is what bounds Base at three slots; its own [1]/[2] are
// `MOV RAX,[qword]; RET` and `MOV EAX,[dword]; RET`, matching the return types
// of ?TypeName@External@Object@@UEBAPEBDXZ and ?Type@External@Object@@UEBAIXZ.
//
// NOTE ON NAMES: retail calls slot 2 `Type()`, not `GetTypeId()`; the static
// companion `?TypeId@…@@SAIXZ` is a separate, NON-virtual member. See
// docs/runtime_port/endgame_b/B33_RULES.md §1.
// ============================================================================
#include "unit.h"

#include <cstdint>
#include <cstddef>
#include <type_traits>

namespace {

// ---------------------------------------------------------------------------
// Observation channel. The hand-built bodies and the class overrides all write
// here so a test can say WHICH body ran, not merely that something returned.
// ---------------------------------------------------------------------------
const void* g_lastSelf = nullptr;
int         g_lastMark = 0;

void Note(const void* self, int mark) { g_lastSelf = self; g_lastMark = mark; }
void Reset() { g_lastSelf = nullptr; g_lastMark = 0; }

// ===========================================================================
// PART A — `Object::Base`'s shape: slot 0 is a normal virtual with the vector
// deleting destructor's signature, there is NO virtual destructor, and the
// remaining slots follow in retail order.
// ===========================================================================

class FakeBase {
public:
    // slot 0  (+0x00)  retail 0x1800065b0. flags&1 = free, flags&2 = array.
    virtual FakeBase* DeletingDtor(unsigned flags) { Note(this, 100 + (int)flags); return this; }
    // slot 1  (+0x08)  retail _purecall in Base. Declared non-pure here so a
    //                  FakeBase can be instantiated and its own table read.
    virtual const char* TypeName() const { Note(this, 101); return "Base"; }
    // slot 2  (+0x10)  retail 0x1800c0380 `XOR EAX,EAX; RET`.
    virtual uint32_t Type() const { Note(this, 102); return 0; }

protected:
    // NOT virtual — ENDGAME §B.1 decision 2. Deletion goes through
    // `Object::Destroy(p)` -> slot 0, never through `delete p`.
    ~FakeBase() = default;
};

// Only the vptr, exactly like `RtObject::Base` (runtime/types/object/TypeInfo.h:58).
static_assert(sizeof(FakeBase) == 8, "Object::Base is one vtable pointer");
static_assert(!std::is_polymorphic<FakeBase>::value == false, "FakeBase must be polymorphic");
static_assert(std::has_virtual_destructor<FakeBase>::value == false,
              "decision 2: NO virtual destructor during the migration");

// A direct subclass in `Object::External`'s shape: overrides all three of
// Base's slots and appends its own, exactly as `??_7External@Object@@6B@` does.
class FakeExternal : public FakeBase {
public:
    FakeExternal* DeletingDtor(unsigned flags) override { Note(this, 200 + (int)flags); return this; }
    const char* TypeName() const override { Note(this, 201); return "External"; }
    uint32_t    Type() const override     { Note(this, 202); return 0xEE11u; }

    // slot 3 (+0x18) — retail Goal's override is `MOV EAX,0x10000; RET`,
    // i.e. `virtual int GetFileVersion() const`.
    virtual int GetFileVersion() const { Note(this, 203); return 0x10000; }
    // slot 4 (+0x20) — retail External's is `MOV EAX,1; RET`; name UNKNOWN,
    // carried as a numbered slot so the indices after it stay right.
    virtual int Slot4() const { Note(this, 204); return 1; }

    uint64_t _path = 0;      // mirrors External's +0x08 System::Filename
    uint64_t _refsData = 0;  // +0x10
    uint64_t _refsSize = 0;  // +0x18

    // Public and NON-virtual. Public only so the tests below can hold one by
    // value; the rule decision 2 states is "not virtual", which is what the
    // `has_virtual_destructor` assert on FakeBase pins.
    ~FakeExternal() = default;
};

static_assert(sizeof(FakeExternal) == 0x20, "External is 0x20 bytes (External.h:118-126)");

// The CONTROL that shows why decision 2 forbids a virtual destructor. Nothing
// depends on this class; it exists so the rule carries its own evidence.
class FakeWithVirtualDtor {
public:
    virtual ~FakeWithVirtualDtor() {}
    virtual const char* TypeName() const { return "shifted"; }
    virtual uint32_t    Type() const     { return 0xDEAD; }
};

// --- raw-slot helpers, the shape every unconverted C body in the tree uses ---

void** VptrOf(const void* obj) { return *reinterpret_cast<void** const*>(obj); }

// A polymorphic class is not standard-layout, and `offsetof` on one is only
// "conditionally supported" — GCC accepts it but emits -Winvalid-offsetof, and
// this tree builds with -Wall -Wextra and no -Wno-invalid-offsetof (Makefile:59,
// :120). EVERY type header B.3.3 converts carries `static_assert(offsetof(...))`
// pins that decision 1 says to keep, so this is a real, tree-wide consequence of
// the conversion and B33_RULES.md §6 states the decision. The tests here measure
// the offset from two live addresses instead, which is warning-free and pins the
// same fact.
uintptr_t FieldOffset(const void* obj, const void* field) {
    return reinterpret_cast<uintptr_t>(field) - reinterpret_cast<uintptr_t>(obj);
}

template <class Fn>
Fn SlotAs(const void* obj, int n) { return reinterpret_cast<Fn>(VptrOf(obj)[n]); }

using DeletingDtorFn = void* (*)(void*, unsigned);
using TypeNameFn     = const char* (*)(const void*);
using TypeFn         = uint32_t (*)(const void*);
using IntSlotFn      = int (*)(const void*);

// ---------------------------------------------------------------------------
// DIRECTION 1 — an unconverted C body reaching a real class through raw slots.
//
// What this actually proves: the function pointer GCC put at index N of the
// class's vtable, called with `this` passed unchanged as the first argument, is
// the override for the Nth DECLARED virtual. If GCC had spent a slot on
// anything else (an Itanium two-entry virtual destructor, an RTTI or
// offset-to-top entry INSIDE the table rather than before the address point,
// a vcall thunk), the marks below would come out shifted or the call would
// fault.
// ---------------------------------------------------------------------------
UNIT_TEST(base_raw_slot_dispatch_reaches_the_override) {
    FakeExternal ext;
    FakeBase* asBase = &ext;

    Reset();
    void* r = SlotAs<DeletingDtorFn>(asBase, 0)(asBase, 3u);
    CHECK_EQ(g_lastMark, 203 + 0);          // 200 + flags(3)
    CHECK(g_lastSelf == &ext);
    CHECK(r == static_cast<void*>(&ext));

    Reset();
    CHECK_STREQ(SlotAs<TypeNameFn>(asBase, 1)(asBase), "External");
    CHECK_EQ(g_lastMark, 201);

    Reset();
    CHECK_EQ(SlotAs<TypeFn>(asBase, 2)(asBase), 0xEE11u);
    CHECK_EQ(g_lastMark, 202);

    Reset();
    CHECK_EQ(SlotAs<IntSlotFn>(asBase, 3)(asBase), 0x10000);
    CHECK_EQ(g_lastMark, 203);

    Reset();
    CHECK_EQ(SlotAs<IntSlotFn>(asBase, 4)(asBase), 1);
    CHECK_EQ(g_lastMark, 204);

    // `this` is NOT adjusted for a single-inheritance chain: the pointer the
    // slot receives is the object address itself. Every ported C body relies on
    // this when it passes `self` straight through.
    CHECK(static_cast<void*>(asBase) == static_cast<void*>(&ext));
}

// A `FakeBase` that is exactly a Base, not a subclass, resolves to Base's own
// three bodies at the same indices. This is the "Base itself is registered in
// deleting_dtor.cpp as a probe subject" row's C++ counterpart.
UNIT_TEST(base_own_vtable_has_exactly_the_three_retail_slots) {
    // FakeBase's dtor is protected, so it is constructed through a trivial
    // local subclass that adds nothing — the vtable is still Base's three
    // bodies at 0,1,2 because nothing is overridden.
    struct Plain : FakeBase { ~Plain() = default; };
    Plain b;

    Reset();
    CHECK_STREQ(SlotAs<TypeNameFn>(&b, 1)(&b), "Base");
    CHECK_EQ(g_lastMark, 101);
    Reset();
    CHECK_EQ(SlotAs<TypeFn>(&b, 2)(&b), 0u);
    CHECK_EQ(g_lastMark, 102);
    Reset();
    SlotAs<DeletingDtorFn>(&b, 0)(&b, 0u);
    CHECK_EQ(g_lastMark, 100);

    // The control: with a virtual destructor, `TypeName` is NOT at slot 1.
    // Itanium spends TWO slots on `virtual ~T()` (complete + deleting), so the
    // first user-declared virtual lands at index 2. Asserting the shift makes
    // decision 2's "no virtual destructor during the migration" a tested rule
    // rather than a remembered one.
    FakeWithVirtualDtor v;
    CHECK_STREQ(SlotAs<TypeNameFn>(&v, 2)(&v), "shifted");
    CHECK_EQ(SlotAs<TypeFn>(&v, 3)(&v), 0xDEADu);
}

// ---------------------------------------------------------------------------
// DIRECTION 2 — a real class dispatching through a HAND-BUILT array.
//
// This is the coexistence claim proper. A converted class must be able to make
// an ordinary C++ virtual call on an object whose vptr came from
// `rt_data_mint` / a `Register*Vtable` array, because half the tree still hands
// such objects around. Here the array is built out of plain free functions with
// the `(self, …)` shape those tables use.
// ---------------------------------------------------------------------------
namespace hand {
FakeBase*   DeletingDtor(void* self, unsigned flags) { Note(self, 300 + (int)flags); return static_cast<FakeBase*>(self); }
const char* TypeName(const void* self)               { Note(self, 301); return "minted"; }
uint32_t    Type(const void* self)                   { Note(self, 302); return 0x4D1Du; }
}  // namespace hand

UNIT_TEST(real_class_virtual_call_lands_in_a_hand_built_table) {
    struct Plain : FakeBase { ~Plain() = default; };
    Plain obj;

    void* const original = VptrOf(&obj);

    // The table a mint row would produce: three entries, retail order, nothing
    // before the address point that the call path reads.
    void* handTable[3] = {
        reinterpret_cast<void*>(&hand::DeletingDtor),
        reinterpret_cast<void*>(&hand::TypeName),
        reinterpret_cast<void*>(&hand::Type),
    };
    *reinterpret_cast<void***>(&obj) = handTable;

    FakeBase* p = &obj;

    Reset();
    CHECK_STREQ(p->TypeName(), "minted");     // ordinary C++ virtual call
    CHECK_EQ(g_lastMark, 301);
    CHECK(g_lastSelf == &obj);

    Reset();
    CHECK_EQ(p->Type(), 0x4D1Du);
    CHECK_EQ(g_lastMark, 302);

    Reset();
    CHECK(p->DeletingDtor(0u) == p);
    CHECK_EQ(g_lastMark, 300);

    // Put the compiler's own table back before `obj` is destroyed. Leaving the
    // stack array installed would be a dangling vptr the moment this frame
    // returns, and ~Plain is reached through the object.
    *reinterpret_cast<void***>(&obj) = reinterpret_cast<void**>(original);
    Reset();
    CHECK_STREQ(p->TypeName(), "Base");
    CHECK_EQ(g_lastMark, 101);
}

// A COPY of the compiler's table is as good as the compiler's table. This is
// literally what `tools/rt_mint_gen.py` did with the DLL's exported vtables,
// and what `code/Game/Game/tornado.cpp:622-640` still does at runtime: copy the
// live table, patch a few slots, stamp it back.
UNIT_TEST(copied_compiler_vtable_is_interchangeable_slot_for_slot) {
    FakeExternal a;
    FakeExternal b;

    void** src = VptrOf(&a);
    void*  patched[5];
    for (int i = 0; i < 5; ++i) patched[i] = src[i];
    patched[1] = reinterpret_cast<void*>(&hand::TypeName);   // patch ONE slot

    void* const original = VptrOf(&b);
    *reinterpret_cast<void***>(&b) = patched;

    FakeBase* p = &b;
    Reset();
    CHECK_STREQ(p->TypeName(), "minted");   // the patched slot
    CHECK_EQ(g_lastMark, 301);
    Reset();
    CHECK_EQ(p->Type(), 0xEE11u);           // an untouched copied slot still
    CHECK_EQ(g_lastMark, 202);              // reaches FakeExternal::Type

    *reinterpret_cast<void***>(&b) = reinterpret_cast<void**>(original);
    Reset();
    CHECK_STREQ(p->TypeName(), "External");
    CHECK_EQ(g_lastMark, 201);
}

// ===========================================================================
// PART B — the secondary vptr.
//
// `engine/types/ControllerComponent.h` pins a second vtable pointer at +0x28,
// immediately after the 0x28-byte `Component` base
// (`engine/types/Component.h:41`, `static_assert(sizeof(Component) == 0x28)`),
// and `engine/types/BuildComponent.h:48` asserts the same offset for the
// largest member of that family. ENDGAME §B.1 decision 2 proposes to reproduce
// it as `class BuildComponent : public Component, public ControllerComponent`.
//
// THE FINDING THIS TEST RECORDS (see B33_RULES.md §5): +0x28 is the COMMON
// case, not the rule. `engine/types/AttendanceComponent.h:48` pins the
// secondary vptr at +0x38 and `engine/types/NomadComponent.h:55` at +0x58,
// with the class's OWN data at +0x28..  Two non-virtual bases are always laid
// out back to back, so no base-class spelling can produce those two layouts:
// they are an embedded polymorphic MEMBER after some of the derived class's
// own fields. Both shapes are tested below, because B.3.3's rule has to cover
// both and the per-class offset assert is the thing that decides which.
// ===========================================================================

class FakeComponent {
public:
    virtual FakeComponent* DeletingDtor(unsigned flags) { Note(this, 400 + (int)flags); return this; }
    virtual void  Startup()  { Note(this, 401); }
    virtual void  Shutdown() { Note(this, 402); }

    void*    _next  = nullptr;   // +0x08
    void*    _owner = nullptr;   // +0x10
    void*    _unk18 = nullptr;   // +0x18
    uint32_t _flags = 0x80;      // +0x20
    uint32_t _pad24 = 0;         // +0x24

protected:
    ~FakeComponent() = default;
};
static_assert(sizeof(FakeComponent) == 0x28, "Component is 0x28 (engine/types/Component.h:41)");

// The controller/state-node interface reached through the SECOND vptr.
// Slot names from code/Game/Component/controller_live_base.h:24-31.
class FakeControllerIface {
public:
    virtual int  Step(int input)   { Note(this, 500 + input); return 0; }
    virtual char Enter(void* sm)   { Note(this, 501); (void)sm; return 1; }
    virtual char Update(void* sm)  { Note(this, 502); (void)sm; return 1; }

protected:
    ~FakeControllerIface() = default;
};
static_assert(sizeof(FakeControllerIface) == 8, "the secondary sub-object is one vptr");

// SHAPE 1 — two non-virtual bases, secondary vptr at +0x28 (BuildComponent).
class FakeBuildComponent : public FakeComponent, public FakeControllerIface {
public:
    void Startup() override { Note(this, 601); }
    int  Step(int input) override { Note(this, 700 + input); return input * 2; }

    uint64_t _own0 = 0;  // +0x30
    uint64_t _own1 = 0;  // +0x38

    ~FakeBuildComponent() = default;
};

UNIT_TEST(two_base_class_puts_the_second_vptr_at_0x28) {
    FakeBuildComponent obj;

    const auto objAddr  = reinterpret_cast<uintptr_t>(&obj);
    const auto baseAddr = reinterpret_cast<uintptr_t>(static_cast<FakeComponent*>(&obj));
    const auto ctrlAddr = reinterpret_cast<uintptr_t>(static_cast<FakeControllerIface*>(&obj));

    // The primary base sits at +0x00 and the secondary at +0x28, exactly as
    // ControllerComponent.h:54 asserts for the retail object.
    CHECK_EQ(baseAddr - objAddr, (uintptr_t)0x00);
    CHECK_EQ(ctrlAddr - objAddr, (uintptr_t)0x28);

    // And the secondary vptr is a DIFFERENT table from the primary one.
    CHECK(VptrOf(reinterpret_cast<const void*>(objAddr))
          != VptrOf(reinterpret_cast<const void*>(ctrlAddr)));

    // Retail's state-machine walker reaches the node by raw offset and raw
    // slot: `node = self + 0x28; (*(void***)node)[0](node, input)`. That call
    // must land on FakeBuildComponent::Step with `this` adjusted BACK to the
    // complete object — the this-adjusting thunk decision 2 says the compiler
    // emits for us instead of retail hand-writing it.
    void* node = reinterpret_cast<void*>(objAddr + 0x28);
    using StepFn = int (*)(void*, int);
    Reset();
    const int rc = reinterpret_cast<StepFn>(VptrOf(node)[0])(node, 7);
    CHECK_EQ(rc, 14);
    CHECK_EQ(g_lastMark, 707);
    CHECK(g_lastSelf == static_cast<void*>(&obj));   // THE THUNK RAN

    // A slot the derived class did NOT override still reaches the interface's
    // own body, and still with the adjusted `this`.
    using EnterFn = char (*)(void*, void*);
    Reset();
    CHECK_EQ((int)reinterpret_cast<EnterFn>(VptrOf(node)[1])(node, nullptr), 1);
    CHECK_EQ(g_lastMark, 501);
    CHECK(g_lastSelf == static_cast<void*>(static_cast<FakeControllerIface*>(&obj)));

    // The primary chain is untouched by the second base: slot 1 is still
    // Startup, at the same index it had in FakeComponent.
    Reset();
    reinterpret_cast<void (*)(void*)>(VptrOf(&obj)[1])(&obj);
    CHECK_EQ(g_lastMark, 601);
    CHECK(g_lastSelf == static_cast<void*>(&obj));
}

// SHAPE 2 — the secondary vptr AFTER the derived class's own fields
// (AttendanceComponent +0x38, NomadComponent +0x58). Not expressible as a
// second base; it is an embedded polymorphic member.
class FakeEmbeddedNode {
public:
    virtual int  Step(int input) { Note(this, 800 + input); return -1; }
    virtual char Enter(void* sm) { Note(this, 801); (void)sm; return 1; }
    virtual ~FakeEmbeddedNode() = default;   // a member CAN have one: nothing
                                             // dispatches through its slot 0.
};

class FakeAttendanceComponent : public FakeComponent {
public:
    uint64_t        _field28 = 0;   // +0x28  (AttendanceComponent.h:47)
    uint64_t        _field30 = 0;   // +0x30
    FakeEmbeddedNode _node;         // +0x38  (AttendanceComponent.h:48)

    ~FakeAttendanceComponent() = default;
};

UNIT_TEST(embedded_member_shape_reaches_a_secondary_vptr_at_0x38) {
    FakeAttendanceComponent obj;

    const auto objAddr  = reinterpret_cast<uintptr_t>(&obj);
    const auto nodeAddr = reinterpret_cast<uintptr_t>(&obj._node);
    CHECK_EQ(nodeAddr - objAddr, (uintptr_t)0x38);
    CHECK_EQ(FieldOffset(&obj, &obj._field28), (uintptr_t)0x28);
    CHECK_EQ(FieldOffset(&obj, &obj._node),    (uintptr_t)0x38);

    // The state machine's raw-offset walk works identically here — but `this`
    // is the MEMBER, not the complete object, which is the whole behavioural
    // difference between the two shapes and the reason each class's own
    // offset assert has to decide which one it is.
    void* node = reinterpret_cast<void*>(objAddr + 0x38);
    using StepFn = int (*)(void*, int);
    Reset();
    CHECK_EQ(reinterpret_cast<StepFn>(VptrOf(node)[0])(node, 5), -1);
    CHECK_EQ(g_lastMark, 805);
    CHECK(g_lastSelf == static_cast<void*>(&obj._node));
    CHECK(g_lastSelf != static_cast<void*>(&obj));
}

// ===========================================================================
// PART C — layout invariants decision 1 keeps for the whole migration.
// ===========================================================================
UNIT_TEST(layout_invariants_hold_for_every_shape_in_this_file) {
    static_assert(sizeof(FakeBase) == 0x08, "Base");
    static_assert(sizeof(FakeExternal) == 0x20, "External");
    static_assert(sizeof(FakeComponent) == 0x28, "Component");

    // Field pins are measured, not `offsetof`-ed: see FieldOffset's comment.
    FakeComponent* c = nullptr;
    {
        FakeBuildComponent probe;
        c = static_cast<FakeComponent*>(&probe);
        CHECK_EQ(FieldOffset(c, &c->_next),  (uintptr_t)0x08);
        CHECK_EQ(FieldOffset(c, &c->_owner), (uintptr_t)0x10);
        CHECK_EQ(FieldOffset(c, &c->_unk18), (uintptr_t)0x18);
        CHECK_EQ(FieldOffset(c, &c->_flags), (uintptr_t)0x20);
    }

    // The +0x28 secondary vptr is pinned at runtime in
    // `two_base_class_puts_the_second_vptr_at_0x28`. What CAN be asserted
    // statically is that adding the second base costs exactly one pointer and
    // nothing else: 0x28 (Component) + 8 (second vptr) + 0x10 (two own
    // qwords) = 0x40.
    static_assert(sizeof(FakeBuildComponent) == 0x40,
                  "a second non-virtual polymorphic base costs one pointer");

    // A vptr-only polymorphic class is neither trivially copyable nor
    // trivially destructible in the standard's sense, which is why
    // `Collection::Array<T>`'s `byteCopy` trait (B32_RULES) must never be fed
    // an Object-derived T by value. Recorded here so the day someone tries it
    // the reason is in a test, not in a crash.
    CHECK(std::is_trivially_copyable<FakeBase>::value == false);
    CHECK(std::is_polymorphic<FakeBase>::value == true);
}

}  // namespace
