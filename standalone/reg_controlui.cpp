// standalone/reg_controlui.cpp
// Registration wave for the generic ControlUI component-command buttons.
//
// These are the .rsc buttons that send a command to a component on the selected
// entity — on a pasture they are work / split / empty. All three share element id
// 50000; the command int lives in ControlUIConfig+0x20 (work = 2, split = 1,
// empty = 0) and the target component type in +0x24.
//
// Both classes were UI_DIALOG_STUBs (all-no-op) until 2026-07-31, so the whole
// chain was dead even though ControlUI's three slots were ported and the
// PastureComponent end (slots 13/14 -> Pasture_CanCommand / Pasture_Command) has
// been live all along. ControlUI / ControlUIConfig / ResourceLimitUIConfig are real
// classes since ENDGAME B.3.8 (compiler vtables, reflected by DEFINE_EXE_TYPE); their
// reader-less vtable rows are gone, and only the ControlUIConfig type-id cell is left.
//
// Hook: address_registry.cpp calls RegisterControlUIBatch().

#include "address_registry.h"
#include "resolve.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include "Dialog/ControlUIConfig.h"

// ControlUIConfig's TYPE ID — exe .rdata slot 0x1401c4e20, whose retail DATA writer is
// ControlUIConfig_Register @ 0x14014be40 (standalone/factory_registrations.csv:76).
// It was registered NOWHERE in this tree, so ResolveAddr returned 0,
// g_TypeId_ControlComponent (engine_funcs.cpp:2236 — the name is a misnomer, this is
// ControlUIConfig, not any "ControlComponent") stayed a NULL POINTER, and the very first
// guard in ControlUI::Slot4/Slot7/Slot9 rejected every call:
//     IsDerivedFrom(cfg, 0) -> false -> component null -> early return.
// Slot4 therefore returned BEFORE stamping element id 50000 and before resolving the
// target component, so no element ever carried id 50000 and Slot9's `p4 == 50000` test
// could never match — zero clicks, for every ControlUI button in the game.
// Four readers, all ControlUI/ControlUIConfig (0x14005f82b, 0x14005f886, 0x14005f9cc,
// 0x140010cb0), so filling it has no blast radius beyond this class — no env gate.
static uint32_t g_tid_ControlUIConfig = 0;

// Must run AFTER Engine::Startup assigns the real CRC ids — called from game_main.cpp
// alongside the other Fill* hooks.
extern "C" void FillControlUITypeIds() {
    g_tid_ControlUIConfig = ControlUIConfig::TypeId();
}

void RegisterControlUIBatch() {
    RegisterExeAddr(0x1401c4e20, reinterpret_cast<uintptr_t>(&g_tid_ControlUIConfig));
}
