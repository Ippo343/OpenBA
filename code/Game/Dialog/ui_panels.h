#pragma once
// Function declarations for deeply reimplemented UI panel methods.
// These are clean game logic functions called from engine/reimpl/ wrappers.

#include "ui/Panels.h"
#include "ui/DialogController.h"

// ============================================================
// TitleDescToolTipUI
// ============================================================

// ============================================================
// RadiusUI
// ============================================================

// ============================================================
// BuildRequirementUI
// ============================================================

// ============================================================
// StorageCapacityUI
// ============================================================

// ============================================================
// StatusIconUI
// ============================================================

// ============================================================
// CropUI
// ============================================================

// Slot4 (vtable +0x20): Finds "selectResource" combo, sets ID 500, populates crop list.
void CropUI_Slot4(CropUI* self);

// Slot8 (vtable +0x40): Repopulates combo if item count changed.
void CropUI_Slot8(CropUI* self);

// ============================================================
// DestroyUI
// ============================================================

// ============================================================
// ImmigrationUI
// ============================================================

// ============================================================
// WorkplaceUI
// ============================================================

// OnRemove (vtable +0x48): Dispatches worker count up/down, job count up/down,
// and workplace toggle button clicks.

// ============================================================
// UpgradeUI
// ============================================================

// ============================================================
// HelpUI
// ============================================================

// ============================================================
// AttendanceUI
// ============================================================

// ============================================================
// CemeteryUI
// ============================================================

// ============================================================
// ResourceLimitUI
// ============================================================

// Slot4 (vtable +0x20): Finds "editLimit" spinner, configures min/max, sets current value.
void ResourceLimitUI_Slot4(ResourceLimitUI* self);

// Slot7 (vtable +0x38): Syncs spinner with current resource limit value.
void ResourceLimitUI_Slot7(ResourceLimitUI* self);

// Slot9 (vtable +0x48): Writes spinner value back to global resource limit array.
void ResourceLimitUI_Slot9(ResourceLimitUI* self, void* param2,
                           int param3, int param4);

// ============================================================
// PageToggleUI
// ============================================================

// ============================================================
// ProfessionUI
// ============================================================

// ============================================================
// ProductionUI
// ============================================================

// ============================================================
// CitizenUI
// ============================================================

// ============================================================
// ResidenceUI
// ============================================================

// ============================================================
// EventLogUI
// ============================================================

// ============================================================
// ControlUI
// ============================================================

// ============================================================
// BuildUI
// ============================================================

// ============================================================
// EntityUI
// ============================================================

// Slot8 (vtable +0x40): Input handler — reads mouse position, checks key 5 press,
// selects entity in edit tool if mouse is over this dialog.
void EntityUI_Slot8(EntityUI* self);

// ============================================================
// OrchardUI
// ============================================================

// ============================================================
// SelectResourceUI (shared base for CropUI, OrchardUI, PastureUI)
// ============================================================

// ============================================================
// PastureUI
// ============================================================

// ============================================================
// StorageUI
// ============================================================

// ============================================================
// ToolUI
// ============================================================

// ============================================================
// MapUI
// ============================================================

// ============================================================
// TradeUI
// ============================================================

// Sub-functions (called by event handlers and Startup):
void TradeUI_UpdateBuyTotals(TradeUI* self);
void TradeUI_UpdateSellTotals(TradeUI* self);
void TradeUI_OnBuySpinner(TradeUI* self, int idx);
void TradeUI_OnSellSpinner(TradeUI* self, int idx);
void TradeUI_OnComboPurchase(TradeUI* self);
void TradeUI_OnComboOrder(TradeUI* self);
void TradeUI_OnOrderAmountSpinner(TradeUI* self, int idx);
void TradeUI_OnOrderSpinner(TradeUI* self, int idx, int idx2);
void TradeUI_OnDismissButton(TradeUI* self);
void TradeUI_OnPurchaseButton(TradeUI* self, int idx);
void TradeUI_OnStorageSpinner(TradeUI* self, int idx, void* reserved);
void TradeUI_RefreshDisplay(TradeUI* self, uint64_t p2, uint64_t p3, int64_t p4);
void TradeUI_RefreshSellTable(TradeUI* self);
void TradeUI_RefreshBuyTable(TradeUI* self);
void TradeUI_RefreshOrderQuantityDisplay(TradeUI* self);
void TradeUI_RefreshSellSpinnerValues(TradeUI* self);
void TradeUI_RefreshOrderDisplay(TradeUI* self);

// ============================================================
// StatusBarUI
// ============================================================

// ============================================================
// NoiseUI
// ============================================================

// Slot4 (vtable +0x20): Allocates 256x256 ImageBuffer, fills black,
// creates MaterialInstance/SpriteSheet, finds 5 slider elements
// ("noisePersistance", "noiseOctaves", "noiseScale", "noiseCutOff",
// "noiseGradient") and "buttonGenerate" button (ID 1000).
void NoiseUI_Slot4(NoiseUI* self);

// Slot9 (vtable +0x48): On button click (msg=0x12, id=1000),
// regenerates Perlin noise texture using current slider values.
void NoiseUI_Slot9(NoiseUI* self, void* param2, int msg, int id,
                   int param5, int param6);

// ============================================================
// TownHallUI
// ============================================================

// ============================================================
// SpriteCaptureUI
// ============================================================

// Serialize (vtable +0x20): Finds editCapture/buttonCapture/captureBox elements,
// sets button ID 1000, sets default capture path, initializes capture state.
void SpriteCaptureUI_Serialize(SpriteCaptureUI* self);

// OnCreate (vtable +0x40): Per-frame update — drag capture box with left mouse + key 0x3E.
// When pendingExternal set and saveFrameCount reaches 1, EndSpriteCapture + SaveText.
void SpriteCaptureUI_OnCreate(SpriteCaptureUI* self);

// ============================================================
// DebugUI
// ============================================================

// Slot9 (vtable +0x48): Event dispatch — handles button clicks (resource spawn,
// disasters, debug toggles), combo changes (disease/draw), slider changes (day/colors).
void DebugUI_Slot9(DebugUI* self, uint64_t param2, int param3, int param4);

// ToggleDebugFlag: helper for checkbox cases in Slot9 — reads checkbox state,
// sets/clears a bit in game world debug flags at game+0xD0.
void DebugUI_ToggleDebugFlag(void* checkboxElement, uint32_t flag);

// ============================================================
// Unhookable no-op stubs for standalone exe
// ============================================================
// These are 3-byte "xor eax,eax; ret" or similar no-ops in the original exe.
// They must exist as named functions for the standalone exe vtable.

// BuildRequirementUI::Shutdown (vtable +0x18, 0x14005EE80): 3-byte RET 0 no-op.
inline void BuildRequirementUI_Shutdown(BuildRequirementUI* /*self*/) {}

// DebugUI::Slot6 (vtable +0x30, 0x1400645F0): 3-byte RET 0 no-op.
inline void DebugUI_Slot6(DebugUI* /*self*/) {}

// EntityUI::Shutdown (vtable +0x18, 0x1400672A0): 3-byte RET 0 no-op.
inline void EntityUI_Shutdown(EntityUI* /*self*/) {}

// EventLogUI::Slot6 (vtable +0x30, 0x140067630): 3-byte RET 0 no-op.
inline void EventLogUI_Slot6(EventLogUI* /*self*/) {}

// HelpUI::Slot5 (vtable +0x28, 0x14006A4F0): 3-byte RET 0 no-op.
inline void HelpUI_Slot5(HelpUI* /*self*/) {}

// MapUI::Slot3/Shutdown (vtable +0x18, 0x14006CE80): 3-byte RET 0 no-op.
inline void MapUI_Slot3(MapUI* /*self*/) {}

// ModBrowseUI::Slot3/Shutdown (vtable +0x18, 0x14006D390): 3-byte RET 0 no-op.
inline void ModBrowseUI_Slot3(void* /*self*/) {}

// NoiseUI::Slot3/Shutdown (vtable +0x18, 0x140071BB0): 3-byte RET 0 no-op.
inline void NoiseUI_Slot3(NoiseUI* /*self*/) {}

// PageToggleUI::Slot3/Shutdown (vtable +0x18, 0x1400759A0): 3-byte RET 0 no-op.
inline void PageToggleUI_Slot3(PageToggleUI* /*self*/) {}

// ProductionUI::Slot3/Shutdown (vtable +0x18, 0x140076300): 3-byte RET 0 no-op.
inline void ProductionUI_Slot3(ProductionUI* /*self*/) {}

// ResidenceUI::Slot3/Shutdown (vtable +0x18, 0x1400B5DB0): 3-byte RET 0 no-op.
inline void ResidenceUI_Slot3(ResidenceUI* /*self*/) {}

// SpriteCaptureUI::Slot3/Shutdown (vtable +0x18, 0x140079850): 3-byte RET 0 no-op.
inline void SpriteCaptureUI_Slot3(SpriteCaptureUI* /*self*/) {}

// StorageUI::Shutdown (vtable +0x18, 0x1400b8ca0): 3-byte RET 0 no-op.
inline void StorageUI_Shutdown(StorageUI* /*self*/) {}

// StorageCapacityUI::Shutdown (vtable +0x18, 0x14007b6b0): 3-byte RET 0 no-op.
inline void StorageCapacityUI_Shutdown(StorageCapacityUI* /*self*/) {}

// TitleDescToolTipUI::Shutdown (vtable +0x18, 0x14007b8a0): 3-byte RET 0 no-op.
inline void TitleDescToolTipUI_Shutdown(TitleDescToolTipUI* /*self*/) {}

// ToolUI::Shutdown (vtable +0x18, 0x14007c070): 3-byte RET 0 no-op.
inline void ToolUI_Shutdown(ToolUI* /*self*/) {}

// UpgradeUI::Startup (vtable +0x38, 0x140086d80): 3-byte RET 0 no-op.
inline void UpgradeUI_Startup(UpgradeUI* /*self*/) {}

// StatusBarUI::Slot6 (vtable +0x30, 0x14007a3c0): 3-byte RET 0 no-op.
inline void StatusBarUI_Slot6(StatusBarUI* /*self*/) {}
