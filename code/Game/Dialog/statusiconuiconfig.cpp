// code/Game/Dialog/statusiconuiconfig.cpp
// Clean reimplementation of StatusIconUIConfig (the status-icon ribbon's config).
//
// StatusIconUIConfig (0x28 bytes) — extends UI::DialogControllerConfig. Registered
// by retail StatusIconUIConfig_Register 0x14014c840 into type-id slot
// DWORD_1401c56e0. Vtable 0x14015d900.
//
//   +0x20  bool _showOnMainPage
//
// Why it exists: a workplace dialog mounts the SAME "icons" ribbon twice
// (ToolKit/resource/Template/Pasture.rsc): once at "userGroup1" with no config and
// once at "userTitle2" with `_config = "CropField.rsc:titleBuildStatus"`, where
// CropField.rsc declares `StatusIconUIConfig titleBuildStatus { bool
// _showOnMainPage = false; }`. That single byte is what keeps the title-bar copy
// off the main info page — without it the stop-work icon renders twice.
//
// Retail bodies (all decompiled, not assumed):
//   Factory        0x140014250: MemAlloc(0x28) + DialogControllerConfig ctor +
//                  vtable + `this[0x20] = 0`  (ONE byte, not a qword).
//   CreateInPlace  0x1400142a0: same, no alloc.
//   vtable 0x14015d900 = { 0x140014320 dtor, 0x140014240 slot1, 0x140014230
//                  GetTypeId, 0x14007b450 GetVersion(=1), 0x14003a912,
//                  0x14007b400 Serialize, 0x1400142e0 reconstruct,
//                  0x140001640, 0x1400142a0 CreateInPlace }
//   Serialize      0x14007b400:
//                      IO::Serializer s(stream, 0);          // slot 0x140152ad8
//                      IO::Serializer::Serialize(s, (bool*)(this+0x20), nullptr);
//                                                            // slot 0x140152a58
//                      ~Serializer(s);                       // slot 0x140152ad0
//                      return 1;
//                  ONE BYTE off the stream. That is the whole point of promoting
//                  this class: the generic UI_DIALOG_STUB left slot 5 a StubNop,
//                  so the external-object load consumed ZERO bytes where the
//                  resource stream holds one — the external never resolved and
//                  every consumer saw a NULL config at controller+0x10.
//
// Replaces UI_DIALOG_STUB(StatusIconUIConfig, "UI::DialogControllerConfig", 0x28);
// that stub registration is removed to avoid a duplicate REGISTER_TYPE. Same
// structure as code/Game/Dialog/upgradeuiconfig.cpp.

#include "ui/UserInterface.h"
#include "object/External.h"
#include "object/TypeInfo.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch D
#include <new>

#include "object/Internal.h"
#include "Dialog/StatusIconUIConfig.h"

// ============================================================================
// ENDGAME B.3.8 PNL — StatusIconUIConfig as a REAL class (StatusIconUIConfig.h). The hand-built
// 9-slot table, its Init/Factory/CreateInPlace entry points and the lazy
// GetTypeId cache are gone; the compiler emits the table.
// ============================================================================

// [4] not declared: retail's slot is the imported External::Compile, which
// the class inherits.

// [5] 0x14007b400 — deserialize _showOnMainPage (ONE byte; retail MOV AL,1).
bool StatusIconUIConfig::Serialize(IO::Stream& stream) {
    IO::Serializer serializer(stream, 0);
    serializer.Serialize(_showOnMainPage, nullptr);
    return true;
}

// [6] 0x1400142e0 — destroy in place, construct again.
Object::External* StatusIconUIConfig::Reconstruct() {
    this->~StatusIconUIConfig();
    return new (this) StatusIconUIConfig();
}

// [7] 0x140001640, the body shared by 119 exe tables:
// `MOV RAX,[RCX]; XOR EDX,EDX; JMP [RAX]` = destroy in place, no free.
void StatusIconUIConfig::Destruct() { this->~StatusIconUIConfig(); }

// [8] 0x1400142a0 — placement construction (MSVC folded it with createInPlace).
Object::External* StatusIconUIConfig::Construct() { return new (this) StatusIconUIConfig(); }

// create 0x140014250: MemAlloc(0x28, 8), then the ctor.
void* StatusIconUIConfig::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(StatusIconUIConfig), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) StatusIconUIConfig());
}
// createInPlace 0x1400142a0.
void* StatusIconUIConfig::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) StatusIconUIConfig());
}


DEFINE_EXE_TYPE(StatusIconUIConfig, "UI::DialogControllerConfig")
