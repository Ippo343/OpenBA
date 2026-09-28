// ============================================================================
// tests/unit/object_vtable_abi_test.cpp — ENDGAME B.3.3 step 2; rewritten at
// B3X W1 (the normal-virtual cutover) as the test of the NEW rule.
//
// THE RULE THIS FILE PROVES
// -------------------------
// Retail's slot 0 of every Object::Base class is MSVC's (vector) deleting
// destructor: ONE slot, `T* (T*, unsigned flags)`. Since B3X W1 the product
// declares `virtual ~T()` instead, and the Itanium ABI spends TWO slots on it:
//
//   compiler slot 0   D1, the complete-object destructor   (retail flags 0)
//   compiler slot 1   D0, the deleting destructor          (retail flags 1:
//                     the chain, then the class-scope `operator delete`)
//
// so retail slot N >= 1 is compiler slot N + 1. That translation is what
// tests/unit/slot_check.h's RetailSlot<T> makes for the ~1,200 slot checks of
// the suite; this file pins it on hand-declared classes, with the retired
// one-slot shape kept as the CONTROL (`FakeRetailShape`, a slot-0
// `DeletingDtor(unsigned)` virtual and no virtual destructor), so the ONE-slot
// difference between the two ABIs stays a tested fact rather than a
// remembered one.
//
// "Interchangeable" still has the two directions the step-2 file tested:
//
//   A. A C body that reads `(*(void***)obj)[N]` off a REAL C++ object must land
//      on that object's override, with `this` unadjusted, for N = the COMPILER
//      slot (retail N + 1 past slot 0) — direction 1: raw reader -> class.
//   B. A REAL C++ virtual call on an object whose vptr has been stamped with a
//      HAND-BUILT array of free functions must land on those free functions,
//      the two destructor entries included (direction 2: class -> hand table).
//      It still proves the call path reads nothing before the address point.
//
// WHAT IS *NOT* PROVEN HERE
// -------------------------
// Nothing about MSVC. The retail DLL is gone (B31_RULES "the one fact that
// changes the rules"): every caller and callee is this same GCC now. The only
// binary facts still binding are OBJECT LAYOUT and RETAIL SLOT ORDER.
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
// Under the new rule Base's compiler table is FOUR entries: D1, D0, TypeName,
// Type.
//
// NOTE ON NAMES: retail calls slot 2 `Type()`, not `GetTypeId()`; the static
// companion `?TypeId@…@@SAIXZ` is a separate, NON-virtual member. See
// docs/runtime_port/endgame_b/B33_RULES.md §1.
// ============================================================================
#include "unit.h"
#include "slot_check.h"

#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <type_traits>

namespace {

// ---------------------------------------------------------------------------
// Observation channel. The hand-built bodies and the class overrides all write
// here so a test can say WHICH body ran, not merely that something returned.
// ---------------------------------------------------------------------------
const void* g_lastSelf = nullptr;
int         g_lastMark = 0;
int         g_extDtors = 0;          // ~FakeExternal runs
int         g_frees = 0;             // FakeBase::operator delete calls
const void* g_lastFree = nullptr;

void Note(const void* self, int mark) { g_lastSelf = self; g_lastMark = mark; }
void Reset() { g_lastSelf = nullptr; g_lastMark = 0; g_extDtors = 0; g_frees = 0; g_lastFree = nullptr; }

// ===========================================================================
// PART A — `Object::Base`'s shape: slot 0 is a public virtual destructor (two
// compiler slots), with a class-scope operator delete; the remaining retail
// slots follow in retail order, one index up.
// ===========================================================================

class FakeBase {
public:
    // retail slot 0 (+0x00) 0x1800065b0 -> compiler slots 0 (D1) and 1 (D0).
    virtual ~FakeBase() { Note(this, 100); }
    // retail slot 1 (+0x08) _purecall in Base -> compiler slot 2. Declared
    //                  non-pure here so a FakeBase can be instantiated.
    virtual const char* TypeName() const { Note(this, 101); return "Base"; }
    // retail slot 2 (+0x10) 0x1800c0380 `XOR EAX,EAX; RET` -> compiler slot 3.
    virtual uint32_t Type() const { Note(this, 102); return 0; }

    // Object::Base's class-scope operator delete is MemFree; this one records
    // what D0 hands it and frees with std::free (the heap objects below come
    // from std::malloc).
    static void operator delete(void* p) { ++g_frees; g_lastFree = p; std::free(p); }
};

// Only the vptr, exactly like `RtObject::Base` (runtime/types/object/TypeInfo.h).
static_assert(sizeof(FakeBase) == 8, "Object::Base is one vtable pointer");
static_assert(std::is_polymorphic<FakeBase>::value, "FakeBase must be polymorphic");
static_assert(std::has_virtual_destructor<FakeBase>::value,
              "B3X W1: retail slot 0 is a virtual destructor");

// A direct subclass in `Object::External`'s shape: overrides Base's slots and
// appends its own, exactly as `??_7External@Object@@6B@` does.
class FakeExternal : public FakeBase {
public:
    ~FakeExternal() override { ++g_extDtors; Note(this, 200); }
    const char* TypeName() const override { Note(this, 201); return "External"; }
    uint32_t    Type() const override     { Note(this, 202); return 0xEE11u; }

    // retail slot 3 (+0x18) — retail Goal's override is `MOV EAX,0x10000; RET`,
    // i.e. `virtual int GetFileVersion() const`. Compiler slot 4.
    virtual int GetFileVersion() const { Note(this, 203); return 0x10000; }
    // retail slot 4 (+0x20) — retail External's is `MOV EAX,1; RET` (Compile),
    // carried as a numbered slot so the indices after it stay right. Compiler 5.
    virtual int Slot4() const { Note(this, 204); return 1; }

    uint64_t _path = 0;      // mirrors External's +0x08 System::Filename
    uint64_t _refsData = 0;  // +0x10
    uint64_t _refsSize = 0;  // +0x18
};

static_assert(sizeof(FakeExternal) == 0x20, "External is 0x20 bytes (External.h)");

// THE CONTROL: the retired ONE-slot MSVC-compatible shape (ENDGAME §B.1
// decision 2 as it stood until B3X W1) — slot 0 is a normal virtual with the
// scalar deleting destructor's signature and there is NO virtual destructor.
// Nothing depends on this class; it exists so the rule carries its own
// evidence: TypeName is at 1 here and at 2 in FakeBase.
class FakeRetailShape {
public:
    virtual FakeRetailShape* DeletingDtor(unsigned flags) { Note(this, 900 + (int)flags); return this; }
    virtual const char* TypeName() const { Note(this, 901); return "retail"; }
    virtual uint32_t    Type() const     { Note(this, 902); return 0xDEAD; }

protected:
    ~FakeRetailShape() = default;
};
static_assert(!std::has_virtual_destructor<FakeRetailShape>::value, "the control has none");

// --- raw-slot helpers, the shape every raw reader in the tree uses ---------

void** VptrOf(const void* obj) { return *reinterpret_cast<void** const*>(obj); }

// A polymorphic class is not standard-layout, and `offsetof` on one is only
// "conditionally supported" — GCC accepts it but emits -Winvalid-offsetof. The
// tests here measure the offset from two live addresses instead, which is
// warning-free and pins the same fact.
uintptr_t FieldOffset(const void* obj, const void* field) {
    return reinterpret_cast<uintptr_t>(field) - reinterpret_cast<uintptr_t>(obj);
}

// `n` is a COMPILER index on purpose: this file states the translation.
template <class Fn>
Fn SlotAs(const void* obj, int n) { return reinterpret_cast<Fn>(VptrOf(obj)[n]); }

using DtorFn         = void (*)(void*);                 // D1 / D0: `this` only
using RetailDtorFn   = void* (*)(void*, unsigned);      // the control's slot 0
using TypeNameFn     = const char* (*)(const void*);
using TypeFn         = uint32_t (*)(const void*);
using IntSlotFn      = int (*)(const void*);

// ---------------------------------------------------------------------------
// DIRECTION 1 — a raw reader reaching a real class through raw slots.
//
// The function pointer GCC put at compiler index N, called with `this` passed
// unchanged, is the override for the retail slot N - 1 (N >= 2), and the two
// destructor entries sit at 0 and 1. Any other entry INSIDE the table (RTTI or
// offset-to-top after the address point, a vcall thunk) would shift the marks.
// This is the direction-1 proof for every product raw `vt[N]` read that
// RetailSlot<T> translates.
// ---------------------------------------------------------------------------
UNIT_TEST(base_raw_slot_dispatch_reaches_the_override) {
    FakeExternal ext;
    FakeBase* asBase = &ext;

    // The helper every test uses agrees with the literal indices below.
    CHECK_EQ(RetailSlot<FakeExternal>(1), 2L);
    CHECK_EQ(RetailSlot<FakeExternal>(4), 5L);
    CHECK_EQ(RetailSlot<FakeExternal>(0), 0L);

    Reset();
    CHECK_STREQ(SlotAs<TypeNameFn>(asBase, 2)(asBase), "External");
    CHECK_EQ(g_lastMark, 201);

    Reset();
    CHECK_EQ(SlotAs<TypeFn>(asBase, 3)(asBase), 0xEE11u);
    CHECK_EQ(g_lastMark, 202);

    Reset();
    CHECK_EQ(SlotAs<IntSlotFn>(asBase, 4)(asBase), 0x10000);
    CHECK_EQ(g_lastMark, 203);

    Reset();
    CHECK_EQ(SlotAs<IntSlotFn>(asBase, 5)(asBase), 1);
    CHECK_EQ(g_lastMark, 204);

    // `this` is NOT adjusted for a single-inheritance chain: the pointer the
    // slot receives is the object address itself. Every ported C body relies on
    // this when it passes `self` straight through.
    CHECK(static_cast<void*>(asBase) == static_cast<void*>(&ext));

    // Retail slot 0 (was: flags 3 through slot 0). D1 at compiler slot 0 runs
    // ~FakeExternal then ~FakeBase and frees nothing — on an object
    // placement-new'd into a buffer and never destroyed again.
    alignas(FakeExternal) unsigned char buf[sizeof(FakeExternal)];
    FakeExternal* d1obj = ::new (static_cast<void*>(buf)) FakeExternal();
    Reset();
    SlotAs<DtorFn>(d1obj, 0)(d1obj);
    CHECK_EQ(g_extDtors, 1);
    CHECK_EQ(g_lastMark, 100);                      // the base's body ran last
    CHECK(g_lastSelf == d1obj);
    CHECK_EQ(g_frees, 0);

    // D0 at compiler slot 1 runs the same chain and hands the block to the
    // class-scope operator delete.
    FakeExternal* d0obj = ::new (std::malloc(sizeof(FakeExternal))) FakeExternal();
    Reset();
    SlotAs<DtorFn>(d0obj, 1)(d0obj);
    CHECK_EQ(g_extDtors, 1);
    CHECK_EQ(g_lastMark, 100);
    CHECK_EQ(g_frees, 1);
    CHECK(g_lastFree == d0obj);
}

// A `FakeBase` that is exactly a Base, not a subclass, resolves to Base's own
// bodies: four compiler entries for the three retail slots. The control shows
// the retired one-slot shape next to it.
UNIT_TEST(base_own_vtable_has_exactly_the_three_retail_slots) {
    struct Plain : FakeBase {};
    Plain b;

    Reset();
    CHECK_STREQ(SlotAs<TypeNameFn>(&b, 2)(&b), "Base");
    CHECK_EQ(g_lastMark, 101);
    Reset();
    CHECK_EQ(SlotAs<TypeFn>(&b, 3)(&b), 0u);
    CHECK_EQ(g_lastMark, 102);
    // D1 and D0 are two different bodies.
    CHECK(VptrOf(&b)[0] != VptrOf(&b)[1]);
    alignas(Plain) unsigned char buf[sizeof(Plain)];
    Plain* once = ::new (static_cast<void*>(buf)) Plain();
    Reset();
    SlotAs<DtorFn>(once, 0)(once);
    CHECK_EQ(g_lastMark, 100);
    CHECK_EQ(g_frees, 0);

    // Measured on the members themselves: Itanium spends TWO slots on
    // `virtual ~T()` (complete + deleting), so the first user-declared virtual
    // lands at index 2 ...
    CHECK_EQ(CompilerSlot(&FakeBase::TypeName), 2L);
    CHECK_EQ(CompilerSlot(&FakeBase::Type), 3L);
    // ... and in the CONTROL — the retired MSVC-compatible shape, a one-slot
    // deleting destructor and no virtual destructor — at index 1: exactly one
    // slot apart.
    CHECK_EQ(CompilerSlot(&FakeRetailShape::DeletingDtor), 0L);
    CHECK_EQ(CompilerSlot(&FakeRetailShape::TypeName), 1L);
    CHECK_EQ(CompilerSlot(&FakeRetailShape::Type), 2L);
    CHECK_EQ(CompilerSlot(&FakeBase::TypeName) - CompilerSlot(&FakeRetailShape::TypeName), 1L);
    CHECK_EQ(RetailSlot<FakeRetailShape>(1), 1L);   // RetailSlot does not shift it

    struct PlainRetail : FakeRetailShape {};
    PlainRetail v;
    Reset();
    CHECK_STREQ(SlotAs<TypeNameFn>(&v, 1)(&v), "retail");
    CHECK_EQ(g_lastMark, 901);
    CHECK_EQ(SlotAs<TypeFn>(&v, 2)(&v), 0xDEADu);
    Reset();
    SlotAs<RetailDtorFn>(&v, 0)(&v, 0u);            // the control's slot 0, flags 0
    CHECK_EQ(g_lastMark, 900);
}

// ---------------------------------------------------------------------------
// DIRECTION 2 — a real class dispatching through a HAND-BUILT array.
//
// An ordinary C++ virtual call (the destructor included) on an object whose
// vptr came from a hand-built array lands in that array's free functions. The
// array is built with the `(self, …)` shape; both destructor entries take only
// `self` and return nothing.
// ---------------------------------------------------------------------------
namespace hand {
void        D1(void* self)             { Note(self, 300); }
void        D0(void* self)             { Note(self, 310); }
const char* TypeName(const void* self) { Note(self, 301); return "minted"; }
uint32_t    Type(const void* self)     { Note(self, 302); return 0x4D1Du; }
}  // namespace hand

UNIT_TEST(real_class_virtual_call_lands_in_a_hand_built_table) {
    struct Plain : FakeBase {};
    alignas(Plain) unsigned char buf[sizeof(Plain)];
    Plain* obj = ::new (static_cast<void*>(buf)) Plain();

    void* const original = VptrOf(obj);

    // The table the new rule produces: four entries — D1, D0, then the retail
    // slots 1 and 2 — nothing before the address point that the call path reads.
    void* handTable[4] = {
        reinterpret_cast<void*>(&hand::D1),
        reinterpret_cast<void*>(&hand::D0),
        reinterpret_cast<void*>(&hand::TypeName),
        reinterpret_cast<void*>(&hand::Type),
    };
    *reinterpret_cast<void***>(obj) = handTable;

    FakeBase* p = obj;

    Reset();
    CHECK_STREQ(p->TypeName(), "minted");     // ordinary C++ virtual call
    CHECK_EQ(g_lastMark, 301);
    CHECK(g_lastSelf == obj);

    Reset();
    CHECK_EQ(p->Type(), 0x4D1Du);
    CHECK_EQ(g_lastMark, 302);

    // A dispatching destructor call (was: `p->DeletingDtor(0u)`) reads slot 0.
    Reset();
    p->~FakeBase();
    CHECK_EQ(g_lastMark, 300);
    CHECK(g_lastSelf == obj);

    // `delete p` reads slot 1 and leaves the free to it: the caller calls no
    // operator delete of its own (hand::D0 frees nothing).
    Reset();
    delete p;
    CHECK_EQ(g_lastMark, 310);
    CHECK_EQ(g_frees, 0);

    // Put the compiler's own table back, then destroy the object for real,
    // once: D1 runs ~FakeBase, nothing is freed (a buffer).
    *reinterpret_cast<void***>(obj) = reinterpret_cast<void**>(original);
    Reset();
    CHECK_STREQ(p->TypeName(), "Base");
    CHECK_EQ(g_lastMark, 101);
    Reset();
    p->~FakeBase();
    CHECK_EQ(g_lastMark, 100);
    CHECK_EQ(g_frees, 0);
}

// A COPY of the compiler's table is as good as the compiler's table: copy the
// live table (six entries: D1, D0 and retail slots 1-4), patch one slot, stamp
// it back. (The runtime copy site this used to cite, code/Game/Game/tornado.cpp,
// is gone; the fact still holds.)
UNIT_TEST(copied_compiler_vtable_is_interchangeable_slot_for_slot) {
    FakeExternal a;
    FakeExternal b;

    void** src = VptrOf(&a);
    void*  patched[6];
    for (int i = 0; i < 6; ++i) patched[i] = src[i];
    patched[RetailSlot<FakeExternal>(1)] = reinterpret_cast<void*>(&hand::TypeName);   // patch ONE slot
    CHECK(patched[0] != patched[1]);                                      // D1 != D0

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
// immediately after the 0x28-byte `Component` base, and
// `engine/types/BuildComponent.h` asserts the same offset for the largest
// member of that family: `class BuildComponent : public Component, public
// ControllerComponent`.
//
// THE FINDING THIS TEST RECORDS (see B33_RULES.md §5): +0x28 is the COMMON
// case, not the rule. `engine/types/AttendanceComponent.h` pins the secondary
// vptr at +0x38 and `engine/types/NomadComponent.h` at +0x58, with the class's
// OWN data at +0x28..  Two non-virtual bases are always laid out back to back,
// so no base-class spelling can produce those two layouts: they are an
// embedded polymorphic MEMBER after some of the derived class's own fields.
//
// B3X W1: the PRIMARY chain (Component, an Object::Base class) flips to a
// virtual destructor; the SECONDARY interface (the Controller, retail slot 0
// `FindState`) is EXEMPT and has none. This is the one place the flip/exempt
// boundary inside ONE object is proven: the primary table shifts, the
// secondary table keeps its indices.
// ===========================================================================

class FakeComponent {
public:
    virtual ~FakeComponent() { Note(this, 400); }   // retail slot 0 -> D1/D0
    virtual void  Startup()  { Note(this, 401); }   // retail 1 -> compiler 2
    virtual void  Shutdown() { Note(this, 402); }   // retail 2 -> compiler 3

    void*    _next  = nullptr;   // +0x08
    void*    _owner = nullptr;   // +0x10
    void*    _unk18 = nullptr;   // +0x18
    uint32_t _flags = 0x80;      // +0x20
    uint32_t _pad24 = 0;         // +0x24
};
static_assert(sizeof(FakeComponent) == 0x28, "Component is 0x28 (engine/types/Component.h)");

// The controller/state-node interface reached through the SECOND vptr (EXEMPT:
// no destructor slot). Slot names from code/Game/Component/controller_live_base.h.
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
};

UNIT_TEST(two_base_class_puts_the_second_vptr_at_0x28) {
    FakeBuildComponent obj;

    const auto objAddr  = reinterpret_cast<uintptr_t>(&obj);
    const auto baseAddr = reinterpret_cast<uintptr_t>(static_cast<FakeComponent*>(&obj));
    const auto ctrlAddr = reinterpret_cast<uintptr_t>(static_cast<FakeControllerIface*>(&obj));

    // The primary base sits at +0x00 and the secondary at +0x28, exactly as
    // ControllerComponent.h asserts for the retail object.
    CHECK_EQ(baseAddr - objAddr, (uintptr_t)0x00);
    CHECK_EQ(ctrlAddr - objAddr, (uintptr_t)0x28);

    // And the secondary vptr is a DIFFERENT table from the primary one.
    CHECK(VptrOf(reinterpret_cast<const void*>(objAddr))
          != VptrOf(reinterpret_cast<const void*>(ctrlAddr)));

    // Retail's state-machine walker reaches the node by raw offset and raw
    // slot: `node = self + 0x28; (*(void***)node)[0](node, input)`. That call
    // must land on FakeBuildComponent::Step with `this` adjusted BACK to the
    // complete object — the this-adjusting thunk the compiler emits. The
    // secondary table did NOT shift: the exempt interface has no destructor
    // slot, and the derived class's virtual destructor adds none to it.
    void* node = reinterpret_cast<void*>(objAddr + 0x28);
    using StepFn = int (*)(void*, int);
    Reset();
    const int rc = reinterpret_cast<StepFn>(VptrOf(node)[0])(node, 7);
    CHECK_EQ(rc, 14);
    CHECK_EQ(g_lastMark, 707);
    CHECK(g_lastSelf == static_cast<void*>(&obj));   // THE THUNK RAN
    CHECK_EQ(RetailSlot<FakeControllerIface>(1), 1L);

    // A slot the derived class did NOT override still reaches the interface's
    // own body, and still with the adjusted `this`.
    using EnterFn = char (*)(void*, void*);
    Reset();
    CHECK_EQ((int)reinterpret_cast<EnterFn>(VptrOf(node)[1])(node, nullptr), 1);
    CHECK_EQ(g_lastMark, 501);
    CHECK(g_lastSelf == static_cast<void*>(static_cast<FakeControllerIface*>(&obj)));

    // The primary chain DID shift: retail slot 1 (Startup) is compiler slot 2,
    // behind FakeComponent's D1/D0 — the same index it has in FakeComponent.
    CHECK_EQ(RetailSlot<FakeComponent>(1), 2L);
    Reset();
    reinterpret_cast<void (*)(void*)>(VptrOf(&obj)[2])(&obj);
    CHECK_EQ(g_lastMark, 601);
    CHECK(g_lastSelf == static_cast<void*>(&obj));
    CHECK_EQ(CompilerSlot(&FakeComponent::Startup), 2L);
    CHECK_EQ(CompilerSlot(&FakeControllerIface::Step), 0L);
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
    uint64_t        _field28 = 0;   // +0x28  (AttendanceComponent.h)
    uint64_t        _field30 = 0;   // +0x30
    FakeEmbeddedNode _node;         // +0x38  (AttendanceComponent.h)
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
// PART C — layout invariants decision 1 keeps (a virtual destructor adds no
// bytes: the vptr was already there).
// ===========================================================================
UNIT_TEST(layout_invariants_hold_for_every_shape_in_this_file) {
    static_assert(sizeof(FakeBase) == 0x08, "Base");
    static_assert(sizeof(FakeExternal) == 0x20, "External");
    static_assert(sizeof(FakeComponent) == 0x28, "Component");
    static_assert(sizeof(FakeRetailShape) == 0x08, "the control: same bytes, one slot fewer");

    // B3X W1: the three flipped fakes have a virtual destructor; the exempt
    // interface and the control do not.
    CHECK(std::has_virtual_destructor<FakeBase>::value);
    CHECK(std::has_virtual_destructor<FakeExternal>::value);
    CHECK(std::has_virtual_destructor<FakeComponent>::value);
    CHECK(!std::has_virtual_destructor<FakeControllerIface>::value);
    CHECK(!std::has_virtual_destructor<FakeRetailShape>::value);

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
    // an Object-derived T by value.
    CHECK(std::is_trivially_copyable<FakeBase>::value == false);
    CHECK(std::is_polymorphic<FakeBase>::value == true);
}

}  // namespace
