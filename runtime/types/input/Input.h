#pragma once
// ============================================================================
// runtime/types/input/Input.h — Input::KeyMap, Input::Device, Input::Configuration
// (Wave 8, lane D)
// ============================================================================
//
// Retail Runtime-x64.dll, image base 0x180000000. Every offset below was read
// from the DISASSEMBLY (not Ghidra's decompiled C — see
// reference_ghidra_c_output_vs_disassembly.md) of the 9 KeyMap point-lookup
// members, Device::GetValue/Pressed/Released/ResetState, and
// Configuration::Load/Save (all listed with retail addresses in
// docs/runtime_port/WAVE8_LANE_D.md §1).
//
// No `??_7` vtable is exported for any of the three classes (WAVE8.md §2).
// Verified NOT inferred: `Input::KeyMap::RegisterInput` (0x1800a7e70) and
// `Input::Device::GetValue` (0x1800e1660) both begin by reading fields of
// `this` with no vtable dereference anywhere in either body, and neither
// class's export list contains a `??0`/`??1` this lane needed to disassemble
// for a vtable stamp. Treated as non-polymorphic, consistent with (not proven
// by) the absent export.
// ============================================================================

#include <cstdint>
#include <cstddef>

namespace RtInput {

// ============================================================================
// Input::KeyMap
// ============================================================================
//
// `_bindings` is a `Collection::Set<Binding>` (PLAN.md §4.3 risk 1 — this is
// the SAME family as Persistence's Set<Pair<String,String>>, just a POD key
// this time) embedded directly at KeyMap+0x08: `Set::_root` lands at
// KeyMap+0x08, `Set::_count` at KeyMap+0x10 — pinned by every one of
// ClearAll/ClearInput/GetDeviceForId/GetInputForId/GetValue/Pressed/
// RegisterInput/SetInput reading/writing `*(KeyMap+8)` as the root pointer
// and `RegisterInput` incrementing the dword at `KeyMap+0x10` on every insert.
//
// `_disableKeyboardCount` (+0x00) is NOT a proven engine name — no exported
// accessor names it. It gates exactly one thing, visible in both
// `KeyMap::GetValue` (0x1800a7b60) and `KeyMap::Pressed` (0x1800a7a80): when
// `> 0` AND the matched binding's device slot is 1 (keyboard), the function
// returns 0/false for that slot instead of consulting `Device`'s live state,
// i.e. "something has temporarily suppressed keyboard bindings" — the shape
// of a text-edit-has-focus guard. Named for its observed effect, not for any
// evidence of what sets/clears it (this lane found no writer in its own 21
// functions).
struct KeyMap {
    int32_t  _disableKeyboardCount;  // +0x00 — see comment above
    int32_t  _pad04;                 // +0x04 — never read by any ported fn
    void*    _root;                  // +0x08 — Collection::Set<Binding>::_root
    int32_t  _count;                 // +0x10 — Collection::Set<Binding>::_count
    int32_t  _pad14;                 // +0x14 — never read by any ported fn
};
static_assert(offsetof(KeyMap, _root)  == 0x08, "KeyMap::_root @ +0x08");
static_assert(offsetof(KeyMap, _count) == 0x10, "KeyMap::_count @ +0x10");

// One binding: an action id maps to up to 2 (device, deviceInput) slots
// ("primary"/"secondary" binding). 24 bytes — the `E24` element `Collection.h`
// already names as "Input::KeyMap's key-set element" from a Wave 2 cross-
// check; this lane pins every byte of it from `RegisterInput`'s disassembly
// (0x1800a7e70: the stack blob built at [RSP+0x20]..[RSP+0x37] before the
// call to the shared node-insert helper at 0x180023060).
struct Binding {
    uint8_t     id;            // +0x00 — BST key, UNSIGNED byte compare
    uint8_t     exclusive;     // +0x01 — RegisterInput's `bool exclusive` param, verbatim
    uint8_t     _pad02[6];     // +0x02..+0x07 — never written by RegisterInput; garbage
    const char* description;  // +0x08 — RegisterInput's `const char* name` param, verbatim
    uint8_t     device[2];    // +0x10 — device[slot], 0xFF == unbound
    uint8_t     inputId[2];   // +0x12 — deviceInput[slot]
    uint8_t     _pad14[4];    // +0x14..+0x17 — never written by RegisterInput; garbage
};
static_assert(sizeof(Binding) == 0x18, "Binding must be 0x18 (Collection::Set element)");
static_assert(offsetof(Binding, description) == 0x08, "");
static_assert(offsetof(Binding, device)      == 0x10, "");
static_assert(offsetof(Binding, inputId)     == 0x12, "");

// `Collection::Set<Binding>::Node` — align8(0x18)+0x00 == +0x18, matching
// Collection.h's S24 layout check exactly (parent +0x18, left +0x20,
// right +0x28, balance +0x30, height +0x32). Verified against every point
// function's `+0x20`/`+0x28` child-descent and `RegisterInput`'s `+0x18`
// parent write (`FUN_180023170`, not reimplemented — see the .cpp).
struct BindingNode {
    Binding      element;   // +0x00
    BindingNode* parent;    // +0x18
    BindingNode* left;      // +0x20
    BindingNode* right;     // +0x28
    int16_t      balance;   // +0x30
    int16_t      height;    // +0x32
    uint32_t     _padTail;  // +0x34
};
static_assert(sizeof(BindingNode) == 0x38, "BindingNode must be 0x38");
static_assert(offsetof(BindingNode, parent)  == 0x18, "");
static_assert(offsetof(BindingNode, left)    == 0x20, "");
static_assert(offsetof(BindingNode, right)   == 0x28, "");
static_assert(offsetof(BindingNode, balance) == 0x30, "");
static_assert(offsetof(BindingNode, height)  == 0x32, "");

// ============================================================================
// Input::Device — the DirectInput8 layer. Layout is PARTIAL: only the byte
// ranges the 5 ported const accessors + ResetState touch are pinned. Nothing
// past +0x11D0 is evidenced (the DirectInput8 COM device pointers this lane
// did NOT map — see WAVE8_LANE_D.md §"Update / EnableMouseClipping").
//
// Three flat arrays of a 24-byte `State` record, back to back, pinned by
// GetValue/Pressed/Released (0x1800e1660/0x1800e16c0/0x1800e1720) AND
// independently by ResetState (0x1800e0ef0)'s three zeroing loops, which
// agree on every base and count:
//
//   +0x0000  State analog[4][0x12]   4 "joystick" devices (InputDevice==0),
//                                    0x12 (18) inputs each, stride 0x1B0
//                                    (== 0x12 * sizeof(State)).
//   +0x06C0  State keyboard[0x69]    105 keys (InputDevice==1).
//   +0x1098  State mouse[0xD]        13 inputs (InputDevice==2).
//   +0x11D0  bool _mouseClipping     RIGHT after the mouse array
//                                    (0x1098 + 0xD*0x18 == 0x11D0), pinned by
//                                    `EnableMouseClipping` (0x1800e1780).
//
// `State` (0x18 bytes). Wave 8 lane D could evidence only +0x04/+0x06/+0x07
// from the const accessors and left the rest opaque. Wave 9 lane ENGIO ported
// `Device::UpdateState` (0x1800E0FC0 — the body `Device::Update` calls twice),
// which WRITES every field of this record and so names three more. Each offset
// below cites the store that pins it; all six appear in the 4x-unrolled body
// AND again in its scalar tail, which agree instruction for instruction.
//
//   +0x00 float normalized   1800e11f0  MOVSS [RCX+R10-0x4],XMM0
//                            == (float)value / 32767.0f
//   +0x04 int16 value        1800e1229  MOV   word [RCX+R10],AX
//   +0x06 bool  pressed      1800e1208  MOV   byte [RCX+R10+0x2],AL
//   +0x07 bool  released     1800e1221  MOV   byte [RCX+R10+0x3],AL
//   +0x08 bool  repeat       1800e123f  MOV   byte [RCX+R10+0x4],SIL   and
//                            1800e138b  MOV   byte [RCX+R10+0x4],1
//   +0x10 int64 holdTicks    1800e136b  ADD   qword [RCX+R10+0xc],RAX
//
// +0x09..+0x0F and the bytes past +0x17 are still not written by anything read
// so far; they stay padding rather than being guessed
// (reference_never_written_needs_by_whom.md). Note that `Device::Update`'s two
// scratch-buffer zeroing loops deliberately do NOT cover +0x08..+0x0F — see
// the comment on `ZeroScratch` in input_device_update.cpp.
struct State {
    float    normalized;     // +0x00 — value / 32767.0f, written by UpdateState
    int16_t  value;          // +0x04 — axis/analog value (Device::GetValue)
    bool     pressed;        // +0x06 — Device::Pressed
    bool     released;       // +0x07 — Device::Released
    bool     repeat;         // +0x08 — key-repeat pulse for this frame
    uint8_t  _unknown09[7];  // +0x09..+0x0F — never written by any body read so far
    int64_t  holdTicks;      // +0x10 — accumulated hold time, in System::Time ticks
};
static_assert(sizeof(State) == 0x18, "State must be 0x18");
static_assert(offsetof(State, normalized) == 0x00, "");
static_assert(offsetof(State, value)      == 0x04, "");
static_assert(offsetof(State, pressed)    == 0x06, "");
static_assert(offsetof(State, released)   == 0x07, "");
static_assert(offsetof(State, repeat)     == 0x08, "");
static_assert(offsetof(State, holdTicks)  == 0x10, "");

constexpr int kJoystickCount       = 4;
constexpr int kJoystickInputCount  = 0x12;  // 18
constexpr int kJoystickStride      = 0x1B0; // == kJoystickInputCount * sizeof(State)
constexpr int kKeyboardOffset      = 0x6C0;
constexpr int kKeyboardInputCount  = 0x69;  // 105
constexpr int kMouseOffset         = 0x1098;
constexpr int kMouseInputCount     = 0x0D;  // 13
constexpr int kMouseClippingOffset = 0x11D0;

// +0x11F0  bool _hasFocus — added by Wave 9 lane IOA, which ported
// `PlatformUpdate` (0x1800e2130) and needed the second half of its guard.
// Pinned by FOUR independent writers, all found by a byte-pattern search for
// `C6 ?? F0 11 00 00 ??` (`MOV byte ptr [reg+0x11F0], imm8`) over the whole
// image — there are exactly four and no others, and no non-immediate writer
// (`88 ?? F0 11 00 00` returns nothing):
//
//   0x1800e1436  Input::Device::Device        (ctor)
//   0x1800e17db  Input::Device::OnLostFocusMessage  -> 0, then PlatformUpdate
//   0x1800e180b  Input::Device::OnFocusMessage      -> 1, then PlatformUpdate
//   0x1800e1832  Input::Device::PlatformConstruct
//
// The two message handlers are what name it: WM_KILLFOCUS clears it,
// WM_SETFOCUS sets it, and each then re-runs `PlatformUpdate` so the cursor
// clip follows the window's focus. NOT a DirectInput field — nothing in this
// range is (see the block comment in runtime/src/input/input_win.cpp).
constexpr int kHasFocusOffset      = 0x11F0;

// The rest of the tail, pinned by Wave 9 lane ENGIO from
// `Device::PlatformGetKeyboardState` (0x1800E1C60) and
// `Device::PlatformGetMouseState` (0x1800E1E10). These are the fields WAVE9.md
// row 16 called "DirectInput8 COM pointers past Device+0x11D0 (unmapped)".
// There are exactly TWO COM pointers and three plain int32s, and only TWO
// methods of the COM interface are ever used — so the region was never as
// unbounded as that row implies.
//
//   +0x11E0  IDirectInputDevice8*  keyboard   1800e1d15 MOV RCX,[RCX+0x11e0]
//   +0x11E8  IDirectInputDevice8*  mouse      1800e1f81 MOV RCX,[RCX+0x11e8]
//   +0x11F4  int32 wheelAccum   (legacy path only)  1800e1f27 MOV ECX,[RSI+0x11f4]
//   +0x11F8  int32 lastCursorX  (legacy path only)  1800e1e98 MOV [RSI+0x11f8],EAX
//   +0x11FC  int32 lastCursorY  (legacy path only)  1800e1e9e MOV [RSI+0x11fc],EDX
//
// The two COM slots used, and NO others, in either poller:
//   [vtbl+0x38]  IDirectInputDevice8::Acquire
//   [vtbl+0x48]  IDirectInputDevice8::GetDeviceState
// Those are COM vtable indices 7 and 9, which is where `IDirectInputDevice8W`
// puts Acquire and GetDeviceState — an independent confirmation that these two
// pointers really are DirectInput devices rather than something else that
// happens to live at the end of the object.
constexpr int kKeyboardComOffset   = 0x11E0;
constexpr int kMouseComOffset      = 0x11E8;
constexpr int kWheelAccumOffset    = 0x11F4;
constexpr int kLastCursorXOffset   = 0x11F8;
constexpr int kLastCursorYOffset   = 0x11FC;

// The smallest size containing every field any ported body touches. This is
// NOT `sizeof(Input::Device)`: nothing read so far establishes an upper bound,
// so it is a LOWER BOUND, and the probe's byte comparison over it is SILENT
// about anything past it — the same limit lane M records for the
// released-pointer detector, stated here rather than left to be discovered.
constexpr int kDeviceMappedSize    = 0x1200;

static_assert(kJoystickCount * kJoystickStride == kKeyboardOffset,
              "joystick block must exactly fill [0, keyboard)");
static_assert(kKeyboardOffset + kKeyboardInputCount * (int)sizeof(State) == kMouseOffset,
              "keyboard block must exactly fill [keyboard, mouse)");
static_assert(kMouseOffset + kMouseInputCount * (int)sizeof(State) == kMouseClippingOffset,
              "mouse block must exactly fill [mouse, mouseClipping)");

// Input::Device::InputDevice enum, from the mangled parameter type
// (`W4InputDevice@Device@Input@@`) and the three branches every ported
// accessor takes.
enum class InputDevice : int32_t {
    Joystick = 0,
    Keyboard = 1,
    Mouse    = 2,
};

// ============================================================================
// Input::Configuration — 3 bools behind an owned KeyMap pointer. Pinned by
// BOTH Load (0x1800e0c80) and Save (0x1800e0c00) disassembly, which agree
// exactly on all three field offsets and the registry key names.
// ============================================================================
struct Configuration {
    void* keyMap;         // +0x00 — Input::KeyMap*
    bool  mouseClipping;  // +0x08 — registry "Input"/"MouseClipping"
    bool  legacy;         // +0x09 — registry "Input"/"Legacy"
    bool  mouseDisplay;   // +0x0A — registry "Input"/"MouseDisplay"
};
static_assert(offsetof(Configuration, keyMap)        == 0x00, "");
static_assert(offsetof(Configuration, mouseClipping) == 0x08, "");
static_assert(offsetof(Configuration, legacy)        == 0x09, "");
static_assert(offsetof(Configuration, mouseDisplay)  == 0x0A, "");

}  // namespace RtInput
