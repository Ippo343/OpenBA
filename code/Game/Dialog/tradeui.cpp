// code/Game/Dialog/tradeui.cpp
// Clean reimplementation of TradeUI methods.
//
// TradeUI (0x148 bytes): Trading post panel — shows incoming merchants,
// trade items, buy/sell interface with order management.
// Extends DialogController (0x30 base).
//
// Layout:
//   +0x30..+0x58: sell order array (ptr, ptr, count, capacity) x3 vectors
//   +0x60..+0x78: sell entries (ptr, count, capacity, stride 0x20)
//   +0x70..+0x88: buy entries (ptr, count, capacity, stride 0x20)
//   +0x80..+0x98: storage entries (ptr, count, capacity, stride 0x18)
//   +0x90..+0xA8: order entries (ptr, count, capacity, stride 0x20)
//   +0xA0..+0xB8: purchase entries (ptr, count, capacity, stride 0x08)
//   +0xB0..+0xC8: reserved
//   +0xC0: comboOrder element
//   +0xC8: tableSell element
//   +0xD0: tableBuy element
//   +0xD8: tableStorage element
//   +0xE0: tablePurchase element
//   +0xE8: tableOrder element
//   +0xF0: buttonTrade element
//   +0xF8: buttonDismiss element
//   +0x100: numberSellTotal element
//   +0x108: numberBuyTotal element
//   +0x110: textNoStorage element
//   +0x118: textTooMuch element
//   +0x120: textNotEnough element
//   +0x128: reserved
//   +0x130: reserved
//   +0x138: comboPurchase element
//   +0x140: sortCombo element

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>

// ============================================================
// Draw (vtable +0x28): Mark tables for layout recalc, set building dirty flag
// ============================================================
// Original: 0x140082a70
//
// Marks tableSell (+0xC8) and tableBuy (+0xD0) as needing size recalculation.
// Then finds the BuildingComponent on the owning entity and sets its
// dirty flag (byte at +0x71) to 1, triggering a visual refresh.

void TradeUI_Draw(TradeUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Mark sell and buy tables for layout recalculation
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xC8));
    fn_Element_SetNeedsSizeCalc(*(void**)(s + 0xC8));

    fn_InternalBasePtr_CheckPtr((void*)(s + 0xD0));
    fn_Element_SetNeedsSizeCalc(*(void**)(s + 0xD0));

    // Get owning entity and find BuildingComponent
    void* entity = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entity : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    fn_InternalBasePtr_Ctor(&entityPtr, compEntity);

    fn_InternalBasePtr_CheckPtr(&entityPtr);
    uint64_t buildingPtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &buildingPtr);

    // Set building dirty flag
    fn_InternalBasePtr_CheckPtr(&buildingPtr);
    *(uint8_t*)(buildingPtr[0] + 0x71) = 1;
}

// ============================================================
// Slot6 (vtable +0x30): Clear building dirty flag
// ============================================================
// Original: 0x140082a00
//
// Finds BuildingComponent on owning entity, clears its dirty flag
// (byte at +0x71 = 0). Called when panel is hidden/deactivated.

void TradeUI_Slot6(TradeUI* self) {
    uintptr_t s = (uintptr_t)self;

    void* entity = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entity : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    fn_InternalBasePtr_Ctor(&entityPtr, compEntity);

    fn_InternalBasePtr_CheckPtr(&entityPtr);
    uint64_t buildingPtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &buildingPtr);

    fn_InternalBasePtr_CheckPtr(&buildingPtr);
    *(uint8_t*)(buildingPtr[0] + 0x71) = 0;
}

// ============================================================
// Serialize (vtable +0x20): Complex UI setup — delegated to exe
// ============================================================
// Original: 0x140084dd0
//
// Finds 15+ named elements (tableSell, tableBuy, tableStorage, tablePurchase,
// tableOrder, numberSellTotal, numberBuyTotal, buttonTrade, buttonDismiss,
// textNoStorage, textTooMuch, textNotEnough, comboPurchase, comboOrder,
// sortCombo), builds table arrays with spinners, enumerates resource types.

void TradeUI_Serialize(TradeUI* self) {
    fn_TradeUI_Serialize(self);
}

// ============================================================
// Startup (vtable +0x38): Trade value calculations — delegated to exe
// ============================================================
// Original: 0x140082150
//
// Calculates total sell/buy values from order arrays and resource values.
// Checks storage capacity, shows/hides warning text (textNoStorage,
// textTooMuch, textNotEnough). Enables/disables trade and dismiss buttons
// based on trade state and merchant presence.

void TradeUI_Startup(TradeUI* self) {
    fn_TradeUI_Startup(self);
}

// ============================================================
// OnCreate (vtable +0x40): Refresh on order changes — delegated to exe
// ============================================================
// Original: 0x1400827b0
//
// Finds BuildingComponent, checks if the buy order count (+0x48 on self)
// differs from building's order count (+0x58 on building). If changed,
// refreshes sell, buy, and order tables.

void TradeUI_OnCreate(TradeUI* self) {
    fn_TradeUI_OnCreate(self);
}

// ============================================================
// OnRemove (vtable +0x48): Event dispatcher — delegated to exe
// ============================================================
// Original: 0x140081f20
//
// Dispatches UI events based on type and element ID:
//   eventType 0x12: buttonTrade (20000), buttonDismiss (0x4e21),
//     order spinners (5000-5999, 6000-6999), purchase buttons (8000+)
//   eventType 0x14: comboPurchase (0x4e22), comboOrder (0x4e23), sortCombo (199)
//   eventType 0x15: storage spinners (2000-2999), sell spinners (3000-3999),
//     buy spinners (4000-4999), order amount spinners (7000-7999)

void TradeUI_OnRemove(TradeUI* self, void* param2, int eventType, int eventId) {
    fn_TradeUI_OnRemove(self, param2, eventType, eventId);
}

// ============================================================
// Update (vtable +0x50): Serialize tab/sort state — delegated to exe
// ============================================================
// Original: 0x1400868c0
//
// Finds "tradeTabs" TabElement, serializes active tab index and
// sortCombo selection via IO::Serializer. On load, restores tab
// and combo state.

void TradeUI_Update(TradeUI* self, void* stream, void* p3, int64_t p4) {
    fn_TradeUI_Update(self, stream, p3, p4);
}
