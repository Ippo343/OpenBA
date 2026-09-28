// ============================================================================
// runtime/src/ui/dialog_create.cpp — Wave 10 lane UI3
//   The four allocating UI::DialogInstance rows Wave 7 lane B held back.
// ============================================================================
//
//   | Export (short form)                    | Retail      | Ord  |
//   |----------------------------------------|-------------|------|
//   | ??0DialogInstance (ctor)               | 0x180114870 |  147 |
//   | ?AddController@ (…ExtPtr<ED>, String&) | 0x180113470 | 1541 |
//   | ?AddController@ (…IntPtr<Element>)     | 0x1801133b0 | 1542 |
//   | ?StartTimer@                           | 0x1801137c0 | 4081 |
//
// All four mangled names were read back out of the shipped export directory of
// C:\GOG Games\Banished\Runtime-x64.dll with tools/pe_exports.py BEFORE a line
// of body was written, and all four are present at exactly those addresses
// (reference_resolveexport_phantom_names.md — a name the DLL does not contain
// resolves to NULL in silence and the census still counts the registration).
// The full query output is in docs/runtime_port/WAVE10_LANE_UI3.md §1.
//
// ---------------------------------------------------------------------------
// THE BLOCKER THESE FOUR SAT BEHIND, AND WHY IT IS ANSWERED
// ---------------------------------------------------------------------------
// WAVE7_LANE_B.md §2a held all four back under one heading: "eight allocate or
// free — split ownership, and no oracle can be built". Three sub-claims, each
// re-derived here rather than inherited
// (reference_holdback_blockers_are_stale_check_them_first.md):
//
//  1. "`EnterCriticalSection` is Win32 and cannot appear in a non-`_win` file."
//     TRUE OF THE INLINED FORM, and irrelevant. Every allocation these four
//     make is the byte-for-byte body of an EXPORTED static:
//         ?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z   0x1800c7a50
//             = Enter(_singleton+0x640); Allocate(_singleton,n,align); Leave
//         ?MemFree@MemoryAllocator@System@@SAXPEAX@Z      0x1800c7ad0
//     runtime/src/collection/array.cpp made exactly this call three waves ago
//     and states the argument: "calling it IS retail's body minus the inlining,
//     and it keeps <windows.h> out of runtime/src/". This file follows it and
//     uses the SAME two accessors, by address, for the same reason.
//
//  2. "The node it allocates is freed by `??1DialogInstance`, which stays in
//     the DLL." BOTH HALVES ARE NOW FALSE. `??1DialogInstance@UI@@` (0x180114d40)
//     was ported by Wave 9 lane UID and is registered in dialog.cpp. And even
//     when the DLL's own dtor runs (it still does, through the DLL's own direct
//     near-CALLs, which no registry can redirect), it frees into the SAME heap:
//     `MemAlloc`/`MemFree` reach 0x1800c7a50/0x1800c7ad0, which is the DLL's
//     allocator port. THERE IS NO CONFIGURATION IN WHICH THE TWO SIDES SEE
//     DIFFERENT HEAPS. That is the whole of the "split ownership" objection.
//
//  3. "`MemFree` is Wave 2's allocator port, which shipped opt-in with a NO-GO."
//     EXPIRED. docs/runtime_port/WAVE10_LANE_ALLOC.md returns GO: armed with
//     the DLL present, 901,685 allocations and 540,750 frees served by the
//     ported bodies in 30 s, a 3,000-frame redzone stress town with ZERO
//     violations, and an unarmed run byte-identical in behaviour. It remains
//     OPT-IN, which is why point 2 above is stated for BOTH settings.
//
// ---------------------------------------------------------------------------
// TWO UNEXPORTED ARRAY HELPERS, AND WHY REGISTERING THEM IS NOT NEW WORK
// ---------------------------------------------------------------------------
// `?StartTimer@` tail-calls 0x180076250 and the 4-arg `?AddController@`
// tail-calls 0x180076370. Neither is an export. Neither needed porting:
//
//   0x180076250  ≡  0x18005fb00   (registered, `kSpec_Node18`, stride 0x18)
//   0x180076370  ≡  0x180076900   (registered, `kSpec_Fill8Z`, stride 8)
//
// "≡" is a MEASUREMENT, not an impression: both pairs were compared
// instruction-for-instruction with the leading address stripped and every
// INTERNAL branch target rewritten as a body-relative delta, so every operand
// naming anything outside the body (0x1800c7a50, 0x1801fc718, 0x18014b390,
// 0x1800c6c00, 0x18014b388) had to match ABSOLUTELY
// (reference_position_only_diffs_hide_one_body.md). Both pairs: 0 differing
// instructions. THE INSTRUMENT WAS CONTROLLED — the same comparison between
// the stride-8 body and the stride-0x18 body reports 62 differing instructions,
// so it is not structurally incapable of saying "different"
// (reference_diagnostic_field_cannot_express_the_answer.md). The two extra
// `RegisterRuntimeAddr` lines live in runtime/src/collection/array.cpp beside
// the bodies they name, NOT here, because a private second registration of a
// body another file owns is the drift that parameterisation exists to prevent.
//
// ---------------------------------------------------------------------------
// FAITHFULNESS NOTES A "TIDY" PORT WOULD DESTROY
// ---------------------------------------------------------------------------
//  * `?StartTimer@` finds an existing entry with the same id, rewrites its two
//    `Time` fields, AND THEN FALLS THROUGH AND APPENDS A SECOND ENTRY ANYWAY.
//    There is no `return` and no `else` at 0x1801137fd. Retail accumulates
//    duplicate timer ids and `?StopTimer@` removes only the first match.
//    WAVE7_LANE_B.md §6c wrote this down for whoever landed the body; this is
//    that landing, and the behaviour is reproduced, not repaired.
//  * The 4-arg `?AddController@` DEREFERENCES A NULL when the type id is not in
//    `Object::Factory`'s tree (0x1801133f4 sets RAX to the zero register and
//    0x1801133f7 immediately reads [RAX+0x10]), and again when the constructed
//    object is not a `UI::DialogController` (the `CMOVNZ` at 0x18011341e leaves
//    RDI at zero and 0x180113422 writes [RDI+0x10]). Both are reproduced.
//    CLAUDE.md: retail's unguarded derefs are load-bearing; a guard does not
//    restore the behaviour, it skips it.
//  * The 6-arg `?AddController@` calls `Array::SetSize` on `RBX+0xa0` and then
//    ORs `[RBX+0x34]` with RBX possibly null, for the same reason.
//  * The 6-arg's element-name test compares LENGTHS FIRST and only then calls
//    `StrCompare`, and its "lengths equal" arm re-tests the pointer and the
//    length a second time. Transcribed as written.
//  * `?StartTimer@`'s appended element leaves bytes +0x14..0x17 of the 0x18-byte
//    record UNWRITTEN — retail builds the fill record on the stack and never
//    touches that word. The differential oracle EXCLUDES those four bytes per
//    element; comparing them would fail 100 % of calls and read as a
//    catastrophic port defect
//    (reference_oracle_must_not_compare_unwritten_bytes.md).
//
// PORTABILITY: no <windows.h>, per PLAN.md §4.1 decision 1.
// ============================================================================

// *** INCLUDE ORDER IS LOAD-BEARING. ***
// `runtime/types/object/External.h` opens `namespace RtObject {` and then
// `#include "TypeInfo.h"` INSIDE it, so whichever translation unit pulls
// External.h first nests every TypeInfo declaration into `RtObject::RtObject`
// for the whole TU — which is why dialog.cpp records that TypeInfo.h "IS NOT
// INCLUDABLE HERE". Including it FIRST fixes that for this TU: `#pragma once`
// then makes External.h's nested include a no-op. Do not move this line.
#include "../../types/object/TypeInfo.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/system/String.h"
#include "../../types/collection/Collection.h"
#include "../../types/ui/Dialog.h"
#include "../../types/ui/DialogInstance.h"
#include "../../types/ui/DialogController.h"   // B.3.9 TI-UI: X::TypeId()
#include "../../types/ui/GroupElement.h"
#include "../../types/ui/Element.h"       // ENDGAME B.3.6 closing pass: FindElement
#include "../../types/ui/SpriteSheet.h"   // ENDGAME B.3.6 closing pass: GetSpriteIndexFromName
#include "../../types/system/MemoryAllocator.h"
#include <new>
#include "../collection/array_internal.h" // ENDGAME B.3.2 step 2 — kSpec_Fill8Z

using RtUI::Dialog;
using RtUI::DialogController;
using RtUI::DialogInstance;
using RtUI::DialogTimer;

namespace {

// ============================================================================
// Offsets this file needs that live in ANOTHER lane's class.
//
// runtime/types/ui/Element.h owns these and asserts every one of them; they are
// re-spelled as named constants rather than included because Element.h pulls
// the UI::String value type and this file has no other use for it. Each carries
// the instruction that pins it, which is the convention
// runtime/types/ui/UserInterface.h states for foreign offsets.
// ============================================================================
namespace EL {
enum : size_t {
    kName          = 0x10,   // ?SetName@   0x1801154f0            (Element.h)
    kFlags         = 0x34,   // ?GetElementFlags@ 0x1800354d0      (Element.h)
    kDialogParent  = 0x90,   // ctor 0x180114965: root->[0x90] = this
    kParent        = 0x98,   // ?SetParent@ 0x1801154d0            (Element.h)
    kChildrenData  = 0xa0,   // ?Child@      0x180035540           (Element.h)
    kChildrenSize  = 0xa8,   // ?ChildCount@ 0x180035530           (Element.h)
};
}  // namespace EL

// Element::_flags bit the 6-arg AddController propagates up the parent chain.
// `OR dword ptr [RBX+0x34],0x8000000` at 0x180113620. Element.h calls bit 27
// kElementFlagNeedsSizeCalc.
constexpr uint32_t kElementNeedsSizeCalc = 0x8000000u;

// ---- The allocator boundary ------------------------------------------------
// `?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z` (0x1800c7a50), called
// directly (B3R RT-UI).
void* MemAlloc(uint64_t bytes, int align) {
    return System::MemoryAllocator::MemAlloc(bytes, align);
}

// ---- Probe scheduling ------------------------------------------------------
//
// *** THE DIFFERENTIAL MUST NOT RUN FROM THE REGISTER FUNCTION. *** That runs
// inside `RuntimePortInit()`, immediately after `ResolveInit()`'s LoadLibrary —
// at which point the DLL's `MemoryAllocator` has been CONSTRUCTED but not
// STARTED (`Engine::Startup` does that later). `?StartTimer@` allocates, so a
// ---- Byte cursor -----------------------------------------------------------
inline uint8_t* B(void* p, size_t off) { return static_cast<uint8_t*>(p) + off; }
inline const uint8_t* B(const void* p, size_t off) {
    return static_cast<const uint8_t*>(p) + off;
}
template <class T> inline T& F(void* p, size_t off) {
    return *reinterpret_cast<T*>(B(p, off));
}
template <class T> inline const T& F(const void* p, size_t off) {
    return *reinterpret_cast<const T*>(B(p, off));
}

// ============================================================================
// Callees, called by name (B3R RT-UI).
// ============================================================================

// ENDGAME B.3.2 step 2 — the three forwarders that used to sit here are GONE.
// All three fields are real `Collection::Array<T>` members now
// (DialogInstance.h `_timers`/`_controllers`, Element.h `_children`), and all
// three rows are TRAIT-EQUIVALENT for the field's element type:
//
//   0x180076250  kSpec_Node18  {0x18,8,Assign18(full byte copy),init=null,
//                destroy=null,byteCopy} == SpecFor<DialogTimer>() field for
//                field: DialogTimer is a trivially copyable/constructible/
//                destructible 0x18-byte POD, so the trait-derived
//                `Array<DialogTimer>::SetSize` IS that instantiation.
//   0x180076370  kSpec_Fill8Z  {8,8,Assign8Z(zero-then-copy; net effect is a
//                full 8-byte write, same bytes a placement copy-construct of
//                a pointer produces),init=null,destroy=null,byteCopy} ==
//                SpecFor<DialogController*>().
//   0x180076900  kSpec_Fill8Z, same reasoning, == SpecFor<Element*>() for
//                `Element::_children`.
//
// No `ExplicitSpec` bridge needed at any of the three call sites below.

// ?GetSpriteIndexFromName@SpriteSheet@UI@@QEBAHAEBVString@System@@@Z  0x1800031f0
// ENDGAME B.3.6 closing pass: the real member (was a function-local lazy
// ResolveExport cache, a RULE B6-5 shape on a loader-thread path).
int32_t GetSpriteIndexFromName(const void* sheet, const void* nameString) {
    return reinterpret_cast<const UI::SpriteSheet*>(sheet)->GetSpriteIndexFromName(  // B.3.6 bridge
        *static_cast<const System::String*>(nameString));
}

// ?IsDerivedFrom@Base@Object@@QEBA_NI@Z  0x1800c0390.
bool IsDerivedFrom(const void* obj, uint32_t typeId) {
    return reinterpret_cast<const Object::Base*>(obj)->IsDerivedFrom(typeId);   // B.3.x bridge
}


// ?FindElement@Element@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@AEBVString@System@@@Z
// 0x180115650. HIDDEN RETURN SLOT in RDX, returns that same pointer in RAX
// (reference_hidden_retslot_wild_write.md — an fn_* typedef that omits the slot
// is a wild write that still returns the right answer).
// ENDGAME B.3.6 closing pass: the real member, its prvalue placement-new'd into
// the caller's slot exactly as the door does (was a lazy ResolveExport cache).
void** ElementFindElement(void* self, void** out, const void* nameString) {
    ::new (static_cast<void*>(out)) Object::InternalPtr<UI::Element>(
        reinterpret_cast<UI::Element*>(self)->FindElement(  // B.3.6 bridge
            *static_cast<const System::String*>(nameString)));
    return out;
}

// ??0Dialog@UI@@QEAA@XZ 0x180112db0 / ??1Dialog@UI@@UEAA@XZ 0x180112e40: the
// 6-arg AddController builds a default UI::Dialog on its stack with both
// bodies INLINED. ENDGAME B.3.9 UI-D: `::new (p) UI::Dialog()` and
// `~T()` (slot 0, flags 0) (the door pair is retired).

// The two type ids the bodies test against. Retail loads the FIRST DWORD of
// each class's `_typeInfo` (`MOV EDX,dword ptr [0x1801f9e48]` for
// DialogController, 0x1801fabd8 for GroupElement): `X::TypeId()`, read at the
// use. ENDGAME B.3.9 TI-UI: the by-name lookup, its first-use cache and the
// WAVE 10C `[TidStatic]` attendance line (it reported which minted node the
// lookup read; there is no minted node left to report) are gone.

// ?_singleton@Factory@Object@@0PEAV12@EA — the class static now (ENDGAME
// B.3.9 SS1, B39_RULES D7): read fresh, no lazy cache (B9-10).
RtObject::Factory* FactorySingleton() {
    return reinterpret_cast<RtObject::Factory*>(::Object::Factory::_singleton);
}

// ============================================================================
// ?StartTimer@DialogInstance@UI@@QEAAXHVTime@System@@@Z   0x1801137c0
//
//     void DialogInstance::StartTimer(int id, System::Time t)
//
// `System::Time` is ONE int64 tick count and is passed BY VALUE in R8 — proved
// by the body itself, which stores R8 into the record's +0x00 and reloads the
// SAME home slot for +0x08 (0x1801137eb / 0x1801137f3). Both fields get the
// same value; DialogInstance.h names them `_remaining` and `_period` and calls
// the second one "the reload", which is what an equal seed means.
//
// The loop cursor is a BYTE offset stepped by 0x18 (`ADD RAX,0x18`), and the id
// it compares lives at record+0x10 — the two facts DialogInstance.h pinned
// `DialogTimer` from, and the two the displacement oracle re-pins out of the
// shipped bytes below.
//
// THE FALL-THROUGH IS THE SHIPPED BEHAVIOUR. After rewriting a matching entry
// the body does NOT return; it walks to the end of the loop and appends another
// record with the same id (WAVE7_LANE_B.md §6c). Ported as written.
// ============================================================================
void RtUiDialogInstance_StartTimer(void* self, int32_t id, int64_t time) {
    // B.3.6: the body is UI::DialogInstance::StartTimer (bottom of file).
    reinterpret_cast<UI::DialogInstance*>(self)             // B.3.6 bridge
        ->UI::DialogInstance::StartTimer(id, time);
}

// ============================================================================
// ?AddController@DialogInstance@UI@@QEAAX I PEAVBase@Object@@
//     V?$ExternalPtr@VDialogControllerConfig@UI@@@4@
//     V?$InternalPtr@VElement@UI@@@4@ @Z                       0x1801133b0
//
//     void DialogInstance::AddController(unsigned typeId, Object::Base* dataSource,
//                                        ExternalPtr<DialogControllerConfig> config,
//                                        InternalPtr<Element> container)
//
// ARGUMENT ABI, read off the homing stores rather than from the mangled name:
//   RCX  this
//   EDX  typeId
//   R8   Object::Base* dataSource          -> controller+0x28
//   R9   the ExternalPtr's 8 bytes BY VALUE -> controller+0x10
//   [rsp+0x28]  a POINTER to the InternalPtr — 0x180113416 loads the argument
//   and 0x18011341b DEREFERENCES it before storing. engine_funcs.h's
//   `fn_DialogInstance_AddControllerTab_t` already spells it that way and its
//   exe call sites pass an address; the two agree.
//
// THE TWO UNGUARDED NULL DEREFERENCES ARE RETAIL'S. See the banner.
// ============================================================================
void RtUiDialogInstance_AddControllerTab(void* self, uint32_t typeId, void* dataSource,
                                         void* config, void** containerPtr) {
    // B.3.6: the body is UI::DialogInstance::AddController (4-arg, bottom of
    // file). The InternalPtr arrives BY ADDRESS; it is dereferenced here, on
    // both of retail's arms, exactly where 0x18011341b dereferences it.
    reinterpret_cast<UI::DialogInstance*>(self)             // B.3.6 bridge
        ->UI::DialogInstance::AddController(
            typeId, static_cast<Object::Base*>(dataSource),
            Object::ExternalPtr<UI::DialogControllerConfig>(
                static_cast<UI::DialogControllerConfig*>(config)),
            Object::InternalPtr<UI::Element>(static_cast<UI::Element*>(*containerPtr)));
}

// ============================================================================
// ?AddController@DialogInstance@UI@@QEAAX I PEAVBase@Object@@
//     V?$ExternalPtr@VDialogControllerConfig@UI@@@4@
//     V?$ExternalPtr@VElementDescription@UI@@@4@
//     AEBVString@System@@ @Z                                   0x180113470
//
//     void DialogInstance::AddController(unsigned typeId, Object::Base* dataSource,
//                                        ExternalPtr<DialogControllerConfig> config,
//                                        ExternalPtr<ElementDescription> desc,
//                                        const System::String& name)
//
// Three phases:
//   1. RESOLVE the insertion element by name — the dialog root itself if its
//      name matches, else the first hit of `Element::FindElement` over the
//      root's direct children, else null; then require it to be a
//      `UI::GroupElement` (`?_typeInfo@GroupElement@UI@@`, 0x1801fabd8).
//   2. CREATE the element: default-construct a temporary `UI::Dialog`, call the
//      description's vtable slot 8 with (out, &tmpDialog), parent the result
//      into the resolved element, append it to that element's `_children`, and
//      OR kElementNeedsSizeCalc up the whole parent chain. Then destroy tmp.
//   3. FORWARD to the 4-arg overload with the created element, if id != 0.
//
// THE TEMPORARY IS A `UI::Dialog`, NOT AN `ElementDescription`. The inlined
// ctor stamps `??_7Dialog@UI@@6B@` (0x1801562a0) as its third vtable — the
// ElementDescription ctor stamps 0x1801561c0. Getting that wrong would have put
// the wrong object on the description's slot-8 call. See lane doc §3.
//
// WHEN `desc` IS NULL the whole of phases 1 and 2 is skipped (0x1801134aa) and
// the body goes straight to phase 3 with a null element.
// ============================================================================
void RtUiDialogInstance_AddController6(void* self, uint32_t typeId, void* dataSource,
                                       void* config, void* desc, const void* nameString) {
    // B.3.6: the body is UI::DialogInstance::AddController (5-arg, bottom of file).
    reinterpret_cast<UI::DialogInstance*>(self)             // B.3.6 bridge
        ->UI::DialogInstance::AddController(
            typeId, static_cast<Object::Base*>(dataSource),
            Object::ExternalPtr<UI::DialogControllerConfig>(
                static_cast<UI::DialogControllerConfig*>(config)),
            Object::ExternalPtr<UI::ElementDescription>(
                static_cast<UI::ElementDescription*>(desc)),
            *static_cast<const System::String*>(nameString));
}

// (ENDGAME B.3.9 UI-D: the ??0DialogInstance door `RtUiDialogInstance_Ctor`
// (0x180114870) is retired; the exe's last three door sites spell
// `::new (p) UI::DialogInstance(ui, dialog, parent, dataSource, userData,
// userFlags)`. The ctor's body is `InitFields`, below.)

}  // namespace

// ============================================================================
// UI::DialogInstance::InitFields — the body of ??0DialogInstance 0x180114870
// after `CALL ??0Internal` and the vptr store (ENDGAME B.3.8 block RB, B8-1).
// The real constructor's body (DialogInstance.h); since ENDGAME B.3.9 UI-D no
// door shares it. MOVED VERBATIM from the retired door: it still
// works through the field-identical `RtUI::` POD views (cross-asserted in
// DialogInstance.h), so not one store changed.
// ============================================================================
namespace UI {

void DialogInstance::InitFields(UserInterface* uiArg, const Dialog* dialogArg,
                                const Object::InternalPtr<DialogInstance>* parentArg,
                                Object::Base* dataSourceArg, void* userData,
                                int32_t userFlags) {
    void* const self = this;
    void* const ui = uiArg;
    const void* const dialog = dialogArg;
    void* const dataSource = dataSourceArg;
    void* const* const parentPtr = reinterpret_cast<void* const*>(parentArg);   // B.3.8 bridge
    RtUI::DialogInstance* const d = reinterpret_cast<RtUI::DialogInstance*>(this);  // B.3.8 bridge
    const RtUI::Dialog* const dlg = static_cast<const RtUI::Dialog*>(dialog);
    d->_ui         = ui;                          // +0x10
    d->_dialog     = const_cast<void*>(dialog);   // +0x18
    d->_flags      = dlg->_defaultFlags;          // +0x20  from _dialog->+0x40
    d->_result     = 0;                           // +0x24
    d->_parent     = nullptr;                     // +0x28  written 0 then...
    d->_parent     = *parentPtr;                  // +0x28  ...the real value
    d->_dataSource = dataSource;                  // +0x30
    d->_children._head  = nullptr;                // +0x38
    d->_children._tail  = nullptr;                // +0x40
    d->_children._count = 0;                      // +0x48   (+0x4c left alone)

    // 0x1801148e7: `_dialog->_rootDescription`'s vtable slot 8, with the hidden
    // return slot pointed AT THE OBJECT'S OWN `_root` FIELD and the Dialog as
    // the second argument — the same slot the 6-arg AddController calls.
    static_cast<const UI::ElementDescription*>(dlg->_rootDescription)->CreateInstance(
        reinterpret_cast<Object::InternalPtr<UI::Element>*>(&d->_root),   // B.3.x bridge
        dialogArg);

    d->_spriteSheet = dlg->_spriteSheet;           // +0x58
    // 0x180114900: -1 when the sheet is null, otherwise the sheet's lookup of
    // `&_dialog->_spriteName` — the String OBJECT's address, not the char*.
    d->_spriteIndex = d->_spriteSheet
        ? GetSpriteIndexFromName(d->_spriteSheet, &dlg->_spriteName)
        : -1;                                      // +0x60   (+0x64 left alone)

    d->_timers._data      = nullptr;               // +0x68
    d->_timers._size      = 0;                     // +0x70
    d->_timers._capacity  = 0;                     // +0x74
    d->_controllers._data     = nullptr;           // +0x78
    d->_controllers._size     = 0;                 // +0x80
    d->_controllers._capacity = 0;                 // +0x84
    d->_upperLeftX = 0;                            // +0x88
    d->_upperLeftY = 0;                            // +0x8c
    d->_hoverMouseX = -1;                          // +0x90  a QWORD -1 --
    d->_hoverMouseY = -1;                          // +0x94  both hover ints
    d->_hovering    = 0;                           // +0x98  a single BYTE
    d->_hoverTime   = 0;                           // +0xa0   (+0x99.. left alone)
    d->_userData    = userData;                    // +0xa8
    d->_userFlags   = userFlags;                   // +0xb0   (+0xb4 left alone)

    // 0x180114965: the freshly created root Element points back at us.
    // UNGUARDED in retail — if slot 8 returned null this faults. Faithful.
    F<void*>(d->_root, EL::kDialogParent) = self;

    // ---- link into the parent's child list -------------------------------
    // A PREPEND: head at +0x38, tail at +0x40, node = {element, prev, next}
    // with prev at +0x08 and next at +0x10. DialogInstance.h records the
    // tie-break (`?GetFirstDialog@UserInterface@UI@@`, 0x180031150, reads the
    // FIRST element from the list's +0x00) and warns that reading the ctor
    // alone gets this backwards.
    if (RtUI::DialogInstance* const parent = static_cast<RtUI::DialogInstance*>(d->_parent)) {
        // 0x180114999: `MOV EDX,0x18 / LEA R8D,[RDX-0x10]` -> Allocate(0x18, 8),
        // wrapped in Enter/Leave — i.e. exactly ?MemAlloc@…(0x18, 8).
        uint8_t* node = static_cast<uint8_t*>(MemAlloc(0x18, 8));
        if (node) {
            F<void*>(node, 0x00) = self;      // element
            F<void*>(node, 0x08) = nullptr;   // prev
            F<void*>(node, 0x10) = nullptr;   // next
        }
        // 0x1801149cf: on an OOM `node` is null and retail links the null in
        // anyway. Reproduced, guard-free.
        if (void* head = parent->_children._head) {
            F<void*>(head, 0x08) = node;                      // old head->prev
            F<void*>(node, 0x10) = parent->_children._head;   // node->next
        } else {
            parent->_children._tail = reinterpret_cast<decltype(parent->_children._tail)>(node);
        }
        parent->_children._head = reinterpret_cast<decltype(parent->_children._head)>(node);
        parent->_children._count++;
    }

    // ---- run the dialog's controller table -------------------------------
    // Stride 0x20 (`ADD RDI,0x20`).
    //
    // *** THE ARGUMENT MAPPING IS THE OPPOSITE OF THE FIELD NAMES, AND THIS
    //     PORT HAD IT BACKWARDS UNTIL THE DISASSEMBLY WAS RE-READ. ***
    //
    //     0x180114a13  MOV RAX,[R9+0x08]  ->  0x180114a17  [RSP+0x20]  = ARG 5
    //     0x180114a1c  MOV R9,[R9+0x10]                                = ARG 4
    //
    // Argument 4 is the `ExternalPtr<DialogControllerConfig>` and argument 5 is
    // the `ExternalPtr<ElementDescription>`, so entry+0x10 is the CONFIG and
    // entry+0x08 is the DESCRIPTION.
    //
    // *** 2026-09-08, lane DLG2: THE HEADER IS NOW CORRECT AND THIS CALL SITE
    //     NO LONGER COMPENSATES. *** `runtime/types/ui/Dialog.h` used to name
    //     `_config` at +0x08 and `_elementDesc` at +0x10, so this loop passed
    //     them crossed over to get the right bytes into the right registers.
    //     The header now names them the way the two `MOV`s above prove, and the
    //     arguments below read straight. THE BYTES DID NOT CHANGE; only the
    //     spelling did. The three PIN lines at the bottom of this file check
    //     exactly this mapping against the shipped stream, so a re-swap in
    //     either place fails loudly rather than silently.
    for (int32_t i = 0; i < dlg->_controllerCount; ++i) {
        const RtUI::DialogControllerConfigEntry& e = dlg->_controllerConfigs[i];
        // B.3.6: was `ResolveAddr(Module::Runtime, 0x180113470)` — this
        // block's own non-virtual member now, called directly. `self` is still
        // the POD this door builds, hence the bridge.
        reinterpret_cast<UI::DialogInstance*>(self)->AddController(   // B.3.6 bridge
            e._typeId, static_cast<Object::Base*>(d->_dataSource),
            Object::ExternalPtr<UI::DialogControllerConfig>(
                static_cast<UI::DialogControllerConfig*>(e._config)),       // entry+0x10 -> R9
            Object::ExternalPtr<UI::ElementDescription>(
                static_cast<UI::ElementDescription*>(e._elementDesc)),      // entry+0x08 -> a5
            *reinterpret_cast<const System::String*>(&e._name));  // B.3.6 bridge  entry+0x18 -> a6
    }
}

}  // namespace UI

// ############################################################################
// ENDGAME B.3.6 block B — StartTimer and the two AddController overloads are
// UI::DialogInstance members now. Bodies unchanged except `self->x` became `x`
// and the calls onto THIS block's classes became member calls:
//   controller vtable +0x20 (slot 4)     -> c->OnCreated()      (VIRTUAL)
//   ResolveAddr(Module::Runtime, 0x1801133b0) -> AddController(...) (4-arg)
// Element / Factory / SpriteSheet / Dialog-temporary calls stay as they were
// (other blocks' classes, or ctor/dtor doors). Evidence: the doors' banners.
// ############################################################################
namespace UI {

// ?StartTimer@DialogInstance@UI@@QEAAXHVTime@System@@@Z   0x1801137c0
// THE FALL-THROUGH IS THE SHIPPED BEHAVIOUR: a matching entry is rewritten
// and ANOTHER record with the same id is appended anyway.
void DialogInstance::StartTimer(int32_t id, int64_t time) {
    for (int32_t i = 0; i < _timers._size; ++i) {
        // _data is RE-READ from the object on every iteration. Kept.
        uint8_t* const rec = reinterpret_cast<uint8_t*>(_timers._data) +
                             static_cast<size_t>(i) * sizeof(DialogTimer);
        if (F<int32_t>(rec, offsetof(DialogTimer, _id)) == id) {
            F<int64_t>(rec, offsetof(DialogTimer, _remaining)) = time;
            uint8_t* const rec2 = reinterpret_cast<uint8_t*>(_timers._data) +
                                  static_cast<size_t>(i) * sizeof(DialogTimer);
            F<int64_t>(rec2, offsetof(DialogTimer, _period)) = time;
            // NO `break`, NO `return`.
        }
    }
    // +0x14..0x17 is never written; `tmp` is deliberately left uninitialised
    // there for the same reason as the door's banner gives.
    DialogTimer tmp;
    tmp._remaining = time;
    tmp._period    = time;
    tmp._id        = id;
    _timers.SetSize(_timers.GetSize() + 1, tmp);   // retail 0x180076250
}

// ?AddController@…@V?$InternalPtr@VElement@UI@@@4@@Z   0x1801133b0
// THE TWO UNGUARDED NULL DEREFERENCES ARE RETAIL'S (a null factory node; a
// non-controller leaves `c` null and the four stores write through it).
void DialogInstance::AddController(uint32_t typeId, Object::Base* dataSource,
                                   Object::ExternalPtr<DialogControllerConfig> config,
                                   Object::InternalPtr<Element> container) {
    // Object::Factory's AVL find, inlined by retail (0x1801133c1).
    RtObject::Factory* const factory = FactorySingleton();
    RtObject::TypeNode* node = factory ? factory->root : nullptr;
    RtObject::TypeNode* found = nullptr;
    while (node) {
        const uint32_t key = node->key;
        if (typeId < key)      node = node->left;
        else if (key >= typeId) { found = node; break; }
        else                    node = node->right;
    }

    // 0x1801133f7 `MOV RCX,[RAX+0x10]` with RAX possibly zero. FAITHFUL.
    RtObject::BaseTypeInfo* const info =
        *reinterpret_cast<RtObject::BaseTypeInfo* const*>(B(found, 0x10));

    // info+0x20 (`factoryFn`), RCX left holding `info`.
    void* const created =
        reinterpret_cast<void* (*)(void*)>(info->factoryFn)(info);

    const bool isController = ::IsDerivedFrom(created, UI::DialogController::TypeId());

    // 0x18011341e `CMOVNZ RDI,RBX` — a non-controller leaves the target NULL.
    DialogController* const c =
        static_cast<DialogController*>(isController ? created : nullptr);

    c->_config     = config;                    // +0x10  0x180113422
    c->_dataSource = dataSource;                // +0x28  0x180113426
    c->_dialog     = this;                      // +0x18  0x18011342a
    c->_container  = static_cast<void*>(container.Get());   // +0x20  0x18011342e

    c->OnCreated();                             // 0x18011343d — controller slot 4

    _controllers.SetSize(_controllers.GetSize() + 1, c);   // retail 0x180076370
}

// ?AddController@…@V?$ExternalPtr@VElementDescription@UI@@@4@AEBVString@System@@@Z
//                                                          0x180113470
void DialogInstance::AddController(uint32_t typeId, Object::Base* dataSource,
                                   Object::ExternalPtr<DialogControllerConfig> config,
                                   Object::ExternalPtr<ElementDescription> description,
                                   const System::String& name) {
    void* const desc = static_cast<void*>(description.Get());
    void* createdElement = nullptr;   // RDI

    if (desc) {
        void* insertion = _root;      // RDI starts at this->_root (0x1801134b4)

        // ---- phase 1a: does the ROOT's own name match? --------------------
        const char* const want = *reinterpret_cast<const char* const*>(&name);
        bool useRoot = false;
        if (want && *reinterpret_cast<const int32_t*>(want - 8) != 0) {
            // 0x1801134cb `MOV RCX,[RDI+0x10]` — UNGUARDED on `insertion`.
            const char* const have =
                *reinterpret_cast<const char* const*>(B(insertion, EL::kName));
            const int32_t haveLen =
                have ? *reinterpret_cast<const int32_t*>(have - 8) : 0;
            if (haveLen == *reinterpret_cast<const int32_t*>(want - 8)) {
                if (!have) useRoot = true;
                else if (*reinterpret_cast<const int32_t*>(have - 8) == 0) useRoot = true;
                else if (System::String::StrCompare(have, want) == 0) useRoot = true;
            }
        }

        // ---- phase 1b: otherwise scan the root's DIRECT children ----------
        if (!useRoot) {
            const int32_t n =
                *reinterpret_cast<const int32_t*>(B(insertion, EL::kChildrenSize));
            void* hit = nullptr;
            if (n > 0) {
                void** kids = *reinterpret_cast<void***>(B(insertion, EL::kChildrenData));
                for (int32_t i = 0; i < n; ++i) {
                    void* const child = kids[i];
                    if (child) {
                        void* out = nullptr;
                        ElementFindElement(child, &out, &name);
                        if (out) { hit = out; break; }
                    }
                }
            }
            insertion = hit;          // 0x180113538: a miss zeroes RDI
        }

        // ---- the GroupElement gate ---------------------------------------
        void* const target =
            ::IsDerivedFrom(insertion, UI::GroupElement::TypeId()) ? insertion : nullptr;

        // ---- phase 2: build the element through a UI::Dialog temporary ----
        alignas(16) uint8_t tmpDialog[0x90];
        UI::Dialog* const tmpDlg = ::new (static_cast<void*>(tmpDialog)) UI::Dialog();

        void* newEl = nullptr;
        {
            // The description's slot 8 (CreateInstance, RETSLOT, B6-2).
            Object::InternalPtr<Element>* const ret = description.Get()->CreateInstance(
                reinterpret_cast<Object::InternalPtr<Element>*>(&newEl),   // B.3.x bridge
                tmpDlg);
            newEl = *reinterpret_cast<void**>(ret);   // 0x1801135ef `MOV RDI,qword ptr [RAX]`
        }
        createdElement = newEl;

        if (newEl) F<void*>(newEl, EL::kParent) = target;   // 0x1801135fc

        // 0x180113603: unguarded on `target`; Element::_children through the
        // kSpec_Fill8Z row (see the door's banner).
        rtcoll::AsArray<&rtcoll::kSpec_Fill8Z>(B(target, EL::kChildrenData))
            ->SetSizeFill(
                *reinterpret_cast<int32_t*>(B(target, EL::kChildrenSize)) + 1,
                &newEl);

        // 0x180113620: a DO-WHILE — a null target faults here.
        void* walk = target;
        do {
            F<uint32_t>(walk, EL::kFlags) |= kElementNeedsSizeCalc;
            walk = F<void*>(walk, EL::kParent);
        } while (walk);

        tmpDlg->UI::Dialog::~Dialog();
    }

    // ---- phase 3: id 0 means "element only, no controller" ---------------
    if (typeId != 0)
        AddController(typeId, dataSource, config,
                      Object::InternalPtr<Element>(static_cast<Element*>(createdElement)));
}

}  // namespace UI

// ============================================================================
// Registration.
//
// Every mangled name below is repeated AS ITS OWN STRING LITERAL rather than
// through a named constant: tools/runtime_census.py cannot see through a named
// constant and would under-count the lane
// (reference_census_cannot_see_nonliteral_registrations.md).
// ============================================================================
int RtUiDialogCreateRegister() {
    int n = 0;

    RegisterRuntimeExport(
        "?AddController@DialogInstance@UI@@QEAAXIPEAVBase@Object@@"
        "V?$ExternalPtr@VDialogControllerConfig@UI@@@4@"
        "V?$ExternalPtr@VElementDescription@UI@@@4@AEBVString@System@@@Z",
        (void*)&RtUiDialogInstance_AddController6);
    RegisterRuntimeAddr(0x180113470, (void*)&RtUiDialogInstance_AddController6);      ++n;

    RegisterRuntimeExport(
        "?AddController@DialogInstance@UI@@QEAAXIPEAVBase@Object@@"
        "V?$ExternalPtr@VDialogControllerConfig@UI@@@4@"
        "V?$InternalPtr@VElement@UI@@@4@@Z",
        (void*)&RtUiDialogInstance_AddControllerTab);
    RegisterRuntimeAddr(0x1801133b0, (void*)&RtUiDialogInstance_AddControllerTab);    ++n;

    RegisterRuntimeExport("?StartTimer@DialogInstance@UI@@QEAAXHVTime@System@@@Z",
                          (void*)&RtUiDialogInstance_StartTimer);
    RegisterRuntimeAddr(0x1801137c0, (void*)&RtUiDialogInstance_StartTimer);          ++n;

    return n;
}
