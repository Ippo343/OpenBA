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

// Slot6 (vtable +0x30): Removes entity from world tracking set.
void EntityUI_Slot6(EntityUI* self);

// ============================================================
// OrchardUI
// ============================================================

// Slot7 (vtable +0x38): Updates growth progress bar with percentage text.
void OrchardUI_Slot7(OrchardUI* self);

// Slot11 (vtable +0x58): Sets crop/resource type on OrchardComponent.
void OrchardUI_Slot11(OrchardUI* self, int64_t param2);

// Slot12 (vtable +0x60): Gets crop/resource type from OrchardComponent.
void* OrchardUI_Slot12(OrchardUI* self, void* param2);

// ============================================================
// PastureUI
// ============================================================

// Slot11 (vtable +0x58): Sets animal/resource type, recalcs max count, updates slider.
void PastureUI_Slot11(PastureUI* self, int64_t param2);

// Slot12 (vtable +0x60): Gets animal/resource type from PastureComponent.
void* PastureUI_Slot12(PastureUI* self, void* param2);
