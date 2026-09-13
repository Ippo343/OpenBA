// ============================================================================
// runtime/src/ui/element.cpp — Wave 7 lane A — UI::Element + ContainerElement
// ============================================================================
//
// Ports 21 of the 23 `UI::Element` / `UI::ContainerElement` exports our tree
// calls (PLAN.md §4.2 wave 7, §6). The recovered layout and the evidence for
// every offset are in runtime/types/ui/Element.h; the per-function write-up,
// the oracle argument for each row and everything this lane could NOT verify
// are in docs/runtime_port/WAVE7_LANE_A.md.
//
//   | Export (short form)             | Retail      | Ord  | Ported | Oracle  |
//   |---------------------------------|-------------|------|--------|---------|
//   | ??0Element (ctor)               | 0x180115e50 |  171 | yes    | build   |
//   | ??1Element (dtor)               | 0x1801161a0 |  584 | yes    | none    |
//     ^ Wave 9 lane UIF. `none` is the literal oracle disposition and is
//       argued for at the body; the "TWO HOLD-BACKS" section below is the
//       ORIGINAL Wave 7 text, kept because reason (b) still stands.
//   | ?AddChild@                      | 0x180115df0 | 1537 | yes    | shadow  |
//   | ?Child@                         | 0x180035540 | 1693 | yes    | replay  |
//   | ?ChildCount@                    | 0x180035530 | 1695 | yes    | replay  |
//   | ?ComputeLayout@                 | 0x1801168d0 | 1763 | yes    | thunk   |
//   | ?Draw@                          | 0x1801168f0 | 2243 | yes    | thunk   |
//   | ?FindElement@                   | 0x180115650 | 2347 | yes    | replay  |
//   | ?FindElementType@               | 0x180115760 | 2349 | yes    | replay  |
//   | ?GetAlignedPosition@ (member)   | 0x180115ff0 | 2442 | yes    | outputs |
//   | ?GetPaddedSize@                 | 0x180115f10 | 2830 | yes    | outputs |
//   | ?GetUserData@                   | 0x180115550 | 2997 | yes    | replay  |
//   | ?Hide@                          | 0x180035830 | 3056 | yes    | replay  |
//   | ?RemoveChildren@                | 0x180115570 | 3552 | yes    | none    |
//     ^ Wave 9 lane B3, in runtime/src/ui/widgets_layout.cpp, NOT this file.
//   | ?SetElementId@                  | 0x180115820 | 3875 | yes    | replay  |
//   | ?SetFlagsOnOff@                 | 0x180116160 | 3883 | yes    | replay  |
//   | ?SetName@                       | 0x1801154f0 | 3927 | yes    | shadow  |
//   | ?SetNeedsSizeCalc@              | 0x180035470 | 3928 | yes    | replay  |
//   | ?SetPadding@                    | 0x180115480 | 3935 | yes    | replay  |
//   | ?SetToolTip@                    | 0x180035740 | 3989 | yes    | replay  |
//   | ?SetToolTipString@              | 0x1800356e0 | 3990 | yes    | shadow  |
//   | ?SetUserData@                   | 0x180115560 | 3998 | yes    | replay  |
//   | ?SetOffsets@ContainerElement@   | 0x1800cc1e0 | 3932 | yes    | shadow  |
//
// All 23 mangled names were checked against the shipped export directory of
// C:\GOG Games\Banished\Runtime-x64.dll with a pefile-style reader BEFORE any
// body was written (reference_resolveexport_phantom_names.md: a name the DLL
// does not contain resolves to NULL in silence, the port still registers, and
// the census still counts it). All 23 are present; the addresses and ordinals
// above are that query's output, not Ghidra's guess. The six Wave 7 phantoms
// are all in lanes B, C and D.
//
// ---------------------------------------------------------------------------
// THE VTABLE QUESTION, WHICH IS THE ONE THAT MATTERS FOR THIS CLASS
// ---------------------------------------------------------------------------
// `Element` is polymorphic with a deep derived hierarchy, so PLAN §4.3 risk 3
// ("port classes whole") is live here in a way it was not for Wave 6's
// non-polymorphic `Video::Context`. It is defused, not ignored:
//
//   * `??_7Element@UI@@6B@` is an EXPORTED data symbol (retail 0x180156160),
//     and the ported ctor stamps THAT address, resolved by name at run time,
//     rather than minting a table of its own. So a half-ported hierarchy never
//     puts two vtables in play: every object built by our ctor and every object
//     built by the DLL's ctor carry the identical pointer, and a virtual call
//     from either side lands on the same twelve slots.
//   * No method in this lane is registered as a vtable slot. Registering
//     redirects OUR direct callers only; the DLL's own internal near-CALLs and
//     its widgets' vtables still reach the DLL's bodies. That is fine here
//     because every ported body is a pure function of the object's fields.
//   * The two functions that would have made this dangerous — the dtor and
//     `RemoveChildren`, which DESTROY objects — are held back (see below).
//     WAVE 9: BOTH HAVE SINCE LANDED. `RemoveChildren` in lane B3, the dtor in
//     lane UIF. The section below is preserved as the Wave 7 record, and its
//     reason (b) for the dtor — "no oracle" — is still true of the port that
//     now exists; what changed is that a cutover with no DLL makes the
//     alternative a NULL CALL rather than a fallback. Read §0 of
//     docs/runtime_port/WAVE9_LANE_UIF.md before trusting reason (a).
//
// ---------------------------------------------------------------------------
// THE TWO HOLD-BACKS  (WAVE 7 RECORD — BOTH NOW LANDED, see the note above)
// ---------------------------------------------------------------------------
// `??1Element@UI@@UEAA@XZ` (0x1801161a0). Held back for two independent
// reasons, either of which is sufficient:
//   (a) NOTHING IN OUR TREE CALLS IT. `fn_Element_Dtor`
//       (engine/funcs/engine_funcs.h) is assigned and never invoked; a
//       grep across code/, engine/, standalone/, runtime/ and dx11/ finds the
//       declaration, the assignment and no call. A registered port on a path
//       nothing executes reports `calls=0 divergences=0` forever, which READS
//       as a pass (reference_uncalled_port_is_not_verified.md — a wrong
//       Quaternion::operator* survived all of Wave 1 exactly that way).
//   (b) It is the most destructive function in the class and it has no oracle:
//       it releases a System::String, destroys every child through vtable slot
//       0, destroys the border element, frees the child array through the
//       allocator, and unregisters the object's handle in
//       `Object::InternalFactory::_singleton`. Replaying any of that runs the
//       destruction twice.
//
// `?RemoveChildren@Element@UI@@QEAAXXZ` (0x180115570). Fully reversed — the
// body is transcribed in WAVE7_LANE_A.md §4 for whoever lands it — and held
// back because it calls each child's vtable slot 0 as `fn(child, 1)`, the
// SCALAR DELETING destructor. That frees the child. There is no oracle for it:
// a replay destroys every child a second time, and a shadow predicate can only
// observe that the array ended up empty, which is true whether or not the
// children were correctly destroyed. Its live caller is `fn_Element_RemoveChildren`
// (nine sites under code/Game/Dialog/), so this is a real, deliberate cost:
// nine call sites keep going to the DLL. An unverified port that frees objects
// is a worse outcome than a hold-back (WAVE2 allocator verdict).
//
// PORTABILITY: no <windows.h>, per PLAN §4.1 decision 1.
// ============================================================================

#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/ui/Element.h"
#include "../../types/system/String.h"

// ----------------------------------------------------------------------------
// ResolveAddr / ResolveExport, forward-declared rather than pulled in from
// standalone/resolve.h — same reasoning rt_port.h gives for declaring
// RegisterRuntimeExport locally: resolve.h drags in <windows.h>, and PLAN §4.1
// requires runtime/src/ to stay platform-neutral. These declarations must stay
// byte-identical to resolve.h's; the linker checks them against it. (Precedent:
// runtime/src/achievement/achievement_manager.cpp:69-71.)
// ----------------------------------------------------------------------------
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);
void*     ResolveExport(const char* mangled_name);

using UI::Element;
using ::UIString;   // runtime/types/ui/String.h, owned by lane F

namespace {

constexpr size_t kElementSize = sizeof(Element);   // 0xC0, static_assert'd in the header

// ============================================================================
// Mangled names.
//
// The registrations at the bottom of this file deliberately repeat each name as
// its own string literal instead of using these constants:
// tools/runtime_census.py cannot see through a NAMED CONSTANT and would
// under-count the lane, which is how Wave 3's Object count stood at 21/41 when
// it was really 27/41 (reference_census_cannot_see_nonliteral_registrations.md).
// ============================================================================
const char* const kCtor         = "??0Element@UI@@QEAA@XZ";
const char* const kDtor         = "??1Element@UI@@UEAA@XZ";
const char* const kAddChild     = "?AddChild@Element@UI@@QEAAXV?$InternalPtr@VElement@UI@@@Object@@@Z";
const char* const kChild        = "?Child@Element@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@H@Z";
const char* const kChildCount   = "?ChildCount@Element@UI@@QEBAHXZ";
const char* const kFindElement  = "?FindElement@Element@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@AEBVString@System@@@Z";
const char* const kFindElemType = "?FindElementType@Element@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@I@Z";
const char* const kGetAligned   = "?GetAlignedPosition@Element@UI@@QEAAXHHHHHHAEAH000_N1@Z";
const char* const kGetPadded    = "?GetPaddedSize@Element@UI@@QEAAXAEAH0@Z";
const char* const kGetUserData  = "?GetUserData@Element@UI@@QEAAPEAVBase@Object@@XZ";
const char* const kHide         = "?Hide@Element@UI@@QEAAX_N@Z";
const char* const kSetElementId = "?SetElementId@Element@UI@@QEAAXH@Z";
const char* const kSetFlags     = "?SetFlagsOnOff@Element@UI@@QEAAXII@Z";
const char* const kSetNeedsCalc = "?SetNeedsSizeCalc@Element@UI@@QEAAXXZ";
const char* const kSetPadding   = "?SetPadding@Element@UI@@QEAAXHHHH@Z";
const char* const kSetToolTip   = "?SetToolTip@Element@UI@@QEAAXVString@2@@Z";
const char* const kSetUserData  = "?SetUserData@Element@UI@@QEAAXPEAVBase@Object@@@Z";
const char* const kSetOffsets   = "?SetOffsets@ContainerElement@UI@@QEAAXHH@Z";
// WAVE 9 LANE UIC
const char* const kAdjustUL     = "?AdjustUpperLeft@Element@UI@@QEAAXHH@Z";

// ============================================================================
// Dependencies this lane CALLS rather than reimplements.
//
// Two different resolution rules, on purpose (Wave 4 lane C's rule, restated in
// achievement_manager.cpp:80):
//
//  * ResolveExport(name) — "whatever is registered". Used where retail called
//    another engine function that a sibling wave may have ported: our body
//    should then use the port, exactly as every other one of our call sites
//    does. That is what Wave 6 lane A did with Math::Matrix::Inverse.
//  * ResolveAddr(Module::Runtime, addr) — "the address door". Used for the
//    Collection::Array helpers and the other unexported internals, which are
//    header-only template instantiations with NO exported symbol to ask for.
//
//    WAVE 9 LANE UIF CORRECTION: this bullet used to read "always the DLL",
//    and that has been FALSE for the whole runtime port. Read the
//    implementation — standalone/resolve_standalone.cpp:471 — `ResolveAddr`
//    consults `RtAddrReg()` BEFORE it touches the module handle and before it
//    applies the ASLR delta, so an address another lane has registered comes
//    back as OUR code (reference_resolveaddr_consults_port_registry_first.md,
//    and lane K's runtime/src/object/deleting_dtor.cpp says the same thing in
//    its own words). The correction matters here rather than being pedantic:
//    `??1Element` below reaches RemoveChildren, ~Array and ~Internal through
//    this door precisely BECAUSE it lands on the ports, which is what lets the
//    destructor close its cutover row instead of moving it one frame deeper.
//    Believing the old sentence would have made that port look useless.
//
// Every one of these is looked up ONCE and cached. ResolveExport
// are GetProcAddress and cost like a syscall — two of them in the body of
// System::Time cost 5.6 points of sim time in Wave 2 and were invisible to both
// profilers (reference_rtoriginal_is_getprocaddress.md).
// ============================================================================

// ??0Internal@Object@@QEAA@XZ — the base ctor Element::Element calls first
// (retail 0x1800bfd10). It stamps ??_7Base@Object@@6B@ then
// ??_7Internal@Object@@6B@, writes the handle pair at +0x08/+0x0c, and
// REGISTERS the object in Object::InternalFactory::_singleton.
using InternalCtorFn = void* (*)(void*);
InternalCtorFn ObjectInternalCtor() {
    static InternalCtorFn fn =
        reinterpret_cast<InternalCtorFn>(ResolveExport("??0Internal@Object@@QEAA@XZ"));
    return fn;
}

// ?StrCompare@String@System@@SAHPEBD0@Z (0x1800c98a0) — the byte compare
// FindElement uses. Static, two char* arguments.
using StrCompareFn = int (*)(const char*, const char*);
StrCompareFn StrCompare() {
    static StrCompareFn fn =
        reinterpret_cast<StrCompareFn>(ResolveExport("?StrCompare@String@System@@SAHPEBD0@Z"));
    return fn;
}

// ?MemFree@MemoryAllocator@System@@SAXPEAX@Z (0x1800c7ad0) — the static,
// null-safe free the String release path calls. Resolved by name, so an armed
// Wave 2 allocator port would be used here too.
using MemFreeFn = void (*)(void*);
MemFreeFn MemFree() {
    static MemFreeFn fn =
        reinterpret_cast<MemFreeFn>(ResolveExport("?MemFree@MemoryAllocator@System@@SAXPEAX@Z"));
    return fn;
}

// ?IsDerivedFrom@Base@Object@@QEBA_NI@Z (0x1800c0390) — ContainerElement::
// SetOffsets' type test. NOTE for whoever reads a crash log: this is the
// function standalone/main.cpp:518's crash handler recognises by the offset
// +0xC0390, because retail calls it on a possibly-null child (see SetOffsets).
using IsDerivedFromFn = bool (*)(const void*, uint32_t);
IsDerivedFromFn IsDerivedFrom() {
    static IsDerivedFromFn fn =
        reinterpret_cast<IsDerivedFromFn>(ResolveExport("?IsDerivedFrom@Base@Object@@QEBA_NI@Z"));
    return fn;
}

// The static 17-argument overload the member GetAlignedPosition forwards to.
// It is exported, and it is NOT in this lane's 23 — it stays the DLL's.
//   ?GetAlignedPosition@Element@UI@@SAXW4Alignment@ElementDescription@2@HHHHHHHHHHAEAH111_N2@Z
// WAVE 10: THIS IS NOW OURS. The note above used to end "it stays the DLL's",
// which was true of Wave 7 lane A's scope and had quietly become the reason a
// pure integer leaf sat unported while the member that calls it was verified.
// The body is below; this door still goes through ResolveExport, which consults
// the port registry FIRST, so the member automatically reaches our static.
const char* const kGetAlignedStatic =
    "?GetAlignedPosition@Element@UI@@SAXW4Alignment@ElementDescription@2@HHHHHHHHHHAEAH111_N2@Z";

using GetAlignedStaticFn = void (*)(int alignment, int, int, int, int, int, int,
                                    int leftPad, int rightPad, int topPad, int bottomPad,
                                    int*, int*, int*, int*, bool, bool);
GetAlignedStaticFn GetAlignedStatic() {
    static GetAlignedStaticFn fn =
        reinterpret_cast<GetAlignedStaticFn>(ResolveExport(kGetAlignedStatic));
    return fn;
}

// Collection::Array<Element*>::SetSize(int, const T&). Header-only template:
// NO export exists, so this is the one place this file uses
// ResolveAddr(Module::Runtime, …).
//
// *** WAVE 9 LANE Q, 2026-09-07: 0x180076900 IS NOW REGISTERED. ***
// This comment used to say "safe here -- and only here -- because nothing
// registers 0x180076900, so the port registry (consulted first) does not
// intercept it". It DOES intercept it now: lane Q registers this address in
// runtime/src/collection/array.cpp, so this call reaches our SetSize. That is
// the intended cutover behaviour, and the body is verified against the shipped
// one by runtime/src/collection/array_probe.cpp -- but the old sentence was
// load-bearing for a reader, so it is corrected here rather than deleted.
//
// This is the instantiation for 8-BYTE elements, and it is the exact address
// AddChild itself calls; a different Array<T> gets a different copy of the
// template, so this address must not be reused for anything else. Read out of
// the shipped body at 0x180076900:
//   grow to `newSize`, tripling the capacity when the growth is +1 from a
//   non-empty array, MemAlloc(capacity*8, 8), copy the old elements, fill the
//   new tail from *fillValue, MemFree the old buffer under the allocator's CS.
using ArraySetSizeFillFn = void (*)(void*, int, const void*);
ArraySetSizeFillFn ArraySetSizeFill() {
    static ArraySetSizeFillFn fn = reinterpret_cast<ArraySetSizeFillFn>(
        ResolveAddr(Module::Runtime, 0x180076900));
    return fn;
}

// The exported data symbol the ctor stamps. Resolved by NAME, so our objects
// and the DLL's carry the identical vtable pointer.
void* ElementVTable() {
    static void* vt = ResolveExport("??_7Element@UI@@6B@");
    return vt;
}

// The two Object::BaseTypeInfo statics SetOffsets reads. Their FIRST DWORD is
// the type id; retail loads it as `MOV EDX,[0x1801f9d68]`.
uint32_t ClipRegionTypeId() {
    const uint32_t* p = RT_DATA_SLOT(const uint32_t*,
        "?_typeInfo@ClipRegionElement@UI@@0VBaseTypeInfo@Object@@A");
    return p ? *p : 0u;
}
uint32_t ScrollTypeId() {
    const uint32_t* p = RT_DATA_SLOT(const uint32_t*,
        "?_typeInfo@ScrollElement@UI@@0VBaseTypeInfo@Object@@A");
    return p ? *p : 0u;
}

// Video::Device::_singleton (exported, 0x1801fe7a8). GetPaddedSize reads the
// screen size out of the pointed-to Device at +0x04 / +0x08.
void* VideoDeviceSingleton() {
    void** p = RT_DATA_SLOT(void**,
        "?_singleton@Device@Video@@0PEAV12@EA");
    return p ? *p : nullptr;
}

// ---- vtable helpers --------------------------------------------------------
// Slot indices are proven in runtime/types/ui/Element.h: the table is twelve
// slots because ??_7ElementDescription@UI@@6B@ (0x1801561c0) sits exactly 0x60
// bytes past ??_7Element@UI@@6B@ (0x180156160).
constexpr int kSlotScalarDtor   = 0;   // +0x00, called as fn(this, 1)
constexpr int kSlotType         = 2;   // +0x10
constexpr int kSlotComputeSize  = 4;   // +0x20, _purecall in Element itself
constexpr int kSlotComputeLayout = 5;  // +0x28
constexpr int kSlotDraw         = 6;   // +0x30

template <typename Fn>
inline Fn VSlot(const void* obj, int index) {
    return reinterpret_cast<Fn>((*reinterpret_cast<void* const* const*>(obj))[index]);
}

// ---- the parent dirty walk -------------------------------------------------
// `?SetNeedsSizeCalc@` (0x180035470) verbatim, and the tail every mutator in
// this class shares. Note the shape: the OR happens BEFORE the null test, so it
// always runs at least once on `self` — a do/while, not a while.
inline void NeedsSizeCalcWalk(Element* e) {
    do {
        e->_flags |= UI::kElementFlagNeedsSizeCalc;
        e = e->_parent;
    } while (e != nullptr);
}

} // namespace

// ============================================================================
// The ports
// ============================================================================
// Every body is byte-faithful to the retail disassembly — read as
// DISASSEMBLY, not as Ghidra's C, because that C drops dereferences and prints
// MOVSXD as a zero extension (reference_ghidra_c_output_vs_disassembly.md; three
// Wave 6 defects, one a crash, came from trusting it). Where retail derefs
// without a null check, so do we: CLAUDE.md's rule is that retail's unguarded
// derefs are load-bearing, and a guard skips the behaviour rather than
// restoring it.

// ---- ??0Element@UI@@QEAA@XZ  0x180115e50 -----------------------------------
// Returns `this` in RAX, as MSVC ctors do.
//
// The base ctor is CALLED, not reimplemented. `Object::Internal::Internal`
// registers the object in the global handle table; reproducing that here would
// mean this lane owned the handle-table protocol, which is Wave 3's. Calling it
// also keeps the vtable stamping ORDER retail has: ??_7Base, then ??_7Internal,
// then ??_7Element.
//
// FIELDS THE CTOR DOES NOT WRITE: +0x54, +0x60, +0x64, +0x7c, +0x8c. Retail
// leaves them holding whatever the allocator returned, and so do we — writing
// them would be a divergence, and comparing them would be the Wave 6 mistake of
// diffing bytes neither side wrote
// (reference_oracle_must_not_compare_unwritten_bytes.md).
void* RtUi_Element_Ctor(Element* self) {

    InternalCtorFn base = ObjectInternalCtor();
    if (base) base(self);

    self->_vtable           = ElementVTable();
    self->_name             = nullptr;
    self->_leftPad          = 0;
    self->_rightPad         = 0;
    self->_topPad           = 0;
    self->_bottomPad        = 0;
    self->_minWidth         = 0;
    self->_minHeight        = 0;
    self->_alignment        = 3;                                  // literal 3 at 0x180115e88
    self->_flags            = UI::kElementFlagNeedsSizeCalc;      // 0x08000000
    self->_color            = 0xFFFFFFFFu;
    self->_mouseCursorIndex = 0;
    self->_toolTipDialog    = nullptr;
    self->_toolTip.table    = nullptr;
    self->_toolTip.index    = -1;                                 // +0x54 deliberately untouched
    self->_toolTipString    = nullptr;
    self->_layoutLeft       = 0;
    self->_layoutTop        = 0;
    self->_layoutRight      = 0;
    self->_layoutBottom     = 0;
    self->_unknown78        = -1;                                 // +0x7c deliberately untouched
    self->_userData         = nullptr;
    self->_elementId        = -1;                                 // +0x8c deliberately untouched
    self->_dialogParent     = nullptr;
    self->_parent           = nullptr;
    self->_children._data   = nullptr;
    self->_children._size   = 0;
    self->_children._capacity = 0;
    self->_border           = nullptr;
    self->_description      = nullptr;

    // ---- BUILD ORACLE ------------------------------------------------------
    // A ctor cannot be replayed onto a copy of the object it just built: the
    // second run would register a SECOND handle in
    // Object::InternalFactory::_singleton pointing at our scratch buffer, and
    // that entry would dangle the moment the buffer is reused.
    //
    // So instead: let the DLL BUILD ITS OWN object in the scratch, compare the
    // two byte for byte, and then DESTROY the scratch with the DLL's own dtor
    // so its handle is unregistered again. On a freshly constructed Element
    // that dtor is a sequence of no-ops (no name, no tooltip string, no border,
    // empty child array), so it cleans up the handle and nothing else.
    //
    // Excluded from the comparison, with the reason for each:
    //   +0x00        the VTABLE POINTER. WAVE 10B LANE ELEM. Both ctors write
    //                it and they write DIFFERENT VALUES ON PURPOSE the moment
    //                `??_7Element@UI@@6B@` is a promoted mint row: ours writes
    //                the table our tree publishes, theirs writes the DLL's.
    //                That disagreement IS the promotion, so comparing the field
    //                reports a permanent divergence for a correct port — the
    //                shape reference_oracle_must_not_compare_unwritten_bytes.md
    //                warns about, one step over (a field both sides write, that
    //                they are *required* to write differently).
    //                Measured, not predicted: with the minted
    //                ??_7Element@UI@@6B@ row this site reported exactly one
    //                DIVERGENCE, at +0x0, ours=<exe .data> theirs=<Runtime
    //                .rdata>, and nothing else in the run changed.
    //                NOTHING IS LOST BY EXCLUDING IT: the field is checked
    //                immediately below by its own predicate. That check is a
    //                SELF-CHECK, not an oracle — it asks "did the ctor write
    //                the table this tree publishes", which is the invariant —
    //                and it still catches the two ways this field actually
    //                breaks (a null resolve, and a stale cached pointer).
    //   +0x08, +0x0c  the handle index/id — necessarily different for two
    //                distinct live objects; that they differ is correct.
    //   +0x54, +0x60, +0x64, +0x7c, +0x8c  never written by either ctor.
    return self;
}

// ---- ?AddChild@  0x180115df0 -----------------------------------------------
// `child` is an `Object::InternalPtr<Element>` passed BY VALUE, which MSVC
// hands over as a pointer to the caller's 8-byte temporary (the disassembly's
// first act is `MOV RAX,[RDX]`).
//
// SHADOW ORACLE, not replay: the array grow at 0x180076900 allocates, and
// replaying it would push the same child a second time and reallocate the
// buffer. The predicate below names EVERY term it checks, so a failure message
// says which one broke (reference_shadow_predicate_must_name_its_failing_term.md).
void RtUi_Element_AddChild(Element* self, void** child) {

    const int  oldSize = self->_children._size;
    Element* const c   = reinterpret_cast<Element*>(*child);

    if (c) c->_parent = self;
    ArraySetSizeFillFn grow = ArraySetSizeFill();
    if (grow) grow(&self->_children, self->_children._size + 1, child);
    NeedsSizeCalcWalk(self);

}

// ---- ?Child@  0x180035540 --------------------------------------------------
// Hidden return slot in RDX (the InternalPtr the caller wants filled), index in
// R8D. NO BOUNDS CHECK, and the index is SIGN-extended (MOVSXD RCX,R8D) — a
// negative index reads before the buffer, exactly as retail does.
void* RtUi_Element_Child(Element* self, void** ret, int index) {

    *ret = self->_children._data[static_cast<intptr_t>(index)];

    return ret;
}

// ---- ?ChildCount@  0x180035530 ---------------------------------------------
int RtUi_Element_ChildCount(const Element* self) {
    const int ours = self->_children._size;
    return ours;
}

// ---- ?ComputeLayout@  0x1801168d0  (vtable slot 5) -------------------------
// A tail-jump into the BORDER element's slot 5. `Element` itself lays nothing
// out; every widget that has geometry overrides this. Oracle: the registration
// -time thunk decode (see VerifyBorderThunk). There is no per-call comparison,
// because running the original as well would lay the border out twice.
void RtUi_Element_ComputeLayout(Element* self, int x, int y, int w, int h) {
    Element* border = reinterpret_cast<Element*>(self->_border);
    if (!border) return;
    using Fn = void (*)(Element*, int, int, int, int);
    VSlot<Fn>(border, kSlotComputeLayout)(border, x, y, w, h);
}

// ---- ?Draw@  0x1801168f0  (vtable slot 6, const) ---------------------------
void RtUi_Element_Draw(const Element* self, void* spriteContext, unsigned char alpha) {
    Element* border = reinterpret_cast<Element*>(self->_border);
    if (!border) return;
    using Fn = void (*)(const Element*, void*, unsigned char);
    VSlot<Fn>(border, kSlotDraw)(border, spriteContext, alpha);
}

// ---- ?FindElement@  0x180115650 --------------------------------------------
// Depth-first search by name, self first. `name` is a `const System::String&`,
// i.e. a POINTER to the 8-byte String, whose one field is the character body;
// the length lives at body[-8] and the refcount at body[-4]
// (runtime/types/system/String.h).
//
// Two faithfulness details that a tidied version would drop:
//   * An EMPTY search string never matches anything, not even an unnamed
//     element: the `lq == 0` test jumps straight to the child scan.
//   * Two elements with equal-length names are compared with StrCompare only
//     when THIS element's name is non-null and non-empty; a null name of
//     length 0 against a length-0 query would have matched, but the query
//     length test above has already excluded that case.
//
// The recursion goes through this private helper.
namespace {
void* FindElementImpl(Element* self, void** ret, const char* const* name) {
    const char* q = *name;
    if (q && RtSystem::StringLength(q) != 0) {
        const char* mine = reinterpret_cast<const char*>(self->_name);
        const int32_t lm = mine ? RtSystem::StringLength(mine) : 0;
        if (lm == RtSystem::StringLength(q)) {
            bool equal;
            if (!mine || RtSystem::StringLength(mine) == 0) {
                equal = true;
            } else {
                StrCompareFn cmp = StrCompare();
                equal = cmp && cmp(mine, q) == 0;
            }
            if (equal) { *ret = self; return ret; }
        }
    }
    for (int i = 0; i < self->_children._size; ++i) {
        Element* c = self->_children._data[i];
        if (!c) continue;
        void* found = nullptr;
        FindElementImpl(c, &found, name);
        if (found) { *ret = found; return ret; }
    }
    *ret = nullptr;
    return ret;
}
} // namespace

void* RtUi_Element_FindElement(Element* self, void** ret, const char* const* name) {
    FindElementImpl(self, ret, name);
    return ret;
}

// ---- ?FindElementType@  0x180115760 ----------------------------------------
// Same shape, keyed on the virtual `Type()` (slot 2) rather than the name.
// EXACT equality, not IsDerivedFrom — a subclass of the requested type is NOT
// a hit.
namespace {
void* FindElementTypeImpl(Element* self, void** ret, uint32_t typeId) {
    using TypeFn = uint32_t (*)(const Element*);
    if (VSlot<TypeFn>(self, kSlotType)(self) == typeId) { *ret = self; return ret; }
    for (int i = 0; i < self->_children._size; ++i) {
        Element* c = self->_children._data[i];
        if (!c) continue;
        void* found = nullptr;
        FindElementTypeImpl(c, &found, typeId);
        if (found) { *ret = found; return ret; }
    }
    *ret = nullptr;
    return ret;
}
} // namespace

void* RtUi_Element_FindElementType(Element* self, void** ret, uint32_t typeId) {
    FindElementTypeImpl(self, ret, typeId);
    return ret;
}

// ---- ?GetAlignedPosition@ (STATIC, 17 args)  0x1801158e0 -------------------
//
// WAVE 10. Wave 7 lane A ported the MEMBER overload and left this one to the
// DLL ("It is exported, and it is NOT in this lane's 23 -- it stays the DLL's"),
// which made it a real dependency of our own port rather than an absent one.
// It is a pure integer leaf: no calls, no allocation, no globals, four out
// parameters. Nothing about it was ever hard; it was simply out of scope.
//
// It places a box of size (*outW x *outH) inside a container of size
// (containerW x containerH), honouring four pads and a 3x3 alignment, and
// optionally STRETCHES the box to the available space first.
//
// THE ALIGNMENT ENUM IS A ROW-MAJOR 3x3, and this is read off the jump table at
// 0x180115a14 rather than guessed. The X axis switches on `alignment - 1` with
// `JA -> default` for anything above 7, and the eight table entries cycle
// center / right / left; the Y axis is a compare chain (<3 top, 3..5 middle,
// 6..8 bottom, >8 top). Cross-multiplying the two gives exactly:
//
//        0 TopLeft      1 TopCenter      2 TopRight
//        3 MiddleLeft   4 MiddleCenter   5 MiddleRight
//        6 BottomLeft   7 BottomCenter   8 BottomRight
//
// and ANY value outside 0..8 lands on left/top, because it misses both the
// jump table's range check and the Y chain's. That agreement between two
// independently-encoded dispatches is what makes the mapping trustworthy.
//
// TWO FAITHFULNESS TRAPS, both of which a tidied port gets wrong:
//
//  * The centring halves are `SAR reg,1` -- an ARITHMETIC SHIFT, which rounds
//    toward negative infinity. `/ 2` rounds toward zero, so for a negative
//    available size the two differ by one pixel. `>>` on a signed int is the
//    faithful spelling (GCC guarantees arithmetic shift; C++20 mandates it).
//
//  * `*outW` / `*outH` are IN/OUT. They are read as the box's size, may be
//    OVERWRITTEN by the stretch, and are then read AGAIN by the placement --
//    which therefore sees the stretched value. The order is load-bearing.
// `useDivide` exists ONLY for element_aligned_probe.cpp's negative control: it
// swaps the arithmetic shift for a division, which is identical for
// non-negative values and off by one for odd negatives. It is the mistake a
// tidied port makes, and the probe uses it to prove it can SEE that mistake.
// Every real caller passes false.
void RtUi_Element_GetAlignedPositionStatic_Body(
        int alignment, int x, int y, int containerW, int containerH,
        int offsetX, int offsetY,
        int leftPad, int rightPad, int topPad, int bottomPad,
        int* outX, int* outY, int* outW, int* outH,
        bool stretchX, bool stretchY, bool useDivide = false) {
    // SAR, not divide -- see the banner. `half()` keeps the two spellings in
    // one place so the control cannot drift from the real arithmetic.
    auto half = [useDivide](int v) { return useDivide ? (v / 2) : (v >> 1); };
    // Cursor = origin + offset, then pulled back inside the container if the
    // box would overhang. Retail compares against the CURRENT *outW/*outH.
    int curX = x + offsetX;
    int curY = y + offsetY;

    if (containerW < *outW + curX) curX = containerW - *outW;
    if (containerH < *outH + curY) curY = containerH - *outH;

    curX += leftPad;
    curY += topPad;

    // Space left between the cursor and the far pad.
    const int availW = (containerW - rightPad)  - curX;
    const int availH = (containerH - bottomPad) - curY;

    // The stretch, BEFORE the placement reads the size back.
    if (stretchX) *outW = availW;
    if (stretchY) *outH = availH;

    // ---- X: center for 1/4/7, right for 2/5/8, left otherwise -------------
    switch (alignment) {
    case 1: case 4: case 7:
        *outX = (half(availW) - half(*outW)) + curX;
        break;
    case 2: case 5: case 8:
        *outX = (containerW - rightPad) - *outW;
        break;
    default:
        *outX = curX;
        break;
    }

    // ---- Y: middle for 3..5, bottom for 6..8, top otherwise ---------------
    if (alignment >= 3 && alignment <= 5) {
        *outY = (half(availH) - half(*outH)) + curY;
    } else if (alignment >= 6 && alignment <= 8) {
        *outY = (containerH - bottomPad) - *outH;
    } else {
        *outY = curY;
    }
}

// ORACLE -- outputs only, and the inputs must be SNAPSHOT first.
//
// This is a pure function, so a straight A/B is available: run ours into the
// real outputs, run retail into scratch ints, compare all four. The one thing
// that would quietly break it is that `outW`/`outH` are IN/OUT -- by the time
// ours returns it has already overwritten the size retail needs as its INPUT.
// Handing retail the post-state would make a CORRECT port read as a divergence
// on every stretched call, which is exactly
// reference_oracle_must_snapshot_aliased_inputs.md. So the in-values are
// captured before ours runs and replayed into retail's scratch.
void RtUi_Element_GetAlignedPositionStatic(
        int alignment, int x, int y, int containerW, int containerH,
        int offsetX, int offsetY,
        int leftPad, int rightPad, int topPad, int bottomPad,
        int* outX, int* outY, int* outW, int* outH,
        bool stretchX, bool stretchY) {

    // Snapshot BEFORE ours runs -- see the oracle note above.
    const int inW = outW ? *outW : 0;
    const int inH = outH ? *outH : 0;

    RtUi_Element_GetAlignedPositionStatic_Body(
        alignment, x, y, containerW, containerH, offsetX, offsetY,
        leftPad, rightPad, topPad, bottomPad,
        outX, outY, outW, outH, stretchX, stretchY);

}

// ---- ?GetAlignedPosition@ (member)  0x180115ff0 ----------------------------
// Twelve parameters after `this`: six ints, four int& outputs, two bools. The
// body recomputes the element's size through virtual slot 4, publishes it into
// the third and fourth outputs, then forwards everything to the STATIC
// 17-argument overload (0x1801158e0), which stays the DLL's.
//
// The two bools are each OR'd with a sticky flag bit before being forwarded:
// `stretchX` with bit 2 and `stretchY` with bit 3. Getting that pairing
// backwards is the obvious way to break this function, so it is worth naming
// exactly where it comes from: the LAST parameter (+0x100 in the frame) pairs
// with flag 0x8 and lands in the LAST outgoing slot; the eleventh (+0xf8) pairs
// with flag 0x4 and lands one slot earlier.
//
// ORACLE — OUTPUTS ONLY, and this is the weakest oracle in the lane. The
// function writes nothing to `this` itself; everything it changes, it changes
// through virtual slot 4 (ComputeSize), which we cannot replay onto a scratch
// copy because a derived ComputeSize walks the REAL children. So the original
// is run against the REAL object into a second set of output ints and only the
// outputs are compared. That is sound only because ComputeSize is idempotent:
// its implementations early-out on flag bit 27, which our call has already
// cleared, and even an unconditional one is a pure function of the unchanged
// child tree. See WAVE7_LANE_A.md §7.
void RtUi_Element_GetAlignedPosition(Element* self, int a, int b, int c, int d,
                                     int e, int f, int* outX, int* outY,
                                     int* outW, int* outH, bool stretchX, bool stretchY) {

    using ComputeSizeFn = void (*)(Element*);
    VSlot<ComputeSizeFn>(self, kSlotComputeSize)(self);

    *outW = self->_layoutWidth;
    *outH = self->_layoutHeight;

    const bool bx = stretchX || (self->_flags & UI::kElementFlagAlignArg1) != 0;
    const bool by = stretchY || (self->_flags & UI::kElementFlagAlignArg2) != 0;

    GetAlignedStaticFn st = GetAlignedStatic();
    if (st) {
        st(self->_alignment, a, b, c, d, e, f,
           self->_leftPad, self->_rightPad, self->_topPad, self->_bottomPad,
           outX, outY, outW, outH, bx, by);
    }

}

// ---- ?GetPaddedSize@  0x180115f10 ------------------------------------------
// Size + padding, floored by the minimum size, then optionally REPLACED
// outright by a fraction of the screen when flag bits 4 / 5 are set.
//
// Faithfulness details, all of them observable:
//   * The additions are `layout + right + left` and `layout + bottom + top`,
//     in that order, in 32-bit — an overflowing pad wraps the same way.
//   * The minimum-size floor is `if (out < min) out = min`, applied to the
//     PADDED value, not to the layout value.
//   * The screen-size override walks to the ROOT element and reads the root's
//     `_dialogParent`, not this element's.
//   * The second flag test RE-READS `self->_flags` from memory (retail keeps
//     `LEA RDX,[RBX+0x34]` alive across the root walk, which clobbers RBX).
//     Reproduced as a second load rather than reusing the cached value.
//   * Both conversions are `CVTTSS2SI` — truncation toward zero, not rounding.
//
// ORACLE — OUTPUTS ONLY, same argument as GetAlignedPosition above.
void RtUi_Element_GetPaddedSize(Element* self, int* outW, int* outH) {

    using ComputeSizeFn = void (*)(Element*);
    VSlot<ComputeSizeFn>(self, kSlotComputeSize)(self);

    *outW = self->_layoutWidth  + self->_rightPad  + self->_leftPad;
    *outH = self->_layoutHeight + self->_bottomPad + self->_topPad;
    if (*outW < self->_minWidth)  *outW = self->_minWidth;
    if (*outH < self->_minHeight) *outH = self->_minHeight;

    const uint32_t flags = self->_flags;
    if (flags & (UI::kElementFlagSizeToScreenH | UI::kElementFlagSizeToScreenW)) {
        Element* root = self;
        while (root->_parent != nullptr) root = root->_parent;
        void* dialogParent = root->_dialogParent;
        const uint8_t* device = reinterpret_cast<const uint8_t*>(VideoDeviceSingleton());

        if (flags & UI::kElementFlagSizeToScreenW) {
            const int32_t screenW = *reinterpret_cast<const int32_t*>(device + 0x04);
            const uint8_t* scaleOwner = *reinterpret_cast<const uint8_t* const*>(
                reinterpret_cast<const uint8_t*>(dialogParent) + 0x10);
            const float scale = *reinterpret_cast<const float*>(scaleOwner + 0xdc);
            *outW = static_cast<int>(static_cast<float>(screenW) * scale);
        }
        // RE-READ, not the cached `flags` — see the note above.
        if (self->_flags & UI::kElementFlagSizeToScreenH) {
            const int32_t screenH = *reinterpret_cast<const int32_t*>(device + 0x08);
            const uint8_t* scaleOwner = *reinterpret_cast<const uint8_t* const*>(
                reinterpret_cast<const uint8_t*>(dialogParent) + 0x10);
            const float scale = *reinterpret_cast<const float*>(scaleOwner + 0xdc);
            *outH = static_cast<int>(static_cast<float>(screenH) * scale);
        }
    }

}

// ---- ?GetUserData@  0x180115550 --------------------------------------------
void* RtUi_Element_GetUserData(Element* self) {
    void* ours = self->_userData;
    return ours;
}

// ---- ?Hide@  0x180035830 ---------------------------------------------------
// Sets or clears flag bit 0 (which means HIDDEN — `?IsVisible@` is its
// negation) and runs the dirty walk only if the bit actually changed. Retail
// builds this branchlessly out of SETZ/SETNZ/NOT; the mask is
// `hide ? 0xFFFFFFFF : ~1`, so the AND is a no-op on the hide path.
void RtUi_Element_Hide(Element* self, bool hide) {

    const uint32_t old = self->_flags;
    const uint32_t setMask   = hide ? 1u : 0u;
    const uint32_t clearMask = hide ? 0xFFFFFFFFu : ~1u;
    const uint32_t next = (old | setMask) & clearMask;
    self->_flags = next;
    if ((next ^ old) & 1u) NeedsSizeCalcWalk(self);

}

// ---- ?SetElementId@  0x180115820 -------------------------------------------
void RtUi_Element_SetElementId(Element* self, int id) {
    self->_elementId = id;
}

// ---- ?SetFlagsOnOff@  0x180116160 ------------------------------------------
// `flags = (flags | on) & ~off`, then the dirty walk if BIT 0 changed — only
// bit 0, not any other bit. The parameter names in
// engine/funcs/engine_funcs.h WERE "hideFlags, showFlags", which happens to
// read right for the common case of bit 0 (bit 0 means HIDDEN) but inverts for
// every other bit; this lane renamed them to (setFlags, clearFlags).
// See WAVE7_LANE_A.md §6.
void RtUi_Element_SetFlagsOnOff(Element* self, uint32_t on, uint32_t off) {

    const uint32_t old = self->_flags;
    const uint32_t next = (old | on) & ~off;
    self->_flags = next;
    if ((next ^ old) & 1u) NeedsSizeCalcWalk(self);

}

// ---- ?SetName@  0x1801154f0 ------------------------------------------------
// System::String release/acquire. `str` is a `const System::String&` — a
// pointer to the 8-byte String, whose one field is the character body.
//
// The release path is retail's, including the detail that the FIELD is used as
// the scratch for the pointer arithmetic: `this->_name -= 8` and then
// `MemFree(this->_name)`, which leaves a freed pointer in the field for the two
// instructions before it is overwritten. Reproduced, because the object is
// observable from a signal handler and because "tidying" it would hide the fact
// that the field is briefly dangling.
//
// NO REPLAY ORACLE IS POSSIBLE. The scratch copy would hold the OLD string
// pointer, so the replayed original would decrement that string's refcount a
// SECOND time — and if our body already dropped it to zero and freed it, the
// replay writes into freed memory. Refcounts are global state; PLAN §4.3 risk 2
// calls this the shadow case. So the check below is a postcondition predicate
// that names each term it tests.
void RtUi_Element_SetName(Element* self, void* const* str) {

    char* const incoming = reinterpret_cast<char*>(*str);
    char* const previous = reinterpret_cast<char*>(self->_name);
    int32_t prevRefAfter = 0;
    bool    freedPrev    = false;

    if (previous != incoming) {
        if (previous) {
            if (--RtSystem::StringRefCount(previous) == 0) {
                self->_name = previous - 8;
                MemFreeFn free = MemFree();
                if (free) free(self->_name);
                freedPrev = true;
            } else {
                prevRefAfter = RtSystem::StringRefCount(previous);
                self->_name = nullptr;
            }
        }
        self->_name = incoming;
        if (incoming) ++RtSystem::StringRefCount(incoming);
    }

}

// ---- ?SetNeedsSizeCalc@  0x180035470 ---------------------------------------
void RtUi_Element_SetNeedsSizeCalc(Element* self) {
    NeedsSizeCalcWalk(self);
}

// ---- ?SetPadding@  0x180115480 ---------------------------------------------
// *** THE PARAMETER ORDER IS (left, RIGHT, TOP, bottom). ***
// Retail stores EDX->+0x18, R8D->+0x1c, R9D->+0x20, [RSP+0x28]->+0x24, and the
// one-line accessors pin which offset is which: GetLeftPad (0x1800355e0) reads
// +0x18, GetRightPad (0x180035600) +0x1c, GetTopPad (0x1800355f0) +0x20,
// GetBottomPad (0x180035610) +0x24. `fn_Element_SetPadding_t`
// (engine/funcs/engine_funcs.h) DECLARED (left, top, right, bottom); this lane
// corrected it in place. See WAVE7_LANE_A.md §6.
//
// Note also that SetPadding does NOT run the dirty walk, although the closely
// related SetMinSize (0x1801154a0) does. Not an omission — the walk is simply
// not in the shipped body.
void RtUi_Element_SetPadding(Element* self, int left, int right, int top, int bottom) {

    self->_leftPad   = left;
    self->_rightPad  = right;
    self->_bottomPad = bottom;   // retail writes +0x24 BEFORE +0x20; same bytes
    self->_topPad    = top;

}

// ---- ?SetToolTip@  0x180035740 ---------------------------------------------
// `tip` is a `UI::String` BY VALUE — 16 bytes, so MSVC passes a pointer to the
// caller's temporary. UI::String is NOT refcounted (contrast SetToolTipString
// below), so this is a plain 16-byte copy and the replay oracle applies.
//
// Three faithfulness details:
//   * The `_toolTipDialog != 0` test is evaluated FIRST, before the copy — so
//     the branch below uses the dialog pointer as it was on entry, and the
//     copy happens either way.
//   * The second store is a full QWORD at +0x50, so it also writes the 4 bytes
//     at +0x54 that the ctor leaves alone. Reproduced with a 16-byte copy.
//
//     *** DO NOT "FIX" THIS COPY TO kUIStringWrittenBytes (0x0C). *** That
//     constant is the written extent of a UI::String built by its own three
//     CONSTRUCTORS, which never touch pad0C — it is the right bound for
//     COMPARING two constructed UI::Strings (lane F's Sprite.h has the same
//     trap, and lane E hit it). It is the WRONG bound here, because this is a
//     COPY, not a comparison, and the shipped body copies sixteen bytes:
//         180035745  MOV RAX,[RDX]      ; 180035748  MOV [RCX+0x48],RAX
//         18003574c  MOV RAX,[RDX+0x8]  ; 180035750  MOV [RCX+0x50],RAX
//     Copying only 0x0C would drop +0x54, which retail writes, and the replay
//     oracle would report it — correctly — as a divergence at +0x54.
//
//     The same distinction runs through this lane's two oracles and both are
//     already on the right side of it: the ctor's BUILD oracle EXCLUDES +0x54
//     (neither ctor writes it), and this function's replay INCLUDES it (both
//     implementations write it). "Never written" is a property of the writer,
//     not of the type.
//   * Both branches end with `if ((flags ^ old) & 1) walk`, but neither
//     `|= 0x90000` nor `&= ~0x80000` can change bit 0, so that walk is
//     UNREACHABLE in the shipped code. Kept, because it is in the shipped code
//     and because the replay would report its absence as a divergence the day
//     someone changes the masks.
void RtUi_Element_SetToolTip(Element* self, const UIString* tip) {

    const bool hadDialog = (self->_toolTipDialog != nullptr);
    std::memcpy(&self->_toolTip, tip, sizeof(UIString));

    const uint32_t old = self->_flags;
    uint32_t next;
    if (hadDialog && self->_toolTip.table != nullptr && self->_toolTip.index >= 0 &&
        (old & UI::kElementFlagDisabled) == 0) {
        next = old | (UI::kElementFlagToolTip16 | UI::kElementFlagToolTipValid);
    } else {
        next = old & ~static_cast<uint32_t>(UI::kElementFlagToolTipValid);
    }
    self->_flags = next;
    if ((next ^ old) & 1u) NeedsSizeCalcWalk(self);

}

// ---- ?SetToolTipString@  0x1800356e0 ---------------------------------------
// Byte-for-byte the same release/acquire as SetName, on +0x58 instead of +0x10.
// Same shadow oracle, for the same reason.
void RtUi_Element_SetToolTipString(Element* self, void* const* str) {

    char* const incoming = reinterpret_cast<char*>(*str);
    char* const previous = reinterpret_cast<char*>(self->_toolTipString);
    int32_t prevRefAfter = 0;
    bool    freedPrev    = false;

    if (previous != incoming) {
        if (previous) {
            if (--RtSystem::StringRefCount(previous) == 0) {
                self->_toolTipString = previous - 8;
                MemFreeFn free = MemFree();
                if (free) free(self->_toolTipString);
                freedPrev = true;
            } else {
                prevRefAfter = RtSystem::StringRefCount(previous);
                self->_toolTipString = nullptr;
            }
        }
        self->_toolTipString = incoming;
        if (incoming) ++RtSystem::StringRefCount(incoming);
    }

}

// ---- ?SetUserData@  0x180115560 --------------------------------------------
void RtUi_Element_SetUserData(Element* self, void* userData) {
    self->_userData = userData;
}

// ---- ?SetOffsets@ContainerElement@UI@@QEAAXHH@Z  0x1800cc1e0 ---------------
// A ContainerElement built by `UI::ContainerDescription::CreateInstance`
// (0x1800ccb90) has three children: a ClipRegionElement and up to two
// ScrollElements. This pushes the scroll offsets into them.
//
// *** THE FIRST WRITE IS UNGUARDED AND RETAIL MEANS IT. *** Retail does not
// null-check `children[0]` before calling IsDerivedFrom on it, and it does not
// null-check the RESULT before storing through it: when child 0 is not a
// ClipRegionElement, `R9` is 0 and `MOV [R9+0xd0],EDX` writes to 0xd0. That is
// exactly the fault standalone/main.cpp:518's crash handler recognises as
// "IsDerivedFrom null-this at Runtime+0xC0390". CLAUDE.md's rule applies: the
// deref is load-bearing (it asserts the invariant that a ContainerElement's
// first child IS its clip region), and a guard would skip the behaviour rather
// than restore it. Reproduced.
//
// Retail re-loads `_children._data` before EACH of the three child reads rather
// than caching it. Reproduced, because the loads are in the shipped code.
//
// SHADOW ORACLE. Ours writes at most four locations, all of them inside objects
// SOMEBODY ELSE owns, so a replay onto a scratch `this` would still walk the
// real children and write to them a second time. The predicate below runs the
// original against the real object AFTER ours and asserts that every location
// ours wrote still holds the value ours put there. That catches a swapped x/y,
// a wrong constant and a wrong target object; it does NOT catch a write theirs
// makes at an offset ours never touched. Stated as a limitation in
// WAVE7_LANE_A.md §7 rather than dressed up.
void RtUi_ContainerElement_SetOffsets(Element* self, int x, int y) {

    IsDerivedFromFn isA = IsDerivedFrom();

    Element* child0 = self->_children._data[0];
    Element* clip = (isA && isA(child0, ClipRegionTypeId())) ? child0 : nullptr;
    uint8_t* clipBytes = reinterpret_cast<uint8_t*>(clip);
    *reinterpret_cast<int32_t*>(clipBytes + 0xd0) = x;
    *reinterpret_cast<int32_t*>(clipBytes + 0xd4) = y;

    Element* child1 = self->_children._data[1];
    Element* vscroll = nullptr;
    if (child1 && isA && isA(child1, ScrollTypeId())) vscroll = child1;

    Element* child2 = self->_children._data[2];
    Element* hscroll = nullptr;
    if (child2 && isA && isA(child2, ScrollTypeId())) hscroll = child2;

    if (vscroll) {
        *reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(vscroll) + 0x160) =
            static_cast<float>(y);
    }
    if (hscroll) {
        *reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(hscroll) + 0x160) =
            static_cast<float>(x);
    }

}

// ============================================================================
// Registration
// ============================================================================
// Each ported function is registered TWICE, under its mangled name and under
// its retail address, even though today every live call site in our tree uses
// the name form (verified by grep across code/, engine/, standalone/, runtime/
// and dx11/: all of them go through engine/funcs/engine_funcs.cpp or
// standalone/address_registry.cpp, both of which call ResolveExport). The
// address registration costs nothing and closes the Wave 5 lane D failure mode
// where one live object is split between two implementations because a later
// call site used the other door.
//
// The four exe PE-import slots that carry lane-A methods —
// 0x1401518d8 (ctor), 0x1401518e0 (dtor), 0x1401518e8 (ComputeLayout),
// 0x1401518f0 (GetAlignedPosition member) and 0x1401518f8 (Draw) — are filled
// by `RegisterDispatchSlot(slot, ResolveExport(name))` in
// standalone/address_registry.cpp, and `AddressRegistryInit()` runs AFTER
// `RuntimePortInit()` (standalone/main.cpp:1614 then :1626). So those slots
// pick this port up too, without this file touching them. CHECKED, because a
// slot filled BEFORE the port registered would have kept the DLL pointer and
// quietly halved the lane. (WAVE 9 LANE UIF: this used to end "0x1401518e0
// keeps the DLL's dtor, which is correct: the dtor is held back." The dtor is
// no longer held back, so that slot now picks up `RtUi_Element_Dtor` by the
// same after-the-fact ResolveExport the other four use — no change needed
// here, but the sentence had to go rather than stay and be wrong.)
//
// runtime/src/ui/rt_ui_port.cpp before this function is called.
// ============================================================================

// ============================================================================
// ??1Element@UI@@UEAA@XZ  retail 0x1801161a0  ord 584   (WAVE 9 LANE UIF)
// ============================================================================
//
// WHY THE WAVE 7 HOLD-BACK NO LONGER STANDS. It rested on two reasons, and
// BOTH were re-read rather than assumed (reference_holdback_blockers_are_stale
// _check_them_first.md — three stated blockers in one session turned out to be
// facts the tree already had):
//
//   (a) "NOTHING IN OUR TREE CALLS IT." That was true of DIRECT callers and is
//       still true of them. It is no longer the whole picture, because it was
//       a statement about our C++ call graph and the thing that reaches this
//       function is a VTABLE SLOT. `??_7Element@UI@@6B@` slot 0 is not `??1`
//       and never was — it is the VECTOR deleting destructor
//       `??_EElement@UI@@UEAAPEAXI@Z` at 0x180035c10 — and Wave 9 lane K
//       ported all 37 of those slot-0 bodies. 0x180035c10 was disassembled for
//       this lane: it reaches `??1Element` at 0x1801161a0 on BOTH branches
//       (`LEA R9,[0x1801161a0]` into the array iterator at 0x180035c28, and
//       `CALL 0x1801161a0` in the scalar branch at 0x180035c61). Lane K calls
//       `~Class` by ADDRESS through ResolveAddr, which consults the port
//       registry first, so the registration below is what makes that edge land
//       in our code. Until it did, lane K's ported slot 0 called into the DLL —
//       and with the DLL absent, into NULL. That is the shape the brief warned
//       about: a null reached through a LIVE MINTED VTABLE, faulting far from
//       its cause.
//
//   (b) "It is the most destructive function in the class and it has no
//       oracle." Still exactly true, and nothing below pretends otherwise —
//       see THE ORACLE QUESTION. What changed is only that (a) turned this
//       from an optional port into a cutover blocker, so the choice is no
//       longer "unverified port vs. hold-back" but "unverified port vs. a null
//       call". The Wave 2 allocator verdict that an unverified destroying port
//       is worse than a hold-back assumed a working DLL underneath it. There
//       is none after cutover.
//
// THE BODY, read from the disassembly at 0x1801161a0 and not from Ghidra's C
// (reference_ghidra_c_output_vs_disassembly.md):
//
//     this->_vtable = &??_7Element@UI@@6B@;         // 0x180156160
//     RemoveChildren(this);                         // 0x180115570
//     if (this->_border != 0 && this->_border != 0) // yes, retail tests twice
//         this->_border->vtbl[0](this->_border, 1); //   scalar deleting dtor
//         this->_border = nullptr;
//     ~Array<Element*>(&this->_children);           // 0x180037240, this+0xa0
//     <release this->_toolTipString>                // +0x58
//     <release this->_name>                         // +0x10
//     JMP ~Internal(this);                          // 0x1800bfdb0, TAIL call
//
// THE DOUBLED NULL TEST ON `_border` IS REPRODUCED. Retail emits both
// `CMP qword ptr [RBX+0xb0],0x0` (0x1801161c5) and `TEST RCX,RCX` (0x1801161d6)
// on the same loaded value. It is redundant and it is kept: this is a
// byte-faithful port and "tidying" a test out of a destructor is how a
// difference gets introduced that nothing then looks for.
//
// EVERY CALLEE IS SATISFIED ON OUR SIDE — which is the whole admission test
// (a port only CLOSES a cutover row if it does not merely move it one frame
// deeper). Checked one at a time, not assumed:
//   * 0x180115570 RemoveChildren  — PORTED, Wave 9 lane B3, widgets_layout.cpp
//   * slot 0 on `_border`         — PORTED, Wave 9 lane K's deleting-dtor family
//   * 0x180037240 ~Array<Element*> — PORTED BY THIS LANE as the twelfth address
//                                    of lane Q's J1 family; see the byte-level
//                                    argument in runtime/src/collection/array.cpp
//   * 0x1800c7ad0 MemFree         — PORTED, lane Q
//   * 0x1800bfdb0 ~Internal       — PORTED, Wave 3, runtime/src/object/internal.cpp
// So this row closes rather than relocating.
//
// THE TAIL CALL TO ~Internal IS LOAD-BEARING AND IS NOT OPTIONAL. 0x1800bfdb0
// is the InternalFactory swap-remove that
// reference_internalfactory_swapremove_wild_write.md is about: an Element freed
// without it leaves a stale handle-table entry that a later swap-remove writes
// through. Reached here by ADDRESS, exactly where retail reaches it, so
// whichever variant internal.cpp has live (plain or shadowed) is what runs.
//
// THE ORACLE QUESTION — THERE IS NO ORACLE, AND THIS PORT SHIPS WITHOUT ONE.
// Stated plainly because a quiet site reads as a pass
// (reference_uncalled_port_is_not_verified.md). A replay here would not be a
// comparison but a SECOND DESTRUCTION: it would destroy every child again
// through slot 0, decrement two String refcounts our body may already have
// taken to zero and FREED, free the child array again, and run the handle
// swap-remove twice on a handle that no longer exists. Any one of those is
// heap corruption; the last is this project's worst failure family. So this is
// a NO-ORACLE PORT in the exact disposition Wave 9 lane M established and lane
// Z reused four rows over in runtime/src/ui/userinterface.cpp: a real body,
// registered, with NO call into the shipped body at all.
//
// Nothing in this file is evidence that the body is right; the evidence is the
// disassembly transcribed above.
void RtUi_Element_Dtor(Element* self) {
    bool didWork = false;

    // Retail's vtable restamp. Resolved by name so our objects and the DLL's
    // carry the identical pointer (the argument at the top of this file).
    if (void* vt = ElementVTable()) self->_vtable = vt;

    // 0x180115570 RemoveChildren, reached BY ADDRESS. ResolveAddr consults the
    // port registry first (reference_resolveaddr_consults_port_registry_first
    // .md), so this lands on lane B3's port, not the DLL.
    {
        using Fn = void (*)(void*);
        static Fn fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x180115570));
        if (fn) fn(self);
    }

    // The border element, destroyed through ITS OWN slot 0 with flag 1 —
    // scalar delete. Not `??1Element`: slot 0 is `??_E`, and picking the wrong
    // one of the three spellings resolves null behind a guard
    // (reference_phantom_dtor_mangling_qeaa_vs_ueaa.md).
    if (self->_border != nullptr) {
        void* border = self->_border;
        if (border != nullptr) {           // retail's second, redundant test
            using DeletingDtorFn = void* (*)(void*, unsigned);
            void** vtbl = *reinterpret_cast<void***>(border);
            reinterpret_cast<DeletingDtorFn>(vtbl[0])(border, 1u);
            didWork = true;
        }
        self->_border = nullptr;
    }

    // ~Array<Element*> on this+0xa0, by ADDRESS so it lands on lane Q's J1 body
    // (the twelfth address, registered by this lane).
    {
        using Fn = void* (*)(void*);
        static Fn fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x180037240));
        if (fn) fn(&self->_children);
    }

    // The two System::String releases, in retail's order (+0x58 then +0x10) and
    // with retail's asymmetry: on the FREEING branch the field is left holding
    // `data - 8`, a dangling pointer; only the SHARING branch nulls it. Both
    // reproduced — see runtime/types/system/String.h, "RETAIL'S RELEASE IDIOM,
    // AND WHY IT MUST BE COPIED EXACTLY".
    if (char* p = reinterpret_cast<char*>(self->_toolTipString)) {
        if (--RtSystem::StringRefCount(p) == 0) {
            self->_toolTipString = p - 8;
            MemFreeFn freeFn = MemFree();
            if (freeFn) freeFn(self->_toolTipString);
            didWork = true;
        } else {
            self->_toolTipString = nullptr;
        }
    }
    if (char* p = reinterpret_cast<char*>(self->_name)) {
        if (--RtSystem::StringRefCount(p) == 0) {
            self->_name = p - 8;
            MemFreeFn freeFn = MemFree();
            if (freeFn) freeFn(self->_name);
            didWork = true;
        } else {
            self->_name = nullptr;
        }
    }


    // 0x1800bfdb0 ~Internal — a TAIL CALL in retail, a normal call here. That
    // is the one deliberate departure: it costs a stack frame and changes
    // nothing the object can observe.
    {
        using Fn = void (*)(void*);
        static Fn fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800bfdb0));
        if (fn) fn(self);
    }
}

// ---- ?AdjustUpperLeft@Element@UI@@QEAAXHH@Z  0x180115840  ord 1596 ---------
//                                                        WAVE 9 LANE UIC
//
// Translate this element's screen rectangle by (dx, dy), then every descendant,
// then everything on its border chain. Thirty-two instructions, ZERO calls
// other than the recursion into itself — the cheapest enabling port in the
// dialog cluster, and the only thing standing between
// `?SetUpperLeft@DialogInstance@` (WAVE9_LANE_UIF.md row 8) and admission.
//
//     180115860  ADD [RBX+0x68],EDX      ; _layoutLeft   += dx
//     180115863  ADD [RBX+0x70],EDX      ; _layoutRight  += dx
//     180115866  ADD [RBX+0x6c],EBP      ; _layoutTop    += dy
//     180115869  ADD [RBX+0x74],EBP      ; _layoutBottom += dy
//     18011586c  XOR EDI,EDI             ; i = 0   <- INSIDE the border loop
//     18011586e  CMP [RBX+0xa8],EDI / JLE
//     180115880  MOV RAX,[RBX+0xa0] / MOV RCX,[RSI+RAX] ; children[i]
//     18011588b  TEST RCX,RCX / JZ                      ; null children skipped
//     180115893  CALL 0x180115840                       ; recurse
//     180115898  MOV EDX,[RSP+0x38]      ; dx reloaded — EDX is volatile
//     1801158aa  MOV RBX,[RBX+0xb0] / TEST RBX,RBX / JNZ 0x180115860
//
// THE BORDER IS A LOOP, NOT A RECURSION. The tail at 0x1801158aa jumps back to
// 0x180115860, so the border chain is walked ITERATIVELY while children are
// walked recursively. Written as a `while` here for that reason; a recursive
// `AdjustUpperLeft(self->_border, dx, dy)` computes the same answer but is a
// different stack profile on a long border chain, and this body is reached from
// a UI frame.
//
// `_children._data` IS RE-LOADED ON EVERY ITERATION (0x180115880 is inside the
// loop). Nothing in the body can move it — but the load is in the shipped code,
// and hoisting it is the kind of tidy-up that turns a transcription into an
// assumption. Reproduced.
//
// No null test on `self`: retail has none, and the two callers
// (`?SetUpperLeft@DialogInstance@` 0x180114a60 and this body itself) both test
// before calling. CLAUDE.md's rule — the deref is load-bearing.
namespace {
// The recursive core.
void AdjustUpperLeftCore(Element* self, int32_t dx, int32_t dy) {
    while (self) {
        self->_layoutLeft   += dx;
        self->_layoutRight  += dx;
        self->_layoutTop    += dy;
        self->_layoutBottom += dy;
        for (int32_t i = 0; i < self->_children._size; ++i) {
            Element* child = self->_children._data[i];
            if (child) AdjustUpperLeftCore(child, dx, dy);
        }
        self = static_cast<Element*>(self->_border);
    }
}

void AdjustUpperLeftImpl(Element* self, int32_t dx, int32_t dy) {
    AdjustUpperLeftCore(self, dx, dy);
}

}  // namespace

void RtUi_Element_AdjustUpperLeft(Element* self, int32_t dx, int32_t dy) {
    AdjustUpperLeftImpl(self, dx, dy);
}

int RtUiElementRegister() {
    int count = 0;

    RegisterRuntimeExport("??0Element@UI@@QEAA@XZ", (void*)&RtUi_Element_Ctor); ++count;
    RegisterRuntimeAddr(0x180115e50, (void*)&RtUi_Element_Ctor);

    RegisterRuntimeExport("?AddChild@Element@UI@@QEAAXV?$InternalPtr@VElement@UI@@@Object@@@Z", (void*)&RtUi_Element_AddChild); ++count;
    RegisterRuntimeAddr(0x180115df0, (void*)&RtUi_Element_AddChild);

    RegisterRuntimeExport("?Child@Element@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@H@Z", (void*)&RtUi_Element_Child); ++count;
    RegisterRuntimeAddr(0x180035540, (void*)&RtUi_Element_Child);

    RegisterRuntimeExport("?ChildCount@Element@UI@@QEBAHXZ", (void*)&RtUi_Element_ChildCount); ++count;
    RegisterRuntimeAddr(0x180035530, (void*)&RtUi_Element_ChildCount);

    RegisterRuntimeExport("?ComputeLayout@Element@UI@@UEAAXHHHH@Z", (void*)&RtUi_Element_ComputeLayout); ++count;
    RegisterRuntimeAddr(0x1801168d0, (void*)&RtUi_Element_ComputeLayout);

    RegisterRuntimeExport("?Draw@Element@UI@@UEBAXAEAVSpriteContext@2@E@Z", (void*)&RtUi_Element_Draw); ++count;
    RegisterRuntimeAddr(0x1801168f0, (void*)&RtUi_Element_Draw);

    RegisterRuntimeExport("?FindElement@Element@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@AEBVString@System@@@Z", (void*)&RtUi_Element_FindElement); ++count;
    RegisterRuntimeAddr(0x180115650, (void*)&RtUi_Element_FindElement);

    RegisterRuntimeExport("?FindElementType@Element@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@I@Z", (void*)&RtUi_Element_FindElementType); ++count;
    RegisterRuntimeAddr(0x180115760, (void*)&RtUi_Element_FindElementType);

    RegisterRuntimeExport("?GetAlignedPosition@Element@UI@@QEAAXHHHHHHAEAH000_N1@Z", (void*)&RtUi_Element_GetAlignedPosition); ++count;
    RegisterRuntimeAddr(0x180115ff0, (void*)&RtUi_Element_GetAlignedPosition);
    // WAVE 10: the STATIC 17-argument overload the member forwards to. Name AND
    // address, the standing convention -- lane B3 registered a port by name only
    // and left the tree's one address-door bypass (see the RemoveChildren note
    // in widgets_layout.cpp).
    RegisterRuntimeExport(kGetAlignedStatic, (void*)&RtUi_Element_GetAlignedPositionStatic); ++count;
    RegisterRuntimeAddr(0x1801158e0, (void*)&RtUi_Element_GetAlignedPositionStatic);
    RegisterRuntimeExport("?GetPaddedSize@Element@UI@@QEAAXAEAH0@Z", (void*)&RtUi_Element_GetPaddedSize); ++count;
    RegisterRuntimeAddr(0x180115f10, (void*)&RtUi_Element_GetPaddedSize);

    RegisterRuntimeExport("?GetUserData@Element@UI@@QEAAPEAVBase@Object@@XZ", (void*)&RtUi_Element_GetUserData); ++count;
    RegisterRuntimeAddr(0x180115550, (void*)&RtUi_Element_GetUserData);

    RegisterRuntimeExport("?Hide@Element@UI@@QEAAX_N@Z", (void*)&RtUi_Element_Hide); ++count;
    RegisterRuntimeAddr(0x180035830, (void*)&RtUi_Element_Hide);

    RegisterRuntimeExport("?SetElementId@Element@UI@@QEAAXH@Z", (void*)&RtUi_Element_SetElementId); ++count;
    RegisterRuntimeAddr(0x180115820, (void*)&RtUi_Element_SetElementId);

    RegisterRuntimeExport("?SetFlagsOnOff@Element@UI@@QEAAXII@Z", (void*)&RtUi_Element_SetFlagsOnOff); ++count;
    RegisterRuntimeAddr(0x180116160, (void*)&RtUi_Element_SetFlagsOnOff);

    RegisterRuntimeExport("?SetName@Element@UI@@QEAAXAEBVString@System@@@Z", (void*)&RtUi_Element_SetName); ++count;
    RegisterRuntimeAddr(0x1801154f0, (void*)&RtUi_Element_SetName);

    RegisterRuntimeExport("?SetNeedsSizeCalc@Element@UI@@QEAAXXZ", (void*)&RtUi_Element_SetNeedsSizeCalc); ++count;
    RegisterRuntimeAddr(0x180035470, (void*)&RtUi_Element_SetNeedsSizeCalc);

    RegisterRuntimeExport("?SetPadding@Element@UI@@QEAAXHHHH@Z", (void*)&RtUi_Element_SetPadding); ++count;
    RegisterRuntimeAddr(0x180115480, (void*)&RtUi_Element_SetPadding);

    RegisterRuntimeExport("?SetToolTip@Element@UI@@QEAAXVString@2@@Z", (void*)&RtUi_Element_SetToolTip); ++count;
    RegisterRuntimeAddr(0x180035740, (void*)&RtUi_Element_SetToolTip);

    RegisterRuntimeExport("?SetToolTipString@Element@UI@@QEAAXAEBVString@System@@@Z", (void*)&RtUi_Element_SetToolTipString); ++count;
    RegisterRuntimeAddr(0x1800356e0, (void*)&RtUi_Element_SetToolTipString);

    RegisterRuntimeExport("?SetUserData@Element@UI@@QEAAXPEAVBase@Object@@@Z", (void*)&RtUi_Element_SetUserData); ++count;
    RegisterRuntimeAddr(0x180115560, (void*)&RtUi_Element_SetUserData);

    RegisterRuntimeExport("?SetOffsets@ContainerElement@UI@@QEAAXHH@Z", (void*)&RtUi_ContainerElement_SetOffsets); ++count;
    RegisterRuntimeAddr(0x1800cc1e0, (void*)&RtUi_ContainerElement_SetOffsets);

    // WAVE 9 LANE UIF: both Wave 7 hold-backs are now landed. RemoveChildren
    // went in with Wave 9 lane B3 (runtime/src/ui/widgets_layout.cpp); the
    // destructor is directly below.
    RegisterRuntimeExport("??1Element@UI@@UEAA@XZ", (void*)&RtUi_Element_Dtor); ++count;
    RegisterRuntimeAddr(0x1801161a0, (void*)&RtUi_Element_Dtor);

    // WAVE 9 LANE UIC: the enabling port for ?SetUpperLeft@DialogInstance@.
    // Exported (checked against the shipped export directory before this line
    // was written — reference_resolveexport_phantom_names.md) so it is
    // registered under BOTH doors like every sibling above.
    RegisterRuntimeExport("?AdjustUpperLeft@Element@UI@@QEAAXHH@Z", (void*)&RtUi_Element_AdjustUpperLeft); ++count;
    RegisterRuntimeAddr(0x180115840, (void*)&RtUi_Element_AdjustUpperLeft);

    return count;
}
