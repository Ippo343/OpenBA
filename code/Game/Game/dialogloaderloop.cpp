// code/Game/Game/dialogloaderloop.cpp
// Shared reimplementation of Engine::DialogLoaderLoopT::Draw (Runtime 0x1800DAE20).
//
// Called from two sites:
//   1. engine/reimpl/dialogloaderloop.cpp Reimpl_Draw (DLL hook over Runtime symbol)
//   2. standalone/splash_loop_stubs.cpp Stub_Draw (vtable slot +0x20 in standalone exe)
//
// Original sequence:
//   1. device->Clear(7, 0, 1.0f, 0)   - clear back buffer + depth + stencil
//   2. write m_gameTime float4 @ ctx+0x3C8, dirty bit |= 0x800
//   3. write m_prevTime float4 @ ctx+0x3B8, m_viewTime float4 @ ctx+0x288
//   4. dirty bits |= 0x10800
//   5. UI::UserInterface::Draw(ui at self+0x60, context)
//
// Defense-in-depth: silently no-op on null mainLoop / ctx / device. The DLL hook
// never sees these as null in practice, but the standalone vtable runs through
// here from earliest splash frames so the guards keep both modes safe without
// emitting noise into DLL-mode logs.

#include "Game/dialogloaderloop.h"
#include "MainLoop.h"
#include "engine_funcs.h"
#include <cstdint>

void DialogLoaderLoopT_Draw_Impl(DialogLoaderLoopT* self) {
    MainLoop* mainLoop = reinterpret_cast<MainLoop*>(
        g_pMainLoopSingleton ? *g_pMainLoopSingleton : nullptr);
    if (!mainLoop) return;

    uint8_t* ctx = reinterpret_cast<uint8_t*>(mainLoop->m_context);
    if (!ctx) return;

    // 1. Clear screen: device->Clear(flags=7, color=0, depth=1.0f, stencil=0)
    void* device = *reinterpret_cast<void**>(ctx + 0x2160);
    if (!device) return;

    void** deviceVtable = *reinterpret_cast<void***>(device);
    typedef void (*DeviceClear_t)(void*, uint32_t, uint32_t, float, int);
    ((DeviceClear_t)deviceVtable[4])(device, 7, 0, 1.0f, 0);

    // 2. Compute elapsed time in seconds (matches original: double -> float)
    float totalSeconds = (float)((double)self->m_elapsedTicks * (*g_Timer_secondsPerTick));

    // 3. Set m_gameTime FIRST (context+0x3C8, float4) — matches original order
    *reinterpret_cast<float*>(ctx + 0x3C8) = totalSeconds;
    *reinterpret_cast<uint32_t*>(ctx + 0x3CC) = 0;
    *reinterpret_cast<uint32_t*>(ctx + 0x3D0) = 0;
    *reinterpret_cast<uint32_t*>(ctx + 0x3D4) = 0;

    // 4. Intermediate dirty flag: gameTime updated
    *reinterpret_cast<uint32_t*>(ctx + 0x10) |= 0x800;

    // 5. Recompute time as double for prevTime/viewTime (matches original's dVar2 path)
    double dVar2 = (double)self->m_elapsedTicks * (*g_Timer_secondsPerTick);

    // 6. Set m_prevTime (context+0x3B8, float4)
    *reinterpret_cast<float*>(ctx + 0x3B8) = (float)dVar2;
    *reinterpret_cast<uint32_t*>(ctx + 0x3BC) = 0;
    *reinterpret_cast<uint32_t*>(ctx + 0x3C0) = 0;
    *reinterpret_cast<uint32_t*>(ctx + 0x3C4) = 0;

    // 7. Set m_viewTime (context+0x288, float4)
    *reinterpret_cast<float*>(ctx + 0x288) = (float)dVar2;
    *reinterpret_cast<uint32_t*>(ctx + 0x28C) = 0;
    *reinterpret_cast<uint32_t*>(ctx + 0x290) = 0;
    *reinterpret_cast<uint32_t*>(ctx + 0x294) = 0;

    // 8. Final dirty flags: time + view updated
    *reinterpret_cast<uint32_t*>(ctx + 0x10) |= 0x10800;

    // 9. Draw the UI overlay (UserInterface at self+0x60)
    fn_UI_Draw(reinterpret_cast<uint8_t*>(self) + 0x60, mainLoop->m_context);
}
