// ============================================================================
// runtime/src/ui/userinterface_frame.cpp — Wave 10 lane UI1
// ?Update@UserInterface@UI@@QEAAHAEBVTime@System@@@Z   retail 0x1801213c0
// ============================================================================
//
// WHY THIS FILE EXISTS AND WHY IT IS NOT `userinterface.cpp`
//
// `userinterface.cpp` is Wave 7 lane C's file and every body in it is a LEAF
// verified by the REPLAY oracle (snapshot 0x128 bytes, run ours live, run
// theirs on the copy, memcmp). Lane C explicitly refused `Update` on that
// instrument, and was right to:
//
//     "`Update` drains `_addDialogs` into the list (allocating a node each),
//      frees the array, advances the fade, calls `DialogInstance::Update` on
//      every dialog, consumes input from `Input::Device::_singleton`, and ends
//      drag-drop; it is not replayable and not even idempotent."
//
// Every clause of that is true and none of it has expired. A replay of `Update`
// would drain the SAME `_addDialogs._data` pointer twice, MemFree it twice, and
// pump a second round of `DialogInstance::Update` into live dialogs.
//
// WHAT DID EXPIRE — the three blockers this lane re-tested from scratch:
//
//  1. "SPLIT BRAIN, needs a detour."  EXPIRED. `standalone/runtime_detour.{h,cpp}`
//     is a mature relocating installer with live `[port]` detours, and
//     `standalone/ui_detour.cpp` is already a UI subject on it
//     (`TabElement::UpdateContainers`, 0x1800d9710). Building the mechanism is
//     not this lane's work; using it is. See `RtUiUserInterfaceUpdateInstallDetour`.
//
//  2. "`EnterCriticalSection` cannot appear in a non-`_win` file, and the
//     allocator is Wave 2's NO-GO."  EXPIRED, and it was a CONFLATION.
//     Retail's inline `EnterCriticalSection(alloc+0x640); Allocate(...);
//     LeaveCriticalSection(...)` sequence at 0x180121429..0x18012144b is
//     BYTE-FOR-BYTE the body of the exported static
//     `?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z` (0x1800c7a50) — decompiled
//     and compared, not assumed. Calling that export reproduces the lock
//     discipline exactly with no `windows.h` in this file. The Wave 2 NO-GO is a
//     verdict on REPLACING the allocator arena (`standalone/rt_alloc_swap.cpp`),
//     not on a ported body calling it; ~30 ported files already call `MemAlloc`.
//
//  3. "No oracle."  The body is written against the live sink
//     (`runtime/funcs/rt_sink.h`); every store below goes through it.
//
// ---------------------------------------------------------------------------
// FACTS THE DECOMPILED C DROPPED (read off the disassembly, per
// reference_ghidra_c_output_vs_disassembly.md)
// ---------------------------------------------------------------------------
//
//  * **`Update` RETURNS THE MOUSE CURSOR INDEX.** Ghidra prints the prototype as
//    `void`. The disassembly does not: the last call on the main path is
//    `CALL 0x18011fa40` (`GetMouseCursorIndex`) at 0x18012185e and the epilogue
//    is reached by a bare `JMP` that never touches EAX, so its return value IS
//    `Update`'s. The fullscreen-fade early-out at 0x180121865 is
//    `XOR EAX,EAX` — **return 0**. `docs/runtime_port/export_name_verdicts.md`
//    already corrected the declaration from `X` (void) to `H` (int); this is
//    what the int MEANS, and it is load-bearing because the caller feeds it
//    straight to the cursor.
//
//  * **`CloseDialog(this)` is `CloseDialog(this, &tmp)`.** Ghidra dropped the
//    second argument entirely. 0x1801214b2 stores `_toolTipDialog` into a stack
//    slot and 0x1801214b6 passes its ADDRESS — the by-value `InternalPtr` ABI
//    (`WAVE7.md`: a non-trivial copy ctor means MSVC x64 passes by address in
//    both spellings).
//
//  * **A dead compare with a LIVE load.** 0x1801214a9 is
//    `MOV RAX,[RDX+0x30]; CMP RAX,RAX; JNZ` — `CMP RAX,RAX` is never unequal, so
//    the branch is unreachable, but the LOAD still happens and still faults on a
//    bad `_toolTipDialog`. Ghidra printed the whole thing as `(lVar != lVar)` and
//    then folded it away. Reproduced below as a `volatile` read: dropping it
//    would make our body survive a pointer retail dies on, which is a behaviour
//    change in the direction this project explicitly refuses.
//
//  * **`node->prev = tail` is UNGUARDED.** 0x18012147e writes `[RDI+8]` where RDI
//    is the freshly allocated node, on the path where the allocation may have
//    returned null (0x180121456 sets RDI = 0 and jumps INTO the same tail
//    fixup). Retail dereferences null here if `MemAlloc` ever fails. Kept
//    faithful — CLAUDE.md, "Don't 'fix' a faithful crash with a null check".
//
//  * **The fade's zero-out is one QWORD, not two DWORDs.** 0x1801215d7 is
//    `MOV qword[RSI+0xf0], 0`, which clears `_fadeVelocity` AND `_fadeAmount`
//    together. The `>= 1.0f` arm instead writes two separate DWORDs
//    (`_fadeAmount = 1.0f`, `_fadeVelocity = 0`). Different widths on the two
//    arms; a typed-field port would silently unify them.
//
//  * **`Update`'s float constant is 4.0f.** `DAT_1801616e4` read out of the
//    shipped `.rdata`: `00 00 80 40`. The fade step is
//    `amount += (float)(ticks * _secondsPerTick) * velocity * 4.0f`.
//
// ---------------------------------------------------------------------------
// THE ORACLE'S COVERAGE BOUNDARY — stated before the code, not after
// ---------------------------------------------------------------------------
// In predict mode our body makes NO call (every one goes through
// `RtSink::Call`, which returns false). So anything a suppressed callee writes
// is not ours to predict, and anything whose VALUE depends on a suppressed
// callee is not ours to predict either. Both are `Exclude()`d:
//
//   PREDICTED and COMPARED (the whole point):
//     +0x028/+0x030  `_addDialogs` cleared after the drain      (changes on any
//                    frame a dialog was added — a real `changed=` contributor)
//     +0x0fc..+0x10c `_clipRegion`  (fully determined by Video::Device + scale)
//     +0x0f0/+0x0f4  the fade advance (fully determined by time + pre-state —
//                    the strongest `changed=` contributor: it moves EVERY frame
//                    of every fade)
//     +0x058/+0x05c  `_mouseDown{X,Y}` (fully determined by Input::Device)
//     +0x0d9         the entry `_mouseCaptured = 0` store (an INTERMEDIATE; the
//                    exit store is excluded, and the instrument counts this as
//                    `superseded`, never as a claim about the post-state)
//
//   EXCLUDED, with the specific reason each:
//     +0x000..+0x018 `_dialogs`      — `CleanRemovedDialogs` (x3) unlinks here,
//                                      and the drain's node POINTERS are
//                                      addresses `MemAlloc` chose, which no
//                                      predictor can name.
//     +0x018..+0x028 `_removedDialogs` — cleared by `CleanRemovedDialogs`.
//     +0x038/+0x040  the tooltip pair  — written by the suppressed `CloseDialog`
//                                      arm.
//     +0x048..+0x058 `_events`         — `UpdateEvents`, `CleanRemovedDialogs`
//                                      and the two suppressed array-grow pushes.
//     +0x060..+0x089 the drag-drop block — the branch that clears it is gated on
//                                      a byte `DialogInstance::Update` writes
//                                      into our stack-local `UpdateState`. With
//                                      that call suppressed the branch condition
//                                      is UNKNOWABLE, so the body ABSTAINS
//                                      rather than guessing — excluded only when
//                                      the dialog list is non-empty, and fully
//                                      predicted when it is empty and the
//                                      condition is therefore determined.
//     +0x0d9         the EXIT store    — same cause: it is `UpdateState+0x20`.
//
// The return value is NOT compared in predict mode and must not be: our body
// runs on the PRE-drain list and retail's runs on the POST-drain list, so a
// difference would be correct rather than a defect. The detour returns RETAIL's
// value in predict mode, which is also what keeps the frame correct.
//
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../funcs/rt_sink.h"
#include "../../types/ui/UserInterface.h"
#include "../../types/ui/DialogInstance.h"
#include "../../types/ui/Element.h"
// ENDGAME B.3.6 block E: element_common.h carries the same file-scope
// `Module` / `ResolveAddr` declaration userinterface.cpp and dialog.cpp use
// (the address door lives in standalone/), and the RULE B6-5 atomic door
// caches.
#include "element_common.h"
#include "../../types/video/Device.h"    // ENDGAME B.3.9 SS1: Video::Device::_singleton
#include "../../types/input/Device.h"    // ENDGAME B.3.9 SS1: Input::Device::_singleton
#include "../../types/input/Input.h"     // ENDGAME B.3.9 SS1: Input::Configuration::_singleton
#include "../../types/system/Timer.h"    // ENDGAME B.3.9 SS1: Timer::_secondsPerTick
#include "../../types/collection/Collection.h"
#include "../../types/system/MemoryAllocator.h"


namespace {

using UI::UserInterface;

inline uint8_t* B(void* p, size_t off) { return static_cast<uint8_t*>(p) + off; }

// ---- data slots -------------------------------------------------------------
// ENDGAME B.3.9 SS1 (B39_RULES D7): all four are class statics now, no lazy
// AtomicExport cache (B9-10).
inline const double* TimerSecondsPerTick() {
    return &::System::Timer::_secondsPerTick;
}
inline void** VideoDeviceSingleton() {
    return reinterpret_cast<void**>(&::Video::Device::_singleton);
}
inline void** InputDeviceSingleton() {
    return reinterpret_cast<void**>(&::Input::Device::_singleton);
}
inline void** InputConfigSingleton() {
    return reinterpret_cast<void**>(&::Input::Configuration::_singleton);
}

// ---- the callees ------------------------------------------------------------
// B3R RT-UI: all called by name -- MemoryAllocator::MemAlloc / MemFree
// (0x1800c7a50 / 0x1800c7ad0) and 0x180033690,
// `Collection::Array<UI::Event>::SetSize(int, const Event*)`, the grow-and-fill
// helper both event pushes call (not exported:
// rtcoll::RtColl_ArraySetSizeFill_180033690).

// ---------------------------------------------------------------------------
// UI::Element::UpdateState — 0x30 bytes, recovered from THIS function's frame
// ---------------------------------------------------------------------------
// Not a guess: every field below is a store or a load in 0x180121635..0x18012185e,
// and the size is pinned by the span from the base (RBP-0x51) to the last field
// (RBP-0x29, 8 bytes). Only the fields Update touches are named; the rest is
// left as explicit padding rather than invented, per PLAN §4.3 risk 1.
//
// `DialogInstance::Update` takes this by reference and WRITES BACK into it —
// at least `+0x07` (the mouse-down byte reloaded at 0x180121781) and `+0x20`
// (stored into `_mouseCaptured` at 0x18012184e). That write-back is exactly why
// predict mode cannot know the drag-drop branch.
struct UpdateState {
    uint8_t  dragActive;      // +0x00  = (_dragDropDialog != 0)
    uint8_t  buttonPressed;   // +0x01  Input +0x10fe
    uint8_t  b02;             // +0x02  Input +0x112e
    uint8_t  b03;             // +0x03  Input +0x1116
    uint8_t  b04;             // +0x04  Input +0x10ff
    uint8_t  b05;             // +0x05  Input +0x112f
    uint8_t  b06;             // +0x06  Input +0x1117
    uint8_t  leftDown;        // +0x07  (Input word +0x10fc > 0); WRITTEN BACK
    uint8_t  b08;             // +0x08  (Input word +0x1114 > 0)
    uint8_t  b09;             // +0x09  (Input word +0x112c > 0)
    uint8_t  pad0a[2];        // +0x0a  never written
    int32_t  dragDx;          // +0x0c
    int32_t  dragDy;          // +0x10
    int32_t  wheel;           // +0x14  Input word +0x11bc
    int32_t  mouseX;          // +0x18  init -1
    int32_t  mouseY;          // +0x1c  init -1
    uint8_t  mouseCaptured;   // +0x20  WRITTEN BACK; -> _mouseCaptured
    uint8_t  pad21[7];        // +0x21
    void*    config;          // +0x28  = *(void**)Input::Configuration::_singleton
};
static_assert(sizeof(UpdateState) == 0x30, "UpdateState frame span is 0x30");

// One queued UI event, built on the stack by both drag-drop pushes.
struct EventRecord {
    void*    dialog;   // +0x00
    void*    z08;      // +0x08 = 0
    int32_t  id;       // +0x10 = 0xd then 0xe
    int32_t  m1;       // +0x14 = -1
    void*    z18;      // +0x18 = 0
    uint32_t dragId;   // +0x20 = _dragDropId
    uint32_t pad24;    // +0x24 NEVER written by retail, and never copied by the
                       //       grow helper either (it copies +0x00,+0x08,+0x10,
                       //       +0x14,+0x18,+0x1c,+0x20,+0x28 field by field)
    void*    dragData; // +0x28 = _dragDropData
};
static_assert(sizeof(EventRecord) == 0x30, "UI::Event stride");

// A list node: value, prev, next. Proven by the drain's own stores
// (tail->[+0x10] = node, node->[+0x08] = tail) and by every walk in the class.
struct Node { void* value; Node* prev; Node* next; };
static_assert(sizeof(Node) == 0x18, "MemAlloc(0x18, 8) in the drain");

// ============================================================================
// The body, written against the live sink (rt_sink.h).
// ============================================================================
// `out` is a live sink: every write is a store, every `Call` returns true.
//
// Reads of locations THIS BODY WRITES go through `out.ReadU*` so the predictor
// is self-consistent; reads of locations it never writes stay plain loads. The
// distinction matters here in exactly one place — `_fadeAmount`, which is read
// back inside the fade arm after being written.
} // namespace

// ENDGAME B.3.6 block E: the body is the member (the main-loop hot path).
int32_t UI::UserInterface::Update(const System::Time& timeRef) {
    RtSink out;                                                 // the live sink
    const void* const time = &timeRef;                          // one int64 tick count
    uint8_t* const S = reinterpret_cast<uint8_t*>(this);

    // 0x1801213ef — _mouseCaptured = 0, BEFORE anything else.
    out.U8(S + 0xd9, 0, "_mouseCaptured = 0 (entry)");

    // 0x1801213f6
    CleanRemovedDialogs();                                      // #1

    // ---- drain _addDialogs back-to-front into the list ---------------------
    // Retail walks i = _size-1 down to 0 (0x1801213fb: MOVSXD R14, size-1, and
    // JS past the whole loop when negative — a genuine sign-extension, which
    // Ghidra prints as a zero extension).
    const int32_t addCount = *reinterpret_cast<int32_t*>(S + 0x30);
    for (int64_t i = static_cast<int64_t>(addCount) - 1; i >= 0; --i) {
        void** const addData = *reinterpret_cast<void***>(S + 0x28);

        Node* node = nullptr;
        if (out.Call("MemAlloc(0x18, 8) — list node",
                     reinterpret_cast<const void*>(&System::MemoryAllocator::MemAlloc), 0x18, 8)) {
            node = static_cast<Node*>(System::MemoryAllocator::MemAlloc(0x18, 8));
            if (node) {
                node->value = nullptr;          // retail zeroes then stores
                node->value = addData[i];
                node->prev  = nullptr;
                node->next  = nullptr;
            }
            // else: RDI := R13 (0) at 0x180121456 — and the tail fixup below
            // still runs. That unguarded `node->prev = tail` is retail's.
        } else {
            // Predict mode: no allocation, and therefore no honest prediction
            // for the list. The caller has excluded +0x000..+0x018 on exactly
            // this path; there is nothing to record here.
            continue;
        }

        Node* const tail = *reinterpret_cast<Node**>(S + 0x08);
        if (tail) {
            out.Ptr(&tail->next, node, "_dialogs._tail->next = node");
            out.Ptr(&node->prev, *reinterpret_cast<Node**>(S + 0x08),
                    "node->prev = _dialogs._tail");
        } else {
            out.Ptr(S + 0x00, node, "_dialogs._head = node");
        }
        out.Ptr(S + 0x08, node, "_dialogs._tail = node");
        out.U32(S + 0x10, out.ReadU32(S + 0x10) + 1, "_dialogs._count++");

        // ---- the tooltip teardown arm ------------------------------------
        void* const toolTip = *reinterpret_cast<void**>(S + 0x40);
        if (toolTip) {
            void** const reload = *reinterpret_cast<void***>(S + 0x28);
            uint8_t* const dlg  = static_cast<uint8_t*>(reload[i]);
            if ((*reinterpret_cast<uint32_t*>(dlg + 0x20) >> 4) & 1u) {
                // 0x1801214a9: MOV RAX,[RDX+0x30]; CMP RAX,RAX; JNZ ...
                // The compare can never be unequal, so the branch is dead — but
                // the LOAD is real and faults on a bad pointer exactly as retail
                // does. Kept, and kept `volatile` so no compiler deletes it.
                (void)*static_cast<void* volatile*>(
                    static_cast<void*>(static_cast<uint8_t*>(toolTip) + 0x30));

                // the by-value InternalPtr copy, passed by address (B6-11)
                Object::InternalPtr<DialogInstance> tmp(static_cast<DialogInstance*>(toolTip));
                CloseDialog(&tmp);
                out.Ptr(S + 0x40, nullptr, "_toolTipDialog = 0");
                out.Ptr(S + 0x38, nullptr, "_toolTipOwner = 0");
            }
        }
    }

    // 0x1801214d3 — the "a dialog opened" sound, gated on the PRE-drain count.
    if (addCount > 0) {
        Play(0);
    }

    // 0x1801214e3 — free the array and clear BOTH size and capacity with one
    // QWORD store (0x180121525: MOV qword[RSI+0x30], 0).
    void* const addData = *reinterpret_cast<void**>(S + 0x28);
    if (addData) {
        if (out.Call("MemFree(_addDialogs._data)",
                     reinterpret_cast<const void*>(&System::MemoryAllocator::MemFree)))
            System::MemoryAllocator::MemFree(addData);
    }
    out.Ptr(S + 0x28, nullptr, "_addDialogs._data = 0");
    out.U64(S + 0x30, 0, "_addDialogs._size/_capacity = 0 (one qword)");

    // ---- clip region ------------------------------------------------------
    const float scale = *reinterpret_cast<float*>(S + 0xdc);
    uint8_t* const vdev = static_cast<uint8_t*>(*VideoDeviceSingleton());
    const int32_t w = static_cast<int32_t>(
        static_cast<float>(*reinterpret_cast<int32_t*>(vdev + 4)) * scale);
    const int32_t h = static_cast<int32_t>(
        static_cast<float>(*reinterpret_cast<int32_t*>(vdev + 8)) * scale);
    out.U64(S + 0x0fc, 0, "_clipRegion.left/top = 0 (one qword)");
    out.U32(S + 0x104, static_cast<uint32_t>(w - 1), "_clipRegion.right");
    out.U32(S + 0x108, static_cast<uint32_t>(h - 1), "_clipRegion.bottom");

    // ---- fullscreen fade --------------------------------------------------
    const float vel = *reinterpret_cast<float*>(S + 0xf0);
    if (vel != 0.0f) {
        const double secs = static_cast<double>(*reinterpret_cast<const int64_t*>(time))
                          * *TimerSecondsPerTick();
        const float amount =
            static_cast<float>(secs) * vel * 4.0f
            + *reinterpret_cast<float*>(S + 0xf4);
        uint32_t bits;
        __builtin_memcpy(&bits, &amount, 4);
        out.U32(S + 0xf4, bits, "_fadeAmount +=");

        if (!(amount < 1.0f)) {
            out.U32(S + 0xf4, 0x3f800000u, "_fadeAmount = 1.0f");
            out.U32(S + 0xf0, 0, "_fadeVelocity = 0");
        } else if (!(0.0f < amount)) {
            // ONE qword — clears velocity AND amount together (0x1801215d7).
            out.U64(S + 0xf0, 0, "_fadeVelocity/_fadeAmount = 0 (one qword)");
        }
    }

    // 0x1801215de / 0x1801215e9
    UpdateEvents();
    CleanRemovedDialogs();                                      // #2

    // ---- layout every dialog, head -> tail --------------------------------
    // `DialogInstance::ComputeLayout` (0x180113d90) — non-virtual member of the
    // real class (block B).
    for (Node* n = *reinterpret_cast<Node**>(S + 0x00); n; n = n->next) {
        reinterpret_cast<DialogInstance*>(n->value)->ComputeLayout(0, 0, w, h);  // B.3.6 bridge
    }

    // 0x18012161e — a fade in progress swallows the whole input half AND the
    // return value.
    if (*reinterpret_cast<float*>(S + 0xf0) != 0.0f)
        return 0;

    // ---- gather input into UpdateState ------------------------------------
    UpdateState st = {};
    st.mouseX = -1;
    st.mouseY = -1;

    uint8_t* const in = static_cast<uint8_t*>(*InputDeviceSingleton());
    st.dragActive = (*reinterpret_cast<void**>(S + 0x60) != nullptr) ? 1 : 0;

    const int32_t mx = static_cast<int32_t>(
        static_cast<float>(*reinterpret_cast<int16_t*>(in + 0x109c)) * scale);
    const int32_t my = static_cast<int32_t>(
        static_cast<float>(*reinterpret_cast<int16_t*>(in + 0x10b4)) * scale);
    st.mouseX = mx;
    st.mouseY = my;

    st.buttonPressed = in[0x10fe];
    st.b03           = in[0x1116];
    st.b02           = in[0x112e];
    st.b04           = in[0x10ff];
    st.b06           = in[0x1117];
    st.b05           = in[0x112f];
    st.leftDown      = (*reinterpret_cast<int16_t*>(in + 0x10fc) > 0) ? 1 : 0;
    st.b08           = (*reinterpret_cast<int16_t*>(in + 0x1114) > 0) ? 1 : 0;
    st.b09           = (*reinterpret_cast<int16_t*>(in + 0x112c) > 0) ? 1 : 0;
    st.wheel         = *reinterpret_cast<int16_t*>(in + 0x11bc);
    st.config        = *reinterpret_cast<void**>(*InputConfigSingleton());
    st.mouseCaptured = 0;

    uint8_t leftDown = st.leftDown;

    if (st.buttonPressed) {
        out.U32(S + 0x58, static_cast<uint32_t>(mx), "_mouseDownX");
        out.U32(S + 0x5c, static_cast<uint32_t>(my), "_mouseDownY");
        st.dragDx = 0;                     // the JMP to 0x180121746 zeroes both
        st.dragDy = 0;
    } else if (leftDown) {
        st.dragDx = *reinterpret_cast<int32_t*>(S + 0x58) - mx;
        st.dragDy = *reinterpret_cast<int32_t*>(S + 0x5c) - my;
    } else {
        st.dragDx = 0;
        st.dragDy = 0;
    }

    // ---- pump every dialog, tail (frontmost) -> head -----------------------
    Node* n = *reinterpret_cast<Node**>(S + 0x08);
    if (n) {
        for (;;) {
            uint8_t* const dlg = static_cast<uint8_t*>(n->value);
            if (*reinterpret_cast<void**>(dlg + 0x28) == nullptr) {
                // `DialogInstance::Update(const Time&, UserInterface*,
                // Element::UpdateState&)` (0x180113980) — non-virtual member of
                // the real class (block B), which spells the state as its
                // address; it WRITES BACK into `st`.
                reinterpret_cast<DialogInstance*>(dlg)->Update(            // B.3.6 bridge
                    timeRef, this, reinterpret_cast<uint8_t*>(&st));
            }
            if ((*reinterpret_cast<uint32_t*>(dlg + 0x20) >> 1) & 1u) break;
            n = n->prev;
            if (!n) break;
        }
        // 0x180121781 — RELOADED from the UpdateState, which the callee may
        // have rewritten. This is the byte predict mode cannot know.
        leftDown = st.leftDown;
    }

    // ---- end drag-drop ----------------------------------------------------
    if (!leftDown && *reinterpret_cast<void**>(S + 0x60) != nullptr) {
        for (int32_t id = 0xd; id <= 0xe; ++id) {
            EventRecord rec;
            rec.dialog   = *reinterpret_cast<void**>(S + 0x60);
            rec.z08      = nullptr;
            rec.id       = id;
            rec.m1       = -1;
            rec.z18      = nullptr;
            rec.dragId   = *reinterpret_cast<uint32_t*>(S + 0x70);
            rec.dragData = *reinterpret_cast<void**>(S + 0x78);
            // rec.pad24 deliberately left uninitialised: retail never writes it
            // and the grow helper never copies it.
            const int32_t newSize = *reinterpret_cast<int32_t*>(S + 0x50) + 1;
            if (out.Call("Array<Event>::SetSize (drag-drop end/cancel)",
                         reinterpret_cast<const void*>(&rtcoll::RtColl_ArraySetSizeFill_180033690),
                         newSize, id))
                rtcoll::RtColl_ArraySetSizeFill_180033690(S + 0x48, newSize, &rec);
        }

        out.Ptr(S + 0x60, nullptr, "_dragDropDialog = 0");
        void* const dragEl = *reinterpret_cast<void**>(S + 0x68);
        if (dragEl) {
            // Element vtable slot 0 `delete` (slot 0, flags 1) — VIRTUAL.
            delete reinterpret_cast<UI::Element*>(dragEl);   // B.3.6 bridge
            out.Ptr(S + 0x68, nullptr, "_dragDropElement = 0 (inside the guard)");
        }
        out.Ptr(S + 0x68, nullptr, "_dragDropElement = 0 (again, unguarded)");
        out.U32(S + 0x70, 0, "_dragDropId = 0");
        out.Ptr(S + 0x78, nullptr, "_dragDropData = 0");
        out.Ptr(S + 0x80, nullptr, "_dragDropUserData = 0");
    }

    CleanRemovedDialogs();                                      // #3

    out.U8(S + 0xd9, st.mouseCaptured, "_mouseCaptured = UpdateState+0x20");

    return GetMouseCursorIndex(mx, my);
}

extern "C" int32_t RtUi_UserInterface_Update(UserInterface* self, const void* time) {
    return self->UI::UserInterface::Update(*static_cast<const System::Time*>(time));
}
