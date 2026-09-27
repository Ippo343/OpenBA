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
// BridgeTool::CalcCost (0x1400facf0), CalcPath (0x1400f9cd0) and Draw (0x1400fadc0)
// themselves are real virtuals (ENDGAME B.3.8), reached through the compiler vtable;
// nothing reads their addresses back, so they need no address entry here.

#include "resolve.h"
#include <cstdint>

extern "C" int      BridgeTool_SnapEndCell_Impl(void* self, int endCell);
extern "C" uint32_t BridgeTool_ValidatePath_Impl(void* self, int startCell, int endCell,
                                                 int* outLength);

void RegisterPlayerBridgeBatch() {
    RegisterExeAddr(0x1400fb710, reinterpret_cast<uintptr_t>(&BridgeTool_SnapEndCell_Impl));
    RegisterExeAddr(0x1400fb2d0, reinterpret_cast<uintptr_t>(&BridgeTool_ValidatePath_Impl));
}
