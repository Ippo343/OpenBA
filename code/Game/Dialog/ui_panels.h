#pragma once
// Function declarations for deeply reimplemented UI panel methods.
// These are clean game logic functions called from engine/reimpl/ wrappers.

#include "ui/Panels.h"
#include "ui/DialogController.h"

// ============================================================
// TitleDescToolTipUI
// ============================================================

// Serialize (vtable +0x20): Finds "textTitle" and "textDesc" elements in the
// parent dialog, reads user data from the controller's data source (+0x28),
// and sets them as text parameters on the respective elements.
void TitleDescToolTipUI_Serialize(TitleDescToolTipUI* self);

// ============================================================
// RadiusUI
// ============================================================

// ============================================================
// BuildRequirementUI
// ============================================================

// Slot4 (vtable +0x20): Resolves description, finds ToolbarDescription,
// sets icon sprite and title text, populates label elements with resource requirements.
void BuildRequirementUI_Slot4(BuildRequirementUI* self);

// ============================================================
// ConsumeProduceUI
// ============================================================

// Slot4 (vtable +0x20): Setup — finds combo/progress elements, populates combo with products.
void ConsumeProduceUI_Slot4(ConsumeProduceUI* self);

// Slot7 (vtable +0x38): Updates production progress bar with ratio and percentage.
void ConsumeProduceUI_Slot7(ConsumeProduceUI* self);

// Slot9 (vtable +0x48): Handles combo selection — writes product index to component.
void ConsumeProduceUI_Slot9(ConsumeProduceUI* self, uint64_t param2,
                            int param3, int param4);

// ============================================================
// StorageCapacityUI
// ============================================================

// Serialize (vtable +0x20): Finds ProgressElement by type, stores at +0x30.
void StorageCapacityUI_Serialize(StorageCapacityUI* self);

// Startup (vtable +0x38): Queries storage count, calculates fill ratio, updates progress.
void StorageCapacityUI_Startup(StorageCapacityUI* self);

// ============================================================
// StatusIconUI
// ============================================================

// Serialize (vtable +0x20): Finds "icons" container, caches child element pointers.
void StatusIconUI_Serialize(StatusIconUI* self);

// Startup (vtable +0x38): Updates icon visibility based on MeshInstance bitfield and page state.
void StatusIconUI_Startup(StatusIconUI* self);

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

// Serialize (vtable +0x20): Finds worker/job count elements, sets IDs, checks job visibility.
void WorkplaceUI_Serialize(WorkPlaceUI* self);

// Startup (vtable +0x38): Queries components, displays worker count/max,
// enables/disables up/down buttons based on state.
void WorkplaceUI_Startup(WorkPlaceUI* self);

// OnRemove (vtable +0x48): Dispatches worker count up/down, job count up/down,
// and workplace toggle button clicks.

// ============================================================
// UpgradeUI
// ============================================================

// Serialize (vtable +0x20): Finds button element, creates sprite from
// ToolbarDescription, sets on button's child image element.
void UpgradeUI_Serialize(UpgradeUI* self);

// OnRemove (vtable +0x48): On button click (ID 33000), finds DestroyComponent,
// copies upgrade description reference, and starts upgrade.
void UpgradeUI_OnRemove(UpgradeUI* self, void* p2, int eventType, int eventId);

// ============================================================
// HelpUI
// ============================================================

// ============================================================
// AttendanceUI
// ============================================================

// Slot4 (vtable +0x20): Finds ProgressElement, reads attendance capacity, sets progress max.
void AttendanceUI_Slot4(AttendanceUI* self);

// Slot7 (vtable +0x38): Updates progress bar with current attendance count.
void AttendanceUI_Slot7(AttendanceUI* self);

// ============================================================
// CemeteryUI
// ============================================================

// Slot4 (vtable +0x20): Finds ProgressElement, reads cemetery max slots, sets progress max.
void CemeteryUI_Slot4(CemeteryUI* self);

// Slot7 (vtable +0x38): Updates progress bar with burial count and max slots text.
void CemeteryUI_Slot7(CemeteryUI* self);

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

// Serialize (vtable +0x20): Finds storage table, counts resource types via
// ExternalFactory, resizes table, clones template elements, caches row elements.
void StorageUI_Serialize(StorageUI* self);

// Startup (vtable +0x38): Queries StorageComponent entries, builds sorted
// resource list, populates table rows with sprite/name/count, hides unused rows.
void StorageUI_Startup(StorageUI* self);

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

// Draw (vtable +0x28): Finds all named UI elements (~35), builds icon arrays
// for hearts/stars, sets element IDs on status icons, loads initial map name.
void StatusBarUI_Draw(StatusBarUI* self);

// Startup (vtable +0x38): Main HUD update — iterates citizens for status counts,
// updates health/happiness icons, resource amounts, weather, temperature, date.
void StatusBarUI_Startup(StatusBarUI* self);

// OnRemove (vtable +0x48): Status icon click handler — cycles to next citizen
// with the clicked status problem (home/food/cold/job/tool/disease),
// navigates camera and opens CitizenUI for them.
// Icon IDs 100-105 map to statusBit (1<<0)..(1<<5).
void StatusBarUI_OnRemove(StatusBarUI* self, void* param2, int eventType, int eventId);

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

// Serialize (vtable +0x20): Finds "tabs" TabElement, stores at +0x30,
// then calls 6 sub-functions to initialize each tab's UI elements.
void TownHallUI_Serialize(TownHallUI* self);

// Startup (vtable +0x38): Updates General/Professions/Production/Agriculture tabs.
void TownHallUI_Startup(TownHallUI* self);

// OnCreate (vtable +0x40): Checks game tick, updates graph + inventory.
void TownHallUI_OnCreate(TownHallUI* self);

// OnRemove (vtable +0x48): Event dispatcher — profession buttons/spinners, graph combos.
void TownHallUI_OnRemove(TownHallUI* self, void* param2, int msg, int id);

// Update (vtable +0x50): IO::Serializer save/load of tab index and
// 5 combo box selections. On load, restores tab/combo states and
// triggers graph update.
void TownHallUI_Update(TownHallUI* self, void* stream);

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

// Address-registry entry points (Lane 3 wave). Thin wrappers around the
// static *_Impl bodies in debugui.cpp; see notes/wave_remaining/LANE3_DEBUGMENU_REPORT.md.
void DebugUI_OnDiseaseComboChanged(DebugUI* self);          // 0x14005fc60
void Game_SpawnTornado(void* unused);                       // 0x14005fd20
void Game_InfectRandomCitizen(void* unused);                // 0x14005fe10
void Game_StartRandomFire(void* unused);                    // 0x14005feb0
void DebugUI_TriggerDisasterOnBuilding(DebugUI* self);      // 0x14005ff30
void Game_KillAllCitizens();                                // 0x140060000 (destroys raw materials — see report)
void Game_TriggerDisasterByType(void* unused, int type);    // 0x140060110
void Game_SpawnCitizens();                                  // 0x140060380
void DebugUI_OnDaySliderChanged(DebugUI* self);              // 0x1400607e0
void DebugUI_WriteLightingColors(DebugUI* self, int month);  // 0x140061530
void DebugUI_SetDayLighting(DebugUI* self, int month);       // 0x140061780

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
