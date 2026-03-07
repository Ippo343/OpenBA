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

#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include "resolve.h"
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
// Startup (0x1400742d0): Find all UI elements, assign IDs, populate initial values
// ============================================================
// Layout of audio config singleton (int[4]):
//   +0x00: UI volume (sliderUI)
//   +0x04: Music volume (sliderMusic)
//   +0x08: Ambient volume (sliderAmbient)
//   +0x0C: Effect volume (sliderEffect)
//
// Layout of video config singleton (relevant fields):
//   +0x04: renderer index (savedRenderer)
//   +0x08: adapter index (savedAdapter)
//   +0x0C: resolution width (savedWidth)
//   +0x10: resolution height (savedHeight)
//   +0x24: antialias (savedAntialias)
//   +0x28: filter (savedFilter)
//   +0x2C: shadow resolution combo index
//   +0x30: shadow quality combo index
//   +0x34: reflection quality combo index
//   +0x38: refresh rate numerator (savedRefreshNum)
//   +0x3C: refresh rate denominator (savedRefreshDen)
//   +0x40: fullscreen flag (bool)
//   +0x41: vsync flag (bool)
//
// Layout of g_pGameSettings (int32_t array, but some byte-sized fields):
//   +0x00 [0]: UI scale value (sliderUIScale)
//   +0x04 [1]: autosave interval (comboAutosave selection, 0-8)
//   +0x08 [2]: status icon distance (sliderStatusIcon)
//   +0x0C [3]: status icon enabled (checkStatusIcon byte)
//   +0x10 [4]: status icon scale * conversion constant (sliderStatusIconScale)
//   +0x14 [5]: edge scroll (checkEdgeScroll byte)
//   +0x15:     show celsius (checkShowCelsius byte)
//   +0x16:     show weather (checkShowWeather byte)
//   +0x17:     autoslow (checkAutoslow byte)
//   +0x18 [6]: autopause (checkAutopause byte)
//   +0x1C [7]: last active tab (tabOptions)
//
// Slider range constants (as hex float):
//   0x42c80000 = 100.0f (slider max for audio and status icon distance)
//   0x42960000 = 75.0f  (UI scale min)
//   0x43160000 = 150.0f (UI scale max)
//   0x41c80000 = 25.0f  (status icon scale min)
//   0x43480000 = 200.0f (status icon scale max)
//
// Scale conversion: sliderStatusIconScale = (int)settings[4] * DAT_1401720c4 * DAT_140172208
// (two float multiplier constants for converting integer to display scale)

// Helper macro: find a named element in the dialog and store in outField.
// Uses the fn_DialogInstance_FindElement non-trivial-return ABI:
//   RCX=self, RDX=&retval(uint64_t), R8=name(String const&)
// Returns the raw Internal* pointer stored in the 8-byte InternalPtr result.
#define FIND_ELEMENT(outField, name)                       \
    do {                                                    \
        uint8_t _nameBuf[8];                               \
        uint64_t _retval = 0;                              \
        fn_String_Ctor_CStr(_nameBuf, (name));             \
        fn_DialogInstance_FindElement(self, &_retval, _nameBuf); \
        fn_String_Dtor(_nameBuf);                          \
        (outField) = (void*)_retval;                       \
    } while (0)

// Helper: set checkbox state (byte at +0xD8)
static inline void SetCheckState(void* elem, uint8_t val) {
    if (elem) *(uint8_t*)((uintptr_t)elem + 0xD8) = val;
}

// Helper: set slider min/max/value (float fields at +0x118/+0x11c/+0x128)
static inline void SetSliderRange(void* elem, float minVal, float maxVal) {
    if (!elem) return;
    *(float*)((uintptr_t)elem + 0x118) = minVal;
    *(float*)((uintptr_t)elem + 0x11c) = maxVal;
}
static inline void SetSliderValue(void* elem, float val) {
    if (elem) *(float*)((uintptr_t)elem + 0x128) = val;
}

void OptionsDialog_Startup(OptionsDialog* self) {
    // --- Buttons: find, assign IDs, set buttonCancel's hidden flag ---
    void *buttonOk = nullptr, *buttonCancel = nullptr, *buttonApply = nullptr;
    FIND_ELEMENT(buttonOk,     "buttonOk");
    FIND_ELEMENT(buttonCancel, "buttonCancel");
    FIND_ELEMENT(buttonApply,  "buttonApply");
    fn_Element_SetElementId(buttonOk,     kButton_Ok);
    fn_Element_SetElementId(buttonCancel, kButton_Cancel);
    if (buttonCancel) *(uint32_t*)((uintptr_t)buttonCancel + 0xC0) = 8;
    fn_Element_SetElementId(buttonApply,  kButton_Apply);

    // --- Resolve singletons ---
    // Audio::Configuration singleton: int[4] of volume levels (UI/Music/Ambient/Effect)
    int32_t* audioVol = g_pAudioConfigSingleton
                        ? reinterpret_cast<int32_t*>(*g_pAudioConfigSingleton) : nullptr;

    // Video::Configuration singleton: holds current video settings
    void* vidCfg = g_pVideoConfigSingleton ? *g_pVideoConfigSingleton : nullptr;

    // Input::Configuration singleton (exref 0x140151738):
    //   +0x08: mouse clip flag (bool)
    //   +0x0A: software mouse flag (bool)
    void* inputCfg = nullptr;
    {
        void*** exref = reinterpret_cast<void***>(ResolveAddr(Module::Exe, 0x140151738));
        if (exref && *exref) inputCfg = *exref;
    }

    // --- Audio sliders: find, assign IDs, set range [0,100], value from audio config ---
    FIND_ELEMENT(self->sliderUI,     "sliderUI");
    FIND_ELEMENT(self->sliderMusic,  "sliderMusic");
    FIND_ELEMENT(self->sliderAmbient,"sliderAmbient");
    FIND_ELEMENT(self->sliderEffect, "sliderEffect");

    fn_Element_SetElementId(self->sliderUI,     kSlider_UI);
    fn_Element_SetElementId(self->sliderMusic,  kSlider_Music);
    fn_Element_SetElementId(self->sliderAmbient,kSlider_Ambient);
    fn_Element_SetElementId(self->sliderEffect, kSlider_Effect);

    void* audioSliders[4] = { self->sliderUI, self->sliderMusic,
                               self->sliderAmbient, self->sliderEffect };
    for (int i = 0; i < 4; i++) {
        SetSliderRange(audioSliders[i], 0.0f, 100.0f);
        if (audioVol) SetSliderValue(audioSliders[i], (float)audioVol[i]);
    }

    // --- Video combo boxes: find and assign IDs ---
    FIND_ELEMENT(self->comboRenderer,  "comboRenderer");
    FIND_ELEMENT(self->comboAdapter,   "comboAdapter");
    FIND_ELEMENT(self->comboResolution,"comboResolution");
    FIND_ELEMENT(self->comboAntialias, "comboAntialias");
    FIND_ELEMENT(self->comboFilter,    "comboFilter");
    FIND_ELEMENT(self->comboRefresh,   "comboRefresh");

    fn_Element_SetElementId(self->comboRenderer,   kCombo_Renderer);
    fn_Element_SetElementId(self->comboAdapter,    kCombo_Adapter);
    fn_Element_SetElementId(self->comboResolution, kCombo_Resolution);
    fn_Element_SetElementId(self->comboRefresh,    kCombo_Refresh);
    fn_Element_SetElementId(self->comboAntialias,  kCombo_Antialias);
    fn_Element_SetElementId(self->comboFilter,     kCombo_Filter);

    // --- Save current video config into OptionsDialog fields ---
    if (vidCfg) {
        self->savedRenderer   = *(uint32_t*)((uintptr_t)vidCfg + 0x04);
        self->savedAdapter    = *(uint32_t*)((uintptr_t)vidCfg + 0x08);
        self->savedWidth      = *(uint32_t*)((uintptr_t)vidCfg + 0x0C);
        self->savedHeight     = *(uint32_t*)((uintptr_t)vidCfg + 0x10);
        self->savedRefreshNum = *(uint32_t*)((uintptr_t)vidCfg + 0x38);
        self->savedRefreshDen = *(uint32_t*)((uintptr_t)vidCfg + 0x3C);
        self->savedAntialias  = *(uint32_t*)((uintptr_t)vidCfg + 0x24);
        self->savedFilter     = *(uint32_t*)((uintptr_t)vidCfg + 0x28);
    }

    // --- Populate renderer/adapter/resolution/refresh/antialias/filter combos ---
    fn_OptionsDialog_PopulateVideoComboBoxes(self);

    // --- Shadow/reflection quality combos: find, then set from video config ---
    FIND_ELEMENT(self->comboShadowRes,        "comboShadowResolution");
    FIND_ELEMENT(self->comboShadowQuality,    "comboShadowQuality");
    FIND_ELEMENT(self->comboReflectionQuality,"comboReflectionQuality");
    if (vidCfg) {
        fn_ComboElement_SetCurrentItem(self->comboShadowRes,
            *(int32_t*)((uintptr_t)vidCfg + 0x2C));
        fn_ComboElement_SetCurrentItem(self->comboShadowQuality,
            *(int32_t*)((uintptr_t)vidCfg + 0x30));
        fn_ComboElement_SetCurrentItem(self->comboReflectionQuality,
            *(int32_t*)((uintptr_t)vidCfg + 0x34));
    }

    // --- Fullscreen / VSync checkboxes: find and set from video config ---
    FIND_ELEMENT(self->checkFullscreen,"checkFullscreen");
    FIND_ELEMENT(self->checkVSync,     "checkVSync");
    if (vidCfg) {
        SetCheckState(self->checkFullscreen, *(uint8_t*)((uintptr_t)vidCfg + 0x40));
        SetCheckState(self->checkVSync,      *(uint8_t*)((uintptr_t)vidCfg + 0x41));
    }

    // --- Software mouse checkbox: find and set from Input::Configuration +0x0A ---
    FIND_ELEMENT(self->checkSoftwareMouse,"checkSoftwareMouse");
    if (inputCfg) SetCheckState(self->checkSoftwareMouse,
                                *(uint8_t*)((uintptr_t)inputCfg + 0x0A));

    // --- Input binding list ---
    FIND_ELEMENT(self->inputList,"inputList");
    fn_OptionsDialog_PopulateInputBindList(self);

    // --- Autosave combo: load StringTable, add 9 items, set from settings ---
    FIND_ELEMENT(self->comboAutosave,"comboAutosave");
    {
        uint8_t filenameBuf[8];
        fn_String_Ctor_CStr(filenameBuf, "Dialog/StringTable.rsc:autoSave");
        fn_Filename_AdjustDelimiters(filenameBuf);
        void* ext = fn_ExternalFactory_FindOrLoad(*g_pExternalFactorySingleton, filenameBuf);
        fn_String_Dtor(filenameBuf);

        void* stringTable = nullptr;
        if (ext && g_TypeId_StringTable) {
            if (fn_IsDerivedFrom(ext, *g_TypeId_StringTable)) stringTable = ext;
        }

        if (stringTable && self->comboAutosave) {
            for (int i = 0; i < 9; i++) {
                uint8_t textBuf[8];
                fn_String_Ctor_Default(textBuf);
                fn_StringTable_GetText(stringTable, textBuf, i);
                uint8_t spriteBuf[16] = {};
                fn_Sprite_DefaultCtor(spriteBuf);
                fn_ComboElement_AddItem(self->comboAutosave, spriteBuf, textBuf,
                                        (void*)(intptr_t)i);
                fn_String_Dtor(textBuf);
            }
        }

        int autoSaveIdx = g_pGameSettings ? g_pGameSettings[1] : 0;
        if (autoSaveIdx < 0) autoSaveIdx = 0;
        if (autoSaveIdx > 8) autoSaveIdx = 8;
        if (self->comboAutosave) fn_ComboElement_SetCurrentItem(self->comboAutosave, autoSaveIdx);
    }

    // --- UI scale slider ---
    FIND_ELEMENT(self->sliderUIScale,"sliderUIScale");
    SetSliderRange(self->sliderUIScale, 75.0f, 150.0f);
    if (g_pGameSettings) SetSliderValue(self->sliderUIScale, (float)g_pGameSettings[0]);

    // --- Gameplay settings checkboxes and sliders ---
    FIND_ELEMENT(self->checkStatusIcon,      "checkStatusIcon");
    FIND_ELEMENT(self->sliderStatusIcon,     "sliderStatusIcon");
    FIND_ELEMENT(self->sliderStatusIconScale,"sliderStatusIconScale");
    FIND_ELEMENT(self->checkShowCelsius,     "checkShowCelsius");
    FIND_ELEMENT(self->checkEdgeScroll,      "checkEdgeScroll");
    FIND_ELEMENT(self->checkShowWeather,     "checkShowWeather");
    FIND_ELEMENT(self->checkAutoslow,        "checkAutoslow");
    FIND_ELEMENT(self->checkMouseClip,       "checkMouseClip");
    FIND_ELEMENT(self->checkAutopause,       "checkAutopause");

    if (g_pGameSettings) {
        int32_t* gs = g_pGameSettings;
        SetCheckState(self->checkAutoslow,   *(uint8_t*)((uintptr_t)gs + 0x17));
        SetCheckState(self->checkAutopause,  (uint8_t)gs[6]);
        SetCheckState(self->checkStatusIcon, (uint8_t)gs[3]);
        SetCheckState(self->checkEdgeScroll, (uint8_t)gs[5]);
        SetCheckState(self->checkShowCelsius,*(uint8_t*)((uintptr_t)gs + 0x15));
        SetCheckState(self->checkShowWeather,*(uint8_t*)((uintptr_t)gs + 0x16));

        SetSliderRange(self->sliderStatusIcon, 0.0f, 100.0f);
        SetSliderValue(self->sliderStatusIcon, (float)gs[2]);

        // sliderStatusIconScale: range [25,200], value = gs[4] * c1 * c2
        // c1 and c2 are float constants in exe .rdata (conversion factors for display scale)
        SetSliderRange(self->sliderStatusIconScale, 25.0f, 200.0f);
        {
            float* c1 = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x1401720c4));
            float* c2 = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172208));
            if (c1 && c2) SetSliderValue(self->sliderStatusIconScale,
                                          (float)gs[4] * (*c1) * (*c2));
        }
    }

    // checkMouseClip: from Input::Configuration +0x08
    if (inputCfg) SetCheckState(self->checkMouseClip,
                                *(uint8_t*)((uintptr_t)inputCfg + 0x08));

    // --- Tab options: find and restore last active tab ---
    FIND_ELEMENT(self->tabOptions,"tabOptions");
    if (self->tabOptions) {
        int lastTab = g_pGameSettings ? g_pGameSettings[7] : 0;
        fn_TabElement_SetActiveTab(self->tabOptions, lastTab);
    }
}

#undef FIND_ELEMENT

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
