// standalone/reg_toolbar_utility.cpp
// GUI_PLAN Keystone F, Phase 4 — Utility dialogs (toolbar case 2).
//
// Registers the two exe helpers Toolbar_Utility_Impl (FUN_140101a80) calls, so
// their engine_funcs pointers resolve non-null in the standalone exe:
//   0x140100270  FindDialogByControllerType -> ToolbarUtility_FindDialogByControllerType_0270
//   0x140014890  BST_Insert_0x14            -> ToolbarUtility_BstInsert_4890
// Bodies live in code/Game/Dialog/toolbar_utility_helpers.cpp.
//
// Closure (bottom-up; every sub-callee registered/resolvable before the caller):
//   0x140100270  needs: fn_UI_GetFirstDialog, fn_DialogInstance_GetController,
//                  fn_IsDerivedFrom, fn_InternalBasePtr_Ctor/_CheckPtr (all Runtime
//                  exports via engine_funcs) + ToolUI type-id (GetTypeIdByName).
//   0x140014890  needs: fn_MemAlloc (Runtime) + the AVL retrace 0x140014b40, which is
//                  ALREADY registered by reg_keystone_a_restree.cpp — resolvable here.
//   g_ToolUI_ChildDialog_Vtable is filled by RegisterPlayerToolUIBatch (already wired).
//   g_TypeId_UIDescription is filled lazily inside Toolbar_Utility_Impl (GetTypeIdByName).
//
// The engine_funcs pointers fn_FindDialogByControllerType / fn_BST_Insert_0x14 read
// ResolveAddr(Exe, 0x140100270 / 0x140014890) in EngineFuncsInit, which runs AFTER
// AddressRegistryInit — so registering here makes them resolve to these reimpls.
#ifdef STANDALONE_EXE
#include "resolve.h"
#include <cstdint>

extern "C" void ToolbarUtility_FindDialogByControllerType_0270(void*, void*, uint64_t); // 0x140100270
extern "C" void ToolbarUtility_BstInsert_4890(int64_t*, void**, uint64_t*);              // 0x140014890

void RegisterToolbarUtilityBatch() {
    RegisterExeAddr(0x140100270,
        reinterpret_cast<uintptr_t>(&ToolbarUtility_FindDialogByControllerType_0270));
    RegisterExeAddr(0x140014890,
        reinterpret_cast<uintptr_t>(&ToolbarUtility_BstInsert_4890));
}
#endif // STANDALONE_EXE
