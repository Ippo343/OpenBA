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
//  3. "No oracle."  PARTIALLY EXPIRED. The PREDICTIVE oracle
//     (`runtime/funcs/rt_predict.h`) inverts the replay: OURS predicts and
//     writes nothing, THEIRS runs for real on pristine state, and every
//     predicted location is compared against what the DLL left. Nothing is
//     drained twice, freed twice or pumped twice, because our body does not run.
//     Its coverage over `Update` is PARTIAL and the boundary is declared with
//     `Exclude()` — see "THE ORACLE'S COVERAGE BOUNDARY" below. A partial oracle
//     that reads like a full one is worse than none, so the holes are counted
//     and printed rather than argued away.
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
// GATE: the port is registered under `BANISHED_RT_UIFRAME` (default ON like
// every other wave switch, `=0` to fall back to the DLL). The DETOUR is
// separately OPT-IN under `BANISHED_RT_UIFRAME_DETOUR`, per runtime_detour.h's
// rule that "nothing goes default-on unproven".
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../funcs/rt_predict.h"
#include "../../types/ui/UserInterface.h"

// Same file-scope declaration userinterface.cpp and dialog.cpp use: the address
// door lives in standalone/, and pulling its header into a runtime/src file
// would invert the dependency the whole tree is arranged around.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

using UI::UserInterface;

inline uint8_t* B(void* p, size_t off) { return static_cast<uint8_t*>(p) + off; }

// ---- data slots -------------------------------------------------------------
// Through RT_DATA_SLOT, which never caches a null, for the reason
// userinterface.cpp gives: a slot resolved before the mint must be able to
// retry rather than freeze the DLL's answer for the process.
inline const double* TimerSecondsPerTick() {
    return RT_DATA_SLOT(const double*, "?_secondsPerTick@Timer@System@@0NA");
}
inline void** VideoDeviceSingleton() {
    return RT_DATA_SLOT(void**, "?_singleton@Device@Video@@0PEAV12@EA");
}
inline void** InputDeviceSingleton() {
    return RT_DATA_SLOT(void**, "?_singleton@Device@Input@@0PEAV12@EA");
}
inline void** InputConfigSingleton() {
    return RT_DATA_SLOT(void**, "?_singleton@Configuration@Input@@0PEAV12@EA");
}

// ---- the callees ------------------------------------------------------------
// Nine of the ten are EXPORTED, verified against the shipped export directory
// with pefile rather than inferred (`AEAA` means PRIVATE, not unexported —
// reference_aeaa_is_private_not_unexported.md; `CleanRemovedDialogs` is an
// `AEAA` name and is exported all the same).
const char* const kCleanRemoved = "?CleanRemovedDialogs@UserInterface@UI@@AEAAXXZ";
const char* const kCloseDialog  = "?CloseDialog@UserInterface@UI@@QEAAXV?$InternalPtr@VDialogInstance@UI@@@Object@@@Z";
const char* const kPlay         = "?Play@UserInterface@UI@@QEAAXH@Z";
const char* const kUpdateEvents = "?UpdateEvents@UserInterface@UI@@QEAAXXZ";
const char* const kComputeLayout= "?ComputeLayout@DialogInstance@UI@@QEAAXHHHH@Z";
const char* const kDlgUpdate    = "?Update@DialogInstance@UI@@QEAAXAEBVTime@System@@PEAVUserInterface@2@AEAUUpdateState@Element@2@@Z";
const char* const kCursorIndex  = "?GetMouseCursorIndex@UserInterface@UI@@QEBAHHH@Z";
const char* const kMemAlloc     = "?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z";
const char* const kMemFree      = "?MemFree@MemoryAllocator@System@@SAXPEAX@Z";

// THE ONE ADDRESS DOOR, declared rather than hidden.
//
// 0x180033690 is `Collection::Array<UI::Event>::SetSize(int, const Event*)` —
// the grow-and-fill helper both event pushes call. It is **not in the export
// table** (checked with pefile; the eight other callees are), so there is no
// name door for it. A by-address call is a CUTOVER OBLIGATION, not a free
// choice: `reference_by_address_door_dies_at_cutover.md`. Recorded as such in
// docs/runtime_port/WAVE10_LANE_UI1.md rather than left for a residue run to
// rediscover.
//
// It is reached through `ResolveAddr(Module::Runtime, ...)`, which consults the
// port registry FIRST — so the day anyone ports this helper we get theirs, which
// is the correct outcome and not a bypass
// (reference_resolveaddr_consults_port_registry_first.md).
constexpr uintptr_t kEventsSetSizeAddr = 0x180033690;

using CleanFn   = void  (*)(void*);
using CloseFn   = void  (*)(void*, void*);
using PlayFn    = void  (*)(void*, int32_t);
using EventsFn  = void  (*)(void*);
using LayoutFn  = void  (*)(void*, int32_t, int32_t, int32_t, int32_t);
using DlgUpdFn  = void  (*)(void*, const void*, void*, void*);
using CursorFn  = int32_t (*)(const void*, int32_t, int32_t);
using AllocFn   = void* (*)(uint64_t, int32_t);
using FreeFn    = void  (*)(void*);
using SetSizeFn = void  (*)(void*, int32_t, const void*);
using DeleteFn  = void  (*)(void*, int32_t);

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
// The body. ONE body, two modes — the rt_predict.h contract.
// ============================================================================
// `out` is a live sink in a normal frame (every write is a store, every
// `Call` returns true) and a predicting sink under BANISHED_RT_PREDICT (every
// write is recorded, every `Call` returns false).
//
// Reads of locations THIS BODY WRITES go through `out.ReadU*` so the predictor
// is self-consistent; reads of locations it never writes stay plain loads. The
// distinction matters here in exactly one place — `_fadeAmount`, which is read
// back inside the fade arm after being written.
int32_t UpdateBody(UserInterface* self, const void* time, RtSink& out) {
    uint8_t* const S = reinterpret_cast<uint8_t*>(self);

    // 0x1801213ef — _mouseCaptured = 0, BEFORE anything else.
    out.U8(S + 0xd9, 0, "_mouseCaptured = 0 (entry)");

    // 0x1801213f6
    CleanFn clean = RT_EXPORT_ONCE(CleanFn, kCleanRemoved);
    if (out.Call("CleanRemovedDialogs #1", (const void*)clean))
        clean(self);

    // ---- drain _addDialogs back-to-front into the list ---------------------
    // Retail walks i = _size-1 down to 0 (0x1801213fb: MOVSXD R14, size-1, and
    // JS past the whole loop when negative — a genuine sign-extension, which
    // Ghidra prints as a zero extension).
    const int32_t addCount = *reinterpret_cast<int32_t*>(S + 0x30);
    for (int64_t i = static_cast<int64_t>(addCount) - 1; i >= 0; --i) {
        void** const addData = *reinterpret_cast<void***>(S + 0x28);

        Node* node = nullptr;
        if (out.Call("MemAlloc(0x18, 8) — list node", (const void*)kMemAlloc, 0x18, 8)) {
            node = static_cast<Node*>(
                RT_EXPORT_ONCE(AllocFn, kMemAlloc)(0x18, 8));
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

                void* tmp = toolTip;   // the by-value InternalPtr copy
                CloseFn close = RT_EXPORT_ONCE(CloseFn, kCloseDialog);
                if (out.Call("CloseDialog(tooltip)", (const void*)close))
                    close(self, &tmp);
                out.Ptr(S + 0x40, nullptr, "_toolTipDialog = 0");
                out.Ptr(S + 0x38, nullptr, "_toolTipOwner = 0");
            }
        }
    }

    // 0x1801214d3 — the "a dialog opened" sound, gated on the PRE-drain count.
    if (addCount > 0) {
        PlayFn play = RT_EXPORT_ONCE(PlayFn, kPlay);
        if (out.Call("Play(0)", (const void*)play, 0))
            play(self, 0);
    }

    // 0x1801214e3 — free the array and clear BOTH size and capacity with one
    // QWORD store (0x180121525: MOV qword[RSI+0x30], 0).
    void* const addData = *reinterpret_cast<void**>(S + 0x28);
    if (addData) {
        FreeFn mfree = RT_EXPORT_ONCE(FreeFn, kMemFree);
        if (out.Call("MemFree(_addDialogs._data)", (const void*)mfree))
            mfree(addData);
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
    EventsFn updEvents = RT_EXPORT_ONCE(EventsFn, kUpdateEvents);
    if (out.Call("UpdateEvents", (const void*)updEvents))
        updEvents(self);
    if (out.Call("CleanRemovedDialogs #2", (const void*)clean))
        clean(self);

    // ---- layout every dialog, head -> tail --------------------------------
    LayoutFn layout = RT_EXPORT_ONCE(LayoutFn, kComputeLayout);
    for (Node* n = *reinterpret_cast<Node**>(S + 0x00); n; n = n->next) {
        if (out.Call("DialogInstance::ComputeLayout", (const void*)layout, 0, 0, w, h))
            layout(n->value, 0, 0, w, h);
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
    const bool listEmpty = (n == nullptr);
    if (n) {
        DlgUpdFn dlgUpd = RT_EXPORT_ONCE(DlgUpdFn, kDlgUpdate);
        for (;;) {
            uint8_t* const dlg = static_cast<uint8_t*>(n->value);
            if (*reinterpret_cast<void**>(dlg + 0x28) == nullptr) {
                if (out.Call("DialogInstance::Update", (const void*)dlgUpd))
                    dlgUpd(dlg, time, self, &st);
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
    // The whole block is gated on a value the suppressed callee owns, so in
    // predict mode with a non-empty list the body ABSTAINS: taking either arm
    // would be a guess, and a predictor that guesses turns a coverage hole into
    // a false mismatch. The caller excludes +0x060..+0x089 on exactly that path.
    const bool conditionKnown = listEmpty || !out.predicting();
    if (conditionKnown && !leftDown && *reinterpret_cast<void**>(S + 0x60) != nullptr) {
        SetSizeFn setSize = reinterpret_cast<SetSizeFn>(
            ResolveAddr(Module::Runtime, kEventsSetSizeAddr));

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
                         (const void*)setSize, newSize, id) && setSize)
                setSize(S + 0x48, newSize, &rec);
        }

        out.Ptr(S + 0x60, nullptr, "_dragDropDialog = 0");
        void* const dragEl = *reinterpret_cast<void**>(S + 0x68);
        if (dragEl) {
            DeleteFn del = *reinterpret_cast<DeleteFn*>(*reinterpret_cast<void**>(dragEl));
            if (out.Call("_dragDropElement vtbl[0] deleting dtor",
                         (const void*)del, 1))
                del(dragEl, 1);
            out.Ptr(S + 0x68, nullptr, "_dragDropElement = 0 (inside the guard)");
        }
        out.Ptr(S + 0x68, nullptr, "_dragDropElement = 0 (again, unguarded)");
        out.U32(S + 0x70, 0, "_dragDropId = 0");
        out.Ptr(S + 0x78, nullptr, "_dragDropData = 0");
        out.Ptr(S + 0x80, nullptr, "_dragDropUserData = 0");
    }

    if (out.Call("CleanRemovedDialogs #3", (const void*)clean))
        clean(self);

    out.U8(S + 0xd9, st.mouseCaptured, "_mouseCaptured = UpdateState+0x20");

    CursorFn cursor = RT_EXPORT_ONCE(CursorFn, kCursorIndex);
    if (!out.Call("GetMouseCursorIndex", (const void*)cursor, mx, my))
        return 0;               // predict mode: the return is not compared
    return cursor(self, mx, my);
}

// The detour is OPT-IN, not a kill switch: `RtWaveEnabled` is default-ON and
// that is the wrong default for a code patch into the shipped DLL.
inline bool DetourOptedIn() {
    const char* e = std::getenv("BANISHED_RT_UIFRAME_DETOUR");
    return e && std::strcmp(e, "0") != 0 && std::strcmp(e, "false") != 0;
}

} // namespace

// ---------------------------------------------------------------------------
// The detour seam
// ---------------------------------------------------------------------------
// standalone/ui_detour.cpp patches 0x1801213c0 so the DLL's OWN caller —
// `Engine::MainLoop`'s frame, which is the ONLY caller that matters for this
// function — reaches the port. Without it `RegisterRuntimeExport` redirects our
// call sites only and the DLL keeps running its own `Update` on an object our
// `AddDialog`/`SetCursor`/`BringDialogToFront` ports already reshape. That is
// the split brain Wave 7 lane C named, and it EXISTS TODAY because those three
// mutators shipped in Wave 9 while `Update` did not.
extern "C" bool  RtUiUserInterfaceUpdateInstallDetour(void* ourFn);
extern "C" void* RtUiUserInterfaceUpdateTrampoline();

namespace {

// Retail's body, through whichever door is correct RIGHT NOW.
//
// WITH THE DETOUR IN, THE RAW EXPORT IS NOT RETAIL — it is a jump back into us.
// `RT_ORIGINAL_ONCE` resolves through `ResolveExportRaw`, i.e. exactly the
// address we patched, so an oracle that used it here would not diverge, it
// would RECURSE until the stack died
// (reference_detour_machinery_exists_wave7_zero_is_stale.md). The trampoline is
// the only correct "theirs" once the detour is installed, and the raw export is
// the only correct one before it. A null trampoline means "no detour", never
// "no original".
int32_t CallRetailUpdate(UserInterface* self, const void* time) {
    using Fn = int32_t (*)(void*, const void*);
    if (void* tramp = RtUiUserInterfaceUpdateTrampoline())
        return reinterpret_cast<Fn>(tramp)(self, time);
    return RT_ORIGINAL_ONCE(Fn, "?Update@UserInterface@UI@@QEAAHAEBVTime@System@@@Z")(self, time);
}

} // namespace

extern "C" int32_t RtUi_UserInterface_Update(UserInterface* self, const void* time) {
    RT_PREDICT_SITE("UI::UserInterface::Update");

    if (RT_PREDICT_ON) {
        int32_t theirs = 0;
        RT_PREDICT_ROUND(round) {
            uint8_t* const S = reinterpret_cast<uint8_t*>(self);

            // COVERAGE CONTROL: watch the whole object, so any byte retail
            // changes that we neither predicted nor excluded is REPORTED.
            // Predictions on their own are only a lower bound.
            round.Watch(self, 0x128, "UI::UserInterface");

            // The declared holes. Each names the suppressed callee that owns it.
            round.Exclude(S + 0x000, 0x18,
                          "_dialogs: CleanRemovedDialogs x3 unlinks here, and the "
                          "drain's node pointers are addresses MemAlloc chose");
            round.Exclude(S + 0x018, 0x10,
                          "_removedDialogs: cleared by CleanRemovedDialogs");
            round.Exclude(S + 0x038, 0x10,
                          "_toolTipOwner/_toolTipDialog: written by the suppressed "
                          "CloseDialog arm");
            round.Exclude(S + 0x048, 0x10,
                          "_events: UpdateEvents + CleanRemovedDialogs + the two "
                          "suppressed Array<Event>::SetSize pushes");
            round.Exclude(S + 0x0d9, 1,
                          "_mouseCaptured EXIT store is UpdateState+0x20, written "
                          "by the suppressed DialogInstance::Update");

            // The drag-drop block is only excluded when the condition that
            // governs it is unknowable — i.e. when there is at least one dialog
            // for the suppressed DialogInstance::Update to have rewritten
            // UpdateState+0x07. With an EMPTY list the byte is ours and the
            // whole block stays under comparison.
            if (*reinterpret_cast<void**>(S + 0x08) != nullptr) {
                round.Exclude(S + 0x060, 0x2a,
                              "drag-drop end: gated on UpdateState+0x07, which "
                              "DialogInstance::Update writes back and predict mode "
                              "suppresses");
            }

            UpdateBody(self, time, round.sink());   // predicts, writes nothing
            theirs = CallRetailUpdate(self, time);  // the authority runs for real
        }
        // Retail's return value, because retail's body is the one that ran.
        return theirs;
    }

    RtSink live;
    return UpdateBody(self, time, live);
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------
int RtUiFrameRegister() {
    int count = 0;

    // Registered under BOTH the mangled name and the address. WAVE7.md §3's
    // rule: a port registered under the name only can be dead at a call site
    // that reaches the DLL through the address fallback, while reporting
    // `calls=0 divergences=0` — a clean-looking oracle over nothing.
    RegisterRuntimeExport("?Update@UserInterface@UI@@QEAAHAEBVTime@System@@@Z",
                          (void*)&RtUi_UserInterface_Update); ++count;
    RegisterRuntimeAddr(0x1801213c0, (void*)&RtUi_UserInterface_Update);

    // OPT-IN, per runtime_detour.h: "Every detour gets an env kill-switch at its
    // call site. Nothing goes default-on unproven." With it off the port keeps
    // only our own call sites and the DLL's MainLoop keeps its own body — i.e.
    // exactly the split brain that exists today, unchanged.
    if (DetourOptedIn())
        RtUiUserInterfaceUpdateInstallDetour((void*)&RtUi_UserInterface_Update);

    return count;
}
