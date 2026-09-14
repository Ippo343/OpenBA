// ============================================================================
// runtime/src/ui/combodialog_ctor.cpp — UI::ComboDialog construction
//
// SPLIT AT MERGE, 2026-09-12. This file and runtime/src/ui/combodialog.cpp were
// written in parallel by two batches — vtable batch 9 took the class's SLOTS,
// the UI-text batch took its CONSTRUCTION — and both landed on the same path
// with the same `RtUiComboDialogRegister` name. Neither body set overlaps the
// other, so the merge kept both and gave this half its own file and its own
// `RtUiComboDialogCtorRegister`. Read the other file's banner for the vtable.
//
//   ??2ComboDialog@UI@@SAPEAX_K@Z                        retail 0x180074770
//   ??0DialogInstance@UI@@QEAA@PEAVUserInterface@1@
//       V?$InternalPtr@VElement@UI@@@Object@@
//       V?$InternalPtr@VDialogInstance@UI@@@4@
//       PEAVBase@4@PEAXH@Z                               retail 0x1801146b0
//   ??0ComboDialog@UI@@QEAA@PEAVUserInterface@1@
//       V?$InternalPtr@VElement@UI@@@Object@@
//       V?$InternalPtr@VDialogInstance@UI@@@4@
//       V?$InternalPtr@VComboElement@UI@@@4@@Z           retail 0x180111d00
// ============================================================================
//
// ---------------------------------------------------------------------------
// ABSENCE ESTABLISHED BEFORE A LINE WAS WRITTEN
// ---------------------------------------------------------------------------
// A body that is ported but never REGISTERED is indistinguishable from an
// absent one to the census, so the grep was by ADDRESS *and* by NAME, scoped to
// the source roots (never the repo root — `.claude/worktrees/` holds sibling
// lane checkouts and an unscoped grep returns a confident wrong number):
//
//   grep -rn '180111d00\|1801146b0\|180074770'
//       runtime/ code/ standalone/ engine/ dx11/
//     -> ONE hit, and it is a COMMENT, not a port:
//        runtime/src/ui/ui_operator_delete.cpp:47 names 0x180074770 only to say
//        that its sibling `operator delete` (0x1800747f0) is the one in THAT
//        batch. The `operator new` itself was left on the DLL.
//   grep -rn 'ComboDialog' <same roots>
//     -> factory table rows (factory_ui1_tables.inc) and vtable-mint rows
//        (rt_data_mint_tables.inc). No body.
//
// ---------------------------------------------------------------------------
// THE SECOND DialogInstance CONSTRUCTOR, AND WHY IT IS A SEPARATE BODY
// ---------------------------------------------------------------------------
// runtime/src/ui/dialog_create.cpp already ports a DialogInstance ctor — but a
// DIFFERENT OVERLOAD, 0x180114870, whose second parameter is a `const Dialog*`
// resource and which therefore builds its own root Element from the Dialog's
// root ElementDescription, copies the Dialog's sprite sheet, seeds `_flags`
// from the Dialog and runs the Dialog's controller table.
//
// THIS overload (0x1801146b0) takes an ALREADY-BUILT root Element instead of a
// Dialog, and consequently:
//   * `_dialog` (+0x18) is left NULL,
//   * `_flags` / `_flags2` (+0x20 / +0x24) are zeroed rather than seeded,
//   * `_spriteSheet` (+0x58) is NULL and `_spriteIndex` (+0x60) is -1 with no
//     SpriteSheet lookup at all,
//   * there is no controller loop.
// Everything else — the Object::Internal base ctor, the SAME exported vtable
// `??_7DialogInstance@UI@@6B@`, the root's back-pointer at Element+0x90, and
// the child-list PREPEND into the parent — is identical instruction for
// instruction. The two bodies are siblings, not duplicates.
//
// ---------------------------------------------------------------------------
// ARGUMENT MAPPING — WORKED OUT FROM THE STACK, NOT FROM THE MANGLING
// ---------------------------------------------------------------------------
// Getting this wrong corrupts every combo box in the game, so both frames were
// laid out by hand.
//
// 0x1801146b0 pushes RSI/RDI/R14 (0x18) and SUBs 0x30, so the incoming home
// space sits at RSP+0x50 .. RSP+0x80. The body reads:
//     RCX  -> this            (R14)
//     RDX  -> UserInterface*  (RBX)   stored RAW at +0x10
//     R8   -> InternalPtr<Element>,        DEREFERENCED at 0x18011471e
//     R9   -> InternalPtr<DialogInstance>, DEREFERENCED at 0x1801146fe
//     [RSP+0x70] -> Object::Base* dataSource  -> +0x30
//     [RSP+0x78] -> void*        userData     -> +0xa8
//     [RSP+0x80] -> int          userFlags    -> +0xb0
// So BOTH InternalPtr parameters arrive as a POINTER TO A CALLER TEMPORARY and
// the ctor takes NO reference on either — it copies the raw pointer out and
// nothing else. That matches how dialog_create.cpp spells the other overload's
// `parent`, and how all the exe call sites pass one.
//
// 0x180111d00 pushes seven registers (0x38) and SUBs 0x50, and spills R8 and
// RCX into their home slots FIRST (`MOV [R11+0x18],R8` / `MOV [R11+8],RCX`
// with R11 = entry RSP), which is what lets it reuse the Element InternalPtr
// pointer after the two searches clobber the scratch slots. Its arg5, the
// InternalPtr<ComboElement>, is read at [RSP+0xb0] == [R11+0x28] — the fifth
// home slot — and DEREFERENCED, so it too is by hidden reference.
//
// *** THE PARENT IS NOT PASSED DOWN. *** At 0x180111d31 the ctor zeroes the
// arg4 home slot and at 0x180111d48 passes the address of THAT ZERO as the base
// ctor's `parent`. A ComboDialog is therefore deliberately NOT linked into its
// parent dialog's child list; the parent pointer is kept at ComboDialog+0xc0
// instead. Reading the mangled name alone gets this backwards.
//
// NO HIDDEN RETURN SLOT on either constructor — checked against the
// disassembly, not the mangling. Both return `this` in RAX
// (0x180114803 `MOV RAX,R14`; 0x180112106 `MOV RAX,R14`), which is what MSVC
// always does for a ctor and what the callers are typed to receive.
//
// ---------------------------------------------------------------------------
// FOUR INLINED CALLEES, EACH IDENTIFIED AGAINST ITS OWN EXPORTED BODY
// ---------------------------------------------------------------------------
// The ComboDialog ctor is large because MSVC inlined four member functions into
// it. Each inlined sequence below was diffed instruction for instruction
// against the exported body it came from, and the exported body's address is
// quoted at the site:
//
//   ?SetElementId@Element@UI@@QEAAXH@Z         0x180115820  (one store)
//   ?SetFlagsOnOff@Element@UI@@QEAAXII@Z       0x180116160
//   ?GetContentSize@ClipRegionElement@UI@@QEAAXAEAH0@Z   0x1800381e0
//   ?GetPaddedSize@Element@UI@@                0x180115f10
//   ?SetSize@ContainerElement@UI@@QEAAXHH@Z    0x180039eb0
//
// They are reproduced INLINE here rather than routed through the seam, because
// inlining is what the shipped bytes do: calling the exports would be a
// different instruction stream reaching the same result, and one of them
// (`GetPaddedSize`) differs from its export in exactly one register-vs-memory
// reload of a value nothing writes in between.
//
// `?SetFlagsOnOff@`'s dirty-walk is DEAD at this call site and is kept anyway.
// The export tests `(new ^ old) & 1` — the HIDDEN bit — to decide whether to
// mark ancestors dirty. Here the only bit touched is bit 16, so the XOR can
// never have bit 0 set and MSVC still emitted the compare (0x180111e37). It is
// written out faithfully below; the C++ compiler folds it, and the folding
// changes nothing because the branch is provably never taken.
//
// ---------------------------------------------------------------------------
// UNGUARDED DEREFERENCES THAT ARE KEPT UNGUARDED
// ---------------------------------------------------------------------------
// Three places deref a pointer retail has just allowed to be null:
//   0x180114782  root->[0x90] = this        (root came from *rootPtr)
//   0x180111e1e  _list->[0x88] = 0          (_list is `IsDerivedFrom ? x : 0`)
//   0x1801120e1  container->[0xc0] = w      (same CMOV-null shape)
// Retail relies on the value always being there; a guard would not restore the
// behaviour, it would skip it. They are reproduced as they ship.
//
// PORTABILITY: no <windows.h>, per PLAN.md §4.1 decision 1.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>

#include "../../funcs/rt_port.h"
#include "../../types/ui/DialogInstance.h"

// ResolveAddr / ResolveExport live in standalone/resolve.h, which drags in
// <windows.h> (whose <wingdi.h> declares a function called `Rectangle` and
// whose <windef.h> #defines `near`/`far` away), so they are declared here
// exactly as ~30 sibling runtime files declare them. The linker checks these
// against resolve.h's.
void* ResolveExport(const char* mangled_name);
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

using RtUI::ComboDialog;
using RtUI::DialogInstance;

namespace {

// ---------------------------------------------------------------------------
// Foreign offsets. Every one carries the instruction that pins it.
//
// `runtime/types/ui/Element.h` owns the UI::Element numbers and asserts all of
// them; they are re-spelled as named constants rather than included because
// Element.h pulls in the UI::String value type and this file has no other use
// for it. That is the convention runtime/types/ui/UserInterface.h states for
// foreign offsets and the one runtime/src/ui/dialog_create.cpp follows.
// ---------------------------------------------------------------------------
namespace EL {
enum : size_t {
    kLeftPad      = 0x18,   // ?GetLeftPad@    0x1800355e0
    kRightPad     = 0x1c,   // ?GetRightPad@   0x180035600
    kTopPad       = 0x20,   // ?GetTopPad@     0x1800355f0
    kBottomPad    = 0x24,   // ?GetBottomPad@  0x180035610
    kMinWidth     = 0x28,   // ?SetMinSize@    0x1801154a0 (arg 1)
    kMinHeight    = 0x2c,   // ?SetMinSize@    0x1801154a0 (arg 2)
    kFlags        = 0x34,   // ?GetElementFlags@ 0x1800354d0
    kLayoutWidth  = 0x60,   // ?GetLayoutWidth@  0x1800355c0
    kLayoutHeight = 0x64,   // ?GetLayoutHeight@ 0x1800355d0
    kElementId    = 0x88,   // ?SetElementId@  0x180115820
    kDialogParent = 0x90,   // InternalPtr<DialogInstance>
    kParent       = 0x98,   // ?SetParent@     0x1801154d0
    kChildrenData = 0xa0,   // ?Child@         0x180035540
    kChildrenSize = 0xa8,   // ?ChildCount@    0x180035530
};
// Vtable slots. Slot 2 (+0x10) is Object::Internal's Type slot — the same
// offset runtime/types/object/Internal.h calls kInternalVtblType. Slot 4
// (+0x20) is ComputeSize(), which is _purecall in the Element base and
// overridden by every class this file calls it on.
enum : int {
    kVtblType        = 0x10,
    kVtblComputeSize = 0x20,
};
}  // namespace EL

// UI::Element::_flags bits, named by runtime/types/ui/Element.h.
constexpr uint32_t kElementFlagSizeToScreenH = 0x00000010u;  // bit 4
constexpr uint32_t kElementFlagSizeToScreenW = 0x00000020u;  // bit 5
constexpr uint32_t kElementFlagSizeToScreen  = 0x00000030u;  // the mask
constexpr uint32_t kElementFlagHidden        = 0x00000001u;  // bit 0
constexpr uint32_t kElementFlagToolTip16     = 0x00010000u;  // bit 16
constexpr uint32_t kElementFlagNeedsSizeCalc = 0x08000000u;  // bit 27

// UI::ContainerElement, pinned by its own one-line exported accessors:
//   ?GetWidth@ContainerElement@UI@@QEBAHXZ  0x180039ee0 = `MOV EAX,[RCX+0xc0]`
//   ?GetHeight@ContainerElement@UI@@QEBAHXZ 0x180039ef0
constexpr size_t kContainerWidth  = 0xc0;
constexpr size_t kContainerHeight = 0xc4;

// UI::ClipRegionElement, pinned by ?GetContentSize@ (0x1800381e0), which reads
// exactly these two after calling ComputeSize.
constexpr size_t kClipContentWidth  = 0xc8;
constexpr size_t kClipContentHeight = 0xcc;

// UI::DialogInstance::_ui (+0x10) and the UI scale float the screen-size flags
// multiply by. `[dialog+0x10][+0xdc]`, per Element.h's own note on +0x90.
constexpr size_t kDialogInstanceUi = 0x10;
constexpr size_t kUserInterfaceScale = 0xdc;

// Video::Device's screen width / height, the two dwords GetPaddedSize reads out
// of `?_singleton@Device@Video@@0PEAV12@EA` (0x1801fe7a8).
constexpr size_t kDeviceScreenWidth  = 0x04;
constexpr size_t kDeviceScreenHeight = 0x08;

// ---- Byte cursor -----------------------------------------------------------
inline uint8_t* B(void* p, size_t off) { return static_cast<uint8_t*>(p) + off; }
template <class T> inline T& F(void* p, size_t off) {
    return *reinterpret_cast<T*>(B(p, off));
}

// ---------------------------------------------------------------------------
// ATTENDANCE — entries, and NOT a verification of anything. A zero here means
// the row is registered and DEAD on the route that was run; a non-zero means
// only that the body was entered.
// ---------------------------------------------------------------------------
uint64_t g_newEntries   = 0;
uint64_t g_newLastBytes = 0;
uint64_t g_diCtorEntries = 0;
uint64_t g_cdCtorEntries = 0;

struct ComboDialogSummary {
    ~ComboDialogSummary() {
        printf("[RtPort] UI::ComboDialog ATTENDANCE (entries, not comparisons): "
               "operator_new=%llu lastBytes=0x%llx  DialogInstance::ctor(Element)=%llu  "
               "ComboDialog::ctor=%llu\n",
               (unsigned long long)g_newEntries,
               (unsigned long long)g_newLastBytes,
               (unsigned long long)g_diCtorEntries,
               (unsigned long long)g_cdCtorEntries);
        fflush(stdout);
    }
};
ComboDialogSummary g_summary;

// ===========================================================================
// Callees reached through the SEAM (ResolveAddr / ResolveExport), never the raw
// door — so that when another lane ports one of these, this body follows it,
// which is what retail's own direct near-CALL into the same DLL amounted to.
// ===========================================================================

// `?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z`, 0x1800c7a50. BY ADDRESS, the
// same pair runtime/src/collection/array.cpp and runtime/src/ui/dialog_create.cpp
// use, including the retry-a-miss contract: at cutover this address is supplied
// by the allocator's own arming path, and a caller that ran before that point
// would otherwise freeze a null into the static for the life of the process.
void* MemAlloc(uint64_t bytes, int align) {
    using Fn = void* (*)(uint64_t, int);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7a50));
    return fn ? fn(bytes, align) : nullptr;
}

// ??0Internal@Object@@QEAA@XZ   0x1800bfd10   (an EXPORT — checked)
void* InternalCtor(void* self) {
    using Fn = void* (*)(void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveExport("??0Internal@Object@@QEAA@XZ"));
    return fn(self);
}

// ?IsDerivedFrom@Base@Object@@QEBA_NI@Z  0x1800c0390 — PORTED (Wave 3 lane A),
// which is exactly why this goes through the seam and not the raw address.
bool IsDerivedFrom(const void* obj, uint32_t typeId) {
    using Fn = bool (*)(const void*, uint32_t);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveExport("?IsDerivedFrom@Base@Object@@QEBA_NI@Z"));
    return fn(obj, typeId);
}

// ?FindElementType@Element@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@I@Z
// 0x180115760. HIDDEN RETURN SLOT in RDX, returns that same pointer in RAX
// (reference_hidden_retslot_wild_write.md — an fn_* typedef that omits the slot
// is a wild write that still returns the right answer). The type id is a DWORD
// in R8D.
void** FindElementType(void* self, void** out, uint32_t typeId) {
    using Fn = void** (*)(void*, void**, uint32_t);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveExport(
        "?FindElementType@Element@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@I@Z"));
    return fn(self, out, typeId);
}

// The three `?_typeInfo@…@0VBaseTypeInfo@Object@@A` statics the ComboDialog
// ctor loads a type id out of. Retail reads the FIRST DWORD of the static
// (`MOV R12D,dword ptr [0x1801fa120]`), which BaseTypeInfo.h calls `typeId`.
//
// *** AEAA / `0V…` MEANS PRIVATE, NOT UNEXPORTED. *** This DLL exports its
// private members and all three ARE in the export table
// (reference_aeaa_is_private_not_unexported.md), so they are resolved BY NAME
// and there is no address door here to justify.
uint32_t TypeIdOfStatic(const char* mangled, uint32_t* cache) {
    if (*cache) return *cache;
    if (const uint32_t* p = reinterpret_cast<const uint32_t*>(ResolveExport(mangled)))
        *cache = *p;
    return *cache;
}

uint32_t g_tidListElement       = 0;   // 0x1801fa120
uint32_t g_tidClipRegionElement = 0;   // 0x1801f9d68
uint32_t g_tidContainerElement  = 0;   // 0x1801f9dd8

inline uint32_t TypeIdListElement() {
    return TypeIdOfStatic("?_typeInfo@ListElement@UI@@0VBaseTypeInfo@Object@@A",
                          &g_tidListElement);
}
inline uint32_t TypeIdClipRegionElement() {
    return TypeIdOfStatic("?_typeInfo@ClipRegionElement@UI@@0VBaseTypeInfo@Object@@A",
                          &g_tidClipRegionElement);
}
inline uint32_t TypeIdContainerElement() {
    return TypeIdOfStatic("?_typeInfo@ContainerElement@UI@@0VBaseTypeInfo@Object@@A",
                          &g_tidContainerElement);
}

// `?_singleton@Device@Video@@0PEAV12@EA` (0x1801fe7a8) — an exported DATA
// symbol, the CELL rather than the Device. Read through the port-aware data
// door so a minted run and a DLL run both land on whatever storage the Device
// actually published into.
void* DeviceSingleton() {
    void** slot = RT_DATA_SLOT(void**, "?_singleton@Device@Video@@0PEAV12@EA");
    return slot ? *slot : nullptr;
}

// ---- virtual dispatch ------------------------------------------------------
inline uint32_t VCallType(void* el) {
    void* const* const vt = *reinterpret_cast<void* const* const*>(el);
    return reinterpret_cast<uint32_t (*)(void*)>(
        *reinterpret_cast<void* const*>(
            reinterpret_cast<const uint8_t*>(vt) + EL::kVtblType))(el);
}
inline void VCallComputeSize(void* el) {
    void* const* const vt = *reinterpret_cast<void* const* const*>(el);
    reinterpret_cast<void (*)(void*)>(
        *reinterpret_cast<void* const*>(
            reinterpret_cast<const uint8_t*>(vt) + EL::kVtblComputeSize))(el);
}

// ===========================================================================
// The four inlined member functions, each reproduced as the shipped stream
// inlines it.
// ===========================================================================

// Inlined at 0x180111d91-0x180111df8 and again at 0x180111e56-0x180111ec5:
// "is the root itself this type, and if not, does any DIRECT child contain one".
// The recursion lives in ?FindElementType@ (0x180115760); this wrapper is the
// one level retail open-coded. Both the children POINTER and the child COUNT
// are re-read every iteration, exactly as 0x180111dc0 / 0x180111df0 do.
void* FindSelfOrUnderChildren(void* root, uint32_t typeId) {
    if (VCallType(root) == typeId) return root;      // 0x180111da4 CMP / JZ

    int32_t i = 0;
    while (i < F<int32_t>(root, EL::kChildrenSize)) {
        void* const child = F<void**>(root, EL::kChildrenData)[i];
        if (child) {
            void* out = nullptr;
            FindElementType(child, &out, typeId);
            if (out) return out;                     // 0x180111de8 JNZ
        }
        ++i;
    }
    return nullptr;                                  // 0x180111df5 MOV RDI,RBX
}

// ?SetFlagsOnOff@Element@UI@@QEAAXII@Z  0x180116160, inlined at 0x180111e24
// with `off` folded to 0 and the OR turned into `BTS EDX,0x10`. The dirty walk
// is dead at this call site — see the banner — and is kept.
void SetFlagsOnOff(void* el, uint32_t on, uint32_t off) {
    const uint32_t old = F<uint32_t>(el, EL::kFlags);
    const uint32_t now = (old | on) & ~off;
    F<uint32_t>(el, EL::kFlags) = now;
    if ((now ^ old) & kElementFlagHidden) {
        for (void* p = el; p; p = F<void*>(p, EL::kParent))
            F<uint32_t>(p, EL::kFlags) |= kElementFlagNeedsSizeCalc;
    }
}

// ?SetSize@ContainerElement@UI@@QEAAXHH@Z  0x180039eb0, inlined at 0x1801120e1.
// The walk here is UNCONDITIONAL — it is ?SetNeedsSizeCalc@ (0x180035470), not
// the guarded walk above.
void ContainerSetSize(void* el, int32_t w, int32_t h) {
    F<int32_t>(el, kContainerWidth)  = w;
    F<int32_t>(el, kContainerHeight) = h;
    for (void* p = el; p; p = F<void*>(p, EL::kParent))
        F<uint32_t>(p, EL::kFlags) |= kElementFlagNeedsSizeCalc;
}

// ?GetContentSize@ClipRegionElement@UI@@QEAAXAEAH0@Z  0x1800381e0, inlined at
// 0x180111f3e: ComputeSize() first, then the two dwords.
void ClipGetContentSize(void* el, int32_t* outW, int32_t* outH) {
    VCallComputeSize(el);
    *outW = F<int32_t>(el, kClipContentWidth);
    *outH = F<int32_t>(el, kClipContentHeight);
}

// ?GetPaddedSize@Element@UI@@  0x180115f10, inlined at 0x180111f61-0x180112010.
// Layout size plus both pads, floored at the min size, and then — when either
// size-to-screen bit is set — replaced outright by the Video::Device screen
// dimension scaled by the ROOT dialog's UI scale. `flags` is captured ONCE,
// before the walk to the root, which is what both the export and the inline do.
void GetPaddedSize(void* el, int32_t* outW, int32_t* outH) {
    VCallComputeSize(el);

    int32_t w = F<int32_t>(el, EL::kLayoutWidth)
              + F<int32_t>(el, EL::kRightPad)
              + F<int32_t>(el, EL::kLeftPad);
    int32_t h = F<int32_t>(el, EL::kLayoutHeight)
              + F<int32_t>(el, EL::kBottomPad)
              + F<int32_t>(el, EL::kTopPad);
    if (w < F<int32_t>(el, EL::kMinWidth))  w = F<int32_t>(el, EL::kMinWidth);
    if (h < F<int32_t>(el, EL::kMinHeight)) h = F<int32_t>(el, EL::kMinHeight);

    const uint32_t flags = F<uint32_t>(el, EL::kFlags);
    if (flags & kElementFlagSizeToScreen) {
        void* root = el;
        while (F<void*>(root, EL::kParent)) root = F<void*>(root, EL::kParent);

        void* const dialog = F<void*>(root, EL::kDialogParent);
        void* const device = DeviceSingleton();

        if (flags & kElementFlagSizeToScreenW) {
            void* const ui = F<void*>(dialog, kDialogInstanceUi);
            w = static_cast<int32_t>(
                static_cast<float>(F<int32_t>(device, kDeviceScreenWidth))
                * F<float>(ui, kUserInterfaceScale));
        }
        if (flags & kElementFlagSizeToScreenH) {
            void* const ui = F<void*>(dialog, kDialogInstanceUi);
            h = static_cast<int32_t>(
                static_cast<float>(F<int32_t>(device, kDeviceScreenHeight))
                * F<float>(ui, kUserInterfaceScale));
        }
    }

    *outW = w;
    *outH = h;
}

}  // namespace

// ============================================================================
// ??2ComboDialog@UI@@SAPEAX_K@Z                            retail 0x180074770
//
//     void* UI::ComboDialog::operator new(size_t bytes)
//
// The whole shipped body is EnterCriticalSection(allocator+0x640) /
// Allocate(allocator, bytes, 8) / LeaveCriticalSection — which is
// `?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z` (0x1800c7a50) with the
// alignment frozen to 8. The two disassemblies were compared instruction for
// instruction; they differ only in which callee-saved register holds `bytes`
// and in MemAlloc taking the alignment from EDX instead of an immediate. This
// is the same equivalence runtime/src/ui/spritesheet_new.cpp states for
// `??2SpriteSheet@UI@@`.
//
// Its ONE call site asks for 0xd0 (`MOV ECX,0xd0` at 0x18011282b), which is
// what pins sizeof(UI::ComboDialog) in DialogInstance.h.
// ============================================================================
extern "C" void* RtUi_ComboDialog_OperatorNew(uint64_t bytes) {
    ++g_newEntries;
    g_newLastBytes = bytes;
    return MemAlloc(bytes, 8);
}

// ============================================================================
// ??0DialogInstance@UI@@QEAA@PEAVUserInterface@1@
//     V?$InternalPtr@VElement@UI@@@Object@@
//     V?$InternalPtr@VDialogInstance@UI@@@4@
//     PEAVBase@4@PEAXH@Z                                   retail 0x1801146b0
//
//     DialogInstance::DialogInstance(UserInterface* ui,
//                                    InternalPtr<Element> root,
//                                    InternalPtr<DialogInstance> parent,
//                                    Object::Base* dataSource,
//                                    void* userData,
//                                    int userFlags)
//
// The root-Element overload. See the banner for how it differs from the
// Dialog-resource overload at 0x180114870 that dialog_create.cpp ports, and for
// why both InternalPtr parameters arrive as pointers to caller temporaries.
//
// Every field offset below is asserted in runtime/types/ui/DialogInstance.h.
// The header names four holes the OTHER overload never writes; this one leaves
// the same four alone, and additionally writes +0x18 / +0x20 / +0x24 / +0x58 as
// zero and +0x60 as -1 rather than deriving them from a Dialog.
//
// Note the qword-width zero stores: retail clears +0x20 as ONE qword
// (0x1801146f6), covering `_flags` AND `_flags2`, and likewise +0x70, +0x80 and
// +0x88 each cover a pair of dwords. Spelled out field by field here; the bytes
// are identical.
// ============================================================================
extern "C" void* RtUi_DialogInstance_CtorFromElement(void* self, void* ui,
                                                     void** rootPtr,
                                                     void** parentPtr,
                                                     void* dataSource,
                                                     void* userData,
                                                     int32_t userFlags) {
    ++g_diCtorEntries;
    DialogInstance* const d = static_cast<DialogInstance*>(self);

    InternalCtor(self);                                       // 0x1801146dc

    // 0x1801146e2: the DLL's OWN exported vtable, never a minted one. Resolving
    // retail's table keeps a half-ported class from putting two vtables in play
    // — the `??_7Geometry@Graphics@@6B@` lever from Wave 6.
    static void* vtbl = nullptr;
    if (!vtbl) vtbl = ResolveExport("??_7DialogInstance@UI@@6B@");
    F<void*>(d, 0x00) = vtbl;

    d->_ui         = ui;                           // +0x10
    d->_dialog     = nullptr;                      // +0x18  no Dialog resource
    d->_flags      = 0;                            // +0x20  \ one qword store
    d->_flags2     = 0;                            // +0x24  /
    d->_parent     = nullptr;                      // +0x28  written 0 then...
    d->_parent     = *parentPtr;                   // +0x28  ...the real value
    d->_dataSource = dataSource;                   // +0x30
    // ENDGAME B.3.2 step 2 — these three stay raw field writes, deliberately:
    // `Collection::LinkedList<T>` has NO user-provided default constructor on
    // purpose (Collection.h's banner), so there is no member call this could
    // become; it is the ctor's own explicit zeroing, exactly like retail's.
    d->_children._head  = nullptr;                 // +0x38
    d->_children._tail  = nullptr;                 // +0x40
    d->_children._count = 0;                       // +0x48   (+0x4c left alone)
    d->_root       = nullptr;                      // +0x50  written 0 then...
    d->_root       = *rootPtr;                     // +0x50  ...the real value
    d->_spriteSheet = nullptr;                     // +0x58
    d->_spriteIndex = -1;                          // +0x60   (+0x64 left alone)
    d->_timers._data      = nullptr;               // +0x68
    d->_timers._size      = 0;                     // +0x70  \ one qword store
    d->_timers._capacity  = 0;                     // +0x74  /
    d->_controllers._data     = nullptr;           // +0x78
    d->_controllers._size     = 0;                 // +0x80  \ one qword store
    d->_controllers._capacity = 0;                 // +0x84  /
    d->_upperLeftX = 0;                            // +0x88  \ one qword store
    d->_upperLeftY = 0;                            // +0x8c  /
    d->_hoverMouseX = -1;                          // +0x90  a QWORD -1 --
    d->_hoverMouseY = -1;                          // +0x94  both hover ints
    d->_hovering    = 0;                           // +0x98  a single BYTE
    d->_hoverTime   = 0;                           // +0xa0   (+0x99.. left alone)
    d->_userData    = userData;                    // +0xa8
    d->_userFlags   = userFlags;                   // +0xb0   (+0xb4 left alone)

    // 0x18011477e: the root Element points back at us. UNGUARDED in retail — if
    // the caller handed us a null InternalPtr this faults. Faithful.
    F<void*>(d->_root, EL::kDialogParent) = self;

    // ---- link into the parent's child list -------------------------------
    // A PREPEND: head at +0x38, tail at +0x40, node = {element, prev, next}
    // with prev at +0x08 and next at +0x10. DialogInstance.h records the
    // tie-break (`?GetFirstDialog@UserInterface@UI@@`, 0x180031150, reads the
    // FIRST element from the list's +0x00) and warns that reading a ctor alone
    // gets this backwards.
    //
    // ENDGAME B.3.2 step 2 — NOT converted, deliberately. `Collection.h`'s
    // `LinkedList<T>::Add()` appends at the TAIL (its own banner: "append at
    // the TAIL... `_head` is the BACKMOST... `_tail` the FRONTMOST"); this body
    // inserts at the HEAD, which is the opposite end and not a member the
    // class has today. A hypothetical `AddFront()` — NewNode, then link at
    // `_head` with the old head's `prev` pointed back at the new node, mirror
    // image of `Add()` — would be the conversion, but retail ships exactly one
    // `LinkedList<T=8>` instantiation and this is its only head-insert site,
    // so there is no second caller to justify adding a class member for it
    // yet. Left as the raw field manipulation, including the guard-free
    // linking through a possibly-null `node` on OOM (retail's own asymmetry).
    if (DialogInstance* const parent = static_cast<DialogInstance*>(d->_parent)) {
        // 0x1801147af: `LEA EDX,[RBP+0x18] / LEA R8D,[RBP+0x8]` with RBP == 0,
        // wrapped in Enter/Allocate/Leave — i.e. exactly ?MemAlloc@…(0x18, 8).
        uint8_t* node = static_cast<uint8_t*>(MemAlloc(0x18, 8));
        if (node) {
            F<void*>(node, 0x00) = self;      // element
            F<void*>(node, 0x08) = nullptr;   // prev
            F<void*>(node, 0x10) = nullptr;   // next
        }
        // 0x1801147e1: on an OOM `node` is null (RBP stayed 0) and retail links
        // the null in anyway. Reproduced, guard-free.
        if (void* head = parent->_children._head) {
            F<void*>(head, 0x08) = node;                      // old head->prev
            F<void*>(node, 0x10) = parent->_children._head;   // node->next
        } else {
            parent->_children._tail =
                reinterpret_cast<decltype(parent->_children._tail)>(node);
        }
        parent->_children._head =
            reinterpret_cast<decltype(parent->_children._head)>(node);
        parent->_children._count++;
    }

    return self;   // 0x180114803 `MOV RAX,R14`
}

// ============================================================================
// ??0ComboDialog@UI@@QEAA@PEAVUserInterface@1@
//     V?$InternalPtr@VElement@UI@@@Object@@
//     V?$InternalPtr@VDialogInstance@UI@@@4@
//     V?$InternalPtr@VComboElement@UI@@@4@@Z               retail 0x180111d00
//
//     ComboDialog::ComboDialog(UserInterface* ui,
//                              InternalPtr<Element> root,
//                              InternalPtr<DialogInstance> parent,
//                              InternalPtr<ComboElement> combo)
//
// Builds the drop-down popup for a combo box: run the base ctor on an
// already-built Element tree, find the ListElement and the ClipRegionElement
// inside it, then size the list's container so the drop-down is exactly as wide
// and tall as the combo box that opened it — unless the clip region already
// reports more content than fits, in which case the padded size wins.
//
// All four InternalPtr parameters arrive by hidden reference; the parent is
// deliberately NOT forwarded to the base ctor. Both facts are argued in the
// banner. Returns `this` in RAX.
// ============================================================================
extern "C" void* RtUi_ComboDialog_Ctor(void* self, void* ui, void** rootPtr,
                                       void** parentPtr, void** comboPtr) {
    ++g_cdCtorEntries;
    ComboDialog* const cd = static_cast<ComboDialog*>(self);

    // ---- 0x180111d31-0x180111d55: the base ctor ---------------------------
    // The Element InternalPtr is COPIED into a local (0x180111d35) and the
    // parent slot is ZEROED (0x180111d31); the base ctor is handed the address
    // of each. dataSource / userData / userFlags are all zero — the three stack
    // arguments at [R11-0x68], [R11-0x60] and [RSP+0x30].
    void* rootTmp   = *rootPtr;
    void* parentTmp = nullptr;
    {
        using Fn = void* (*)(void*, void*, void**, void**, void*, void*, int32_t);
        static Fn fn = nullptr;
        if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1801146b0));
        fn(self, ui, &rootTmp, &parentTmp, nullptr, nullptr, 0);
    }

    // 0x180111d56: stamp our own vtable over the base's.
    static void* vtbl = nullptr;
    if (!vtbl) vtbl = ResolveExport("??_7ComboDialog@UI@@6B@");
    F<void*>(cd, 0x00) = vtbl;

    cd->_list         = nullptr;      // +0xb8
    cd->_parentDialog = nullptr;      // +0xc0  written 0 then...
    cd->_parentDialog = *parentPtr;   // +0xc0  ...the real value
    cd->_combo        = nullptr;      // +0xc8  written 0 then...
    cd->_combo        = *comboPtr;    // +0xc8  ...the real value

    // ---- 0x180111d91-0x180111e1e: find the ListElement --------------------
    // Re-reads the INCOMING InternalPtr (`MOV RDI,[R15]`, R15 == the R8 spilled
    // at entry), not the local copy handed to the base ctor.
    const uint32_t tidList = TypeIdListElement();
    {
        void* const found = FindSelfOrUnderChildren(*rootPtr, tidList);
        // 0x180111e0e: CMOVNZ — null unless it really is a ListElement.
        cd->_list = IsDerivedFrom(found, tidList) ? found : nullptr;
    }

    // 0x180111e1e / 0x180111e24: ?SetElementId@(0) then
    // ?SetFlagsOnOff@(kElementFlagToolTip16, 0), both on the list.
    // UNGUARDED derefs of a pointer the CMOV above may have just nulled.
    F<int32_t>(cd->_list, EL::kElementId) = 0;
    SetFlagsOnOff(cd->_list, kElementFlagToolTip16, 0);

    // ---- 0x180111e56-0x180111edb: find the ClipRegionElement --------------
    const uint32_t tidClip = TypeIdClipRegionElement();
    void* clipRegion = nullptr;
    {
        void* const found = FindSelfOrUnderChildren(*rootPtr, tidClip);
        clipRegion = IsDerivedFrom(found, tidClip) ? found : nullptr;
    }

    // ---- 0x180111edf-0x180111f38: the list's second child, as a container --
    // Retail re-walks `_list -> _children.data[1]` and re-runs IsDerivedFrom
    // for EACH of the two dwords. Reproduced rather than hoisted: the two
    // lookups are separate in the shipped stream and hoisting them would be a
    // different instruction count for the same answer only if nothing in
    // between can change the tree, which is not something this port can claim.
    const uint32_t tidContainer = TypeIdContainerElement();
    int32_t targetWidth = 0;
    int32_t targetHeight = 0;
    {
        void* const kid = F<void**>(cd->_list, EL::kChildrenData)[1];
        void* const c = IsDerivedFrom(kid, tidContainer) ? kid : nullptr;
        targetWidth = F<int32_t>(c, kContainerWidth);
    }
    {
        void* const kid = F<void**>(cd->_list, EL::kChildrenData)[1];
        void* const c = IsDerivedFrom(kid, tidContainer) ? kid : nullptr;
        targetHeight = F<int32_t>(c, kContainerHeight);
    }

    // ---- 0x180111f3e-0x180111f5a: the clip region's content size ----------
    int32_t contentWidth = 0;
    int32_t contentHeight = 0;
    ClipGetContentSize(clipRegion, &contentWidth, &contentHeight);

    // ---- 0x180111f61-0x18011200e: the combo box's padded size -------------
    int32_t paddedWidth = 0;
    int32_t paddedHeight = 0;
    GetPaddedSize(cd->_combo, &paddedWidth, &paddedHeight);

    // ---- 0x180112010-0x1801120b9: grow to the combo when nothing forces a
    //      size and the clip region does not already need more ---------------
    // `container` here is the RAW second child — retail keeps the untyped
    // pointer in RDI for the pads and uses the type-checked one only for the
    // +0xc0 / +0xc4 tests.
    void* const container = F<void**>(cd->_list, EL::kChildrenData)[1];
    {
        void* const kid = F<void**>(cd->_list, EL::kChildrenData)[1];
        void* const c = IsDerivedFrom(kid, tidContainer) ? kid : nullptr;
        if (F<int32_t>(c, kContainerWidth) <= 0) {
            const int32_t left  = F<int32_t>(container, EL::kLeftPad);
            const int32_t right = F<int32_t>(container, EL::kRightPad);
            if (contentWidth < paddedWidth - right - left)
                targetWidth = paddedWidth - left - right;
        }
    }
    {
        void* const kid = F<void**>(cd->_list, EL::kChildrenData)[1];
        void* const c = IsDerivedFrom(kid, tidContainer) ? kid : nullptr;
        if (F<int32_t>(c, kContainerHeight) <= 0) {
            const int32_t top    = F<int32_t>(container, EL::kTopPad);
            const int32_t bottom = F<int32_t>(container, EL::kBottomPad);
            if (contentHeight < paddedHeight - bottom - top)
                targetHeight = paddedHeight - top - bottom;
        }
    }

    // ---- 0x1801120bb-0x180112104: ?SetSize@ContainerElement@ --------------
    {
        void* const kid = F<void**>(cd->_list, EL::kChildrenData)[1];
        void* const c = IsDerivedFrom(kid, tidContainer) ? kid : nullptr;
        ContainerSetSize(c, targetWidth, targetHeight);
    }

    return self;   // 0x180112106 `MOV RAX,R14`
}

// ============================================================================
// Registration. BOTH doors — BY NAME and BY ADDRESS, unconditionally.
//
// The address door is not redundant with the name door: a caller that reaches a
// body by address silently keeps using the DLL when only the name is registered
// (reference_bypassed_port_needs_a_source_checker.md). Both are spelled as
// LITERALS so tools/runtime_census.py can see them
// (reference_census_cannot_see_nonliteral_registrations.md).
//
// No kill switch and no env gate: DIRECTION.md §2 — a port registers
// unconditionally.
// ============================================================================
int RtUiComboDialogCtorRegister() {
    int n = 0;

    RegisterRuntimeExport("??2ComboDialog@UI@@SAPEAX_K@Z",
                          (void*)&RtUi_ComboDialog_OperatorNew);
    RegisterRuntimeAddr(0x180074770, (void*)&RtUi_ComboDialog_OperatorNew);      ++n;

    RegisterRuntimeExport(
        "??0DialogInstance@UI@@QEAA@PEAVUserInterface@1@"
        "V?$InternalPtr@VElement@UI@@@Object@@"
        "V?$InternalPtr@VDialogInstance@UI@@@4@PEAVBase@4@PEAXH@Z",
        (void*)&RtUi_DialogInstance_CtorFromElement);
    RegisterRuntimeAddr(0x1801146b0, (void*)&RtUi_DialogInstance_CtorFromElement); ++n;

    RegisterRuntimeExport(
        "??0ComboDialog@UI@@QEAA@PEAVUserInterface@1@"
        "V?$InternalPtr@VElement@UI@@@Object@@"
        "V?$InternalPtr@VDialogInstance@UI@@@4@"
        "V?$InternalPtr@VComboElement@UI@@@4@@Z",
        (void*)&RtUi_ComboDialog_Ctor);
    RegisterRuntimeAddr(0x180111d00, (void*)&RtUi_ComboDialog_Ctor);             ++n;

    return n;
}
