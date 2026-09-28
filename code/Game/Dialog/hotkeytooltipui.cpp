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

#include "ui/UserInterface.h"
#include "object/TypeInfo.h"
#include "Game/game.h"   // B3R STORAGE: g_gameContainer
#include "Dialog/HotKeyToolTipUI.h"
#include "Object/factory_registration.h"
#include "system/String.h"
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "runtime/types/ui/Element.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/DialogInstance.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/String.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/StringTable.h"  // ENDGAME B.3.6 UI callers (K3)
#include "object/External.h"  // ENDGAME B.3.6 UI callers (K3)
#include <new>  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/TextElement.h"  // ENDGAME B.3.6 UI callers round 2 (K3)
#include "input/Input.h"  // ENDGAME B.3.7 callers (K3)

namespace Game {


// Controller layout (shared DialogController base, see buildrequirementui.cpp):
//   +0x18 = dialog instance (InternalBasePtr<DialogInstance>)
//   +0x28 = controlled element / datasource (InternalBasePtr<Object::Internal>)

// Live Input::KeyMap (maps shortcut id -> device/key). Triple-deref of the
// exref slot — identical to optionsdialog.cpp's GetActualKeyMap. Proven path:
// SetStandaloneKeyMap publishes the Engine[+0x6d8] keymap into this chain.
static void* GetActualKeyMap() {
    if (Input::Configuration::_singleton == nullptr) return nullptr;
    void* step2 = Input::Configuration::_singleton;
    if (!step2) return nullptr;
    return *(void**)step2;
}

// The Game container object (holds the keyNames StringTable at +0xD0). Single
// deref of g_pKeyMapSingleton (the slot holds the container pointer).
static void* GetKeyMapContainer() {
    if (g_gameContainer == nullptr) return nullptr;
    return g_gameContainer;
}

// Port of FUN_140099c20: build a UI::String for the (device,key) pair by
// formatting "<deviceName><keyIndex>" and resolving it against the keyNames
// StringTable. retval must be 16 bytes (UI::String). device < 0 => "Unbound".
//
// Exported (2026-08-30, LANE1 row 36) so the two Tutorial call sites can share it
// instead of resolving the raw address 0x140099c20, which is unregistered and so
// left tutorial key names blank. Retail's param_1 is the KeyMap container whose
// +0xD0 is the keyNames StringTable; both tutorial callers pass
// *g_pKeyMapSingleton, which is exactly the container this function looks up, so
// the substitution is exact.
void Hotkey_BuildKeyName(void* retval, int device, int keyIndex);
void Hotkey_BuildKeyName(void* retval, int device, int keyIndex) {
    void* container = GetKeyMapContainer();
    if (!container) return;

    System::String tempStr;
    if (device < 0) {
        tempStr.Set("Unbound");
    } else {
        static const char* kDeviceNames[] = { "Gamepad", "Keyboard", "Mouse" };
        const char* devName = (device <= 2) ? kDeviceNames[device] : "Unknown";
        tempStr = System::String::Format("%s%i", devName, keyIndex);
    }

    uint64_t keyNamesTable = *(uint64_t*)((uintptr_t)container + 0xD0);
    ::new (retval) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(keyNamesTable)), tempStr);  // B.3.6 bridge; (retbuf, table, name)
}

// Find the named TextElement in the controller's dialog, resolve the bound key
// into a localized System::String, and set it as the element's parameter.
static void FillHotKeyText(void* self, const char* elemName, int paramIndex,
                           void* keyMap, int shortcutId) {
    System::String nameStr(elemName);
    void* dlgInst = *(void**)((uintptr_t)self + 0x18);
    uint64_t retval = 0;
    ::new (&retval) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(dlgInst)->FindElement(nameStr));  // B.3.6 bridge

    void* textElem = (void*)retval;
    if (!textElem) return;

    int input  = reinterpret_cast<const Input::KeyMap*>(keyMap)->GetInputForId(shortcutId, 0);  // B.3.7 bridge
    int device = reinterpret_cast<const Input::KeyMap*>(keyMap)->GetDeviceForId(shortcutId, 0);  // B.3.7 bridge

    uint8_t uiStr[16];
    Hotkey_BuildKeyName(uiStr, device, input);

    // UI::String -> System::String (member, hidden retval). TextElement params
    // take a System::String; the converted string owns a ref released by its dtor.
    System::String sysStr;
    ::new (&sysStr) System::String(reinterpret_cast<const UI::String*>(uiStr)->operator System::String());  // B.3.6 bridge

    reinterpret_cast<UI::TextElement*>(textElem)->SetParameter(paramIndex, sysStr);  // B.3.6 bridge
}

}  // namespace Game

// ---- HotKeyToolTipUI: PROMOTED to the real reimpl (2026-06-28) ---------------
// This controller is bound to the "textHotKey"/"textToolTip" TextElements of
// every toolbar button. Its Slot4 (Serialize, +0x20) reads the button's
// auto-assigned shortcut id (element+0xC0), resolves it through Input::KeyMap +
// the keyNames StringTable, and fills the hotkey text. The generic stub left
// Slot4 a no-op, so the buttons showed the .rsc placeholder "[]" (empty
// brackets). Wiring Slot4 to the real HotKeyToolTipUI_Slot4 restores the key
// display. Construction stays the stub alloc+zero+vtable; the engine fills the
// base fields (dialog +0x18, datasource +0x28) afterward. Same shape as the
// BuildRequirementUI promotion above.
// ENDGAME B.3.9: the reimpl unconditionally dereferences a type id for the button
// element. That cell (exref 0x140151660) is UI::Element's own _typeInfo (Table 1 /
// standalone/reg_element_typeids.cpp), NOT "Internal" as this file's old heal
// guessed (Object::Internal has no ported TypeId() and the heal never actually ran:
// the pointer is set at EngineFuncsInit before OnCreated's first call). UI::Element
// now has a ported TypeId(), so the lazy-cache is dead.

// HotKeyToolTipUI::Serialize — Slot4 (+0x20). self = the controller.
// [4] 0x14006a980.
void HotKeyToolTipUI::OnCreated() {
    // Resolve the controlled element (the button) and read its shortcut id.
    void* obj = *(void**)((uintptr_t)this + 0x28);
    void* elem = (obj && ::Object::IsDerivedFromNullSafe(obj, UI::Element::TypeId())) ? obj : nullptr;

    uint64_t elemIbp = 0;
    new (&elemIbp) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(elem));   // B.3.3 bridge
    if (!elemIbp) return;

    int shortcutId = *(int*)((uintptr_t)elemIbp + 0xC0);
    if (shortcutId < 0) return;  // no shortcut assigned -> leave default

    void* keyMap = Game::GetActualKeyMap();
    if (!keyMap) return;


    Game::FillHotKeyText(this, "textToolTip", 1, keyMap, shortcutId);
    Game::FillHotKeyText(this, "textHotKey",  0, keyMap, shortcutId);
}
