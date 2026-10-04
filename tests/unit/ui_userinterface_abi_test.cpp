// ============================================================================
// tests/unit/ui_userinterface_abi_test.cpp — ENDGAME B.3.6 block E
//   UI::UserInterface: a real, concrete, NON-polymorphic class of 0x128 bytes
//
// PART A — size / alignment / shape. 0x128 is the literal ShowDialog hands the
//   allocator (0x18011fd54); the class has no vtable (the ctor's first store is
//   `_dialogs._head`); it is NOT trivially copyable, because three of its members
//   are Collection::Array<T>, whose destructor is user-declared (the block
//   prompt's is_trivially_copyable assert is false for this layout and for the
//   POD before it); its default ctor is protected (RULE B6-4.5 / B5-4).
// PART B — the two BY-VALUE holders in runtime/types/engine/UpdateLoop.h:
//   `RtEngine::DialogLoopT::_ui` at +0x40 (pinned by
//   ?GetUserInterface@DialogLoopT@Engine@@ 0x18004d7c0) and
//   `RtEngine::DialogLoaderLoopT::_ui` at +0x60.
// PART C — the hidden-retslot DOORS keep retail's `(this, ret, …)` shape while
//   the non-virtual members return by value (B5-12); checked on the declared
//   door types, unevaluated (the doors live in runtime/src/ui, which the unit
//   binary does not link).
// PART D — RULE B6-11: an `InternalPtr<T>` BY-VALUE argument is the ADDRESS of
//   the caller's temporary. `IsDialogOpen` (0x180120410, header-inline) is
//   raw-called in retail order — RCX = this, RDX = &temporary — through its
//   bare code address, exactly as a retail caller reaches it.
// ============================================================================
#include "unit.h"

#include "runtime/types/ui/UserInterface.h"
#include "runtime/types/engine/UpdateLoop.h"
#include "runtime/types/object/Internal.h"

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <new>
#include <type_traits>
#include <utility>

namespace UI { class DialogInstance; class Element; }

extern "C" void* RtUi_UserInterface_GetFirstDialog(UI::UserInterface* self, void* retval);
extern "C" void* RtUi_UserInterface_GetFirstContainedDialog(const UI::UserInterface* self,
                                                            void* retval, int x, int y);
extern "C" void  RtUi_UserInterface_AddDialog(UI::UserInterface* self, void** dlgPtr);

namespace {

using UI::UserInterface;

// ---------------------------------------------------------------- PART A
UNIT_TEST(ui_userinterface_size_align_shape) {
    CHECK_EQ((int)sizeof(UserInterface), 0x128);
    CHECK_EQ((int)alignof(UserInterface), 8);
    CHECK(!std::is_polymorphic<UserInterface>::value);
    CHECK(!std::is_trivially_copyable<UserInterface>::value);
    CHECK(!std::is_default_constructible<UserInterface>::value);   // protected
    CHECK_EQ((int)sizeof(UserInterface::Iterator), 8);
    CHECK_EQ((int)sizeof(UserInterface::ClipRegion), 0x10);
    CHECK_EQ((int)sizeof(UserInterface::Event), 0x30);
}

UNIT_TEST(ui_userinterface_field_offsets) {
    CHECK_EQ((int)offsetof(UserInterface, _dialogs),         0x000);
    CHECK_EQ((int)offsetof(UserInterface, _removedDialogs),  0x018);
    CHECK_EQ((int)offsetof(UserInterface, _addDialogs),      0x028);
    CHECK_EQ((int)offsetof(UserInterface, _toolTipOwner),    0x038);
    CHECK_EQ((int)offsetof(UserInterface, _toolTipDialog),   0x040);
    CHECK_EQ((int)offsetof(UserInterface, _events),          0x048);
    CHECK_EQ((int)offsetof(UserInterface, _dragDropDialog),  0x060);
    CHECK_EQ((int)offsetof(UserInterface, _dragDropActive),  0x088);
    CHECK_EQ((int)offsetof(UserInterface, _audioScene),      0x090);
    CHECK_EQ((int)offsetof(UserInterface, _sounds),          0x098);
    CHECK_EQ((int)offsetof(UserInterface, _cursor),          0x0d0);
    CHECK_EQ((int)offsetof(UserInterface, _uiScale),         0x0dc);
    CHECK_EQ((int)offsetof(UserInterface, _fadeVelocity),    0x0f0);
    CHECK_EQ((int)offsetof(UserInterface, _clipRegion),      0x0fc);
    CHECK_EQ((int)offsetof(UserInterface, _spriteCapture),   0x110);
    CHECK_EQ((int)offsetof(UserInterface, _captureViewport), 0x118);
}

// The header-inline accessors read the fields their one-line retail bodies read.
UNIT_TEST(ui_userinterface_inline_accessors) {
    alignas(8) unsigned char storage[sizeof(UserInterface)];
    std::memset(storage, 0, sizeof storage);
    auto* ui = reinterpret_cast<UserInterface*>(storage);
    ui->SetUIScale(150);                           // 0x180031070: percent / 100
    CHECK(ui->_uiScale == 1.5f);
    CHECK_EQ(ui->UIScaled(100), 150);              // 0x180031090
    CHECK_EQ(ui->UIUnscaled(150), 100);            // 0x1800310b0
    CHECK(!ui->IsFading());
    ui->_fadeVelocity = -1.0f;
    CHECK(ui->IsFading());                         // 0x180031010
    ui->_mouseCaptured = 1;
    CHECK(ui->WasMouseCaptured());                 // 0x180031060
    CHECK(&ui->GetClipRegion() == &ui->_clipRegion);   // 0x180031000
}

// ---------------------------------------------------------------- PART B
UNIT_TEST(ui_userinterface_by_value_holders) {
    CHECK_EQ((int)offsetof(RtEngine::DialogLoopT, _ui), 0x40);
    CHECK_EQ((int)offsetof(RtEngine::DialogLoaderLoopT, _ui), 0x60);
    CHECK_EQ((int)sizeof(RtEngine::DialogLoopT), 0x168);
    CHECK_EQ((int)sizeof(RtEngine::DialogLoaderLoopT), 0x188);
    CHECK((std::is_same<decltype(RtEngine::DialogLoopT::_ui), UI::UserInterface>::value));
    CHECK((std::is_same<decltype(RtEngine::DialogLoaderLoopT::_ui), UI::UserInterface>::value));
}

// ---------------------------------------------------------------- PART C
UNIT_TEST(ui_userinterface_retslot_door_shapes) {
    // The members return by value …
    CHECK((std::is_same<decltype(std::declval<UserInterface&>().GetFirstDialog()),
                        UserInterface::Iterator>::value));
    CHECK((std::is_same<decltype(std::declval<const UserInterface&>().GetFirstContainedDialog(0, 0)),
                        Object::InternalPtr<UI::DialogInstance>>::value));
    CHECK((std::is_same<decltype(std::declval<UserInterface&>().SetClipRegion(0, 0, 0, 0)),
                        UserInterface::ClipRegion>::value));
    // … and the registered doors keep retail's `(this, ret, …)` order and hand
    // the slot back in RAX.
    CHECK((std::is_same<decltype(&RtUi_UserInterface_GetFirstDialog),
                        void* (*)(UI::UserInterface*, void*)>::value));
    CHECK((std::is_same<decltype(&RtUi_UserInterface_GetFirstContainedDialog),
                        void* (*)(const UI::UserInterface*, void*, int, int)>::value));
    // An InternalPtr-argument door takes the caller's temporary BY ADDRESS.
    CHECK((std::is_same<decltype(&RtUi_UserInterface_AddDialog),
                        void (*)(UI::UserInterface*, void**)>::value));
}

// ---------------------------------------------------------------- PART D
// GCC's pointer-to-member for a NON-virtual member is {code address, this
// adjustment}; the first qword is the bare function address. That is the entry
// a retail caller reaches through the door, so calling it with the retail
// register order is the B6-11 proof.
template <typename Fn, typename PMF>
Fn BareAddress(PMF pmf) {
    static_assert(sizeof(PMF) == 16, "Itanium/GCC PMF is two qwords");
    void* code = nullptr;
    std::memcpy(&code, &pmf, sizeof code);
    Fn f;
    std::memcpy(&f, &code, sizeof f);
    return f;
}

UNIT_TEST(ui_userinterface_internalptr_argument_is_by_address) {
    // Why the rule is needed: GCC would pass this 8-byte class IN the register.
    CHECK(std::is_trivially_copyable<Object::InternalPtr<UI::DialogInstance>>::value);
    CHECK_EQ((int)sizeof(Object::InternalPtr<UI::DialogInstance>), 8);

    using ListT = Collection::LinkedList<UI::DialogInstancePtr>;
    alignas(8) unsigned char storage[sizeof(UserInterface)];
    std::memset(storage, 0, sizeof storage);
    auto* ui = reinterpret_cast<UserInterface*>(storage);

    int dlgA = 0, dlgB = 0, dlgC = 0;                // any distinct addresses
    ListT::Node n1{}; ListT::Node n2{};
    n1.element = reinterpret_cast<UI::DialogInstance*>(&dlgA); n1.prev = nullptr; n1.next = &n2;
    n2.element = reinterpret_cast<UI::DialogInstance*>(&dlgB); n2.prev = &n1;     n2.next = nullptr;
    ui->_dialogs._head = &n1;
    ui->_dialogs._tail = &n2;

    using RawFn = bool (*)(const UserInterface* self, void** dialogTemporary);
    RawFn raw = BareAddress<RawFn>(&UserInterface::IsDialogOpen);

    void* tempB = &dlgB;                              // the caller's temporary
    void* tempC = &dlgC;
    CHECK(raw(ui, &tempB));                           // RCX = this, RDX = &temporary
    CHECK(!raw(ui, &tempC));

    // An empty list answers false WITHOUT reading the argument (retail's order:
    // the head test comes first), so a null temporary address is never touched.
    ui->_dialogs._head = nullptr;
    CHECK(!raw(ui, nullptr));
}

}  // namespace
