// ============================================================================
// tests/unit/engine_loop_abi_test.cpp — ENDGAME B.3.7 block B1
//   Engine::UpdateLoop / LoaderLoop / DialogLoopT / DialogLoaderLoopT / Loader,
//   Engine::MainLoop and Engine::Engine as real classes.
//
// PART A — SIZE / ALIGNMENT / SHAPE, read against B37_SLOTS.md sec. 1 / 1a and
//   B37_RULES sec. 2.3: the four loops are polymorphic and abstract (LoaderLoop
//   abstract-by-policy, B7-13), Loader is a concrete System::Thread subclass,
//   MainLoop and Engine are NOT polymorphic. Engine's 0x6f8 is the byte count
//   `??4Engine@0@` (0x18000c7d0) copies.
// PART B — RETAIL SLOT ORDER, measured WITHOUT an object: MinGW GCC uses the
//   Itanium C++ ABI for pointers to members, where a pointer to a VIRTUAL
//   member function stores `1 + vtable byte offset` in its first word. So
//   `&Engine::UpdateLoop::Load` must read back as slot 6. This needs no vtable
//   emitted and no body linked (runtime/src/engine is not in the unit binary),
//   which is exactly why it is the instrument here. B7-1: slot 1 is Startup
//   (the loops are their own root, not Object::Base); slot 12 is new on the two
//   Dialog loops; Loader's slot 0 is Run (no deleting-dtor slot).
// PART C — THE RETSLOT VIRTUAL. Slot 12 returns `InternalPtr<DialogInstance>`
//   BY VALUE through a vtable slot, so it is declared retslot-FIRST (B6-2): the
//   declared type is checked, and a probe shows a retail-order caller (RCX =
//   this, RDX = the caller's slot) reaches the right storage.
// PART D — ONE DEFINITION PER LAYOUT (B7-6): the classes and the `RtEngine::`
//   PODs cross-asserted field for field; `RtEngine::MainLoop` and
//   `RtEngine::Engine` ARE the classes; the exe-side mirrors in engine/types
//   (UpdateLoop, LoaderLoop, DialogLoaderLoopT, MainLoop, GameUpdateLoop's base
//   prefix) name the same bytes. The engine/types headers keep their `m_`
//   spellings until the caller round renames their users.
// ============================================================================
#include "unit.h"

#include "runtime/types/engine/UpdateLoop.h"
#include "runtime/types/engine/MainLoop.h"
#include "runtime/types/engine/Engine.h"
#include "runtime/types/object/Internal.h"

// The exe-side mirrors (global structs), measured against the runtime classes.
#include "engine/types/UpdateLoop.h"
#include "engine/types/LoaderLoop.h"
#include "engine/types/DialogLoaderLoopT.h"
#include "engine/types/MainLoop.h"
#include "engine/types/GameUpdateLoop.h"

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <type_traits>

namespace {

// Itanium pointer-to-virtual-member: { ptr = 1 + vtable_byte_offset, adj }.
template <typename PMF>
long SlotOf(PMF pmf) {
    static_assert(sizeof(PMF) == 2 * sizeof(void*), "Itanium PMF is two words");
    uintptr_t words[2];
    std::memcpy(words, &pmf, sizeof(words));
    // (A NON-virtual member's pointer names its body, which this binary does
    // not link -- so only virtual members are measured here.)
    if ((words[0] & 1u) == 0) return -1;           // not virtual
    return static_cast<long>((words[0] - 1) / sizeof(void*));
}

// ---------------------------------------------------------------- PART A
UNIT_TEST(engine_loop_sizes_and_shape) {
    CHECK_EQ((int)sizeof(Engine::UpdateLoop), 0x28);
    CHECK_EQ((int)sizeof(Engine::LoaderLoop), 0x48);
    CHECK_EQ((int)sizeof(Engine::DialogLoopT), 0x168);
    CHECK_EQ((int)sizeof(Engine::DialogLoaderLoopT), 0x188);
    CHECK_EQ((int)sizeof(Engine::Loader), 0x38);
    CHECK_EQ((int)alignof(Engine::DialogLoaderLoopT), 8);

    CHECK(std::is_polymorphic<Engine::UpdateLoop>::value);
    CHECK(std::is_abstract<Engine::UpdateLoop>::value);
    CHECK(std::is_abstract<Engine::LoaderLoop>::value);          // B7-13, by policy
    CHECK(std::is_abstract<Engine::DialogLoopT>::value);
    CHECK(std::is_abstract<Engine::DialogLoaderLoopT>::value);
    CHECK((std::is_base_of<Engine::UpdateLoop, Engine::LoaderLoop>::value));
    CHECK((std::is_base_of<Engine::LoaderLoop, Engine::DialogLoaderLoopT>::value));
    CHECK((std::is_base_of<Engine::UpdateLoop, Engine::DialogLoopT>::value));
    CHECK(!(std::is_base_of<Engine::LoaderLoop, Engine::DialogLoopT>::value));

    // Loader: the REAL System::Thread is its base, not a POD member.
    CHECK((std::is_base_of<System::Thread, Engine::Loader>::value));
    CHECK(!std::is_abstract<Engine::Loader>::value);
    CHECK(!std::is_default_constructible<Engine::Loader>::value);   // protected (B4b-2)

    // No virtual destructor anywhere in the family (ENDGAME B.2).
    CHECK(!std::has_virtual_destructor<Engine::UpdateLoop>::value);
    CHECK(!std::has_virtual_destructor<Engine::DialogLoaderLoopT>::value);
    CHECK(!std::has_virtual_destructor<Engine::Loader>::value);
}

UNIT_TEST(engine_mainloop_and_engine_shape) {
    CHECK_EQ((int)sizeof(Engine::MainLoop), 0xa0);
    CHECK_EQ((int)alignof(Engine::MainLoop), 8);
    CHECK(!std::is_polymorphic<Engine::MainLoop>::value);
    CHECK_EQ((int)sizeof(Engine::Engine), 0x6f8);                // ??4Engine@0@ copies 0x6f8
    CHECK_EQ((int)alignof(Engine::Engine), 8);
    CHECK(!std::is_polymorphic<Engine::Engine>::value);
    CHECK(!std::is_default_constructible<Engine::Engine>::value);  // protected (B5-4)
    CHECK_EQ((int)offsetof(Engine::Engine, _allocator), 0);        // GetMemoryAllocator is `MOV RAX,RCX`
    CHECK_EQ((int)offsetof(Engine::Engine, _videoDevice), 0x6c0);  // Loader::Run's `MOV RAX,[RSI+0x6c0]`
    CHECK_EQ((int)offsetof(Engine::Engine, _noConfigure), 0x6f1);
    CHECK_EQ((int)offsetof(Engine::MainLoop, _currentLoop), 0x20);
    CHECK_EQ((int)offsetof(Engine::MainLoop, _nextLoop), 0x28);    // SetNextLoop: MOV [RCX+0x28],RDX
    CHECK_EQ((int)offsetof(Engine::MainLoop, _engine), 0x78);
    CHECK((std::is_same<decltype(Engine::MainLoop::_currentLoop), Engine::UpdateLoop*>::value));
    CHECK((std::is_same<decltype(Engine::MainLoop::_engine), Engine::Engine*>::value));
}

// ---------------------------------------------------------------- PART B
UNIT_TEST(engine_updateloop_retail_slot_order) {
    // B37_SLOTS.md sec. 1, ??_7UpdateLoop@Engine@@6B@ 0x180155ba0.
    CHECK_EQ(SlotOf(&Engine::UpdateLoop::DeletingDtor), 0L);
    CHECK_EQ(SlotOf(&Engine::UpdateLoop::Startup), 1L);            // NOT Object::Base::TypeName
    CHECK_EQ(SlotOf(&Engine::UpdateLoop::Shutdown), 2L);
    CHECK_EQ(SlotOf(&Engine::UpdateLoop::Update), 3L);
    CHECK_EQ(SlotOf(&Engine::UpdateLoop::Draw), 4L);
    CHECK_EQ(SlotOf(&Engine::UpdateLoop::DrawFailed), 5L);
    CHECK_EQ(SlotOf(&Engine::UpdateLoop::Load), 6L);
    CHECK_EQ(SlotOf(&Engine::UpdateLoop::SaveRestore), 7L);
    CHECK_EQ(SlotOf(&Engine::UpdateLoop::OnVideoError), 8L);
    CHECK_EQ(SlotOf(&Engine::UpdateLoop::OnVideoRestart), 9L);
    CHECK_EQ(SlotOf(&Engine::UpdateLoop::GetResourceFilename), 10L);
    CHECK_EQ(SlotOf(&Engine::UpdateLoop::AllowsModChanges), 11L);
}

UNIT_TEST(engine_derived_loop_slot_order) {
    // The overrides keep their base slots ...
    CHECK_EQ(SlotOf(&Engine::LoaderLoop::Startup), 1L);
    CHECK_EQ(SlotOf(&Engine::LoaderLoop::Load), 6L);
    CHECK_EQ(SlotOf(&Engine::LoaderLoop::OnVideoError), 8L);
    CHECK_EQ(SlotOf(&Engine::DialogLoopT::Draw), 4L);
    CHECK_EQ(SlotOf(&Engine::DialogLoaderLoopT::Update), 3L);
    CHECK_EQ(SlotOf(&Engine::DialogLoaderLoopT::Shutdown), 2L);
    // ... DialogLoaderLoopT inherits LoaderLoop's OnVideoError (B7-1.3) ...
    CHECK_EQ(SlotOf(&Engine::DialogLoaderLoopT::OnVideoError), 8L);
    // ... and slot 12 is NEW on each Dialog loop.
    CHECK_EQ(SlotOf(&Engine::DialogLoopT::CreateLoopDialog), 12L);
    CHECK_EQ(SlotOf(&Engine::DialogLoaderLoopT::CreateLoopDialog), 12L);
    // Loader: ??_7Loader@Engine@@6B@ 0x180157a08 -- [0] Run, [1] OnCompleted.
    CHECK_EQ(SlotOf(&Engine::Loader::Run), 0L);
    CHECK_EQ(SlotOf(&Engine::Loader::OnCompleted), 1L);
}

// ---------------------------------------------------------------- PART C
// A probe with the retslot-first shape: `this` first, the caller's storage
// second, the storage back in RAX. Called here in RETAIL order through a raw
// two-argument function pointer, as MainLoop-era hand-inlined sites did.
struct RetslotProbe {
    virtual Object::InternalPtr<UI::DialogInstance>* Slot0(
        Object::InternalPtr<UI::DialogInstance>* ret);
    void* stamped = nullptr;
};
Object::InternalPtr<UI::DialogInstance>* RetslotProbe::Slot0(
        Object::InternalPtr<UI::DialogInstance>* ret) {
    *reinterpret_cast<void**>(ret) = stamped;
    return ret;
}

UNIT_TEST(engine_dialog_slot12_is_declared_retslot_first) {
    using Ret = Object::InternalPtr<UI::DialogInstance>;
    CHECK((std::is_same<decltype(&Engine::DialogLoopT::CreateLoopDialog),
                        Ret* (Engine::DialogLoopT::*)(Ret*)>::value));
    CHECK((std::is_same<decltype(&Engine::DialogLoaderLoopT::CreateLoopDialog),
                        Ret* (Engine::DialogLoaderLoopT::*)(Ret*)>::value));

    RetslotProbe probe;
    int marker = 0;
    probe.stamped = &marker;
    void* slot = nullptr;
    using Raw = void* (*)(void* self, void* retSlot);                  // RCX, RDX
    Raw raw = reinterpret_cast<Raw>((*reinterpret_cast<void***>(&probe))[0]);
    void* back = raw(&probe, &slot);
    CHECK(back == &slot);                                           // RAX = the caller's slot
    CHECK(slot == &marker);                                         // written THROUGH it
    CHECK(probe.stamped == &marker);                                // `this` untouched
}

// ---------------------------------------------------------------- PART D
#define PIN(C, f) CHECK_EQ((int)offsetof(Engine::C, f), (int)offsetof(RtEngine::C, f))
UNIT_TEST(engine_loop_class_pod_cross_asserts) {
    CHECK_EQ((int)sizeof(Engine::UpdateLoop), (int)sizeof(RtEngine::UpdateLoop));
    PIN(UpdateLoop, _resourceList);  PIN(UpdateLoop, _loadBit);  PIN(UpdateLoop, _loadProgress);
    CHECK_EQ((int)sizeof(Engine::LoaderLoop), (int)sizeof(RtEngine::LoaderLoop));
    PIN(LoaderLoop, _nextLoop);  PIN(LoaderLoop, _elapsedTicks);  PIN(LoaderLoop, _delayTicks);
    PIN(LoaderLoop, _autoAdvance);
    CHECK_EQ((int)sizeof(Engine::DialogLoopT), (int)sizeof(RtEngine::DialogLoopT));
    PIN(DialogLoopT, _dialogFilename);  PIN(DialogLoopT, _dialogInstance);  PIN(DialogLoopT, _ui);
    CHECK_EQ((int)sizeof(Engine::DialogLoaderLoopT), (int)sizeof(RtEngine::DialogLoaderLoopT));
    PIN(DialogLoaderLoopT, _dialogExternal);  PIN(DialogLoaderLoopT, _ui);
    CHECK_EQ((int)sizeof(Engine::Loader), (int)sizeof(RtEngine::Loader));
    PIN(Loader, _loop);
    // The two non-polymorphic PODs ARE the classes now.
    CHECK((std::is_same<RtEngine::MainLoop, Engine::MainLoop>::value));
    CHECK((std::is_same<RtEngine::Engine, Engine::Engine>::value));
}
#undef PIN

// The exe-side names (engine/types) ARE the classes since ENDGAME B.3.8 block
// LOOP: the POD mirrors were retired with their last users and the headers
// became aliases / forwarders (B8-4).
UNIT_TEST(engine_exe_side_mirrors_match_the_classes) {
    CHECK((std::is_same< ::UpdateLoop, Engine::UpdateLoop>::value));
    CHECK((std::is_same< ::LoaderLoop, Engine::LoaderLoop>::value));
    CHECK((std::is_same< ::DialogLoaderLoopT, Engine::DialogLoaderLoopT>::value));
    CHECK((std::is_same< ::MainLoop, Engine::MainLoop>::value));
    // GameUpdateLoop is a real Engine::UpdateLoop subclass (engine/types forwards
    // to code/Game/Game/GameUpdateLoop.h; tests/unit/loop_abi_test.cpp pins it).
    CHECK((std::is_base_of<Engine::UpdateLoop, ::GameUpdateLoop>::value));
}

}  // namespace
