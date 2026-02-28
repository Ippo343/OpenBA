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
#include "engine_funcs.h"
#include "hooks.h"
#include "resolve.h"
#include <cstdio>
#include <cstring>

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
// SetLoadProgress (0x1800317A0) [4 bytes — too small to hook]
// ============================================================

/* NOT HOOKED — function is 4 bytes (mov [rcx+0x24],edx; ret).
static void Reimpl_SetLoadProgress(UpdateLoop* self, int32_t progress) {
    self->m_loadProgress = progress;
}
*/

// ============================================================
// GetLoadProgress (0x1800317B0) [4 bytes — too small to hook]
// ============================================================

/* NOT HOOKED — function is 4 bytes (mov eax,[rcx+0x24]; ret).
static int32_t Reimpl_GetLoadProgress(UpdateLoop* self) {
    return self->m_loadProgress;
}
*/

// ============================================================
// GetLoadBit (0x1800317C0) [4 bytes — too small to hook]
// ============================================================

/* NOT HOOKED — function is 4 bytes (mov eax,[rcx+0x20]; ret).
static int32_t Reimpl_GetLoadBit(UpdateLoop* self) {
    return self->m_loadBit;
}
*/

// ============================================================
// UpdateLoop::UpdateLoop (ctor) (0x1800DDD30) [hookable, prologue=17]
// Initializes all fields, allocates a bit from ExternalFactory bitmask.
// ============================================================

// Vtable for UpdateLoop base class (resolved at install time)
static void* g_pUpdateLoopVtable = nullptr;

static HookInfo g_hookUpdateLoopCtor;

static UpdateLoop* Reimpl_UpdateLoopCtor(UpdateLoop* self) {
#ifdef _DEBUG
    printf("[UpdateLoop::Ctor] self=%p\n", (void*)self);
#endif

    // Set vtable
    *reinterpret_cast<void**>(self) = g_pUpdateLoopVtable;

    // Zero fields
    *reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(self) + 0x08) = 0;
    *reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(self) + 0x10) = 0;
    *reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(self) + 0x18) = 0;
    self->m_loadProgress = 0;

    // Allocate a bit from ExternalFactory bitmask
    void* factory = *g_pExternalFactorySingleton;
    uint32_t* bitmask = reinterpret_cast<uint32_t*>(
        reinterpret_cast<uint8_t*>(factory) + 4);

    uint32_t bit = 0;
    while (bit < 32) {
        if ((*bitmask >> bit & 1) == 0) {
            *bitmask |= (1u << bit);
            break;
        }
        bit++;
    }
    if (bit >= 32) {
        bit = 0xFFFFFFFF;  // no free bits
    }
    self->m_loadBit = static_cast<int32_t>(bit);

    return self;
}

// ============================================================
// UpdateLoop::~UpdateLoop (dtor) (0x1800DDDB0) [hookable, prologue=19]
// Frees bitmask bit, calls ExternalFactory::Clean, cleans internal list.
// ============================================================

static HookInfo g_hookUpdateLoopDtor;

static void Reimpl_UpdateLoopDtor(UpdateLoop* self) {
#ifdef _DEBUG
    printf("[UpdateLoop::Dtor] self=%p loadBit=%d\n", (void*)self, self->m_loadBit);
#endif

    // Reset vtable to base class
    *reinterpret_cast<void**>(self) = g_pUpdateLoopVtable;

    void* factory = *g_pExternalFactorySingleton;

    if (self->m_loadBit >= 0) {
        // Store bit index in factory+0 (used by Clean)
        *reinterpret_cast<int32_t*>(factory) = self->m_loadBit;

        // Clean resources for this bit
        fn_ExternalFactory_Clean(factory);

        // Clear the bit in the bitmask
        uint32_t* bitmask = reinterpret_cast<uint32_t*>(
            reinterpret_cast<uint8_t*>(factory) + 4);
        *bitmask &= ~(1u << (self->m_loadBit & 0x1f));
    }

    // Clean up internal list at offset 0x10
    fn_ListCleanup(reinterpret_cast<uint8_t*>(self) + 0x10);
}

// ============================================================
// UpdateLoop::LoadResources (0x1800DD7D0) — NOT REIMPLEMENTED
// Too complex (uses FileSystem, Modification, String ops).
// Called via fn_UpdateLoop_LoadResources function pointer.
// ============================================================

// ============================================================
// LoaderLoop::Load (0x1800DC350) [3 bytes — too small to hook]
// No-op stub — LoaderLoop doesn't load External resources.
// ============================================================

/* NOT HOOKED — function is 3 bytes (ret 0x0).
static void Reimpl_LoaderLoopLoad(void* self) {
    // no-op
}
*/

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

// --- UpdateLoop::Ctor (0x1800DDD30, prologue=17) ---

void InstallUpdateLoopCtor() {
    g_pUpdateLoopVtable = reinterpret_cast<void*>(
        ResolveAddr(Module::Runtime, 0x180155ba0));
    uintptr_t target = ResolveAddr(Module::Runtime, UpdateLoopAddr::Ctor);
    InstallHook(target, (void*)Reimpl_UpdateLoopCtor, &g_hookUpdateLoopCtor, 17);
#ifdef _DEBUG
    printf("[UpdateLoop] Ctor hooked at %p (vtable=%p)\n", (void*)target, g_pUpdateLoopVtable);
#endif
}
void RemoveUpdateLoopCtor() { RemoveHook(&g_hookUpdateLoopCtor); }

// --- UpdateLoop::Dtor (0x1800DDDB0, prologue=19) ---

void InstallUpdateLoopDtor() {
    if (!g_pUpdateLoopVtable) {
        g_pUpdateLoopVtable = reinterpret_cast<void*>(
            ResolveAddr(Module::Runtime, 0x180155ba0));
    }
    uintptr_t target = ResolveAddr(Module::Runtime, UpdateLoopAddr::Dtor);
    InstallHook(target, (void*)Reimpl_UpdateLoopDtor, &g_hookUpdateLoopDtor, 19);
#ifdef _DEBUG
    printf("[UpdateLoop] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveUpdateLoopDtor() { RemoveHook(&g_hookUpdateLoopDtor); }
