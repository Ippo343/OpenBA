// standalone/reg_player_toolui.cpp
// Player registration wave (round 2c — tool-ui) registration.
//
// Registers the OpenToolDialog exe helper (0x1400ff9e0, resolved from
// code/Game/Player/dragtool.cpp + placementtool.cpp) and the child-dialog vtable
// it installs (0x14015f5a0), to the reimplementations in
// code/Game/Player/tool_base_toolui.cpp. The integrator wires
// RegisterPlayerToolUIBatch() into RegisterCodeFunctions() in
// standalone/address_registry.cpp.
//
// Safety / closure:
//   0x1400ff9e0 OpenToolDialog — verified real function ENTRY (get_function_by_address
//     Entry == 0x1400ff9e0). Its only non-Runtime/non-engine_funcs dependency was the
//     child-dialog vtable at 0x14015f5a0 (unregistered → 0 in standalone → broken
//     dialog vtable), which is the reason it was DEFERRED in the P1 wave. We satisfy
//     the closure by registering a faithful standalone replacement vtable
//     (g_ToolUI_ChildDialog_Vtable_Standalone) whose 10 slots forward to the SAME
//     Runtime-x64.dll DialogInstance virtuals the exe thunks JMP to. With the vtable
//     present the whole closure is satisfied (engine_funcs exports + singleton
//     globals + Runtime DLL), so OpenToolDialog is now closure-safe to register.
//   0x14015f5a0 g_ToolUI_ChildDialog_Vtable — registered to the static C++ array so
//     ResolveAddr(Module::Exe, 0x14015f5a0) returns it (mirrors how mainmenudialog.cpp
//     registers its standalone dialog vtable at 0x14015f550).
#ifdef STANDALONE_EXE
#include "resolve.h"
#include <cstdint>

// OpenToolDialog reimpl + the child-dialog vtable (tool_base_toolui.cpp). Kept for the
// dedicated fix; NOT registered (see DEFER note below).
extern "C" void* ToolUI_OpenToolDialog_ff9e0(void*, void*, uint64_t);  // 0x1400ff9e0
extern void* g_ToolUI_ChildDialog_Vtable_Standalone[10];              // 0x14015f5a0
extern "C" void ToolUI_InitChildDialogVtable();                       // eager slot-fill

// ============================================================================
// DEFERRED — both addrs intentionally NOT registered (batch is a no-op).
//
// 0x1400ff9e0 OpenToolDialog needs the shared child-dialog vtable 0x14015f5a0
// registered. But 0x14015f5a0 is a SHARED DialogInstance-subclass vtable installed
// at STARTUP by MainMenuDialog_Serialize (and EntityUI/PlacementTool). The existing
// standalone menu reimpl (code/Game/Dialog/mainmenudialog.cpp, g_MainMenuDialog_
// BuildIdVtable) was written ASSUMING ResolveAddr(Exe,0x14015f5a0)==0 (a null guard
// skips installing it on the menu's BuildId child). Registering it (even with slots
// fully filled to the base Runtime DialogInstance virtuals) makes the menu's child
// dispatch a real OnOpened (Runtime 0x1801ba468) on a child that the standalone menu
// does not fully construct -> ACCESS_VIOLATION at startup. A/B-bisected + confirmed
// (crash RIP == Runtime+0x1BA468 OnOpened). Landing ff9e0 requires the menu BuildId
// child to be made fully constructible with the real vtable (mainmenudialog.cpp work)
// OR a non-address-registration way to give ONLY the tool dialog this vtable. Until
// then this stays a no-op so the gap metric honestly reports ff9e0 as unregistered.
// ============================================================================
void RegisterPlayerToolUIBatch() {
    // intentionally empty — 0x1400ff9e0 + 0x14015f5a0 DEFERRED (shared-vtable startup
    // conflict with mainmenudialog.cpp; see header). Reference the externs so the TU's
    // symbols aren't dropped while the bodies await the dedicated fix.
    (void)&ToolUI_OpenToolDialog_ff9e0;
    (void)&g_ToolUI_ChildDialog_Vtable_Standalone;
    (void)&ToolUI_InitChildDialogVtable;
}
#endif // STANDALONE_EXE
