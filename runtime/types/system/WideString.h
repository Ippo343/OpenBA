#pragma once
// ============================================================================
// runtime/types/system/WideString.h — RtSystem::WideString (the POD) and the
//                                     real `System::WideString` class
// ============================================================================
//
// WideString is System::String over wchar_t: ONE pointer, the SAME allocation
// header (int32 length in CHARACTERS at data-8, int32 refcount at data-4,
// characters at data+0), and the SAME release idiom including its asymmetry.
// Pinned by ?SetLength@WideString@System@@AEAAXH@Z (0x1800ca9b0):
//     MemAlloc(8 + (len+1)*2, 4); data = block+8; data[-8]=len; data[-4]=1
//
// Modelled as its own type rather than a typedef of String so a caller cannot
// pass one where the other belongs; the two are cast into each other
// deliberately and only inside runtime/src/system/widestring.cpp.
//
// ============================================================================
// ENDGAME B.3.4b SYSTEM (4a) — WHY THIS IS A CLASS, AND WHAT THAT COST
// ============================================================================
// Established from the mangled names in `standalone/rt_export_addrs.inc` before
// a line was moved (B34B_RULES.md §1.1's WideString row, re-read this pass):
//
//   ?SetLength@WideString@System@@AEAAXH@Z          0x1800ca9b0  AEAA  private
//   ?Set@WideString@System@@QEAAXPEB_W@Z            0x1800caf40  QEAA
//   ?AsUTF8@WideString@System@@QEBA?AVString@2@XZ   0x1800cacd0  QEBA  const
//   ??0WideString@System@@QEAA@AEBVString@1@@Z      0x1800cab50  QEAA  ctor
//   ??0WideString@System@@QEAA@PEB_W@Z              0x18000a6e0  QEAA  ctor
//   ??0WideString@System@@QEAA@PEBD@Z               0x1800cb110  QEAA  ctor
//   ??1WideString@System@@QEAA@XZ                   0x18000a730  QEAA  dtor
//   ?StrLength@WideString@System@@SAHPEB_W@Z        0x1800cab20  SA    static
//
// Seven instance members with a `this` plus one `SA` static: retail NAMED this
// class and gave it a real lifecycle, so the established shape applies in full
// — `class WideString { ... }` with the bodies moved and the registered doors
// delegating. It is NOT the `IO::TextCodec` case (receiverless free functions,
// nothing to land).
//
// *** IT DOES GET REAL CONSTRUCTORS AND A REAL DESTRUCTOR, WHICH IS THE ONE
// *** PLACE THIS CLASS DIFFERS FROM `Random`/`Time`/`Color`/`DateTime`. ***
// B34_RULES §4.3's test — "would the member form change which vtable a live
// object wears?" — passes vacuously: there is no vtable. And unlike the four
// value classes, none of this class's four lifecycle exports is UNPORTED: all
// three constructors and the destructor have bodies in this tree today and all
// four are registered. Nothing is invented; the bodies that were already there
// move onto members, exactly as `System::String` did at B.3.1 step 1
// (runtime/types/system/String.h), whose shims this file's doors now mirror.
//
// *** THE POD DOES NOT MOVE AND DOES NOT GET A DESTRUCTOR. ***
// RULE B4b-1 does NOT fire here: the POD is `RtSystem::WideString` and has
// been since Wave 9 — it never occupied the class's name, so nothing is
// renamed and no by-value holder changes spelling. The three files that hold
// one by value and seed it through the ctor door —
//   runtime/src/application/platform_startup_win.cpp:269,383
//   runtime/src/application/application_win.cpp:343,360,362
//   runtime/src/engine/updateloop_loadresources.cpp:158
// and `value_types.cpp`'s `RtSystem::WideString ws{};` in DateAsString —
// keep the POD spelling and keep calling the doors. The POD therefore stays
// TRIVIALLY DESTRUCTIBLE (B34_RULES §1.5 rule 3, the `RtSystem::String`
// double-free lesson) and is asserted so below; the CLASS's destructor exists
// and is asserted NON-trivial, which is what says the two are deliberately
// different and not a drift.
//
// ⚠ THE GREP HAZARD (B34_RULES §7.2) IS ARMED FOR THIS NAME FROM THIS COMMIT
// ON. Inside any `namespace System { ... }` block a bare `WideString` binds the
// REAL CLASS; inside `namespace RtSystem` it binds the POD. Both are one
// `wchar_t*`, so the wrong spelling compiles and behaves until the day one of
// the two gains a member. Spell `RtSystem::WideString` for the POD, always.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <type_traits>

// `AsUTF8` returns `System::String` BY VALUE, so the definition is needed, not
// a declaration. Global scope, above every namespace block, never inside an
// `#if` (B31 hygiene / B34_BRIEFS §A.8).
#include "String.h"

namespace RtSystem {

// ---- the POD ---------------------------------------------------------------
struct WideString {
    wchar_t* data;   // +0x00 — block + 8, or null for "no buffer"
};
static_assert(sizeof(WideString) == 8, "System::WideString must be 8 bytes");
static_assert(sizeof(wchar_t) == 2,
              "WideString is UTF-16: this port assumes a 2-byte wchar_t, which "
              "is what both MSVC and MinGW give on Windows");
static_assert(std::is_trivially_destructible<WideString>::value,
              "B34_RULES 1.5 rule 3: the POD stays trivially destructible -- "
              "three files hold one by value and release it through the door");

}  // namespace RtSystem

namespace System {

// ----------------------------------------------------------------------------
// System::WideString — 8 bytes, NO vtable. One refcounted UTF-16 buffer.
// ----------------------------------------------------------------------------
class WideString {
public:
    // ---- lifecycle ---------------------------------------------------------
    // ??0WideString@System@@QEAA@AEBVString@1@@Z   0x1800cab50
    explicit WideString(const String& src);
    // ??0WideString@System@@QEAA@PEB_W@Z           0x18000a6e0
    explicit WideString(const wchar_t* src);
    // ??0WideString@System@@QEAA@PEBD@Z            0x1800cb110
    explicit WideString(const char* src);
    // ??1WideString@System@@QEAA@XZ                0x18000a730
    // Retail's release idiom, asymmetry included: the FREEING branch leaves
    // `data - 8` in the object, the SHARING branch nulls it. Non-virtual and
    // public (nothing derives from this class and nothing deletes one through
    // a base), and deliberately NOT trivial -- see the assert below.
    ~WideString();

    // RETAIL HAS NO DEFAULT CONSTRUCTOR, NO COPY AND NO ASSIGNMENT for this
    // class: the export table carries exactly the three `??0` rows above, and
    // an uncalled port is an unverified port. The user-declared constructors
    // above already suppress the implicit default one; the copy constructor and
    // copy assignment are left IMPLICIT rather than deleted because deleting
    // them is a decision retail did not make -- nothing in this tree copies a
    // WideString, so neither is ever instantiated.

    // ---- content -----------------------------------------------------------
    // ?Set@WideString@System@@QEAAXPEB_W@Z         0x1800caf40
    // `Set(nullptr)` is a RELEASE, not "become empty" (retail tail-jumps into
    // MemFree), and an equal string is a NO-OP that does not disturb the
    // refcount.
    void Set(const wchar_t* src);

    // ?AsUTF8@WideString@System@@QEBA?AVString@2@XZ  0x1800cacd0
    //
    // ⚠ RETURNS `System::String` BY VALUE, WHICH IS A HIDDEN RETURN SLOT AT THE
    // ABI. Retail's is `String* f(const WideString* this, String* ret)` — RCX =
    // this, RDX = ret. The registered DOOR keeps that two-argument shape and
    // placement-news the member's prvalue into the caller's slot; the MEMBER
    // returns by value, which is what retail's source said. Never register the
    // member directly and never reach it from a raw slot caller — that is
    // `reference_hidden_retslot_wild_write.md`.
    String AsUTF8() const;

    // ?SetLength@WideString@System@@AEAAXH@Z       0x1800ca9b0
    // `AEAA` is the PRIVATE spelling and it is a real export (ord 3902), so it
    // is registered and its door calls it. PUBLIC here for the same reason
    // `Random::NextGroup` is: access control is not ABI. It does NOT write a
    // terminator — both callers write their own.
    void SetLength(int len);

    // ---- statics -----------------------------------------------------------
    // ?StrLength@WideString@System@@SAHPEB_W@Z     0x1800cab20
    // `SA` — a static member, i.e. retail NAMED this class and hung wcslen off
    // it. Not null-tolerant, and kept that way.
    static int StrLength(const wchar_t* s);

    // ---- the 8 bytes -------------------------------------------------------
    // PUBLIC for the same reason `System::String::_data` is: the unconverted
    // tree reads the POD's `data` on these same eight bytes, and `offsetof` has
    // to be well formed.
    wchar_t* _data;   // +0x00
};

static_assert(sizeof(WideString) == 8, "System::WideString is 8 bytes");
static_assert(sizeof(WideString) == sizeof(RtSystem::WideString),
              "the class and the POD stay interchangeable");
static_assert(alignof(WideString) == alignof(RtSystem::WideString),
              "alias alignment");
static_assert(offsetof(WideString, _data) == 0x00, "WideString::_data @ +0x00");
static_assert(offsetof(WideString, _data) == offsetof(RtSystem::WideString, data),
              "alias data");
static_assert(!std::is_polymorphic<WideString>::value,
              "System::WideString must never acquire a vtable: retail has none");
static_assert(std::is_standard_layout<WideString>::value,
              "the doors reach the one field through a reinterpret_cast");
// The two halves of the deliberate asymmetry with the POD, stated as assertions
// so a future "tidy" in either direction fails the build rather than the boot:
static_assert(!std::is_trivially_destructible<WideString>::value,
              "the CLASS owns the release idiom -- if this ever becomes trivial "
              "the ??1 door has stopped releasing anything");
static_assert(!std::is_default_constructible<WideString>::value,
              "retail exports no ??0WideString@System@@QEAA@XZ, and an implicit "
              "default ctor would let `System::WideString w;` leave _data "
              "uninitialised for the destructor to release");

}  // namespace System
