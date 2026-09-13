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

// ---- ResolveExport / ResolveAddr -------------------------------------------
// Forward-declared rather than pulled in from standalone/resolve.h, for the
// reason rt_port.h gives: resolve.h drags in <windows.h>, whose <wingdi.h>
// declares a function called `Rectangle` and whose <windef.h> #defines
// `near`/`far` away. These declarations must stay byte-identical to
// resolve.h's; the linker checks the two against each other.
void* ResolveExport(const char* mangled_name);
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);
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
// BY ADDRESS, NOT BY NAME, and deliberately: see the banner. Identical to
// runtime/src/collection/array.cpp's pair, including the retry-a-miss contract
// (at cutover this address is supplied by the allocator's own arming path, and
// a caller that ran before that point would otherwise freeze a null into the
// static for the life of the process).
void* MemAlloc(uint64_t bytes, int align) {
    using Fn = void* (*)(uint64_t, int);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7a50));
    return fn(bytes, align);
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
// Callees, all through the SEAM (ResolveAddr / ResolveExport), never the raw
// door — so that when another lane ports one of these, our body follows it,
// which is what retail's own direct near-CALL into the same DLL amounted to.
// ============================================================================

// Collection::Array<DialogTimer(0x18)>::SetSize(int newSize, const T* fill)
// 0x180076250. Registered as the SAME BODY as 0x18005fb00 (kSpec_Node18).
void ArraySetSize18(void* arrayHdr, int32_t newSize, const void* fill) {
    using Fn = void (*)(void*, int32_t, const void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x180076250));
    fn(arrayHdr, newSize, fill);
}

// Collection::Array<T*>::SetSize(int newSize, const T* const* fill)
// 0x180076370. Registered as the SAME BODY as 0x180076900 (kSpec_Fill8Z).
void ArraySetSize8_76370(void* arrayHdr, int32_t newSize, const void* fill) {
    using Fn = void (*)(void*, int32_t, const void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x180076370));
    fn(arrayHdr, newSize, fill);
}

// The SAME shape at 0x180076900 — already registered by array.cpp under
// kSpec_Fill8Z. The 6-arg AddController calls THIS address, not 0x180076370.
void ArraySetSize8_76900(void* arrayHdr, int32_t newSize, const void* fill) {
    using Fn = void (*)(void*, int32_t, const void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x180076900));
    fn(arrayHdr, newSize, fill);
}

// ??0Internal@Object@@QEAA@XZ   0x1800bfd10   (an EXPORT — checked)
void* InternalCtor(void* self) {
    using Fn = void* (*)(void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveExport("??0Internal@Object@@QEAA@XZ"));
    return fn(self);
}

// ?GetSpriteIndexFromName@SpriteSheet@UI@@QEBAHAEBVString@System@@@Z  0x1800031f0
int32_t GetSpriteIndexFromName(const void* sheet, const void* nameString) {
    using Fn = int32_t (*)(const void*, const void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveExport(
        "?GetSpriteIndexFromName@SpriteSheet@UI@@QEBAHAEBVString@System@@@Z"));
    return fn(sheet, nameString);
}

// ?IsDerivedFrom@Base@Object@@QEBA_NI@Z  0x1800c0390  (PORTED, Wave 3 lane A —
// which is exactly why this goes through the seam and not the raw address.)
bool IsDerivedFrom(const void* obj, uint32_t typeId) {
    using Fn = bool (*)(const void*, uint32_t);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveExport("?IsDerivedFrom@Base@Object@@QEBA_NI@Z"));
    return fn(obj, typeId);
}


// ?FindElement@Element@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@AEBVString@System@@@Z
// 0x180115650. HIDDEN RETURN SLOT in RDX, returns that same pointer in RAX
// (reference_hidden_retslot_wild_write.md — an fn_* typedef that omits the slot
// is a wild write that still returns the right answer).
void** ElementFindElement(void* self, void** out, const void* nameString) {
    using Fn = void** (*)(void*, void**, const void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveExport(
        "?FindElement@Element@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@"
        "AEBVString@System@@@Z"));
    return fn(self, out, nameString);
}

// ??0Dialog@UI@@QEAA@XZ  0x180112db0 / ??1Dialog@UI@@UEAA@XZ  0x180112e40.
// The 6-arg AddController builds a default UI::Dialog on its stack with both
// bodies INLINED; the inlined sequences were diffed against these two exports
// instruction for instruction and are the same code (lane doc §3). Calling the
// exports IS retail's body minus the inlining — array.cpp's MemAlloc argument.
void* DialogCtor(void* self) {
    using Fn = void* (*)(void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveExport("??0Dialog@UI@@QEAA@XZ"));
    return fn(self);
}
void DialogDtor(void* self) {
    using Fn = void (*)(void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveExport("??1Dialog@UI@@UEAA@XZ"));
    fn(self);
}

// The two `?_typeInfo@…@0VBaseTypeInfo@Object@@A` statics the bodies read a
// type id out of. Retail loads the FIRST DWORD of the static
// (`MOV EDX,dword ptr [0x1801f9e48]`), which BaseTypeInfo.h calls `typeId`.
//
// *** AEAA/0V… MEANS PRIVATE, NOT UNEXPORTED. *** This DLL exports its private
// members and both of these ARE in the export table
// (reference_aeaa_is_private_not_unexported.md), so they are resolved BY NAME
// and there is no address door here to justify.
//
// WAVE 10C lane TIDENT -- the ATTENDANCE line, and why a promotion needs it.
//
// `ResolveExport` consults the PORT REGISTRY FIRST
// (reference_resolveaddr_consults_port_registry_first.md), so once a
// `?_typeInfo@` row is promoted this helper reads OUR minted node instead of
// the DLL's. The value it takes out is the typeId, and our node's typeId is the
// same CRC the DLL's holds -- so a promotion is, correctly, invisible in
// behaviour. That is exactly what makes an A/B on it UNINFORMATIVE: a run in
// which nothing changes cannot tell a safe promotion from one that was never
// exercised (this file's own "WHY NOT ALL 104" rule, in reg_mint_promoted.cpp).
//
// This line resolves that, the way lane ELEM's `[ElemTid]` line did: it prints
// WHICH NODE was read, not just what came out. Across an A/B the pointer must
// move out of the DLL's data into our image while the id stays byte-identical
// -- and if the line never prints at all, the row has no live consumer on that
// route and must not be promoted.
uint32_t TypeIdOfStatic(const char* mangled, uint32_t* cache) {
    if (*cache) return *cache;
    if (const uint32_t* p = reinterpret_cast<const uint32_t*>(ResolveExport(mangled))) {
        *cache = *p;
        // The RAW pointer, deliberately unclassified. A "(ours)/(theirs)"
        // verdict here would need an image-range test this TU does not have,
        // and inventing one is how a lane ships a label that means nothing.
        // The A/B does the classifying, exactly as lane ELEM's [ElemTid] line
        // did: our image and the DLL sit in visibly different ranges.
        printf("[TidStatic] %s  node=%p id=0x%08x\n",
               mangled, (const void*)p, *cache);
    }
    return *cache;
}
uint32_t g_tidDialogController = 0;   // ?_typeInfo@DialogController@UI@@  0x1801f9e48
uint32_t g_tidGroupElement     = 0;   // ?_typeInfo@GroupElement@UI@@      0x1801fabd8

inline uint32_t TypeIdDialogController() {
    return TypeIdOfStatic("?_typeInfo@DialogController@UI@@0VBaseTypeInfo@Object@@A",
                          &g_tidDialogController);
}
inline uint32_t TypeIdGroupElement() {
    return TypeIdOfStatic("?_typeInfo@GroupElement@UI@@0VBaseTypeInfo@Object@@A",
                          &g_tidGroupElement);
}

// ?_singleton@Factory@Object@@0PEAV12@EA  — an exported DATA symbol (the cell,
// not the Factory). PLAN.md §2.3 calls 0x1801fc6e8 a global that "has no
// export"; both halves of that are wrong, and type_system.cpp already says so.
RtObject::Factory* FactorySingleton() {
    static RtObject::Factory** cell = nullptr;
    if (!cell) cell = reinterpret_cast<RtObject::Factory**>(
        ResolveExport("?_singleton@Factory@Object@@0PEAV12@EA"));
    return cell ? *cell : nullptr;
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
    DialogInstance* const d = static_cast<DialogInstance*>(self);

    // 0x1801137d2: `CMP dword ptr [RCX+0x70],R10D` / `JLE` — a SIGNED test on
    // _timers._size, so a negative size skips the loop rather than wrapping.
    for (int32_t i = 0; i < d->_timers._size; ++i) {
        uint8_t* const rec = reinterpret_cast<uint8_t*>(d->_timers._data) +
                             static_cast<size_t>(i) * sizeof(DialogTimer);
        // The data pointer is RE-READ from the object on every iteration
        // (0x1801137e0 and 0x1801137ef both `MOV …,[R11+0x68]`). Kept.
        if (F<int32_t>(rec, offsetof(DialogTimer, _id)) == id) {
            F<int64_t>(rec, offsetof(DialogTimer, _remaining)) = time;
            uint8_t* const rec2 = reinterpret_cast<uint8_t*>(d->_timers._data) +
                                  static_cast<size_t>(i) * sizeof(DialogTimer);
            F<int64_t>(rec2, offsetof(DialogTimer, _period)) = time;
            // NO `break`, NO `return` — see the header block.
        }
    }

    // The fill record is built on the stack with THREE stores; +0x14..0x17 is
    // never written and the 0x18-byte byte-copy in the grow helper carries
    // whatever the stack held. `tmp` is deliberately left uninitialised there
    // for the same reason.
    DialogTimer tmp;
    tmp._remaining = time;
    tmp._period    = time;
    tmp._id        = id;
    ArraySetSize18(&d->_timers, d->_timers._size + 1, &tmp);
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
    // ---- Object::Factory's AVL find, transcribed from 0x1801133c1 ----------
    // Descend LEFT on `id < key` (`CMP EDX,R9D / JC`), stop on equal
    // (`CMP R9D,EDX / JNC`), otherwise RIGHT. Identical to the walk
    // runtime/src/object/type_system.cpp already ports as `FindNode`; spelled
    // out here because retail inlined it and the port reproduces the inlining.
    RtObject::Factory* const factory = FactorySingleton();
    RtObject::TypeNode* node = factory ? factory->root : nullptr;
    RtObject::TypeNode* found = nullptr;
    while (node) {
        const uint32_t key = node->key;
        if (typeId < key)      node = node->left;
        else if (key >= typeId) { found = node; break; }
        else                    node = node->right;
    }

    // 0x1801133f7 `MOV RCX,[RAX+0x10]` with RAX possibly zero. TypeNode+0x10 is
    // `value.info`, the BaseTypeInfo*. FAITHFUL: no guard (CLAUDE.md).
    RtObject::BaseTypeInfo* const info =
        *reinterpret_cast<RtObject::BaseTypeInfo* const*>(B(found, 0x10));

    // 0x1801133fb `CALL qword ptr [RCX+0x20]` — the call goes through
    // info+0x20 (`factoryFn`), and RCX is left holding `info`. The callee is
    // `?CreateNewObject@…@SAPEAVBase@Object@@XZ`, a STATIC that ignores it; the
    // argument is passed anyway so the register state matches retail exactly.
    void* const created =
        reinterpret_cast<void* (*)(void*)>(info->factoryFn)(info);

    // 0x1801133fe: the type id comes from the FIRST DWORD of
    // ?_typeInfo@DialogController@UI@@, not from a literal.
    const bool isController = IsDerivedFrom(created, TypeIdDialogController());

    // 0x18011341e `CMOVNZ RDI,RBX` with RDI still zero — so a NON-DialogController
    // leaves the target null and the four stores below write through null.
    DialogController* const c =
        static_cast<DialogController*>(isController ? created : nullptr);

    // Retail loads the container BEFORE the CMOV (0x180113416/0x18011341b), so
    // the deref of `containerPtr` happens on both arms. Kept in that order.
    void* const container = *containerPtr;

    c->_config     = config;       // +0x10  0x180113422
    c->_dataSource = dataSource;   // +0x28  0x180113426
    c->_dialog     = self;         // +0x18  0x18011342a
    c->_container  = container;    // +0x20  0x18011342e

    // 0x18011343d `CALL qword ptr [RAX+0x20]` where RAX = *(void**)c — a REAL
    // vtable dispatch this time (slot 4), with `this` and nothing else.
    void* const* const vt = *reinterpret_cast<void* const* const*>(c);
    reinterpret_cast<void (*)(void*)>(vt[4])(c);

    // 0x180113440: append to _controllers through the stride-8 grow helper.
    // The fill is the ADDRESS OF A STACK SLOT holding the pointer, not the
    // pointer — `LEA R8,[RSP+0x40]` where [RSP+0x40] was stored at 0x180113438.
    DialogInstance* const d = static_cast<DialogInstance*>(self);
    void* slot = c;
    ArraySetSize8_76370(&d->_controllers, d->_controllers._size + 1, &slot);
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
    void* createdElement = nullptr;   // RDI

    if (desc) {
        DialogInstance* const d = static_cast<DialogInstance*>(self);
        void* insertion = d->_root;   // RDI starts at this->_root (0x1801134b4)

        // ---- phase 1a: does the ROOT's own name match? ----------------------
        // `name` is `const String&`, so `nameString` IS the String object and
        // its single member is the refcounted `char*`. Length lives at ptr-8.
        const char* const want = *reinterpret_cast<const char* const*>(nameString);
        bool useRoot = false;
        if (want && *reinterpret_cast<const int32_t*>(want - 8) != 0) {
            // 0x1801134cb `MOV RCX,[RDI+0x10]` — UNGUARDED on `insertion`.
            // `insertion` is `this->_root`, which the ctor already dereferences
            // unguarded at +0x90, so retail's assumption holds by construction.
            // No guard added: it would not restore the behaviour, it would skip
            // it (CLAUDE.md).
            const char* const have =
                *reinterpret_cast<const char* const*>(B(insertion, EL::kName));
            const int32_t haveLen =
                have ? *reinterpret_cast<const int32_t*>(have - 8) : 0;
            if (haveLen == *reinterpret_cast<const int32_t*>(want - 8)) {
                // Retail re-tests `have` and its length here (0x1801134e2 and
                // 0x1801134e7) before calling StrCompare. Both re-tests jump to
                // the SAME "use it" label, so an empty-but-equal-length name
                // matches without a compare. Transcribed as written.
                if (!have) useRoot = true;
                else if (*reinterpret_cast<const int32_t*>(have - 8) == 0) useRoot = true;
                else if (System::String::StrCompare(have, want) == 0) useRoot = true;
            }
        }

        // ---- phase 1b: otherwise scan the root's DIRECT children ------------
        if (!useRoot) {
            // 0x1801134f5 `MOV R14D,[RDI+0xa8]` — unguarded, same argument.
            const int32_t n =
                *reinterpret_cast<const int32_t*>(B(insertion, EL::kChildrenSize));
            void* hit = nullptr;
            if (n > 0) {
                void** kids = *reinterpret_cast<void***>(B(insertion, EL::kChildrenData));
                for (int32_t i = 0; i < n; ++i) {
                    void* const child = kids[i];
                    if (child) {
                        void* out = nullptr;
                        ElementFindElement(child, &out, nameString);
                        if (out) { hit = out; break; }
                    }
                }
            }
            // 0x180113538: a miss zeroes RDI through a 32-bit `MOV EDI,ESI`,
            // which zero-extends and clears the whole register. Ghidra prints
            // that as a plain assignment; the disassembly is what says it kills
            // the upper half (reference_ghidra_c_output_vs_disassembly.md).
            insertion = hit;
        }

        // ---- the GroupElement gate -----------------------------------------
        // `CMOVNZ RBX,RDI` with RBX pre-loaded from the zero register: a
        // non-GroupElement (or a miss) leaves the insertion point NULL and the
        // stores below run through null, exactly as retail does.
        void* const target =
            IsDerivedFrom(insertion, TypeIdGroupElement()) ? insertion : nullptr;

        // ---- phase 2: build the element -------------------------------------
        // A default-constructed UI::Dialog on OUR stack, byte-for-byte what
        // retail inlines. `alignas(16)` because the inlined ctor zeroes +0x20
        // with a MOVDQA, i.e. retail's temporary is 16-byte aligned.
        alignas(16) uint8_t tmpDialog[0x90];
        DialogCtor(tmpDialog);

        void* newEl = nullptr;
        {
            // 0x1801135db: RCX is the ExternalPtr's TARGET, RDX the hidden
            // return slot, R8 the Dialog*. Slot +0x40 == index 8.
            void* const* const vt = *reinterpret_cast<void* const* const*>(desc);
            void** const ret = reinterpret_cast<void** (*)(void*, void**, void*)>(vt[8])(
                desc, &newEl, tmpDialog);
            newEl = *ret;   // 0x1801135ef `MOV RDI,qword ptr [RAX]`
        }
        createdElement = newEl;

        if (newEl) F<void*>(newEl, EL::kParent) = target;   // 0x1801135fc

        // 0x180113603: unguarded on `target`. The fill is the ADDRESS of the
        // slot holding the pointer (0x1801135f2 stored it there first).
        ArraySetSize8_76900(B(target, EL::kChildrenData),
                            *reinterpret_cast<int32_t*>(B(target, EL::kChildrenSize)) + 1,
                            &newEl);

        // 0x180113620: a DO-WHILE — the flag is set on `target` itself before
        // the null test, so a null target faults here rather than skipping.
        void* walk = target;
        do {
            F<uint32_t>(walk, EL::kFlags) |= kElementNeedsSizeCalc;
            walk = F<void*>(walk, EL::kParent);
        } while (walk);

        DialogDtor(tmpDialog);
    }

    // ---- phase 3 ------------------------------------------------------------
    // 0x1801136e5: `TEST R12D,R12D` — id 0 means "element only, no controller".
    // The InternalPtr is handed on BY ADDRESS, which is what the 4-arg overload
    // dereferences.
    if (typeId != 0) {
        void* slot = createdElement;
        using Fn = void (*)(void*, uint32_t, void*, void*, void**);
        static Fn fn = nullptr;
        if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1801133b0));
        fn(self, typeId, dataSource, config, &slot);
    }
}

// ============================================================================
// ??0DialogInstance@UI@@QEAA@ PEAVUserInterface@1@ PEBVDialog@1@
//     V?$InternalPtr@VDialogInstance@UI@@@Object@@ PEAVBase@5@ PEAX H @Z
//                                                              0x180114870
//
//     DialogInstance::DialogInstance(UserInterface* ui, const Dialog* dialog,
//                                    InternalPtr<DialogInstance> parent,
//                                    Object::Base* dataSource,
//                                    void* userData, int userFlags)
//
// `parent` is an InternalPtr passed BY HIDDEN REFERENCE (R9 is dereferenced at
// 0x1801148c9), which is how engine_funcs.h's `fn_DialogInstance_Ctor_t` already
// spells it and how all sixteen exe call sites pass it.
//
// Returns `this` in RAX (0x180114a37). MSVC always does for a ctor; the port
// must produce it because the exe's call sites are typed to receive it.
//
// Every field offset below is asserted in runtime/types/ui/DialogInstance.h and
// the four holes the ctor never writes (+0x4c, +0x64, +0x99..0x9f, +0xb4) are
// named there. This body writes exactly the fields it is shown writing; it does
// not zero the holes.
// ============================================================================
void* RtUiDialogInstance_Ctor(void* self, void* ui, const void* dialog,
                              void** parentPtr, void* dataSource,
                              void* userData, int32_t userFlags) {
    DialogInstance* const d = static_cast<DialogInstance*>(self);
    const Dialog* const dlg = static_cast<const Dialog*>(dialog);

    InternalCtor(self);                                       // 0x1801148a2

    // 0x1801148a8: the DLL's OWN exported vtable, never a minted one. This is
    // the `??_7Geometry@Graphics@@6B@` lever from Wave 6 — resolving retail's
    // table keeps a half-ported class from putting two vtables in play.
    static void* vtbl = nullptr;
    if (!vtbl) vtbl = ResolveExport("??_7DialogInstance@UI@@6B@");
    F<void*>(d, 0x00) = vtbl;

    d->_ui         = ui;                          // +0x10
    d->_dialog     = const_cast<void*>(dialog);   // +0x18
    d->_flags      = dlg->_defaultFlags;          // +0x20  from _dialog->+0x40
    d->_flags2     = 0;                           // +0x24
    d->_parent     = nullptr;                     // +0x28  written 0 then...
    d->_parent     = *parentPtr;                  // +0x28  ...the real value
    d->_dataSource = dataSource;                  // +0x30
    d->_children._head  = nullptr;                // +0x38
    d->_children._tail  = nullptr;                // +0x40
    d->_children._count = 0;                      // +0x48   (+0x4c left alone)

    // 0x1801148e7: `_dialog->_rootDescription`'s vtable slot 8, with the hidden
    // return slot pointed AT THE OBJECT'S OWN `_root` FIELD and the Dialog as
    // the second argument — the same slot the 6-arg AddController calls.
    {
        void* const rootDesc = dlg->_rootDescription;
        void* const* const vt = *reinterpret_cast<void* const* const*>(rootDesc);
        reinterpret_cast<void** (*)(void*, void**, const void*)>(vt[8])(
            rootDesc, &d->_root, dialog);
    }

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
    if (DialogInstance* const parent = static_cast<DialogInstance*>(d->_parent)) {
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
        using Fn = void (*)(void*, uint32_t, void*, void* /*arg4: config*/,
                            void* /*arg5: description*/, const void*);
        static Fn fn = nullptr;
        if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x180113470));
        fn(self, e._typeId, d->_dataSource,
           e._config,        // entry+0x10 -> R9  -> ExternalPtr<DialogControllerConfig>
           e._elementDesc,   // entry+0x08 -> a5  -> ExternalPtr<ElementDescription>
           &e._name);        // entry+0x18 -> a6  -> const String&
    }

    return self;   // 0x180114a37 `MOV RAX,RSI`
}

}  // namespace

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
        "??0DialogInstance@UI@@QEAA@PEAVUserInterface@1@PEBVDialog@1@"
        "V?$InternalPtr@VDialogInstance@UI@@@Object@@PEAVBase@5@PEAXH@Z",
        (void*)&RtUiDialogInstance_Ctor);
    RegisterRuntimeAddr(0x180114870, (void*)&RtUiDialogInstance_Ctor);                ++n;

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
