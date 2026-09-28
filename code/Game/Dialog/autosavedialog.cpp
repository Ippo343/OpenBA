#include "ui/ImageElement.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/Element.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/DialogInstance.h"   // ENDGAME B.3.6 UI callers (K2)
#include "object/Internal.h"   // ENDGAME B.3.6 UI callers (K2)
#include <new>   // ENDGAME B.3.6 UI callers (K2)
#include "save/SaveLoad.h"   // ENDGAME B.3.7 callers (K1)
#include "Dialog/AutoSaveDialog.h"
#include "Dialog/ui_dialogs.h"
#include "Dialog/dialog_exe_helpers.h"
#include "Object/factory_registration.h"
#include <cstddef>
#include "engine_funcs.h"
#include "Game/exe_bodies.h"   // B3R STORAGE: g_gameSettings
#include "system/String.h"
#include "system/Value.h"   // ENDGAME B.3.4b System callers: System::Time
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "Dialog/exe_bodies.h"

// ============================================================
// [4] AutoSaveDialog::OnOpened (0x14005E110) — vtable +0x20
// ============================================================
// 5-byte JMP thunk to 0x14005E120 (timer setup).
// Stops the current timer, reads game speed setting, picks an
// autosave interval multiplier, and starts a new timer.
//

//   0 → 60.0f,  1 → 120.0f,  2 → 300.0f,  3 → 480.0f,
//   4 → 600.0f, 5 → 900.0f,  6 → 1800.0f, 7 → 3600.0f
// Final interval = multiplier * 3600.0f (seconds).
//
// Original: 0x14005E110 → 0x14005E120.

// UI::DialogInstance::StopTimer / StartTimer live in the Runtime DLL
// (0x180113730 / 0x1801137c0). The exe reaches them through the .rdata dispatch
// slots 0x140151618 / 0x140151620, and NEITHER of those is registered in
// standalone - so the previous `*(void**)ResolveAddr(...)` here dereferenced NULL
// and this function crashed the moment it ran. Resolve them from the Runtime DLL
// instead, preferring the export name and falling back to the verified entry
// address. Fixed 2026-08-30 (LANE1 rows 13/14).
// ENDGAME B.3.6 (K2): both are now direct UI::DialogInstance member calls; the
// two by-name caches went with their last callers.
// Time::Time(float) is the import slot 0x140152b00: RtSystem_Time_Ctor, called by
// name (B3R).

void AutoSaveDialog::OnOpened() {
    StopTimer(0);

    // Autosave-interval setting: gs[1] = the Options 'Autosave' combo index (0..7).
    if (g_gameSettings == nullptr)
        return;
    int speedSetting = *(g_gameSettings + 1);

    // Autosave interval table, in MINUTES, read off retail's .rdata (2026-08-30).
    // The switch at 0x14005e130 picks one of these eight floats and multiplies by
    // DAT_1401721f0; Time::Time() then treats the product as SECONDS:
    //   [0] g_fUVScale             0x14017214c = 1.0
    //   [1] g_pAgeMultiplier       0x140172198 = 5.0
    //   [2] DAT_1401721b0                      = 10.0
    //   [3] DAT_1401721bc                      = 15.0
    //   [4] DAT_1401721c4                      = 20.0
    //   [5] g_pNotifyTimerDuration 0x1401721cc = 30.0
    //   [6] DAT_1401721e0                      = 45.0
    //   [7] DAT_1401721f0                      = 60.0   <- also the multiplier
    // This table previously held {60,120,300,480,600,900,1800,3600} and multiplied by
    // 3600.0f - wrong twice over: four of the eight entries did not match retail, and
    // the multiplier is 60 (minutes->seconds), not 3600. At the default "5 minutes"
    // setting it asked for 120*3600 s = 5 DAYS of game time, so the timer could never
    // fire even once the dialog exists. See LANE1 rows 13/14.
    static const float kIntervalMinutes[8] = {
        1.0f, 5.0f, 10.0f, 15.0f, 20.0f, 30.0f, 45.0f, 60.0f
    };
    static const float kMinutesToSeconds = 60.0f;   // DAT_1401721f0

    // Retail's switch has no default arm: an out-of-range setting leaves the timer
    // stopped rather than restarting it.
    if (speedSetting < 0 || speedSetting > 7)
        return;

    float interval = kIntervalMinutes[speedSetting] * kMinutesToSeconds;

    uint8_t timeBuf[8];
    void* timeResult = RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(timeBuf), interval);
    uint64_t timeVal = *(uint64_t*)timeResult;

    StartTimer(0, static_cast<int64_t>(timeVal));

}

// ============================================================
// [8] AutoSaveDialog::OnElementEvent (0x14005E0F0) — vtable +0x40, OnCommand
// ============================================================
// 12 bytes: CMP R8D,0x13; JZ target; RET
// If msg (param3) == 0x13 (timer event), triggers autosave via
// SaveLoad system. Otherwise returns immediately.
// The autosave body at 0x14005DEC0 is ported as Dialog_AutoSaveHandler
// (dialog_exe_helpers.cpp); call that directly rather than the unregistered
// raw address.
//
// Original: 0x14005E0F0.

void AutoSaveDialog::OnElementEvent(Object::InternalPtr<UI::Element>* param2,
                                    int32_t param3, int32_t param4, int32_t /*c*/,
                                    int32_t /*d*/, int32_t /*e*/, void* /*userData*/) {
    if (param3 != 0x13)
        return;

    // 0x14005DEC0 is the autosave body. It is NOT registered, so resolving the raw
    // address returned null here and the guard above it swallowed every timer tick -
    // autosave was dead even where the dialog existed. The function has been ported
    // all along as Dialog_AutoSaveHandler (dialog_exe_helpers.cpp); it was simply
    // never wired to its only caller. Fixed 2026-08-30 (LANE1 rows 13/14).
    Dialog_AutoSaveHandler(this, param2, param3, param4);
}

// [6] AutoSaveDialog::OnUpdateLayout (0x14005DFC0) — Draw
// Checks SaveLoad operation type. If operation==4 (load), shows load indicator
// and hides save indicator; otherwise shows save and hides load.
// Applies rotation to the visible indicator.
void AutoSaveDialog::OnUpdateLayout() {
    uintptr_t s = (uintptr_t)this;

    if (!::Save::SaveLoad::_singleton)
        return;

    void* saveLoad = ::Save::SaveLoad::_singleton;
    int operation = reinterpret_cast<const Save::SaveLoad*>(saveLoad)->GetOperation();  // B.3.7 bridge

    // operation==4 means "load"; otherwise it's a save
    uint32_t hideFlags = (uint32_t)(operation != 4);
    uint32_t showFlags = (uint32_t)(operation == 4);

    void* elem1 = *(void**)(s + 0xB8);
    if (elem1) {
        reinterpret_cast<UI::Element*>(elem1)->SetFlagsOnOff(hideFlags, showFlags);  // B.3.6 bridge
    }

    void* elem2 = *(void**)(s + 0xC0);
    if (elem2) {
        reinterpret_cast<UI::Element*>(elem2)->SetFlagsOnOff(hideFlags, showFlags);  // B.3.6 bridge
    }

    // Apply rotation to first indicator element
    float angle = *(float*)(s + 0xC8);
    if (elem1) {
        reinterpret_cast<UI::ImageElement*>(elem1)->SetRotation(angle);  // B.3.6 bridge
    }
}

// [7] AutoSaveDialog::OnUpdate (0x14005E090) — Update
// Accumulates rotation angle from elapsed time at 90 degrees/sec.
// Wraps angle to stay within [0, 360).
void AutoSaveDialog::OnUpdate(const System::Time& time) {
    uintptr_t s = (uintptr_t)this;
    float elapsed = time.AsSeconds();
    float angle = *(float*)(s + 0xC8);

    angle += elapsed * 90.0f;

    if (angle >= 360.0f) {
        do {
            angle -= 360.0f;
        } while (angle >= 360.0f);
    }

    *(float*)(s + 0xC8) = angle;
}

// ============================================================
// Construction, destruction, reflection
// ============================================================
// Retail AutoSaveDialog_CopyCtor (0x14005e230): DialogInstance base ctor, then
// resolve the two spinner elements by name and zero the angle.
// AutoSaveDialog owns no heap of its own: +0xB8/+0xC0 are borrowed element refs and
// +0xC8 is the spinner angle, so the dtor is just the DialogInstance base dtor.

AutoSaveDialog::AutoSaveDialog() : dataB8(0), dataC0(0) {}

AutoSaveDialog::AutoSaveDialog(UI::UserInterface* ui, const UI::Dialog* dialog,
                               Object::InternalPtr<UI::DialogInstance> parent)
    : UI::DialogInstance(ui, dialog, parent, nullptr, nullptr, 0), dataB8(0), dataC0(0) {
    {
        System::String nameStr("savingImage");
        dataB8 = reinterpret_cast<uint64_t>(FindElement(nameStr).Get());
    }
    {
        System::String nameStr("animatedImage");
        dataC0 = reinterpret_cast<uint64_t>(FindElement(nameStr).Get());
    }
    angle = 0.0f;   // spinner angle
}

// create 0x1400106c0: MemAlloc(0xd0, 8), then the ctor.
void* AutoSaveDialog::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(AutoSaveDialog), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) AutoSaveDialog());
}
// createInPlace 0x140010730 (the tree's in-place build is kept, B8-13).
void* AutoSaveDialog::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(::new (storage) AutoSaveDialog());
}

DEFINE_EXE_TYPE(AutoSaveDialog, "UI::DialogInstance")

// The ctor DOOR, reached by address: reg_code_lane2.cpp registers it at
// 0x14005e230 and GameUpdateLoop::Startup calls it.
extern "C" void* AutoSaveDialog_Ctor_Standalone(void* self, void* ui, void* extRes, void* parentIBP) {
    if (!self) return nullptr;
    Object::InternalPtr<UI::DialogInstance> parent;
    if (parentIBP)
        parent = *static_cast<const Object::InternalPtr<UI::DialogInstance>*>(parentIBP);
    return ::new (self) AutoSaveDialog(static_cast<UI::UserInterface*>(ui),
                                       static_cast<const UI::Dialog*>(extRes), parent);
}

// The slot-4 DOOR, reached by address: reg_autosave.cpp registers it at
// 0x14005e120 / 0x14005e110 and OptionsDialog's apply path calls it through
// fn_UpdateAutoSaveTimer (engine_funcs.cpp). A QUALIFIED member call.
void AutoSaveDialog_Slot4(AutoSaveDialog* self) {
    self->AutoSaveDialog::OnOpened();
}
