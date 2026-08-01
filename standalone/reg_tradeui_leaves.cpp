// standalone/reg_tradeui_leaves.cpp
// Wave A — registration for the TradeUI leaf helpers ported in
// code/Game/Dialog/tradeui_leaves.cpp.
//
// These addresses are consumed via engine_funcs' fn_DynArray_Clear_* pointers, which
// are filled by ResolveAddr(Module::Exe, ...) during engine_funcs init. Registering
// them here makes those pointers non-null, which in turn lets DepsReady() in
// ui_dialog_stub_factories.cpp tick off the leaves registered below.
//
// This file deliberately states no tally of what is done vs outstanding. The gate in
// ui_dialog_stub_factories.cpp counts the leaves at runtime and names the missing ones
// on stderr; that count is the authority, and a number written here would be a lie the
// moment a leaf lands. Verify with BANISHED_TOOL_HARNESS=tradepanel and read the
// "[TradeUI] STAYING STUBBED" block (absent entirely once the gate opens).
//
// Registrations here cover more than the gate lists: several gate entries turned out
// to have unported sub-callees of their own (the two introsorts needed four extra
// functions between them), and those are registered too so nothing else resolves null.
//
// Wired via RegisterTradeUILeavesBatch() from RegisterCodeFunctions() in
// standalone/address_registry.cpp (integrator step).
#include "resolve.h"
#include <cstdint>

extern "C" void TradeUI_DynArrayClear_16050(int64_t* header);   // 0x140016050
extern "C" void TradeUI_DynArrayClear_16130(int64_t* header);   // 0x140016130
extern "C" void TradeUI_DynArrayClear_16200(int64_t* header);   // 0x140016200
extern "C" void* TradeUI_GetOrderName_15fd0(int64_t tradeState, void* out); // 0x140015fd0
extern "C" void TradeUI_AddPurchaseOrder_59490(int64_t tradeState,
                                               int64_t resourceEntity,
                                               int8_t  addFlag);            // 0x140059490
extern "C" void* TradeUI_StorageTreeNodeNew_e2c0(uint64_t key, const uint64_t* entry,
                                                 void* parent);              // 0x14000e2c0
extern "C" void* TradeUI_StorageTree_Insert_e1c0(int64_t* tree, void** out,
                                                 const uint64_t* entry);     // 0x14000e1c0
extern "C" void TradeUI_IntroSort_ByName_162d0(void* array, int count,
                                               void* phantomCtx, int64_t p4); // 0x1400162d0
extern "C" void TradeUI_IntroSort_ByComponent_165d0(void* array, int count,
                                                    void* ctx);              // 0x1400165d0
extern "C" void TradeUI_SmallSort_ByName_15ad0(void* first, void* last);      // 0x140015ad0
extern "C" void TradeUI_SmallSort_ByComponent_16920(void* first, void* last,
                                                    void* ctx);              // 0x140016920
extern "C" bool TradeUI_NameLess_845d0(void* self, const void* a, const void* b);   // 0x1400845d0
extern "C" bool TradeUI_ComponentLess_84430(void* ctx, const void* a, const void* b); // 0x140084430
extern "C" void TradeUI_SortSelfTest();
extern "C" void TradeUI_GetBuyEntries_59670(void* tradeState, void* outArray); // 0x140059670
extern "C" int   TradeUI_GetResourcePrice_a520(void* tradeState, void* resourceEntity); // 0x14005a520
extern "C" void** TradeUI_FindValueDescription_ea80(const void* resourceEntity,
                                                    void** out);             // 0x14000ea80

void RegisterTradeUILeavesBatch() {
    RegisterExeAddr(0x140016050, reinterpret_cast<uintptr_t>(&TradeUI_DynArrayClear_16050));
    RegisterExeAddr(0x140016130, reinterpret_cast<uintptr_t>(&TradeUI_DynArrayClear_16130));
    RegisterExeAddr(0x140016200, reinterpret_cast<uintptr_t>(&TradeUI_DynArrayClear_16200));
    RegisterExeAddr(0x140015fd0, reinterpret_cast<uintptr_t>(&TradeUI_GetOrderName_15fd0));
    RegisterExeAddr(0x140059490, reinterpret_cast<uintptr_t>(&TradeUI_AddPurchaseOrder_59490));
    // 0x14000e2c0 is registered as well as ported: TradeUI_impl_55a0 (0x1400255a0) is a
    // SECOND caller of the node allocator, so leaving it unregistered would hand that
    // caller a null when it is eventually wired.
    RegisterExeAddr(0x14000e2c0, reinterpret_cast<uintptr_t>(&TradeUI_StorageTreeNodeNew_e2c0));
    RegisterExeAddr(0x14000e1c0, reinterpret_cast<uintptr_t>(&TradeUI_StorageTree_Insert_e1c0));

    // The sort cluster (code/Game/Dialog/tradeui_sort.cpp). The gate names only the
    // two drivers, but their comparators and small-range sorts were equally unported
    // and unregistered — registering all six keeps any other caller from resolving
    // null. See the ABI block at the top of tradeui_sort.cpp: these signatures come
    // from the prologues, not the decompile, which invents trailing parameters.
    RegisterExeAddr(0x1400162d0, reinterpret_cast<uintptr_t>(&TradeUI_IntroSort_ByName_162d0));
    RegisterExeAddr(0x1400165d0, reinterpret_cast<uintptr_t>(&TradeUI_IntroSort_ByComponent_165d0));
    RegisterExeAddr(0x140015ad0, reinterpret_cast<uintptr_t>(&TradeUI_SmallSort_ByName_15ad0));
    RegisterExeAddr(0x140016920, reinterpret_cast<uintptr_t>(&TradeUI_SmallSort_ByComponent_16920));
    RegisterExeAddr(0x1400845d0, reinterpret_cast<uintptr_t>(&TradeUI_NameLess_845d0));
    RegisterExeAddr(0x140084430, reinterpret_cast<uintptr_t>(&TradeUI_ComponentLess_84430));

    // The price lookup (code/Game/Dialog/tradeui_price.cpp) and its ValueDescription
    // finder. 0x14000ea80 is registered as well as ported because it is a real exe
    // leaf with its own callers, not just an implementation detail of a520.
    RegisterExeAddr(0x14005a520, reinterpret_cast<uintptr_t>(&TradeUI_GetResourcePrice_a520));
    RegisterExeAddr(0x14000ea80, reinterpret_cast<uintptr_t>(&TradeUI_FindValueDescription_ea80));

    // The last gate leaf (code/Game/Dialog/tradeui_buyentries.cpp).
    RegisterExeAddr(0x140059670, reinterpret_cast<uintptr_t>(&TradeUI_GetBuyEntries_59670));

    // Opt-in (BANISHED_TRADE_SORT_SELFTEST=1), no-op otherwise. Uses no engine
    // functions, so running it this early is safe.
    TradeUI_SortSelfTest();
}
