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
