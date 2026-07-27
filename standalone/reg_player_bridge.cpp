// standalone/reg_player_bridge.cpp
// Registration for the BridgeTool batch (code/Game/Player/bridgetool_validate.cpp).
//
// Maps each ported exe helper's Ghidra address to our reimplementation so that
// ResolveAddr(Module::Exe, 0xADDR) returns our function instead of 0 in standalone.
//
// These two are bound by engine/funcs/engine_funcs.cpp into fn_BridgeTool_SnapEndCell /
// fn_BridgeTool_ValidatePath, which BridgeTool::CalcCost (slot 14) null-checks and bails
// on. Unregistered they resolved to 0, so every frame of a bridge drag returned 3
// (invalid) and no cost or length was ever computed.
//
// BridgeTool::CalcCost (0x1400facf0) and CalcPath (0x1400f9cd0) themselves are reached
// through the BridgeTool vtable (bridgetool.cpp), so they need no address entry here.

#include "resolve.h"
#include <cstdint>

extern "C" int      BridgeTool_SnapEndCell_Impl(void* self, int endCell);
extern "C" uint32_t BridgeTool_ValidatePath_Impl(void* self, int startCell, int endCell,
                                                 int* outLength);
extern "C" uint64_t BridgeTool_CalcPath_Impl(void* self, int startCell, int endCell);
extern "C" void     BridgeTool_Draw_Impl(void* self, void* context, char skip);

void RegisterPlayerBridgeBatch() {
    RegisterExeAddr(0x1400fb710, reinterpret_cast<uintptr_t>(&BridgeTool_SnapEndCell_Impl));
    RegisterExeAddr(0x1400fb2d0, reinterpret_cast<uintptr_t>(&BridgeTool_ValidatePath_Impl));
    RegisterExeAddr(0x1400f9cd0, reinterpret_cast<uintptr_t>(&BridgeTool_CalcPath_Impl));
    // Slot 8 Draw (0x1400fadc0) — the red/green drag preview, bridgetool_draw.cpp.
    // Reached through the BridgeTool vtable, but registered so the address resolves
    // for any caller that goes through ResolveAddr.
    RegisterExeAddr(0x1400fadc0, reinterpret_cast<uintptr_t>(&BridgeTool_Draw_Impl));
}
