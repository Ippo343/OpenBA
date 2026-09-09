// ============================================================================
// runtime/src/ui/userinterface_showdialog.cpp — Wave 10B lane SHOW
// ?ShowDialog@UserInterface@UI@@QEAAHV?$InternalPtr@VDialogInstance@UI@@@Object@@@Z
// retail 0x18011fd10, ordinal 4034
// ============================================================================
//
// THE RECORDED BLOCKER, AND WHAT WAS WRONG WITH IT
//
// WAVE10_UI_CENSUS.md §6 and WAVE10_LANE_UI1.md §4 record this row as blocked
// because "`BANISHED_MODAL_ANSWER` is NOT a route -- it is a BYPASS. Both
// harness call sites skip `fn_UI_ShowDialog` entirely."
//
// The MECHANISM in that sentence is exactly right and is re-derived below. The
// ENUMERATION is wrong: there are FOUR call sites, not two, and the two UI1 did
// not name are not bypassed at all --
//
//     code/Game/Dialog/optionsdialog.cpp:1646   (the input-bind capture dialog)
//     code/Game/Dialog/optionsdialog.cpp:1759   (the binding-conflict confirm)
//
// both call `fn_UI_ShowDialog` unconditionally. And the two that ARE bypassed
// are bypassed CONDITIONALLY: `moddialog.cpp:697` takes the bypass only when
// `BANISHED_MODAL_ANSWER` is set, and `saveloaddialog.cpp:938` only when it is
// set or the run is unattended. So the body was always reachable; what did not
// exist was a way to make the nested pump TERMINATE, which is what
// `standalone/modal_driver.cpp` now provides.
//
// ---------------------------------------------------------------------------
// WHAT THE BODY DOES (shipped bytes; see modal_driver.cpp for the full trace)
// ---------------------------------------------------------------------------
// Allocate + construct a SECOND UserInterface, inherit eight qwords of the
// caller's presentation state into it, hand the dialog to it, register a
// draw/update callback pair with `Engine::MainLoop`, spin `MainLoop::Frame`
// until the dialog leaves the nested UI's list, un-register both callbacks,
// destroy and free the nested UI, and return the dialog's result dword.
//
// FACTS THE DECOMPILED C DROPPED OR MISLEADS ON
// (reference_ghidra_c_output_vs_disassembly.md):
//
//  * **`dialog->_flags |= 0x20` is NOT the modal bit.** Ghidra shows the OR and
//    nothing more. `?IsModal@DialogInstance@UI@@QEBA_NXZ` (0x180114170) reads
//    `(flags >> 1) & 1`; the bit ShowDialog sets is read by
//    `?DeleteOnClose@DialogInstance@UI@@QEBA_NXZ` (0x180114110) as
//    `!((flags >> 5) & 1)`. Setting it makes DeleteOnClose FALSE -- the dialog
//    survives its own close so the caller can read the result out of it. Every
//    call site depends on that: optionsdialog.cpp reads dialog+0xC0/+0xC4
//    AFTER ShowDialog returns and only then DeletePtr's.
//
//  * **The return is read from the InternalPtr a SECOND time.** 0x1801202e2 is
//    `MOV RAX,[RDI]; MOV EAX,[RAX+0x24]` where RDI is the reloaded argument --
//    it is NOT cached from the top of the function. If the dialog pointer in
//    the caller's InternalPtr were rewritten during the modal, retail would read
//    the new one. Reproduced faithfully (`*(int*)(*dialogPtr + 0x24)`).
//
//  * **+0x24 is the RESULT, not `_flags2`.** `runtime/types/ui/DialogInstance.h`
//    currently calls it "_flags2 ... no ported body reads it". It is the field
//    `?GetResult@DialogInstance@UI@@QEBAHXZ` (0x1800455c0, two instructions:
//    `MOV EAX,[RCX+0x24]; RET`) returns and `?SetDialogResult@` (0x1800455d0,
//    `MOV [RCX+0x24],EDX; RET`) writes, and it is what this function returns.
//
//  * **The nested-loop test reloads the dialog pointer every iteration.**
//    0x180120150 `MOV RCX,[RDI]` sits INSIDE the loop, above the list walk.
//
//  * **The loop has TWO exits.** The list-walk falling off the end (dialog gone)
//    AND `MainLoop::Frame` returning true (a WM_QUIT / Application::Quit while
//    the modal is up). Ghidra's `while (!bVar3)` renders this correctly but it
//    is easy to lose: the second exit is why a modal does not wedge a quit.
//
//  * **The two callback appends are the SAME inlined grow-and-compact routine
//    twice**, once on (+0x80,+0x88,+0x8c) and once on (+0x90,+0x98,+0x9c), with
//    the `*3` overallocation policy applied only when growing by exactly one
//    from a non-empty array. Factored here as AppendCallback / RemoveCallback;
//    the arithmetic is transcribed, not re-derived.
//
//  * **`MemAlloc(0x128, 4)`** -- alignment FOUR, not 8. 0x18011fd59 is
//    `MOV R8D,0x4`. It pins sizeof(UI::UserInterface) at 0x128.
//
// ---------------------------------------------------------------------------
// WHAT IS AND IS NOT VERIFIED -- read this before reading the switch as a claim
// ---------------------------------------------------------------------------
// A PREDICTIVE oracle over this body is worth almost nothing, and UI1 said so
// correctly: everything it writes other than one flag bit is an address the
// allocator chose. This lane therefore uses the TWIN-SUBJECT DIFFERENTIAL in
// `standalone/modal_driver.cpp` instead -- two identically-built dialogs, one
// shown through the shipped DLL body and one through ours, comparing the return
// value, the flag bit, and MainLoop's two callback counts back at their
// pre-call values. See docs/runtime_port/WAVE10B_LANE_SHOW.md for the numbers
// and for the list of effects that instrument does NOT cover.
// ============================================================================

#include <cstdint>
#include <cstdio>

#include "resolve.h"
#include "rt_port.h"

namespace {

// --- Shipped addresses of the callees, for the reader ------------------------
// Only kAddDialogsSetSize is used to RESOLVE; the rest are reached by mangled
// name (see Resolved/R() below) and are listed here because a Ghidra address is
// what a future reader will have in hand.
//   0x180120de0  ??0UserInterface@UI@@QEAA@XZ
//   0x180120b90  ??1UserInterface@UI@@QEAA@XZ
//   0x1801213c0  ?Update@UserInterface@UI@@QEAAHAEBVTime@System@@@Z
//   0x180120310  ?UpdateStandAloneUI@UserInterface@UI@@CAXAEBVTime@System@@PEAX@Z
//   0x1801203f0  ?DrawStandAloneUI@UserInterface@UI@@CAXAEAVContext@Video@@PEAX@Z
//   0x1800dd100  ?Frame@MainLoop@Engine@@QEAA_N_N@Z
//   0x1801fe750  ?_singleton@MainLoop@Engine@@1PEAV12@EA          (DATA)
//   0x1801fe728  ?_secondsPerTick@Timer@System@@0NA               (DATA)
constexpr uintptr_t kAddDialogsSetSize      = 0x18007a0d0;  // Array<InternalPtr<DialogInstance>>::SetSize

// --- Object layout ----------------------------------------------------------
constexpr uintptr_t kUI_Dialogs      = 0x00;   // list head; node->dialog +0x00, node->next +0x10
constexpr uintptr_t kUI_AddDialogs   = 0x28;   // Array<InternalPtr<DialogInstance>>; count at +0x08
constexpr uintptr_t kUI_InheritFirst = 0x90;   // eight qwords copied verbatim, +0x90..+0xc8
constexpr int       kUI_InheritCount = 8;
constexpr size_t    kUI_Size         = 0x128;

constexpr uintptr_t kDlg_Ui     = 0x10;
constexpr uintptr_t kDlg_Flags  = 0x20;
constexpr uintptr_t kDlg_Result = 0x24;

constexpr uintptr_t kML_DrawArr = 0x80, kML_DrawCnt = 0x88, kML_DrawCap = 0x8c;
constexpr uintptr_t kML_UpdArr  = 0x90, kML_UpdCnt  = 0x98, kML_UpdCap  = 0x9c;

struct Entry { void* fn; void* ctx; };

// The exported allocator statics. Retail inlines
// `EnterCriticalSection(alloc+0x640); Allocate(...); LeaveCriticalSection(...)`
// six times in this body; `?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z`
// (0x1800c7a50) and `?MemFree@MemoryAllocator@System@@SAXPEAX@Z` (0x1800c7ad0)
// ARE that sequence -- lane UI1 decompiled and compared them. Calling them
// reproduces the lock discipline exactly with no windows.h in a runtime/src
// file, which is what ~30 other ported files already do.
typedef void* (*fn_MemAllocS_t)(uint64_t size, int align);
typedef void  (*fn_MemFreeS_t)(void* p);

void* MemAllocS(uint64_t size, int align) {
    static fn_MemAllocS_t f = (fn_MemAllocS_t)ResolveExport(
        "?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z");
    return f ? f(size, align) : nullptr;
}
void MemFreeS(void* p) {
    static fn_MemFreeS_t f = (fn_MemFreeS_t)ResolveExport(
        "?MemFree@MemoryAllocator@System@@SAXPEAX@Z");
    if (f) f(p);
}

typedef void* (*fn_UICtor_t)(void*);
typedef void  (*fn_UIDtor_t)(void*);
typedef int   (*fn_UIUpdate_t)(void*, const void* time);
typedef void  (*fn_SetSize_t)(void* arr, int newCount, const void* fill);
typedef bool  (*fn_Frame_t)(void*, bool);

// EVERY callee of this body except one has a NAME door. That was checked
// against the shipped export directory with pefile rather than assumed:
// `UpdateStandAloneUI` and `DrawStandAloneUI` mangle as `CAXA...`, i.e. private
// STATIC members, and this DLL exports its private members
// (reference_aeaa_is_private_not_unexported.md). Reaching them by address when
// a name exists is how silent address doors accumulate, so they are named here.
//
// THE ONE ADDRESS DOOR, declared as a cutover obligation
// (reference_by_address_door_dies_at_cutover.md):
//   0x18007a0d0  Collection::Array<InternalPtr<DialogInstance>>::SetSize
//                NOT exported. Already ported and registered by
//                runtime/src/collection/array.cpp (J3_ENTRY3(18007a0d0,
//                kSpec_Fill8Z)) and reached through ResolveAddr(Module::Runtime,
//                ...), which consults the port registry first -- so this door is
//                already satisfied today, unlike a fresh one.
struct Resolved {
    fn_UICtor_t   ctor;
    fn_UIDtor_t   dtor;
    fn_UIUpdate_t update;
    fn_SetSize_t  setSize;
    fn_Frame_t    frame;
    void*         drawFn;
    void*         updFn;
    void**        mlSlot;    // &Engine::MainLoop::_singleton
    const double* secondsPerTick;
};

const Resolved& R() {
    static Resolved r = {
        (fn_UICtor_t)  ResolveExport("??0UserInterface@UI@@QEAA@XZ"),
        (fn_UIDtor_t)  ResolveExport("??1UserInterface@UI@@QEAA@XZ"),
        (fn_UIUpdate_t)ResolveExport("?Update@UserInterface@UI@@QEAAHAEBVTime@System@@@Z"),
        (fn_SetSize_t) ResolveAddr(Module::Runtime, kAddDialogsSetSize),
        (fn_Frame_t)   ResolveExport("?Frame@MainLoop@Engine@@QEAA_N_N@Z"),
        ResolveExport("?DrawStandAloneUI@UserInterface@UI@@CAXAEAVContext@Video@@PEAX@Z"),
        ResolveExport("?UpdateStandAloneUI@UserInterface@UI@@CAXAEBVTime@System@@PEAX@Z"),
        (void**)ResolveExport("?_singleton@MainLoop@Engine@@1PEAV12@EA"),
        (const double*)ResolveExport("?_secondsPerTick@Timer@System@@0NA"),
    };
    return r;
}

// Retail's inlined callback-array append, transcribed.
// 0x18011fe7d..0x18011ffd6 (draw) and 0x18011ffe7..0x18012013e (update) are the
// same code on different offsets.
//
//   newCount = count + 1
//   if (newCount == count) return;                 // dead in practice; kept
//   old = arr
//   count32 = newCount
//   if (newCount > cap) {
//       cap = newCount
//       if (newCount == oldCount + 1 && oldCount != 0) cap = newCount * 3
//       arr = MemAlloc(cap * 16, 8)
//   }
//   // copy: if the buffer did NOT move, only the tail is written; if it DID
//   // move, everything from index 0 is written, sourced from `old` below
//   // oldCount and from `entry` at or above it.
//   start = (old == arr) ? oldCount : 0
//   for (i = start; i < newCount; ++i)
//       arr[i] = (i < oldCount) ? old[i] : entry;
//   if (old != arr && old) MemFree(old)
void AppendCallback(uintptr_t ml, uintptr_t arrOff, uintptr_t cntOff, uintptr_t capOff,
                    const Entry& entry) {
    int oldCount = *reinterpret_cast<int*>(ml + cntOff);
    int newCount = oldCount + 1;
    if (newCount == oldCount) return;

    Entry* old = *reinterpret_cast<Entry**>(ml + arrOff);
    *reinterpret_cast<int*>(ml + cntOff) = newCount;

    if (newCount > *reinterpret_cast<int*>(ml + capOff)) {
        *reinterpret_cast<int*>(ml + capOff) = newCount;
        if (newCount == oldCount + 1 && oldCount != 0)
            *reinterpret_cast<int*>(ml + capOff) = newCount * 3;
        int cap = *reinterpret_cast<int*>(ml + capOff);
        *reinterpret_cast<Entry**>(ml + arrOff) =
            reinterpret_cast<Entry*>(MemAllocS((uint64_t)cap << 4, 8));
    }

    Entry* cur = *reinterpret_cast<Entry**>(ml + arrOff);
    int i = (old == cur) ? oldCount : 0;
    for (; i < newCount; ++i) {
        Entry* dst = cur ? (cur + i) : nullptr;
        if (!dst) continue;                       // retail's `if (puVar17 != 0)`
        const Entry* src = (i < oldCount) ? (old + i) : &entry;
        *dst = *src;
    }

    if (old != cur && old) MemFreeS(old);
}

// Retail's inlined removal-and-compaction, 0x180120179.. and 0x180120210..
// Scans for every (fn, ctx) match and shifts the tail down over it.
void RemoveCallback(uintptr_t ml, uintptr_t arrOff, uintptr_t cntOff,
                    void* fn, void* ctx) {
    int i = 0;
    int count = *reinterpret_cast<int*>(ml + cntOff);
    if (count <= 0) return;
    do {
        Entry* arr = *reinterpret_cast<Entry**>(ml + arrOff);
        if (arr && arr[i].fn == fn && arr[i].ctx == ctx) {
            int j = i;
            while (j + 1 < *reinterpret_cast<int*>(ml + cntOff)) {
                Entry* a = *reinterpret_cast<Entry**>(ml + arrOff);
                if (a) a[j] = a[j + 1];
                ++j;
            }
            *reinterpret_cast<int*>(ml + cntOff) -= 1;
            --i;
        }
        ++i;
        count = *reinterpret_cast<int*>(ml + cntOff);
    } while (i < count);
}

// Target of defect 1 (see the negative-control block below). Never registered
// in any shipping configuration.
void DefectNoop(void*, void*) {}

}  // namespace

// ---------------------------------------------------------------------------
// NEGATIVE CONTROL FOR THE TWIN-SUBJECT DIFFERENTIAL
// ---------------------------------------------------------------------------
// `standalone/modal_driver.cpp` sets this immediately before the OURS cell and
// clears it after, so each round injects ONE defect aimed at ONE screen of the
// comparison. Scoring the screens separately is deliberate: a control scored on
// the UNION of several screens reads 3/3 while hiding that one screen caught
// nothing (reference_union_control_hides_a_blind_screen.md).
//
//   1  append an extra BENIGN callback pair and never remove it
//        -> must be caught by drawCount / updCount
//        (benign on purpose: leaking one of OUR OWN entries would leave retail's
//         Frame calling UpdateStandAloneUI on a freed nested UI every tick)
//   2  return the dword at +0x28 instead of the result at +0x24
//        -> must be caught by `return`
//   3  additionally set flags bit 0x80000000 (nothing reads it) alongside 0x20
//        -> must be caught by `flags`
//
// Zero in every shipping configuration; nothing but the self-test writes it.
extern "C" { int g_RtShowDialogDefect = 0; }

// `InternalPtr<DialogInstance>` is passed BY VALUE, which for a class with a
// non-trivial copy ctor means MSVC x64 passes its ADDRESS (WAVE7.md). So the
// second parameter is `DialogInstance**`.
extern "C" int RtUi_UserInterface_ShowDialog(void* self, void** dialogPtr) {
    uintptr_t outer = reinterpret_cast<uintptr_t>(self);

    // 1. Allocate + construct the nested UserInterface. Alignment 4 (MOV R8D,4).
    void* nested = MemAllocS(kUI_Size, 4);
    if (nested) {
        auto ctor = R().ctor;
        nested = ctor(nested);        // retail takes the RETURN, not the argument
    }
    uintptr_t n = reinterpret_cast<uintptr_t>(nested);

    // 2. Inherit +0x90..+0xc8. Retail does this with EIGHT unconditional
    //    qword moves -- including on the path where the allocation FAILED and
    //    `nested` is null. That is an unguarded null store in the shipped code
    //    and it stays unguarded here (CLAUDE.md: don't "fix" a faithful crash).
    for (int i = 0; i < kUI_InheritCount; ++i) {
        *reinterpret_cast<uint64_t*>(n + kUI_InheritFirst + i * 8) =
            *reinterpret_cast<uint64_t*>(outer + kUI_InheritFirst + i * 8);
    }

    // 3. Hand the dialog to the nested UI and clear its DeleteOnClose.
    uintptr_t dlg = reinterpret_cast<uintptr_t>(*dialogPtr);
    *reinterpret_cast<void**>(dlg + kDlg_Ui) = nested;
    *reinterpret_cast<uint32_t*>(dlg + kDlg_Flags) |= 0x20;
    if (g_RtShowDialogDefect == 3)
        *reinterpret_cast<uint32_t*>(dlg + kDlg_Flags) |= 0x80000000u;   // control

    // 4. Dialog vtable slot 4 (+0x20). Retail re-reads the InternalPtr, stores
    //    the dialog into a stack slot, and calls through it with RCX = dialog.
    void* dialog = *dialogPtr;
    {
        void** vt = *reinterpret_cast<void***>(dialog);
        ((void (*)(void*))vt[4])(dialog);
    }

    // 5. Push the dialog into nested->_addDialogs. `&dialog` is the by-value
    //    InternalPtr the stack slot from step 4 holds -- retail passes that same
    //    slot (LEA R8,[RSP+0x80]).
    {
        auto setSize = R().setSize;
        void* arr = reinterpret_cast<void*>(n + kUI_AddDialogs);
        int   cnt = *reinterpret_cast<int*>(n + kUI_AddDialogs + 8);
        setSize(arr, cnt + 1, &dialog);
    }

    // 6. Update(nested, Time(0)). Retail computes `(int64)(0.0 / _secondsPerTick)`
    //    -- a DIVSD by a loaded double, not a constant zero. 0.0/x is 0 for every
    //    finite non-zero x, but it is NaN->0 (CVTTSD2SI of NaN yields the
    //    integer indefinite 0x8000000000000000) if _secondsPerTick is 0.
    //    Transcribed as the division so that degenerate case behaves alike.
    {
        double spt = *R().secondsPerTick;
        int64_t t = (int64_t)(0.0 / spt);
        auto upd = R().update;
        upd(nested, &t);
    }

    // 7 + 8. Register the draw and update callbacks, in that order.
    void* drawFn = R().drawFn;
    void* updFn  = R().updFn;
    void* ml     = *R().mlSlot;
    uintptr_t m  = reinterpret_cast<uintptr_t>(ml);

    AppendCallback(m, kML_DrawArr, kML_DrawCnt, kML_DrawCap, Entry{ drawFn, nested });
    // Retail re-reads MainLoop::_singleton before the second append. Kept.
    ml = *R().mlSlot;
    m  = reinterpret_cast<uintptr_t>(ml);
    AppendCallback(m, kML_UpdArr, kML_UpdCnt, kML_UpdCap, Entry{ updFn, nested });

    // 9. The nested pump. Two exits: the dialog leaving nested->_dialogs, and
    //    Frame() returning true (quit). The dialog pointer is reloaded from the
    //    caller's InternalPtr on every iteration.
    {
        auto frame = R().frame;
        for (;;) {
            void* want = *dialogPtr;
            void* node = *reinterpret_cast<void**>(n + kUI_Dialogs);
            bool present = false;
            while (node) {
                if (*reinterpret_cast<void**>(node) == want) { present = true; break; }
                node = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(node) + 0x10);
            }
            if (!present) break;
            void* cur = *R().mlSlot;
            if (frame(cur, true)) break;
        }
    }

    // 10. Un-register both callbacks (draw first, then update -- retail's order).
    ml = *R().mlSlot;
    m  = reinterpret_cast<uintptr_t>(ml);
    RemoveCallback(m, kML_DrawArr, kML_DrawCnt, drawFn, nested);
    ml = *R().mlSlot;
    m  = reinterpret_cast<uintptr_t>(ml);
    RemoveCallback(m, kML_UpdArr, kML_UpdCnt, updFn, nested);

    // 11. Destroy and free the nested UI.
    {
        auto dtor = R().dtor;
        dtor(nested);
    }
    MemFreeS(nested);

    if (g_RtShowDialogDefect == 1) {                                     // control
        ml = *R().mlSlot;
        m  = reinterpret_cast<uintptr_t>(ml);
        // A no-op, NOT drawFn/updFn: a leaked real entry would have retail's
        // Frame calling UpdateStandAloneUI on a freed nested UI every tick, and
        // a control that crashes the subject measures nothing.
        void* noop = (void*)&DefectNoop;
        AppendCallback(m, kML_DrawArr, kML_DrawCnt, kML_DrawCap, Entry{ noop, nullptr });
        AppendCallback(m, kML_UpdArr,  kML_UpdCnt,  kML_UpdCap,  Entry{ noop, nullptr });
    }

    // 12. The result, re-read through the caller's InternalPtr.
    if (g_RtShowDialogDefect == 2)                                       // control
        return *reinterpret_cast<int*>(
            reinterpret_cast<uintptr_t>(*dialogPtr) + 0x28);
    return *reinterpret_cast<int*>(
        reinterpret_cast<uintptr_t>(*dialogPtr) + kDlg_Result);
}

// Registered under BOTH the mangled name and the address (WAVE7.md §3): a
// name-only registration is dead at any call site that reaches the DLL through
// the address fallback, while an oracle over it still reports clean.
int RtUiShowDialogRegister() {
    RegisterRuntimeExport(
        "?ShowDialog@UserInterface@UI@@QEAAHV?$InternalPtr@VDialogInstance@UI@@@Object@@@Z",
        (void*)&RtUi_UserInterface_ShowDialog);
    RegisterRuntimeAddr(0x18011fd10, (void*)&RtUi_UserInterface_ShowDialog);
    return 1;
}
