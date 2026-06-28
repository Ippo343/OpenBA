// code/Game/Dialog/hotkeytooltipui.cpp
// Clean reimplementation of HotKeyToolTipUI::Serialize (Slot4, vtable +0x20).
//
// HotKeyToolTipUI (0x30 bytes) is the per-element DialogController attached to
// toolbar (and other) buttons that displays a button's keyboard shortcut. Each
// time the dialog refreshes (Slot4 / "Serialize") it:
//   1. reads the controlled element's shortcut id at element+0xC0 (set by
//      ToolbarDialog_Ctor_Impl when it auto-assigns Toolbar0..8 / Tool0..9 ids),
//   2. resolves that id to a (device, keyCode) pair via Input::KeyMap,
//   3. formats "<deviceName><keyCode>" (e.g. "Keyboard1") and looks it up in the
//      keyNames StringTable (Game container +0xD0) for a localized display
//      string,
//   4. writes it into the "textToolTip" (param 1) and "textHotKey" (param 0)
//      TextElements of its dialog.
//
// In standalone the generic UI_DIALOG_STUB left Slot4 a no-op, so the buttons'
// hotkey text kept the .rsc placeholder "[]" (the reported empty-brackets bug).
// Promoting Slot4 to this reimpl fills it. This is the same pattern as the
// BuildRequirementUI "????????" fix (code/Game/Dialog/buildrequirementui.cpp).
//
// Decompiled from Application-x64.exe:
//   HotKeyToolTipUI_Serialize @0x14006a980 (vtable 0x14015bfc0, slot 4 / +0x20)
//   key-name builder FUN_140099c20 (format "%s%i" -> keyNames lookup)
// The (device,key) -> UI::String step mirrors the proven GetInputKeyName helper
// already used by optionsdialog.cpp in standalone.

#include "engine_funcs.h"
#include <cstdint>
#include <cstdlib>
#include <cstdio>

namespace Game {

// Env-gated one-shot-per-element trace to verify the shortcut->key chain
// resolves valid data in standalone. Set BANISHED_HOTKEY_TRACE=1.
static bool HotKeyTraceOn() {
    static int s = -1;
    if (s < 0) { const char* e = getenv("BANISHED_HOTKEY_TRACE"); s = (e && e[0] && e[0] != '0') ? 1 : 0; }
    return s == 1;
}

// Controller layout (shared DialogController base, see buildrequirementui.cpp):
//   +0x18 = dialog instance (InternalBasePtr<DialogInstance>)
//   +0x28 = controlled element / datasource (InternalBasePtr<Object::Internal>)

// Live Input::KeyMap (maps shortcut id -> device/key). Triple-deref of the
// exref slot — identical to optionsdialog.cpp's GetActualKeyMap. Proven path:
// SetStandaloneKeyMap publishes the Engine[+0x6d8] keymap into this chain.
static void* GetActualKeyMap() {
    if (!g_pKeyMapObjectExref || !*g_pKeyMapObjectExref) return nullptr;
    void* step2 = *(void**)*g_pKeyMapObjectExref;
    if (!step2) return nullptr;
    return *(void**)step2;
}

// The Game container object (holds the keyNames StringTable at +0xD0). Single
// deref of g_pKeyMapSingleton (the slot holds the container pointer).
static void* GetKeyMapContainer() {
    if (!g_pKeyMapSingleton || !*g_pKeyMapSingleton) return nullptr;
    return *g_pKeyMapSingleton;
}

// Port of FUN_140099c20: build a UI::String for the (device,key) pair by
// formatting "<deviceName><keyIndex>" and resolving it against the keyNames
// StringTable. retval must be 16 bytes (UI::String). device < 0 => "Unbound".
static void BuildKeyName(void* retval, int device, int keyIndex) {
    void* container = GetKeyMapContainer();
    if (!container) return;

    uint8_t tempStr[8];
    if (device < 0) {
        fn_String_Ctor_CStr(tempStr, "Unbound");
    } else {
        static const char* kDeviceNames[] = { "Gamepad", "Keyboard", "Mouse" };
        const char* devName = (device <= 2) ? kDeviceNames[device] : "Unknown";
        fn_String_Format(tempStr, "%s%i", devName, keyIndex);
    }

    uint64_t keyNamesTable = *(uint64_t*)((uintptr_t)container + 0xD0);
    fn_UIString_Ctor(retval, keyNamesTable, tempStr);  // (retbuf, table, name)
    fn_String_Dtor(tempStr);
}

// Find the named TextElement in the controller's dialog, resolve the bound key
// into a localized System::String, and set it as the element's parameter.
static void FillHotKeyText(void* self, const char* elemName, int paramIndex,
                           void* keyMap, int shortcutId) {
    uint8_t nameBuf[8];
    fn_String_Ctor_CStr(nameBuf, elemName);
    fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)self + 0x18));
    void* dlgInst = *(void**)((uintptr_t)self + 0x18);
    uint64_t retval = 0;
    fn_DialogInstance_FindElement(dlgInst, &retval, nameBuf);
    fn_String_Dtor(nameBuf);

    void* textElem = (void*)retval;
    if (!textElem) return;

    int input  = fn_KeyMap_GetInputForId(keyMap, shortcutId, 0);
    int device = fn_KeyMap_GetDeviceForId(keyMap, shortcutId, 0);

    uint8_t uiStr[16];
    BuildKeyName(uiStr, device, input);

    // UI::String -> System::String (member, hidden retval). TextElement params
    // take a System::String; the converted string owns a ref we release below.
    uint8_t sysStr[8] = {0};
    fn_UIString_ToSystemString(uiStr, sysStr);

    fn_InternalBasePtr_CheckPtr(&textElem);
    fn_TextElement_SetParameter(textElem, paramIndex, sysStr);
    fn_String_Dtor(sysStr);
}

// HotKeyToolTipUI::Serialize — Slot4 (+0x20). self = the controller.
void HotKeyToolTipUI_Slot4(void* self) {
    if (!self) return;
    if (!fn_InternalBasePtr_Ctor || !fn_InternalBasePtr_CheckPtr || !fn_IsDerivedFrom
        || !fn_String_Ctor_CStr || !fn_String_Dtor || !fn_String_Format
        || !fn_UIString_Ctor || !fn_UIString_ToSystemString
        || !fn_DialogInstance_FindElement || !fn_TextElement_SetParameter
        || !fn_KeyMap_GetInputForId || !fn_KeyMap_GetDeviceForId
        || !g_TypeId_ObjectInternal) {
        return;
    }

    // Resolve the controlled element (the button) and read its shortcut id.
    void* obj = *(void**)((uintptr_t)self + 0x28);
    void* elem = (obj && fn_IsDerivedFrom(obj, *g_TypeId_ObjectInternal)) ? obj : nullptr;

    uint64_t elemIbp = 0;
    fn_InternalBasePtr_Ctor(&elemIbp, elem);
    if (!elemIbp) return;
    fn_InternalBasePtr_CheckPtr(&elemIbp);

    int shortcutId = *(int*)((uintptr_t)elemIbp + 0xC0);
    if (shortcutId < 0) return;  // no shortcut assigned -> leave default

    void* keyMap = GetActualKeyMap();
    if (!keyMap) return;

    if (HotKeyTraceOn()) {
        int input  = fn_KeyMap_GetInputForId(keyMap, shortcutId, 0);
        int device = fn_KeyMap_GetDeviceForId(keyMap, shortcutId, 0);
        void* cont = GetKeyMapContainer();
        void* keyNames = cont ? *(void**)((uintptr_t)cont + 0xD0) : nullptr;
        static int s_count = 0;
        if (s_count < 24) {
            printf("[HotKey] self=%p shortcutId=%d device=%d input=%d keyNames=%p\n",
                   self, shortcutId, device, input, keyNames);
            s_count++;
        }
    }

    FillHotKeyText(self, "textToolTip", 1, keyMap, shortcutId);
    FillHotKeyText(self, "textHotKey",  0, keyMap, shortcutId);
}

}  // namespace Game
