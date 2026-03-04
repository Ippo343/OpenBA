#pragma once
// Function declarations for deeply reimplemented UI panel methods.
// These are clean game logic functions called from engine/reimpl/ wrappers.
// No dependency on hooks.h or resolve.h — portable to standalone exe.

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
// ConsumeProduceUI
// ============================================================

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

// Slot4 (vtable +0x20): Finds text/button elements, sets button IDs.
void ImmigrationUI_Slot4(ImmigrationUI* self);

// Slot7 (vtable +0x38): Updates immigration count, enables buttons when build complete.
void ImmigrationUI_Slot7(ImmigrationUI* self);

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
void WorkplaceUI_OnRemove(WorkPlaceUI* self, void* param2, int msg, int id);

// ============================================================
// UpgradeUI
// ============================================================

// Serialize (vtable +0x20): Finds button element, creates sprite from
// BuildRequirementComponent, sets on button's child image element.
void UpgradeUI_Serialize(UpgradeUI* self);

// OnRemove (vtable +0x48): On button click (ID 33000), finds DestroyComponent,
// copies upgrade description reference, and starts upgrade.
void UpgradeUI_OnRemove(UpgradeUI* self, void* p2, int eventType, int eventId);

// ============================================================
// HelpUI
// ============================================================

// Slot7 (vtable +0x38): Enables/disables prev/next buttons based on page index.
void HelpUI_Slot7(HelpUI* self);

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

// Slot4 (vtable +0x20): Finds checkbox element by type, finds "pages" PageElement,
// restores active page if valid.
void PageToggleUI_Slot4(PageToggleUI* self);

// Slot7 (vtable +0x38): Enables/disables checkbox based on active page state.
void PageToggleUI_Slot7(PageToggleUI* self);

// Slot9 (vtable +0x48): On checkbox toggle, sets active page (0 or 4).
void PageToggleUI_Slot9(PageToggleUI* self, void* param2, int param3, int param4);

// ============================================================
// TutorialUI
// ============================================================

// Serialize (vtable +0x20): Finds named elements, sets button IDs, hides close button.
void TutorialUI_Serialize(TutorialUI* self);

// Startup (vtable +0x38): Toggles visibility of next/quit buttons based on current flags.
void TutorialUI_Startup(TutorialUI* self);

// ============================================================
// CitizenUI
// ============================================================

// Slot6 (vtable +0x30): Closes sub-dialog if open, clears reference.
void CitizenUI_Slot6(CitizenUI* self);

// ============================================================
// ResidenceUI
// ============================================================

// Slot4 (vtable +0x20): Finds residence table element, iterates rows to find
// child elements (buttonGoto, textName, imageMale, imageFemale, textAge,
// textProfession), stores in dynamic array at +0x30.
void ResidenceUI_Slot4(ResidenceUI* self);

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
void ControlUI_Slot9(ControlUI* self, uint64_t param2, int param3, int param4);

// ============================================================
// BuildUI
// ============================================================

// Slot9 (vtable +0x48): On button click, toggles workplace enabled state.
void BuildUI_Slot9(BuildUI* self, uint64_t param2, int param3, int param4);

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
