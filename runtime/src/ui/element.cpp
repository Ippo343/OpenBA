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
//   | ??1Element (dtor)               | 0x1801161a0 |  584 | NO     | —       |
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
//   | ?RemoveChildren@                | 0x180115570 | 3552 | NO     | —       |
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
//
// ---------------------------------------------------------------------------
// THE TWO HOLD-BACKS
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
// These constants exist only for the diff oracle's RT_ORIGINAL_ONCE lookups.
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
//  * ResolveAddr(Module::Runtime, addr) — "always the DLL". Used for the three
//    Collection::Array helpers, which are header-only template instantiations
//    with NO exported symbol, and for nothing else.
//
// Every one of these is looked up ONCE and cached. RtOriginal / ResolveExport
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
using GetAlignedStaticFn = void (*)(int alignment, int, int, int, int, int, int,
                                    int leftPad, int rightPad, int topPad, int bottomPad,
                                    int*, int*, int*, int*, bool, bool);
GetAlignedStaticFn GetAlignedStatic() {
    static GetAlignedStaticFn fn = reinterpret_cast<GetAlignedStaticFn>(ResolveExport(
        "?GetAlignedPosition@Element@UI@@SAXW4Alignment@ElementDescription@2@HHHHHHHHHHAEAH111_N2@Z"));
    return fn;
}

// Collection::Array<Element*>::SetSize(int, const T&). Header-only template:
// NO export exists, so this is the one place this file uses
// ResolveAddr(Module::Runtime, …). Safe here — and only here — because nothing
// registers 0x180076900, so the port registry (consulted first, see
// standalone/resolve_standalone.cpp:325) does not intercept it.
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
    static const uint32_t* p = reinterpret_cast<const uint32_t*>(
        ResolveExport("?_typeInfo@ClipRegionElement@UI@@0VBaseTypeInfo@Object@@A"));
    return p ? *p : 0u;
}
uint32_t ScrollTypeId() {
    static const uint32_t* p = reinterpret_cast<const uint32_t*>(
        ResolveExport("?_typeInfo@ScrollElement@UI@@0VBaseTypeInfo@Object@@A"));
    return p ? *p : 0u;
}

// Video::Device::_singleton (exported, 0x1801fe7a8). GetPaddedSize reads the
// screen size out of the pointed-to Device at +0x04 / +0x08.
void* VideoDeviceSingleton() {
    static void** p = reinterpret_cast<void**>(ResolveExport("?_singleton@Device@Video@@0PEAV12@EA"));
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

// ============================================================================
// The replay oracle
// ============================================================================
// Snapshot the live 0xC0-byte Element into a private scratch copy BEFORE our
// body runs; run our body on the real object; run the DLL's body on the copy;
// memcmp all 0xC0 bytes. RtDiffReportBytes names the first differing offset,
// and Element.h's table turns that offset straight into a field name.
//
// This is a FULL oracle for every function whose only writes land inside
// `this`: it catches a wrong offset, a missed flag bit, an early-out taken
// when retail would not have taken it, and a stray write into padding.
//
// THE ONE THING IT DOES NOT COVER, STATED PLAINLY. Five functions here
// (SetNeedsSizeCalc, SetFlagsOnOff, Hide, and — through them — SetMinSize-style
// callers) walk `_parent` to the root and OR flag bit 27 into every ancestor.
// The scratch copy's `_parent` is the REAL parent, so the replayed original
// walks the real chain and ORs bit 27 a second time. That is safe, and the
// reason is specific rather than hopeful: OUR body has already set exactly
// those bits on exactly those objects, so the replay's writes are a bitwise-OR
// of a bit that is already 1 — idempotent, not a second, different mutation.
// If a future edit makes the walk do anything other than OR a set bit, this
// argument dies with it.
//
// One shared file-scope scratch, not an 0xC0 stack buffer per function.
// A depth counter makes that safe even if a replay ever nests (it does not
// today: FindElement/FindElementType recurse through a private helper that does
// no diffing, and no other ported body calls another ported body).
alignas(16) uint8_t g_replayScratch[kElementSize];
int g_replayDepth = 0;

struct ReplayGuard {
    Element* copy;
    bool     held;
    ReplayGuard(bool on, const Element* live) : copy(nullptr), held(false) {
        if (!on) return;
        if (g_replayDepth != 0) return;          // nested: skip the oracle, never corrupt it
        ++g_replayDepth;
        held = true;
        std::memcpy(g_replayScratch, live, kElementSize);
        copy = reinterpret_cast<Element*>(g_replayScratch);
    }
    ~ReplayGuard() { if (held) --g_replayDepth; }
    ReplayGuard(const ReplayGuard&) = delete;
    ReplayGuard& operator=(const ReplayGuard&) = delete;
};

// ============================================================================
// The thunk oracle (ComputeLayout / Draw)
// ============================================================================
// Both are five-instruction forwarders that load `this->_border` and tail-jump
// through THAT object's vtable. A forwarder cannot be replayed — running Draw
// twice submits the geometry twice — but it has exactly two things that can be
// wrong: the FIELD offset it loads and the SLOT it jumps to, and getting either
// wrong calls a different method with this method's arguments.
//
// So at registration time the port decodes the shipped bodies out of the loaded
// image and reports, through an ordinary diff site, whether the DLL's field
// displacement and jump slot are the ones the port uses. The shapes, verified
// byte-for-byte against the on-disk image:
//
//   ComputeLayout 0x1801168d0:
//     48 8b 89 b0 00 00 00   MOV RCX,[RCX+0xb0]
//     48 85 c9               TEST RCX,RCX
//     74 07                  JZ  +7
//     4c 8b 11               MOV R10,[RCX]
//     49 ff 62 28            JMP qword ptr [R10+0x28]
//   Draw 0x1801168f0:
//     48 8b 89 b0 00 00 00 / 48 85 c9 / 74 07
//     48 8b 01               MOV RAX,[RCX]
//     48 ff 60 30            JMP qword ptr [RAX+0x30]
//
// An unrecognised shape is REPORTED, not silently passed — the right failure
// direction, but note in WAVE7_LANE_A.md §7 that it is an untested path.
bool DecodeBorderThunk(const uint8_t* p, uint32_t* outField, uint8_t* outSlot) {
    // MOV RCX,[RCX+disp32]
    if (!(p[0] == 0x48 && p[1] == 0x8b && p[2] == 0x89)) return false;
    std::memcpy(outField, p + 3, 4);
    // TEST RCX,RCX ; JZ rel8
    if (!(p[7] == 0x48 && p[8] == 0x85 && p[9] == 0xc9 && p[10] == 0x74)) return false;
    const uint8_t* q = p + 12;
    // MOV R10,[RCX] (4c 8b 11) then JMP [R10+disp8] (49 ff 62 xx)
    if (q[0] == 0x4c && q[1] == 0x8b && q[2] == 0x11 &&
        q[3] == 0x49 && q[4] == 0xff && q[5] == 0x62) {
        *outSlot = q[6];
        return true;
    }
    // MOV RAX,[RCX] (48 8b 01) then JMP [RAX+disp8] (48 ff 60 xx)
    if (q[0] == 0x48 && q[1] == 0x8b && q[2] == 0x01 &&
        q[3] == 0x48 && q[4] == 0xff && q[5] == 0x60) {
        *outSlot = q[6];
        return true;
    }
    return false;
}

// NOTE, and it is a trap worth naming: the shipped bytes are located with
// RtOriginal (= ResolveExportRaw = GetProcAddress), NOT with
// ResolveAddr(Module::Runtime, …). This lane calls RegisterRuntimeAddr on both
// forwarder addresses, and ResolveAddr(Module::Runtime, …) consults that
// registry FIRST (standalone/resolve_standalone.cpp:325) — so it would hand
// back OUR function and the decoder would report "unrecognised thunk shape"
// against our own prologue. Only the raw export lookup is guaranteed to reach
// the DLL.
void VerifyBorderThunk(RtDiffSite* site, const char* mangled,
                       uint32_t expectField, uint8_t expectSlot) {
    if (!RtDiffEnabled()) return;
    RtDiffRegister(site);
    const uint8_t* p = reinterpret_cast<const uint8_t*>(RtOriginal<const void*>(mangled));
    char ours[64], theirs[64];
    if (!p) {
        std::snprintf(ours,   sizeof(ours),   "field=+0x%x slot=+0x%x", expectField, expectSlot);
        std::snprintf(theirs, sizeof(theirs), "<export did not resolve>");
        RtDiffReport(site, false, ours, theirs);
        return;
    }
    uint32_t field = 0; uint8_t slot = 0;
    const bool ok = DecodeBorderThunk(p, &field, &slot);
    std::snprintf(ours,   sizeof(ours),   "field=+0x%x slot=+0x%x", expectField, expectSlot);
    if (!ok) {
        std::snprintf(theirs, sizeof(theirs), "<unrecognised thunk shape>");
        RtDiffReport(site, false, ours, theirs);
        return;
    }
    std::snprintf(theirs, sizeof(theirs), "field=+0x%x slot=+0x%x", field, slot);
    RtDiffReport(site, field == expectField && slot == expectSlot, ours, theirs);
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
    RT_DIFF_BEGIN("UI::Element::ctor");

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
    //   +0x08, +0x0c  the handle index/id — necessarily different for two
    //                 distinct live objects; that they differ is correct.
    //   +0x54, +0x60, +0x64, +0x7c, +0x8c  never written by either ctor.
    if (rt_diff_on__ && g_replayDepth == 0) {
        ++g_replayDepth;
        using CtorFn = void* (*)(void*);
        using DtorFn = void  (*)(void*);
        CtorFn theirCtor = RT_ORIGINAL_ONCE(CtorFn, kCtor);
        DtorFn theirDtor = RT_ORIGINAL_ONCE(DtorFn, kDtor);
        if (theirCtor && theirDtor) {
            std::memset(g_replayScratch, 0, kElementSize);
            theirCtor(g_replayScratch);

            uint8_t a[kElementSize], b[kElementSize];
            std::memcpy(a, self, kElementSize);
            std::memcpy(b, g_replayScratch, kElementSize);
            static const struct { size_t off, len; } kSkip[] = {
                { 0x08, 8 },   // _handleIndex + _handleId
                { 0x54, 4 },   // UI::String tail padding
                { 0x60, 8 },   // _layoutWidth + _layoutHeight
                { 0x7c, 4 },
                { 0x8c, 4 },
            };
            for (const auto& s : kSkip) {
                std::memset(a + s.off, 0, s.len);
                std::memset(b + s.off, 0, s.len);
            }
            RT_DIFF_BYTES(a, b, kElementSize);
            theirDtor(g_replayScratch);
        }
        --g_replayDepth;
    }
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
    RT_DIFF_BEGIN("UI::Element::AddChild");

    const int  oldSize = self->_children._size;
    Element* const c   = reinterpret_cast<Element*>(*child);

    if (c) c->_parent = self;
    ArraySetSizeFillFn grow = ArraySetSizeFill();
    if (grow) grow(&self->_children, self->_children._size + 1, child);
    NeedsSizeCalcWalk(self);

    if (rt_diff_on__) {
        const bool sizeOk   = (self->_children._size == oldSize + 1);
        const bool slotOk   = sizeOk && self->_children._data != nullptr &&
                              self->_children._data[oldSize] == c;
        const bool parentOk = (c == nullptr) || (c->_parent == self);
        const bool dirtyOk  = (self->_flags & UI::kElementFlagNeedsSizeCalc) != 0;
        char ours[160], theirs[160];
        std::snprintf(ours, sizeof(ours),
                      "size %d->%d, data[%d]=%p, child->_parent=%p, dirty=%d",
                      oldSize, self->_children._size, oldSize,
                      (slotOk && self->_children._data) ? (void*)self->_children._data[oldSize] : nullptr,
                      c ? (void*)c->_parent : nullptr, dirtyOk ? 1 : 0);
        std::snprintf(theirs, sizeof(theirs),
                      "expected size %d, data[%d]=%p, child->_parent=%p, dirty=1",
                      oldSize + 1, oldSize, (void*)c, (void*)self);
        RT_DIFF_SHADOW(sizeOk && slotOk && parentOk && dirtyOk, ours, theirs);
    }
}

// ---- ?Child@  0x180035540 --------------------------------------------------
// Hidden return slot in RDX (the InternalPtr the caller wants filled), index in
// R8D. NO BOUNDS CHECK, and the index is SIGN-extended (MOVSXD RCX,R8D) — a
// negative index reads before the buffer, exactly as retail does.
void* RtUi_Element_Child(Element* self, void** ret, int index) {
    RT_DIFF_BEGIN("UI::Element::Child");

    *ret = self->_children._data[static_cast<intptr_t>(index)];

    if (rt_diff_on__) {
        using Fn = void* (*)(Element*, void**, int);
        void* theirs = nullptr;
        RT_ORIGINAL_ONCE(Fn, kChild)(self, &theirs, index);
        RT_DIFF_SCALAR(static_cast<const void*>(*ret), static_cast<const void*>(theirs));
    }
    return ret;
}

// ---- ?ChildCount@  0x180035530 ---------------------------------------------
int RtUi_Element_ChildCount(const Element* self) {
    RT_DIFF_BEGIN("UI::Element::ChildCount");
    const int ours = self->_children._size;
    if (rt_diff_on__) {
        using Fn = int (*)(const Element*);
        RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(Fn, kChildCount)(self));
    }
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
// The recursion goes through this private helper so that under
// BANISHED_RT_DIFF=1 the oracle runs ONCE per top-level call instead of
// doubling the work at every level of the tree.
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
    RT_DIFF_BEGIN("UI::Element::FindElement");
    FindElementImpl(self, ret, name);
    if (rt_diff_on__) {
        using Fn = void* (*)(Element*, void**, const char* const*);
        void* theirs = nullptr;
        RT_ORIGINAL_ONCE(Fn, kFindElement)(self, &theirs, name);
        RT_DIFF_SCALAR(static_cast<const void*>(*ret), static_cast<const void*>(theirs));
    }
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
    RT_DIFF_BEGIN("UI::Element::FindElementType");
    FindElementTypeImpl(self, ret, typeId);
    if (rt_diff_on__) {
        using Fn = void* (*)(Element*, void**, uint32_t);
        void* theirs = nullptr;
        RT_ORIGINAL_ONCE(Fn, kFindElemType)(self, &theirs, typeId);
        RT_DIFF_SCALAR(static_cast<const void*>(*ret), static_cast<const void*>(theirs));
    }
    return ret;
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
    RT_DIFF_BEGIN("UI::Element::GetAlignedPosition");

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

    if (rt_diff_on__) {
        using Fn = void (*)(Element*, int, int, int, int, int, int,
                            int*, int*, int*, int*, bool, bool);
        int tx = 0, ty = 0, tw = 0, th = 0;
        RT_ORIGINAL_ONCE(Fn, kGetAligned)(self, a, b, c, d, e, f,
                                          &tx, &ty, &tw, &th, stretchX, stretchY);
        const bool eq = (*outX == tx && *outY == ty && *outW == tw && *outH == th);
        char ours[96], theirs[96];
        std::snprintf(ours,   sizeof(ours),   "x=%d y=%d w=%d h=%d", *outX, *outY, *outW, *outH);
        std::snprintf(theirs, sizeof(theirs), "x=%d y=%d w=%d h=%d", tx, ty, tw, th);
        RT_DIFF_SHADOW(eq, ours, theirs);
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
    RT_DIFF_BEGIN("UI::Element::GetPaddedSize");

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

    if (rt_diff_on__) {
        using Fn = void (*)(Element*, int*, int*);
        int tw = 0, th = 0;
        RT_ORIGINAL_ONCE(Fn, kGetPadded)(self, &tw, &th);
        const bool eq = (*outW == tw && *outH == th);
        char ours[64], theirs[64];
        std::snprintf(ours,   sizeof(ours),   "w=%d h=%d", *outW, *outH);
        std::snprintf(theirs, sizeof(theirs), "w=%d h=%d", tw, th);
        RT_DIFF_SHADOW(eq, ours, theirs);
    }
}

// ---- ?GetUserData@  0x180115550 --------------------------------------------
void* RtUi_Element_GetUserData(Element* self) {
    RT_DIFF_BEGIN("UI::Element::GetUserData");
    void* ours = self->_userData;
    if (rt_diff_on__) {
        using Fn = void* (*)(Element*);
        RT_DIFF_SCALAR(static_cast<const void*>(ours),
                       static_cast<const void*>(RT_ORIGINAL_ONCE(Fn, kGetUserData)(self)));
    }
    return ours;
}

// ---- ?Hide@  0x180035830 ---------------------------------------------------
// Sets or clears flag bit 0 (which means HIDDEN — `?IsVisible@` is its
// negation) and runs the dirty walk only if the bit actually changed. Retail
// builds this branchlessly out of SETZ/SETNZ/NOT; the mask is
// `hide ? 0xFFFFFFFF : ~1`, so the AND is a no-op on the hide path.
void RtUi_Element_Hide(Element* self, bool hide) {
    RT_DIFF_BEGIN("UI::Element::Hide");
    ReplayGuard replay(rt_diff_on__, self);

    const uint32_t old = self->_flags;
    const uint32_t setMask   = hide ? 1u : 0u;
    const uint32_t clearMask = hide ? 0xFFFFFFFFu : ~1u;
    const uint32_t next = (old | setMask) & clearMask;
    self->_flags = next;
    if ((next ^ old) & 1u) NeedsSizeCalcWalk(self);

    if (replay.copy) {
        using Fn = void (*)(Element*, bool);
        RT_ORIGINAL_ONCE(Fn, kHide)(replay.copy, hide);
        RT_DIFF_BYTES(self, replay.copy, kElementSize);
    }
}

// ---- ?SetElementId@  0x180115820 -------------------------------------------
void RtUi_Element_SetElementId(Element* self, int id) {
    RT_DIFF_BEGIN("UI::Element::SetElementId");
    ReplayGuard replay(rt_diff_on__, self);
    self->_elementId = id;
    if (replay.copy) {
        using Fn = void (*)(Element*, int);
        RT_ORIGINAL_ONCE(Fn, kSetElementId)(replay.copy, id);
        RT_DIFF_BYTES(self, replay.copy, kElementSize);
    }
}

// ---- ?SetFlagsOnOff@  0x180116160 ------------------------------------------
// `flags = (flags | on) & ~off`, then the dirty walk if BIT 0 changed — only
// bit 0, not any other bit. The parameter names in
// engine/funcs/engine_funcs.h WERE "hideFlags, showFlags", which happens to
// read right for the common case of bit 0 (bit 0 means HIDDEN) but inverts for
// every other bit; this lane renamed them to (setFlags, clearFlags).
// See WAVE7_LANE_A.md §6.
void RtUi_Element_SetFlagsOnOff(Element* self, uint32_t on, uint32_t off) {
    RT_DIFF_BEGIN("UI::Element::SetFlagsOnOff");
    ReplayGuard replay(rt_diff_on__, self);

    const uint32_t old = self->_flags;
    const uint32_t next = (old | on) & ~off;
    self->_flags = next;
    if ((next ^ old) & 1u) NeedsSizeCalcWalk(self);

    if (replay.copy) {
        using Fn = void (*)(Element*, uint32_t, uint32_t);
        RT_ORIGINAL_ONCE(Fn, kSetFlags)(replay.copy, on, off);
        RT_DIFF_BYTES(self, replay.copy, kElementSize);
    }
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
    RT_DIFF_BEGIN("UI::Element::SetName");

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

    if (rt_diff_on__) {
        const bool storedOk  = (self->_name == incoming);
        const bool acquiredOk = (incoming == nullptr) ||
                                (RtSystem::StringRefCount(incoming) >= 1);
        const bool releaseOk  = (previous == incoming) || (previous == nullptr) ||
                                freedPrev || prevRefAfter >= 1;
        char ours[176], theirs[176];
        std::snprintf(ours, sizeof(ours),
                      "_name=%p (stored=%d), new refcount=%d (acquired=%d), "
                      "old=%p freed=%d refAfter=%d (released=%d)",
                      (void*)self->_name, storedOk ? 1 : 0,
                      incoming ? RtSystem::StringRefCount(incoming) : 0, acquiredOk ? 1 : 0,
                      (void*)previous, freedPrev ? 1 : 0, prevRefAfter, releaseOk ? 1 : 0);
        std::snprintf(theirs, sizeof(theirs),
                      "expected _name=%p, new refcount>=1, old released exactly once",
                      (void*)incoming);
        RT_DIFF_SHADOW(storedOk && acquiredOk && releaseOk, ours, theirs);
    }
}

// ---- ?SetNeedsSizeCalc@  0x180035470 ---------------------------------------
void RtUi_Element_SetNeedsSizeCalc(Element* self) {
    RT_DIFF_BEGIN("UI::Element::SetNeedsSizeCalc");
    ReplayGuard replay(rt_diff_on__, self);
    NeedsSizeCalcWalk(self);
    if (replay.copy) {
        using Fn = void (*)(Element*);
        RT_ORIGINAL_ONCE(Fn, kSetNeedsCalc)(replay.copy);
        RT_DIFF_BYTES(self, replay.copy, kElementSize);
    }
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
    RT_DIFF_BEGIN("UI::Element::SetPadding");
    ReplayGuard replay(rt_diff_on__, self);

    self->_leftPad   = left;
    self->_rightPad  = right;
    self->_bottomPad = bottom;   // retail writes +0x24 BEFORE +0x20; same bytes
    self->_topPad    = top;

    if (replay.copy) {
        using Fn = void (*)(Element*, int, int, int, int);
        RT_ORIGINAL_ONCE(Fn, kSetPadding)(replay.copy, left, right, top, bottom);
        RT_DIFF_BYTES(self, replay.copy, kElementSize);
    }
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
    RT_DIFF_BEGIN("UI::Element::SetToolTip");
    ReplayGuard replay(rt_diff_on__, self);

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

    if (replay.copy) {
        using Fn = void (*)(Element*, const UIString*);
        RT_ORIGINAL_ONCE(Fn, kSetToolTip)(replay.copy, tip);
        RT_DIFF_BYTES(self, replay.copy, kElementSize);
    }
}

// ---- ?SetToolTipString@  0x1800356e0 ---------------------------------------
// Byte-for-byte the same release/acquire as SetName, on +0x58 instead of +0x10.
// Same shadow oracle, for the same reason.
void RtUi_Element_SetToolTipString(Element* self, void* const* str) {
    RT_DIFF_BEGIN("UI::Element::SetToolTipString");

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

    if (rt_diff_on__) {
        const bool storedOk   = (self->_toolTipString == incoming);
        const bool acquiredOk = (incoming == nullptr) ||
                                (RtSystem::StringRefCount(incoming) >= 1);
        const bool releaseOk  = (previous == incoming) || (previous == nullptr) ||
                                freedPrev || prevRefAfter >= 1;
        char ours[176], theirs[176];
        std::snprintf(ours, sizeof(ours),
                      "_toolTipString=%p (stored=%d), new refcount=%d (acquired=%d), "
                      "old=%p freed=%d refAfter=%d (released=%d)",
                      (void*)self->_toolTipString, storedOk ? 1 : 0,
                      incoming ? RtSystem::StringRefCount(incoming) : 0, acquiredOk ? 1 : 0,
                      (void*)previous, freedPrev ? 1 : 0, prevRefAfter, releaseOk ? 1 : 0);
        std::snprintf(theirs, sizeof(theirs),
                      "expected _toolTipString=%p, new refcount>=1, old released exactly once",
                      (void*)incoming);
        RT_DIFF_SHADOW(storedOk && acquiredOk && releaseOk, ours, theirs);
    }
}

// ---- ?SetUserData@  0x180115560 --------------------------------------------
void RtUi_Element_SetUserData(Element* self, void* userData) {
    RT_DIFF_BEGIN("UI::Element::SetUserData");
    ReplayGuard replay(rt_diff_on__, self);
    self->_userData = userData;
    if (replay.copy) {
        using Fn = void (*)(Element*, void*);
        RT_ORIGINAL_ONCE(Fn, kSetUserData)(replay.copy, userData);
        RT_DIFF_BYTES(self, replay.copy, kElementSize);
    }
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
    RT_DIFF_BEGIN("UI::ContainerElement::SetOffsets");

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

    if (rt_diff_on__) {
        using Fn = void (*)(Element*, int, int);
        RT_ORIGINAL_ONCE(Fn, kSetOffsets)(self, x, y);

        const bool clipOk = (*reinterpret_cast<const int32_t*>(clipBytes + 0xd0) == x) &&
                            (*reinterpret_cast<const int32_t*>(clipBytes + 0xd4) == y);
        const bool vOk = !vscroll ||
            *reinterpret_cast<const float*>(reinterpret_cast<const uint8_t*>(vscroll) + 0x160)
                == static_cast<float>(y);
        const bool hOk = !hscroll ||
            *reinterpret_cast<const float*>(reinterpret_cast<const uint8_t*>(hscroll) + 0x160)
                == static_cast<float>(x);
        char ours[192], theirs[192];
        std::snprintf(ours, sizeof(ours),
                      "clip=%p +0xd0/+0xd4 held (%d), vscroll=%p +0x160 held (%d), "
                      "hscroll=%p +0x160 held (%d)",
                      (void*)clip, clipOk ? 1 : 0, (void*)vscroll, vOk ? 1 : 0,
                      (void*)hscroll, hOk ? 1 : 0);
        std::snprintf(theirs, sizeof(theirs),
                      "after the original: expected clip +0xd0=%d +0xd4=%d, "
                      "vscroll +0x160=%d, hscroll +0x160=%d", x, y, y, x);
        RT_DIFF_SHADOW(clipOk && vOk && hOk, ours, theirs);
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
// quietly halved the lane. 0x1401518e0 keeps the DLL's dtor, which is correct:
// the dtor is held back.
//
// The kill switch is BANISHED_RT_UIELEMENT=0, applied by
// runtime/src/ui/rt_ui_port.cpp before this function is called.
// ============================================================================

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

    // The two forwarders' oracle, run once here rather than per call. Each site
    // is a file-scope static so it appears in RtDiffSummary() exactly once.
    static RtDiffSite computeLayoutThunkSite = { "UI::Element::ComputeLayout[thunk]", 0, 0, false, false };
    static RtDiffSite drawThunkSite          = { "UI::Element::Draw[thunk]",          0, 0, false, false };
    VerifyBorderThunk(&computeLayoutThunkSite,
                      "?ComputeLayout@Element@UI@@UEAAXHHHH@Z", 0xb0, 0x28);
    VerifyBorderThunk(&drawThunkSite,
                      "?Draw@Element@UI@@UEBAXAEAVSpriteContext@2@E@Z", 0xb0, 0x30);

    // HELD BACK, with reasons at the top of this file:
    //   ??1Element@UI@@UEAA@XZ          0x1801161a0  (no live caller + no oracle)
    //   ?RemoveChildren@Element@UI@@QEAAXXZ 0x180115570  (destroys children)

    return count;
}
