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
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// In-app RenderDoc capture (standalone only) — ApplyVideoSettings arms a delayed
// capture after a video change so we can inspect the post-rebuild render targets.
#include "renderdoc_integration.h"

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
// g_pGameSettingsPtr -> int32_t* (pointer at 0x1401c3de0 to actual settings array):
//   [0]=uiScale, [1]=autosave, [2]=statusIconDist, [3]=statusIconEnabled
//   [4]=statusIconScale, [5]=edgeScroll, [6]=autopause, [7]=lastTab
//   Byte: +0x14=edgeScroll, +0x15=showCelsius, +0x16=showWeather, +0x17=autoslow, +0x18=autopause
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

// Helper: get KeyMap container via single deref from 0x1401c3de8
// Used for name table (+0xC8), string table (+0xD0), and fn_ApplyKeyMapToUI
static inline void* GetKeyMapContainer() {
    if (!g_pKeyMapSingleton || !*g_pKeyMapSingleton) return nullptr;
    return *g_pKeyMapSingleton;
}

// ============================================================
// ApplyKeyMapToUI (exe 0x140099260)
// ============================================================
// Ghidra calls it OptionsDialog_helper_9260, but it is not options-specific and it
// is not "cursor speed / zoom" as an older comment here guessed. Retail body, in
// order: copy five UI sound-effect pointers out of the KeyMap container into the
// UserInterface's sound table, build a 16-byte sprite from km+0xA8/+0xB0 and hand
// it to UI::UserInterface::SetFullscreenFadeSprite, then write the UI scale ratio
// (settings[0] / 100.0f) to ui+0xDC — which edittool.cpp:674 reads back.
//
// Four retail callers; three matter to us:
//   0x140098fa0  OptionsDialog OK/Apply       -> calls this directly (below)
//   0x1400727d0  OptionsDialog Cancel/Restore -> via fn_ApplyKeyMapToUI
//   0x140097c00  GameUpdateLoop::Startup      -> via fn_SetupUIRefs  [the live one]
//
// Both pointer-driven sites resolve 0x140099260, which was never registered, so
// they silently no-opped. GameUpdateLoop::Startup runs on EVERY world boot, so
// in-game UI click/hover sounds, the fullscreen fade sprite, and the UI scale
// factor were never applied during actual gameplay — the menu only had sound
// because menudialogloopt_stubs.cpp binds those five pointers by a separate path.
//
// Retail null-checks neither param; both call sites already guard, and the
// guards below only cover our own optional resolves.
extern "C" void ApplyKeyMapToUI_Impl(void* keyMap, void* userInterface) {
    if (!keyMap || !userInterface) return;
    uintptr_t ui = (uintptr_t)userInterface;
    uintptr_t km = (uintptr_t)keyMap;
    *(uint64_t*)(ui + 0xA8) = *(uint64_t*)(km + 0x70);   // BeginDrag
    *(uint64_t*)(ui + 0xB0) = *(uint64_t*)(km + 0x78);   // EndDrag
    *(uint64_t*)(ui + 0xB8) = *(uint64_t*)(km + 0x60);   // ButtonDown
    *(uint64_t*)(ui + 0xC0) = *(uint64_t*)(km + 0x68);   // ButtonUp
    *(uint64_t*)(ui + 0xC8) = *(uint64_t*)(km + 0x58);   // Alert
    if (fn_UI_SetFullscreenFadeSprite) {
        uint8_t sprite[16];
        *(uint64_t*)&sprite[0] = *(uint64_t*)(km + 0xA8);
        *(uint64_t*)&sprite[8] = *(uint64_t*)(km + 0xB0);
        fn_UI_SetFullscreenFadeSprite(userInterface, sprite);
    }
    if (g_pGameSettingsPtr && *g_pGameSettingsPtr)
        *(float*)(ui + 0xDC) = (float)(*g_pGameSettingsPtr)[0] / 100.0f;
}

// Helper: get actual KeyMap object via triple deref from exref at 0x140151738
// [exref] -> [runtime global] -> [KeyMap ptr] = actual Input::KeyMap
// Used for method calls (GetDeviceForId, GetInputForId, etc.)
static inline void* GetActualKeyMap() {
    if (!g_pKeyMapObjectExref || !*g_pKeyMapObjectExref) return nullptr;
    void* step2 = *(void**)*g_pKeyMapObjectExref;
    if (!step2) return nullptr;
    return *(void**)step2;
}

// Helper: get input key name as UI::String (replaces FUN_140099c20)
// Converts device type (-1=unbound, 0=gamepad, 1=keyboard, 2=mouse) and key index
// to a UI-display string like "Keyboard5" or "Unbound".
// Returns a UI::String (16 bytes) in retval, using the KeyMap's string table at +0xD0.
static void GetInputKeyName(void* retval, int deviceType, int keyIndex) {
    void* keyMapContainer = GetKeyMapContainer();
    if (!keyMapContainer) return;

    uint8_t tempStr[8];
    if (deviceType < 0) {
        fn_String_Ctor_CStr(tempStr, "Unbound");
    } else {
        const char* deviceNames[] = { "Gamepad", "Keyboard", "Mouse" };
        const char* devName = (deviceType >= 0 && deviceType <= 2) ? deviceNames[deviceType] : "Unknown";
        fn_String_Format(tempStr, "%s%i", devName, keyIndex);
    }

    // Construct UI::String from the KeyMap container's string table at +0xD0
    uint64_t stringTable = *(uint64_t*)((uintptr_t)keyMapContainer + 0xD0);
    fn_UIString_Ctor(retval, stringTable, tempStr);
    fn_String_Dtor(tempStr);
}

// Helper: set the key name text on a button element (replaces FUN_140073490)
// Finds "textInputKey" child in the element, formats device+key into display text.
static void SetInputBindButtonText(void* elementPtr, int deviceType, uint32_t keyIndex) {
    // Find "textInputKey" in the element
    uint8_t nameBuf[8];
    fn_String_Ctor_CStr(nameBuf, "textInputKey");
    uint64_t found = 0;
    fn_FindElement(*(void**)elementPtr, &found, nameBuf);
    fn_String_Dtor(nameBuf);

    if (!found) return;

    // Check it's a TextElement
    void* foundObj = (void*)found;
    if (g_TypeId_TextElement && !fn_IsDerivedFrom(foundObj, *g_TypeId_TextElement))
        return;

    // Get the key name as UI::String, then set as text
    uint8_t uiStr[16];
    GetInputKeyName(uiStr, deviceType, keyIndex);

    fn_TextElement_SetText(foundObj, uiStr);
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

// Helper: lazy-resolve g_TypeId_TextElement in standalone mode.
//
// In standalone the exe exref slot at 0x140151508 isn't populated by Runtime
// reflection, so g_TypeId_TextElement starts NULL and every IsDerivedFrom
// check fails — silently dropping every TextElement::SetText / SetParameter
// call (resulting in placeholder text from the .rsc surviving on screen).
// Same pattern as LoadStringTable's StringTable type-id lazy-resolve.
static void EnsureTextElementTypeId() {
    if (g_TypeId_TextElement) return;
    if (!fn_Factory_GetTypeIdFromName || !g_pFactorySingleton || !*g_pFactorySingleton)
        return;
    static uint32_t s_lazyId = 0;
    if (s_lazyId == 0) {
        void* factoryObj = *(void**)*g_pFactorySingleton;
        if (factoryObj) {
            s_lazyId = fn_Factory_GetTypeIdFromName(factoryObj, "TextElement", true);
            printf("[OptionsDialog] Lazy-resolved TextElement typeId = %u\n", s_lazyId);
        }
    }
    if (s_lazyId != 0) g_TypeId_TextElement = &s_lazyId;
}

// Helper: load a StringTable resource by path.
//
// In standalone mode the .rdata exref slot at 0x140151488 is never written by
// Runtime reflection, so g_TypeId_StringTable starts NULL and the type-check
// gate would silently drop every result. We lazy-resolve the type ID on first
// use via Object::Factory::GetTypeIdFromName ("StringTable", true) — Factory
// has already populated its tree by Engine::Startup, so this hits a real id.
static void* LoadStringTable(const char* path) {
    uint8_t filenameBuf[8];
    fn_String_Ctor_CStr(filenameBuf, path);
    fn_Filename_AdjustDelimiters(filenameBuf);
    void* ext = fn_ExternalFactory_FindOrLoad(*g_pExternalFactorySingleton, filenameBuf);
    fn_String_Dtor(filenameBuf);
    if (!ext) return nullptr;

    if (!g_TypeId_StringTable && fn_Factory_GetTypeIdFromName
            && g_pFactorySingleton && *g_pFactorySingleton) {
        static uint32_t s_lazyStringTableTypeId = 0;
        if (s_lazyStringTableTypeId == 0) {
            void* factoryObj = *(void**)*g_pFactorySingleton;
            if (factoryObj) {
                s_lazyStringTableTypeId =
                    fn_Factory_GetTypeIdFromName(factoryObj, "StringTable", true);
                printf("[OptionsDialog] Lazy-resolved StringTable typeId = %u\n",
                       s_lazyStringTableTypeId);
            }
        }
        if (s_lazyStringTableTypeId != 0) g_TypeId_StringTable = &s_lazyStringTableTypeId;
    }

    if (g_TypeId_StringTable && fn_IsDerivedFrom(ext, *g_TypeId_StringTable))
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
//
// 8th arg note: the original (verified in Ghidra @ 0x140072eb0) passes the
// same `&savedAdapter` field twice — slots [+0x194] for both the 2nd and 8th
// argument. Not a typo in our reimpl; matches retail.
//
// Defensive guard: ValidateConfiguration internally does
// `*p5 / *p6 == *adapterRefresh / *adapterRefreshDen` to find a refresh-rate
// match by ratio (Runtime @ 0x1800f7370). If `*p6` (savedRefreshDen) is 0 we
// hit `idiv 0` -> SIGFPE. That can only happen if the Configuration singleton
// wasn't populated with refresh defaults before the dialog opened — which in
// turn only happens if g_pVideoConfigSingleton was NULL at Startup time (so
// the saved-* fields stayed zero-initialized). In standalone we now fall back
// to the Runtime DLL's _singleton slot in EngineFuncsInit, but keep this
// guard as belt-and-braces so a future regression here can't crash the game.
static void OptionsDialog_ValidateVideoConfig(OptionsDialog* self) {
    if (!fn_VideoDevice_ValidateConfiguration || !g_pVideoDeviceSingleton) return;
    if (self->savedRefreshDen == 0) {
        // Defaults from Video::Configuration ctor (Runtime @ 0x1800f6f10): 60/1.
        // Better to pretend "60Hz" than to crash; the next Apply will write
        // ValidateConfiguration's clamped values back to the singleton.
        self->savedRefreshNum = 60;
        self->savedRefreshDen = 1;
    }
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

    printf("[OptionsDialog::PopulateResolutionCombo] savedW=%u savedH=%u modeCount=%d "
           "renderer=%u adapter=%u\n",
           self->savedWidth, self->savedHeight, modeCount,
           self->savedRenderer, self->savedAdapter);

    fn_InternalBasePtr_CheckPtr(&self->comboResolution);
    fn_ComboElement_RemoveAllItems(self->comboResolution);

    int currentIdx = -1;
    for (int i = 0; i < modeCount; i++) {
        uint32_t w = *(uint32_t*)(modes + i * 0x18);
        uint32_t h = *(uint32_t*)(modes + i * 0x18 + 4);

        if (i < 5 || (w == self->savedWidth && h == self->savedHeight))
            printf("[OptionsDialog::PopRes]   mode[%d] = %ux%u%s\n", i, w, h,
                   (w == self->savedWidth && h == self->savedHeight) ? " <-- MATCH" : "");

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
//
// Bisect harness for the standalone-exe Slot4 crash:
// each major init block is wrapped behind a `static bool s_enable*` flag
// (default ON, trivially flippable).
// To localize the per-frame KeyMap::Pressed crash, flip flags to false one
// at a time. See `notes/NOTES030126.md` and the project memory for context.
void OptionsDialog_Startup(OptionsDialog* self) {
    // Bisect knobs — flip these to narrow down the standalone Slot4 crash.
    // Order mirrors the original Startup body.
    static const bool s_enableButtons         = true;
    static const bool s_enableAudioSliders    = true;
    static const bool s_enableVideoCombos     = true;
    static const bool s_enableShadowReflCombos= true;
    static const bool s_enableVidCheckboxes   = true;
    static const bool s_enableInputList       = true;  // re-enabled after KeyMap default bindings populated at boot
    static const bool s_enableAutosaveCombo   = true;
    static const bool s_enableUIScaleSlider   = true;
    static const bool s_enableGameplayChecks  = true;
    static const bool s_enableMouseClipCheck  = true;
    static const bool s_enableTabRestore      = true;

    printf("[OptionsDialog::Startup] self=%p  flags: btn=%d aud=%d vid=%d "
           "sh=%d vchk=%d ilist=%d auto=%d uisc=%d game=%d mclip=%d tab=%d\n",
           self,
           (int)s_enableButtons, (int)s_enableAudioSliders, (int)s_enableVideoCombos,
           (int)s_enableShadowReflCombos, (int)s_enableVidCheckboxes,
           (int)s_enableInputList, (int)s_enableAutosaveCombo,
           (int)s_enableUIScaleSlider, (int)s_enableGameplayChecks,
           (int)s_enableMouseClipCheck, (int)s_enableTabRestore);

    int32_t* audioVol = g_pAudioConfigSingleton
                        ? reinterpret_cast<int32_t*>(*g_pAudioConfigSingleton) : nullptr;
    void* vidCfg = g_pVideoConfigSingleton ? *g_pVideoConfigSingleton : nullptr;
    void* inputCfg = g_pInputConfigSingleton ? *g_pInputConfigSingleton : nullptr;

    // --- Buttons: find, assign IDs, set buttonCancel's hidden flag ---
    if (s_enableButtons) {
        void *buttonOk = nullptr, *buttonCancel = nullptr, *buttonApply = nullptr;
        FIND_ELEMENT(buttonOk,     "buttonOk");
        FIND_ELEMENT(buttonCancel, "buttonCancel");
        FIND_ELEMENT(buttonApply,  "buttonApply");
        fn_Element_SetElementId(buttonOk,     kButton_Ok);
        fn_Element_SetElementId(buttonCancel, kButton_Cancel);
        fn_Element_SetElementId(buttonApply,  kButton_Apply);
        if (buttonCancel) *(uint32_t*)((uintptr_t)buttonCancel + 0xC0) = 8;
    }

    // --- Audio sliders ---
    if (s_enableAudioSliders) {
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
    }

    // --- Video combos: find + IDs + save config + populate ---
    if (s_enableVideoCombos) {
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
        OptionsDialog_PopulateVideoComboBoxes(self);
    }

    // --- Shadow/reflection combos ---
    if (s_enableShadowReflCombos) {
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
    }

    // --- Video / input checkboxes (fullscreen, vsync, software mouse) ---
    if (s_enableVidCheckboxes) {
        FIND_ELEMENT(self->checkFullscreen,"checkFullscreen");
        FIND_ELEMENT(self->checkVSync,     "checkVSync");
        if (vidCfg) {
            SetCheckState(self->checkFullscreen, *(uint8_t*)((uintptr_t)vidCfg + 0x40));
            SetCheckState(self->checkVSync,      *(uint8_t*)((uintptr_t)vidCfg + 0x41));
        }
        FIND_ELEMENT(self->checkSoftwareMouse,"checkSoftwareMouse");
        if (inputCfg) SetCheckState(self->checkSoftwareMouse,
                                    *(uint8_t*)((uintptr_t)inputCfg + 0x0A));
    }

    // --- Input bind list (prime suspect for the per-frame KeyMap::Pressed crash) ---
    if (s_enableInputList) {
        FIND_ELEMENT(self->inputList,"inputList");
        OptionsDialog_PopulateInputBindList(self);
    }

    // --- Autosave combo ---
    if (s_enableAutosaveCombo) {
        FIND_ELEMENT(self->comboAutosave,"comboAutosave");
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
        int autoSaveIdx = (g_pGameSettingsPtr && *g_pGameSettingsPtr) ? (*g_pGameSettingsPtr)[1] : 0;
        if (autoSaveIdx < 0) autoSaveIdx = 0;
        if (autoSaveIdx > 8) autoSaveIdx = 8;
        if (self->comboAutosave) fn_ComboElement_SetCurrentItem(self->comboAutosave, autoSaveIdx);
    }

    // --- UI scale slider ---
    if (s_enableUIScaleSlider) {
        FIND_ELEMENT(self->sliderUIScale,"sliderUIScale");
        SetSliderRange(self->sliderUIScale, 75.0f, 150.0f);
        if (g_pGameSettingsPtr && *g_pGameSettingsPtr)
            SetSliderValue(self->sliderUIScale, (float)(*g_pGameSettingsPtr)[0]);
    }

    // --- Gameplay settings checkboxes and sliders ---
    if (s_enableGameplayChecks) {
        FIND_ELEMENT(self->checkStatusIcon,      "checkStatusIcon");
        FIND_ELEMENT(self->sliderStatusIcon,     "sliderStatusIcon");
        FIND_ELEMENT(self->sliderStatusIconScale,"sliderStatusIconScale");
        FIND_ELEMENT(self->checkShowCelsius,     "checkShowCelsius");
        FIND_ELEMENT(self->checkEdgeScroll,      "checkEdgeScroll");
        FIND_ELEMENT(self->checkShowWeather,     "checkShowWeather");
        FIND_ELEMENT(self->checkAutoslow,        "checkAutoslow");
        FIND_ELEMENT(self->checkAutopause,       "checkAutopause");

        if (g_pGameSettingsPtr && *g_pGameSettingsPtr) {
            int32_t* gs = *g_pGameSettingsPtr;
            SetCheckState(self->checkAutoslow,   *(uint8_t*)((uintptr_t)gs + 0x17));
            SetCheckState(self->checkAutopause,  (uint8_t)gs[6]);
            SetCheckState(self->checkStatusIcon, (uint8_t)gs[3]);
            SetCheckState(self->checkEdgeScroll, (uint8_t)gs[5]);
            SetCheckState(self->checkShowCelsius,*(uint8_t*)((uintptr_t)gs + 0x15));
            SetCheckState(self->checkShowWeather,*(uint8_t*)((uintptr_t)gs + 0x16));

            SetSliderRange(self->sliderStatusIcon, 0.0f, 100.0f);
            SetSliderValue(self->sliderStatusIcon, (float)gs[2]);

            SetSliderRange(self->sliderStatusIconScale, 25.0f, 200.0f);
            SetSliderValue(self->sliderStatusIconScale, (float)gs[4]);
        }
    }

    // --- MouseClip checkbox (depends on inputCfg) ---
    if (s_enableMouseClipCheck) {
        FIND_ELEMENT(self->checkMouseClip,       "checkMouseClip");
        if (inputCfg) SetCheckState(self->checkMouseClip,
                                    *(uint8_t*)((uintptr_t)inputCfg + 0x08));
    }

    // --- Tab options: restore last active tab ---
    if (s_enableTabRestore) {
        FIND_ELEMENT(self->tabOptions,"tabOptions");
        if (self->tabOptions) {
            int lastTab = (g_pGameSettingsPtr && *g_pGameSettingsPtr) ? (*g_pGameSettingsPtr)[7] : 0;
            // Clamp to valid range — options dialog has 4 tabs (0-3)
            if (lastTab < 0 || lastTab > 3) lastTab = 0;
            fn_TabElement_SetActiveTab(self->tabOptions, lastTab);
        }
    }
}

#undef FIND_ELEMENT

// ============================================================
// Shutdown (vtable +0x28, exe 0x140073480): RET 0 (3 bytes)
// ============================================================
// No-op shutdown; nothing to clean up.
int OptionsDialog_Shutdown(OptionsDialog* self) {
    (void)self;
    return 0;
}

// ============================================================
// ApplyInputSettings (0x140072360)
// ============================================================
// Reads checkMouseClip and checkSoftwareMouse from UI, writes to
// Input::Configuration singleton, enables/disables mouse clipping,
// enables cursor, and saves input config.
static void OptionsDialog_ApplyInputSettings(OptionsDialog* self) {
    void* inputCfg = g_pInputConfigSingleton ? *g_pInputConfigSingleton : nullptr;
    if (!inputCfg) return;

    // Read mouse clip checkbox -> Input::Configuration +0x08
    fn_InternalBasePtr_CheckPtr(&self->checkMouseClip);
    *(uint8_t*)((uintptr_t)inputCfg + 0x08) =
        *(uint8_t*)((uintptr_t)*(void**)&self->checkMouseClip + 0xD8);

    // Read software mouse checkbox -> Input::Configuration +0x0A
    fn_InternalBasePtr_CheckPtr(&self->checkSoftwareMouse);
    *(uint8_t*)((uintptr_t)inputCfg + 0x0A) =
        *(uint8_t*)((uintptr_t)*(void**)&self->checkSoftwareMouse + 0xD8);

    // Enable cursor in UI
    void* userInterface = *(void**)((uintptr_t)self + 0x10);
    if (fn_UI_EnableCursor) fn_UI_EnableCursor(userInterface, true);

    // Enable/disable mouse clipping based on checkbox
    fn_InternalBasePtr_CheckPtr(&self->checkMouseClip);
    bool mouseClip = *(uint8_t*)((uintptr_t)*(void**)&self->checkMouseClip + 0xD8);
    void* inputDevice = g_pInputDeviceSingleton ? *g_pInputDeviceSingleton : nullptr;
    if (fn_InputDevice_EnableMouseClipping && inputDevice)
        fn_InputDevice_EnableMouseClipping(inputDevice, mouseClip);

    // Save input configuration
    if (fn_InputConfig_Save && inputCfg)
        fn_InputConfig_Save(inputCfg);
}

// ============================================================
// ApplyGameSettings (0x140072420)
// ============================================================
// Reads all game settings from UI controls, writes to g_pGameSettingsPtr,
// updates autosave timers on all open dialogs, persists settings,
// applies key map to UI, and updates UI scale.
static void OptionsDialog_ApplyGameSettings(OptionsDialog* self) {
    if (!g_pGameSettingsPtr || !*g_pGameSettingsPtr) return;
    int32_t* gs = *g_pGameSettingsPtr;

    // UI scale slider -> gs[0]
    fn_InternalBasePtr_CheckPtr(&self->sliderUIScale);
    gs[0] = (int32_t)*(float*)((uintptr_t)*(void**)&self->sliderUIScale + 0x128);

    // Autosave combo -> gs[1]
    fn_InternalBasePtr_CheckPtr(&self->comboAutosave);
    gs[1] = *(int32_t*)((uintptr_t)*(void**)&self->comboAutosave + 0x118);

    // Status icon checkbox -> gs[3] (byte at offset +0x0C)
    fn_InternalBasePtr_CheckPtr(&self->checkStatusIcon);
    *(uint8_t*)(&gs[3]) = *(uint8_t*)((uintptr_t)*(void**)&self->checkStatusIcon + 0xD8);

    // Status icon distance slider -> gs[2]
    fn_InternalBasePtr_CheckPtr(&self->sliderStatusIcon);
    gs[2] = (int32_t)*(float*)((uintptr_t)*(void**)&self->sliderStatusIcon + 0x128);

    // Status icon scale slider -> gs[4]
    // Original divides by 100.0f then multiplies by 100.0f (cancels out to identity)
    fn_InternalBasePtr_CheckPtr(&self->sliderStatusIconScale);
    {
        float sliderVal = *(float*)((uintptr_t)*(void**)&self->sliderStatusIconScale + 0x128);
        gs[4] = (int32_t)sliderVal;
    }

    // Edge scroll checkbox -> gs[5] (byte)
    fn_InternalBasePtr_CheckPtr(&self->checkEdgeScroll);
    *(uint8_t*)(&gs[5]) = *(uint8_t*)((uintptr_t)*(void**)&self->checkEdgeScroll + 0xD8);

    // Show celsius -> byte at gs+0x15
    fn_InternalBasePtr_CheckPtr(&self->checkShowCelsius);
    *(uint8_t*)((uintptr_t)gs + 0x15) =
        *(uint8_t*)((uintptr_t)*(void**)&self->checkShowCelsius + 0xD8);

    // Show weather -> byte at gs+0x16
    fn_InternalBasePtr_CheckPtr(&self->checkShowWeather);
    *(uint8_t*)((uintptr_t)gs + 0x16) =
        *(uint8_t*)((uintptr_t)*(void**)&self->checkShowWeather + 0xD8);

    // Auto slow -> byte at gs+0x17
    fn_InternalBasePtr_CheckPtr(&self->checkAutoslow);
    *(uint8_t*)((uintptr_t)gs + 0x17) =
        *(uint8_t*)((uintptr_t)*(void**)&self->checkAutoslow + 0xD8);

    // Auto pause -> gs[6] (byte)
    fn_InternalBasePtr_CheckPtr(&self->checkAutopause);
    *(uint8_t*)(&gs[6]) = *(uint8_t*)((uintptr_t)*(void**)&self->checkAutopause + 0xD8);

    // Last active tab -> gs[7]
    fn_InternalBasePtr_CheckPtr(&self->tabOptions);
    gs[7] = *(int32_t*)((uintptr_t)*(void**)&self->tabOptions + 0xF8);

    // Iterate all open dialogs and update autosave timer on each
    void* userInterface = *(void**)((uintptr_t)self + 0x10);
    if (fn_UI_GetFirstDialog && fn_IsA && fn_IsDerivedFrom && fn_UpdateAutoSaveTimer &&
        g_TypeId_AutoSaveDialogType) {
        uint32_t typeId = *g_TypeId_AutoSaveDialogType;
        void* iter = nullptr;
        fn_UI_GetFirstDialog(userInterface, &iter);
        while (iter != nullptr) {
            // iter is an InternalBasePtr node: deref +0x0 for object, +0x10 for next
            fn_InternalBasePtr_CheckPtr(iter);
            void* dialog = *(void**)iter;
            if (fn_IsA(dialog, typeId)) {
                fn_InternalBasePtr_CheckPtr(iter);
                void* obj = *(void**)iter;
                bool isDerived = fn_IsDerivedFrom(obj, typeId);
                void* target = isDerived ? obj : nullptr;
                if (target) fn_UpdateAutoSaveTimer(target);
            }
            iter = *(void**)((uintptr_t)iter + 0x10);
        }
    }

    // Persist game settings
    if (fn_SaveGameSettings) fn_SaveGameSettings(gs);

    // Apply key map config to UI. Was an inline copy of ApplyKeyMapToUI_Impl's body
    // as a fallback; now just calls it, so the OK path and the two pointer-driven
    // call sites (OnCancel below, GameUpdateLoop::Startup) can never diverge.
    void* keyMap = GetKeyMapContainer();
    if (keyMap) ApplyKeyMapToUI_Impl(keyMap, userInterface);

    // Set UI scale: gs[0] is percentage (0-200), divide by 100.0f to get ratio
    *(float*)((uintptr_t)userInterface + 0xDC) = (float)gs[0] / 100.0f;

    // Notify game world of settings change (virtual call at vtable+0x18)
    void* gameWorld = g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr;
    if (gameWorld) {
        // System::Time::Time(0.0f)
        uint8_t timeVal[8] = {};
        if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(timeVal, 0.0f);
        // Virtual call: gameWorld->vtable[3](gameWorld, &time)
        void** vtable = *(void***)gameWorld;
        typedef void (*VCallUpdate_t)(void* self, void* time);
        if (vtable && vtable[3])
            ((VCallUpdate_t)vtable[3])(gameWorld, timeVal);
    }
}

// ============================================================
// ApplyVideoSettings (0x140071e10)
// ============================================================
// Reads all video combos/checkboxes, compares with current video config,
// applies changes via Video::Device (shutdown/startup or apply changed),
// and saves video config on success. Restores on failure.
static void OptionsDialog_ApplyVideoSettings(OptionsDialog* self) {
    // Diagnostic trace for the video-apply path (default quiet). The dialog
    // logic itself is verified correct — see notes: ApplyChangedSettings runs
    // and returns success; the standalone gap is the render backend not
    // recreating the M5 scene targets with the new sample count.
    static const bool kVidTrace = getenv("BANISHED_OPTIONS_TRACE") != nullptr;
    void* vidDev = g_pVideoDeviceSingleton ? *g_pVideoDeviceSingleton : nullptr;
    void* vidCfg = g_pVideoConfigSingleton ? *g_pVideoConfigSingleton : nullptr;
    if (!vidDev || !vidCfg) return;

    // Save current config for potential rollback
    int oldRenderer  = *(int*)((uintptr_t)vidDev + 0xF8);  // current renderer
    int oldAdapter   = *(int*)((uintptr_t)vidCfg + 0x08);
    int oldRefreshN  = *(int*)((uintptr_t)vidCfg + 0x38);
    int oldRefreshD  = *(int*)((uintptr_t)vidCfg + 0x3C);
    int oldWidth     = *(int*)((uintptr_t)vidCfg + 0x0C);
    int oldHeight    = *(int*)((uintptr_t)vidCfg + 0x10);
    int oldAntialias = *(int*)((uintptr_t)vidCfg + 0x24);
    int oldFilter    = *(int*)((uintptr_t)vidCfg + 0x28);
    int oldShadowRes = *(int*)((uintptr_t)vidCfg + 0x2C);
    int oldShadowQ   = *(int*)((uintptr_t)vidCfg + 0x30);
    int oldReflQ     = *(int*)((uintptr_t)vidCfg + 0x34);
    uint8_t oldFullscreen = *(uint8_t*)((uintptr_t)vidCfg + 0x40);
    uint8_t oldVsync      = *(uint8_t*)((uintptr_t)vidCfg + 0x41);

    // Read new values from UI combos
    // Renderer
    fn_InternalBasePtr_CheckPtr(&self->comboRenderer);
    int rendItem = *(int*)((uintptr_t)*(void**)&self->comboRenderer + 0x118);
    fn_InternalBasePtr_CheckPtr(&self->comboRenderer);
    int newRenderer = (int)(intptr_t)fn_ComboElement_GetItemData(*(void**)&self->comboRenderer, rendItem);

    // Adapter
    fn_InternalBasePtr_CheckPtr(&self->comboAdapter);
    int adpItem = *(int*)((uintptr_t)*(void**)&self->comboAdapter + 0x118);
    fn_InternalBasePtr_CheckPtr(&self->comboAdapter);
    int newAdapter = (int)(intptr_t)fn_ComboElement_GetItemData(*(void**)&self->comboAdapter, adpItem);

    // Resolution
    fn_InternalBasePtr_CheckPtr(&self->comboResolution);
    int resItem = *(int*)((uintptr_t)*(void**)&self->comboResolution + 0x118);
    fn_InternalBasePtr_CheckPtr(&self->comboResolution);
    void* resData = fn_ComboElement_GetItemData(*(void**)&self->comboResolution, resItem);

    // Refresh
    fn_InternalBasePtr_CheckPtr(&self->comboRefresh);
    int refItem = *(int*)((uintptr_t)*(void**)&self->comboRefresh + 0x118);

    // Look up actual width/height/refresh from the video interface data
    void* gfxIface = fn_VideoDevice_GetInterface(vidDev, newRenderer);
    intptr_t adapterArray = *(intptr_t*)((uintptr_t)gfxIface + 8);
    intptr_t adapter = adapterArray + (intptr_t)newAdapter * 0x38;
    intptr_t modes = *(intptr_t*)(adapter + 8);
    intptr_t modeEntry = modes + (intptr_t)(int)(intptr_t)resData * 0x18;
    intptr_t refreshArray = *(intptr_t*)(modeEntry + 8);
    intptr_t refreshEntry = refreshArray + (intptr_t)refItem * 8;

    int newRefreshN = *(int*)refreshEntry;
    int newRefreshD = *(int*)(refreshEntry + 4);
    int newWidth    = *(int*)modeEntry;
    int newHeight   = *(int*)(modeEntry + 4);

    // Antialias
    fn_InternalBasePtr_CheckPtr(&self->comboAntialias);
    int aaItem = *(int*)((uintptr_t)*(void**)&self->comboAntialias + 0x118);
    fn_InternalBasePtr_CheckPtr(&self->comboAntialias);
    int newAntialias = (int)(intptr_t)fn_ComboElement_GetItemData(*(void**)&self->comboAntialias, aaItem);

    // Filter
    fn_InternalBasePtr_CheckPtr(&self->comboFilter);
    int fltItem = *(int*)((uintptr_t)*(void**)&self->comboFilter + 0x118);
    fn_InternalBasePtr_CheckPtr(&self->comboFilter);
    int newFilter = (int)(intptr_t)fn_ComboElement_GetItemData(*(void**)&self->comboFilter, fltItem);

    // Shadow res, shadow quality, reflection quality
    fn_InternalBasePtr_CheckPtr(&self->comboShadowRes);
    int newShadowRes = *(int*)((uintptr_t)*(void**)&self->comboShadowRes + 0x118);
    fn_InternalBasePtr_CheckPtr(&self->comboShadowQuality);
    int newShadowQ = *(int*)((uintptr_t)*(void**)&self->comboShadowQuality + 0x118);
    fn_InternalBasePtr_CheckPtr(&self->comboReflectionQuality);
    int newReflQ = *(int*)((uintptr_t)*(void**)&self->comboReflectionQuality + 0x118);

    // Fullscreen / VSync checkboxes
    fn_InternalBasePtr_CheckPtr(&self->checkFullscreen);
    uint8_t newFullscreen = *(uint8_t*)((uintptr_t)*(void**)&self->checkFullscreen + 0xD8);
    fn_InternalBasePtr_CheckPtr(&self->checkVSync);
    uint8_t newVsync = *(uint8_t*)((uintptr_t)*(void**)&self->checkVSync + 0xD8);

    // Check if anything changed. If nothing changed, return early.
    bool rendererChanged = (oldRenderer != newRenderer);
    bool adapterChanged = (oldAdapter != newAdapter);
    bool needsFullRestart = rendererChanged || adapterChanged;

    if (kVidTrace) {
    printf("[OptVideo] vidDev=%p vidCfg=%p | rend %d->%d adap %d->%d "
           "res %dx%d->%dx%d AA %d->%d filt %d->%d fs %d->%d vsync %d->%d "
           "shRes %d->%d shQ %d->%d reflQ %d->%d\n",
           vidDev, vidCfg, oldRenderer, newRenderer, oldAdapter, newAdapter,
           oldWidth, oldHeight, newWidth, newHeight, oldAntialias, newAntialias,
           oldFilter, newFilter, (int)oldFullscreen, (int)newFullscreen,
           (int)oldVsync, (int)newVsync, oldShadowRes, newShadowRes,
           oldShadowQ, newShadowQ, oldReflQ, newReflQ);
    printf("[OptVideo] fns: ApplyChanged=%p Startup=%p Shutdown=%p ReleaseDyn=%p "
           "RecreateDyn=%p CfgSave=%p\n",
           (void*)fn_VideoDevice_ApplyChangedSettings, (void*)fn_VideoDevice_Startup,
           (void*)fn_VideoDevice_Shutdown, (void*)fn_VideoDevice_ReleaseDynamic,
           (void*)fn_VideoDevice_RecreateDynamic, (void*)fn_VideoConfig_Save);
    }

    if (!rendererChanged) {
        // Same renderer — check if resolution/antialias/fullscreen match
        if (oldWidth == newWidth && oldHeight == newHeight &&
            oldAntialias == newAntialias && oldFullscreen == newFullscreen) {
            if (!adapterChanged) {
                // Same adapter — check all remaining settings
                if (oldRefreshN == newRefreshN && oldRefreshD == newRefreshD &&
                    oldVsync == newVsync && oldFilter == newFilter &&
                    oldShadowRes == newShadowRes && oldShadowQ == newShadowQ &&
                    oldReflQ == newReflQ) {
                    if (kVidTrace) printf("[OptVideo] -> NOTHING CHANGED, early return\n");
                    return;  // Nothing changed
                }
            }
        }
    }
    if (kVidTrace) printf("[OptVideo] -> applying (needsFullRestart=%d)\n", (int)needsFullRestart);

    // A video setting actually changed → arm a RenderDoc capture ~8 frames out
    // so we capture a steady-state frame AFTER the engine's deferred swapchain/
    // target rebuild (BeginFrame consumes Device+0x9c next frame and skips it).
    // No-op unless launched via RenderDoc's UI. Lets us inspect the scene render
    // target's actual SampleDesc.Count to confirm whether MSAA is bound.
    RenderDocIntegration::RequestCaptureInFrames(8);

    // If renderer or adapter changed, need full shutdown/startup
    if (needsFullRestart) {
        void* mainLoop = g_pMainLoopSingleton ? *g_pMainLoopSingleton : nullptr;
        if (fn_MainLoop_OnVideoError && mainLoop)
            fn_MainLoop_OnVideoError(mainLoop);
        if (fn_VideoDevice_ReleaseDynamic) fn_VideoDevice_ReleaseDynamic(vidDev);
        if (fn_VideoDevice_Shutdown) fn_VideoDevice_Shutdown(vidDev);
    }

    // Write new settings to video config
    *(int*)((uintptr_t)vidCfg + 0x04) = newRenderer;
    *(int*)((uintptr_t)vidCfg + 0x08) = newAdapter;
    *(int*)((uintptr_t)vidCfg + 0x38) = newRefreshN;
    *(int*)((uintptr_t)vidCfg + 0x0C) = newWidth;
    *(int*)((uintptr_t)vidCfg + 0x10) = newHeight;
    *(int*)((uintptr_t)vidCfg + 0x3C) = newRefreshD;
    // Update windowed size if resolution changed
    if (newWidth != oldWidth || newHeight != oldHeight) {
        *(int*)((uintptr_t)vidCfg + 0x1C) = newWidth;
        *(int*)((uintptr_t)vidCfg + 0x20) = newHeight;
    }
    *(uint8_t*)((uintptr_t)vidCfg + 0x41) = newVsync;
    *(int*)((uintptr_t)vidCfg + 0x24) = newAntialias;
    *(uint8_t*)((uintptr_t)vidCfg + 0x40) = newFullscreen;
    *(int*)((uintptr_t)vidCfg + 0x28) = newFilter;
    *(int*)((uintptr_t)vidCfg + 0x2C) = newShadowRes;
    *(int*)((uintptr_t)vidCfg + 0x30) = newShadowQ;
    *(int*)((uintptr_t)vidCfg + 0x34) = newReflQ;

    bool success;
    if (!needsFullRestart) {
        // Same renderer+adapter: just apply changed settings
        success = fn_VideoDevice_ApplyChangedSettings
                  ? fn_VideoDevice_ApplyChangedSettings(vidDev) : false;
    } else {
        // Full restart
        success = fn_VideoDevice_Startup ? fn_VideoDevice_Startup(vidDev) : false;
        if (fn_VideoDevice_RecreateDynamic) fn_VideoDevice_RecreateDynamic(vidDev);
        void* mainLoop = g_pMainLoopSingleton ? *g_pMainLoopSingleton : nullptr;
        if (fn_MainLoop_OnVideoRestart && mainLoop)
            fn_MainLoop_OnVideoRestart(mainLoop);
    }

    if (kVidTrace) printf("[OptVideo] apply result success=%d (path=%s)\n", (int)success,
           needsFullRestart ? "Startup" : "ApplyChangedSettings");

    if (success) {
        // Save the new config
        if (fn_VideoConfig_Save) fn_VideoConfig_Save(vidCfg);
    } else {
        if (kVidTrace) printf("[OptVideo] -> ROLLBACK (apply returned false)\n");
        // Rollback to old config
        *(int*)((uintptr_t)vidCfg + 0x08) = oldAdapter;
        *(int*)((uintptr_t)vidCfg + 0x0C) = oldWidth;
        *(int*)((uintptr_t)vidCfg + 0x04) = oldRenderer;
        *(int*)((uintptr_t)vidCfg + 0x10) = oldHeight;
        *(int*)((uintptr_t)vidCfg + 0x38) = oldRefreshN;
        *(int*)((uintptr_t)vidCfg + 0x3C) = oldRefreshD;
        *(int*)((uintptr_t)vidCfg + 0x24) = oldAntialias;
        *(uint8_t*)((uintptr_t)vidCfg + 0x40) = oldFullscreen;
        *(uint8_t*)((uintptr_t)vidCfg + 0x41) = oldVsync;
        *(int*)((uintptr_t)vidCfg + 0x28) = oldFilter;
        *(int*)((uintptr_t)vidCfg + 0x2C) = oldShadowRes;
        *(int*)((uintptr_t)vidCfg + 0x30) = oldShadowQ;
        *(int*)((uintptr_t)vidCfg + 0x34) = oldReflQ;
    }
}

// ============================================================
// RestoreVideoSettings (0x1400727d0)
// ============================================================
// Restores video/game settings on cancel: re-reads from persistence,
// restores last tab, and re-applies keymap config to UI.
static void OptionsDialog_RestoreVideoSettings(OptionsDialog* self) {
    // Re-read all settings from persistence
    void* persistence = g_pPersistenceSingleton ? *g_pPersistenceSingleton : nullptr;
    if (fn_Persistence_ReadSettings && persistence)
        fn_Persistence_ReadSettings(persistence);

    // Re-read game settings from persistence
    if (fn_ReadGameSettings && g_pGameSettingsPtr && *g_pGameSettingsPtr)
        fn_ReadGameSettings(*g_pGameSettingsPtr);

    // Restore last tab
    fn_InternalBasePtr_CheckPtr(&self->tabOptions);
    if (g_pGameSettingsPtr && *g_pGameSettingsPtr)
        (*g_pGameSettingsPtr)[7] = *(int32_t*)((uintptr_t)*(void**)&self->tabOptions + 0xF8);

    // Re-apply keymap config to UI
    void* keyMap = GetKeyMapContainer();
    void* userInterface = *(void**)((uintptr_t)self + 0x10);
    if (keyMap && fn_ApplyKeyMapToUI)
        fn_ApplyKeyMapToUI(keyMap, userInterface);
}

// ============================================================
// OnApply (0x1400728e0): Apply all settings and save (keep dialog open)
// ============================================================
// Calls ApplyVideoSettings, ApplyInputSettings, saves audio config,
// applies game settings, and writes persistence settings.

void OptionsDialog_OnApply(OptionsDialog* self) {
    OptionsDialog_ApplyVideoSettings(self);
    OptionsDialog_ApplyInputSettings(self);

    // Save audio configuration
    if (g_pAudioConfigSingleton && *g_pAudioConfigSingleton && fn_AudioConfig_Save)
        fn_AudioConfig_Save(*g_pAudioConfigSingleton);

    OptionsDialog_ApplyGameSettings(self);

    // Write all settings to disk
    if (g_pPersistenceSingleton && *g_pPersistenceSingleton && fn_Persistence_WriteSettings)
        fn_Persistence_WriteSettings(*g_pPersistenceSingleton);
}

// ============================================================
// OnOk (0x140072930): Apply all settings, save, and close dialog
// ============================================================
// Same as OnApply plus closes the dialog window.

void OptionsDialog_OnOk(OptionsDialog* self) {
    OptionsDialog_OnApply(self);

    // Close dialog
    uint8_t ibpBuf[8];
    fn_InternalBasePtr_Ctor(ibpBuf, self);
    void* userInterface = *(void**)((uintptr_t)self + 0x10);
    fn_UI_CloseDialog(userInterface, ibpBuf);
}

// ============================================================
// OnCancel (0x140072840): Revert settings and close dialog
// ============================================================
// Reloads settings from disk, re-registers default input bindings,
// restores video settings, and closes the dialog.

void OptionsDialog_OnCancel(OptionsDialog* self) {
    // Reload persistence settings from disk
    if (g_pPersistenceSingleton && *g_pPersistenceSingleton && fn_Persistence_ReadSettings)
        fn_Persistence_ReadSettings(*g_pPersistenceSingleton);

    // Re-register default input bindings and load saved config
    if (fn_RegisterDefaultInputBindings)
        fn_RegisterDefaultInputBindings();

    // Reload input configuration
    if (g_pInputConfigSingleton && *g_pInputConfigSingleton && fn_InputConfig_Load)
        fn_InputConfig_Load(*g_pInputConfigSingleton);

    // Restore mouse clipping from input config byte at +0x08
    if (g_pInputDeviceSingleton && *g_pInputDeviceSingleton &&
        g_pInputConfigSingleton && *g_pInputConfigSingleton &&
        fn_InputDevice_EnableMouseClipping) {
        bool clipEnabled = *(bool*)((uintptr_t)*g_pInputConfigSingleton + 0x08);
        fn_InputDevice_EnableMouseClipping(*g_pInputDeviceSingleton, clipEnabled);
    }

    // Reload persistence settings again (for audio config)
    if (g_pPersistenceSingleton && *g_pPersistenceSingleton && fn_Persistence_ReadSettings)
        fn_Persistence_ReadSettings(*g_pPersistenceSingleton);

    // Reinitialize audio from saved config
    if (g_pAudioConfigSingleton && *g_pAudioConfigSingleton && fn_AudioConfig_Startup)
        fn_AudioConfig_Startup(*g_pAudioConfigSingleton);

    // Restore video settings to pre-dialog state
    OptionsDialog_RestoreVideoSettings(self);

    // Close dialog
    uint8_t ibpBuf[8];
    fn_InternalBasePtr_Ctor(ibpBuf, self);
    void* userInterface = *(void**)((uintptr_t)self + 0x10);
    fn_UI_CloseDialog(userInterface, ibpBuf);
}

// ============================================================
// OnEvent (0x140072f20): Handle options changes
// ============================================================

void OptionsDialog_OnEvent(OptionsDialog* self, void* element, int eventType, int eventData) {

    if (eventType == 0x12) {
        // --- Button click ---
        if (eventData == kButton_Ok) {
            OptionsDialog_OnOk(self);
            return;
        }
        if (eventData == kButton_Cancel) {
            OptionsDialog_OnCancel(self);
            return;
        }
        if (eventData == kButton_Apply) {
            OptionsDialog_OnApply(self);
            return;
        }
        // Input bind button clicks: IDs >= 13
        if (eventData >= kInputBind_Base) {
            fn_InternalBasePtr_CheckPtr(&element);
            // FUN_140010460: IsDerivedFrom(elem, UIElementBase type) — returns elem if valid, else null
            void* clickedElem = *(void**)element;
            bool isUIElement = g_TypeId_UIElementBase && fn_IsDerivedFrom(clickedElem, *g_TypeId_UIElementBase);
            void* parent = isUIElement ? clickedElem : nullptr;
            uint8_t parentPtr[16];
            fn_InternalBasePtr_Ctor(parentPtr, parent);

            unsigned int offset = (unsigned int)(eventData - kInputBind_Base);
            unsigned int slot = offset & 0x80000001;
            if ((int)slot < 0) {
                slot = ((slot - 1) | 0xFFFFFFFE) + 1;
            }
            int bindIndex = (int)offset / 2;

            OptionsDialog_OnInputBindClick(self, parentPtr, bindIndex, slot);
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

// ============================================================
// PopulateInputBindList (0x140073580): Fills input binding list
// ============================================================
// Iterates children of inputList in groups of 3:
//   child[i*3+0] = row container (contains "textInputName" label)
//   child[i*3+1] = bind button for slot 0
//   child[i*3+2] = bind button for slot 1
// Sets label text from KeyMap string table, sets button key name text,
// assigns element IDs starting at kInputBind_Base (13).
void OptionsDialog_PopulateInputBindList(OptionsDialog* self) {
    EnsureTextElementTypeId();
    void* keyMapContainer = GetKeyMapContainer();
    if (!keyMapContainer) return;
    void* keyMap = GetActualKeyMap();  // for method calls (GetDeviceForId, etc.)
    if (!keyMap) return;

    fn_InternalBasePtr_CheckPtr(&self->inputList);
    void* inputList = self->inputList;
    int childCount = fn_Element_ChildCount(inputList);

    int numRows = childCount / 3;
    if (numRows <= 0) return;

    int elemId = kInputBind_Base;  // 13
    int inputId = 5;  // KeyMap input IDs start at 5

    for (int row = 0; row < numRows; row++, inputId++) {
        // Get the 3 children for this row
        fn_InternalBasePtr_CheckPtr(&self->inputList);
        uint64_t rowChild = 0;
        fn_Element_Child(self->inputList, &rowChild, row * 3);

        // Find "textInputName" in the row container and set label
        {
            uint8_t nameBuf[8];
            fn_String_Ctor_CStr(nameBuf, "textInputName");
            uint64_t textElem = 0;
            fn_InternalBasePtr_CheckPtr((void*)&rowChild);
            fn_FindElement((void*)rowChild, &textElem, nameBuf);
            fn_String_Dtor(nameBuf);

            if (textElem) {
                void* textObj = (void*)textElem;
                if (g_TypeId_TextElement && fn_IsDerivedFrom(textObj, *g_TypeId_TextElement)) {
                    // Get input name from KeyMap container's name table at +0xC8
                    uint64_t nameTable = *(uint64_t*)((uintptr_t)keyMapContainer + 0xC8);
                    uint8_t uiStr[16];
                    fn_UIString_Ctor_Index(uiStr, nameTable, inputId);
                    fn_TextElement_SetText(textObj, uiStr);
                }
            }
        }

        // Set bind button texts and element IDs for slots 0 and 1
        for (int slot = 0; slot < 2; slot++) {
            fn_InternalBasePtr_CheckPtr(&self->inputList);
            uint64_t btnChild = 0;
            fn_Element_Child(self->inputList, &btnChild, row * 3 + 1 + slot);

            // Get current device and key for this input binding
            int device = fn_KeyMap_GetDeviceForId(keyMap, inputId, slot);
            int key = fn_KeyMap_GetInputForId(keyMap, inputId, slot);

            // Set the button's key name text
            SetInputBindButtonText((void*)&btnChild, device, key);

            // Assign element ID
            fn_InternalBasePtr_CheckPtr((void*)&btnChild);
            fn_Element_SetElementId((void*)btnChild, elemId + slot);
        }

        elemId += 2;
    }
}

// ============================================================
// OnInputBindClick (0x1400729a0): Shows rebind dialog
// ============================================================
// When user clicks a key bind button in the options:
// 1. Creates OptionInputBindDialog from "Dialog/Options.rsc:inputBind"
// 2. Sets the input name text as a parameter
// 3. Shows the dialog modally (blocks until user presses a key)
// 4. Reads the captured key from the dialog result
// 5. If result=2 (clear), clears the binding
// 6. If result=1 (bound), checks for conflicts and handles them
// 7. Refreshes the input bind list
void OptionsDialog_OnInputBindClick(OptionsDialog* self, void* element, int bindIndex, int slot) {
    EnsureTextElementTypeId();
    void* keyMapContainer = GetKeyMapContainer();
    if (!keyMapContainer) return;
    void* keyMap = GetActualKeyMap();  // for method calls
    if (!keyMap) return;

    int inputId = bindIndex + 5;  // Convert bind index to KeyMap input ID

    // Load the input bind dialog resource
    uint8_t filenameBuf[8];
    fn_String_Ctor_CStr(filenameBuf, "Dialog/Options.rsc:inputBind");
    fn_Filename_AdjustDelimiters(filenameBuf);
    void* extResult = fn_ExternalFactory_FindOrLoad(*g_pExternalFactorySingleton, filenameBuf);
    // Type check the external resource
    void* external = nullptr;
    if (extResult && g_pExternalTypeId) {
        bool isDerived = fn_IsDerivedFrom(extResult, *g_pExternalTypeId);
        if (isDerived) external = extResult;
    }
    fn_String_Dtor(filenameBuf);

    // Allocate and construct OptionInputBindDialog
    void* mem = fn_MemAlloc(g_pMemAllocSingleton, 200, 8);  // 0xC8 = 200 bytes
    void* dialog = nullptr;
    if (mem) {
        uint8_t nullPtr[8] = {};
        fn_InternalBasePtr_Ctor(nullPtr, nullptr);
        void* userInterface = *(void**)((uintptr_t)self + 0x10);
        dialog = fn_OptionInputBindDialog_Ctor(mem, userInterface, external, nullPtr);
    }

    // Wrap in InternalPtr
    uint64_t dialogPtr = 0;
    fn_InternalBasePtr_Ctor((void*)&dialogPtr, dialog);

    // Set the input name text on the dialog
    // Get input name from KeyMap container's name table at +0xC8
    uint8_t inputNameUI[16];
    uint64_t nameTable = *(uint64_t*)((uintptr_t)keyMapContainer + 0xC8);
    fn_UIString_Ctor_Index(inputNameUI, nameTable, inputId);

    // Convert to System::String
    uint8_t inputNameStr[8];
    fn_UIString_ToSystemString(inputNameUI, inputNameStr);

    // Find TextElement in dialog and set parameter 0
    fn_InternalBasePtr_CheckPtr((void*)&dialogPtr);
    if (g_TypeId_TextElement) {
        uint64_t textElem = 0;
        fn_DialogInstance_FindElementType((void*)dialogPtr, &textElem, *g_TypeId_TextElement);
        if (textElem) {
            fn_InternalBasePtr_CheckPtr((void*)&textElem);
            fn_TextElement_SetParameter((void*)textElem, 0, inputNameStr);
        }
    }

    // Show dialog modally — get result
    void* userInterface = *(void**)((uintptr_t)&self->base + 0x10);
    uint64_t showPtr = dialogPtr;
    int result = fn_UI_ShowDialog(userInterface, &showPtr);

    // Read captured key data from dialog before deleting
    fn_InternalBasePtr_CheckPtr((void*)&dialogPtr);
    int capturedDevice = *(int*)((uintptr_t)(void*)dialogPtr + 0xC4);
    fn_InternalBasePtr_CheckPtr((void*)&dialogPtr);
    int capturedKey = *(int*)((uintptr_t)(void*)dialogPtr + 0xC0);

    // Check if the new binding conflicts with an existing one
    int conflictId = fn_KeyMap_GetIdForDeviceInput(keyMap, capturedDevice, capturedKey);

    // Delete the input bind dialog
    fn_InternalBasePtr_DeletePtr((void*)&dialogPtr);

    if (result == 2) {
        // User chose "clear" — unbind this slot
        fn_KeyMap_ClearInput2(keyMap, inputId, slot);
        // Update button text to "Unbound"
        uint64_t elemVal = *(uint64_t*)element;
        SetInputBindButtonText((void*)&elemVal, -1, 0);
        goto cleanup;
    }

    if (result != 1) goto cleanup;  // User cancelled

    if (conflictId < 0) {
        // No conflict — just set the new binding
        goto set_binding;
    }

    // There IS a conflict — show the "already bound" confirmation dialog
    {
        // Load the inputBound dialog resource
        uint8_t fnameBuf2[8];
        fn_String_Ctor_CStr(fnameBuf2, "Dialog/Options.rsc:inputBound");
        fn_Filename_AdjustDelimiters(fnameBuf2);
        void* extResult2 = fn_ExternalFactory_FindOrLoad(*g_pExternalFactorySingleton, fnameBuf2);
        void* external2 = nullptr;
        if (extResult2 && g_pExternalTypeId) {
            if (fn_IsDerivedFrom(extResult2, *g_pExternalTypeId)) external2 = extResult2;
        }
        fn_String_Dtor(fnameBuf2);

        // Allocate and construct MessageDialog (0xC0 bytes)
        void* mem2 = fn_MemAlloc(g_pMemAllocSingleton, 0xC0, 8);
        void* dialog2 = nullptr;
        if (mem2) {
            uint8_t nullPtr2[8] = {};
            fn_InternalBasePtr_Ctor(nullPtr2, nullptr);
            void* ui = *(void**)((uintptr_t)self + 0x10);
            dialog2 = fn_MessageDialog_Ctor(mem2, ui, external2, nullPtr2);
        }

        uint64_t dialog2Ptr = 0;
        fn_InternalBasePtr_Ctor((void*)&dialog2Ptr, dialog2);

        // Get the name of the key that was pressed (new binding)
        uint8_t newKeyUI[16];
        GetInputKeyName(newKeyUI, capturedDevice, capturedKey);
        uint8_t newKeyStr[8];
        fn_UIString_ToSystemString(newKeyUI, newKeyStr);

        // Get the name of the existing binding that conflicts
        uint8_t conflictNameUI[16];
        fn_UIString_Ctor_Index(conflictNameUI, nameTable, conflictId);
        uint8_t conflictNameStr[8];
        fn_UIString_ToSystemString(conflictNameUI, conflictNameStr);

        // Check if the conflicting binding is the same as the current one
        // (in which case no confirmation is needed)
        bool sameBinding = false;
        {
            const char* str1 = *(const char**)inputNameStr;
            const char* str2 = *(const char**)conflictNameStr;
            if (str1 && str2) {
                sameBinding = (strcmp(str1, str2) == 0);
            } else {
                sameBinding = (str1 == str2);
            }
        }

        int confirmResult = 1;  // default: proceed
        if (!sameBinding) {
            // Set parameters on the confirmation dialog:
            // param 0 = new key name, param 1 = conflicting binding name, param 2 = current input name
            if (g_TypeId_TextElement) {
                fn_InternalBasePtr_CheckPtr((void*)&dialog2Ptr);
                uint64_t textElem2 = 0;
                fn_DialogInstance_FindElementType((void*)dialog2Ptr, &textElem2, *g_TypeId_TextElement);
                if (textElem2) {
                    fn_InternalBasePtr_CheckPtr((void*)&textElem2);
                    fn_TextElement_SetParameter((void*)textElem2, 0, newKeyStr);
                }

                fn_InternalBasePtr_CheckPtr((void*)&dialog2Ptr);
                uint64_t textElem3 = 0;
                fn_DialogInstance_FindElementType((void*)dialog2Ptr, &textElem3, *g_TypeId_TextElement);
                if (textElem3) {
                    fn_InternalBasePtr_CheckPtr((void*)&textElem3);
                    fn_TextElement_SetParameter((void*)textElem3, 1, conflictNameStr);
                }

                fn_InternalBasePtr_CheckPtr((void*)&dialog2Ptr);
                uint64_t textElem4 = 0;
                fn_DialogInstance_FindElementType((void*)dialog2Ptr, &textElem4, *g_TypeId_TextElement);
                if (textElem4) {
                    fn_InternalBasePtr_CheckPtr((void*)&textElem4);
                    fn_TextElement_SetParameter((void*)textElem4, 2, inputNameStr);
                }
            }

            // Show confirmation dialog
            uint64_t show2Ptr = dialog2Ptr;
            confirmResult = fn_UI_ShowDialog(userInterface, &show2Ptr);
            fn_InternalBasePtr_DeletePtr((void*)&dialog2Ptr);
        }

        fn_String_Dtor(conflictNameStr);
        fn_String_Dtor(newKeyStr);

        if (confirmResult >= 1 && confirmResult <= 2) {
            // User confirmed (OK or Yes) — clear the old conflicting binding
            fn_KeyMap_ClearInput3(keyMap, conflictId, capturedDevice, capturedKey);
            goto set_binding;
        }
        // User cancelled — don't change anything
    }

    // Refresh the input bind list display
    OptionsDialog_PopulateInputBindList(self);
    goto cleanup;

set_binding:
    fn_KeyMap_SetInput(keyMap, inputId, slot, capturedDevice, capturedKey);
    fn_InputDevice_ResetState(*g_pInputDeviceSingleton);

    // Refresh the entire input bind list
    OptionsDialog_PopulateInputBindList(self);

cleanup:
    fn_String_Dtor(inputNameStr);
}


// ============================================================
// OptionsDialog standalone scaffolding
// ============================================================
// Provides a 10-slot vtable + ctor + factory + REGISTER_TYPE so that
// MainMenuDialog::OpenOptions can produce a real OptionsDialog instance.
//
// Vtable layout (exe @ 0x14015ceb0, 10 slots):
//   +0x00 Slot0 Dtor                — sets vtable, ~DialogInstance, conditional free
//   +0x08 Slot1 TypeName            — exe const-load
//   +0x10 Slot2 TypeId              — exe const-load
//   +0x18 Slot3 Persist             — runtime thunk
//   +0x20 Slot4 Setup               — REIMPL'd above (OptionsDialog_Startup)
//   +0x28 Slot5 Shutdown            — REIMPL'd above (OptionsDialog_Shutdown)
//   +0x30 Slot6 OnUpdateLayout      — runtime thunk
//   +0x38 Slot7 OnUpdate            — runtime thunk
//   +0x40 Slot8 OnEvent             — REIMPL'd above (OptionsDialog_OnEvent)
//   +0x48 Slot9 OnSerializeSettings — runtime thunk
//
// 10 slots vs MainMenuDialog's 20: OptionsDialog has only the DialogInstance
// interface (no DialogController), so no second-vtable block.

#include "Object/factory_registration.h"
#include "address_registry.h"
#include "resolve.h"

namespace {

void* GetOptionsDialogVtable();  // forward decl

// Slot 0 — Dtor. Mirrors generic ~DialogInstance scalar-deleting dtor.
void* OptionsStub_Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    *(void**)self = GetOptionsDialogVtable();
    if (fn_DialogInstance_Dtor) fn_DialogInstance_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

// Slot 1 — TypeName.
const char* OptionsStub_Slot1_TypeName(void* /*self*/) { return "OptionsDialog"; }

// Slot 2 — TypeId.
uint32_t OptionsStub_Slot2_TypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("OptionsDialog");
    return cached;
}

// Generic logging stub for the runtime-thunk slots (3, 6, 7, 9). Logs once
// per slot so we can see when wiring matters.
template <int SlotIdx>
void OptionsStub_RuntimeThunk(void* self, ...) {
    static bool warned = false;
    if (!warned) {
        warned = true;
        printf("[OptionsDialog::Slot%d_Stub] self=%p (runtime thunk not wired)\n",
               SlotIdx, self);
    }
}

// Stub Setup — prints once then no-ops. Used to localize crashes:
// if commenting out Startup causes the crash to go away, the bug is inside
// our reimpl; if it persists, it's downstream. Kept around for re-bisecting.
[[maybe_unused]] void OptionsStub_Slot4_NoOp(void* self) {
    static bool warned = false;
    if (!warned) {
        warned = true;
        printf("[OptionsDialog::Slot4_NoOp] self=%p (Setup stubbed off for "
               "diagnostics)\n", self);
    }
}

void* g_OptionsDialog_Vtable_Standalone[10] = {
    reinterpret_cast<void*>(&OptionsStub_Dtor),                  // +0x00 Dtor
    reinterpret_cast<void*>(&OptionsStub_Slot1_TypeName),        // +0x08 TypeName
    reinterpret_cast<void*>(&OptionsStub_Slot2_TypeId),          // +0x10 TypeId
    reinterpret_cast<void*>(&OptionsStub_RuntimeThunk<3>),       // +0x18 Persist
    // Slot4 — real Startup, with bisect flags inside it (input list off first).
    reinterpret_cast<void*>(&OptionsDialog_Startup),             // +0x20 Setup
    reinterpret_cast<void*>(&OptionsDialog_Shutdown),            // +0x28 Shutdown
    reinterpret_cast<void*>(&OptionsStub_RuntimeThunk<6>),       // +0x30 OnUpdateLayout
    reinterpret_cast<void*>(&OptionsStub_RuntimeThunk<7>),       // +0x38 OnUpdate
    reinterpret_cast<void*>(&OptionsDialog_OnEvent),             // +0x40 OnEvent
    reinterpret_cast<void*>(&OptionsStub_RuntimeThunk<9>),       // +0x48 OnSerializeSettings
};

void* GetOptionsDialogVtable() {
    return reinterpret_cast<void*>(g_OptionsDialog_Vtable_Standalone);
}

// In-place ctor — clean reimpl of FUN_140075150.
// Original mirrors: copy parentIBP to a local stack slot, call
// DialogInstance::DialogInstance(self, ui, extRes, &localParent, 0, 0, 0),
// patch *self with vtable_OptionsDialog, zero +0xB8..+0x1B8.
void* OptionsDialog_Ctor_Standalone(void* self, void* ui, void* extRes,
                                    void* parentIBP) {
    if (!self) return nullptr;
    printf("[OptionsDialog_Ctor] self=%p ui=%p extRes=%p parentIBP=%p\n",
           self, ui, extRes, parentIBP);

    // Stack copy of the parent IBP qword (matches original SEH frame layout).
    uint64_t parentCopy = parentIBP ? *reinterpret_cast<uint64_t*>(parentIBP) : 0;

    if (fn_DialogInstance_Ctor && extRes) {
        fn_DialogInstance_Ctor(self, ui, extRes, &parentCopy,
                                nullptr, nullptr, 0);
    } else {
        printf("[OptionsDialog_Ctor] skipping DialogInstance::DialogInstance "
               "(fn=%p extRes=%p) — bare-bones init\n",
               (void*)fn_DialogInstance_Ctor, extRes);
    }

    *reinterpret_cast<void**>(self) = GetOptionsDialogVtable();

    // Zero +0xB8..+0x1B8 (covers all OptionsDialog-specific fields).
    OptionsDialog* od = reinterpret_cast<OptionsDialog*>(self);
    char* base = reinterpret_cast<char*>(od);
    for (size_t off = 0xB8; off < sizeof(OptionsDialog); off += 8) {
        *reinterpret_cast<uint64_t*>(base + off) = 0;
    }
    return self;
}

void* OptionsDialog_Factory_Bare(void* /*loop*/) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, sizeof(OptionsDialog), 8);
    if (!p) return nullptr;
    // Bare-bones — no UI/extRes/parent. Caller is the .rsc factory loader,
    // which currently doesn't instantiate OptionsDialog this way (always
    // built via MainMenuDialog::OpenOptions). Just zero + vtable.
    for (size_t off = 0; off < sizeof(OptionsDialog); off += 8) {
        *reinterpret_cast<uint64_t*>(reinterpret_cast<char*>(p) + off) = 0;
    }
    *reinterpret_cast<void**>(p) = GetOptionsDialogVtable();
    return p;
}

void* OptionsDialog_CreateInPlace_Bare(void* mem) {
    if (!mem) return nullptr;
    for (size_t off = 0; off < sizeof(OptionsDialog); off += 8) {
        *reinterpret_cast<uint64_t*>(reinterpret_cast<char*>(mem) + off) = 0;
    }
    *reinterpret_cast<void**>(mem) = GetOptionsDialogVtable();
    return mem;
}

}  // namespace

extern "C" void RegisterOptionsDialogVtable() {
    RegisterExeAddr(0x14015ceb0, reinterpret_cast<uintptr_t>(g_OptionsDialog_Vtable_Standalone));
    RegisterExeAddr(0x140075150, reinterpret_cast<uintptr_t>(&OptionsDialog_Ctor_Standalone));
}

extern "C" void* OptionsDialog_StandaloneFactory(void) {
    return OptionsDialog_Factory_Bare(nullptr);
}
extern "C" void* OptionsDialog_StandaloneCreateInPlace(void* memory) {
    return OptionsDialog_CreateInPlace_Bare(memory);
}

REGISTER_TYPE(OptionsDialog, "UI::DialogInstance",
              &OptionsDialog_StandaloneFactory, &OptionsDialog_StandaloneCreateInPlace);

