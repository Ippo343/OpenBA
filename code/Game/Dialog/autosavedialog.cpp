#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

// ============================================================
// AutoSaveDialog::Slot4 (0x14005E110) — vtable +0x20
// ============================================================
// 5-byte JMP thunk to 0x14005E120 (timer setup).
// Stops the current timer, reads game speed setting, picks an
// autosave interval multiplier, and starts a new timer.
//
// Game speed → interval multiplier table:
//   0 → 60.0f,  1 → 120.0f,  2 → 300.0f,  3 → 480.0f,
//   4 → 600.0f, 5 → 900.0f,  6 → 1800.0f, 7 → 3600.0f
// Final interval = multiplier * 3600.0f (seconds).
//
// Original: 0x14005E110 → 0x14005E120.

// Runtime dispatch slots for DialogInstance timer functions
static void* g_addr_StopTimer  = nullptr;  // 0x140151618
static void* g_addr_StartTimer = nullptr;  // 0x140151620
static void* g_addr_TimeCtor   = nullptr;  // 0x140152b00

void AutoSaveDialog_Slot4(AutoSaveDialog* self) {
    // Lazily resolve runtime dispatch slots
    if (!g_addr_StopTimer) {
        g_addr_StopTimer  = *(void**)(void*)ResolveAddr(Module::Exe, 0x140151618);
        g_addr_StartTimer = *(void**)(void*)ResolveAddr(Module::Exe, 0x140151620);
        g_addr_TimeCtor   = *(void**)(void*)ResolveAddr(Module::Exe, 0x140152b00);
    }
    if (!g_addr_StopTimer || !g_addr_StartTimer || !g_addr_TimeCtor)
        return;

    // StopTimer(self, 0)
    typedef void (*fn_StopTimer_t)(void*, int);
    ((fn_StopTimer_t)g_addr_StopTimer)(self, 0);

    // Read game speed setting: *(*g_pGameSettingsPtr + 1) i.e. int at offset +4
    if (!g_pGameSettingsPtr || !*g_pGameSettingsPtr)
        return;
    int speedSetting = *(*g_pGameSettingsPtr + 1);

    // Autosave interval table (seconds multiplied by 3600.0f)
    static const float kIntervalTable[8] = {
        60.0f, 120.0f, 300.0f, 480.0f, 600.0f, 900.0f, 1800.0f, 3600.0f
    };

    if (speedSetting < 0 || speedSetting > 7)
        return;

    float interval = kIntervalTable[speedSetting] * 3600.0f;

    // Time::Time(retbuf, float) -> returns Time*
    typedef void* (*fn_TimeCtor_t)(void*, float);
    uint8_t timeBuf[8];
    fn_TimeCtor_t timeCtor = (fn_TimeCtor_t)g_addr_TimeCtor;
    void* timeResult = timeCtor(timeBuf, interval);
    uint64_t timeVal = *(uint64_t*)timeResult;

    // StartTimer(self, 0, timeVal)
    typedef void (*fn_StartTimer_t)(void*, int, uint64_t);
    ((fn_StartTimer_t)g_addr_StartTimer)(self, 0, timeVal);
}

// ============================================================
// AutoSaveDialog::Slot8 (0x14005E0F0) — vtable +0x40, OnCommand
// ============================================================
// 12 bytes: CMP R8D,0x13; JZ target; RET
// If msg (param3) == 0x13 (timer event), triggers autosave via
// SaveLoad system. Otherwise returns immediately.
// The actual autosave logic at 0x14005DEC0 is complex (uses
// PausesGame, SaveLoad operations, KeyMap sprite/name), so we
// call through a resolved function pointer to the original.
//
// Original: 0x14005E0F0.

static void* g_addr_AutoSaveHandler = nullptr;  // 0x14005DEC0

void AutoSaveDialog_Slot8(AutoSaveDialog* self, void* param2, int param3, int param4) {
    if (param3 != 0x13)
        return;

    // Lazily resolve the autosave handler function
    if (!g_addr_AutoSaveHandler) {
        g_addr_AutoSaveHandler = (void*)ResolveAddr(Module::Exe, 0x14005DEC0);
    }
    if (!g_addr_AutoSaveHandler)
        return;

    // Forward to the autosave handler (passes all 4 params via Win64 ABI)
    typedef void (*fn_AutoSaveHandler_t)(void*, void*, int, int);
    ((fn_AutoSaveHandler_t)g_addr_AutoSaveHandler)(self, param2, param3, param4);
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
