// code/Game/Dialog/optionsdialog.cpp
// Clean reimplementation of OptionsDialog methods.
//
// OptionsDialog (0x1C0 bytes): Game options panel (audio, video, input, gameplay).
// Layout:
//   +0x00..+0xB7 = DialogInstance base
//   +0xB8..+0x1B8 = UI element pointers and saved settings
//
// Original source: C:\Projects\Buildy\code\Game\Dialog/OptionsDialog.h
//                  Dialog\OptionsDialog.cpp
//
// NOTE: Startup is NOT reimplemented here — it is deeply dependent on
// _singleton_exref (per-instruction relocations to video/audio/input config
// singletons) that we cannot replicate in clean code without resolving all
// those singletons. The trampoline passthrough is used for Startup.
//
// OnEvent IS reimplemented: the dispatch logic is clean, with complex handlers
// delegated to original exe helper functions via resolved function pointers.

#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Element IDs assigned in Startup, dispatched in OnEvent
// ============================================================
enum OptionsElementId {
    // Slider IDs (msg 0x17)
    kSlider_UI          = 0,
    kSlider_Music       = 1,
    kSlider_Ambient     = 2,
    kSlider_Effect      = 3,
    // Combo IDs (msg 0x14)
    kCombo_Renderer     = 4,
    kCombo_Adapter      = 5,
    kCombo_Resolution   = 6,
    kCombo_Refresh      = 7,
    kCombo_Antialias    = 8,
    kCombo_Filter       = 9,
    // Button IDs (msg 0x12)
    kButton_Ok          = 10,
    kButton_Cancel      = 11,
    kButton_Apply       = 12,
    // Input bind buttons start at ID 13 (0x0D)
    kInputBind_Base     = 13,
};

// ============================================================
// OnEvent (0x140072f20): Handle options changes
// ============================================================
// Dispatches events by type:
//   0x12 (button click): OK applies+saves+closes, Cancel reverts+closes,
//                        Apply applies+saves, input bind buttons open bind dialog
//   0x14 (combo selection): reads new selection, updates saved settings,
//                           validates config, repopulates dependent combos
//   0x17 (slider change): writes float value back to audio config singleton

void OptionsDialog_OnEvent(OptionsDialog* self, void* element, int eventType, int eventData) {

    if (eventType == 0x12) {
        // --- Button click ---
        if (eventData == kButton_Ok) {
            fn_OptionsDialog_OnOk(self);
            return;
        }
        if (eventData == kButton_Cancel) {
            fn_OptionsDialog_OnCancel(self);
            return;
        }
        if (eventData == kButton_Apply) {
            fn_OptionsDialog_OnApply(self);
            return;
        }
        // Input bind button clicks: IDs >= 13
        if (eventData >= kInputBind_Base) {
            fn_InternalBasePtr_CheckPtr(&element);
            void* parent = fn_Element_GetParent(*(void**)element);
            uint8_t parentPtr[16];
            fn_InternalBasePtr_Ctor(parentPtr, parent);

            // Calculate bind index and slot from element ID
            // Original: bindIndex = (id - 0xD) / 2, slot = (id - 0xD) & 1 (sign-preserving)
            unsigned int offset = (unsigned int)(eventData - kInputBind_Base);
            unsigned int slot = offset & 0x80000001;
            if ((int)slot < 0) {
                slot = ((slot - 1) | 0xFFFFFFFE) + 1;
            }
            int bindIndex = (int)offset / 2;

            fn_OptionsDialog_OnInputBindClick(self, parentPtr, bindIndex, slot);
        }
        return;
    }

    if (eventType == 0x14) {
        // --- Combo selection change ---
        if (eventData == kCombo_Renderer) {
            // Renderer changed: update saved index, validate, repopulate adapter chain
            fn_InternalBasePtr_CheckPtr(&self->comboRenderer);
            int curItem = *(int*)((uintptr_t)self->comboRenderer + 0x118);
            fn_InternalBasePtr_CheckPtr(&self->comboRenderer);
            void* data = fn_ComboElement_GetItemData(self->comboRenderer, curItem);
            self->savedRenderer = (uint32_t)(intptr_t)data;
            fn_OptionsDialog_ValidateVideoConfig(self);
            fn_OptionsDialog_PopulateAdapterCombo(self);
            return;
        }
        if (eventData == kCombo_Adapter) {
            // Adapter changed: update saved index, validate, repopulate dependent combos
            fn_InternalBasePtr_CheckPtr(&self->comboAdapter);
            int curItem = *(int*)((uintptr_t)self->comboAdapter + 0x118);
            fn_InternalBasePtr_CheckPtr(&self->comboAdapter);
            void* data = fn_ComboElement_GetItemData(self->comboAdapter, curItem);
            self->savedAdapter = (uint32_t)(intptr_t)data;
            fn_OptionsDialog_ValidateVideoConfig(self);
            fn_OptionsDialog_PopulateAntialiasCombo(self);
            fn_OptionsDialog_PopulateFilterCombo(self);
            fn_OptionsDialog_PopulateResolutionCombo(self);
            return;
        }
        if (eventData == kCombo_Resolution) {
            // Resolution changed: read width/height from adapter display mode
            fn_InternalBasePtr_CheckPtr(&self->comboResolution);
            int curItem = *(int*)((uintptr_t)self->comboResolution + 0x118);
            fn_InternalBasePtr_CheckPtr(&self->comboResolution);
            void* resData = fn_ComboElement_GetItemData(self->comboResolution, curItem);

            int adapterIdx = self->savedAdapter;
            void* gfxIface = fn_VideoDevice_GetInterface(*g_pVideoDeviceSingleton, self->savedRenderer);
            int adapterIdx2 = self->savedAdapter;

            // adapter[adapterIdx].displayModes[resolutionIdx] = {width, height, ...}
            // Adapter stride = 0x38, mode list at adapter+8, mode stride = 0x18
            intptr_t adapterArray = *(intptr_t*)((uintptr_t)gfxIface + 8);
            intptr_t displayModes = *(intptr_t*)(adapterArray + (intptr_t)adapterIdx * 0x38 + 8);
            intptr_t modeEntry = displayModes + (intptr_t)(int)(intptr_t)resData * 0x18;

            self->savedWidth = *(uint32_t*)modeEntry;

            gfxIface = fn_VideoDevice_GetInterface(*g_pVideoDeviceSingleton, self->savedRenderer);
            adapterArray = *(intptr_t*)((uintptr_t)gfxIface + 8);
            displayModes = *(intptr_t*)(adapterArray + (intptr_t)adapterIdx2 * 0x38 + 8);
            self->savedHeight = *(uint32_t*)(displayModes + (intptr_t)(int)(intptr_t)resData * 0x18 + 4);

            fn_OptionsDialog_ValidateVideoConfig(self);
            fn_OptionsDialog_PopulateRefreshCombo(self);
            return;
        }
        if (eventData == kCombo_Refresh) {
            // Refresh changed: read numerator/denominator from refresh array
            fn_InternalBasePtr_CheckPtr(&self->comboResolution);
            int resItem = *(int*)((uintptr_t)self->comboResolution + 0x118);
            fn_InternalBasePtr_CheckPtr(&self->comboResolution);
            void* resData = fn_ComboElement_GetItemData(self->comboResolution, resItem);

            fn_InternalBasePtr_CheckPtr(&self->comboRefresh);
            int refItem = *(int*)((uintptr_t)self->comboRefresh + 0x118);
            fn_InternalBasePtr_CheckPtr(&self->comboRefresh);
            void* refData = fn_ComboElement_GetItemData(self->comboRefresh, refItem);

            int adapterIdx = self->savedAdapter;
            void* gfxIface = fn_VideoDevice_GetInterface(*g_pVideoDeviceSingleton, self->savedRenderer);
            int adapterIdx2 = self->savedAdapter;

            intptr_t adapterArray = *(intptr_t*)((uintptr_t)gfxIface + 8);
            intptr_t displayMode = adapterArray + (intptr_t)adapterIdx * 0x38;
            intptr_t modeEntry = *(intptr_t*)(displayMode + 8) + (intptr_t)(int)(intptr_t)resData * 0x18;
            intptr_t refreshArray = *(intptr_t*)(modeEntry + 8);
            intptr_t refreshEntry = refreshArray + (intptr_t)(int)(intptr_t)refData * 8;

            self->savedRefreshNum = *(uint32_t*)refreshEntry;

            gfxIface = fn_VideoDevice_GetInterface(*g_pVideoDeviceSingleton, self->savedRenderer);
            adapterArray = *(intptr_t*)((uintptr_t)gfxIface + 8);
            displayMode = adapterArray + (intptr_t)adapterIdx2 * 0x38;
            modeEntry = *(intptr_t*)(displayMode + 8) + (intptr_t)(int)(intptr_t)resData * 0x18;
            refreshArray = *(intptr_t*)(modeEntry + 8);
            self->savedRefreshDen = *(uint32_t*)(refreshArray + (intptr_t)(int)(intptr_t)refData * 8 + 4);

            fn_OptionsDialog_ValidateVideoConfig(self);
            return;
        }
        if (eventData == kCombo_Antialias) {
            // Antialias changed: store new value
            fn_InternalBasePtr_CheckPtr(&self->comboAntialias);
            int curItem = *(int*)((uintptr_t)self->comboAntialias + 0x118);
            fn_InternalBasePtr_CheckPtr(&self->comboAntialias);
            void* data = fn_ComboElement_GetItemData(self->comboAntialias, curItem);
            self->savedAntialias = (uint32_t)(intptr_t)data;
            fn_OptionsDialog_ValidateVideoConfig(self);
            return;
        }
        if (eventData == kCombo_Filter) {
            // Filter changed: store new value
            fn_InternalBasePtr_CheckPtr(&self->comboFilter);
            int curItem = *(int*)((uintptr_t)self->comboFilter + 0x118);
            fn_InternalBasePtr_CheckPtr(&self->comboFilter);
            void* data = fn_ComboElement_GetItemData(self->comboFilter, curItem);
            self->savedFilter = (uint32_t)(intptr_t)data;
            fn_OptionsDialog_ValidateVideoConfig(self);
            return;
        }
        return;
    }

    if (eventType == 0x17) {
        // --- Slider value change ---
        // Audio volume sliders write their float value (cast to int) to the
        // audio configuration singleton. The singleton is accessed via
        // _singleton_exref which we can't replicate here.
        // This path is handled by the trampoline fallback in the hook wrapper.
        return;
    }
}
