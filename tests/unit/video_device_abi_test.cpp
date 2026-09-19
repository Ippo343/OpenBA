// ============================================================================
// tests/unit/video_device_abi_test.cpp — ENDGAME B.3.5 block D2.
//
// The ABI claims block D2 makes, and nothing else. Model:
// `tests/unit/video_data_abi_test.cpp` (block A) and `io_vtable_abi_test.cpp`.
//
// WHAT THIS FILE HAS TO ESTABLISH
// -------------------------------
// Block D2 declares nine real C++ classes over layouts the rest of the tree —
// and `dx11/`, and `standalone/video_interface.cpp`, and
// `code/Game/Dialog/optionsdialog.cpp` — still reach by RAW BYTE OFFSET:
//
//   Video::Device                      0x108, lock at +0x60, dllSlots at +0xb0,
//                                      deviceData at +0x100
//   Video::Device::{RefreshRate, DisplayMode, Adapter, GraphicsInterface,
//                   InterfaceSlot}     8 / 0x18 / 0x38 / 0x18 / 0x18 strides
//   Video::Configuration               0x44 INFERRED — see PART B
//   Video::RenderBuffer                0x30
//   Video::{Index,Vertex}Buffer        0x20 each
//
// None of them is polymorphic, so the whole risk is the opposite of block A's:
// not "does GCC put slot N at N*8", but "did giving these PODs member functions,
// nested classes and a namespace change one single byte of their layout". That is
// what PART A and PART C assert, and PART C asserts it in the form that would
// actually break — a vptr appearing at +0x00 and shifting every field by eight.
//
// WHY NO TREE SOURCE LINKS. Every ported body in this block stands on something a
// host test cannot have: `runtime/src/video/device.cpp` on `RT_DATA_SLOT` and
// `IO::Persistence`, `device_startup.cpp` on the exe's `ResolveAddr` and the
// Application singleton, `video_interfaces_win.cpp` and the three `_win` twins on
// `<windows.h>`, `buffers.cpp` and `dynamic_resources.cpp` on the port registry
// and dx11's live vtables. So the real classes' LAYOUT is checked directly
// (`sizeof`/`offsetof`/`alignof` need no definition and no instantiation) and the
// two ABI SHAPES the block relies on — the qualified non-virtual call and the
// hidden-return-slot door — are checked on classes declared here in the same
// shape, exactly as `io_vtable_abi_test.cpp` does with `TestReaderWriter`.
// A mirror of a body tests itself; a mirror of a CODEGEN RULE tests the compiler,
// which is the thing in question.
// ============================================================================
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <new>
#include <type_traits>
#include <utility>

#include "unit.h"

#include "video/Device.h"
#include "video/Buffers.h"

namespace {

using Dev   = Video::Device;
using Cfg   = Video::Configuration;
using RB    = Video::RenderBuffer;
using IB    = Video::IndexBuffer;
using VB    = Video::VertexBuffer;

// Raw byte access, which is exactly how dx11, standalone/video_interface.cpp and
// optionsdialog.cpp reach these objects.
template <typename T>
T& At(void* base, size_t off) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + off);
}

}  // namespace

// ===========================================================================
// PART A — the layouts, byte for byte.
//
// These are compile-time in the headers too. They are restated at RUNTIME
// because a header assert can be deleted in a "tidy" and nothing fails, whereas
// a failing test names the class and the offset.
// ===========================================================================

UNIT_TEST(video_device_layout) {
    CHECK_EQ(sizeof(Dev), 0x108u);
    CHECK_EQ(alignof(Dev), 8u);

    CHECK_EQ(offsetof(Dev, type), 0x00u);
    CHECK_EQ(offsetof(Dev, backBufferWidth), 0x04u);
    CHECK_EQ(offsetof(Dev, backBufferHeight), 0x08u);
    CHECK_EQ(offsetof(Dev, displayModeIndex), 0x0Cu);
    CHECK_EQ(offsetof(Dev, refreshRateIndex), 0x10u);
    CHECK_EQ(offsetof(Dev, modeData), 0x20u);
    CHECK_EQ(offsetof(Dev, multisampleData), 0x30u);
    CHECK_EQ(offsetof(Dev, interfaceData), 0x40u);
    CHECK_EQ(offsetof(Dev, interfaceCount), 0x48u);
    CHECK_EQ(offsetof(Dev, windowHandle), 0x90u);
    CHECK_EQ(offsetof(Dev, maxAnisotropy), 0x98u);
    CHECK_EQ(offsetof(Dev, settingsChanged), 0x9Cu);
}

// The three offsets that are a CONTRACT WITH A FILE THAT CANNOT BE TOLD THE
// CLASS MOVED, restated here so the contract is a test and not a comment:
//   +0xb0  standalone/video_interface.cpp's kSlot0, stride 0x18 (it writes our
//          own CreateDeviceData/GetDeviceName into slot 1 by hand)
//   +0xf8  standalone/main.cpp's active-interface index
//   +0x100 dx11/src/*.cpp's DeviceData*
UNIT_TEST(video_device_offsets_other_files_hardcode) {
    CHECK_EQ(offsetof(Dev, dllSlots), 0xB0u);
    CHECK_EQ(sizeof(Dev::InterfaceSlot), 0x18u);
    CHECK_EQ(offsetof(Dev::InterfaceSlot, module), 0x00u);
    CHECK_EQ(offsetof(Dev::InterfaceSlot, getDeviceName), 0x08u);
    CHECK_EQ(offsetof(Dev::InterfaceSlot, createDeviceData), 0x10u);
    CHECK_EQ(offsetof(Dev, activeInterface), 0xF8u);
    CHECK_EQ(offsetof(Dev, deviceData), 0x100u);
}

UNIT_TEST(video_device_nested_value_type_strides) {
    // Every one of these is a STRIDE some other body walks with, which is why
    // the size matters as much as the offsets.
    CHECK_EQ(sizeof(Dev::RefreshRate), 0x08u);
    CHECK_EQ(offsetof(Dev::RefreshRate, numerator), 0x00u);
    CHECK_EQ(offsetof(Dev::RefreshRate, denominator), 0x04u);

    CHECK_EQ(sizeof(Dev::DisplayMode), 0x18u);
    CHECK_EQ(offsetof(Dev::DisplayMode, width), 0x00u);
    CHECK_EQ(offsetof(Dev::DisplayMode, height), 0x04u);
    CHECK_EQ(offsetof(Dev::DisplayMode, refreshData), 0x08u);
    CHECK_EQ(offsetof(Dev::DisplayMode, refreshCount), 0x10u);

    CHECK_EQ(sizeof(Dev::Adapter), 0x38u);
    CHECK_EQ(offsetof(Dev::Adapter, name), 0x00u);
    CHECK_EQ(offsetof(Dev::Adapter, modeData), 0x08u);
    CHECK_EQ(offsetof(Dev::Adapter, modeCount), 0x10u);
    CHECK_EQ(offsetof(Dev::Adapter, multisampleData), 0x18u);
    CHECK_EQ(offsetof(Dev::Adapter, multisampleCount), 0x20u);
    CHECK_EQ(offsetof(Dev::Adapter, maxFilterLevel), 0x28u);

    CHECK_EQ(sizeof(Dev::GraphicsInterface), 0x18u);
    CHECK_EQ(offsetof(Dev::GraphicsInterface, name), 0x00u);
    CHECK_EQ(offsetof(Dev::GraphicsInterface, adapterData), 0x08u);
    CHECK_EQ(offsetof(Dev::GraphicsInterface, adapterCount), 0x10u);
}

UNIT_TEST(video_renderbuffer_and_the_two_buffers_layout) {
    CHECK_EQ(sizeof(RB), 0x30u);
    CHECK_EQ(offsetof(RB, colorBuffer), 0x00u);
    CHECK_EQ(offsetof(RB, depthBuffer), 0x08u);
    CHECK_EQ(offsetof(RB, width), 0x10u);
    CHECK_EQ(offsetof(RB, height), 0x14u);
    CHECK_EQ(offsetof(RB, next), 0x18u);
    CHECK_EQ(offsetof(RB, prev), 0x20u);
    CHECK_EQ(offsetof(RB, platformData), 0x28u);

    CHECK_EQ(sizeof(IB), 0x20u);
    CHECK_EQ(sizeof(VB), 0x20u);
    CHECK_EQ(offsetof(IB, size), 0x00u);
    CHECK_EQ(offsetof(IB, dynamic), 0x04u);
    CHECK_EQ(offsetof(IB, platformData), 0x08u);
    CHECK_EQ(offsetof(IB, listPrev), 0x10u);
    CHECK_EQ(offsetof(IB, listNext), 0x18u);
    // Layout-identical, which is what lets dynamic_resources.cpp drive both
    // through one template.
    CHECK_EQ(offsetof(VB, size), offsetof(IB, size));
    CHECK_EQ(offsetof(VB, dynamic), offsetof(IB, dynamic));
    CHECK_EQ(offsetof(VB, platformData), offsetof(IB, platformData));
    CHECK_EQ(offsetof(VB, listPrev), offsetof(IB, listPrev));
    CHECK_EQ(offsetof(VB, listNext), offsetof(IB, listNext));
}

// ===========================================================================
// PART B — Video::Configuration, and the ONE size in this block that is NOT
// measured.
//
// B35_RULES §13 item 1 asked block D2 to close the `sizeof == 0x44` gap from the
// retail DLL. It could not be closed, and this test states the bound that IS
// established rather than a number that is not:
//   * the constructor's last store is `MOV byte ptr [RCX+0x42],0` at 0x1800f6f64;
//   * no body in the DLL (ctor, Save, ValidateConfiguration,
//     ApplyChangedSettings) touches a byte above +0x42;
//   * there is NO allocation site — the object is a static reached only through
//     an indirect call, so no size immediate and no next-symbol bound exist.
// So 0x43 <= sizeof <= 0x44, and asserting == 0x44 would be a static_assert that
// cannot fail for the right reason.
// ===========================================================================

UNIT_TEST(video_configuration_size_is_bounded_not_measured) {
    CHECK(sizeof(Cfg) >= 0x43u);
    CHECK(sizeof(Cfg) <= 0x44u);
    CHECK_EQ(alignof(Cfg), 4u);
    // The ctor's last store, which is the only hard fact about the tail.
    CHECK_EQ(offsetof(Cfg, field42), 0x42u);
}

UNIT_TEST(video_configuration_eighteen_offsets) {
    CHECK_EQ(offsetof(Cfg, settingsValid), 0x00u);
    CHECK_EQ(offsetof(Cfg, interfaceIndex), 0x04u);
    CHECK_EQ(offsetof(Cfg, adapterIndex), 0x08u);
    CHECK_EQ(offsetof(Cfg, width), 0x0Cu);
    CHECK_EQ(offsetof(Cfg, height), 0x10u);
    CHECK_EQ(offsetof(Cfg, windowX), 0x14u);
    CHECK_EQ(offsetof(Cfg, windowY), 0x18u);
    CHECK_EQ(offsetof(Cfg, windowWidth), 0x1Cu);
    CHECK_EQ(offsetof(Cfg, windowHeight), 0x20u);
    CHECK_EQ(offsetof(Cfg, multisample), 0x24u);
    CHECK_EQ(offsetof(Cfg, textureFilter), 0x28u);
    CHECK_EQ(offsetof(Cfg, shadowScalar), 0x2Cu);
    CHECK_EQ(offsetof(Cfg, shaderDetail), 0x30u);
    CHECK_EQ(offsetof(Cfg, reflectionDetail), 0x34u);
    CHECK_EQ(offsetof(Cfg, refreshNumerator), 0x38u);
    CHECK_EQ(offsetof(Cfg, refreshDenominator), 0x3Cu);
    CHECK_EQ(offsetof(Cfg, fullscreen), 0x40u);
    CHECK_EQ(offsetof(Cfg, vsync), 0x41u);
}

// ===========================================================================
// PART C — RULE B5-6, and the failure mode that would actually happen.
//
// The classes gained member functions, five nested classes and a namespace. The
// one thing that would silently destroy every raw reader in the tree is a VPTR
// appearing at +0x00. `sizeof` alone would catch it for `Device` (0x108 -> 0x110)
// but NOT for a class whose tail padding could absorb it, so this is asserted
// three ways: standard-layout, trivially-copyable, and a direct byte read of the
// first field through a `uint8_t*`.
// ===========================================================================

UNIT_TEST(video_device_gained_no_vptr_and_stays_a_raw_readable_object) {
    CHECK(std::is_standard_layout<Dev>::value);
    CHECK(std::is_trivially_copyable<Dev>::value);
    CHECK(std::is_standard_layout<Cfg>::value);
    CHECK(std::is_trivially_copyable<Cfg>::value);
    CHECK(std::is_standard_layout<RB>::value);
    CHECK(std::is_trivially_copyable<RB>::value);
    CHECK(std::is_standard_layout<IB>::value);
    CHECK(std::is_trivially_copyable<VB>::value);

    // The five nested value types too — Collection::Array bulk-copies them.
    CHECK(std::is_trivially_copyable<Dev::RefreshRate>::value);
    CHECK(std::is_trivially_copyable<Dev::DisplayMode>::value);
    CHECK(std::is_trivially_copyable<Dev::Adapter>::value);
    CHECK(std::is_trivially_copyable<Dev::GraphicsInterface>::value);
    CHECK(std::is_trivially_copyable<Dev::InterfaceSlot>::value);

    alignas(8) uint8_t raw[sizeof(Dev)];
    std::memset(raw, 0, sizeof(raw));
    Dev* d = new (raw) Dev;

    // Write through the CLASS, read through raw bytes — the direction dx11 and
    // standalone/video_interface.cpp use.
    d->type             = 0x11223344;
    d->interfaceCount   = 7;
    d->activeInterface  = 1;
    d->deviceData       = reinterpret_cast<void*>(static_cast<uintptr_t>(0xDEADBEEFu));
    CHECK_EQ(At<int32_t>(raw, 0x00), 0x11223344);
    CHECK_EQ(At<int32_t>(raw, 0x48), 7);
    CHECK_EQ(At<int32_t>(raw, 0xF8), 1);
    CHECK_EQ(At<uintptr_t>(raw, 0x100), static_cast<uintptr_t>(0xDEADBEEFu));

    // And the other direction: write raw, read through the class.
    At<int32_t>(raw, 0x98) = 16;                       // maxAnisotropy
    CHECK_EQ(d->maxAnisotropy, 16);
    At<int32_t>(raw, 0x04) = 1920;
    At<int32_t>(raw, 0x08) = 1080;
    CHECK_EQ(d->backBufferWidth, 1920);
    CHECK_EQ(d->backBufferHeight, 1080);
}

// RULE B5-6 in its own right: the CRITICAL_SECTION stays an opaque 0x28-byte
// block with the owner-tid and depth words at PINNED offsets, because dx11 and
// the four `Platform{Construct,Destruct}` bodies read them there. A `std::mutex`
// or a scoped guard would move or resize this.
UNIT_TEST(video_device_lock_is_opaque_and_pinned_rule_b5_6) {
    CHECK_EQ(offsetof(Dev, lock), 0x60u);
    CHECK_EQ(sizeof(Dev::lock), 0x28u);
    CHECK_EQ(offsetof(Dev, lockOwnerThreadId), 0x88u);
    CHECK_EQ(offsetof(Dev, lockRecursionCount), 0x8Cu);
    // The two bookkeeping words sit immediately after the block, with no
    // padding: 0x60 + 0x28 == 0x88.
    CHECK_EQ(offsetof(Dev, lock) + sizeof(Dev::lock), offsetof(Dev, lockOwnerThreadId));
    // Writing the depth through raw bytes is visible as the member, which is
    // what buffers_win.cpp's hand-rolled recursive acquire does.
    alignas(8) uint8_t raw[sizeof(Dev)];
    std::memset(raw, 0, sizeof(raw));
    Dev* d = new (raw) Dev;
    At<int32_t>(raw, 0x8C) = 3;
    CHECK_EQ(d->lockRecursionCount, 3);
}

// The two fields whose MEANING this block recovered from dx11's side. The values
// are DXGI enumeration coordinates, and dx11/src/devicedata.cpp reads the Device
// copies back at exactly these offsets (`Dev::kAdapterIndex = 0x014`,
// `Dev::kOutputIndex = 0x018`) to call EnumAdapters1 / EnumOutputs. Restated
// here so a rename on either side breaks a test rather than a frame.
UNIT_TEST(video_device_dxgi_coordinates_match_the_dx11_constants) {
    CHECK_EQ(offsetof(Dev, dxgiAdapterIndex), 0x14u);
    CHECK_EQ(offsetof(Dev, dxgiOutputIndex), 0x18u);
    CHECK_EQ(offsetof(Dev::Adapter, dxgiAdapterIndex), 0x2Cu);
    CHECK_EQ(offsetof(Dev::Adapter, dxgiOutputIndex), 0x30u);
}

// ===========================================================================
// PART D — ONE DEFINITION, NOT TWO.
//
// The old PODs were field-identical to the retail classes, so block D2 made the
// old spellings ALIASES instead of writing a second layout. That is the claim
// that keeps four other blocks' files compiling untouched, and it is the claim a
// later "restore the POD" edit would break silently — a duplicate would still
// compile and still pass every offset test above.
// ===========================================================================

UNIT_TEST(video_device_pod_spellings_are_the_same_type_not_a_copy) {
    CHECK((std::is_same<RtVideo::Device, Video::Device>::value));
    CHECK((std::is_same<RtVideo::Configuration, Video::Configuration>::value));
    CHECK((std::is_same<RtVideo::RenderBuffer, Video::RenderBuffer>::value));
    CHECK((std::is_same<RtVideo::RefreshRate, Video::Device::RefreshRate>::value));
    CHECK((std::is_same<RtVideo::DisplayMode, Video::Device::DisplayMode>::value));
    CHECK((std::is_same<RtVideo::Adapter, Video::Device::Adapter>::value));
    CHECK((std::is_same<RtVideo::GraphicsInterface, Video::Device::GraphicsInterface>::value));
    CHECK((std::is_same<RtVideo::InterfaceSlot, Video::Device::InterfaceSlot>::value));
    // Buffers.h's old GLOBAL-scope spelling (B4b-1: it occupied no namespace at
    // all) and the RtVideo one.
    CHECK((std::is_same<::IndexBuffer, Video::IndexBuffer>::value));
    CHECK((std::is_same<::VertexBuffer, Video::VertexBuffer>::value));
    CHECK((std::is_same<RtVideo::IndexBuffer, Video::IndexBuffer>::value));
    CHECK((std::is_same<RtVideo::VertexBuffer, Video::VertexBuffer>::value));
}

// ===========================================================================
// PART E — the two ABI shapes the block's doors rely on, on mirror classes.
//
// E1: THE QUALIFIED NON-VIRTUAL CALL. Every door in this block is
// `reinterpret_cast<Ns::C*>(self)->Ns::C::Method(args)`. What has to be true is
// that on a NON-POLYMORPHIC class that call needs no vtable, adjusts `this` by
// zero, and reaches the body even when the receiver was never constructed as a
// C++ object — which is the case for every live `Video::Device` (dx11's factory
// MemAllocs it and the retail-shaped ctor door fills it).
// ===========================================================================

namespace {

struct MirrorNonPoly {
    int32_t a;      // +0x00
    int32_t b;      // +0x04
    void*   p;      // +0x08
    int Sum() const { return a + b; }
    void Bump(int by) { a += by; }
};
static_assert(sizeof(MirrorNonPoly) == 0x10, "the mirror is the shape under test");

}  // namespace

UNIT_TEST(video_device_qualified_call_on_an_unconstructed_receiver) {
    alignas(8) uint8_t raw[sizeof(MirrorNonPoly)];
    // Deliberately NOT placement-new'd: filled the way a MemAlloc'd retail
    // object is, by writing its bytes.
    std::memset(raw, 0, sizeof(raw));
    *reinterpret_cast<int32_t*>(raw + 0) = 40;
    *reinterpret_cast<int32_t*>(raw + 4) = 2;

    MirrorNonPoly* m = reinterpret_cast<MirrorNonPoly*>(raw);
    CHECK_EQ(m->MirrorNonPoly::Sum(), 42);
    m->MirrorNonPoly::Bump(-2);
    CHECK_EQ(*reinterpret_cast<int32_t*>(raw + 0), 38);

    // `this` is the object's own address, unadjusted — no vptr, nothing added.
    CHECK_EQ(reinterpret_cast<const void*>(m), reinterpret_cast<const void*>(raw));
}

// ===========================================================================
// E2: THE HIDDEN-RETURN-SLOT DOOR, RULE B5-12 as amended.
//
// `Device::GetDisplayMode` and `Device::GetDLLForInterface` return a class BY
// VALUE. GCC's ms_abi and MSVC do NOT agree on where the hidden pointer goes
// relative to `this`, so the amendment's rule is: the MEMBER may return by value
// only because the REGISTERED DOOR keeps retail's explicit `(this, ret, args)`
// signature, placement-constructs the member's prvalue into the caller's slot and
// returns that slot pointer. Both of this block's retslot doors are written that
// way; this test proves the shape does what it claims:
//   * the caller's slot is never READ (it arrives uninitialised, and C++17
//     guarantees the prvalue is built in place rather than copied in);
//   * the door returns exactly the slot it was handed, which is retail's
//     `MOV RAX,RDI`.
// ===========================================================================

namespace {

struct MirrorByValue {
    int32_t x;
    int32_t y;
    void*   owned;
};

int g_mirrorCtorCount = 0;

// The MEMBER: returns by value, deep-copying an owned pointer the way
// `Device::GetDisplayMode` deep-copies its refresh array.
MirrorByValue MirrorMake(int x, int y, void* owned) {
    ++g_mirrorCtorCount;
    MirrorByValue out;
    out.x = x;
    out.y = y;
    out.owned = owned;
    return out;
}

// The DOOR, in the block's shape.
MirrorByValue* MirrorDoor(void* ret, int x, int y, void* owned) {
    return new (ret) MirrorByValue(MirrorMake(x, y, owned));
}

}  // namespace

UNIT_TEST(video_device_retslot_door_builds_in_place_and_returns_the_slot) {
    // An UNINITIALISED slot, poisoned so a read of it would be visible.
    alignas(8) uint8_t slot[sizeof(MirrorByValue)];
    std::memset(slot, 0xCD, sizeof(slot));

    g_mirrorCtorCount = 0;
    void* owned = reinterpret_cast<void*>(static_cast<uintptr_t>(0x1234u));
    MirrorByValue* got = MirrorDoor(slot, 1920, 1080, owned);

    // Retail's MOV RAX,RDI: the door hands back the slot it was given.
    CHECK_EQ(reinterpret_cast<const void*>(got), reinterpret_cast<const void*>(slot));
    CHECK_EQ(got->x, 1920);
    CHECK_EQ(got->y, 1080);
    CHECK_EQ(reinterpret_cast<uintptr_t>(got->owned), static_cast<uintptr_t>(0x1234u));
    // Built once, in place — not built then copied.
    CHECK_EQ(g_mirrorCtorCount, 1);
    // The poison is gone from every byte the value covers, which is what "the
    // slot is written, never read" looks like from outside.
    CHECK_EQ(*reinterpret_cast<const int32_t*>(slot + 0), 1920);
    CHECK_EQ(*reinterpret_cast<const int32_t*>(slot + 4), 1080);
}

// The member's own return type is the class, by value, and the class is the same
// type the door writes — the two cannot drift.
UNIT_TEST(video_device_getdisplaymode_returns_the_nested_class_by_value) {
    CHECK((std::is_same<decltype(std::declval<const Dev&>().GetDisplayMode(0)),
                        Dev::DisplayMode>::value));
    // A `DisplayMode` is trivially copyable, so the deep copy the member makes is
    // handed back by a bitwise move of {width, height, data, count, capacity} —
    // which transfers ownership of the refresh array, as retail's does.
    CHECK(std::is_trivially_copyable<Dev::DisplayMode>::value);
}
