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

// Slot5 (vtable +0x28): Calculates radius overlay from building center tile.
void RadiusUI_Slot5(RadiusUI* self);

// Slot6 (vtable +0x30): Removes active radius overlay and resets index.
void RadiusUI_Slot6(RadiusUI* self);

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

// Slot7 (vtable +0x38): Enables/disables combo based on item count.
void CropUI_Slot7(CropUI* self);

// Slot8 (vtable +0x40): Repopulates combo if item count changed.
void CropUI_Slot8(CropUI* self);

// ============================================================
// DestroyUI
// ============================================================

// Slot4 (vtable +0x20): Finds all named elements, sets button IDs, checks reclaim state.
void DestroyUI_Slot4(DestroyUI* self);

// Slot7 (vtable +0x38): Visibility state machine for destroy/reclaim panel.
void DestroyUI_Slot7(DestroyUI* self);

// Slot9 (vtable +0x48): Dispatches reclaim/rebuild/destroy actions.
void DestroyUI_Slot9(DestroyUI* self, uint64_t param2, int param3, int param4);

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

// Slot4 (vtable +0x20): Finds 15 named elements (imageMale/Female, textName,
// numberAge, textProfession, textEducated*, textClothing, textTool, textNameIs,
// textAction, groupHearts, groupStars, buttonProfession/Home/Follow).
// Builds icon arrays for hearts/stars, sets button element IDs.
void CitizenUI_Slot4(CitizenUI* self);

// Slot6 (vtable +0x30): Closes sub-dialog if open, clears reference.
void CitizenUI_Slot6(CitizenUI* self);

// Slot7 (vtable +0x38): Updates citizen display — gender icons, name, age,
// profession, education, clothing, tool, health hearts, happiness stars,
// button states, action text.
void CitizenUI_Slot7(CitizenUI* self);

// Slot9 (vtable +0x48): Button dispatch — profession (0x227) navigates to
// workplace, home (0x226) navigates to residence, follow (0x228) opens
// follow dialog (delegated to original exe function).
void CitizenUI_Slot9(CitizenUI* self, uint64_t param2, int param3, int param4);

// ============================================================
// ResidenceUI
// ============================================================

// Slot4 (vtable +0x20): Finds residence table element, iterates rows to find
// child elements (buttonGoto, textName, imageMale, imageFemale, textAge,
// textProfession), stores in dynamic array at +0x30.
void ResidenceUI_Slot4(ResidenceUI* self);

// Slot7 (vtable +0x38): Updates resident display — iterates storage component rooms,
// shows gender icons, name, age, profession per occupied row. Hides unused rows.
void ResidenceUI_Slot7(ResidenceUI* self);

// Slot9 (vtable +0x48): Dispatches resident row clicks (ID 300+index) to navigate
// camera to that citizen.
void ResidenceUI_Slot9(ResidenceUI* self, void* param2, int msg, int id);

// ============================================================
// EventLogUI
// ============================================================

// Slot5 (vtable +0x28): Finds navigation buttons, display container, and
// notification buttons. Allocates display arrays, sets element IDs.
void EventLogUI_Slot5(EventLogUI* self);

// Slot7 (vtable +0x38): Updates event log display rows from event data,
// enables/disables scroll buttons based on offset.
void EventLogUI_Slot7(EventLogUI* self);

// Slot9 (vtable +0x48): Handles scroll buttons (prev/next/bottom),
// notification toggles, and row click navigation.
void EventLogUI_Slot9(EventLogUI* self, void* param2, int msg, int id);

// ============================================================
// ControlUI
// ============================================================

// Slot4 (vtable +0x20): Finds checkbox element, finds sub-component, sets initial state.
void ControlUI_Slot4(ControlUI* self);

// Slot7 (vtable +0x38): Updates checkbox state from component, sets enabled/disabled.
void ControlUI_Slot7(ControlUI* self);

// Slot9 (vtable +0x48): On checkbox click, toggles component state.
void ControlUI_Slot9(ControlUI* self, uint64_t* param2, int param3, int param4);

// ============================================================
// BuildUI
// ============================================================

// ============================================================
// EntityUI
// ============================================================

// Slot4 (vtable +0x20): Startup — finds buttonCenter/checkPin/pages elements,
// sets IDs, gets ComponentEntity from data source, resets ToolUI controller flags.
void EntityUI_Slot4(EntityUI* self);

// Slot6 (vtable +0x30): Removes entity from world tracking set.
void EntityUI_Slot6(EntityUI* self);

// Slot7 (vtable +0x38): Determines active page based on entity state
// (build, destroy, profession). Updates ToolUI controller hint.
void EntityUI_Slot7(EntityUI* self);

// Slot8 (vtable +0x40): Input handler — reads mouse position, checks key 5 press,
// selects entity in edit tool if mouse is over this dialog.
void EntityUI_Slot8(EntityUI* self);

// Slot9 (vtable +0x48): Button dispatch — close (1), center (0x65), pin (100).
void EntityUI_Slot9(EntityUI* self, void* param2, int msg, int id);

// ============================================================
// OrchardUI
// ============================================================

// Slot4 (vtable +0x20): Finds "yield" progress element, sets to 1.0f, calls base Slot4.
void OrchardUI_Slot4(OrchardUI* self);

// Slot7 (vtable +0x38): Updates growth progress bar with percentage text.
void OrchardUI_Slot7(OrchardUI* self);

// Slot11 (vtable +0x58): Sets crop/resource type on OrchardComponent.
void OrchardUI_Slot11(OrchardUI* self, int64_t param2);

// Slot12 (vtable +0x60): Gets crop/resource type from OrchardComponent.
void* OrchardUI_Slot12(OrchardUI* self, void* param2);

// ============================================================
// SelectResourceUI (shared base for CropUI, OrchardUI, PastureUI)
// ============================================================

// Slot4 (vtable +0x20): Finds combo element, sets ID 500, populates resources.
void SelectResourceUI_Slot4(void* self);

// Slot8 (vtable +0x40): Checks combo item count, repopulates if changed.
void SelectResourceUI_Slot8(void* self);

// Slot9 (vtable +0x48): On combo selection, looks up resource type, calls Slot11.
void SelectResourceUI_Slot9(void* self, void* param2, int param3, int param4);

// ============================================================
// PastureUI
// ============================================================

// Slot4 (vtable +0x20): Initializes combo + slider elements, sets IDs, updates slider.
void PastureUI_Slot4(PastureUI* self);

// Slot7 (vtable +0x38): Enables/disables combo based on animal count, updates slider text.
void PastureUI_Slot7(PastureUI* self);

// Slot9 (vtable +0x48): Combo selection (0x14/500) + slider change (0x17/501) handler.
void PastureUI_Slot9(PastureUI* self, void* param2, int param3, int param4);

// Slot11 (vtable +0x58): Sets animal/resource type, recalcs max count, updates slider.
void PastureUI_Slot11(PastureUI* self, int64_t param2);

// Slot12 (vtable +0x60): Gets animal/resource type from PastureComponent.
void* PastureUI_Slot12(PastureUI* self, void* param2);

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

// Slot4 (vtable +0x20): Finds 50+ UI elements, assigns element IDs,
// populates disease/draw/weather combos, sets slider ranges,
// initializes day lighting from game time.
void DebugUI_Slot4(DebugUI* self);

// Slot7 (vtable +0x38): Reads day slider and RGB color sliders,
// formats day text, packs ARGB colors, updates 3 color preview swatches.
void DebugUI_Slot7(DebugUI* self);

// Slot8 (vtable +0x40): Reads 4 debug counters from engine state,
// smooths FPS/frame timing with EMA (alpha=0.95), displays as numbers.
void DebugUI_Slot8(DebugUI* self);

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

// OrchardUI::Slot13/GetCategoryFilter (vtable +0x68, 0x140075420): returns 1.
int OrchardUI_Slot13(OrchardUI* self);

// PastureUI::Slot13/GetCategoryFilter (vtable +0x68, 0x1400B4CF0): returns 2.
int PastureUI_Slot13(PastureUI* self);

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
