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

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <new>

#include "../../funcs/rt_port.h"
#include "../../types/system/String.h"
#include "../../types/ui/Element.h"
#include "../../types/ui/ContainerElement.h"   // fragment pass F2: SetOffsets
// `enum class Module` + `ResolveAddr` (declared once there, byte-identical to
// standalone/resolve.h) and `rtuielem::AtomicExport`, the B6-5 door cache.
#include "element_common.h"

// ----------------------------------------------------------------------------
// ResolveExport, forward-declared rather than pulled in from
// standalone/resolve.h — same reasoning rt_port.h gives for declaring
// RegisterRuntimeExport locally: resolve.h drags in <windows.h>, and PLAN §4.1
// requires runtime/src/ to stay platform-neutral. This declaration must stay
// byte-identical to resolve.h's; the linker checks it against it.
// ----------------------------------------------------------------------------
void*     ResolveExport(const char* mangled_name);

// ENDGAME B.3.6 block A: `UI::Element` is the REAL class now (Element.h), so
// every door below receives a typed `Element*` and delegates with a QUALIFIED
// non-virtual call (`self->UI::Element::M(…)`): a live Element wears the MINTED
// table, whose slots resolve back to these doors, so an unqualified call from a
// door into a virtual would re-enter the door (B34_BRIEFS A.1).
using UI::Element;
using ::UIString;   // runtime/types/ui/String.h

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

// ?MemFree@MemoryAllocator@System@@SAXPEAX@Z (0x1800c7ad0) — the static,
// null-safe free the String release path calls. Resolved by name, so the
// allocator port is what runs.
//
// B.3.6 (RULE B6-5): the MEMBER bodies below reach it through a namespace-
// scope std::atomic slot (`rtuielem::AtomicExport`) rather than a function-
// local static, because UI is constructed on the loader thread while the main
// thread draws. `MemFree()` itself keeps its old static for the two DOORS
// (ctor/dtor) that are not moved in step 6.
using MemFreeFn = void (*)(void*);
MemFreeFn MemFree() {
    static MemFreeFn fn =
        reinterpret_cast<MemFreeFn>(ResolveExport("?MemFree@MemoryAllocator@System@@SAXPEAX@Z"));
    return fn;
}
std::atomic<void*> g_memFreeSlot{nullptr};
MemFreeFn MemFreeAtomic() {
    return reinterpret_cast<MemFreeFn>(rtuielem::AtomicExport(
        g_memFreeSlot, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z"));
}

// ?IsDerivedFrom@Base@Object@@QEBA_NI@Z (0x1800c0390) — ContainerElement::
// SetOffsets' type test. NOTE for whoever reads a crash log: this is the
// function standalone/main.cpp:518's crash handler recognises by the offset
// +0xC0390, because retail calls it on a possibly-null child (see SetOffsets).
// B6-5 (fragment pass F2): an atomic retry-a-miss slot, not a first-call
// static — ContainerElement::SetOffsets, a member, is its caller.
using IsDerivedFromFn = bool (*)(const void*, uint32_t);
std::atomic<void*> g_isDerivedFromSlot{nullptr};
IsDerivedFromFn BaseIsDerivedFromFn() {
    return reinterpret_cast<IsDerivedFromFn>(rtuielem::AtomicExport(
        g_isDerivedFromSlot, "?IsDerivedFrom@Base@Object@@QEBA_NI@Z"));
}

// The static 17-argument overload the member GetAlignedPosition forwards to:
//   ?GetAlignedPosition@Element@UI@@SAXW4Alignment@ElementDescription@2@HHHHHHHHHHAEAH111_N2@Z
// Ours since Wave 10. B.3.6: the member now calls the static MEMBER directly;
// the ResolveExport door that used to sit here (and its first-call cache) is
// gone. The registered door below still carries the name for other callers.
const char* const kGetAlignedStatic =
    "?GetAlignedPosition@Element@UI@@SAXW4Alignment@ElementDescription@2@HHHHHHHHHHAEAH111_N2@Z";

// ENDGAME B.3.2 step 2 — the 0x180076900 forwarder that used to sit here is
// GONE. `Element::_children` is a real `Collection::Array<Element*>` member
// (Element.h, +0xa0), and 0x180076900 is `kSpec_Fill8Z`, trait-equivalent for
// `Element*` (dialog_create.cpp's banner has the full spec comparison), so
// `AddChild` below calls `.SetSizeFill()` on the member directly.

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
// screen size out of the pointed-to Device at +0x04 / +0x08. B6-5: an atomic
// slot, not a first-call static — GetPaddedSize is a member.
std::atomic<void*> g_deviceSingletonSlot{nullptr};
void* VideoDeviceSingleton() {
    void** p = static_cast<void**>(rtuielem::AtomicExport(
        g_deviceSingletonSlot, "?_singleton@Device@Video@@0PEAV12@EA"));
    return p ? *p : nullptr;
}

} // namespace

// ============================================================================
// The member bodies (ENDGAME B.3.6 block A)
// ============================================================================
// Each body below MOVED here from its registered door (`self->x` became `x`),
// and the door, further down, is now a one-line QUALIFIED delegation. Virtual
// calls onto OTHER elements (the border's slot 5/6, a child's slot 2/4) are
// plain virtual calls: they dispatch through the RECEIVER's table — minted or
// GCC's, slot for slot the same — exactly where the old `VSlot<Fn>(p, N)` read.
namespace UI {

// ---- ?SetNeedsSizeCalc@  0x180035470 ---------------------------------------
// The tail every mutator in this class shares. Note the shape: the OR happens
// BEFORE the null test, so it always runs at least once on `this` — a
// do/while, not a while.
void Element::SetNeedsSizeCalc() {
    Element* e = this;
    do {
        e->_flags |= kElementFlagNeedsSizeCalc;
        e = e->_parent;
    } while (e != nullptr);
}

}  // namespace UI

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

    // The vptr store. `UI::Element` is a real polymorphic class now, so the
    // pointer has no field name; the door stamps the MINTED table exactly as
    // before (B4b-2: this stays a door, it is not a C++ constructor).
    *reinterpret_cast<void**>(self) = ElementVTable();
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

// ============================================================================
// Member bodies, moved from their doors by ENDGAME B.3.6 block A
// ============================================================================
namespace UI {

// ---- ?AddChild@  0x180115df0 -----------------------------------------------
// `child` is an `Object::InternalPtr<Element>` passed BY VALUE, which MSVC
// hands over as a pointer to the caller's 8-byte temporary (the disassembly's
// first act is `MOV RAX,[RDX]`); the door below keeps that shape and this
// member takes the handle itself.
//
// retail 0x180076900 — kSpec_Fill8Z, trait-equivalent for `Element*` (see
// dialog_create.cpp's banner): `_children` is the trait-derived
// `Array<Element*>` instantiation already. The fill argument is the address of
// an 8-byte handle holding the child pointer — the same bytes retail hands it.
void Element::AddChild(Object::InternalPtr<Element> child) {
    Element* const c = child.Get();
    if (c) c->_parent = this;
    Element* fill = c;
    _children.SetSizeFill(_children.GetSize() + 1, &fill);
    SetNeedsSizeCalc();
}

// ---- ?Child@  0x180035540 --------------------------------------------------
// NO BOUNDS CHECK, and the index is SIGN-extended (MOVSXD RCX,R8D) — a
// negative index reads before the buffer, exactly as retail does.
Object::InternalPtr<Element> Element::Child(int32_t index) {
    return Object::InternalPtr<Element>(_children.GetData()[static_cast<intptr_t>(index)]);
}

// ---- ?ChildCount@  0x180035530  (`MOV EAX,[RCX+0xa8]`) ---------------------
int32_t Element::ChildCount() const {
    return _children.GetSize();
}

// ---- ?ComputeLayout@  0x1801168d0  (vtable slot 5) -------------------------
// A tail-jump into the BORDER element's slot 5. `Element` itself lays nothing
// out; every widget that has geometry overrides this. The border call is a
// VIRTUAL call on the border — its own table's slot 5, which is what retail's
// `JMP [RAX+0x28]` reads.
void Element::ComputeLayout(int32_t x, int32_t y, int32_t w, int32_t h) {
    Element* border = static_cast<Element*>(_border);
    if (!border) return;
    border->ComputeLayout(x, y, w, h);
}

// ---- ?Draw@  0x1801168f0  (vtable slot 6, const) ---------------------------
void Element::Draw(SpriteContext& context, uint8_t alpha) const {
    Element* border = static_cast<Element*>(_border);
    if (!border) return;
    border->Draw(context, alpha);
}

// ---- ?FindElement@  0x180115650 --------------------------------------------
// Depth-first search by name, self first. `name` is a `const System::String&`
// whose one field is the character body; the length lives at body[-8] and the
// refcount at body[-4] (runtime/types/system/String.h).
//
// Two faithfulness details that a tidied version would drop:
//   * An EMPTY search string never matches anything, not even an unnamed
//     element: the `lq == 0` test jumps straight to the child scan.
//   * Two elements with equal-length names are compared with StrCompare only
//     when THIS element's name is non-null and non-empty; a null name of
//     length 0 against a length-0 query would have matched, but the query
//     length test above has already excluded that case.
// The recursion is a direct call of this member on each non-null child.
Object::InternalPtr<Element> Element::FindElement(const System::String& name) {
    const char* q = name._data;
    if (q && RtSystem::StringLength(q) != 0) {
        const char* mine = static_cast<const char*>(_name);
        const int32_t lm = mine ? RtSystem::StringLength(mine) : 0;
        if (lm == RtSystem::StringLength(q)) {
            bool equal;
            if (!mine || RtSystem::StringLength(mine) == 0) {
                equal = true;
            } else {
                equal = System::String::StrCompare(mine, q) == 0;
            }
            if (equal) return Object::InternalPtr<Element>(this);
        }
    }
    for (int i = 0; i < _children.GetSize(); ++i) {
        Element* c = _children.GetData()[i];
        if (!c) continue;
        Object::InternalPtr<Element> found = c->FindElement(name);
        if (found) return found;
    }
    return Object::InternalPtr<Element>();
}

// ---- ?FindElementType@  0x180115760 ----------------------------------------
// Same shape, keyed on the virtual `Type()` (slot 2) rather than the name.
// EXACT equality, not IsDerivedFrom — a subclass of the requested type is NOT
// a hit. `Type()` is a VIRTUAL call: the dynamic type is the point.
Object::InternalPtr<Element> Element::FindElementType(uint32_t typeId) {
    if (Type() == typeId) return Object::InternalPtr<Element>(this);
    for (int i = 0; i < _children.GetSize(); ++i) {
        Element* c = _children.GetData()[i];
        if (!c) continue;
        Object::InternalPtr<Element> found = c->FindElementType(typeId);
        if (found) return found;
    }
    return Object::InternalPtr<Element>();
}

// ---- ?GetAlignedPosition@ (STATIC, 17 args)  0x1801158e0 -------------------
//
// A pure integer leaf: no calls, no allocation, no globals, four out
// parameters. It places a box of size (outW x outH) inside a container of size
// (containerW x containerH), honouring four pads and a 3x3 alignment, and
// optionally STRETCHES the box to the available space first.
//
// THE ALIGNMENT ENUM IS A ROW-MAJOR 3x3, read off the jump table at
// 0x180115a14 rather than guessed. The X axis switches on `alignment - 1` with
// `JA -> default` for anything above 7, and the eight table entries cycle
// center / right / left; the Y axis is a compare chain (<3 top, 3..5 middle,
// 6..8 bottom, >8 top):
//
//        0 TopLeft      1 TopCenter      2 TopRight
//        3 MiddleLeft   4 MiddleCenter   5 MiddleRight
//        6 BottomLeft   7 BottomCenter   8 BottomRight
//
// and ANY value outside 0..8 lands on left/top.
//
// TWO FAITHFULNESS TRAPS, both of which a tidied port gets wrong:
//  * The centring halves are `SAR reg,1` -- an ARITHMETIC SHIFT, which rounds
//    toward negative infinity. `/ 2` rounds toward zero. `>>` on a signed int is
//    the faithful spelling (GCC guarantees arithmetic shift).
//  * `outW` / `outH` are IN/OUT: read as the box's size, possibly OVERWRITTEN
//    by the stretch, then read AGAIN by the placement. The order is load-bearing.
//
// (B.3.6: the `useDivide` negative-control parameter the old free function
// carried is gone with it; its only user, element_aligned_probe.cpp, no longer
// exists in the tree.)
void Element::GetAlignedPosition(ElementDescription::Alignment alignment,
                                 int32_t x, int32_t y,
                                 int32_t containerW, int32_t containerH,
                                 int32_t offsetX, int32_t offsetY,
                                 int32_t leftPad, int32_t rightPad,
                                 int32_t topPad, int32_t bottomPad,
                                 int32_t& outX, int32_t& outY,
                                 int32_t& outW, int32_t& outH,
                                 bool stretchX, bool stretchY) {
    const int32_t align = static_cast<int32_t>(alignment);
    // Cursor = origin + offset, then pulled back inside the container if the
    // box would overhang. Retail compares against the CURRENT outW/outH.
    int curX = x + offsetX;
    int curY = y + offsetY;

    if (containerW < outW + curX) curX = containerW - outW;
    if (containerH < outH + curY) curY = containerH - outH;

    curX += leftPad;
    curY += topPad;

    // Space left between the cursor and the far pad.
    const int availW = (containerW - rightPad)  - curX;
    const int availH = (containerH - bottomPad) - curY;

    // The stretch, BEFORE the placement reads the size back.
    if (stretchX) outW = availW;
    if (stretchY) outH = availH;

    // ---- X: center for 1/4/7, right for 2/5/8, left otherwise -------------
    switch (align) {
    case 1: case 4: case 7:
        outX = ((availW >> 1) - (outW >> 1)) + curX;
        break;
    case 2: case 5: case 8:
        outX = (containerW - rightPad) - outW;
        break;
    default:
        outX = curX;
        break;
    }

    // ---- Y: middle for 3..5, bottom for 6..8, top otherwise ---------------
    if (align >= 3 && align <= 5) {
        outY = ((availH >> 1) - (outH >> 1)) + curY;
    } else if (align >= 6 && align <= 8) {
        outY = (containerH - bottomPad) - outH;
    } else {
        outY = curY;
    }
}

// ---- ?GetAlignedPosition@ (member)  0x180115ff0 ----------------------------
// Twelve parameters after `this`: six ints, four int& outputs, two bools. The
// body recomputes the element's size through virtual slot 4, publishes it into
// the third and fourth outputs, then forwards everything to the STATIC
// 17-argument overload (0x1801158e0) — now a direct call of the static member.
//
// The two bools are each OR'd with a sticky flag bit before being forwarded:
// `stretchX` with bit 2 and `stretchY` with bit 3. The LAST parameter (+0x100
// in the frame) pairs with flag 0x8 and lands in the LAST outgoing slot; the
// eleventh (+0xf8) pairs with flag 0x4 and lands one slot earlier.
void Element::GetAlignedPosition(int32_t a, int32_t b, int32_t c, int32_t d,
                                 int32_t e, int32_t f,
                                 int32_t& outX, int32_t& outY,
                                 int32_t& outW, int32_t& outH,
                                 bool stretchX, bool stretchY) {
    ComputeSize();                                  // slot 4, virtual

    outW = _layoutWidth;
    outH = _layoutHeight;

    const bool bx = stretchX || (_flags & kElementFlagAlignArg1) != 0;
    const bool by = stretchY || (_flags & kElementFlagAlignArg2) != 0;

    GetAlignedPosition(static_cast<ElementDescription::Alignment>(_alignment),
                       a, b, c, d, e, f,
                       _leftPad, _rightPad, _topPad, _bottomPad,
                       outX, outY, outW, outH, bx, by);
}

// ---- ?GetPaddedSize@  0x180115f10 ------------------------------------------
// Size + padding, floored by the minimum size, then optionally REPLACED
// outright by a fraction of the screen when flag bits 4 / 5 are set.
//
// Faithfulness details, all of them observable:
//   * The additions are `layout + right + left` and `layout + bottom + top`,
//     in that order, in 32-bit — an overflowing pad wraps the same way.
//   * The minimum-size floor is applied to the PADDED value.
//   * The screen-size override walks to the ROOT element and reads the root's
//     `_dialogParent`, not this element's.
//   * The second flag test RE-READS `_flags` from memory (retail keeps
//     `LEA RDX,[RBX+0x34]` alive across the root walk, which clobbers RBX).
//   * Both conversions are `CVTTSS2SI` — truncation toward zero, not rounding.
void Element::GetPaddedSize(int32_t& outW, int32_t& outH) {
    ComputeSize();                                  // slot 4, virtual

    outW = _layoutWidth  + _rightPad  + _leftPad;
    outH = _layoutHeight + _bottomPad + _topPad;
    if (outW < _minWidth)  outW = _minWidth;
    if (outH < _minHeight) outH = _minHeight;

    const uint32_t flags = _flags;
    if (flags & (kElementFlagSizeToScreenH | kElementFlagSizeToScreenW)) {
        Element* root = this;
        while (root->_parent != nullptr) root = root->_parent;
        void* dialogParent = root->_dialogParent;
        const uint8_t* device = static_cast<const uint8_t*>(VideoDeviceSingleton());

        if (flags & kElementFlagSizeToScreenW) {
            const int32_t screenW = *reinterpret_cast<const int32_t*>(device + 0x04);
            const uint8_t* scaleOwner = *reinterpret_cast<const uint8_t* const*>(
                static_cast<const uint8_t*>(dialogParent) + 0x10);
            const float scale = *reinterpret_cast<const float*>(scaleOwner + 0xdc);
            outW = static_cast<int>(static_cast<float>(screenW) * scale);
        }
        // RE-READ, not the cached `flags` — see the note above.
        if (_flags & kElementFlagSizeToScreenH) {
            const int32_t screenH = *reinterpret_cast<const int32_t*>(device + 0x08);
            const uint8_t* scaleOwner = *reinterpret_cast<const uint8_t* const*>(
                static_cast<const uint8_t*>(dialogParent) + 0x10);
            const float scale = *reinterpret_cast<const float*>(scaleOwner + 0xdc);
            outH = static_cast<int>(static_cast<float>(screenH) * scale);
        }
    }
}

// ---- ?GetUserData@  0x180115550 --------------------------------------------
Object::Base* Element::GetUserData() {
    return static_cast<Object::Base*>(_userData);
}

// ---- ?Hide@  0x180035830 ---------------------------------------------------
// Sets or clears flag bit 0 (which means HIDDEN — `?IsVisible@` is its
// negation) and runs the dirty walk only if the bit actually changed. Retail
// builds this branchlessly out of SETZ/SETNZ/NOT; the mask is
// `hide ? 0xFFFFFFFF : ~1`, so the AND is a no-op on the hide path.
void Element::Hide(bool hide) {
    const uint32_t old = _flags;
    const uint32_t setMask   = hide ? 1u : 0u;
    const uint32_t clearMask = hide ? 0xFFFFFFFFu : ~1u;
    const uint32_t next = (old | setMask) & clearMask;
    _flags = next;
    if ((next ^ old) & 1u) SetNeedsSizeCalc();
}

// ---- ?SetElementId@  0x180115820 -------------------------------------------
void Element::SetElementId(int32_t id) {
    _elementId = id;
}

// ---- ?SetFlagsOnOff@  0x180116160 ------------------------------------------
// `flags = (flags | on) & ~off`, then the dirty walk if BIT 0 changed — only
// bit 0, not any other bit. (setFlags, clearFlags), not "hide, show".
void Element::SetFlagsOnOff(uint32_t on, uint32_t off) {
    const uint32_t old = _flags;
    const uint32_t next = (old | on) & ~off;
    _flags = next;
    if ((next ^ old) & 1u) SetNeedsSizeCalc();
}

}  // namespace UI

namespace {
// The System::String release/acquire SetName and SetToolTipString share, on
// the field each passes. The release path is retail's, including the detail
// that the FIELD is used as the scratch for the pointer arithmetic:
// `field -= 8` and then `MemFree(field)`, which leaves a freed pointer in the
// field for the two instructions before it is overwritten. Reproduced.
void AssignStringField(void*& field, char* incoming) {
    char* const previous = static_cast<char*>(field);
    if (previous != incoming) {
        if (previous) {
            if (--RtSystem::StringRefCount(previous) == 0) {
                field = previous - 8;
                MemFreeFn freeFn = MemFreeAtomic();
                if (freeFn) freeFn(field);
            } else {
                field = nullptr;
            }
        }
        field = incoming;
        if (incoming) ++RtSystem::StringRefCount(incoming);
    }
}
}  // namespace

namespace UI {

// ---- ?SetName@  0x1801154f0 ------------------------------------------------
// System::String release/acquire on +0x10.
void Element::SetName(const System::String& name) {
    AssignStringField(_name, name._data);
}

// ---- ?SetPadding@  0x180115480 ---------------------------------------------
// *** THE PARAMETER ORDER IS (left, RIGHT, TOP, bottom). ***
// Retail stores EDX->+0x18, R8D->+0x1c, R9D->+0x20, [RSP+0x28]->+0x24, and the
// one-line accessors pin which offset is which. SetPadding does NOT run the
// dirty walk (SetMinSize does); the walk is simply not in the shipped body.
void Element::SetPadding(int32_t left, int32_t right, int32_t top, int32_t bottom) {
    _leftPad   = left;
    _rightPad  = right;
    _bottomPad = bottom;   // retail writes +0x24 BEFORE +0x20; same bytes
    _topPad    = top;
}

// ---- ?SetToolTip@  0x180035740 ---------------------------------------------
// `tip` is a `UI::String` BY VALUE — 16 bytes, NOT refcounted, so this is a
// plain 16-byte copy.
//   * The `_toolTipDialog != 0` test is evaluated FIRST, before the copy.
//   * The second store is a full QWORD at +0x50, so it also writes the 4 bytes
//     at +0x54 that the ctor leaves alone. *** DO NOT "FIX" THIS COPY TO 0x0C
//     BYTES ***: `MOV RAX,[RDX+0x8] ; MOV [RCX+0x50],RAX` (0x180035748..50).
//   * Both branches end with `if ((flags ^ old) & 1) walk`, but neither
//     `|= 0x90000` nor `&= ~0x80000` can change bit 0, so that walk is
//     UNREACHABLE in the shipped code. Kept, because it is in the shipped code.
void Element::SetToolTip(::UIString tip) {
    const bool hadDialog = (_toolTipDialog != nullptr);
    std::memcpy(&_toolTip, &tip, sizeof(::UIString));

    const uint32_t old = _flags;
    uint32_t next;
    if (hadDialog && _toolTip.table != nullptr && _toolTip.index >= 0 &&
        (old & kElementFlagDisabled) == 0) {
        next = old | (kElementFlagToolTip16 | kElementFlagToolTipValid);
    } else {
        next = old & ~static_cast<uint32_t>(kElementFlagToolTipValid);
    }
    _flags = next;
    if ((next ^ old) & 1u) SetNeedsSizeCalc();
}

// ---- ?SetToolTipString@  0x1800356e0 ---------------------------------------
// Byte-for-byte the same release/acquire as SetName, on +0x58 instead of +0x10.
void Element::SetToolTipString(const System::String& text) {
    AssignStringField(_toolTipString, text._data);
}

// ---- ?SetUserData@  0x180115560 --------------------------------------------
void Element::SetUserData(Object::Base* userData) {
    _userData = userData;
}

// ---- ??1Element@UI@@UEAA@XZ — the C++ destructor, EMPTY on purpose ---------
// The retail body (0x1801161a0) is the registered door `RtUi_Element_Dtor`
// below, reached through the minted slot 0. No live Element is destroyed as a
// C++ object; this exists so a probe subclass can derive (Material precedent).
Element::~Element() {}

// ---- ?SetOffsets@ContainerElement@UI@@QEAAXHH@Z  0x1800cc1e0 ---------------
// ENDGAME B.3.6 fragment pass F2: block C4's class, its body in block A's file
// (the evidence, the UNGUARDED first write and the re-load of `_children` per
// read are documented at the door `RtUi_ContainerElement_SetOffsets` below).
void ContainerElement::SetOffsets(int32_t x, int32_t y) {

    IsDerivedFromFn isA = BaseIsDerivedFromFn();

    Element* child0 = _children.GetData()[0];
    Element* clip = (isA && isA(child0, ClipRegionTypeId())) ? child0 : nullptr;
    uint8_t* clipBytes = reinterpret_cast<uint8_t*>(clip);
    *reinterpret_cast<int32_t*>(clipBytes + 0xd0) = x;
    *reinterpret_cast<int32_t*>(clipBytes + 0xd4) = y;

    Element* child1 = _children.GetData()[1];
    Element* vscroll = nullptr;
    if (child1 && isA && isA(child1, ScrollTypeId())) vscroll = child1;

    Element* child2 = _children.GetData()[2];
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

}  // namespace UI

// ============================================================================
// The DOORS. Registered names and addresses unchanged; each is now a one-line
// QUALIFIED delegation. The retslot ones keep retail's `(this, ret, …)` shape
// and placement-new the member's prvalue into the caller's slot (B5-12).
// ============================================================================

void RtUi_Element_AddChild(Element* self, void** child) {
    self->UI::Element::AddChild(
        Object::InternalPtr<Element>(static_cast<Element*>(*child)));
}

void* RtUi_Element_Child(Element* self, void** ret, int index) {
    new (static_cast<void*>(ret)) Object::InternalPtr<Element>(
        self->UI::Element::Child(index));
    return ret;
}

int RtUi_Element_ChildCount(const Element* self) {
    return self->UI::Element::ChildCount();
}

void RtUi_Element_ComputeLayout(Element* self, int x, int y, int w, int h) {
    self->UI::Element::ComputeLayout(x, y, w, h);
}

void RtUi_Element_Draw(const Element* self, void* spriteContext, unsigned char alpha) {
    self->UI::Element::Draw(*static_cast<UI::SpriteContext*>(spriteContext), alpha);
}

void* RtUi_Element_FindElement(Element* self, void** ret, const char* const* name) {
    new (static_cast<void*>(ret)) Object::InternalPtr<Element>(
        self->UI::Element::FindElement(
            *reinterpret_cast<const System::String*>(name)));   // B.3.6 bridge
    return ret;
}

void* RtUi_Element_FindElementType(Element* self, void** ret, uint32_t typeId) {
    new (static_cast<void*>(ret)) Object::InternalPtr<Element>(
        self->UI::Element::FindElementType(typeId));
    return ret;
}

void RtUi_Element_GetAlignedPositionStatic(
        int alignment, int x, int y, int containerW, int containerH,
        int offsetX, int offsetY,
        int leftPad, int rightPad, int topPad, int bottomPad,
        int* outX, int* outY, int* outW, int* outH,
        bool stretchX, bool stretchY) {
    UI::Element::GetAlignedPosition(
        static_cast<UI::ElementDescription::Alignment>(alignment),
        x, y, containerW, containerH, offsetX, offsetY,
        leftPad, rightPad, topPad, bottomPad,
        *outX, *outY, *outW, *outH, stretchX, stretchY);
}

void RtUi_Element_GetAlignedPosition(Element* self, int a, int b, int c, int d,
                                     int e, int f, int* outX, int* outY,
                                     int* outW, int* outH, bool stretchX, bool stretchY) {
    self->UI::Element::GetAlignedPosition(a, b, c, d, e, f,
                                          *outX, *outY, *outW, *outH,
                                          stretchX, stretchY);
}

void RtUi_Element_GetPaddedSize(Element* self, int* outW, int* outH) {
    self->UI::Element::GetPaddedSize(*outW, *outH);
}

void* RtUi_Element_GetUserData(Element* self) {
    return self->UI::Element::GetUserData();
}

void RtUi_Element_Hide(Element* self, bool hide) {
    self->UI::Element::Hide(hide);
}

void RtUi_Element_SetElementId(Element* self, int id) {
    self->UI::Element::SetElementId(id);
}

void RtUi_Element_SetFlagsOnOff(Element* self, uint32_t on, uint32_t off) {
    self->UI::Element::SetFlagsOnOff(on, off);
}

void RtUi_Element_SetName(Element* self, void* const* str) {
    self->UI::Element::SetName(*reinterpret_cast<const System::String*>(str));   // B.3.6 bridge
}

void RtUi_Element_SetNeedsSizeCalc(Element* self) {
    self->UI::Element::SetNeedsSizeCalc();
}

void RtUi_Element_SetPadding(Element* self, int left, int right, int top, int bottom) {
    self->UI::Element::SetPadding(left, right, top, bottom);
}

// `tip` arrives as a pointer to the caller's 16-byte temporary (MSVC passes a
// 16-byte class by address); the member takes the value.
void RtUi_Element_SetToolTip(Element* self, const UIString* tip) {
    self->UI::Element::SetToolTip(*tip);
}

void RtUi_Element_SetToolTipString(Element* self, void* const* str) {
    self->UI::Element::SetToolTipString(*reinterpret_cast<const System::String*>(str));   // B.3.6 bridge
}

void RtUi_Element_SetUserData(Element* self, void* userData) {
    self->UI::Element::SetUserData(static_cast<Object::Base*>(userData));
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
//
// ENDGAME B.3.6 fragment pass F2: the body MOVED into UI::ContainerElement::
// SetOffsets (member section above); the door delegates QUALIFIED.
void RtUi_ContainerElement_SetOffsets(Element* self, int x, int y) {
    static_cast<UI::ContainerElement*>(self)->UI::ContainerElement::SetOffsets(x, y);
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
    // Retail's vtable restamp. Resolved by name so our objects and the DLL's
    // carry the identical pointer (the argument at the top of this file).
    if (void* vt = ElementVTable()) *reinterpret_cast<void**>(self) = vt;

    // 0x180115570 RemoveChildren — a member since fragment pass F1 (body in
    // widgets_layout.cpp); called as one instead of by ADDRESS.
    self->UI::Element::RemoveChildren();

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
        }
        self->_border = nullptr;
    }

    // ENDGAME B.3.2 step 2 — the 0x180037240 forwarder is GONE. `_children` is
    // the real `Collection::Array<Element*>` member, and 0x180037240 is a J1
    // `~Array` (destroy == null): trait-equivalent, since `Element*` is
    // trivially destructible. `.~Array()` IS that instantiation.
    self->_children.~Array();

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
        } else {
            self->_toolTipString = nullptr;
        }
    }
    if (char* p = reinterpret_cast<char*>(self->_name)) {
        if (--RtSystem::StringRefCount(p) == 0) {
            self->_name = p - 8;
            MemFreeFn freeFn = MemFree();
            if (freeFn) freeFn(self->_name);
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
namespace UI {
void Element::AdjustUpperLeft(int32_t dx, int32_t dy) {
    Element* e = this;
    while (e) {
        e->_layoutLeft   += dx;
        e->_layoutRight  += dx;
        e->_layoutTop    += dy;
        e->_layoutBottom += dy;
        for (int32_t i = 0; i < e->_children.GetSize(); ++i) {
            Element* child = e->_children.GetData()[i];
            if (child) child->AdjustUpperLeft(dx, dy);
        }
        e = static_cast<Element*>(e->_border);
    }
}
}  // namespace UI

void RtUi_Element_AdjustUpperLeft(Element* self, int32_t dx, int32_t dy) {
    self->UI::Element::AdjustUpperLeft(dx, dy);
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
