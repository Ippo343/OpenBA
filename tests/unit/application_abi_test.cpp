// ============================================================================
// tests/unit/application_abi_test.cpp — ENDGAME B.3.7 block B1
//   Application::Application as a real, NON-polymorphic class (B7-10).
//
// PART A — size / shape: 0x30 (`MemAlloc(0x30)` in Engine::Startup
//   0x1800dbbb3), no vtable, not default-constructible (the ctor is the door
//   `Ctor_Application`), and `RtApplication::Application` IS the class.
// PART B — the field offsets every ported member reads, from the retail
//   one-liners (IsCursorEnabled `MOVZX EAX,[RCX+0x10]`, IsPaused `[RCX+0x11]`,
//   GetInstance `+0x08`, the message-callback Set at +0x18/+0x20, Quit +0x28).
// PART C — declared signatures that carry an ABI claim: IsCursorEnabled
//   returns the RAW byte (uint8_t, the same AL as retail's `_N`, without C++
//   bool normalisation); WindowProc is a STATIC member with the WNDPROC shape
//   spelled portably; RegisterMessageCallback takes the plain callback pointer.
//   The member bodies live in runtime/src/application (not linked here), so
//   everything below is layout and declared type, not behaviour.
// ============================================================================
#include "unit.h"

#include "runtime/types/application/Application.h"
#include "runtime/types/engine/Engine.h"

#include <cstdint>
#include <cstddef>
#include <type_traits>

namespace {

using App = Application::Application;

// ---------------------------------------------------------------- PART A
UNIT_TEST(application_size_and_shape) {
    CHECK_EQ((int)sizeof(App), 0x30);
    CHECK_EQ((int)alignof(App), 8);
    CHECK(!std::is_polymorphic<App>::value);
    CHECK(!std::is_default_constructible<App>::value);   // the ctor is the door
    CHECK((std::is_same<RtApplication::Application, App>::value));
    // Engine's accessor hands back the retail type.
    CHECK((std::is_same<decltype(std::declval<Engine::Engine&>().GetApplication()), App*>::value));
}

// ---------------------------------------------------------------- PART B
UNIT_TEST(application_field_offsets) {
    CHECK_EQ((int)offsetof(App, _windowHandle), 0x00);          // GetWindow
    CHECK_EQ((int)offsetof(App, _platformHandle), 0x08);        // GetInstance
    CHECK_EQ((int)offsetof(App, _cursorEnabled), 0x10);         // IsCursorEnabled / EnableCursor
    CHECK_EQ((int)offsetof(App, _running), 0x11);               // IsPaused / PlatformIsPaused
    CHECK_EQ((int)offsetof(App, _messageCallbackRoot), 0x18);   // the Set's root
    CHECK_EQ((int)offsetof(App, _messageCallbackCount), 0x20);  // the Set's count
    CHECK_EQ((int)offsetof(App, _quitRequested), 0x28);         // Quit / MainLoop::Frame
    CHECK_EQ((int)offsetof(Engine::Engine, _application), 0x690);
}

// ---------------------------------------------------------------- PART C
UNIT_TEST(application_declared_signatures) {
    CHECK((std::is_same<decltype(&App::IsCursorEnabled), uint8_t (App::*)()>::value));
    CHECK((std::is_same<decltype(&App::WindowProc),
                        int64_t (*)(void*, uint32_t, uint64_t, int64_t)>::value));
    CHECK((std::is_same<decltype(&App::Get), App* (*)()>::value));
    CHECK((std::is_same<App::MessageCallback,
                        int (*)(void*, uint32_t, uint64_t, int64_t)>::value));
    CHECK((std::is_same<decltype(&App::RegisterMessageCallback),
                        void (App::*)(uint32_t, App::MessageCallback)>::value));
    CHECK((std::is_same<decltype(&App::SleepThread), void (App::*)(int32_t)>::value));
    CHECK((std::is_same<decltype(&App::GetInstance), void* (App::*)() const>::value));
}

}  // namespace
