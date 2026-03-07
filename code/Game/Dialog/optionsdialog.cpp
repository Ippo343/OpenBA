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
// Video device data structure layout (from Ghidra analysis)
// ============================================================
// GraphicsInterface (from Video::Device::GetInterface):
//   +0x00: name (System::String, 8 bytes — passed as String* for AddItem)
//   +0x08: adapter array pointer
//   +0x10: adapter count (int)
//
// Adapter (stride 0x38):
//   +0x00: name (System::String, 8 bytes)
//   +0x08: display mode array pointer
//   +0x10: display mode count (int)
//
// DisplayMode (stride 0x18):
//   +0x00: width (uint32_t)
//   +0x04: height (uint32_t)
//   +0x08: refresh rate array pointer
//   +0x10: refresh rate count (int)
//
// RefreshRate (stride 0x08):
//   +0x00: numerator (uint32_t)
//   +0x04: denominator (uint32_t)

// ============================================================
// Layout comments for singletons
// ============================================================
// Audio config singleton (int[4]):
//   +0x00: UI volume, +0x04: Music, +0x08: Ambient, +0x0C: Effect
//
// Video config singleton:
//   +0x04: renderer, +0x08: adapter, +0x0C: width, +0x10: height
//   +0x24: antialias, +0x28: filter
//   +0x2C: shadow res, +0x30: shadow quality, +0x34: reflection quality
//   +0x38: refresh num, +0x3C: refresh den, +0x40: fullscreen, +0x41: vsync
//
// g_pGameSettings (int32_t[8]):
//   [0]=uiScale, [1]=autosave, [2]=statusIconDist, [3]=statusIconEnabled
//   [4]=statusIconScale, [5]=edgeScroll, [6]=autopause, [7]=lastTab
//   Byte: +0x15=showCelsius, +0x16=showWeather, +0x17=autoslow
//
// Video::Device:
//   +0x98: anisotropic filter level count (int)
//   +0xF8: current renderer index (int)

// Helper macro: find a named element in the dialog and store in outField.
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

// Helper: load a StringTable resource by path
static void* LoadStringTable(const char* path) {
    uint8_t filenameBuf[8];
    fn_String_Ctor_CStr(filenameBuf, path);
    fn_Filename_AdjustDelimiters(filenameBuf);
    void* ext = fn_ExternalFactory_FindOrLoad(*g_pExternalFactorySingleton, filenameBuf);
    fn_String_Dtor(filenameBuf);
    if (ext && g_TypeId_StringTable && fn_IsDerivedFrom(ext, *g_TypeId_StringTable))
        return ext;
    return nullptr;
}

// ============================================================
// Forward declarations for internal helper functions
// ============================================================
static void OptionsDialog_PopulateResolutionCombo(OptionsDialog* self);
static void OptionsDialog_PopulateRefreshCombo(OptionsDialog* self);
static void OptionsDialog_PopulateAntialiasCombo(OptionsDialog* self);
static void OptionsDialog_PopulateFilterCombo(OptionsDialog* self);

// ============================================================
// ValidateVideoConfig (replaces FUN_140072eb0)
// ============================================================
// Calls Video::Device::ValidateConfiguration to fix up any invalid
// combinations of renderer/adapter/resolution/antialias settings.
static void OptionsDialog_ValidateVideoConfig(OptionsDialog* self) {
    if (!fn_VideoDevice_ValidateConfiguration || !g_pVideoDeviceSingleton) return;
    fn_VideoDevice_ValidateConfiguration(
        *g_pVideoDeviceSingleton,
        (int*)&self->savedRenderer, (int*)&self->savedAdapter,
        (int*)&self->savedWidth,    (int*)&self->savedHeight,
        (int*)&self->savedRefreshNum, (int*)&self->savedRefreshDen,
        (int*)&self->savedAntialias, (int*)&self->savedAdapter);
}

// ============================================================
// PopulateFilterCombo (replaces FUN_140073800)
// ============================================================
// Fills texture filter combo: "None", "Bilinear", then "Anisotropic Nx" for each level.
static void OptionsDialog_PopulateFilterCombo(OptionsDialog* self) {
    void* vidDev = g_pVideoDeviceSingleton ? *g_pVideoDeviceSingleton : nullptr;
    if (!vidDev) return;

    fn_InternalBasePtr_CheckPtr(&self->comboFilter);
    fn_ComboElement_RemoveAllItems(self->comboFilter);

    void* stringTable = LoadStringTable("Dialog/StringTable.rsc:textureFilter");
    if (!stringTable) return;

    // Item 0: "None" (filter -1)
    {
        uint8_t textBuf[8];
        fn_String_Ctor_Default(textBuf);
        fn_StringTable_GetText(stringTable, textBuf, 0);
        uint8_t spriteBuf[16] = {};
        fn_Sprite_DefaultCtor(spriteBuf);
        fn_ComboElement_AddItem(self->comboFilter, spriteBuf, textBuf, (void*)(intptr_t)-1);
        fn_String_Dtor(textBuf);
    }

    // Item 1: "Bilinear" (filter 0)
    {
        uint8_t textBuf[8];
        fn_String_Ctor_Default(textBuf);
        fn_StringTable_GetText(stringTable, textBuf, 1);
        uint8_t spriteBuf[16] = {};
        fn_Sprite_DefaultCtor(spriteBuf);
        fn_ComboElement_AddItem(self->comboFilter, spriteBuf, textBuf, (void*)(intptr_t)0);
        fn_String_Dtor(textBuf);
    }

    // Anisotropic filter levels: "Anisotropic 1x", "Anisotropic 2x", etc.
    int anisoLevels = *(int*)((uintptr_t)vidDev + 0x98);
    if (anisoLevels > 0) {
        // First level: "Anisotropic 1x"
        uint8_t textBuf[8];
        fn_String_Ctor_Default(textBuf);
        fn_StringTable_GetText(stringTable, textBuf, 2);
        uint8_t fmtBuf[8];
        fn_String_Format(fmtBuf, "%s 1x", *(void**)textBuf);
        fn_String_Dtor(textBuf);
        uint8_t spriteBuf[16] = {};
        fn_Sprite_DefaultCtor(spriteBuf);
        fn_ComboElement_AddItem(self->comboFilter, spriteBuf, fmtBuf, (void*)(intptr_t)1);
        fn_String_Dtor(fmtBuf);
    }
    // Higher levels: "Anisotropic 2x", "Anisotropic 4x", etc.
    for (int level = 2; level <= anisoLevels; level += 2) {
        uint8_t textBuf[8];
        fn_String_Ctor_Default(textBuf);
        fn_StringTable_GetText(stringTable, textBuf, 2);
        uint8_t fmtBuf[8];
        fn_String_Format(fmtBuf, "%s %ix", *(void**)textBuf, level);
        fn_String_Dtor(textBuf);
        uint8_t spriteBuf[16] = {};
        fn_Sprite_DefaultCtor(spriteBuf);
        fn_ComboElement_AddItem(self->comboFilter, spriteBuf, fmtBuf, (void*)(intptr_t)level);
        fn_String_Dtor(fmtBuf);
    }

    // Set current selection based on savedFilter
    int filterVal = (int)self->savedFilter;
    int selIdx;
    if (filterVal == -1) selIdx = 0;
    else if (filterVal == 0) selIdx = 1;
    else if (filterVal == 1) selIdx = 2;
    else selIdx = (filterVal >> 1) + 2;

    fn_InternalBasePtr_CheckPtr(&self->comboFilter);
    fn_ComboElement_SetCurrentItem(self->comboFilter, selIdx);
}

// ============================================================
// PopulateAntialiasCombo (replaces FUN_140073b10)
// ============================================================
// Fills antialias combo: "None", then "Nx multisample" for each supported mode.
static void OptionsDialog_PopulateAntialiasCombo(OptionsDialog* self) {
    void* vidDev = g_pVideoDeviceSingleton ? *g_pVideoDeviceSingleton : nullptr;
    if (!vidDev) return;

    fn_InternalBasePtr_CheckPtr(&self->comboAntialias);
    fn_ComboElement_RemoveAllItems(self->comboAntialias);

    void* stringTable = LoadStringTable("Dialog/StringTable.rsc:multiSample");

    int modeCount = fn_VideoDevice_GetMultisampleModeCount
                    ? fn_VideoDevice_GetMultisampleModeCount(vidDev) : 0;

    for (int i = 0; i < modeCount; i++) {
        bool supported = fn_VideoDevice_IsMultisampleModeSupported
                         ? fn_VideoDevice_IsMultisampleModeSupported(vidDev, i) : false;
        if (!supported) continue;

        int addedIdx;
        if (i == 0) {
            // "None"
            uint8_t textBuf[8];
            fn_String_Ctor_Default(textBuf);
            if (stringTable) fn_StringTable_GetText(stringTable, textBuf, 0);
            uint8_t spriteBuf[16] = {};
            fn_Sprite_DefaultCtor(spriteBuf);
            addedIdx = fn_ComboElement_AddItem(self->comboAntialias, spriteBuf, textBuf,
                                               (void*)(intptr_t)0);
            fn_String_Dtor(textBuf);
        } else {
            // "Nx multisample" — e.g. "2x Multisample"
            uint8_t textBuf[8];
            fn_String_Ctor_Default(textBuf);
            if (stringTable) fn_StringTable_GetText(stringTable, textBuf, 1);
            uint8_t fmtBuf[8];
            fn_String_Format(fmtBuf, "%ix %s", i + 1, *(void**)textBuf);
            fn_String_Dtor(textBuf);
            uint8_t spriteBuf[16] = {};
            fn_Sprite_DefaultCtor(spriteBuf);
            addedIdx = fn_ComboElement_AddItem(self->comboAntialias, spriteBuf, fmtBuf,
                                               (void*)(intptr_t)i);
            fn_String_Dtor(fmtBuf);
        }

        if ((int)self->savedAntialias == i) {
            fn_InternalBasePtr_CheckPtr(&self->comboAntialias);
            fn_ComboElement_SetCurrentItem(self->comboAntialias, addedIdx);
        }
    }
}

// ============================================================
// PopulateRefreshCombo (replaces FUN_140073d20)
// ============================================================
// Fills refresh rate combo for the currently selected resolution.
static void OptionsDialog_PopulateRefreshCombo(OptionsDialog* self) {
    void* vidDev = g_pVideoDeviceSingleton ? *g_pVideoDeviceSingleton : nullptr;
    if (!vidDev) return;

    void* gfxIface = fn_VideoDevice_GetInterface(vidDev, (int)self->savedRenderer);
    intptr_t adapterArray = *(intptr_t*)((uintptr_t)gfxIface + 8);
    intptr_t adapter = adapterArray + (intptr_t)(int)self->savedAdapter * 0x38;

    // Find display mode matching saved width/height
    int modeCount = *(int*)(adapter + 0x10);
    intptr_t modes = *(intptr_t*)(adapter + 8);
    int modeIdx = -1;
    for (int i = 0; i < modeCount; i++) {
        uint32_t w = *(uint32_t*)(modes + i * 0x18);
        uint32_t h = *(uint32_t*)(modes + i * 0x18 + 4);
        if (w == self->savedWidth && h == self->savedHeight) {
            modeIdx = i;
            break;
        }
    }
    if (modeIdx < 0) modeIdx = 0;

    intptr_t modeEntry = modes + (intptr_t)modeIdx * 0x18;
    intptr_t refreshArray = *(intptr_t*)(modeEntry + 8);
    int refreshCount = *(int*)(modeEntry + 0x10);

    fn_InternalBasePtr_CheckPtr(&self->comboRefresh);
    fn_ComboElement_RemoveAllItems(self->comboRefresh);

    int currentIdx = -1;
    for (int i = 0; i < refreshCount; i++) {
        int* refreshEntry = (int*)(refreshArray + (intptr_t)i * 8);
        int num = refreshEntry[0];
        int den = refreshEntry[1];
        int rate = (den != 0) ? (num / den) : 0;

        uint8_t fmtBuf[8];
        fn_String_Format(fmtBuf, "%i", rate);
        uint8_t spriteBuf[16] = {};
        fn_Sprite_DefaultCtor(spriteBuf);
        fn_InternalBasePtr_CheckPtr(&self->comboRefresh);
        fn_ComboElement_AddItem(self->comboRefresh, spriteBuf, fmtBuf, (void*)(intptr_t)i);
        fn_String_Dtor(fmtBuf);

        if ((uint32_t)num == self->savedRefreshNum && (uint32_t)den == self->savedRefreshDen)
            currentIdx = i;
    }

    fn_InternalBasePtr_CheckPtr(&self->comboRefresh);
    fn_ComboElement_SetCurrentItem(self->comboRefresh, currentIdx);
}

// ============================================================
// PopulateResolutionCombo (replaces FUN_140073ec0)
// ============================================================
// Fills resolution combo with "WxH" entries for the current adapter.
static void OptionsDialog_PopulateResolutionCombo(OptionsDialog* self) {
    void* vidDev = g_pVideoDeviceSingleton ? *g_pVideoDeviceSingleton : nullptr;
    if (!vidDev) return;

    void* gfxIface = fn_VideoDevice_GetInterface(vidDev, (int)self->savedRenderer);
    intptr_t adapterArray = *(intptr_t*)((uintptr_t)gfxIface + 8);
    intptr_t adapter = adapterArray + (intptr_t)(int)self->savedAdapter * 0x38;
    int modeCount = *(int*)(adapter + 0x10);
    intptr_t modes = *(intptr_t*)(adapter + 8);

    fn_InternalBasePtr_CheckPtr(&self->comboResolution);
    fn_ComboElement_RemoveAllItems(self->comboResolution);

    int currentIdx = -1;
    for (int i = 0; i < modeCount; i++) {
        uint32_t w = *(uint32_t*)(modes + i * 0x18);
        uint32_t h = *(uint32_t*)(modes + i * 0x18 + 4);

        uint8_t fmtBuf[8];
        fn_String_Format(fmtBuf, "%ix%i", w, h);
        uint8_t spriteBuf[16] = {};
        fn_Sprite_DefaultCtor(spriteBuf);
        fn_InternalBasePtr_CheckPtr(&self->comboResolution);
        fn_ComboElement_AddItem(self->comboResolution, spriteBuf, fmtBuf, (void*)(intptr_t)i);
        fn_String_Dtor(fmtBuf);

        if (w == self->savedWidth && h == self->savedHeight)
            currentIdx = i;
    }

    fn_InternalBasePtr_CheckPtr(&self->comboResolution);
    fn_ComboElement_SetCurrentItem(self->comboResolution, currentIdx);

    OptionsDialog_PopulateRefreshCombo(self);
}

// ============================================================
// PopulateAdapterCombo (replaces FUN_140074040)
// ============================================================
// Fills adapter combo for the current renderer, then repopulates dependent combos.
static void OptionsDialog_PopulateAdapterCombo(OptionsDialog* self) {
    void* vidDev = g_pVideoDeviceSingleton ? *g_pVideoDeviceSingleton : nullptr;
    if (!vidDev) return;

    fn_InternalBasePtr_CheckPtr(&self->comboAdapter);
    fn_ComboElement_RemoveAllItems(self->comboAdapter);

    void* gfxIface = fn_VideoDevice_GetInterface(vidDev, (int)self->savedRenderer);
    int adapterCount = *(int*)((uintptr_t)gfxIface + 0x10);
    intptr_t adapterArray = *(intptr_t*)((uintptr_t)gfxIface + 8);

    for (int i = 0; i < adapterCount; i++) {
        // Adapter name is a System::String at the start of each 0x38-byte entry
        void* adapterName = (void*)(adapterArray + (intptr_t)i * 0x38);
        uint8_t spriteBuf[16] = {};
        fn_Sprite_DefaultCtor(spriteBuf);
        fn_InternalBasePtr_CheckPtr(&self->comboAdapter);
        fn_ComboElement_AddItem(self->comboAdapter, spriteBuf, adapterName, (void*)(intptr_t)i);
    }

    fn_InternalBasePtr_CheckPtr(&self->comboAdapter);
    fn_ComboElement_SetCurrentItem(self->comboAdapter, (int)self->savedAdapter);

    OptionsDialog_PopulateResolutionCombo(self);
    OptionsDialog_PopulateAntialiasCombo(self);
    OptionsDialog_PopulateFilterCombo(self);
}

// ============================================================
// PopulateVideoComboBoxes (replaces FUN_140074140)
// ============================================================
// Fills all video combo boxes: renderer, adapter, resolution, refresh, antialias, filter.
static void OptionsDialog_PopulateVideoComboBoxes(OptionsDialog* self) {
    void* vidDev = g_pVideoDeviceSingleton ? *g_pVideoDeviceSingleton : nullptr;
    if (!vidDev) return;

    // --- Renderer combo ---
    fn_InternalBasePtr_CheckPtr(&self->comboRenderer);
    fn_ComboElement_RemoveAllItems(self->comboRenderer);

    int ifaceCount = fn_VideoDevice_GetInterfaceCount(vidDev);
    for (int i = 0; i < ifaceCount; i++) {
        // GraphicsInterface starts with a System::String (the interface name)
        void* gfxIface = fn_VideoDevice_GetInterface(vidDev, i);
        uint8_t spriteBuf[16] = {};
        fn_Sprite_DefaultCtor(spriteBuf);
        fn_InternalBasePtr_CheckPtr(&self->comboRenderer);
        fn_ComboElement_AddItem(self->comboRenderer, spriteBuf, gfxIface, (void*)(intptr_t)i);
    }

    fn_InternalBasePtr_CheckPtr(&self->comboRenderer);
    fn_ComboElement_SetCurrentItem(self->comboRenderer, (int)self->savedRenderer);

    // --- Adapter combo (also populates resolution/antialias/filter) ---
    OptionsDialog_PopulateAdapterCombo(self);
}

// ============================================================
// Startup (0x1400742d0)
// ============================================================
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
    int32_t* audioVol = g_pAudioConfigSingleton
                        ? reinterpret_cast<int32_t*>(*g_pAudioConfigSingleton) : nullptr;
    void* vidCfg = g_pVideoConfigSingleton ? *g_pVideoConfigSingleton : nullptr;

    // Input::Configuration singleton (exref 0x140151738)
    void* inputCfg = nullptr;
    {
        void*** exref = reinterpret_cast<void***>(ResolveAddr(Module::Exe, 0x140151738));
        if (exref && *exref) inputCfg = *exref;
    }

    // --- Audio sliders ---
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

    // --- Save current video config ---
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

    // --- Populate all video combo boxes ---
    OptionsDialog_PopulateVideoComboBoxes(self);

    // --- Shadow/reflection quality combos ---
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

    // --- Fullscreen / VSync checkboxes ---
    FIND_ELEMENT(self->checkFullscreen,"checkFullscreen");
    FIND_ELEMENT(self->checkVSync,     "checkVSync");
    if (vidCfg) {
        SetCheckState(self->checkFullscreen, *(uint8_t*)((uintptr_t)vidCfg + 0x40));
        SetCheckState(self->checkVSync,      *(uint8_t*)((uintptr_t)vidCfg + 0x41));
    }

    // --- Software mouse checkbox ---
    FIND_ELEMENT(self->checkSoftwareMouse,"checkSoftwareMouse");
    if (inputCfg) SetCheckState(self->checkSoftwareMouse,
                                *(uint8_t*)((uintptr_t)inputCfg + 0x0A));

    // --- Input binding list ---
    FIND_ELEMENT(self->inputList,"inputList");
    fn_OptionsDialog_PopulateInputBindList(self);

    // --- Autosave combo ---
    FIND_ELEMENT(self->comboAutosave,"comboAutosave");
    {
        void* stringTable = LoadStringTable("Dialog/StringTable.rsc:autoSave");
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

    // --- Tab options: restore last active tab ---
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
            fn_InternalBasePtr_CheckPtr(&self->comboRenderer);
            int curItem = *(int*)((uintptr_t)self->comboRenderer + 0x118);
            fn_InternalBasePtr_CheckPtr(&self->comboRenderer);
            void* data = fn_ComboElement_GetItemData(self->comboRenderer, curItem);
            self->savedRenderer = (uint32_t)(intptr_t)data;
            OptionsDialog_ValidateVideoConfig(self);
            OptionsDialog_PopulateAdapterCombo(self);
            return;
        }
        if (eventData == kCombo_Adapter) {
            fn_InternalBasePtr_CheckPtr(&self->comboAdapter);
            int curItem = *(int*)((uintptr_t)self->comboAdapter + 0x118);
            fn_InternalBasePtr_CheckPtr(&self->comboAdapter);
            void* data = fn_ComboElement_GetItemData(self->comboAdapter, curItem);
            self->savedAdapter = (uint32_t)(intptr_t)data;
            OptionsDialog_ValidateVideoConfig(self);
            OptionsDialog_PopulateAntialiasCombo(self);
            OptionsDialog_PopulateFilterCombo(self);
            OptionsDialog_PopulateResolutionCombo(self);
            return;
        }
        if (eventData == kCombo_Resolution) {
            fn_InternalBasePtr_CheckPtr(&self->comboResolution);
            int curItem = *(int*)((uintptr_t)self->comboResolution + 0x118);
            fn_InternalBasePtr_CheckPtr(&self->comboResolution);
            void* resData = fn_ComboElement_GetItemData(self->comboResolution, curItem);

            int adapterIdx = self->savedAdapter;
            void* gfxIface = fn_VideoDevice_GetInterface(*g_pVideoDeviceSingleton, self->savedRenderer);
            int adapterIdx2 = self->savedAdapter;

            intptr_t adapterArray = *(intptr_t*)((uintptr_t)gfxIface + 8);
            intptr_t displayModes = *(intptr_t*)(adapterArray + (intptr_t)adapterIdx * 0x38 + 8);
            intptr_t modeEntry = displayModes + (intptr_t)(int)(intptr_t)resData * 0x18;

            self->savedWidth = *(uint32_t*)modeEntry;

            gfxIface = fn_VideoDevice_GetInterface(*g_pVideoDeviceSingleton, self->savedRenderer);
            adapterArray = *(intptr_t*)((uintptr_t)gfxIface + 8);
            displayModes = *(intptr_t*)(adapterArray + (intptr_t)adapterIdx2 * 0x38 + 8);
            self->savedHeight = *(uint32_t*)(displayModes + (intptr_t)(int)(intptr_t)resData * 0x18 + 4);

            OptionsDialog_ValidateVideoConfig(self);
            OptionsDialog_PopulateRefreshCombo(self);
            return;
        }
        if (eventData == kCombo_Refresh) {
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

            OptionsDialog_ValidateVideoConfig(self);
            return;
        }
        if (eventData == kCombo_Antialias) {
            fn_InternalBasePtr_CheckPtr(&self->comboAntialias);
            int curItem = *(int*)((uintptr_t)self->comboAntialias + 0x118);
            fn_InternalBasePtr_CheckPtr(&self->comboAntialias);
            void* data = fn_ComboElement_GetItemData(self->comboAntialias, curItem);
            self->savedAntialias = (uint32_t)(intptr_t)data;
            OptionsDialog_ValidateVideoConfig(self);
            return;
        }
        if (eventData == kCombo_Filter) {
            fn_InternalBasePtr_CheckPtr(&self->comboFilter);
            int curItem = *(int*)((uintptr_t)self->comboFilter + 0x118);
            fn_InternalBasePtr_CheckPtr(&self->comboFilter);
            void* data = fn_ComboElement_GetItemData(self->comboFilter, curItem);
            self->savedFilter = (uint32_t)(intptr_t)data;
            OptionsDialog_ValidateVideoConfig(self);
            return;
        }
        return;
    }

    if (eventType == 0x17) {
        // --- Slider value change ---
        int32_t* audioVol = g_pAudioConfigSingleton
                            ? reinterpret_cast<int32_t*>(*g_pAudioConfigSingleton) : nullptr;
        if (!audioVol) return;

        void** sliderField = nullptr;
        switch (eventData) {
            case kSlider_UI:      sliderField = &self->sliderUI; break;
            case kSlider_Music:   sliderField = &self->sliderMusic; break;
            case kSlider_Ambient: sliderField = &self->sliderAmbient; break;
            case kSlider_Effect:  sliderField = &self->sliderEffect; break;
            default: return;
        }

        fn_InternalBasePtr_CheckPtr(sliderField);
        audioVol[eventData] = (int32_t)*(float*)((uintptr_t)*sliderField + 0x128);
        return;
    }
}
