#include "Dialog/ui_dialogs.h"
#include "Dialog/dialog_exe_helpers.h"
#include "Object/factory_registration.h"
#include <cstddef>
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// ============================================================
// AutoSaveDialog::Slot4 (0x14005E110) — vtable +0x20
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
typedef void (*fn_StopTimer_t)(void* self, int id);
typedef void (*fn_StartTimer_t)(void* self, int id, uint64_t timeVal);
typedef void* (*fn_TimeCtor_t)(void* retbuf, float seconds);

static fn_StopTimer_t  s_StopTimer  = nullptr;
static fn_StartTimer_t s_StartTimer = nullptr;
static fn_TimeCtor_t   s_TimeCtor   = nullptr;
static bool            s_timerInit  = false;

static void EnsureTimerFns() {
    if (s_timerInit) return;
    s_timerInit = true;
    s_StopTimer = reinterpret_cast<fn_StopTimer_t>(
        ResolveExport("?StopTimer@DialogInstance@UI@@QEAAXH@Z"));
    if (!s_StopTimer)
        s_StopTimer = reinterpret_cast<fn_StopTimer_t>(ResolveAddr(Module::Runtime, 0x180113730));
    s_StartTimer = reinterpret_cast<fn_StartTimer_t>(
        ResolveExport("?StartTimer@DialogInstance@UI@@QEAAXHVTime@System@@@Z"));
    if (!s_StartTimer)
        s_StartTimer = reinterpret_cast<fn_StartTimer_t>(ResolveAddr(Module::Runtime, 0x1801137c0));
    // Time::Time(float) - dispatch slot 0x140152b00 IS registered; guard anyway.
    uintptr_t slot = ResolveAddr(Module::Exe, 0x140152b00);
    if (slot) s_TimeCtor = reinterpret_cast<fn_TimeCtor_t>(*reinterpret_cast<void**>(slot));
    if (!s_TimeCtor) s_TimeCtor = reinterpret_cast<fn_TimeCtor_t>(fn_Time_Ctor_Float);
}

void AutoSaveDialog_Slot4(AutoSaveDialog* self) {
    EnsureTimerFns();
    if (!s_StopTimer || !s_StartTimer || !s_TimeCtor)
        return;

    s_StopTimer(self, 0);

    // Autosave-interval setting: gs[1] = the Options 'Autosave' combo index (0..7).
    if (!g_pGameSettingsPtr || !*g_pGameSettingsPtr)
        return;
    int speedSetting = *(*g_pGameSettingsPtr + 1);

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
    void* timeResult = s_TimeCtor(timeBuf, interval);
    uint64_t timeVal = *(uint64_t*)timeResult;

    s_StartTimer(self, 0, timeVal);

    // BANISHED_AUTOSAVE_TRACE=1: one line every time the timer is (re-)armed. This is
    // how an Options interval change is observed - OptionsDialog::ApplySettings calls
    // back into here after writing gs[1], so a second line with a new interval is the
    // proof that the restart loop found this dialog.
    if (std::getenv("BANISHED_AUTOSAVE_TRACE")) {
        fprintf(stderr, "[AutoSave] timer armed: settingIdx=%d -> %.0f min (%.0f s)\n",
                speedSetting, kIntervalMinutes[speedSetting], interval);
        fflush(stderr);
    }
}

// ============================================================
// AutoSaveDialog::Slot8 (0x14005E0F0) — vtable +0x40, OnCommand
// ============================================================
// 12 bytes: CMP R8D,0x13; JZ target; RET
// If msg (param3) == 0x13 (timer event), triggers autosave via
// SaveLoad system. Otherwise returns immediately.
// The autosave body at 0x14005DEC0 is ported as Dialog_AutoSaveHandler
// (dialog_exe_helpers.cpp); call that directly rather than the unregistered
// raw address.
//
// Original: 0x14005E0F0.

void AutoSaveDialog_Slot8(AutoSaveDialog* self, void* param2, int param3, int param4) {
    // Report EVERY event the dialog receives, so "the timer never fired" can be told
    // apart from "the timer fired and the handler bailed".
    if (std::getenv("BANISHED_AUTOSAVE_TRACE")) {
        fprintf(stderr, "[AutoSave] Slot8 event msg=0x%x data=%d%s\n",
                param3, param4, param3 == 0x13 ? "  <== TIMER" : "");
        fflush(stderr);
    }
    if (param3 != 0x13)
        return;

    // 0x14005DEC0 is the autosave body. It is NOT registered, so resolving the raw
    // address returned null here and the guard above it swallowed every timer tick -
    // autosave was dead even where the dialog existed. The function has been ported
    // all along as Dialog_AutoSaveHandler (dialog_exe_helpers.cpp); it was simply
    // never wired to its only caller. Fixed 2026-08-30 (LANE1 rows 13/14).
    Dialog_AutoSaveHandler(self, param2, param3, param4);
}

// AutoSaveDialog::Slot6 (0x14005DFC0) — Draw
// Checks SaveLoad operation type. If operation==4 (load), shows load indicator
// and hides save indicator; otherwise shows save and hides load.
// Applies rotation to the visible indicator.
void AutoSaveDialog_Slot6(AutoSaveDialog* self) {
    uintptr_t s = (uintptr_t)self;

    if (!g_pSaveLoadSingleton || !*g_pSaveLoadSingleton)
        return;

    void* saveLoad = *g_pSaveLoadSingleton;
    int operation = fn_SaveLoad_GetOperation(saveLoad);

    // operation==4 means "load"; otherwise it's a save
    uint32_t hideFlags = (uint32_t)(operation != 4);
    uint32_t showFlags = (uint32_t)(operation == 4);

    void* elem1 = *(void**)(s + 0xB8);
    if (elem1) {
        fn_Element_SetFlagsOnOff(elem1, hideFlags, showFlags);
    }

    void* elem2 = *(void**)(s + 0xC0);
    if (elem2) {
        fn_Element_SetFlagsOnOff(elem2, hideFlags, showFlags);
    }

    // Apply rotation to first indicator element
    float angle = *(float*)(s + 0xC8);
    if (elem1) {
        fn_ImageElement_SetRotation(elem1, angle);
    }
}

// AutoSaveDialog::Slot7 (0x14005E090) — Update
// Accumulates rotation angle from elapsed time at 90 degrees/sec.
// Wraps angle to stay within [0, 360).
void AutoSaveDialog_Slot7(AutoSaveDialog* self, void* time) {
    uintptr_t s = (uintptr_t)self;
    float elapsed = fn_Time_AsSeconds(time);
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
// Standalone AutoSaveDialog: vtable + ctor (autosave_sa)
// ============================================================
// Retail's ctor (0x14005e230) stamps &vtable_AutoSaveDialog (exe 0x14015b120), which
// has no standalone counterpart - that is what made naive registration of the ctor
// crash. Follow the eventnotify_sa pattern instead: hand-build a 10-entry vtable of
// our own wrappers and install that.
//
// Retail vtable 0x14015b120, entry by entry:
//   [0] 0x140010780 scalar deleting dtor   [5] 0x14003a94e stub
//   [1] 0x1400106b0                        [6] 0x14005dfc0 Draw    -> Slot6
//   [2] 0x1400106a0 GetTypeId              [7] 0x14005e090 Update  -> Slot7
//   [3] 0x14003a948 stub                   [8] 0x14005e0f0 OnEvent -> Slot8
//   [4] 0x14005e110 thunk -> e120          [9] 0x14003a954 stub
//       (restart autosave timer) -> Slot4
//
// AutoSaveDialog owns no heap of its own: +0xB8/+0xC0 are borrowed element refs and
// +0xC8 is the spinner angle, so the dtor is just the DialogInstance base dtor.
namespace autosave_sa {

static void* g_vt[10];
void* GetVtable() { return reinterpret_cast<void*>(g_vt); }

static void  StubNop(void*, ...) {}
static void* Slot1Getter(void*) { return nullptr; }
static uint32_t GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("AutoSaveDialog");
    return c;
}

static void* Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    *reinterpret_cast<void**>(self) = GetVtable();
    if (fn_DialogInstance_Dtor) fn_DialogInstance_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

static void W_Startup(void* self) { if (self) AutoSaveDialog_Slot4(reinterpret_cast<AutoSaveDialog*>(self)); }
static void W_Draw(void* self)    { if (self) AutoSaveDialog_Slot6(reinterpret_cast<AutoSaveDialog*>(self)); }
static void W_Update(void* self, void* t) { if (self) AutoSaveDialog_Slot7(reinterpret_cast<AutoSaveDialog*>(self), t); }
static void W_OnEvent(void* self, void* e, int msg, int id) {
    if (self) AutoSaveDialog_Slot8(reinterpret_cast<AutoSaveDialog*>(self), e, msg, id);
}

struct Init {
    Init() {
        g_vt[0] = reinterpret_cast<void*>(&Dtor);
        g_vt[1] = reinterpret_cast<void*>(&Slot1Getter);
        g_vt[2] = reinterpret_cast<void*>(&GetTypeId);
        g_vt[3] = reinterpret_cast<void*>(&StubNop);
        g_vt[4] = reinterpret_cast<void*>(&W_Startup);
        g_vt[5] = reinterpret_cast<void*>(&StubNop);
        g_vt[6] = reinterpret_cast<void*>(&W_Draw);
        g_vt[7] = reinterpret_cast<void*>(&W_Update);
        g_vt[8] = reinterpret_cast<void*>(&W_OnEvent);
        g_vt[9] = reinterpret_cast<void*>(&StubNop);
    }
} g_init;

// Mirrors AutoSaveDialog_CopyCtor (0x14005e230): DialogInstance base ctor, install the
// vtable, then resolve the two spinner elements by name and zero the angle.
void* Ctor(void* self, void* ui, void* extRes, void* parentIBP) {
    if (!self) return nullptr;
    uint64_t parentCopy = parentIBP ? *reinterpret_cast<uint64_t*>(parentIBP) : 0;
    if (fn_DialogInstance_Ctor && extRes)
        fn_DialogInstance_Ctor(self, ui, extRes, &parentCopy, nullptr, nullptr, 0);
    uintptr_t s = (uintptr_t)self;
    *reinterpret_cast<void**>(self) = GetVtable();
    *(uint64_t*)(s + 0xB8) = 0;
    *(uint64_t*)(s + 0xC0) = 0;

    if (fn_DialogInstance_FindElement) {
        uint8_t nameBuf[8];
        uint64_t found = 0;
        fn_String_Ctor_CStr(nameBuf, "savingImage");
        fn_DialogInstance_FindElement(self, &found, nameBuf);
        *(uint64_t*)(s + 0xB8) = found;
        fn_String_Dtor(nameBuf);

        found = 0;
        fn_String_Ctor_CStr(nameBuf, "animatedImage");
        fn_DialogInstance_FindElement(self, &found, nameBuf);
        *(uint64_t*)(s + 0xC0) = found;
        fn_String_Dtor(nameBuf);
    }
    *(uint32_t*)(s + 0xC8) = 0;   // spinner angle
    return self;
}

static void Zero(void* p) { for (size_t o = 0; o < (size_t)0xD0; o += 8) *reinterpret_cast<uint64_t*>((char*)p + o) = 0; }
void* Factory() {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)0xD0, 8);
    if (!p) return nullptr;
    Zero(p); *reinterpret_cast<void**>(p) = g_vt; return p;
}
void* InPlace(void* m) { if (!m) return nullptr; Zero(m); *reinterpret_cast<void**>(m) = g_vt; return m; }

}  // namespace autosave_sa

REGISTER_TYPE(AutoSaveDialog, "UI::DialogInstance",
              &autosave_sa::Factory, &autosave_sa::InPlace);

extern "C" void* AutoSaveDialog_Ctor_Standalone(void* self, void* ui, void* extRes, void* parentIBP) {
    return autosave_sa::Ctor(self, ui, extRes, parentIBP);
}
