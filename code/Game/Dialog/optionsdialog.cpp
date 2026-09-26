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

#include "ui/TextElement.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/ComboElement.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/TabElement.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/Element.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/DialogInstance.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/String.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/StringTable.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/Sprite.h"   // ENDGAME B.3.6 UI callers (K2)
#include <new>   // ENDGAME B.3.6 UI callers (K2)
#include "engine/MainLoop.h"   // ENDGAME B.3.7 callers (K1)
#include "input/Input.h"   // ENDGAME B.3.7 callers (K3)
#include "Dialog/ui_dialogs.h"
// ENDGAME B.3.5 batch K1: video/Device.h FIRST. It pulls no Volume.h itself, but the
// house rule (a runtime type header ahead of anything that reaches <windows.h> through
// resolve.h, whose <wingdi.h> `Rectangle()` hides Volume.h's global `struct Rectangle`)
// is kept here too so a later include change cannot break it silently.
#include "video/Device.h"       // Video::Device, Video::Configuration
#include "engine_funcs.h"
#include "system/String.h"
#include "system/Filename.h"
#include "ui/UserInterface.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include "io/Persistence.h"   // ENDGAME B.3.4 caller batch P
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// In-app RenderDoc capture (standalone only) — ApplyVideoSettings arms a delayed
// capture after a video change so we can inspect the post-rebuild render targets.
#include "renderdoc_integration.h"
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "object/TypeInfo.h"   // ENDGAME B.3.3 step 6C

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
        System::String _nameStr(name);                     \
        uint64_t _retval = 0;                              \
        ::new (&_retval) Object::InternalPtr<UI::Element>(  /* B.3.6 bridge */ \
            reinterpret_cast<UI::DialogInstance*>(self)->FindElement(_nameStr));  \
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
    {
        uint8_t sprite[16];
        *(uint64_t*)&sprite[0] = *(uint64_t*)(km + 0xA8);
        *(uint64_t*)&sprite[8] = *(uint64_t*)(km + 0xB0);
        reinterpret_cast<UI::UserInterface*>(userInterface)->SetFullscreenFadeSprite(reinterpret_cast<const UI::Sprite*>(sprite));  // B.3.6 bridge
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

    System::String tempStr;
    if (deviceType < 0) {
        tempStr = System::String("Unbound");
    } else {
        const char* deviceNames[] = { "Gamepad", "Keyboard", "Mouse" };
        const char* devName = (deviceType >= 0 && deviceType <= 2) ? deviceNames[deviceType] : "Unknown";
        tempStr = System::String::Format("%s%i", devName, keyIndex);
    }

    // Construct UI::String from the KeyMap container's string table at +0xD0
    uint64_t stringTable = *(uint64_t*)((uintptr_t)keyMapContainer + 0xD0);
    ::new (retval) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(stringTable)), tempStr);  // B.3.6 bridge
}

// Helper: set the key name text on a button element (replaces FUN_140073490)
// Finds "textInputKey" child in the element, formats device+key into display text.
static void SetInputBindButtonText(void* elementPtr, int deviceType, uint32_t keyIndex) {
    // Find "textInputKey" in the element
    System::String nameStr("textInputKey");
    uint64_t found = 0;
    ::new (&found) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(*(void**)elementPtr)->FindElement(nameStr));  // B.3.6 bridge

    if (!found) return;

    // Check it's a TextElement
    void* foundObj = (void*)found;
    if (g_TypeId_TextElement && !fn_IsDerivedFrom(foundObj, *g_TypeId_TextElement))
        return;

    // Get the key name as UI::String, then set as text
    uint8_t uiStr[16];
    GetInputKeyName(uiStr, deviceType, keyIndex);

    reinterpret_cast<UI::TextElement*>(foundObj)->SetText(reinterpret_cast<const UI::String*>(uiStr));  // B.3.6 bridge
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
    if (!g_pFactorySingleton || !*g_pFactorySingleton)
        return;
    static uint32_t s_lazyId = 0;
    if (s_lazyId == 0) {
        void* factoryObj = *(void**)*g_pFactorySingleton;
        if (factoryObj) {
            s_lazyId = reinterpret_cast<const ::Object::Factory*>(factoryObj)->GetTypeIdFromName(
                "TextElement", true);   // B.3.3 bridge
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
    System::Filename filenameObj(path);   // ctor already normalises delimiters
    void* ext = const_cast<::Object::External*>(
        reinterpret_cast<::Object::ExternalFactory*>(*g_pExternalFactorySingleton)   // B.3.3 bridge
            ->FindOrLoad(filenameObj));
    if (!ext) return nullptr;

    if (!g_TypeId_StringTable
            && g_pFactorySingleton && *g_pFactorySingleton) {
        static uint32_t s_lazyStringTableTypeId = 0;
        if (s_lazyStringTableTypeId == 0) {
            void* factoryObj = *(void**)*g_pFactorySingleton;
            if (factoryObj) {
                s_lazyStringTableTypeId =
                    reinterpret_cast<const ::Object::Factory*>(factoryObj)->GetTypeIdFromName(
                        "StringTable", true);   // B.3.3 bridge
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
    if (!g_pVideoDeviceSingleton) return;
    if (self->savedRefreshDen == 0) {
        // Defaults from Video::Configuration ctor (Runtime @ 0x1800f6f10): 60/1.
        // Better to pretend "60Hz" than to crash; the next Apply will write
        // ValidateConfiguration's clamped values back to the singleton.
        self->savedRefreshNum = 60;
        self->savedRefreshDen = 1;
    }
    // B.3.5 bridge — THE DOOR DECIDES THE OVERLOAD. `Video::Device` has TWO
    // `ValidateConfiguration`s at adjacent ordinals; the global this call used is bound to
    // `?ValidateConfiguration@Device@Video@@QEAAXAEAH0000000@Z` (0x1800f7370, the EIGHT
    // `int&` out-parameter one), NOT `…QEAA_NXZ` (0x1800f8830, no arguments). Eight
    // arguments are spelled here so overload resolution cannot pick the sibling.
    reinterpret_cast<Video::Device*>(*g_pVideoDeviceSingleton)->ValidateConfiguration(
        *(int*)&self->savedRenderer, *(int*)&self->savedAdapter,
        *(int*)&self->savedWidth,    *(int*)&self->savedHeight,
        *(int*)&self->savedRefreshNum, *(int*)&self->savedRefreshDen,
        *(int*)&self->savedAntialias, *(int*)&self->savedAdapter);   // B.3.5 bridge
}

// ============================================================
// PopulateFilterCombo (replaces FUN_140073800)
// ============================================================
// Fills texture filter combo: "None", "Bilinear", then "Anisotropic Nx" for each level.
static void OptionsDialog_PopulateFilterCombo(OptionsDialog* self) {
    void* vidDev = g_pVideoDeviceSingleton ? *g_pVideoDeviceSingleton : nullptr;
    if (!vidDev) return;

    reinterpret_cast<UI::ComboElement*>(self->comboFilter)->RemoveAllItems();  // B.3.6 bridge

    void* stringTable = LoadStringTable("Dialog/StringTable.rsc:textureFilter");
    if (!stringTable) return;

    // Item 0: "None" (filter -1)
    {
        System::String textStr;
        ::new (&textStr) System::String(reinterpret_cast<const UI::StringTable*>(stringTable)->GetText(0));  // B.3.6 bridge
        uint8_t spriteBuf[16] = {};
        ::new (spriteBuf) UI::Sprite();
        reinterpret_cast<UI::ComboElement*>(self->comboFilter)->AddItem(*reinterpret_cast<const UI::Sprite*>(spriteBuf), textStr, (void*)(intptr_t)-1);  // B.3.6 bridge
    }

    // Item 1: "Bilinear" (filter 0)
    {
        System::String textStr;
        ::new (&textStr) System::String(reinterpret_cast<const UI::StringTable*>(stringTable)->GetText(1));  // B.3.6 bridge
        uint8_t spriteBuf[16] = {};
        ::new (spriteBuf) UI::Sprite();
        reinterpret_cast<UI::ComboElement*>(self->comboFilter)->AddItem(*reinterpret_cast<const UI::Sprite*>(spriteBuf), textStr, (void*)(intptr_t)0);  // B.3.6 bridge
    }

    // Anisotropic filter levels: "Anisotropic 1x", "Anisotropic 2x", etc.
    int anisoLevels = *(int*)((uintptr_t)vidDev + 0x98);
    if (anisoLevels > 0) {
        // First level: "Anisotropic 1x"
        System::String textStr;
        ::new (&textStr) System::String(reinterpret_cast<const UI::StringTable*>(stringTable)->GetText(2));  // B.3.6 bridge
        System::String fmtStr = System::String::Format("%s 1x", textStr._data);
        uint8_t spriteBuf[16] = {};
        ::new (spriteBuf) UI::Sprite();
        reinterpret_cast<UI::ComboElement*>(self->comboFilter)->AddItem(*reinterpret_cast<const UI::Sprite*>(spriteBuf), fmtStr, (void*)(intptr_t)1);  // B.3.6 bridge
    }
    // Higher levels: "Anisotropic 2x", "Anisotropic 4x", etc.
    for (int level = 2; level <= anisoLevels; level += 2) {
        System::String textStr;
        ::new (&textStr) System::String(reinterpret_cast<const UI::StringTable*>(stringTable)->GetText(2));  // B.3.6 bridge
        System::String fmtStr = System::String::Format("%s %ix", textStr._data, level);
        uint8_t spriteBuf[16] = {};
        ::new (spriteBuf) UI::Sprite();
        reinterpret_cast<UI::ComboElement*>(self->comboFilter)->AddItem(*reinterpret_cast<const UI::Sprite*>(spriteBuf), fmtStr, (void*)(intptr_t)level);  // B.3.6 bridge
    }

    // Set current selection based on savedFilter
    int filterVal = (int)self->savedFilter;
    int selIdx;
    if (filterVal == -1) selIdx = 0;
    else if (filterVal == 0) selIdx = 1;
    else if (filterVal == 1) selIdx = 2;
    else selIdx = (filterVal >> 1) + 2;

    reinterpret_cast<UI::ComboElement*>(self->comboFilter)->SetCurrentItem(selIdx);  // B.3.6 bridge
}

// ============================================================
// PopulateAntialiasCombo (replaces FUN_140073b10)
// ============================================================
// Fills antialias combo: "None", then "Nx multisample" for each supported mode.
static void OptionsDialog_PopulateAntialiasCombo(OptionsDialog* self) {
    void* vidDev = g_pVideoDeviceSingleton ? *g_pVideoDeviceSingleton : nullptr;
    if (!vidDev) return;

    reinterpret_cast<UI::ComboElement*>(self->comboAntialias)->RemoveAllItems();  // B.3.6 bridge

    void* stringTable = LoadStringTable("Dialog/StringTable.rsc:multiSample");

    int modeCount = reinterpret_cast<const Video::Device*>(vidDev)
                        ->GetMultisampleModeCount();   // B.3.5 bridge

    for (int i = 0; i < modeCount; i++) {
        bool supported = reinterpret_cast<const Video::Device*>(vidDev)
                             ->IsMultisampleModeSupported(i);   // B.3.5 bridge
        if (!supported) continue;

        int addedIdx;
        if (i == 0) {
            // "None"
            System::String textStr;
            if (stringTable) ::new (&textStr) System::String(reinterpret_cast<const UI::StringTable*>(stringTable)->GetText(0));  // B.3.6 bridge
            uint8_t spriteBuf[16] = {};
            ::new (spriteBuf) UI::Sprite();
            addedIdx = reinterpret_cast<UI::ComboElement*>(self->comboAntialias)->AddItem(*reinterpret_cast<const UI::Sprite*>(spriteBuf), textStr, (void*)(intptr_t)0);  // B.3.6 bridge
        } else {
            // "Nx multisample" — e.g. "2x Multisample"
            System::String textStr;
            if (stringTable) ::new (&textStr) System::String(reinterpret_cast<const UI::StringTable*>(stringTable)->GetText(1));  // B.3.6 bridge
            System::String fmtStr = System::String::Format("%ix %s", i + 1, textStr._data);
            uint8_t spriteBuf[16] = {};
            ::new (spriteBuf) UI::Sprite();
            addedIdx = reinterpret_cast<UI::ComboElement*>(self->comboAntialias)->AddItem(*reinterpret_cast<const UI::Sprite*>(spriteBuf), fmtStr, (void*)(intptr_t)i);  // B.3.6 bridge
        }

        if ((int)self->savedAntialias == i) {
            reinterpret_cast<UI::ComboElement*>(self->comboAntialias)->SetCurrentItem(addedIdx);  // B.3.6 bridge
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

    void* gfxIface = (void*)&reinterpret_cast<const Video::Device*>(vidDev)
                         ->GetInterface((int)self->savedRenderer);   // B.3.5 bridge
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

    reinterpret_cast<UI::ComboElement*>(self->comboRefresh)->RemoveAllItems();  // B.3.6 bridge

    int currentIdx = -1;
    for (int i = 0; i < refreshCount; i++) {
        int* refreshEntry = (int*)(refreshArray + (intptr_t)i * 8);
        int num = refreshEntry[0];
        int den = refreshEntry[1];
        int rate = (den != 0) ? (num / den) : 0;

        System::String fmtStr = System::String::Format("%i", rate);
        uint8_t spriteBuf[16] = {};
        ::new (spriteBuf) UI::Sprite();
        reinterpret_cast<UI::ComboElement*>(self->comboRefresh)->AddItem(*reinterpret_cast<const UI::Sprite*>(spriteBuf), fmtStr, (void*)(intptr_t)i);  // B.3.6 bridge

        if ((uint32_t)num == self->savedRefreshNum && (uint32_t)den == self->savedRefreshDen)
            currentIdx = i;
    }

    reinterpret_cast<UI::ComboElement*>(self->comboRefresh)->SetCurrentItem(currentIdx);  // B.3.6 bridge
}

// ============================================================
// PopulateResolutionCombo (replaces FUN_140073ec0)
// ============================================================
// Fills resolution combo with "WxH" entries for the current adapter.
static void OptionsDialog_PopulateResolutionCombo(OptionsDialog* self) {
    void* vidDev = g_pVideoDeviceSingleton ? *g_pVideoDeviceSingleton : nullptr;
    if (!vidDev) return;

    void* gfxIface = (void*)&reinterpret_cast<const Video::Device*>(vidDev)
                         ->GetInterface((int)self->savedRenderer);   // B.3.5 bridge
    intptr_t adapterArray = *(intptr_t*)((uintptr_t)gfxIface + 8);
    intptr_t adapter = adapterArray + (intptr_t)(int)self->savedAdapter * 0x38;
    int modeCount = *(int*)(adapter + 0x10);
    intptr_t modes = *(intptr_t*)(adapter + 8);

    printf("[OptionsDialog::PopulateResolutionCombo] savedW=%u savedH=%u modeCount=%d "
           "renderer=%u adapter=%u\n",
           self->savedWidth, self->savedHeight, modeCount,
           self->savedRenderer, self->savedAdapter);

    reinterpret_cast<UI::ComboElement*>(self->comboResolution)->RemoveAllItems();  // B.3.6 bridge

    int currentIdx = -1;
    for (int i = 0; i < modeCount; i++) {
        uint32_t w = *(uint32_t*)(modes + i * 0x18);
        uint32_t h = *(uint32_t*)(modes + i * 0x18 + 4);

        if (i < 5 || (w == self->savedWidth && h == self->savedHeight))
            printf("[OptionsDialog::PopRes]   mode[%d] = %ux%u%s\n", i, w, h,
                   (w == self->savedWidth && h == self->savedHeight) ? " <-- MATCH" : "");

        System::String fmtStr = System::String::Format("%ix%i", w, h);
        uint8_t spriteBuf[16] = {};
        ::new (spriteBuf) UI::Sprite();
        reinterpret_cast<UI::ComboElement*>(self->comboResolution)->AddItem(*reinterpret_cast<const UI::Sprite*>(spriteBuf), fmtStr, (void*)(intptr_t)i);  // B.3.6 bridge

        if (w == self->savedWidth && h == self->savedHeight)
            currentIdx = i;
    }

    reinterpret_cast<UI::ComboElement*>(self->comboResolution)->SetCurrentItem(currentIdx);  // B.3.6 bridge

    OptionsDialog_PopulateRefreshCombo(self);
}

// ============================================================
// PopulateAdapterCombo (replaces FUN_140074040)
// ============================================================
// Fills adapter combo for the current renderer, then repopulates dependent combos.
static void OptionsDialog_PopulateAdapterCombo(OptionsDialog* self) {
    void* vidDev = g_pVideoDeviceSingleton ? *g_pVideoDeviceSingleton : nullptr;
    if (!vidDev) return;

    reinterpret_cast<UI::ComboElement*>(self->comboAdapter)->RemoveAllItems();  // B.3.6 bridge

    void* gfxIface = (void*)&reinterpret_cast<const Video::Device*>(vidDev)
                         ->GetInterface((int)self->savedRenderer);   // B.3.5 bridge
    int adapterCount = *(int*)((uintptr_t)gfxIface + 0x10);
    intptr_t adapterArray = *(intptr_t*)((uintptr_t)gfxIface + 8);

    for (int i = 0; i < adapterCount; i++) {
        // Adapter name is a System::String at the start of each 0x38-byte entry
        void* adapterName = (void*)(adapterArray + (intptr_t)i * 0x38);
        uint8_t spriteBuf[16] = {};
        ::new (spriteBuf) UI::Sprite();
        reinterpret_cast<UI::ComboElement*>(self->comboAdapter)->AddItem(*reinterpret_cast<const UI::Sprite*>(spriteBuf), *reinterpret_cast<const System::String*>(adapterName), (void*)(intptr_t)i);  // B.3.6 bridge
    }

    reinterpret_cast<UI::ComboElement*>(self->comboAdapter)->SetCurrentItem((int)self->savedAdapter);  // B.3.6 bridge

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
    reinterpret_cast<UI::ComboElement*>(self->comboRenderer)->RemoveAllItems();  // B.3.6 bridge

    int ifaceCount = reinterpret_cast<const Video::Device*>(vidDev)
                         ->GetInterfaceCount();   // B.3.5 bridge
    for (int i = 0; i < ifaceCount; i++) {
        // GraphicsInterface starts with a System::String (the interface name)
        void* gfxIface = (void*)&reinterpret_cast<const Video::Device*>(vidDev)
                             ->GetInterface(i);   // B.3.5 bridge
        uint8_t spriteBuf[16] = {};
        ::new (spriteBuf) UI::Sprite();
        reinterpret_cast<UI::ComboElement*>(self->comboRenderer)->AddItem(*reinterpret_cast<const UI::Sprite*>(spriteBuf), *reinterpret_cast<const System::String*>(gfxIface), (void*)(intptr_t)i);  // B.3.6 bridge
    }

    reinterpret_cast<UI::ComboElement*>(self->comboRenderer)->SetCurrentItem((int)self->savedRenderer);  // B.3.6 bridge

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
        reinterpret_cast<UI::Element*>(buttonOk)->SetElementId(kButton_Ok);  // B.3.6 bridge
        reinterpret_cast<UI::Element*>(buttonCancel)->SetElementId(kButton_Cancel);  // B.3.6 bridge
        reinterpret_cast<UI::Element*>(buttonApply)->SetElementId(kButton_Apply);  // B.3.6 bridge
        if (buttonCancel) *(uint32_t*)((uintptr_t)buttonCancel + 0xC0) = 8;
    }

    // --- Audio sliders ---
    if (s_enableAudioSliders) {
        FIND_ELEMENT(self->sliderUI,     "sliderUI");
        FIND_ELEMENT(self->sliderMusic,  "sliderMusic");
        FIND_ELEMENT(self->sliderAmbient,"sliderAmbient");
        FIND_ELEMENT(self->sliderEffect, "sliderEffect");
        reinterpret_cast<UI::Element*>(self->sliderUI)->SetElementId(kSlider_UI);  // B.3.6 bridge
        reinterpret_cast<UI::Element*>(self->sliderMusic)->SetElementId(kSlider_Music);  // B.3.6 bridge
        reinterpret_cast<UI::Element*>(self->sliderAmbient)->SetElementId(kSlider_Ambient);  // B.3.6 bridge
        reinterpret_cast<UI::Element*>(self->sliderEffect)->SetElementId(kSlider_Effect);  // B.3.6 bridge
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
        reinterpret_cast<UI::Element*>(self->comboRenderer)->SetElementId(kCombo_Renderer);  // B.3.6 bridge
        reinterpret_cast<UI::Element*>(self->comboAdapter)->SetElementId(kCombo_Adapter);  // B.3.6 bridge
        reinterpret_cast<UI::Element*>(self->comboResolution)->SetElementId(kCombo_Resolution);  // B.3.6 bridge
        reinterpret_cast<UI::Element*>(self->comboRefresh)->SetElementId(kCombo_Refresh);  // B.3.6 bridge
        reinterpret_cast<UI::Element*>(self->comboAntialias)->SetElementId(kCombo_Antialias);  // B.3.6 bridge
        reinterpret_cast<UI::Element*>(self->comboFilter)->SetElementId(kCombo_Filter);  // B.3.6 bridge
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
            reinterpret_cast<UI::ComboElement*>(self->comboShadowRes)->SetCurrentItem(*(int32_t*)((uintptr_t)vidCfg + 0x2C));  // B.3.6 bridge
            reinterpret_cast<UI::ComboElement*>(self->comboShadowQuality)->SetCurrentItem(*(int32_t*)((uintptr_t)vidCfg + 0x30));  // B.3.6 bridge
            reinterpret_cast<UI::ComboElement*>(self->comboReflectionQuality)->SetCurrentItem(*(int32_t*)((uintptr_t)vidCfg + 0x34));  // B.3.6 bridge
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
                System::String textStr;
                ::new (&textStr) System::String(reinterpret_cast<const UI::StringTable*>(stringTable)->GetText(i));  // B.3.6 bridge
                uint8_t spriteBuf[16] = {};
                ::new (spriteBuf) UI::Sprite();
                reinterpret_cast<UI::ComboElement*>(self->comboAutosave)->AddItem(*reinterpret_cast<const UI::Sprite*>(spriteBuf), textStr, (void*)(intptr_t)i);  // B.3.6 bridge
            }
        }
        int autoSaveIdx = (g_pGameSettingsPtr && *g_pGameSettingsPtr) ? (*g_pGameSettingsPtr)[1] : 0;
        if (autoSaveIdx < 0) autoSaveIdx = 0;
        if (autoSaveIdx > 8) autoSaveIdx = 8;
        if (self->comboAutosave) reinterpret_cast<UI::ComboElement*>(self->comboAutosave)->SetCurrentItem(autoSaveIdx);  // B.3.6 bridge
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
            reinterpret_cast<UI::TabElement*>(self->tabOptions)->SetActiveTab(lastTab);  // B.3.6 bridge
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
    *(uint8_t*)((uintptr_t)inputCfg + 0x08) =
        *(uint8_t*)((uintptr_t)*(void**)&self->checkMouseClip + 0xD8);

    // Read software mouse checkbox -> Input::Configuration +0x0A
    *(uint8_t*)((uintptr_t)inputCfg + 0x0A) =
        *(uint8_t*)((uintptr_t)*(void**)&self->checkSoftwareMouse + 0xD8);

    // Enable cursor in UI
    void* userInterface = *(void**)((uintptr_t)self + 0x10);
    reinterpret_cast<UI::UserInterface*>(userInterface)->EnableCursor(true);  // B.3.6 bridge

    // Enable/disable mouse clipping based on checkbox
    bool mouseClip = *(uint8_t*)((uintptr_t)*(void**)&self->checkMouseClip + 0xD8);
    void* inputDevice = g_pInputDeviceSingleton ? *g_pInputDeviceSingleton : nullptr;
    if (inputDevice)
        reinterpret_cast<Input::Device*>(inputDevice)->EnableMouseClipping(static_cast<uint8_t>(mouseClip));  // B.3.7 bridge

    // Save input configuration
    if (inputCfg)
        reinterpret_cast<Input::Configuration*>(inputCfg)->Save();  // B.3.7 bridge
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
    gs[0] = (int32_t)*(float*)((uintptr_t)*(void**)&self->sliderUIScale + 0x128);

    // Autosave combo -> gs[1]
    gs[1] = *(int32_t*)((uintptr_t)*(void**)&self->comboAutosave + 0x118);

    // Status icon checkbox -> gs[3] (byte at offset +0x0C)
    *(uint8_t*)(&gs[3]) = *(uint8_t*)((uintptr_t)*(void**)&self->checkStatusIcon + 0xD8);

    // Status icon distance slider -> gs[2]
    gs[2] = (int32_t)*(float*)((uintptr_t)*(void**)&self->sliderStatusIcon + 0x128);

    // Status icon scale slider -> gs[4]
    // Original divides by 100.0f then multiplies by 100.0f (cancels out to identity)
    {
        float sliderVal = *(float*)((uintptr_t)*(void**)&self->sliderStatusIconScale + 0x128);
        gs[4] = (int32_t)sliderVal;
    }

    // Edge scroll checkbox -> gs[5] (byte)
    *(uint8_t*)(&gs[5]) = *(uint8_t*)((uintptr_t)*(void**)&self->checkEdgeScroll + 0xD8);

    // Show celsius -> byte at gs+0x15
    *(uint8_t*)((uintptr_t)gs + 0x15) =
        *(uint8_t*)((uintptr_t)*(void**)&self->checkShowCelsius + 0xD8);

    // Show weather -> byte at gs+0x16
    *(uint8_t*)((uintptr_t)gs + 0x16) =
        *(uint8_t*)((uintptr_t)*(void**)&self->checkShowWeather + 0xD8);

    // Auto slow -> byte at gs+0x17
    *(uint8_t*)((uintptr_t)gs + 0x17) =
        *(uint8_t*)((uintptr_t)*(void**)&self->checkAutoslow + 0xD8);

    // Auto pause -> gs[6] (byte)
    *(uint8_t*)(&gs[6]) = *(uint8_t*)((uintptr_t)*(void**)&self->checkAutopause + 0xD8);

    // Last active tab -> gs[7]
    gs[7] = *(int32_t*)((uintptr_t)*(void**)&self->tabOptions + 0xF8);

    // Iterate all open dialogs and update autosave timer on each
    UI::UserInterface* userInterface = *(UI::UserInterface**)((uintptr_t)self + 0x10);
    fprintf(stderr, "[AutoSave] Options ApplySettings: newIntervalIdx=%d isA=%p isDerived=%p restartFn=%p tidSlot=%p\n",
            gs[1], (void*)fn_IsA, (void*)fn_IsDerivedFrom,
            (void*)fn_UpdateAutoSaveTimer, (void*)g_TypeId_AutoSaveDialogType);
    fflush(stderr);
    if (fn_IsA && fn_IsDerivedFrom && fn_UpdateAutoSaveTimer &&
        g_TypeId_AutoSaveDialogType) {
        uint32_t typeId = *g_TypeId_AutoSaveDialogType;
        // `_dialogs.GetFirst()` IS retail's GetFirstDialog (0x180031150, whose
        // whole body is `*retval = this->_dialogs._head`); the iterator is the
        // bare node pointer, so `it.node` is what retail hands to CheckPtr.
        for (auto it = userInterface->_dialogs.GetFirst(); it; ++it) {
            void* dialog = it.node->element;
            if (fn_IsA(dialog, typeId)) {
                void* obj = it.node->element;
                bool isDerived = fn_IsDerivedFrom(obj, typeId);
                void* target = isDerived ? obj : nullptr;
                fprintf(stderr, "[AutoSave]   MATCH obj=%p derived=%d -> restart %s\n",
                        obj, (int)isDerived, target ? "YES" : "NO(null target)");
                fflush(stderr);
                if (target) fn_UpdateAutoSaveTimer(target);
            }
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
    int rendItem = *(int*)((uintptr_t)*(void**)&self->comboRenderer + 0x118);
    int newRenderer = (int)(intptr_t)reinterpret_cast<const UI::ComboElement*>(*(void**)&self->comboRenderer)->GetItemData(rendItem);  // B.3.6 bridge

    // Adapter
    int adpItem = *(int*)((uintptr_t)*(void**)&self->comboAdapter + 0x118);
    int newAdapter = (int)(intptr_t)reinterpret_cast<const UI::ComboElement*>(*(void**)&self->comboAdapter)->GetItemData(adpItem);  // B.3.6 bridge

    // Resolution
    int resItem = *(int*)((uintptr_t)*(void**)&self->comboResolution + 0x118);
    void* resData = reinterpret_cast<const UI::ComboElement*>(*(void**)&self->comboResolution)->GetItemData(resItem);  // B.3.6 bridge

    // Refresh
    int refItem = *(int*)((uintptr_t)*(void**)&self->comboRefresh + 0x118);

    // Look up actual width/height/refresh from the video interface data
    void* gfxIface = (void*)&reinterpret_cast<const Video::Device*>(vidDev)
                         ->GetInterface(newRenderer);   // B.3.5 bridge
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
    int aaItem = *(int*)((uintptr_t)*(void**)&self->comboAntialias + 0x118);
    int newAntialias = (int)(intptr_t)reinterpret_cast<const UI::ComboElement*>(*(void**)&self->comboAntialias)->GetItemData(aaItem);  // B.3.6 bridge

    // Filter
    int fltItem = *(int*)((uintptr_t)*(void**)&self->comboFilter + 0x118);
    int newFilter = (int)(intptr_t)reinterpret_cast<const UI::ComboElement*>(*(void**)&self->comboFilter)->GetItemData(fltItem);  // B.3.6 bridge

    // Shadow res, shadow quality, reflection quality
    int newShadowRes = *(int*)((uintptr_t)*(void**)&self->comboShadowRes + 0x118);
    int newShadowQ = *(int*)((uintptr_t)*(void**)&self->comboShadowQuality + 0x118);
    int newReflQ = *(int*)((uintptr_t)*(void**)&self->comboReflectionQuality + 0x118);

    // Fullscreen / VSync checkboxes
    uint8_t newFullscreen = *(uint8_t*)((uintptr_t)*(void**)&self->checkFullscreen + 0xD8);
    uint8_t newVsync = *(uint8_t*)((uintptr_t)*(void**)&self->checkVSync + 0xD8);

    // Check if anything changed. If nothing changed, return early.
    bool rendererChanged = (oldRenderer != newRenderer);
    bool adapterChanged = (oldAdapter != newAdapter);
    bool needsFullRestart = rendererChanged || adapterChanged;


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
                    return;  // Nothing changed
                }
            }
        }
    }

    // A video setting actually changed → arm a RenderDoc capture ~8 frames out
    // so we capture a steady-state frame AFTER the engine's deferred swapchain/
    // target rebuild (BeginFrame consumes Device+0x9c next frame and skips it).
    // No-op unless launched via RenderDoc's UI. Lets us inspect the scene render
    // target's actual SampleDesc.Count to confirm whether MSAA is bound.
    RenderDocIntegration::RequestCaptureInFrames(8);

    // If renderer or adapter changed, need full shutdown/startup
    if (needsFullRestart) {
        void* mainLoop = g_pMainLoopSingleton ? *g_pMainLoopSingleton : nullptr;
        if (mainLoop)
            reinterpret_cast<Engine::MainLoop*>(mainLoop)->OnVideoError();  // B.3.7 bridge
        reinterpret_cast<Video::Device*>(vidDev)->ReleaseDynamic();   // B.3.5 bridge
        reinterpret_cast<Video::Device*>(vidDev)->Shutdown();         // B.3.5 bridge
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
        success = reinterpret_cast<Video::Device*>(vidDev)
                      ->ApplyChangedSettings();   // B.3.5 bridge
    } else {
        // Full restart
        success = reinterpret_cast<Video::Device*>(vidDev)->Startup();   // B.3.5 bridge
        reinterpret_cast<Video::Device*>(vidDev)->RecreateDynamic();     // B.3.5 bridge
        void* mainLoop = g_pMainLoopSingleton ? *g_pMainLoopSingleton : nullptr;
        if (mainLoop)
            reinterpret_cast<Engine::MainLoop*>(mainLoop)->OnVideoRestart();  // B.3.7 bridge
    }


    if (success) {
        // Save the new config
        reinterpret_cast<Video::Configuration*>(vidCfg)->Save();   // B.3.5 bridge
    } else {
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
    if (persistence)
        reinterpret_cast<IO::Persistence*>(persistence)->ReadSettings();   // B.3.4 bridge

    // Re-read game settings from persistence
    if (fn_ReadGameSettings && g_pGameSettingsPtr && *g_pGameSettingsPtr)
        fn_ReadGameSettings(*g_pGameSettingsPtr);

    // Restore last tab
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
    if (g_pPersistenceSingleton && *g_pPersistenceSingleton)
        reinterpret_cast<const IO::Persistence*>(*g_pPersistenceSingleton)->WriteSettings();   // B.3.4 bridge
}

// ============================================================
// OnOk (0x140072930): Apply all settings, save, and close dialog
// ============================================================
// Same as OnApply plus closes the dialog window.

void OptionsDialog_OnOk(OptionsDialog* self) {
    OptionsDialog_OnApply(self);

    // Close dialog
    uint8_t ibpBuf[8];
    new (ibpBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(self));   // B.3.3 bridge
    void* userInterface = *(void**)((uintptr_t)self + 0x10);
    reinterpret_cast<UI::UserInterface*>(userInterface)->CloseDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(ibpBuf));  // B.3.6 bridge
}

// ============================================================
// OnCancel (0x140072840): Revert settings and close dialog
// ============================================================
// Reloads settings from disk, re-registers default input bindings,
// restores video settings, and closes the dialog.

void OptionsDialog_OnCancel(OptionsDialog* self) {
    // Reload persistence settings from disk
    if (g_pPersistenceSingleton && *g_pPersistenceSingleton)
        reinterpret_cast<IO::Persistence*>(*g_pPersistenceSingleton)->ReadSettings();   // B.3.4 bridge

    // Re-register default input bindings and load saved config
    if (fn_RegisterDefaultInputBindings)
        fn_RegisterDefaultInputBindings();

    // Reload input configuration
    if (g_pInputConfigSingleton && *g_pInputConfigSingleton)
        reinterpret_cast<Input::Configuration*>(*g_pInputConfigSingleton)->Load();  // B.3.7 bridge

    // Restore mouse clipping from input config byte at +0x08
    if (g_pInputDeviceSingleton && *g_pInputDeviceSingleton &&
        g_pInputConfigSingleton && *g_pInputConfigSingleton) {
        bool clipEnabled = *(bool*)((uintptr_t)*g_pInputConfigSingleton + 0x08);
        reinterpret_cast<Input::Device*>(*g_pInputDeviceSingleton)->EnableMouseClipping(static_cast<uint8_t>(clipEnabled));  // B.3.7 bridge
    }

    // Reload persistence settings again (for audio config)
    if (g_pPersistenceSingleton && *g_pPersistenceSingleton)
        reinterpret_cast<IO::Persistence*>(*g_pPersistenceSingleton)->ReadSettings();   // B.3.4 bridge

    // Reinitialize audio from saved config
    if (g_pAudioConfigSingleton && *g_pAudioConfigSingleton && fn_AudioConfig_Startup)
        fn_AudioConfig_Startup(*g_pAudioConfigSingleton);

    // Restore video settings to pre-dialog state
    OptionsDialog_RestoreVideoSettings(self);

    // Close dialog
    uint8_t ibpBuf[8];
    new (ibpBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(self));   // B.3.3 bridge
    void* userInterface = *(void**)((uintptr_t)self + 0x10);
    reinterpret_cast<UI::UserInterface*>(userInterface)->CloseDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(ibpBuf));  // B.3.6 bridge
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
            // FUN_140010460: IsDerivedFrom(elem, UIElementBase type) — returns elem if valid, else null
            void* clickedElem = *(void**)element;
            bool isUIElement = g_TypeId_ButtonElement && fn_IsDerivedFrom(clickedElem, *g_TypeId_ButtonElement);
            void* parent = isUIElement ? clickedElem : nullptr;
            uint8_t parentPtr[16];
            new (parentPtr) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(parent));   // B.3.3 bridge

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
            int curItem = *(int*)((uintptr_t)self->comboRenderer + 0x118);
            void* data = reinterpret_cast<const UI::ComboElement*>(self->comboRenderer)->GetItemData(curItem);  // B.3.6 bridge
            self->savedRenderer = (uint32_t)(intptr_t)data;
            OptionsDialog_ValidateVideoConfig(self);
            OptionsDialog_PopulateAdapterCombo(self);
            return;
        }
        if (eventData == kCombo_Adapter) {
            int curItem = *(int*)((uintptr_t)self->comboAdapter + 0x118);
            void* data = reinterpret_cast<const UI::ComboElement*>(self->comboAdapter)->GetItemData(curItem);  // B.3.6 bridge
            self->savedAdapter = (uint32_t)(intptr_t)data;
            OptionsDialog_ValidateVideoConfig(self);
            OptionsDialog_PopulateAntialiasCombo(self);
            OptionsDialog_PopulateFilterCombo(self);
            OptionsDialog_PopulateResolutionCombo(self);
            return;
        }
        if (eventData == kCombo_Resolution) {
            int curItem = *(int*)((uintptr_t)self->comboResolution + 0x118);
            void* resData = reinterpret_cast<const UI::ComboElement*>(self->comboResolution)->GetItemData(curItem);  // B.3.6 bridge

            int adapterIdx = self->savedAdapter;
            void* gfxIface = (void*)&reinterpret_cast<const Video::Device*>(*g_pVideoDeviceSingleton)
                                 ->GetInterface(self->savedRenderer);   // B.3.5 bridge
            int adapterIdx2 = self->savedAdapter;

            intptr_t adapterArray = *(intptr_t*)((uintptr_t)gfxIface + 8);
            intptr_t displayModes = *(intptr_t*)(adapterArray + (intptr_t)adapterIdx * 0x38 + 8);
            intptr_t modeEntry = displayModes + (intptr_t)(int)(intptr_t)resData * 0x18;

            self->savedWidth = *(uint32_t*)modeEntry;

            gfxIface = (void*)&reinterpret_cast<const Video::Device*>(*g_pVideoDeviceSingleton)
                                 ->GetInterface(self->savedRenderer);   // B.3.5 bridge
            adapterArray = *(intptr_t*)((uintptr_t)gfxIface + 8);
            displayModes = *(intptr_t*)(adapterArray + (intptr_t)adapterIdx2 * 0x38 + 8);
            self->savedHeight = *(uint32_t*)(displayModes + (intptr_t)(int)(intptr_t)resData * 0x18 + 4);

            OptionsDialog_ValidateVideoConfig(self);
            OptionsDialog_PopulateRefreshCombo(self);
            return;
        }
        if (eventData == kCombo_Refresh) {
            int resItem = *(int*)((uintptr_t)self->comboResolution + 0x118);
            void* resData = reinterpret_cast<const UI::ComboElement*>(self->comboResolution)->GetItemData(resItem);  // B.3.6 bridge

            int refItem = *(int*)((uintptr_t)self->comboRefresh + 0x118);
            void* refData = reinterpret_cast<const UI::ComboElement*>(self->comboRefresh)->GetItemData(refItem);  // B.3.6 bridge

            int adapterIdx = self->savedAdapter;
            void* gfxIface = (void*)&reinterpret_cast<const Video::Device*>(*g_pVideoDeviceSingleton)
                                 ->GetInterface(self->savedRenderer);   // B.3.5 bridge
            int adapterIdx2 = self->savedAdapter;

            intptr_t adapterArray = *(intptr_t*)((uintptr_t)gfxIface + 8);
            intptr_t displayMode = adapterArray + (intptr_t)adapterIdx * 0x38;
            intptr_t modeEntry = *(intptr_t*)(displayMode + 8) + (intptr_t)(int)(intptr_t)resData * 0x18;
            intptr_t refreshArray = *(intptr_t*)(modeEntry + 8);
            intptr_t refreshEntry = refreshArray + (intptr_t)(int)(intptr_t)refData * 8;

            self->savedRefreshNum = *(uint32_t*)refreshEntry;

            gfxIface = (void*)&reinterpret_cast<const Video::Device*>(*g_pVideoDeviceSingleton)
                                 ->GetInterface(self->savedRenderer);   // B.3.5 bridge
            adapterArray = *(intptr_t*)((uintptr_t)gfxIface + 8);
            displayMode = adapterArray + (intptr_t)adapterIdx2 * 0x38;
            modeEntry = *(intptr_t*)(displayMode + 8) + (intptr_t)(int)(intptr_t)resData * 0x18;
            refreshArray = *(intptr_t*)(modeEntry + 8);
            self->savedRefreshDen = *(uint32_t*)(refreshArray + (intptr_t)(int)(intptr_t)refData * 8 + 4);

            OptionsDialog_ValidateVideoConfig(self);
            return;
        }
        if (eventData == kCombo_Antialias) {
            int curItem = *(int*)((uintptr_t)self->comboAntialias + 0x118);
            void* data = reinterpret_cast<const UI::ComboElement*>(self->comboAntialias)->GetItemData(curItem);  // B.3.6 bridge
            self->savedAntialias = (uint32_t)(intptr_t)data;
            OptionsDialog_ValidateVideoConfig(self);
            return;
        }
        if (eventData == kCombo_Filter) {
            int curItem = *(int*)((uintptr_t)self->comboFilter + 0x118);
            void* data = reinterpret_cast<const UI::ComboElement*>(self->comboFilter)->GetItemData(curItem);  // B.3.6 bridge
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

    void* inputList = self->inputList;
    int childCount = reinterpret_cast<const UI::Element*>(inputList)->ChildCount();  // B.3.6 bridge

    int numRows = childCount / 3;
    if (numRows <= 0) return;

    int elemId = kInputBind_Base;  // 13
    int inputId = 5;  // KeyMap input IDs start at 5

    for (int row = 0; row < numRows; row++, inputId++) {
        // Get the 3 children for this row
        uint64_t rowChild = 0;
        ::new (&rowChild) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(self->inputList)->Child(row * 3));  // B.3.6 bridge

        // Find "textInputName" in the row container and set label
        {
            System::String nameStr("textInputName");
            uint64_t textElem = 0;
            ::new (&textElem) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>((void*)rowChild)->FindElement(nameStr));  // B.3.6 bridge

            if (textElem) {
                void* textObj = (void*)textElem;
                if (g_TypeId_TextElement && fn_IsDerivedFrom(textObj, *g_TypeId_TextElement)) {
                    // Get input name from KeyMap container's name table at +0xC8
                    uint64_t nameTable = *(uint64_t*)((uintptr_t)keyMapContainer + 0xC8);
                    uint8_t uiStr[16];
                    ::new (uiStr) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(nameTable)), inputId);  // B.3.6 bridge
                    reinterpret_cast<UI::TextElement*>(textObj)->SetText(reinterpret_cast<const UI::String*>(uiStr));  // B.3.6 bridge
                }
            }
        }

        // Set bind button texts and element IDs for slots 0 and 1
        for (int slot = 0; slot < 2; slot++) {
            uint64_t btnChild = 0;
            ::new (&btnChild) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(self->inputList)->Child(row * 3 + 1 + slot));  // B.3.6 bridge

            // Get current device and key for this input binding
            int device = reinterpret_cast<const Input::KeyMap*>(keyMap)->GetDeviceForId(inputId, slot);  // B.3.7 bridge
            int key = reinterpret_cast<const Input::KeyMap*>(keyMap)->GetInputForId(inputId, slot);  // B.3.7 bridge

            // Set the button's key name text
            SetInputBindButtonText((void*)&btnChild, device, key);

            // Assign element ID
            reinterpret_cast<UI::Element*>((void*)btnChild)->SetElementId(elemId + slot);  // B.3.6 bridge
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
    System::Filename filenameObj("Dialog/Options.rsc:inputBind");   // ctor already normalises
    void* extResult = const_cast<::Object::External*>(
        reinterpret_cast<::Object::ExternalFactory*>(*g_pExternalFactorySingleton)   // B.3.3 bridge
            ->FindOrLoad(filenameObj));
    // Type check the external resource
    void* external = nullptr;
    if (extResult && g_TypeId_Dialog) {
        bool isDerived = fn_IsDerivedFrom(extResult, *g_TypeId_Dialog);
        if (isDerived) external = extResult;
    }

    // Allocate and construct OptionInputBindDialog
    void* mem = fn_MemAlloc(g_pMemAllocSingleton, 200, 8);  // 0xC8 = 200 bytes
    void* dialog = nullptr;
    if (mem) {
        uint8_t nullPtr[8] = {};
        new (nullPtr) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        void* userInterface = *(void**)((uintptr_t)self + 0x10);
        dialog = fn_OptionInputBindDialog_Ctor(mem, userInterface, external, nullPtr);
    }

    // Wrap in InternalPtr
    uint64_t dialogPtr = 0;
    new ((void*)&dialogPtr) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(dialog));   // B.3.3 bridge

    // Set the input name text on the dialog
    // Get input name from KeyMap container's name table at +0xC8
    uint8_t inputNameUI[16];
    uint64_t nameTable = *(uint64_t*)((uintptr_t)keyMapContainer + 0xC8);
    ::new (inputNameUI) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(nameTable)), inputId);  // B.3.6 bridge

    // Convert to System::String
    System::String inputNameStr;
    ::new (&inputNameStr) System::String(reinterpret_cast<const UI::String*>(inputNameUI)->operator System::String());  // B.3.6 bridge

    // Find TextElement in dialog and set parameter 0
    if (g_TypeId_TextElement) {
        uint64_t textElem = 0;
        ::new (&textElem) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>((void*)dialogPtr)->FindElementType(*g_TypeId_TextElement));  // B.3.6 bridge
        if (textElem) {
            reinterpret_cast<UI::TextElement*>((void*)textElem)->SetParameter(0, inputNameStr);  // B.3.6 bridge
        }
    }

    // Show dialog modally — get result
    void* userInterface = *(void**)((uintptr_t)&self->base + 0x10);
    uint64_t showPtr = dialogPtr;
    int result = reinterpret_cast<UI::UserInterface*>(userInterface)->ShowDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(&showPtr));  // B.3.6 bridge

    // Read captured key data from dialog before deleting
    int capturedDevice = *(int*)((uintptr_t)(void*)dialogPtr + 0xC4);
    int capturedKey = *(int*)((uintptr_t)(void*)dialogPtr + 0xC0);

    // Check if the new binding conflicts with an existing one
    int conflictId = reinterpret_cast<const Input::KeyMap*>(keyMap)->GetIdForDeviceInput(capturedDevice, capturedKey);  // B.3.7 bridge

    // Delete the input bind dialog
    reinterpret_cast< ::Object::InternalBasePtr*>((void*)&dialogPtr)->DeletePtr();   // B.3.3 bridge

    if (result == 2) {
        // User chose "clear" — unbind this slot
        reinterpret_cast<Input::KeyMap*>(keyMap)->ClearInput(static_cast<int32_t>(inputId), static_cast<int32_t>(slot));  // B.3.7 bridge
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
        System::Filename fnameObj2("Dialog/Options.rsc:inputBound");   // ctor already normalises
        void* extResult2 = const_cast<::Object::External*>(
            reinterpret_cast<::Object::ExternalFactory*>(*g_pExternalFactorySingleton)   // B.3.3 bridge
                ->FindOrLoad(fnameObj2));
        void* external2 = nullptr;
        if (extResult2 && g_TypeId_Dialog) {
            if (fn_IsDerivedFrom(extResult2, *g_TypeId_Dialog)) external2 = extResult2;
        }

        // Allocate and construct MessageDialog (0xC0 bytes)
        void* mem2 = fn_MemAlloc(g_pMemAllocSingleton, 0xC0, 8);
        void* dialog2 = nullptr;
        if (mem2) {
            uint8_t nullPtr2[8] = {};
            new (nullPtr2) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
            void* ui = *(void**)((uintptr_t)self + 0x10);
            dialog2 = fn_MessageDialog_Ctor(mem2, ui, external2, nullPtr2);
        }

        uint64_t dialog2Ptr = 0;
        new ((void*)&dialog2Ptr) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(dialog2));   // B.3.3 bridge

        // Get the name of the key that was pressed (new binding)
        uint8_t newKeyUI[16];
        GetInputKeyName(newKeyUI, capturedDevice, capturedKey);
        System::String newKeyStr;
        ::new (&newKeyStr) System::String(reinterpret_cast<const UI::String*>(newKeyUI)->operator System::String());  // B.3.6 bridge

        // Get the name of the existing binding that conflicts
        uint8_t conflictNameUI[16];
        ::new (conflictNameUI) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(nameTable)), conflictId);  // B.3.6 bridge
        System::String conflictNameStr;
        ::new (&conflictNameStr) System::String(reinterpret_cast<const UI::String*>(conflictNameUI)->operator System::String());  // B.3.6 bridge

        // Check if the conflicting binding is the same as the current one
        // (in which case no confirmation is needed)
        bool sameBinding = false;
        {
            const char* str1 = inputNameStr._data;
            const char* str2 = conflictNameStr._data;
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
                uint64_t textElem2 = 0;
                ::new (&textElem2) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>((void*)dialog2Ptr)->FindElementType(*g_TypeId_TextElement));  // B.3.6 bridge
                if (textElem2) {
                    reinterpret_cast<UI::TextElement*>((void*)textElem2)->SetParameter(0, newKeyStr);  // B.3.6 bridge
                }

                uint64_t textElem3 = 0;
                ::new (&textElem3) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>((void*)dialog2Ptr)->FindElementType(*g_TypeId_TextElement));  // B.3.6 bridge
                if (textElem3) {
                    reinterpret_cast<UI::TextElement*>((void*)textElem3)->SetParameter(1, conflictNameStr);  // B.3.6 bridge
                }

                uint64_t textElem4 = 0;
                ::new (&textElem4) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>((void*)dialog2Ptr)->FindElementType(*g_TypeId_TextElement));  // B.3.6 bridge
                if (textElem4) {
                    reinterpret_cast<UI::TextElement*>((void*)textElem4)->SetParameter(2, inputNameStr);  // B.3.6 bridge
                }
            }

            // Show confirmation dialog
            uint64_t show2Ptr = dialog2Ptr;
            confirmResult = reinterpret_cast<UI::UserInterface*>(userInterface)->ShowDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(&show2Ptr));  // B.3.6 bridge
            reinterpret_cast< ::Object::InternalBasePtr*>((void*)&dialog2Ptr)->DeletePtr();   // B.3.3 bridge
        }

        if (confirmResult >= 1 && confirmResult <= 2) {
            // User confirmed (OK or Yes) — clear the old conflicting binding
            reinterpret_cast<Input::KeyMap*>(keyMap)->ClearInput(static_cast<int32_t>(conflictId), static_cast<int32_t>(capturedDevice), static_cast<int32_t>(capturedKey));  // B.3.7 bridge
            goto set_binding;
        }
        // User cancelled — don't change anything
    }

    // Refresh the input bind list display
    OptionsDialog_PopulateInputBindList(self);
    goto cleanup;

set_binding:
    reinterpret_cast<Input::KeyMap*>(keyMap)->SetInput(inputId, slot, capturedDevice, capturedKey);  // B.3.7 bridge
    reinterpret_cast<Input::Device*>(*g_pInputDeviceSingleton)->ResetState();  // B.3.7 bridge

    // Refresh the entire input bind list
    OptionsDialog_PopulateInputBindList(self);

cleanup:
    ;
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

