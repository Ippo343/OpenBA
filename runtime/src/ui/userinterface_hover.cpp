// ============================================================================
// runtime/src/ui/userinterface_hover.cpp — Wave 10C lane UIF6
// ============================================================================
//
// The four ORDINARY-MEMBER rows that entered the UI census last wave because
// two faithful ports (lane ELEM's ??_7Element@UI@@6B@ slots and lane CLOSEDLG's
// CloseDialog) began calling callees retail itself calls, and both lanes
// resolved them BY NAME rather than through a silent address door. That is the
// correct trade (reference_porting_can_grow_the_cutover_surface.md); this file
// is the follow-through.
//
//   0x180120830  ?IsInsideClipRegion@UserInterface@UI@@QEBA_NHH@Z
//   0x180120af0  ?EndDragDrop@UserInterface@UI@@QEAAXXZ
//   0x180113d10  ?EndHover@DialogInstance@UI@@QEAAXPEAVUserInterface@2@@Z
//   0x180120860  ?BeginToolTip@UserInterface@UI@@QEAAX
//                    V?$InternalPtr@VDialogInstance@UI@@@Object@@
//                    V?$InternalPtr@VElement@UI@@@4@HH@Z
//
// All four were confirmed ABSENT from the tree before a line was written, by
// ADDRESS and by MANGLED NAME across code/, runtime/, engine/ and standalone/
// (reference_grep_for_the_port_before_writing_it.md). Every hit was a CONSUMER
// -- an RT_EXPORT_ONCE / ResolveExport call site in element_vtable.cpp or
// userinterface_close.cpp -- never a definition. `?BeginToolTip@` in particular
// is referenced through a SPLIT STRING LITERAL (element_vtable.cpp:749-750),
// which a naive grep for the full mangled name does not find; the search that
// found it was for the bare identifier.
//
// ---------------------------------------------------------------------------
// TWO KILL SWITCHES, ON PURPOSE
// ---------------------------------------------------------------------------
//   BANISHED_RT_UIHOVER=0    leaves the three SMALL, NON-ALLOCATING rows on the
//                            DLL (IsInsideClipRegion, EndDragDrop, EndHover).
//   BANISHED_RT_UITOOLTIP=0  leaves BeginToolTip on the DLL.
//
// They are separate because BeginToolTip is a different RISK CLASS from the
// other three: it ALLOCATES a 0xc0-byte ToolTipDialog, runs the DLL's
// DialogInstance constructor on it, STAMPS ??_7ToolTipDialog@UI@@6B@ into the
// object, and does two rounds of System::String refcount release. The other
// three only read fields and push a 0x30-byte event record. A bisect must be
// able to separate them.
//
// ---------------------------------------------------------------------------
// THE ONE ADDRESS DOOR, DECLARED RATHER THAN HIDDEN
// ---------------------------------------------------------------------------
// 0x180033690 `Collection::Array<UI::Event>::SetSize(int, const Event*)` and
// 0x18007a0d0 `Collection::Array<InternalPtr<DialogInstance>>::SetSize` are the
// only two callees in this file with no exported name (checked against the
// shipped export table with pefile, not assumed). NEITHER IS NEW DEBT:
//   * 0x180033690 is already owned by userinterface_frame.cpp:216,
//     userinterface_close.cpp:201 and element_vtable.cpp:177 -- this is the
//     FOURTH call site of the same helper, not a fourth obligation.
//   * 0x18007a0d0 is already owned by userinterface_showdialog.cpp:110, and is
//     already PORTED and registered by runtime/src/collection/array.cpp
//     (J3_ENTRY3(18007a0d0, kSpec_Fill8Z)).
// Both go through ResolveAddr(Module::Runtime, ...), which consults the port
// registry FIRST, so a port of either is reached
// (reference_resolveaddr_consults_port_registry_first.md).
//
// EVERY OTHER callee is reached by NAME. That was checked against the export
// table rather than assumed -- several of them mangle `QEAA`/`SA`, and this DLL
// exports its private members anyway (reference_aeaa_is_private_not_unexported).
// ============================================================================

#include <cstdint>
#include <cstdio>

#include "../../funcs/rt_port.h"
#include "../../types/ui/UserInterface.h"
#include "../../types/ui/DialogInstance.h"
#include "../../types/ui/Element.h"
#include "../../types/system/String.h"

// ResolveAddr lives in standalone/resolve.h, which drags in <windows.h> and so
// must not be included from runtime/src (rt_port.h explains why). Declared here
// exactly as ~30 sibling runtime files already declare it.
enum class Module { Exe, Runtime, VideoDX11 };
uintptr_t ResolveAddr(Module m, uintptr_t ghidra_addr);

namespace {

using UI::UserInterface;
using RtUI::DialogInstance;
using UI::Element;

// ---------------------------------------------------------------------------
// Dependencies these bodies CALL rather than reimplement.
// ---------------------------------------------------------------------------
constexpr uintptr_t kEventsSetSizeAddr     = 0x180033690;  // Array<UI::Event>::SetSize
constexpr uintptr_t kAddDialogsSetSizeAddr = 0x18007a0d0;  // Array<InternalPtr<DialogInstance>>::SetSize

using SetSizeFn      = void  (*)(void* array, int32_t newSize, const void* fill);
using CloseDialogFn  = void  (*)(void* ui, void* dlgPtr);
using OperatorNewFn  = void* (*)(uint64_t size);
using DlgInstCtorFn  = void* (*)(void* self, void* ui, const void* dialog, void* parentPtr,
                                 void* dataSource, void* userData, int32_t userFlags);
using GetTipStringFn = RtSystem::String* (*)(const void* element, RtSystem::String* ret);
using UiStrCastFn    = RtSystem::String* (*)(const void* uiString, RtSystem::String* ret);
using SetTextFn      = void  (*)(void* toolTipDialog, const RtSystem::String* text);
using SetUpperLeftFn = void  (*)(void* dlg, int32_t x, int32_t y, bool centered);
using MemFreeFn      = void  (*)(void* p);

SetSizeFn EventsSetSize() {
    static SetSizeFn fn = reinterpret_cast<SetSizeFn>(
        ResolveAddr(Module::Runtime, kEventsSetSizeAddr));
    return fn;
}
SetSizeFn AddDialogsSetSize() {
    static SetSizeFn fn = reinterpret_cast<SetSizeFn>(
        ResolveAddr(Module::Runtime, kAddDialogsSetSizeAddr));
    return fn;
}

// ---------------------------------------------------------------------------
// ENTRY COUNTERS — the evidence that a registered body RAN AT ALL.
//
// Deliberately separate from any oracle counter. `calls=` on an RtPredictSite
// counts SUPPRESSED INDIRECT CALLS and cannot distinguish "the port ran and the
// oracle was quiet" from "the port was never reached"
// (reference_calls_zero_can_mean_always_took_the_uncovered_branch.md,
// reference_uncalled_port_is_not_verified.md). These count EVERY entry in EVERY
// configuration and print unconditionally, so a zero here means the row is
// registered and DEAD whatever else the board says.
// ---------------------------------------------------------------------------
uint64_t g_clipEntries       = 0;
uint64_t g_clipInside        = 0;   // effectiveness control: how many said true
uint64_t g_endDragEntries    = 0;
uint64_t g_endDragDestroyed  = 0;   // took the "destroy the drag element" arm
uint64_t g_endHoverEntries   = 0;
uint64_t g_endHoverPushed    = 0;   // took the _hovering arm (pushed event 0x11)
uint64_t g_tipEntries        = 0;
uint64_t g_tipRejected       = 0;   // failed one of the four early-out guards
uint64_t g_tipClosedPrev     = 0;   // closed an existing tooltip first
uint64_t g_tipNonEmpty       = 0;   // took the GetToolTipString arm
uint64_t g_tipAllocFailed    = 0;

struct HoverSummary {
    ~HoverSummary() {
        printf("[RtPort] UI::UserInterface::IsInsideClipRegion  entries=%llu inside=%llu\n",
                    (unsigned long long)g_clipEntries, (unsigned long long)g_clipInside);
        printf("[RtPort] UI::UserInterface::EndDragDrop  entries=%llu destroyedElement=%llu\n",
                    (unsigned long long)g_endDragEntries, (unsigned long long)g_endDragDestroyed);
        printf("[RtPort] UI::DialogInstance::EndHover  entries=%llu pushedEvent0x11=%llu\n",
                    (unsigned long long)g_endHoverEntries, (unsigned long long)g_endHoverPushed);
        printf("[RtPort] UI::UserInterface::BeginToolTip  entries=%llu rejected=%llu "
                    "closedPrev=%llu nonEmptyString=%llu allocFailed=%llu\n",
                    (unsigned long long)g_tipEntries, (unsigned long long)g_tipRejected,
                    (unsigned long long)g_tipClosedPrev, (unsigned long long)g_tipNonEmpty,
                    (unsigned long long)g_tipAllocFailed);
        fflush(stdout);
    }
};
HoverSummary g_hoverSummary;

// ---------------------------------------------------------------------------
// One queued UI event, 0x30 bytes.
//
// UserInterface.h models this as an opaque 0x30-byte blob on purpose, and the
// interior below is NOT a re-derivation: it is the field set the two push sites
// in THIS file write, named from the same evidence userinterface_close.cpp:221
// records. `pad24` is called out because RETAIL LEAVES IT UNINITIALISED at both
// push sites in this file -- the stack record is never zeroed and no store
// covers +0x24..+0x27. A diff oracle must not compare those four bytes
// (reference_oracle_must_not_compare_unwritten_bytes.md), and this port must
// not "tidy" them to zero either, because that would be a real behavioural
// difference from retail in the bytes SetSize copies.
// ---------------------------------------------------------------------------
struct EventRecord {
    void*    dialog;     // +0x00
    void*    element;    // +0x08
    int32_t  eventId;    // +0x10
    int32_t  elementId;  // +0x14
    void*    userData;   // +0x18
    uint32_t dragId;     // +0x20
    uint32_t pad24;      // +0x24  DELIBERATELY LEFT UNINITIALISED (see above)
    void*    dragData;   // +0x28
};
static_assert(sizeof(EventRecord) == 0x30, "UI event record stride is 0x30");
static_assert(offsetof(EventRecord, eventId)   == 0x10, "");
static_assert(offsetof(EventRecord, elementId) == 0x14, "");
static_assert(offsetof(EventRecord, dragId)    == 0x20, "");
static_assert(offsetof(EventRecord, dragData)  == 0x28, "");

// ---------------------------------------------------------------------------
// Retail's System::String release idiom, transcribed from
// runtime/types/system/String.h and from the two copies inlined in
// BeginToolTip (0x18012096a..0x180120993 and 0x1801209dc..0x180120a05).
//
// The asymmetry is NOT tidied: on the FREEING branch retail leaves the object
// holding `data - 8`, a dangling pointer, and only the SHARING branch nulls it.
// String.h says in so many words "Reproduce it; do not tidy it."
//
// MemFree is reached with RT_ORIGINAL_ONCE, not ResolveExport. Strings cross
// the DLL boundary constantly, so their storage MUST go back to
// Runtime-x64.dll's own allocator even when our allocator port is armed
// (runtime/types/system/String.h, "THE ALLOCATOR IS NOT OURS").
// ---------------------------------------------------------------------------
void ReleaseString(RtSystem::String* s) {
    char* data = s->data;
    if (!data) return;
    if (--RtSystem::StringRefCount(data) == 0) {
        s->data = data - 8;
        RT_ORIGINAL_ONCE(MemFreeFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z")(s->data);
    } else {
        s->data = nullptr;
    }
}

}  // namespace

// ============================================================================
// ?IsInsideClipRegion@UserInterface@UI@@QEBA_NHH@Z          retail 0x180120830
// ============================================================================
// Twelve instructions, no calls, no writes -- a pure predicate over the four
// ints of _clipRegion at +0xfc. The four comparisons are SIGNED (JL/JG), and
// both bounds are INCLUSIVE.
//
//   180120830  CMP EDX,[RCX+0xfc]   JL  false      x <  left   -> false
//   180120838  CMP EDX,[RCX+0x104]  JG  false      x >  right  -> false
//   180120840  CMP R8D,[RCX+0x100]  JL  false      y <  top    -> false
//   180120849  CMP R8D,[RCX+0x108]  JG  false      y >  bottom -> false
//                                                  otherwise   -> true
//
// Note the operand order the disassembly forces and the C would hide: the
// FIRST pair tests x against left/right (+0xfc/+0x104) and the SECOND tests y
// against top/bottom (+0x100/+0x108), i.e. the struct is {left, top, right,
// bottom} and the tests interleave it. ClipRegion in UserInterface.h already
// names that member order from GetClipRegion (0x180031000).
// ============================================================================
extern "C" bool RtUi_UserInterface_IsInsideClipRegion(const UserInterface* self,
                                                     int32_t x, int32_t y) {
    ++g_clipEntries;
    const UI::ClipRegion& c = self->_clipRegion;
    if (x < c.left)   return false;
    if (x > c.right)  return false;
    if (y < c.top)    return false;
    if (y > c.bottom) return false;
    ++g_clipInside;
    return true;
}

// ============================================================================
// ?EndDragDrop@UserInterface@UI@@QEAAXXZ                    retail 0x180120af0
// ============================================================================
// Pushes one 0x30-byte UI event (id 0xe, "drag-drop CANCEL") naming the current
// drag, then tears the whole drag-drop state down.
//
// THREE THINGS A TIDY REWRITE WOULD GET WRONG, all kept:
//
//  1. The event's `elementId` field (+0x14) is -1 here, where EndHover writes 0.
//     0x180120b2e: MOV dword ptr [RSP+0x34],0xffffffff.
//  2. +0x24 of the record is NEVER WRITTEN (see EventRecord above). The record
//     is a raw stack local in retail and stays one here.
//  3. `_dragDropActive` (+0x88) is NOT cleared, even though every neighbouring
//     field is. Retail simply does not touch it -- the stores stop at +0x80.
//
// The `_dragDropElement` store appears TWICE in the disassembly, at 0x180120b5a
// (inside the destroy arm) and 0x180120b5e (the merge point), because the
// compiler duplicated the tail. Both are reproduced; the second is redundant
// and harmless, exactly as shipped.
//
// Vtable slot 0 called with 1 is the DELETING destructor: retail destroys AND
// frees the drag-drop element here. UserInterface.h already records that
// ("_dragDropElement  Update destroys it (vslot 0)").
// ============================================================================
extern "C" void RtUi_UserInterface_EndDragDrop(UserInterface* self) {
    ++g_endDragEntries;

    EventRecord rec;                        // deliberately NOT zeroed
    rec.dialog    = self->_dragDropDialog;  // 0x180120afa / 0x180120b04
    rec.element   = nullptr;                // 0x180120b21
    rec.eventId   = 0xe;                    // 0x180120b26  drag-drop CANCEL
    rec.elementId = -1;                     // 0x180120b2e
    rec.userData  = nullptr;                // 0x180120b36
    rec.dragId    = self->_dragDropId;      // 0x180120b09 / 0x180120b10
    rec.dragData  = self->_dragDropData;    // 0x180120b14 / 0x180120b3b
    EventsSetSize()(&self->_events, self->_events._size + 1, &rec);   // 0x180120b40

    self->_dragDropDialog = nullptr;                                  // 0x180120b45

    void* const elem = self->_dragDropElement;                        // 0x180120b49
    if (elem) {
        ++g_endDragDestroyed;
        void** const vt = *reinterpret_cast<void***>(elem);           // 0x180120b52
        // slot 0, flag 1 == deleting destructor (destroy AND free)
        reinterpret_cast<void (*)(void*, uint32_t)>(vt[0])(elem, 1);  // 0x180120b58
        self->_dragDropElement = nullptr;                             // 0x180120b5a
    }
    self->_dragDropElement   = nullptr;                               // 0x180120b5e
    self->_dragDropId        = 0;                                     // 0x180120b62
    self->_dragDropData      = nullptr;                               // 0x180120b65
    self->_dragDropUserData  = nullptr;                               // 0x180120b69
    // +0x88 _dragDropActive is NOT cleared by retail. Left alone.
}

// ============================================================================
// ?EndHover@DialogInstance@UI@@QEAAXPEAVUserInterface@2@@Z  retail 0x180113d10
// ============================================================================
// Clears the dialog's hover flag, emits UI event 0x11 ("end hover") naming the
// dialog, and resets the hover timestamp.
//
// FIELD-NAME CORRECTION SHIPPED WITH THIS PORT. runtime/types/ui/
// DialogInstance.h called +0x98 `_closing` and flagged it "NAME IS A GUESS",
// and +0xa0 `_opaqueA0` "nothing in this lane reads it". Both are now named
// from evidence:
//   * ?IsHovering@DialogInstance@UI@@QEBA_NXZ (0x1800455a0) is the ENTIRE
//     function `MOVZX EAX,byte ptr [RCX+0x98] / RET` -- a one-line exported
//     accessor, the strongest field evidence class this tree recognises.
//     So +0x98 is `_hovering`, and "closing" was actively misleading.
//   * +0xa0 takes (int64)(0.0 / _secondsPerTick) here, i.e. a System::Time
//     tick count, not a pointer. Retyped void* -> int64_t; same 8 bytes.
//
// THE TRAILING STORE IS UNCONDITIONAL. It sits AFTER the `if`, so a dialog that
// was not hovering still gets its timer reset. 0.0 / 1e-6 is 0.0 and truncates
// to 0, so this is a plain zero store in every real run; it is written as the
// division retail wrote (via the exported _secondsPerTick) rather than folded
// to a literal, so it keeps tracking the constant Engine::Startup installs.
// ============================================================================
extern "C" void RtUi_DialogInstance_EndHover(DialogInstance* self, UserInterface* ui) {
    ++g_endHoverEntries;

    if (self->_hovering != 0) {                     // 0x180113d16
        ++g_endHoverPushed;
        self->_hovering = 0;                        // 0x180113d24

        EventRecord rec;                            // deliberately NOT zeroed
        rec.dialog    = self;                       // 0x180113d2b
        rec.element   = nullptr;                    // 0x180113d45
        rec.eventId   = 0x11;                       // 0x180113d39 (a QWORD store,
        rec.elementId = 0;                          //   covering +0x10 AND +0x14)
        rec.userData  = nullptr;                    // 0x180113d4a
        rec.dragId    = 0;                          // 0x180113d51
        rec.dragData  = nullptr;                    // 0x180113d55
        EventsSetSize()(&ui->_events, ui->_events._size + 1, &rec);   // 0x180113d5a
    }

    // 0x180113d5f: XORPS XMM0,XMM0 / DIVSD XMM0,[0x1801fe728] / CVTTSD2SI
    const double* const spt = RT_EXPORT_ONCE(const double*,
                                             "?_secondsPerTick@Timer@System@@0NA");
    self->_hoverTime = static_cast<int64_t>(0.0 / *spt);              // 0x180113d6f
}

// ============================================================================
// ?BeginToolTip@UserInterface@UI@@QEAAX...                  retail 0x180120860
// ============================================================================
// Builds the tooltip dialog for `elem` and queues it for the next Update.
//
// Both InternalPtr<> parameters are passed BY ADDRESS, not by value: MSVC hands
// a class with a non-trivial copy constructor to a callee by hidden reference,
// and the disassembly reads through both (MOV RAX,[R8] at 0x18012088e, MOV
// RCX,[R14] at 0x180120904). element_vtable.cpp:747 already calls it that way,
// passing `&dlgLocal` / `&elemLocal`, so the two agree.
//
// THE FOUR EARLY-OUT GUARDS (0x18012088e..0x1801208b8), all on the ELEMENT:
//     elem->_toolTipDialog (+0x40) != 0        the Dialog template to instance
//     elem->_toolTip.table (+0x48) != 0   \    i.e. UIString::IsValid(), spelled
//     elem->_toolTip.index (+0x50) >= 0   /    out rather than called
//     (elem->_flags (+0x34) & 0x800000) == 0
//
// A RETAIL QUIRK, KEPT AND NOT "FIXED" (0x1801208ca):
//     MOV RAX,[RCX+0x30] ; CMP RAX,RAX ; JNE skip
// compares a register with ITSELF, so the branch is never taken and the guard
// it looks like is inert: an existing tooltip is ALWAYS closed. This is the
// same class of thing as the "retail re-tests `tip`" note at
// element_vtable.cpp:781 -- it is the body, so it is reproduced. Writing the
// comparison the code LOOKS like it wanted (tip->_dataSource != elem) would be
// a behavioural change, not a cleanup.
//
// UNGUARDED DEREF, DELIBERATELY LEFT UNGUARDED: if operator new returns null,
// retail stores null into _toolTipDialog and then hands it straight to
// ToolTipDialog::SetText. Per CLAUDE.md, a faithful crash there is retail's
// contract with its own allocator, not a bug for this port to paper over.
// ============================================================================
extern "C" void RtUi_UserInterface_BeginToolTip(UserInterface* self,
                                               void** dlgPtr, void** elemPtr,
                                               int32_t x, int32_t y) {
    ++g_tipEntries;

    Element* const elem = reinterpret_cast<Element*>(*elemPtr);       // 0x18012088e
    if (elem->_toolTipDialog == nullptr)      { ++g_tipRejected; return; }  // +0x40
    if (elem->_toolTip.table == nullptr)      { ++g_tipRejected; return; }  // +0x48
    if (elem->_toolTip.index < 0)             { ++g_tipRejected; return; }  // +0x50
    if ((elem->_flags & 0x800000u) != 0)      { ++g_tipRejected; return; }  // +0x34

    // --- close whatever tooltip is already up (always, see the quirk above) --
    if (self->_toolTipDialog != nullptr) {                            // 0x1801208be
        ++g_tipClosedPrev;
        void* local = self->_toolTipDialog;                           // 0x1801208d3
        RT_EXPORT_ONCE(CloseDialogFn,
            "?CloseDialog@UserInterface@UI@@QEAAXV?$InternalPtr@VDialogInstance@UI@@@Object@@@Z")
            (self, &local);                                           // 0x1801208de
        self->_toolTipDialog = nullptr;                               // 0x1801208e3
        self->_toolTipOwner  = nullptr;                               // 0x1801208e7
    }

    // --- allocate and construct the ToolTipDialog ---------------------------
    // 0xc0 is retail's literal, and it is why DialogInstance.h's size assert
    // says the ALLOCATED size of a DialogInstance-derived object is 0xc0 while
    // the ctor-initialised extent is 0xb8.
    void* tip = RT_EXPORT_ONCE(OperatorNewFn,
                               "??2ToolTipDialog@UI@@SAPEAX_K@Z")(0xc0);   // 0x1801208f0
    if (tip) {
        void* parentLocal = *dlgPtr;                                  // 0x180120907
        RT_EXPORT_ONCE(DlgInstCtorFn,
            "??0DialogInstance@UI@@QEAA@PEAVUserInterface@1@PEBVDialog@1@"
            "V?$InternalPtr@VDialogInstance@UI@@@Object@@PEAVBase@5@PEAXH@Z")
            (tip, self, elem->_toolTipDialog, &parentLocal, elem, nullptr, 0);  // 0x180120928
        // Stamp ??_7ToolTipDialog@UI@@6B@ over the base ctor's vtable.
        // Reached by NAME: it is exported DATA (0x180157880).
        *reinterpret_cast<void**>(tip) =
            RT_EXPORT_ONCE(void*, "??_7ToolTipDialog@UI@@6B@");        // 0x18012092e
    } else {
        ++g_tipAllocFailed;                                            // 0x18012093a
    }
    self->_toolTipDialog = tip;                                        // 0x18012093d

    // --- decide which string the tooltip shows ------------------------------
    // Retail asks GetToolTipString FIRST purely to test whether it is empty,
    // RELEASES that string, and then asks a SECOND time on the non-empty arm.
    // The double call is retail's (0x180120948 and 0x18012099f); it is not
    // hoisted, because GetToolTipString is not obviously pure and hoisting it
    // would change the refcount traffic the DLL sees.
    RtSystem::String probe = { nullptr };
    RT_EXPORT_ONCE(GetTipStringFn,
        "?GetToolTipString@Element@UI@@QEBA?AVString@System@@XZ")(elem, &probe);  // 0x180120948

    // 0x18012094e..0x180120966: empty == (data == 0) || (length == 0), then
    // SETE inverts it, so `nonEmpty` is what the branch below actually tests.
    const bool nonEmpty = !(probe.data == nullptr ||
                            RtSystem::StringLength(probe.data) == 0);
    ReleaseString(&probe);                                             // 0x18012096a

    RtSystem::String text = { nullptr };
    if (nonEmpty) {
        ++g_tipNonEmpty;
        RT_EXPORT_ONCE(GetTipStringFn,
            "?GetToolTipString@Element@UI@@QEBA?AVString@System@@XZ")(elem, &text);  // 0x18012099f
    } else {
        // A 16-byte copy of the element's UI::String, then the exported
        // conversion operator UI::String -> System::String.
        ::UIString tmp = elem->_toolTip;                               // 0x1801209b4
        RT_EXPORT_ONCE(UiStrCastFn,
            "??BString@UI@@QEBA?AV0System@@XZ")(&tmp, &text);           // 0x1801209c8
    }
    RT_EXPORT_ONCE(SetTextFn,
        "?SetText@ToolTipDialog@UI@@QEAAXAEBVString@System@@@Z")
        (self->_toolTipDialog, &text);                                  // 0x1801209ac / 0x1801209d6
    ReleaseString(&text);                                               // 0x1801209dc

    // --- position it, open it, queue it -------------------------------------
    self->_toolTipOwner = *dlgPtr;                                      // 0x180120a05

    if (self->_toolTipDialog != nullptr) {                              // 0x180120a0c
        // y is offset UP by the tooltip root element's laid-out height, so the
        // tip sits ABOVE the cursor. element_vtable.cpp:761 does the same sum
        // for the tooltip MOVE arm, from the same two offsets.
        void* const root = *reinterpret_cast<void**>(
            reinterpret_cast<uint8_t*>(self->_toolTipDialog) + 0x50);   // 0x180120a15
        const int32_t h = *reinterpret_cast<const int32_t*>(
            reinterpret_cast<const uint8_t*>(root) + 0x64);             // 0x180120a1d
        RT_EXPORT_ONCE(SetUpperLeftFn,
            "?SetUpperLeft@DialogInstance@UI@@QEAAXHH_N@Z")
            (self->_toolTipDialog, x, y - h, false);                    // 0x180120a27
    }

    void* queued = self->_toolTipDialog;                                // 0x180120a2c
    // vtable slot 4 (byte offset 0x20) is DialogInstance::OnOpened.
    void** const vt = *reinterpret_cast<void***>(queued);               // 0x180120a34
    reinterpret_cast<void (*)(void*)>(vt[4])(queued);                   // 0x180120a37

    AddDialogsSetSize()(&self->_addDialogs,
                        self->_addDialogs._size + 1, &queued);          // 0x180120a47
}

// ============================================================================
// REGISTRATION — BOTH DOORS FOR EVERY ROW
// ============================================================================
// `RegisterRuntimeExport` with the mangled name spelled as its OWN string
// literal: tools/runtime_census.py cannot see through a named constant and
// would under-count the lane (reference_census_cannot_see_nonliteral_
// registrations.md). `RegisterRuntimeAddr` too, because the census's `called`
// column and engine/funcs/ reach some of these by a different door, and a body
// registered under only one is dead code under the other
// (reference_count_the_doors_not_the_calls.md).
// ============================================================================
// ---------------------------------------------------------------------------
// THE INPUT SWEEP — how ?IsInsideClipRegion@ is actually verified
// ---------------------------------------------------------------------------
// The other three rows in this file are mouse-driven and an unattended run has
// no mouse, so they cannot be exercised by booting. IsInsideClipRegion is
// different: it is PURE -- twelve instructions, no calls, no writes, reads four
// ints of its argument -- and for a pure function an INPUT SWEEP against the
// shipped export beats waiting for a caller
// (project_pure_functions_verify_by_sweep.md). OURS predicts, THEIRS runs for
// real, and the two are compared; that is a true differential, unlike an
// RT_DIFF_SHADOW self-check (reference_shadow_marker_is_not_an_oracle.md).
//
// THEIRS is reached with RtOriginal, NEVER ResolveExport: once we register, the
// latter hands back our own function and the oracle compares ours to ours.
//
// THE CONTROL. A sweep that reports 0 divergences proves nothing unless a
// KNOWN-WRONG body would have been caught, and the control must be a plausible
// transcription defect on a subject the comparison actually covers
// (reference_control_must_pick_a_compared_subject.md,
// reference_probe_needs_an_effectiveness_control.md). The one used here flips
// the RIGHT edge from inclusive (`x > right` rejects) to exclusive (`x >= right`
// rejects) -- exactly the mistake a reader of `JG` would make -- and it is only
// visible on cases that land EXACTLY on a boundary, which is why the case grid
// is built around the edges rather than sampled at random.
//
// Enable with BANISHED_UIF6_SWEEP=1. Off by default: it calls into the DLL and
// a shipping run has no reason to.
// ---------------------------------------------------------------------------
namespace {

using ClipFn = bool (*)(const void* self, int32_t x, int32_t y);

// The deliberately wrong twin. `x > right` becomes `x >= right`.
bool ClipControlExclusiveRight(const UserInterface* self, int32_t x, int32_t y) {
    const UI::ClipRegion& c = self->_clipRegion;
    if (x < c.left)   return false;
    if (x >= c.right) return false;      // THE INJECTED DEFECT
    if (y < c.top)    return false;
    if (y > c.bottom) return false;
    return true;
}

void RunClipSweep() {
    ClipFn theirs = RtOriginal<ClipFn>("?IsInsideClipRegion@UserInterface@UI@@QEBA_NHH@Z");
    if (!theirs) {
        printf("[UIF6/sweep] SKIPPED -- the DLL export did not resolve "
                    "(cutover, or a wrong name). NOT a pass.\n");
        return;
    }

    // A full UserInterface, so the callee reads a real +0xfc..+0x10b.
    alignas(8) uint8_t buf[sizeof(UserInterface)];
    UserInterface* self = reinterpret_cast<UserInterface*>(buf);

    struct Rect { int32_t l, t, r, b; };
    static const Rect kRects[] = {
        {   0,   0, 1279,  719 },   // a normal screen-sized region
        {   0,   0,    0,    0 },   // degenerate: a single pixel
        {  10,  20,   30,   40 },   // small, offset
        { -50, -60,   50,   60 },   // straddles the origin
        { 100, 100,   50,   50 },   // INVERTED (right < left): admits nothing
        {  -5,  -5,   -1,   -1 },   // wholly negative
    };
    static const int32_t kOffsets[] = { -2, -1, 0, 1, 2 };

    uint64_t cases = 0, mismatches = 0, caught = 0, trueCases = 0;

    for (const Rect& r : kRects) {
        std::memset(buf, 0xAB, sizeof(buf));    // poison: nothing else may be read
        self->_clipRegion.left   = r.l;
        self->_clipRegion.top    = r.t;
        self->_clipRegion.right  = r.r;
        self->_clipRegion.bottom = r.b;

        // Probe AROUND every edge, plus the centre and far-away points, so the
        // inclusive/exclusive boundary is covered from both sides.
        int32_t xs[16], ys[16];
        int nx = 0, ny = 0;
        for (int32_t d : kOffsets) { xs[nx++] = r.l + d; xs[nx++] = r.r + d; }
        xs[nx++] = (r.l + r.r) / 2;  xs[nx++] = 0x7ffffff;  xs[nx++] = -0x7ffffff;
        for (int32_t d : kOffsets) { ys[ny++] = r.t + d; ys[ny++] = r.b + d; }
        ys[ny++] = (r.t + r.b) / 2;  ys[ny++] = 0x7ffffff;  ys[ny++] = -0x7ffffff;

        for (int i = 0; i < nx; ++i) {
            for (int j = 0; j < ny; ++j) {
                const bool a = RtUi_UserInterface_IsInsideClipRegion(self, xs[i], ys[j]);
                const bool b = theirs(self, xs[i], ys[j]);
                const bool c = ClipControlExclusiveRight(self, xs[i], ys[j]);
                ++cases;
                if (a != b) ++mismatches;
                if (c != b) ++caught;
                if (b) ++trueCases;
            }
        }
    }

    // The sweep's own entry counter is inflated by these calls; say so rather
    // than let a reader attribute them to gameplay.
    printf("[UIF6/sweep] IsInsideClipRegion: cases=%llu  divergences=%llu  "
                "(effectiveness: %llu cases returned true, so the comparison is "
                "not trivially satisfied)\n",
                (unsigned long long)cases, (unsigned long long)mismatches,
                (unsigned long long)trueCases);
    printf("[UIF6/sweep] negative control (right edge made EXCLUSIVE): "
                "caught=%llu of %llu. A zero here would mean the sweep cannot "
                "discriminate and the divergences=0 above is worthless.\n",
                (unsigned long long)caught, (unsigned long long)cases);
    printf("[UIF6/sweep] VERDICT: %s\n",
                (mismatches == 0 && caught > 0)
                    ? "PASS -- port agrees with the shipped export on every case, "
                      "and the control proves the sweep would have caught a defect"
                    : "FAIL -- read the two lines above");
    fflush(stdout);
}

}  // namespace

int RtUiHoverRegister() {
    int n = 0;

    if (RtWaveEnabled("BANISHED_RT_UIHOVER")) {
        RegisterRuntimeExport("?IsInsideClipRegion@UserInterface@UI@@QEBA_NHH@Z",
                              (void*)&RtUi_UserInterface_IsInsideClipRegion);
        RegisterRuntimeAddr(0x180120830, (void*)&RtUi_UserInterface_IsInsideClipRegion);  ++n;

        RegisterRuntimeExport("?EndDragDrop@UserInterface@UI@@QEAAXXZ",
                              (void*)&RtUi_UserInterface_EndDragDrop);
        RegisterRuntimeAddr(0x180120af0, (void*)&RtUi_UserInterface_EndDragDrop);         ++n;

        RegisterRuntimeExport("?EndHover@DialogInstance@UI@@QEAAXPEAVUserInterface@2@@Z",
                              (void*)&RtUi_DialogInstance_EndHover);
        RegisterRuntimeAddr(0x180113d10, (void*)&RtUi_DialogInstance_EndHover);           ++n;
    } else {
        printf("[RtPort] BANISHED_RT_UIHOVER=0 -- IsInsideClipRegion/EndDragDrop/"
                    "EndHover left on the DLL\n");
    }

    if (RtWaveEnabled("BANISHED_RT_UITOOLTIP")) {
        RegisterRuntimeExport("?BeginToolTip@UserInterface@UI@@QEAAX"
                              "V?$InternalPtr@VDialogInstance@UI@@@Object@@"
                              "V?$InternalPtr@VElement@UI@@@4@HH@Z",
                              (void*)&RtUi_UserInterface_BeginToolTip);
        RegisterRuntimeAddr(0x180120860, (void*)&RtUi_UserInterface_BeginToolTip);        ++n;
    } else {
        printf("[RtPort] BANISHED_RT_UITOOLTIP=0 -- BeginToolTip left on the DLL\n");
    }

    if (const char* s = std::getenv("BANISHED_UIF6_SWEEP")) {
        if (std::strcmp(s, "0") != 0) RunClipSweep();
    }

    return n;
}
