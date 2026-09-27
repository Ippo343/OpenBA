// ============================================================================
// runtime/src/ui/userinterface_close.cpp — Wave 10 lane CLOSEDLG
//
//   ?CloseDialog@UserInterface@UI@@QEAAXV?$InternalPtr@VDialogInstance@UI@@@Object@@@Z
//   0x180120f20   ordinal 1725
// ============================================================================
//
// WHAT THIS ROW WAS BLOCKED ON, AND WHAT IT IS ACTUALLY BLOCKED ON
//
// Recorded blocker through Waves 7-9: "allocator NO-GO". Lane UI1 retired that
// (docs/runtime_port/WAVE10_LANE_UI1.md §1b): the NO-GO is a verdict on
// REPLACING the allocator arena, not on a ported body CALLING the shipped one,
// and this function's allocator use is two inlined `MemFree`s plus a
// `DynArrayPush`, all of which have exported doors.
//
// The blocker UI1 said would survive is ORACLE COVERAGE, and it does survive,
// but it is SMALLER than UI1 predicted. UI1 wrote:
//
//     "a predictive round over CloseDialog would compare a handful of scalars
//      on `this` (+0x38/+0x40 tooltip clear, +0x30 array shrink) against a very
//      large Exclude set."
//
// Two of the five effects UI1 listed as unreachable are in fact FULLY
// PREDICTABLE and are compared here:
//
//   * **the unlinks in another object's list** (UI1's item 4). `parent+0x38`,
//     `parent+0x40` and `parent+0x48`, plus the two neighbour-node fixups
//     `prev->next` and `next->prev`, are computed from pointers this body can
//     read and are left in memory retail does not free. UI1 called this "a
//     graph whose extent must be enumerated before Watch can see it" — so this
//     body ENUMERATES IT, by calling `Watch` from inside itself once it has
//     walked to the node. That is sound precisely because a predicting body
//     writes nothing: the snapshot is still taken before the DLL runs.
//
//   * **the `_addDialogs` compaction** — the shift loop at 0x180121337 writes
//     the array's own storage, which is not inside `this` at all. It is watched
//     and compared element by element.
//
// The three that genuinely are not expressible, each declared with `Exclude`
// naming the callee that owns it:
//
//   1. the two `MemFree(node)` calls — the node's post-state belongs to the
//      allocator, so the `node->prev = 0` / `node->next = 0` stores retail
//      makes just before freeing are recorded for read-back and NOT compared
//      (they land in an Exclude region and count as `unverifiable`);
//   2. the `DynArrayPush` onto `_removedDialogs` (+0x18/+0x20) — the pushed
//      element lands at an address the allocator chose;
//   3. the `OnClosed` vtable release, `EndDragDrop`, `UpdateEvents` and
//      `Array<UI::Event>::SetSize` — four suppressed calls whose footprints on
//      `this` are `_events` (+0x48) and the drag-drop block (+0x60).
//
// ---------------------------------------------------------------------------
// THE ARMING PREDICATE, AND WHY A ROUND THAT IS NOT ARMED IS COUNTED
// ---------------------------------------------------------------------------
// `CloseDialog` recurses into itself twice — once per child dialog
// (0x180120f6b) and once for the tooltip (0x180120fa6). The child loop's
// TERMINATION CONDITION is the very count the recursive call decrements:
//
//     while (*(int*)(dlg + 0x48) > 0) { ... CloseDialog(child) ... }
//
// In predict mode a suppressed call decrements nothing, so that loop would spin
// forever. So the round is ARMED only on calls where neither recursion is
// entered — `dlg` has no children and the tooltip arm's condition is false.
// Everything else is handed straight to the DLL and COUNTED.
//
// The count is printed at exit as `indeterminate=N`. It has to be, or the
// coverage claim is a lie by omission: a green board over the armable subset,
// reported without the size of the unarmed subset, is exactly the
// partial-oracle-reading-as-full failure this project keeps hitting. `rounds=`
// alone cannot express it — an unarmed call never constructs a round.
//
// ---------------------------------------------------------------------------
// FACTS THE DECOMPILED C DROPS OR GETS WRONG
// ---------------------------------------------------------------------------
// Read off 0x180120f20..0x1801213b6, not off Ghidra's C
// (reference_ghidra_c_output_vs_disassembly.md):
//
//  1. **A dead compare with a LIVE load**, 0x180120f8a:
//         MOV RAX,[RCX+0x30] ; CMP RAX,RAX ; JNZ
//     `CMP RAX,RAX` is never unequal, so the branch is unreachable — but the
//     LOAD still happens and still faults on a bad `_toolTipDialog`. The same
//     shape UI1 found in `Update`. Reproduced as a `volatile` read; dropping it
//     would make our body survive a pointer retail dies on.
//
//  2. **The `_dialogs` path pushes the NODE, not the InternalPtr**, 0x18012115f:
//     `MOV R8,RBX` where RBX is the list node. `*node` is the dialog, so the
//     eight bytes copied are the same either way — but the pointer handed to
//     the helper is not the caller's `InternalPtr`, and the `_addDialogs` path
//     three hundred bytes later DOES hand it the caller's (`MOV R8,R9`,
//     0x18012139f). Ghidra prints both as `local_res10`.
//
//  3. **A dead store inside the compaction**, 0x18012134b:
//         MOV [RCX],RBP        ; RBP is 0
//         MOV RAX,[RCX+8] ; MOV [RCX],RAX
//     The slot is zeroed and then immediately overwritten with its successor.
//     Both stores are reproduced; the predictor supersedes the first on range
//     overlap (UI1's `RangesOverlap` fix) and compares only the second.
//
//  4. **`TEST RCX,RCX` on `base + i*8`**, 0x180121346 — a null test on an
//     address that can only be null if the array base is exactly `-i*8`. Dead,
//     and kept.
//
//  5. **The tail is `Play(1)` inlined.** 0x180121167 reads `this+0xa0`, which
//     is `_sounds[1]` (the array starts at +0x98), then reproduces
//     `?Play@UserInterface@UI@@QEAAXH@Z` (0x1801223d0) instruction for
//     instruction — including `0.0 / _secondsPerTick`, which is NOT folded to
//     zero here for the reason userinterface.cpp gives at the real `Play`: were
//     that slot ever 0.0, retail computes NaN and `CVTTSD2SI` yields INT64_MIN.
//     Reached ONLY from the `_dialogs` path; the `_addDialogs` path returns
//     without it (0x1801213a7).
//
//  6. **`EDI` is 1 at the sound tail**, set at 0x18012111a, and it is what is
//     stored into the channel's `+0x08`. The `_addDialogs` path reuses EDI as
//     the array index and never reaches the tail, so there is no conflict.
//
//  7. **The vtable slot is `+0x28` = index 5 = `?OnClosed@DialogInstance@UI@@`**
//     (0x180115330), read out of `standalone/rt_data_mint_tables.inc`'s minted
//     copy of `??_7DialogInstance@UI@@6B@` rather than guessed from the name.
//     It is the mirror of `AddDialog`'s slot 4 `OnOpened`. It is NOT a refcount
//     release, so the dialog is still alive when this body reads `d+0x28`
//     afterwards.
//
//  8. **The two unlink blocks are the SAME code twice**, at 0x180121074 and
//     0x180121270, differing only in which register holds the dialog. Written
//     once here as `UnlinkFromParent`. Ghidra's C rotated the first one into a
//     `while` with the loop head below the return, which is why it reads as two
//     unrelated fragments.
//
// ---------------------------------------------------------------------------
// FIELD NAMES NEAR THIS CLASS
// ---------------------------------------------------------------------------
// `runtime/types/ui/Dialog.h` names `DialogControllerConfigEntry`'s two
// `ExternalPtr` fields BACKWARDS and its own warning comment carries the error.
// Nothing in this file touches that struct, and every offset below is written
// as a raw byte offset with the retail address that proves it, deliberately —
// pin to addresses, never to field names, anywhere near this class.
//
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../funcs/rt_sink.h"
#include "../../types/ui/UserInterface.h"
#include "../../types/ui/DialogInstance.h"
// ENDGAME B.3.6 block E: element_common.h carries the file-scope `Module` /
// `ResolveAddr` declaration this file used to spell (the address door lives in
// standalone/, and pulling its header into a runtime/src file would invert the
// dependency the whole tree is arranged around), and the RULE B6-5 atomic
// door caches `rtuielem::AtomicExport` / `AtomicAddr`.
#include "element_common.h"
#include "../../types/system/Timer.h"   // ENDGAME B.3.9 SS1: Timer::_secondsPerTick

#include <atomic>

extern "C" void RtUi_UserInterface_CloseDialog(UI::UserInterface* self, void** dlgPtr);

namespace {

using UI::UserInterface;

// RULE B6-5: namespace-scope atomic slots (the member body runs on UI objects
// the loader thread constructs), not RT_DATA_SLOT / RT_EXPORT_ONCE statics.
std::atomic<void*> g_memFreeSlot{nullptr};
std::atomic<void*> g_eventsSetSizeSlot{nullptr};
std::atomic<void*> g_dynArrayPushSlot{nullptr};

// ENDGAME B.3.9 SS1 (B39_RULES D7): the class static now, no lazy cache.
inline const double* TimerSecondsPerTick() {
    return &::System::Timer::_secondsPerTick;
}

// ---- the callees ------------------------------------------------------------
// Four of the six are EXPORTED, verified against the shipped export directory
// with pefile rather than inferred — `AEAA` means PRIVATE, not unexported
// (reference_aeaa_is_private_not_unexported.md), and `EndDragDrop` is a plain
// `QEAA` name that three earlier lane docs listed among "unsatisfied callees"
// without ever running the export table.
const char* const kEndDragDrop  = "?EndDragDrop@UserInterface@UI@@QEAAXXZ";
const char* const kUpdateEvents = "?UpdateEvents@UserInterface@UI@@QEAAXXZ";
const char* const kMemFree      = "?MemFree@MemoryAllocator@System@@SAXPEAX@Z";
const char* const kCloseDialog  =
    "?CloseDialog@UserInterface@UI@@QEAAXV?$InternalPtr@VDialogInstance@UI@@@Object@@@Z";

// THE TWO ADDRESS DOORS, DECLARED RATHER THAN HIDDEN.
//
// Neither is in the export table (checked with pefile). A by-address call is a
// CUTOVER OBLIGATION, not a free choice
// (reference_by_address_door_dies_at_cutover.md), so both are recorded in
// docs/runtime_port/WAVE10_LANE_CLOSEDLG.md rather than left for a residue run
// to rediscover. NEITHER IS NEW DEBT: 0x180033690 is already lane UI1's
// declared door (userinterface_frame.cpp) and 0x18007a0d0 is already
// `AddDialog`'s (userinterface.cpp). Both reach the DLL through
// `ResolveAddr(Module::Runtime, …)`, which consults the port registry FIRST, so
// the day anyone ports either helper we get theirs
// (reference_resolveaddr_consults_port_registry_first.md).
constexpr uintptr_t kEventsSetSizeAddr = 0x180033690;  // Array<UI::Event>::SetSize
constexpr uintptr_t kDynArrayPushAddr  = 0x18007a0d0;  // the generic 8-byte push

using CloseFn   = void  (*)(void*, void*);
using VoidFn    = void  (*)(void*);
using FreeFn    = void  (*)(void*);
using SetSizeFn = void  (*)(void*, int32_t, const void*);
using PushFn    = void  (*)(void*, int32_t, const void*);

// A list node: value, prev, next. The same 0x18-byte shape lane UI1 proved from
// the drain's own stores, and the same one both unlink blocks here walk.
struct Node { void* value; Node* prev; Node* next; };
static_assert(sizeof(Node) == 0x18, "LinkedList node is value/prev/next");

// One queued UI event, 0x30 bytes. Built on the stack at RSP+0x28 by the
// drag-drop cancel push (0x180121011..0x180121034).
//
// `pad24` is NEVER written by retail — the record is a raw stack frame there,
// so those four bytes are whatever was in the frame. Zeroing them here is
// UNOBSERVABLE and is stated rather than assumed: lane UI1 read the grow helper
// at 0x180033690 and it copies +0x00, +0x08, +0x10, +0x14, +0x18, +0x1c and
// +0x20/+0x28 field by field, never +0x24.
struct EventRecord {
    void*    dialog;    // +0x00 = _dragDropDialog
    void*    z08;       // +0x08 = 0
    int32_t  id;        // +0x10 = 0xd
    int32_t  m1;        // +0x14 = -1
    void*    z18;       // +0x18 = 0
    uint32_t dragId;    // +0x20 = _dragDropId   (this+0x70)
    uint32_t pad24;     // +0x24 never written by retail, never copied
    void*    dragData;  // +0x28 = _dragDropData (this+0x78)
};
static_assert(sizeof(EventRecord) == 0x30, "UI::Event stride");

// THE INVOCATION COUNTER — the evidence that the ported body RAN AT ALL.
//
// It is separate from every oracle counter on purpose: none of those could
// distinguish "the port ran and the oracle was quiet" from "the port was never
// reached and something upstream still holds the DLL's pointer".
// `fn_UI_CloseDialog` is resolved ONCE, in
// `engine/funcs/engine_funcs.cpp:1606`, through `ResolveExport` — and whether
// that lands on the port or on the DLL depends entirely on whether `RtPortInit`
// has already run at that moment. No oracle counter can express that question
// (reference_diagnostic_field_cannot_express_the_answer.md,
// reference_uncalled_port_is_not_verified.md), and lane UI3 needed a separate
// `ProbeGate()` for exactly the same reason.
//
// So this counts every entry into the wrapper, in EVERY configuration, and is
// printed unconditionally. A zero here means the row is registered and DEAD,
// whatever else the board says.
uint64_t g_entries = 0;

struct CloseDlgSummary {
    ~CloseDlgSummary() {
        printf("[RtPort] UI::UserInterface::CloseDialog  entries=%llu\n",
               (unsigned long long)g_entries);
        std::fflush(stdout);
    }
};
CloseDlgSummary g_closeDlgSummary;

// ---------------------------------------------------------------------------
// The shared unlink — 0x180121074..0x180121109 and 0x180121270..0x180121307
// ---------------------------------------------------------------------------
// Removes `dlg` from its parent's child list and frees the node. Both call
// sites in retail are the same instructions with a different register holding
// the dialog, so they are one function here.
//
void UnlinkFromParent(uint8_t* dlg, RtSink& out) {
    uint8_t* par = *reinterpret_cast<uint8_t**>(dlg + 0x28);
    if (!par) return;                                   // 0x18012105e / 0x180121255

    Node* head = *reinterpret_cast<Node**>(par + 0x38);
    if (!head) return;                                  // 0x18012106e / 0x180121265

    for (Node* n = head; n; n = n->next) {              // +0x10 is `next`
        if (n->value != reinterpret_cast<void*>(dlg)) continue;

        if (n == head)                                              // 0x180121088
            out.Ptr(par + 0x38, n->next, "parent list head = node->next");
        if (n == *reinterpret_cast<Node**>(par + 0x40))             // 0x180121095
            out.Ptr(par + 0x40, n->prev, "parent list tail = node->prev");
        if (n->prev)                                                // 0x1801210a3
            out.Ptr(&n->prev->next, n->next, "node->prev->next = node->next");
        if (n->next)                                                // 0x1801210b4
            out.Ptr(&n->next->prev, n->prev, "node->next->prev = node->prev");

        out.Ptr(&n->prev, nullptr, "node->prev = 0");               // 0x1801210c7
        out.Ptr(&n->next, nullptr, "node->next = 0");               // 0x1801210cb
        out.U32(par + 0x48, out.ReadU32(par + 0x48) - 1,            // 0x1801210cf
                "parent list count--");

        // Retail inlines EnterCriticalSection / MemoryAllocator::Free /
        // LeaveCriticalSection here (0x1801210d2..0x180121108). That sequence is
        // byte-for-byte the body of the exported static `MemFree`, which is why
        // calling it reproduces the lock discipline exactly with no windows.h in
        // a runtime/src file. Lane UI1 §1b decompiled and compared the two.
        FreeFn mfree = reinterpret_cast<FreeFn>(rtuielem::AtomicExport(g_memFreeSlot, kMemFree));
        if (out.Call("MemFree(unlinked node)", (const void*)mfree,
                     reinterpret_cast<int64_t>(n)))
            mfree(n);
        return;
    }
}

// ---------------------------------------------------------------------------
// The shared "append to _removedDialogs unless it is already there"
// 0x180121115..0x180121165 and 0x180121361..0x1801213a5
// ---------------------------------------------------------------------------
// Note the loop: retail does NOT break on a hit. It walks the whole array
// setting a flag with `CMOVZ`, then tests once. A port that broke early would
// be faster and would read the same, which is precisely why it is written the
// long way here.
//
// `src` differs between the two call sites and is not an accident — see fact 2
// in the banner.
void PushRemovedUnlessPresent(UserInterface* self, void** dlgPtr, const void* src,
                              RtSink& out) {
    uint8_t* const S = reinterpret_cast<uint8_t*>(self);

    int32_t n = *reinterpret_cast<int32_t*>(S + 0x20);
    if (n > 0) {
        void** p     = *reinterpret_cast<void***>(S + 0x18);
        void*  want  = *dlgPtr;
        bool   found = false;
        for (int64_t c = n; c != 0; --c, ++p)
            if (*p == want) found = true;                   // CMOVZ, no break
        if (found) return;
    }

    PushFn push = reinterpret_cast<PushFn>(rtuielem::AtomicAddr(g_dynArrayPushSlot, kDynArrayPushAddr));
    if (out.Call("DynArrayPush(_removedDialogs)", (const void*)push,
                 reinterpret_cast<int64_t>(S + 0x18),
                 *reinterpret_cast<int32_t*>(S + 0x20) + 1,
                 reinterpret_cast<int64_t>(src)))
        push(S + 0x18, *reinterpret_cast<int32_t*>(S + 0x20) + 1, src);
}

// ---------------------------------------------------------------------------
// The body, written against the live sink (rt_sink.h).
// ---------------------------------------------------------------------------
} // namespace

// ENDGAME B.3.6 block E: the body is the member. `dialog` is the ADDRESS of the
// caller's InternalPtr temporary (RULE B6-11; 0x180120f3d `MOV RDI,RDX` then
// `MOV RBX,[RDX]`), spelled `dlgPtr` below as the port always spelled it.
void UI::UserInterface::CloseDialog(Object::InternalPtr<DialogInstance>* dialog) {
    RtSink out;                                                 // the live sink
    UserInterface* const self = this;
    void** const dlgPtr = reinterpret_cast<void**>(dialog);
    uint8_t* const S   = reinterpret_cast<uint8_t*>(this);
    uint8_t* const dlg = *reinterpret_cast<uint8_t**>(dlgPtr);   // RBX at 0x180120f43

    // ---- Block A, 0x180120f46..0x180120f74 -------------------------------
    // Close every child dialog first. The loop's bound is the count the
    // recursive call decrements — see the banner on why a predicting round is
    // never armed when this is entered. `dlg+0x10` is the child's owning
    // UserInterface, which is not necessarily `self`.
    while (*reinterpret_cast<int32_t*>(dlg + 0x48) > 0) {
        void*  head  = *reinterpret_cast<void**>(dlg + 0x38);
        void*  child = *reinterpret_cast<void**>(head);
        Object::InternalPtr<DialogInstance> tmp(                  // the by-value InternalPtr
            static_cast<DialogInstance*>(child));
        (*reinterpret_cast<UserInterface**>(dlg + 0x10))->CloseDialog(&tmp);
    }

    // ---- Block B, 0x180120f76..0x180120fb1 -------------------------------
    // If the dialog being closed OWNS the current tooltip, close the tooltip.
    void* tip = *reinterpret_cast<void**>(S + 0x40);
    if (tip && *dlgPtr == *reinterpret_cast<void**>(S + 0x38)) {
        // 0x180120f8a  MOV RAX,[RCX+0x30] / CMP RAX,RAX / JNZ — see fact 1.
        // The compare is unreachable; the LOAD is not, and it faults on a bad
        // `_toolTipDialog` exactly as retail does.
        volatile uint64_t deadLoad = *reinterpret_cast<uint64_t*>(
            static_cast<uint8_t*>(tip) + 0x30);
        (void)deadLoad;

        Object::InternalPtr<DialogInstance> tmp(static_cast<DialogInstance*>(tip));
        CloseDialog(&tmp);                                        // 0x180120fa6
        out.Ptr(S + 0x40, nullptr, "_toolTipDialog = 0 (tooltip arm)");
        out.Ptr(S + 0x38, nullptr, "_toolTipOwner = 0 (tooltip arm)");
    }

    // ---- Block C, 0x180120fb3..0x180120fd4 -------------------------------
    // Already queued for removal? Then there is nothing left to do.
    {
        int32_t n = *reinterpret_cast<int32_t*>(S + 0x20);
        if (n > 0) {
            void** p    = *reinterpret_cast<void***>(S + 0x18);
            void*  want = *dlgPtr;
            for (int32_t i = 0; i < n; ++i, ++p)
                if (*p == want) return;                           // 0x180120fc6
        }
    }

    // ---- Block D, 0x180120fd6..0x180120ff1 -------------------------------
    // Find the dialog in the live `_dialogs` list (an intrusive list at +0x00).
    Node* node = *reinterpret_cast<Node**>(S);
    while (node && node->value != *dlgPtr) node = node->next;

    if (node) {
        // ---- Block E, 0x180120ff8..0x180121051 ---------------------------
        // DialogInstance vtable slot 5 (+0x28) `OnClosed` — VIRTUAL, so an exe
        // dialog's hand-built slot 5 is what runs.
        void*  d    = node->value;
        reinterpret_cast<DialogInstance*>(d)->OnClosed();         // B.3.6 bridge

        // If this dialog owns the in-flight drag, queue a cancel event and end it.
        void* drag = *reinterpret_cast<void**>(S + 0x60);
        if (node->value == drag) {
            EventRecord ev;
            ev.dialog   = drag;                                            // 0x180121011
            ev.z08      = nullptr;                                         // 0x180121016
            ev.id       = 0xd;                                             // 0x18012101b
            ev.m1       = -1;                                              // 0x180121023
            ev.z18      = nullptr;                                         // 0x18012102b
            ev.dragId   = *reinterpret_cast<uint32_t*>(S + 0x70);          // 0x180121030
            ev.pad24    = 0;   // never written by retail, never copied
            ev.dragData = *reinterpret_cast<void**>(S + 0x78);             // 0x180121034

            SetSizeFn setSize = reinterpret_cast<SetSizeFn>(
                rtuielem::AtomicAddr(g_eventsSetSizeSlot, kEventsSetSizeAddr));
            if (out.Call("Array<UI::Event>::SetSize (drag cancel, id 0xd)",
                         (const void*)setSize,
                         reinterpret_cast<int64_t>(S + 0x48),
                         *reinterpret_cast<int32_t*>(S + 0x50) + 1))
                setSize(S + 0x48, *reinterpret_cast<int32_t*>(S + 0x50) + 1, &ev);

            EndDragDrop();
        }

        // ---- Block F, 0x180121054..0x18012110b -----------------------------
        // Retail RE-LOADS the dialog out of the node here (0x180121054), after
        // OnClosed has run. Reproduced rather than reused from above.
        UnlinkFromParent(*reinterpret_cast<uint8_t**>(node), out);

        // ---- 0x18012110d..0x180121165 --------------------------------------
        UpdateEvents();

        // FACT 2: the source is the NODE, not the caller's InternalPtr.
        PushRemovedUnlessPresent(self, dlgPtr, node, out);

        // ---- Block H, 0x180121167..0x1801211f1 -----------------------------
        // `Play(1)` inlined: `this+0xa0` is `_sounds[1]` (the array is at +0x98).
        void* sound = *reinterpret_cast<void**>(S + 0xa0);
        if (!sound) return;
        uint8_t* scene = *reinterpret_cast<uint8_t**>(S + 0x90);
        if (!scene) return;
        uint8_t* table = *reinterpret_cast<uint8_t**>(scene + 0x10);

        // NOT folded to zero — see fact 5 and the same note at the real `Play`.
        const double  secondsPerTick = *TimerSecondsPerTick();
        const int64_t ticks = static_cast<int64_t>(0.0 / secondsPerTick);

        int32_t index = 0;
        for (int64_t guard = 0;
             *reinterpret_cast<const int32_t*>(table + static_cast<int64_t>(index) * 0x70 + 0x68) != 0;
             ) {
            ++index;
            if (++guard >= 0x100) return;        // full table: retail drops the sound
        }
        if (index < 0) return;                   // 0x1801211be, dead: index starts at 0

        uint8_t* slot = table + static_cast<int64_t>(index) * 0x70;

        out.Ptr(slot + 0x00, sound, "audio slot: the sound");             // 0x1801211d0
        out.U32(slot + 0x08, 1,     "audio slot: state = 1");             // 0x1801211d3
        out.U64(slot + 0x10, static_cast<uint64_t>(ticks),                // 0x1801211d6
                "audio slot: start tick");
        const uint32_t counter = out.ReadU32(scene + 0x08) + 1;           // 0x1801211da
        out.U32(scene + 0x08, counter, "audio scene counter++");
        out.U32(slot + 0x68,                                              // 0x1801211ee
                static_cast<uint32_t>(index * 2) | (counter << 9),
                "audio slot: handle = index*2 | counter<<9");
        return;
    }

    // ---- Block G, 0x1801211f6..0x180121227 -------------------------------
    // Not live yet — look for it in `_addDialogs`, the queue AddDialog pushes to.
    int32_t count = *reinterpret_cast<int32_t*>(S + 0x30);
    if (count <= 0) return;

    uint8_t* base   = *reinterpret_cast<uint8_t**>(S + 0x28);
    void*    target = *dlgPtr;
    int32_t  i      = 0;
    {
        void** p = reinterpret_cast<void**>(base);
        while (*p != target) {
            ++i; ++p;
            if (i >= count) return;                                    // 0x180121220
        }
    }

    // ---- Block J, 0x180121229..0x1801213a5 -------------------------------
    int64_t off = static_cast<int64_t>(i) * 8;                         // RBX, MOVSXD

    {
        // DialogInstance vtable slot 5 (+0x28) `OnClosed` — VIRTUAL.
        void*  d    = *reinterpret_cast<void**>(base + off);
        reinterpret_cast<DialogInstance*>(d)->OnClosed();         // B.3.6 bridge
    }

    // The array base is RE-LOADED after the virtual (0x180121246): OnClosed can
    // reallocate the queue. Reproduced.
    uint8_t* base2 = *reinterpret_cast<uint8_t**>(S + 0x28);
    UnlinkFromParent(*reinterpret_cast<uint8_t**>(base2 + off), out);

    UpdateEvents();                                                    // 0x180121314

    // 0x180121319 — note this compares against +0x40 and clears BOTH, where
    // block B compared against +0x38. Not a typo in either place.
    if (*dlgPtr == *reinterpret_cast<void**>(S + 0x40)) {
        out.Ptr(S + 0x40, nullptr, "_toolTipDialog = 0");
        out.Ptr(S + 0x38, nullptr, "_toolTipOwner = 0");
    }

    // ---- the compaction, 0x180121332..0x18012135c ------------------------
    if (i < *reinterpret_cast<int32_t*>(S + 0x30)) {
        do {
            ++i;
            if (i < *reinterpret_cast<int32_t*>(S + 0x30)) {
                uint8_t* b = *reinterpret_cast<uint8_t**>(S + 0x28);
                uint8_t* c = b + off;
                if (c != nullptr) {                                    // fact 4, dead, kept
                    // fact 3: the dead zero-store, then the real shift.
                    out.Ptr(c, nullptr, "_addDialogs[k] = 0 (dead store)");
                    out.Ptr(c, out.ReadPtr(c + 8), "_addDialogs[k] = _addDialogs[k+1]");
                }
            }
            off += 8;
        } while (i < *reinterpret_cast<int32_t*>(S + 0x30));
    }
    out.U32(S + 0x30, out.ReadU32(S + 0x30) - 1, "_addDialogs._size--");  // 0x18012135e

    // FACT 2 again, the other way: here the source IS the caller's InternalPtr.
    PushRemovedUnlessPresent(self, dlgPtr, dlgPtr, out);
    // and the sound tail is NOT reached on this path — 0x1801213a2 falls into
    // the epilogue at 0x1801213a7.
}

extern "C" void RtUi_UserInterface_CloseDialog(UserInterface* self, void** dlgPtr) {
    // THE PROBE GATE. `g_entries` is printed by a static destructor, so it only
    // reaches the log when the process EXITS CLEANLY — and the only unattended
    // route that reaches this body in-game (the pause menu's Resume button)
    // leaves the game running afterwards and has to be killed. A killed run
    // would then report nothing at all, which reads exactly like entries=0.
    // Lane UI3 needed the same thing for the same reason. One line, on the
    // first entry only, to stderr, which survives an abnormal exit.
    if (g_entries++ == 0) {
        std::fprintf(stderr,
            "[RtPort] UI::UserInterface::CloseDialog ENTERED (first call) -- the "
            "port is LIVE on this route\n");
        std::fflush(stderr);
    }

    self->UI::UserInterface::CloseDialog(
        reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(dlgPtr));
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------
int RtUiCloseDialogRegister() {
    int count = 0;

    // Registered under BOTH the mangled name and the address. WAVE7.md §3's
    // rule: a port registered under the name only can be dead at a call site
    // that reaches the DLL through the address fallback, while reporting a
    // clean-looking oracle over nothing.
    RegisterRuntimeExport(kCloseDialog, (void*)&RtUi_UserInterface_CloseDialog); ++count;
    RegisterRuntimeAddr(0x180120f20, (void*)&RtUi_UserInterface_CloseDialog);

    return count;
}
