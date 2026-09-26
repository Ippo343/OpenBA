// ============================================================================
// tests/unit/input_abi_test.cpp — ENDGAME B.3.7 block B5
//   Input::KeyMap / Input::Device / Input::Configuration as real,
//   NON-POLYMORPHIC classes, and their layout-only value types.
//
// PART A — sizes / shape, each pinned by a retail allocation (B37_RULES
//   sec. 2.3): KeyMap 0x18 (`MemAlloc(0x18,4)` Configuration::Startup
//   0x1800e0d76), Configuration 0x10 (`MemAlloc(0x10)` 0x1800dbd51), Device
//   0x1200 (`MemAlloc(0x1200)` 0x1800dbe0b). No vtable on any of them. KeyMap
//   is NOT trivially copyable (its exported copy ctor 0x1800229d0 is DEEP,
//   B7-8), the element / node / State value types ARE trivial (no ctor: retail
//   builds them with raw stores that leave bytes unwritten).
// PART B — the field offsets every ported member reads.
// PART C — declared signatures that carry an ABI claim: the KeyMap readers
//   take (joystick, id); the two ClearInput overloads; Device::Update and
//   UpdateState take `System::Time const&`; EnableMouseClipping takes the RAW
//   byte; the three message callbacks have Application's MessageCallback
//   shape; the RtInput:: spellings ARE the classes.
// The member bodies live in runtime/src/input (not linked here), so everything
// below is layout and declared type, not behaviour.
// ============================================================================
#include "unit.h"

#include "runtime/types/input/Input.h"
#include "runtime/types/application/Application.h"

#include <cstdint>
#include <cstddef>
#include <type_traits>

namespace {

using Input::KeyMap;
using Input::Device;
using Input::Configuration;
using Input::Binding;
using Input::BindingNode;
using State = Input::Device::State;

// ---------------------------------------------------------------- PART A
UNIT_TEST(input_sizes_and_shape) {
    CHECK_EQ((int)sizeof(KeyMap), 0x18);
    CHECK_EQ((int)sizeof(Configuration), 0x10);
    CHECK_EQ((int)sizeof(Device), 0x1200);
    CHECK_EQ((int)sizeof(State), 0x18);
    CHECK_EQ((int)sizeof(Binding), 0x18);
    CHECK_EQ((int)sizeof(BindingNode), 0x38);
    CHECK_EQ((int)alignof(KeyMap), 8);
    CHECK_EQ((int)alignof(Configuration), 8);
    CHECK_EQ((int)alignof(Device), 8);

    CHECK(!std::is_polymorphic<KeyMap>::value);
    CHECK(!std::is_polymorphic<Configuration>::value);
    CHECK(!std::is_polymorphic<Device>::value);

    CHECK(!std::is_trivially_copyable<KeyMap>::value);   // deep copy ctor declared
    CHECK(std::is_trivial<Binding>::value);
    CHECK(std::is_trivial<BindingNode>::value);
    CHECK(std::is_trivial<State>::value);
}

// ---------------------------------------------------------------- PART B
UNIT_TEST(input_field_offsets) {
    // KeyMap: the suspend count, then the embedded Set<Binding>.
    CHECK_EQ((int)offsetof(KeyMap, _suspendCount), 0x00);   // Suspend 0x1800a7f90
    CHECK_EQ((int)offsetof(KeyMap, _root), 0x08);           // every point lookup
    CHECK_EQ((int)offsetof(KeyMap, _count), 0x10);          // RegisterInput's INC

    CHECK_EQ((int)offsetof(Binding, id), 0x00);
    CHECK_EQ((int)offsetof(Binding, exclusive), 0x01);
    CHECK_EQ((int)offsetof(Binding, description), 0x08);
    CHECK_EQ((int)offsetof(Binding, device), 0x10);
    CHECK_EQ((int)offsetof(Binding, inputId), 0x12);
    CHECK_EQ((int)offsetof(BindingNode, parent), 0x18);
    CHECK_EQ((int)offsetof(BindingNode, left), 0x20);
    CHECK_EQ((int)offsetof(BindingNode, right), 0x28);

    CHECK_EQ((int)offsetof(Configuration, _keyMap), 0x00);
    CHECK_EQ((int)offsetof(Configuration, _mouseClipping), 0x08);
    CHECK_EQ((int)offsetof(Configuration, _legacy), 0x09);
    CHECK_EQ((int)offsetof(Configuration, _mouseDisplay), 0x0A);

    CHECK_EQ((int)offsetof(State, value), 0x04);
    CHECK_EQ((int)offsetof(State, pressed), 0x06);
    CHECK_EQ((int)offsetof(State, released), 0x07);
    CHECK_EQ((int)offsetof(State, holdTicks), 0x10);

    CHECK_EQ((int)offsetof(Device, _joystick), 0x0000);
    CHECK_EQ((int)offsetof(Device, _keyboard), 0x06C0);
    CHECK_EQ((int)offsetof(Device, _mouse), 0x1098);
    CHECK_EQ((int)offsetof(Device, _mouseClipping), 0x11D0);
    CHECK_EQ((int)offsetof(Device, _directInput), 0x11D8);
    CHECK_EQ((int)offsetof(Device, _keyboardDevice), 0x11E0);
    CHECK_EQ((int)offsetof(Device, _mouseDevice), 0x11E8);
    CHECK_EQ((int)offsetof(Device, _hasFocus), 0x11F0);
    CHECK_EQ((int)offsetof(Device, _wheelAccum), 0x11F4);
    CHECK_EQ((int)offsetof(Device, _lastCursorX), 0x11F8);
    CHECK_EQ((int)offsetof(Device, _lastCursorY), 0x11FC);
    // The joystick stride every reader multiplies by (`IMUL R8,RDI,0x1b0`).
    CHECK_EQ((int)sizeof(Device::_joystick[0]), 0x1B0);
}

// ---------------------------------------------------------------- PART C
UNIT_TEST(input_declared_signatures) {
    CHECK((std::is_same<decltype(&KeyMap::Pressed),  bool (KeyMap::*)(int32_t, int32_t) const>::value));
    CHECK((std::is_same<decltype(&KeyMap::Released), bool (KeyMap::*)(int32_t, int32_t) const>::value));
    CHECK((std::is_same<decltype(&KeyMap::GetValue), int16_t (KeyMap::*)(int32_t, int32_t) const>::value));
    CHECK((std::is_same<decltype(&KeyMap::RegisterInput),
                        void (KeyMap::*)(int32_t, int32_t, int32_t, const char*, bool)>::value));
    // The two retail overloads, `?ClearInput@KeyMap@Input@@QEAAXHH@Z` and `...HHH@Z`.
    // (Unevaluated operands only: the bodies are not linked into the test.)
    using Clear2 = void (KeyMap::*)(int32_t, int32_t);
    using Clear3 = void (KeyMap::*)(int32_t, int32_t, int32_t);
    CHECK((std::is_same<decltype(static_cast<Clear2>(&KeyMap::ClearInput)), Clear2>::value));
    CHECK((std::is_same<decltype(static_cast<Clear3>(&KeyMap::ClearInput)), Clear3>::value));

    CHECK((std::is_same<decltype(&Device::Update), void (Device::*)(const System::Time&)>::value));
    CHECK((std::is_same<decltype(&Device::UpdateState),
                        void (Device::*)(State*, State*, int32_t, const System::Time&)>::value));
    CHECK((std::is_same<decltype(&Device::EnableMouseClipping), void (Device::*)(uint8_t)>::value));
    CHECK((std::is_same<decltype(&Device::GetValue),
                        int16_t (Device::*)(Device::InputDevice, int32_t, int32_t) const>::value));
    // The callbacks PlatformStartup hands to Application::RegisterMessageCallback.
    using Callback = Application::Application::MessageCallback;
    CHECK((std::is_same<decltype(&Device::OnFocusMessage), Callback>::value));
    CHECK((std::is_same<decltype(&Device::OnLostFocusMessage), Callback>::value));
    CHECK((std::is_same<decltype(&Device::OnMouseWheelMessage), Callback>::value));

    CHECK((std::is_same<decltype(&Configuration::Startup), bool (Configuration::*)()>::value));

    // B7-6: the RtInput:: spellings are the classes, not copies.
    CHECK((std::is_same<RtInput::KeyMap, KeyMap>::value));
    CHECK((std::is_same<RtInput::Configuration, Configuration>::value));
    CHECK((std::is_same<RtInput::State, State>::value));
    CHECK((std::is_same<RtInput::BindingNode, BindingNode>::value));
    CHECK_EQ(RtInput::kMouseOffset, (int)offsetof(Device, _mouse));
    CHECK_EQ(RtInput::kDeviceMappedSize, (int)sizeof(Device));
    CHECK_EQ((int)Device::InputDevice::Keyboard, 1);
}

}  // namespace
