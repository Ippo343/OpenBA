// code/Game/Dialog/controluiconfig.cpp
// Clean reimplementation of ControlUIConfig (the config resource behind every
// generic ControlUI button — pasture work/split/empty, and any other .rsc button
// bound to a component command).
//
// ControlUIConfig (0x28 bytes) — extends UI::DialogControllerConfig (Object::External's
// 0x20 base plus two 4-byte fields). Vtable at 0x14015b3e0.
//
//   +0x20  int          COMMAND ID passed to the target component's vtable[14].
//                       For PastureComponent: work = 2, split = 1, empty = 0.
//                       Also the key ControlUI feeds to vtable[13] (the can-command /
//                       checked-state query) in Slot4/Slot7.
//   +0x24  Object::Type TARGET COMPONENT type — which component on the selected
//                       entity the button drives. ControlUI::Slot4 hands this to
//                       FindComponentByTypeId and caches the result at ControlUI+0x38.
//
// Factory       0x140010cd0: MemAlloc(0x28) + base ctor + vtable + zero 8 bytes at
//                            +0x20 (ONE 8-byte store covering BOTH fields).
// CreateInPlace 0x140010d30: same, no alloc.
// Serialize     0x14005fb70 (vtable idx5, Ghidra "ControlUI_vslot17" — the name lies,
//                            it is a ControlUIConfig method):
//                   IO::Serializer(stream, 0)          slot 0x140152ad8
//                   Serialize(int&,  cfg+0x20)         slot 0x140152ac8
//                   Object::Type::Serialize(cfg+0x24)  slot 0x1401516b0
//                   ~Serializer                        slot 0x140152ad0
//                   return 1
//               NOTE the int slot is 0xAC8, NOT the 0xA50 uint overload other
//               call sites use — verified by `disassemble_function 0x14005fb70 CALL`.
// GetVersion    (idx3): return 1.
//
// Replaces UI_DIALOG_STUB(ControlUIConfig, ...), whose idx5 was StubNop — so the
// config stayed zero-filled, every button's command read as 0 ("empty"), and its
// target component type read as 0. Together with the ControlUI stub (both were
// no-ops) that is why the pasture work/split buttons did nothing at all.
//
// Decompiled from Application-x64.exe.

#include "engine_funcs.h"
#include "object/Type.h"   // B3R: RtObject::RtObject_Type_Serialize
#include "Object/factory_registration.h"
#include "address_registry.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch D
#include <new>

#include "object/Internal.h"
#include "Dialog/ControlUIConfig.h"

// ============================================================================
// ENDGAME B.3.8 PNL — ControlUIConfig as a REAL class (ControlUIConfig.h).
// The hand-built 9-slot table, its Init/Factory/CreateInPlace entry points and
// the lazy GetTypeId cache are gone; the compiler emits the table.
// ============================================================================

// [4] not declared: retail's slot is the imported External::Compile, which
// the class inherits.

// [5] 0x14005fb70 — read the command id and the target component type.
// Object::Type::Serialize is the exe import slot 0x1401516b0:
// RtObject_Type_Serialize, called by name (B3R).
bool ControlUIConfig::Serialize(void* stream) {
    IO::Serializer serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    serializer.Serialize(_command, nullptr);
    RtObject::RtObject_Type_Serialize(&_componentType, &serializer);   // import slot 0x1401516b0
    return true;
}

// [6] 0x140010d80 — destroy in place, construct again.
Object::External* ControlUIConfig::Reconstruct() {
    this->~ControlUIConfig();
    return new (this) ControlUIConfig();
}

// [7] 0x140001640, the body shared by 119 exe tables:
// `MOV RAX,[RCX]; XOR EDX,EDX; JMP [RAX]` = destroy in place, no free.
void ControlUIConfig::Destruct() { this->~ControlUIConfig(); }

// [8] 0x140010d30 — placement construction (MSVC folded it with createInPlace).
Object::External* ControlUIConfig::Construct() { return new (this) ControlUIConfig(); }

// create 0x140010cd0: MemAlloc(0x28, 8), then the ctor.
void* ControlUIConfig::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ControlUIConfig), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) ControlUIConfig());
}
// createInPlace 0x140010d30.
void* ControlUIConfig::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) ControlUIConfig());
}


DEFINE_EXE_TYPE(ControlUIConfig, "UI::DialogControllerConfig")
