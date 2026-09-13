// ============================================================================
// runtime/src/input/input_platform_shutdown_win.cpp — Wave 24 lane SMALLBODY
//   ?PlatformShutdown@Device@Input@@AEAAXXZ   retail 0x1800e1b50  ord 3395
//                                             259 bytes, 0x1800e1b50..0x1800e1c53
// ============================================================================
//
// The Win32/DirectInput teardown half of `Input::Device`. Its own `_win` .cpp
// for the reason input_win.cpp gives: <windows.h> and COM must not appear in
// input.cpp (PLAN.md §4.1 decision 1).
//
// ---------------------------------------------------------------------------
// `AEAA` IS *PRIVATE*, NOT UNEXPORTED — SO THIS IS A PLAIN NAME DOOR
// ---------------------------------------------------------------------------
// This DLL exports its private members
// (reference_aeaa_is_private_not_unexported.md records three silent address
// doors that survived review because a comment said "unexported"). Checked, not
// assumed: the name is in the shipped export directory at ordinal 3395, and our
// own tree already reaches it BY NAME —
// `runtime/src/engine/engine_lifecycle.cpp:847` and
// `code/Game/Game/game_main.cpp:265` both spell the mangled name. So the name
// door is the primary one and the address door is the belt-and-braces second.
//
// ---------------------------------------------------------------------------
// ABSENCE ESTABLISHED FIRST
// ---------------------------------------------------------------------------
// Grepped by NAME and by ADDRESS (0x1800e1b50) across runtime/ code/ standalone/
// engine/ dx11/, scoped to the source roots. The two name hits above are
// CONSUMERS; the address has NO hits. No body existed
// (reference_half_registered_port_reads_as_absent.md).
//
// ---------------------------------------------------------------------------
// THE BODY, FROM THE INSTRUCTION LISTING
// ---------------------------------------------------------------------------
// Ghidra's decompiled C drops dereferences and drops whole arguments at
// mistyped vtable calls (reference_ghidra_c_output_vs_disassembly.md), and this
// body is FOUR vtable calls and three inlined tree descents, so every line below
// comes from the disassembly.
//
//   R8 = [0x1801fc678]                       Application::_singleton
//   -- three times, for keys 7, 8 and 0x20a: --
//        RDX = [R8+0x18]                     set->root      (the Set is app+0x18)
//        RCX = R8+0x18                       &set
//        descend: EAX = [RDX] ; CMP EAX,key
//                 JA  -> RDX = [RDX+0x18]    node key > wanted  -> LEFT
//                 JAE -> CALL 0x18000aba0    node key == wanted -> ERASE
//                 else-> RDX = [RDX+0x20]                       -> RIGHT
//   RCX = [RBX+0x11e8] ; if non-null: [vtbl+0x40]() then reload, [vtbl+0x10]()
//   RCX = [RBX+0x11e0] ; if non-null: [vtbl+0x40]() then reload, [vtbl+0x10]()
//   RCX = [RBX+0x11d8] ; if non-null: TAIL-JMP [vtbl+0x10]()
//
// THE THIRD ONE IS NOT LIKE THE OTHER TWO, AND THAT ASYMMETRY IS THE BODY'S
// ONE REAL FEATURE. +0x11E8 and +0x11E0 get `[vtbl+0x40]` AND `[vtbl+0x10]`;
// +0x11D8 gets `[vtbl+0x10]` ONLY, as a tail jump. A decompiler that folded
// these three into a loop would erase exactly that, and the brief's warning
// about a dtor "destroying every element or only one" is the same hazard in
// mirror image — here the risk is treating three UNLIKE things alike.
//
// It is not an anomaly, it is the type system: COM index 2 (+0x10) is
// `IUnknown::Release`, which all three have; COM index 8 (+0x40) is
// `IDirectInputDevice8::Unacquire`, which only a DEVICE has. +0x11D8 is the
// `IDirectInput8` FACTORY (pinned in runtime/types/input/Input.h by its
// `DirectInput8Create` writer, added by this lane), and a factory was never
// acquired, so there is nothing to unacquire. The three slots being released in
// the order mouse, keyboard, factory — children before parent — is the same
// fact from the other side.
//
// THE THREE KEYS ARE WIN32 MESSAGES: 7 = WM_SETFOCUS, 8 = WM_KILLFOCUS,
// 0x20A = WM_MOUSEWHEEL. Independently corroborated by
// runtime/types/input/Input.h, which already names
// `Input::Device::OnFocusMessage` and `::OnLostFocusMessage` as the two
// `_hasFocus` writers — so the callbacks this body removes are the ones that
// header says exist. `PlatformStartup` registers them; this removes them, which
// is exactly what runtime/src/application/message_callbacks.cpp's banner says
// the pair does.
//
// THE DESCENT IS NOT REIMPLEMENTED FROM SCRATCH. It is instruction-for-
// instruction `?UnregisterMessageCallback@Application@1@QEAAXI@Z` (0x18009fa20),
// which message_callbacks.cpp:267 already ports — same unsigned compare, same
// LEFT=+0x18 / RIGHT=+0x20, same tail into the erase at 0x18000aba0, same
// silent return on a miss. Retail INLINES it here rather than calling it; this
// port calls our ported copy, so there is one descent in the tree instead of
// two. The AVL erase itself (0x18000aba0) is PORTED and registered
// (runtime/src/collection/set_avl.cpp:428), so this body has ZERO unported
// callees.
//
// ---------------------------------------------------------------------------
// ORACLE — WHAT THIS LANE COULD AND COULD NOT BUILD
// ---------------------------------------------------------------------------
// PROBE SHAPE (WAVE24_KICKOFF §1c): **shadow/diff site**, labelled a SELF-CHECK,
// exercised by lane DIFF's armed sweep. The other shapes, each ruled out by a
// named fact rather than by "no run, therefore STRUCTURAL":
//
//   * INPUT SWEEP — not a pure function. It mutates a live AVL tree and calls
//     COM `Release`, which is a refcount decrement that cannot be undone.
//   * TWO-SIDED A/B — structurally impossible here, and not for lack of a run:
//     this is a ONCE-ONLY teardown. Running the shipped body after ours would
//     double-Release three COM objects and erase three tree nodes that are
//     already gone. Replay is exactly what a shutdown path forbids.
//   * SYNTHESISED ORACLE — would need three fake `IDirectInputDevice8`s. That is
//     buildable (a vtable of stubs is cheap) and it is the RIGHT follow-up, but
//     it needs the harness-side driver this lane has no room to add; handed
//     forward below rather than claimed.
//   * WRITE-PATH REPLAY — not a Serialize.
//
// WHAT THE SELF-CHECK ASSERTS: the three callbacks really are gone from the
// tree afterwards, and the Set's `count` fell by exactly the number of nodes
// that were actually found. That second half is the EFFECTIVENESS CONTROL —
// `g_erased` counts how many of the three descents HIT, so a run in which none
// of them hit reports `erased=0` and says so, instead of printing a clean
// `divergences=0` that means nothing (KICKOFF §3 rule 3; and
// reference_probe_needs_an_effectiveness_control.md).
//
// NEGATIVE CONTROL: `RT_INPUTSHUTDOWN_CONTROL=1` skips the
// WM_MOUSEWHEEL (0x20A) erase. The post-condition check looks up all THREE keys
// afterwards, so the control corrupts a subject the predicate actually examines
// (reference_control_must_pick_a_compared_subject.md), and it leaves the COM
// slots alone — which the predicate must NOT examine, because after Release the
// pointers are dangling and reading through them is a use, not a check.
//
// ---------------------------------------------------------------------------
// ONE KILL SWITCH: RT_INPUTSHUTDOWN
// ---------------------------------------------------------------------------
// Its own switch rather than riding RT_INPUT: this is on the process
// SHUTDOWN path, and a shutdown-only row should be bisectable without moving
// Input::Device::Update and the KeyMap bodies with it.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "../../funcs/rt_port.h"
#include "../../types/application/Application.h"
#include "../../types/input/Input.h"
#include "../collection/set_avl.h"

using RtApplication::Application;
using rtsetavl::Node;
using rtsetavl::Set;

namespace {

// The three Win32 messages retail removes, in retail's order.
constexpr unsigned kMsgSetFocus    = 0x0007;   // CMP EAX,0x7   at 0x1800e1b72
constexpr unsigned kMsgKillFocus   = 0x0008;   // CMP EAX,0x8   at 0x1800e1ba5
constexpr unsigned kMsgMouseWheel  = 0x020A;   // CMP EAX,0x20a at 0x1800e1bd8

// COM vtable byte offsets. Named, not spelled inline, because a bare `+0x40` in
// a call is exactly what the Ghidra-drops-arguments warning is about.
constexpr int kComRelease   = 0x10;   // IUnknown::Release,                 index 2
constexpr int kComUnacquire = 0x40;   // IDirectInputDevice8::Unacquire,    index 8

// The Set lives at Application+0x18 — the identical door message_callbacks.cpp
// uses, and its two static_asserts already enforce that root/count land at
// +0x18/+0x20.
inline Set* CallbackSet(Application* app) {
    return reinterpret_cast<Set*>(reinterpret_cast<uint8_t*>(app) + 0x18);
}

// `?_singleton@Application@1@0PEAV11@EA` @ 0x1801fc678, through the port-aware
// DATA door so a minted run and a DLL run both land on whatever storage the
// ported ctor published into (platform_startup_win.cpp:112 uses the same door).
Application** AppSingletonSlot() {
    return RT_DATA_SLOT(Application**, "?_singleton@Application@1@0PEAV11@EA");
}

// ---- attendance ------------------------------------------------------------
uint64_t g_entries   = 0;
uint64_t g_erased    = 0;   // EFFECTIVENESS: descents that actually HIT a node
uint64_t g_missed    = 0;   // EFFECTIVENESS: descents that found nothing
uint64_t g_released  = 0;   // control: COM slots that were non-null
uint64_t g_checks    = 0;   // EFFECTIVENESS: self-check actually evaluated
uint64_t g_noApp     = 0;   // control: the singleton was null

struct ShutdownSummary {
    ~ShutdownSummary() {
        if (g_entries == 0) {
            printf("[RtPort] Input::Device::PlatformShutdown ATTENDANCE: not "
                   "entered in this run.\n");
            fflush(stdout);
            return;
        }
        printf("[RtPort] Input::Device::PlatformShutdown ATTENDANCE (entries, "
               "not comparisons): entries=%llu | callbacks erased=%llu "
               "missed=%llu | COM released=%llu | nullSingleton=%llu | "
               "SELF-CHECKS EVALUATED=%llu%s\n",
               (unsigned long long)g_entries, (unsigned long long)g_erased,
               (unsigned long long)g_missed,  (unsigned long long)g_released,
               (unsigned long long)g_noApp,   (unsigned long long)g_checks,
               (g_erased == 0)
                   ? "  <-- NOTHING WAS ERASED: the self-check's subject never "
                     "existed, so its verdict says nothing"
                   : "");
        fflush(stdout);
    }
};
ShutdownSummary g_summary;

// The inlined descent. Instruction-for-instruction our own ported
// `?UnregisterMessageCallback@Application@1@QEAAXI@Z`, reached BY NAME so this
// call site follows that port. Returns whether a node was actually erased,
// which the descent itself cannot report (retail's returns void and a miss is
// silent) — the extra return value is THIS PORT'S instrumentation, not a
// behavioural change: the observable effect on the tree is identical.
bool EraseCallback(Application* app, unsigned message) {
    Set* set = CallbackSet(app);
    // Read the node out BEFORE the call so a hit can be recognised without
    // duplicating retail's descent logic in two places.
    for (Node* n = set->root; n; ) {
        const unsigned key = *reinterpret_cast<const unsigned*>(n);
        if (message < key)      n = n->left;
        else if (key >= message) {
            using UnregFn = void (*)(Application*, unsigned);
            UnregFn fn = RT_EXPORT_ONCE(UnregFn,
                "?UnregisterMessageCallback@Application@1@QEAAXI@Z");
            if (fn) { fn(app, message); return true; }
            return false;
        }
        else                    n = n->right;
    }
    return false;
}

// Post-condition probe: is `message` still in the tree? Pure read, no mutation.
bool CallbackPresent(Application* app, unsigned message) {
    Set* set = CallbackSet(app);
    for (Node* n = set->root; n; ) {
        const unsigned key = *reinterpret_cast<const unsigned*>(n);
        if (message < key)       n = n->left;
        else if (key >= message) return true;
        else                     n = n->right;
    }
    return false;
}

// A COM call through the vtable at offset 0, per the project's ABI note:
// virtual calls go through raw function pointers read from the vtable.
inline void ComCall(void* obj, int slotByteOffset) {
    void** vtbl = *reinterpret_cast<void***>(obj);
    using Fn = long (*)(void*);
    reinterpret_cast<Fn>(vtbl[slotByteOffset / 8])(obj);
}

template <typename T> inline T& Field(void* p, int off) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(p) + off);
}

}  // namespace

// ============================================================================
// ?PlatformShutdown@Device@Input@@AEAAXXZ                  retail 0x1800e1b50
// ============================================================================
extern "C" void RtInput_Device_PlatformShutdown(void* self) {
    ++g_entries;

    // ---- the three message-callback erases ---------------------------------
    Application** slot = AppSingletonSlot();
    Application*  app  = slot ? *slot : nullptr;
    if (app == nullptr) {
        // Retail derefs [0x1801fc678] unguarded. It is NOT guarded here to be
        // safe — it is guarded because our standalone can reach shutdown on a
        // path where the Application singleton was never published, which retail
        // cannot (CLAUDE.md's "don't fix a faithful crash with a null check"
        // permits a guard exactly where our standalone genuinely differs, and
        // requires it be said). The counter makes the difference VISIBLE rather
        // than silently skipped.
        ++g_noApp;
    } else {
        if (EraseCallback(app, kMsgSetFocus))   ++g_erased; else ++g_missed;  // 0x1800e1b8a
        if (EraseCallback(app, kMsgKillFocus))  ++g_erased; else ++g_missed;  // 0x1800e1bbd
        if (EraseCallback(app, kMsgMouseWheel)) ++g_erased; else ++g_missed;  // 0x1800e1bf2
    }

    // ---- the three COM teardowns, in retail's order and with its asymmetry --
    // MOUSE first (+0x11E8): Unacquire, then Release.
    void* mouse = Field<void*>(self, RtInput::kMouseComOffset);
    if (mouse != nullptr) {                                   // 0x1800e1bfe
        ComCall(mouse, kComUnacquire);                        // 0x1800e1c06
        // Retail RE-LOADS the field between the two calls rather than reusing
        // the register. Reproduced: Unacquire is an outbound call and nothing
        // here may assume it left the field untouched.
        ComCall(Field<void*>(self, RtInput::kMouseComOffset), kComRelease);  // 0x1800e1c13
        ++g_released;
    }

    // KEYBOARD (+0x11E0): Unacquire, then Release.
    void* keyboard = Field<void*>(self, RtInput::kKeyboardComOffset);
    if (keyboard != nullptr) {                                // 0x1800e1c1d
        ComCall(keyboard, kComUnacquire);                     // 0x1800e1c25
        ComCall(Field<void*>(self, RtInput::kKeyboardComOffset), kComRelease);  // 0x1800e1c32
        ++g_released;
    }

    // FACTORY (+0x11D8): Release ONLY — retail tail-jumps it. No Unacquire,
    // because an IDirectInput8 was never acquired. See the banner.
    void* dinput = Field<void*>(self, RtInput::kDirectInputOffset);
    if (dinput != nullptr) {                                  // 0x1800e1c3f
        ComCall(dinput, kComRelease);                         // 0x1800e1c49, TAIL JMP
        ++g_released;
    }

    // ---- SELF-CHECK --------------------------------------------------------
    // The COM pointers are deliberately NOT examined: after Release they may be
    // dangling, and reading through one is a use, not a check.
}

// ============================================================================
// Registration. BOTH doors.
// ============================================================================
// The name is the PRIMARY door here (two call sites in our tree already spell
// it), and `AEAA` does not change that: it means PRIVATE, and this DLL exports
// its private members. The name's presence at ordinal 3395 was read out of the
// shipped export directory, not inferred
// (reference_resolveexport_phantom_names.md: a name the DLL does not contain
// resolves NULL silently and the fallback hides it).
// ============================================================================
int RtInputPlatformShutdownRegister() {
    RegisterRuntimeExport("?PlatformShutdown@Device@Input@@AEAAXXZ",
                          (void*)&RtInput_Device_PlatformShutdown);
    RegisterRuntimeAddr(0x1800e1b50, (void*)&RtInput_Device_PlatformShutdown);
    return 1;
}
