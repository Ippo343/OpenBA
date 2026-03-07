// engine/reimpl/loaderloop.cpp
// Reimplementation of Engine::LoaderLoop methods from Runtime-x64.dll.
//
// LoaderLoop is the loading screen loop. It wraps another UpdateLoop (m_nextLoop)
// and uses the Engine::Loader background thread to load resources. While loading,
// it draws a black screen and sleeps 14ms per frame to avoid spinning the CPU.
// When loading completes and the minimum display delay has elapsed, it transitions
// to m_nextLoop via MainLoop::SetNextLoop.
//
// Methods reimplemented:
//   - LoaderLoop::Startup   (0x1800DC260) — kick off background loading
//   - LoaderLoop::Shutdown  (0x1800DC280) — wait for loader thread completion
//   - LoaderLoop::Update    (0x1800DC2A0) — sleep, check completion, accumulate time
//   - LoaderLoop::Draw      (0x1800DC310) — clear screen to black
//   - LoaderLoop::Exit      (0x1800DC130) — force immediate transition
//   - LoaderLoop::IsFinished(0x1800DC150) — check if loading done + delay elapsed
//   - LoaderLoop::OnVideoError (0x1800DC360) — wait for loader completion

#include "LoaderLoop.h"
#include "MainLoop.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <windows.h>
#include <cstdio>

// ============================================================
// Address constants
// ============================================================
namespace LoaderLoopAddr {
    constexpr uintptr_t Startup      = 0x1800DC260;
    constexpr uintptr_t Shutdown     = 0x1800DC280;
    constexpr uintptr_t Update       = 0x1800DC2A0;
    constexpr uintptr_t Draw         = 0x1800DC310;
    constexpr uintptr_t Exit         = 0x1800DC130;
    constexpr uintptr_t IsFinished   = 0x1800DC150;
    constexpr uintptr_t OnVideoError = 0x1800DC360;
}

// ============================================================
// Resolved globals (set at install time)
// ============================================================

// Uses g_pMainLoopSingleton from engine_funcs.h

// ============================================================
// Hook state
// ============================================================

static HookInfo g_hookStartup;
static HookInfo g_hookShutdown;
static HookInfo g_hookUpdate;
static HookInfo g_hookDraw;
static HookInfo g_hookExit;
static HookInfo g_hookIsFinished;
static HookInfo g_hookOnVideoError;

// ============================================================
// LoaderLoop::Startup (0x1800DC260)
// Kicks off background resource loading via the global Loader.
// Calls Loader::Load(_loader, m_nextLoop).
// ============================================================

typedef void (*LoaderLoop_Startup_t)(LoaderLoop* self);

static void Reimpl_Startup(LoaderLoop* self) {
    g_lastHook = "LoaderLoop::Startup";
#ifdef _DEBUG
    printf("[LoaderLoop::Startup] self=%p nextLoop=%p\n", (void*)self, (void*)self->m_nextLoop);
#endif
    fn_Loader_Load(g_pLoader, self->m_nextLoop);
}

// ============================================================
// LoaderLoop::Shutdown (0x1800DC280)
// Blocks until the background loader thread signals completion.
// ============================================================

typedef void (*LoaderLoop_Shutdown_t)(LoaderLoop* self);

static void Reimpl_Shutdown(LoaderLoop* self) {
    g_lastHook = "LoaderLoop::Shutdown";
#ifdef _DEBUG
    printf("[LoaderLoop::Shutdown] self=%p, waiting for loader completion...\n", (void*)self);
#endif
    WaitForSingleObject(*g_pLoaderCompletionEvent, INFINITE);
#ifdef _DEBUG
    printf("[LoaderLoop::Shutdown] loader completed\n");
#endif
}

// ============================================================
// LoaderLoop::Update (0x1800DC2A0)
// Per-frame tick during loading:
//   1. Sleep 14ms (loading screen frame rate cap)
//   2. Check if loading is done AND delay has elapsed
//   3. If so, set MainLoop->m_nextLoop to trigger transition
//   4. Accumulate elapsed time
// Always returns true (loading screen never causes quit).
// ============================================================

typedef bool (*LoaderLoop_Update_t)(LoaderLoop* self, Time* dt);

static bool Reimpl_Update(LoaderLoop* self, Time* dt) {
    g_lastHook = "LoaderLoop::Update";
    // Sleep ~14ms per frame to avoid spinning CPU during loading
    fn_App_SleepThread(*g_pAppSingleton, 14);

    // Check transition conditions:
    //   1. m_finished != 0 (always true for normal LoaderLoop, initially set to 1)
    //   2. Loader thread not currently running (flags bit 0 == 0)
    //   3. Minimum display delay has elapsed
    if (self->m_finished != 0 &&
        (*g_pLoaderFlags & 1) == 0 &&
        self->m_delayTicks <= self->m_elapsedTicks)
    {
#ifdef _DEBUG
        printf("[LoaderLoop::Update] Loading complete, transitioning to nextLoop=%p\n",
               (void*)self->m_nextLoop);
#endif
        MainLoop* mainLoop = reinterpret_cast<MainLoop*>(*g_pMainLoopSingleton);
        mainLoop->m_nextLoop = self->m_nextLoop;
    }

    // Accumulate elapsed time
    self->m_elapsedTicks += dt->ticks;

    return true;
}

// ============================================================
// LoaderLoop::Draw (0x1800DC310)
// Clears the screen to black during loading.
// Uses Video::Device::Clear(flags=7, color=0, depth=1.0f, stencil=0)
// through the device data vtable at context+0x2160.
// ============================================================

typedef void (*LoaderLoop_Draw_t)(LoaderLoop* self, float alpha);

static void Reimpl_Draw(LoaderLoop* /*self*/, float /*alpha*/) {
    g_lastHook = "LoaderLoop::Draw";
    MainLoop* mainLoop = reinterpret_cast<MainLoop*>(*g_pMainLoopSingleton);
    uint8_t* ctx = reinterpret_cast<uint8_t*>(mainLoop->m_context);

    // Get device data: context+0x2160 -> device ptr -> vtable
    void* device = *reinterpret_cast<void**>(ctx + 0x2160);
    void** deviceVtable = *reinterpret_cast<void***>(device);

    // Clear(flags=7, color=0, depth=1.0f, stencil=0)
    // flags 7 = D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL
    // vtable[4] = Clear at offset +0x20
    typedef void (*DeviceClear_t)(void* self, uint32_t flags, uint32_t color, float depth, int stencil);
    ((DeviceClear_t)deviceVtable[4])(device, 7, 0, 1.0f, 0);
}

// ============================================================
// LoaderLoop::Exit (0x1800DC130)
// Non-virtual method: forces immediate transition to m_nextLoop
// by setting MainLoop->m_nextLoop, bypassing the delay timer.
// ============================================================

typedef void (*LoaderLoop_Exit_t)(LoaderLoop* self);

static void Reimpl_Exit(LoaderLoop* self) {
    g_lastHook = "LoaderLoop::Exit";
#ifdef _DEBUG
    printf("[LoaderLoop::Exit] Force transition to nextLoop=%p\n", (void*)self->m_nextLoop);
#endif
    MainLoop* mainLoop = reinterpret_cast<MainLoop*>(*g_pMainLoopSingleton);
    mainLoop->m_nextLoop = self->m_nextLoop;
}

// ============================================================
// LoaderLoop::IsFinished (0x1800DC150)
// Returns true if loader thread has finished AND minimum display
// delay has elapsed. Does NOT check m_finished field.
// ============================================================

typedef bool (*LoaderLoop_IsFinished_t)(LoaderLoop* self);

static bool Reimpl_IsFinished(LoaderLoop* self) {
    g_lastHook = "LoaderLoop::IsFinished";
    if ((*g_pLoaderFlags & 1) == 0 &&
        self->m_delayTicks <= self->m_elapsedTicks)
    {
        return true;
    }
    return false;
}

// ============================================================
// LoaderLoop::OnVideoError (0x1800DC360)
// Same as Shutdown — waits for loader thread to finish before
// handling video error recovery.
// ============================================================

typedef void (*LoaderLoop_OnVideoError_t)(LoaderLoop* self);

static void Reimpl_OnVideoError(LoaderLoop* /*self*/) {
    g_lastHook = "LoaderLoop::OnVideoError";
#ifdef _DEBUG
    printf("[LoaderLoop::OnVideoError] Waiting for loader completion...\n");
#endif
    WaitForSingleObject(*g_pLoaderCompletionEvent, INFINITE);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallLoaderLoopStartup() {
    g_pMainLoopSingleton = reinterpret_cast<void**>(
        ResolveAddr(Module::Runtime, MainLoopAddr::Singleton));

    uintptr_t target = ResolveAddr(Module::Runtime, LoaderLoopAddr::Startup);
    // Prologue: 16 bytes (verified against binary, contains RIP-relative instr)
    InstallHook(target, (void*)Reimpl_Startup, &g_hookStartup, 16);
#ifdef _DEBUG
    printf("[LoaderLoop] Startup hooked at %p\n", (void*)target);
#endif
}

void RemoveLoaderLoopStartup() {
    RemoveHook(&g_hookStartup);
#ifdef _DEBUG
    printf("[LoaderLoop] Startup hook removed\n");
#endif
}

void InstallLoaderLoopShutdown() {
    uintptr_t target = ResolveAddr(Module::Runtime, LoaderLoopAddr::Shutdown);
    // Prologue: 17 bytes (verified against binary, contains RIP-relative instr)
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 17);
#ifdef _DEBUG
    printf("[LoaderLoop] Shutdown hooked at %p\n", (void*)target);
#endif
}

void RemoveLoaderLoopShutdown() {
    RemoveHook(&g_hookShutdown);
#ifdef _DEBUG
    printf("[LoaderLoop] Shutdown hook removed\n");
#endif
}

void InstallLoaderLoopUpdate() {
    g_pMainLoopSingleton = reinterpret_cast<void**>(
        ResolveAddr(Module::Runtime, MainLoopAddr::Singleton));

    uintptr_t target = ResolveAddr(Module::Runtime, LoaderLoopAddr::Update);
    // Prologue: 13 bytes (verified against binary)
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 13);
#ifdef _DEBUG
    printf("[LoaderLoop] Update hooked at %p\n", (void*)target);
#endif
}

void RemoveLoaderLoopUpdate() {
    RemoveHook(&g_hookUpdate);
#ifdef _DEBUG
    printf("[LoaderLoop] Update hook removed\n");
#endif
}

void InstallLoaderLoopDraw() {
    g_pMainLoopSingleton = reinterpret_cast<void**>(
        ResolveAddr(Module::Runtime, MainLoopAddr::Singleton));

    uintptr_t target = ResolveAddr(Module::Runtime, LoaderLoopAddr::Draw);
    // Prologue: 19 bytes (verified against binary, contains RIP-relative instr)
    InstallHook(target, (void*)Reimpl_Draw, &g_hookDraw, 19);
#ifdef _DEBUG
    printf("[LoaderLoop] Draw hooked at %p\n", (void*)target);
#endif
}

void RemoveLoaderLoopDraw() {
    RemoveHook(&g_hookDraw);
#ifdef _DEBUG
    printf("[LoaderLoop] Draw hook removed\n");
#endif
}

void InstallLoaderLoopExit() {
    g_pMainLoopSingleton = reinterpret_cast<void**>(
        ResolveAddr(Module::Runtime, MainLoopAddr::Singleton));

    uintptr_t target = ResolveAddr(Module::Runtime, LoaderLoopAddr::Exit);
    // Prologue: 15 bytes (verified against binary, contains RIP-relative instr)
    InstallHook(target, (void*)Reimpl_Exit, &g_hookExit, 15);
#ifdef _DEBUG
    printf("[LoaderLoop] Exit hooked at %p\n", (void*)target);
#endif
}

void RemoveLoaderLoopExit() {
    RemoveHook(&g_hookExit);
#ifdef _DEBUG
    printf("[LoaderLoop] Exit hook removed\n");
#endif
}

void InstallLoaderLoopIsFinished() {
    uintptr_t target = ResolveAddr(Module::Runtime, LoaderLoopAddr::IsFinished);
    // Prologue: 13 bytes (verified against binary, contains RIP-relative instr)
    InstallHook(target, (void*)Reimpl_IsFinished, &g_hookIsFinished, 13);
#ifdef _DEBUG
    printf("[LoaderLoop] IsFinished hooked at %p\n", (void*)target);
#endif
}

void RemoveLoaderLoopIsFinished() {
    RemoveHook(&g_hookIsFinished);
#ifdef _DEBUG
    printf("[LoaderLoop] IsFinished hook removed\n");
#endif
}

void InstallLoaderLoopOnVideoError() {
    uintptr_t target = ResolveAddr(Module::Runtime, LoaderLoopAddr::OnVideoError);
    // Prologue: 17 bytes (verified against binary, contains RIP-relative instr)
    InstallHook(target, (void*)Reimpl_OnVideoError, &g_hookOnVideoError, 17);
#ifdef _DEBUG
    printf("[LoaderLoop] OnVideoError hooked at %p\n", (void*)target);
#endif
}

void RemoveLoaderLoopOnVideoError() {
    RemoveHook(&g_hookOnVideoError);
#ifdef _DEBUG
    printf("[LoaderLoop] OnVideoError hook removed\n");
#endif
}
