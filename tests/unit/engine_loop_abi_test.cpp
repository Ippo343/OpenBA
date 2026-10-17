// ============================================================================
// tests/unit/engine_loop_abi_test.cpp — ENDGAME B.3.7 block B1
//   Engine::UpdateLoop / LoaderLoop / DialogLoopT / DialogLoaderLoopT / Loader,
//   Engine::MainLoop and Engine::Engine as real classes.
//
// PART A — SHAPE (sizes, alignments and offsets are UpdateLoop.h / MainLoop.h /
//   Engine.h static_asserts): the four loops are polymorphic; three are abstract
//   as in retail (LoaderLoop concrete since B.3.9 ENG W2), Loader is a concrete
//   System::Thread subclass, MainLoop and Engine are NOT polymorphic.
// PART B — RETAIL SLOT ORDER, measured WITHOUT an object: MinGW GCC uses the
//   Itanium C++ ABI for pointers to members, where a pointer to a VIRTUAL
//   member function stores `1 + vtable byte offset` in its first word. So
//   `&Engine::UpdateLoop::Load` must read back as slot 6. This needs no vtable
//   emitted and no body linked (src/engine/engine is not in the unit binary),
//   which is exactly why it is the instrument here. B7-1: slot 1 is Startup
//   (the loops are their own root, not Object::Base); slot 12 is new on the two
//   Dialog loops; Loader's slot 0 is Run (no deleting-dtor slot).
// PART C — THE RETSLOT VIRTUAL. Slot 12 returns `InternalPtr<DialogInstance>`
//   BY VALUE through a vtable slot, so it is declared retslot-FIRST (B6-2): the
//   declared type is a file-scope static_assert.
// PART D — ONE DEFINITION PER LAYOUT (B7-6), as file-scope static_asserts:
//   `RtEngine::MainLoop` and `RtEngine::Engine` ARE the classes; GameUpdateLoop
//   is an Engine::UpdateLoop subclass.
// ============================================================================
#include "unit.h"
#include "slot_check.h"

#include "engine/UpdateLoop.h"
#include "engine/MainLoop.h"
#include "engine/Engine.h"
#include "object/Internal.h"

#include "Game/GameUpdateLoop.h"

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <type_traits>

namespace {

// ---------------------------------------------------------------- PART A
UNIT_TEST(engine_loop_sizes_and_shape) {
    // The five sizes and alignments are UpdateLoop.h's static_asserts.
    CHECK(std::is_polymorphic<Engine::UpdateLoop>::value);
    CHECK(std::is_abstract<Engine::UpdateLoop>::value);
    CHECK(!std::is_abstract<Engine::LoaderLoop>::value);         // concrete, as in retail (B.3.9 ENG W2)
    CHECK(std::is_abstract<Engine::DialogLoopT>::value);
    CHECK(std::is_abstract<Engine::DialogLoaderLoopT>::value);
    CHECK((std::is_base_of<Engine::UpdateLoop, Engine::LoaderLoop>::value));
    CHECK((std::is_base_of<Engine::LoaderLoop, Engine::DialogLoaderLoopT>::value));
    CHECK((std::is_base_of<Engine::UpdateLoop, Engine::DialogLoopT>::value));
    CHECK(!(std::is_base_of<Engine::LoaderLoop, Engine::DialogLoopT>::value));

    // Loader: the REAL System::Thread is its base, not a POD member.
    CHECK((std::is_base_of<System::Thread, Engine::Loader>::value));
    CHECK(!std::is_abstract<Engine::Loader>::value);
    CHECK(std::is_default_constructible<Engine::Loader>::value);    // real public ctor (B.3.9 ENG W2)

    // B3X W1: the UpdateLoop family has a virtual destructor (retail slot 0 = D1 + D0); Engine::Loader (Thread family, slot 0 Run) has none.
    CHECK(std::has_virtual_destructor<Engine::UpdateLoop>::value);
    CHECK(std::has_virtual_destructor<Engine::DialogLoaderLoopT>::value);
    CHECK(!std::has_virtual_destructor<Engine::Loader>::value);
}

UNIT_TEST(engine_mainloop_and_engine_shape) {
    // sizeof / alignof / the field offsets are MainLoop.h's and Engine.h's static_asserts.
    CHECK(!std::is_polymorphic<Engine::MainLoop>::value);
    CHECK(!std::is_polymorphic<Engine::Engine>::value);
    CHECK(!std::is_default_constructible<Engine::Engine>::value);  // protected (B5-4)
    CHECK((std::is_same<decltype(Engine::MainLoop::_currentLoop), Engine::UpdateLoop*>::value));
    CHECK((std::is_same<decltype(Engine::MainLoop::_engine), Engine::Engine*>::value));
}

// ---------------------------------------------------------------- PART B
UNIT_TEST(engine_updateloop_retail_slot_order) {
    // B37_SLOTS.md sec. 1, ??_7UpdateLoop@Engine@@6B@ 0x180155ba0.
    CHECK_DTOR_SLOT(Engine::UpdateLoop);
    CHECK_SLOT(Engine::UpdateLoop, Startup,             1);            // NOT Object::Base::TypeName
    CHECK_SLOT(Engine::UpdateLoop, Shutdown,            2);
    CHECK_SLOT(Engine::UpdateLoop, Update,              3);
    CHECK_SLOT(Engine::UpdateLoop, Draw,                4);
    CHECK_SLOT(Engine::UpdateLoop, DrawFailed,          5);
    CHECK_SLOT(Engine::UpdateLoop, Load,                6);
    CHECK_SLOT(Engine::UpdateLoop, SaveRestore,         7);
    CHECK_SLOT(Engine::UpdateLoop, OnVideoError,        8);
    CHECK_SLOT(Engine::UpdateLoop, OnVideoRestart,      9);
    CHECK_SLOT(Engine::UpdateLoop, GetResourceFilename, 10);
    CHECK_SLOT(Engine::UpdateLoop, AllowsModChanges,    11);
}

UNIT_TEST(engine_derived_loop_slot_order) {
    // The overrides keep their base slots ...
    CHECK_SLOT(Engine::LoaderLoop, Startup,         1);
    CHECK_SLOT(Engine::LoaderLoop, Load,            6);
    CHECK_SLOT(Engine::LoaderLoop, OnVideoError,    8);
    CHECK_SLOT(Engine::DialogLoopT, Draw,           4);
    CHECK_SLOT(Engine::DialogLoaderLoopT, Update,   3);
    CHECK_SLOT(Engine::DialogLoaderLoopT, Shutdown, 2);
    // ... DialogLoaderLoopT inherits LoaderLoop's OnVideoError (B7-1.3) ...
    CHECK_SLOT(Engine::DialogLoaderLoopT, OnVideoError, 8);
    // ... and slot 12 is NEW on each Dialog loop.
    CHECK_SLOT(Engine::DialogLoopT, CreateLoopDialog,       12);
    CHECK_SLOT(Engine::DialogLoaderLoopT, CreateLoopDialog, 12);
    // Loader: ??_7Loader@Engine@@6B@ 0x180157a08 -- [0] Run, [1] OnCompleted.
    CHECK_SLOT(Engine::Loader, Run,         0);
    CHECK_SLOT(Engine::Loader, OnCompleted, 1);
}

// ---------------------------------------------------------------- PART C
// Slot 12 returns `InternalPtr<DialogInstance>` by value through a vtable slot,
// so it is declared retslot-FIRST (B6-2): `this`, then the caller's storage.
using DialogRet = Object::InternalPtr<UI::DialogInstance>;
static_assert(std::is_same<decltype(&Engine::DialogLoopT::CreateLoopDialog),
                           DialogRet* (Engine::DialogLoopT::*)(DialogRet*)>::value,
              "DialogLoopT::CreateLoopDialog spells the hidden return slot explicitly");
static_assert(std::is_same<decltype(&Engine::DialogLoaderLoopT::CreateLoopDialog),
                           DialogRet* (Engine::DialogLoaderLoopT::*)(DialogRet*)>::value,
              "DialogLoaderLoopT::CreateLoopDialog spells the hidden return slot explicitly");

// ---------------------------------------------------------------- PART D
// The two non-polymorphic PODs ARE the classes (the five loop PODs are gone,
// B.4); GameUpdateLoop is a real Engine::UpdateLoop subclass (the exe-side
// POD mirrors were retired in B.3.8 block LOOP, B8-4).
static_assert(std::is_same<RtEngine::MainLoop, Engine::MainLoop>::value, "RtEngine::MainLoop is the class");
static_assert(std::is_same<RtEngine::Engine, Engine::Engine>::value, "RtEngine::Engine is the class");
static_assert(std::is_base_of<Engine::UpdateLoop, ::GameUpdateLoop>::value,
              "GameUpdateLoop is an Engine::UpdateLoop subclass (src/game/Game/GameUpdateLoop.h)");

}  // namespace
