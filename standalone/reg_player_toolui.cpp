// standalone/reg_player_toolui.cpp
// Player registration wave (round 2c — tool-ui) registration.
//
// Registers the OpenToolDialog exe helper (0x1400ff9e0, resolved from
// code/Game/Player/dragtool.cpp + placementtool.cpp) to its reimplementation in
// code/Game/Player/tool_base_toolui.cpp. The integrator wires
// RegisterPlayerToolUIBatch() into RegisterCodeFunctions() in
// standalone/address_registry.cpp.
//
// Safety / closure:
//   0x1400ff9e0 OpenToolDialog — verified real function ENTRY (get_function_by_address
//     Entry == 0x1400ff9e0). Its only non-Runtime/non-engine_funcs dependency was the
//     child-dialog vtable at 0x14015f5a0 (unregistered → 0 in standalone → broken
//     dialog vtable), which is the reason it was DEFERRED in the P1 wave. The closure
//     is satisfied by a faithful standalone replacement vtable
//     (g_ToolUI_ChildDialog_Vtable_Standalone) whose 10 slots forward to the SAME
//     Runtime-x64.dll DialogInstance virtuals the exe thunks JMP to; the reimpl writes
//     that static array directly, so OpenToolDialog is closure-safe to register.
//   0x14015f5a0 — the exe vtable address itself is DELIBERATELY NOT registered in the
//     address registry (see the RESOLVED note below: registering it poisons the menu's
//     BuildId child). Only the engine_funcs global g_ToolUI_ChildDialog_Vtable is
//     filled, by direct assignment.
#include "resolve.h"
#include <cstdint>

// OpenToolDialog reimpl + the child-dialog vtable (tool_base_toolui.cpp). ff9e0 IS
// registered below; only the vtable address 0x14015f5a0 stays unregistered (see the
// RESOLVED note).
extern "C" void* ToolUI_OpenToolDialog_ff9e0(void*, void*, uint64_t);  // 0x1400ff9e0
extern void* g_ToolUI_ChildDialog_Vtable_Standalone[10];              // 0x14015f5a0
extern "C" void ToolUI_InitChildDialogVtable();                       // eager slot-fill

// engine_funcs global (engine_funcs.h:1464). Declared here to avoid pulling the full
// engine_funcs.h into this lightweight registration TU. Set directly (not via the
// address registry) so the ToolbarDialog path has a valid child vtable without
// poisoning the menu's separate g_MainMenuDialog_BuildIdVtable global.
extern uintptr_t g_ToolUI_ChildDialog_Vtable;

// ============================================================================
// RESOLVED (the "non-address-registration way to give ONLY the tool dialog this
// vtable"). We register ONLY the OpenToolDialog helper (0x1400ff9e0) and DELIBERATELY
// LEAVE the shared child-dialog vtable 0x14015f5a0 UNregistered.
//
// Why this is safe (the decoupling): 0x14015f5a0 is a SHARED DialogInstance-subclass
// vtable. Registering it via the address registry makes BOTH engine_funcs globals
// that derive from ResolveAddr(Exe,0x14015f5a0) go non-null:
//   - g_MainMenuDialog_BuildIdVtable — the menu's CreateBuildIdChildDialog then
//     installs the override on its BuildId child and dispatches a real OnOpened
//     (Runtime 0x1801ba468) on a child the standalone menu doesn't fully construct
//     -> ACCESS_VIOLATION at startup (the documented A/B-bisected crash).
//   - g_ToolUI_ChildDialog_Vtable — used only by the (gated-off) ToolbarDialog path.
// But OpenToolDialog's reimpl (ToolUI_OpenToolDialog_ff9e0) NEVER reads either global
// or ResolveAddr(Exe,0x14015f5a0): it writes &g_ToolUI_ChildDialog_Vtable_Standalone
// (the static C++ array in tool_base_toolui.cpp) directly into the new dialog's slot 0.
// So it is fully self-sufficient. Registering 0x14015f5a0 was only ever a gap-metric
// nicety — and it is exactly what poisons the menu. By NOT registering it, the menu
// stays byte-identical to its current working state (BuildId child keeps the default
// DialogInstance vtable -> no crash) while the placement/drag tool dialogs work via
// the static array.
//
// We DO populate the engine_funcs global g_ToolUI_ChildDialog_Vtable here (direct
// assignment, NOT via the address registry) so the future ToolbarDialog path has a
// valid vtable. This global is SEPARATE from g_MainMenuDialog_BuildIdVtable, so
// setting it does not affect the menu.
//
// The gap metric will continue to count 0x14015f5a0 as unregistered. That is correct
// and intentional: it is a shared MENU vtable we specifically do not want the
// standalone menu to use.
// ============================================================================
void RegisterPlayerToolUIBatch() {
    // Eagerly fill the static child-dialog vtable slots 1..9 with the base Runtime
    // DialogInstance virtuals. MUST be eager (not lazy in OpenToolDialog): this batch
    // runs in AddressRegistryInit, AFTER ResolveInit has loaded Runtime-x64.dll, so
    // ResolveAddr(Module::Runtime) is valid; a lazy fill would leave slots null if the
    // table were ever dispatched before the first tool dialog opens.
    ToolUI_InitChildDialogVtable();

    // Register ONLY the OpenToolDialog helper. dragtool.cpp / placementtool.cpp resolve
    // g_fn_OpenToolDialog = ResolveAddr(Exe, 0x1400ff9e0) -> this reimpl.
    RegisterExeAddr(0x1400ff9e0, reinterpret_cast<uintptr_t>(&ToolUI_OpenToolDialog_ff9e0));

    // Give the (gated-off) ToolbarDialog path a valid child vtable WITHOUT registering
    // 0x14015f5a0 in the address registry (which would poison the menu). Direct global
    // assignment — separate from g_MainMenuDialog_BuildIdVtable.
    g_ToolUI_ChildDialog_Vtable =
        reinterpret_cast<uintptr_t>(g_ToolUI_ChildDialog_Vtable_Standalone);
}
