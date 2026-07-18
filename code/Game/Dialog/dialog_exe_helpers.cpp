// code/Game/Dialog/dialog_exe_helpers.cpp
// Clean reimplementations of two exe-internal helpers referenced by the Dialog
// module, ported to close the standalone Dialog code-gap.
//
//   0x14000d6c0  WorkplaceUI FindComponent (type 0x1401c7008) — guard check.
//   0x14005dec0  AutoSaveDialog autosave handler (timer-event 0x13).
//   0x140099d40  autosave-name builder ("Autosave" UIString) — sub-callee of 0x14005dec0.
//
// Portable (no hooks.h) — depends only on engine_funcs.h + resolve.h.
// Decompiled from Application-x64.exe via Ghidra.

#include "dialog_exe_helpers.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

namespace Object { uint32_t GetTypeIdByName(const char* name); }  // factory_registration.h

// ============================================================
// WorkplaceUI FindComponent (FUN_14000d6c0, type 0x1401c7008)
// ============================================================
// Member of the byte-for-byte-identical FindComponent family (cf.
// code/Game/Work/work_base_reimpl.cpp). Scans the entity's component list
// (ptr array at entity+0x30, count at entity+0x38), calls each component's
// GetTypeId (vtable +0x10), and on the first type match runs an IsDerivedFrom
// check before returning the component through an out InternalBasePtr.
// Caller signature (workplaceui.cpp): void* fn(void* entity, void* retIBP).
static const uint32_t* s_pTid_d6c0 = nullptr;  // 0x1401c7008
static bool s_tidInit = false;

void* Dialog_FindComponent_d6c0(void* entity, void* retIBP) {
    if (!s_tidInit) {
        s_pTid_d6c0 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7008));
        // 0x1401c7008 = BuildComponent TypeInfo (decomp/application.md); NULL in
        // standalone -> fall back to the runtime type registry so the gather Execute's
        // FindComp_Unk actually locates the build component (else null deref at +0x18).
        static uint32_t v_d6c0;
        if (!s_pTid_d6c0) { v_d6c0 = ::Object::GetTypeIdByName("BuildComponent"); s_pTid_d6c0 = &v_d6c0; }
        s_tidInit = true;
    }
    void* found = nullptr;
    if (s_pTid_d6c0) {
        uintptr_t e = reinterpret_cast<uintptr_t>(entity);
        int n = *reinterpret_cast<int*>(e + 0x38);
        uintptr_t list = *reinterpret_cast<uintptr_t*>(e + 0x30);
        for (int i = 0; i < n; i++) {
            void** slot = reinterpret_cast<void**>(list + static_cast<int64_t>(i) * 8);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
            void* comp = *slot;
            uint32_t t = (*reinterpret_cast<uint32_t(**)(void*)>(*reinterpret_cast<uintptr_t*>(comp) + 0x10))(comp);
            if (t == *s_pTid_d6c0) {
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
                void* c = *slot;
                found = (fn_IsDerivedFrom && fn_IsDerivedFrom(c, *s_pTid_d6c0)) ? c : nullptr;
                break;
            }
        }
    }
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(retIBP, found);
    return retIBP;
}

// ============================================================
// Autosave-name builder (FUN_140099d40)
// ============================================================
// Builds a UI::String "Autosave" into the out buffer from the KeyMap's
// string-table external (keymap+0xd8). Sub-callee of the autosave handler.
//   String local("Autosave"); UIString::Ctor(retbuf, *(keymap+0xd8), local); ~local;
static void* Dialog_AutoSaveBuildName(void* keymap, void* retUIString) {
    uint8_t nameStr[8];
    if (fn_String_Ctor_CStr) fn_String_Ctor_CStr(nameStr, "Autosave");
    if (fn_UIString_Ctor)
        fn_UIString_Ctor(retUIString,
                         *reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(keymap) + 0xd8),
                         nameStr);
    if (fn_String_Dtor) fn_String_Dtor(nameStr);
    return retUIString;
}

// ============================================================
// AutoSaveDialog autosave handler (FUN_14005dec0)
// ============================================================
// Triggered by AutoSaveDialog::Slot8 on timer event 0x13. If the game is not
// paused and not in the bit-10 "blocked" state, and no save/load operation is
// already in progress, kicks off an autosave: 1s min-display time, the KeyMap
// screenshot sprite, the localized "Autosave" name, then SetOperation(4,-4).
//
// param_1 = AutoSaveDialog* (self; UserInterface at +0x10). p2/p3/p4 forwarded.
//
// UI::UserInterface::PausesGame is resolved locally (no engine_func); the rest
// (incl. fn_SaveLoad_SetSaveName) are existing engine_funcs (ResolveExport).
typedef bool (*fn_UI_PausesGame_t)(void* ui);
static fn_UI_PausesGame_t s_fn_PausesGame = nullptr;
static bool s_asInit = false;

void Dialog_AutoSaveHandler(void* self, void* /*p2*/, int /*p3*/, int /*p4*/) {
    if (!s_asInit) {
        s_fn_PausesGame = reinterpret_cast<fn_UI_PausesGame_t>(
            ResolveExport("?PausesGame@UserInterface@UI@@QEBA_NXZ"));
        s_asInit = true;
    }

    // Not while the UI pauses the game.
    void* ui = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(self) + 0x10);
    if (s_fn_PausesGame && s_fn_PausesGame(ui)) return;

    // Not while GameWorld flag bit 10 is set.
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    uint32_t gwFlags = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xd0);
    if ((gwFlags >> 10) & 1) return;

    // Only when no operation is already in progress (Operation::None == 0).
    if (!g_pSaveLoadSingleton || !*g_pSaveLoadSingleton) return;
    void* saveLoad = *g_pSaveLoadSingleton;
    if (!fn_SaveLoad_GetOperation || fn_SaveLoad_GetOperation(saveLoad) != 0) return;

    // Min operation-display time = 1.0s.
    uint8_t timeBuf[8];
    if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(timeBuf, 1.0f);
    if (fn_SaveLoad_SetMinOperationTime) fn_SaveLoad_SetMinOperationTime(saveLoad, timeBuf);

    // Screenshot sprite (16 bytes) copied from KeyMap+0xb8.
    if (!g_pKeyMapSingleton || !*g_pKeyMapSingleton) return;
    uintptr_t keymap = reinterpret_cast<uintptr_t>(*g_pKeyMapSingleton);
    uint8_t spriteBuf[16];
    *reinterpret_cast<uint64_t*>(spriteBuf)     = *reinterpret_cast<uint64_t*>(keymap + 0xb8);
    *reinterpret_cast<uint64_t*>(spriteBuf + 8) = *reinterpret_cast<uint64_t*>(keymap + 0xc0);
    if (fn_SaveLoad_SetSaveSprite) fn_SaveLoad_SetSaveSprite(saveLoad, spriteBuf);

    // Localized "Autosave" name → System::String → SetSaveName(slot 2).
    uint8_t uiStrBuf[16];
    Dialog_AutoSaveBuildName(reinterpret_cast<void*>(keymap), uiStrBuf);
    uint8_t sysStrBuf[8];
    if (fn_UIString_ToSystemString) fn_UIString_ToSystemString(uiStrBuf, sysStrBuf);
    if (fn_SaveLoad_SetSaveName) fn_SaveLoad_SetSaveName(saveLoad, sysStrBuf, 2);
    if (fn_String_Dtor) fn_String_Dtor(sysStrBuf);

    // Kick off the save operation (Operation 4, param -4).
    if (fn_SaveLoad_SetOperation) fn_SaveLoad_SetOperation(saveLoad, 4, -4);
}
