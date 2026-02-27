// engine/reimpl/updateloop.cpp
// Reimplementation of Engine::UpdateLoop base class methods from Runtime-x64.dll.
//
// UpdateLoop is the abstract base for all game loops (loading screens, menus,
// gameplay). Most methods are pure virtual — overridden in subclasses. The base
// class provides resource loading, progress tracking, and an "is running" check.
//
// Methods reimplemented:
//   - UpdateLoop::IsRunning (0x1800DD7B0) — check if this is the active loop
//
// Methods documented but NOT hooked (trivial accessors or complex dependencies):
//   - GetLoadProgress (0x1800317B0) — return m_loadProgress
//   - SetLoadProgress (0x1800317A0) — write m_loadProgress
//   - GetLoadBit      (0x1800317C0) — return m_loadBit
//   - LoadResources   (0x1800DD7D0) — complex, depends on ExternalFactory/String/Modification
//   - Constructor     (0x1800DDD30) — allocates from ExternalFactory bitmask
//   - Destructor      (0x1800DDDB0) — frees ExternalFactory bitmask bit

#include "UpdateLoop.h"
#include "MainLoop.h"
#include "hooks.h"
#include "resolve.h"
#include <cstdio>

// ============================================================
// Resolved globals
// ============================================================

// MainLoop singleton pointer (MainLoop** -> dereference to get MainLoop*)
static MainLoop** g_pMainLoopSingleton = nullptr;

// ============================================================
// UpdateLoop address constants
// ============================================================
namespace UpdateLoopAddr {
    constexpr uintptr_t Ctor             = 0x1800DDD30;
    constexpr uintptr_t Dtor             = 0x1800DDDB0;
    constexpr uintptr_t IsRunning        = 0x1800DD7B0;
    constexpr uintptr_t LoadResources    = 0x1800DD7D0;
    constexpr uintptr_t SetLoadProgress  = 0x1800317A0;
    constexpr uintptr_t GetLoadProgress  = 0x1800317B0;
    constexpr uintptr_t GetLoadBit       = 0x1800317C0;
}

// ============================================================
// Hook state
// ============================================================

static HookInfo g_hookIsRunning;

// ============================================================
// UpdateLoop::IsRunning (0x1800DD7B0)
// Returns true if this UpdateLoop is the currently active loop
// in the MainLoop singleton.
//
// Original: ?IsRunning@UpdateLoop@Engine@@QEBA_NXZ
// ============================================================

typedef bool (*UpdateLoop_IsRunning_t)(UpdateLoop* self);

static bool Reimpl_IsRunning(UpdateLoop* self) {
    if (!self) return false;

    MainLoop* mainLoop = *g_pMainLoopSingleton;
    if (!mainLoop) return false;

    return (self == mainLoop->m_currentLoop);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallUpdateLoopIsRunning() {
    // Resolve MainLoop singleton pointer
    g_pMainLoopSingleton = reinterpret_cast<MainLoop**>(
        ResolveAddr(Module::Runtime, MainLoopAddr::Singleton));

    uintptr_t target = ResolveAddr(Module::Runtime, UpdateLoopAddr::IsRunning);
    InstallHook(target, (void*)Reimpl_IsRunning, &g_hookIsRunning);

#ifdef _DEBUG
    printf("[UpdateLoop] IsRunning hooked at %p\n", (void*)target);
    printf("[UpdateLoop]   MainLoop singleton @ %p -> %p\n",
           (void*)g_pMainLoopSingleton, (void*)*g_pMainLoopSingleton);
#endif
}

void RemoveUpdateLoopIsRunning() {
    RemoveHook(&g_hookIsRunning);
#ifdef _DEBUG
    printf("[UpdateLoop] IsRunning hook removed\n");
#endif
}
