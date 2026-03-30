// engine/reimpl/gameupdateloop.cpp
// Reimplementation of GameUpdateLoop vtable methods from Application-x64.exe.
//
// GameUpdateLoop (0x160 bytes), vtable at 0x14015e2f0.
// Extends UpdateLoop (0x28 base).
//
// Vtable overrides (exe-range functions):
//   +0x00 (Destroy):     0x140019DD0 — Scalar deleting destructor (19-byte prologue)
//   +0x08 (Startup):     0x140097C00 — Initialize game systems (14-byte prologue)
//   +0x10 (Shutdown):    0x1400977C0 — Tear down game systems (12-byte prologue)
//   +0x18 (Update):      0x140096380 — Game tick (VTABLE PATCHED — Bug Class 3)
//   +0x20 (Draw):        0x140096010 — Render frame (VTABLE PATCHED — Bug Class 3)
//   +0x28 (DrawFailed):  0x140096000 — 3 bytes, ret (too small to hook)
//   +0x30 (Load):        0x140096B00 — Load resources (16-byte prologue, LEA RBP variant safe)
//   +0x38 (SaveRestore): 0x1400959C0 — Save/restore (MOV RAX,RSP — Bug Class 3, UNHOOKABLE)

#include "UpdateLoop.h"
#include "GameUpdateLoop.h"
#include "engine_funcs.h"
#include "hooks.h"
#include "resolve.h"
#include <cstdio>
#include <cstdint>
#include <windows.h>

// ============================================================
// Address constants
// ============================================================
namespace GameUpdateLoopAddr {
    constexpr uintptr_t Vtable      = 0x14015e2f0;
    constexpr uintptr_t Destroy     = 0x140019DD0;
    constexpr uintptr_t Startup     = 0x140097C00;
    constexpr uintptr_t Shutdown    = 0x1400977C0;
    constexpr uintptr_t Update      = 0x140096380;  // Bug Class 3 — MOV RAX,RSP
    constexpr uintptr_t Draw        = 0x140096010;  // Bug Class 3 — MOV RAX,RSP
    constexpr uintptr_t DrawFailed  = 0x140096000;  // 3 bytes — unhookable
    constexpr uintptr_t Load        = 0x140096B00;
    constexpr uintptr_t SaveRestore = 0x1400959C0;  // Bug Class 3 — MOV RAX,RSP
}

// ============================================================
// Hook state (prologue hooks)
// ============================================================
static HookInfo g_hookDestroy;
static HookInfo g_hookStartup;
static HookInfo g_hookShutdown;
static HookInfo g_hookLoad;

// ============================================================
// Vtable patch state (for Bug Class 3 functions)
// ============================================================
// Instead of prologue hooking, we overwrite the vtable entry directly.
// The original function address is saved so we can call it or restore it.
static void* g_origUpdate = nullptr;
static void* g_origDraw = nullptr;
static uintptr_t g_vtableAddr = 0;  // resolved vtable address

// ============================================================
// Forward declarations for clean reimpl functions
// (defined in code/Game/Game/gameupdateloop.cpp)
// ============================================================
extern "C" bool GameUpdateLoop_Update(GameUpdateLoop* self, void* time);
extern "C" void GameUpdateLoop_Draw(GameUpdateLoop* self, float alpha);
extern "C" void GameUpdateLoop_Startup(GameUpdateLoop* self);
extern "C" void GameUpdateLoop_Shutdown(GameUpdateLoop* self);
extern "C" void* GameUpdateLoop_Destroy(GameUpdateLoop* self, uint32_t flags);

// ============================================================
// Prologue-hooked wrappers (call clean reimpl)
// ============================================================

// Destroy (0x140019DD0) — scalar deleting destructor
// Prologue: MOV [RSP+8],RCX; PUSH RDI; SUB RSP,0x30; MOV [RSP+0x20],-2
// 19 bytes, Bug Class 4 (RSP-relative store) — safe for passthrough
// Delegates to clean reimpl.
typedef void* (*fn_Destroy_t)(void* self, uint32_t flags);
static void* Reimpl_Destroy(void* self, uint32_t flags) {
    g_lastHook = "GameUpdateLoop::Destroy";
    return GameUpdateLoop_Destroy((GameUpdateLoop*)self, flags);
}

// Startup (0x140097C00) — initialize game systems
// Delegates to clean reimpl. ExternalList::Find crash fixed (dispatch slot fallback).
typedef void (*fn_Startup_t)(void* self);
static void Reimpl_Startup(void* self) {
    g_lastHook = "GameUpdateLoop::Startup";
    GameUpdateLoop_Startup((GameUpdateLoop*)self);
}

// Shutdown (0x1400977C0) — tear down game systems
// Delegates to clean reimpl (paired with Startup).
typedef void (*fn_Shutdown_t)(void* self);
static void Reimpl_Shutdown(void* self) {
    g_lastHook = "GameUpdateLoop::Shutdown";
    GameUpdateLoop_Shutdown((GameUpdateLoop*)self);
}

// Load (0x140096B00) — load resources (759 instructions)
// Prologue: 7 pushes + LEA RBP,[RSP-0x27] + SUB RSP,0x100
// 16 bytes at LEA boundary, Bug Class 3 LEA variant — safe (internally consistent)
typedef void (*fn_Load_t)(void* self);
static void Passthrough_Load(void* self) {
    g_lastHook = "GameUpdateLoop::Load";
    ((fn_Load_t)g_hookLoad.trampoline)(self);
}

// ============================================================
// Vtable-patched wrappers (for Bug Class 3 functions)
// ============================================================
// These are installed by overwriting the vtable entry, NOT by patching
// the function prologue. The original function is called via saved pointer.

// Update (0x140096380) — main simulation tick
// Bug Class 3: MOV RAX,RSP as first instruction — cannot prologue-hook.
// Reimplemented in code/Game/Game/gameupdateloop.cpp.
typedef bool (*fn_Update_t)(void* self, void* time);
static bool Reimpl_Update(void* self, void* time) {
    g_lastHook = "GameUpdateLoop::Update";
    return GameUpdateLoop_Update((GameUpdateLoop*)self, time);
}

// Draw (0x140096010) — render interpolated frame
// Bug Class 3: MOV RAX,RSP as first instruction — cannot prologue-hook.
// Vtable-patched. Delegates to clean reimpl in code/Game/Game/gameupdateloop.cpp.
typedef void (*fn_Draw_t)(void* self, float alpha);
static void Reimpl_Draw(void* self, float alpha) {
    g_lastHook = "GameUpdateLoop::Draw";
    GameUpdateLoop_Draw((GameUpdateLoop*)self, alpha);
}

// ============================================================
// Vtable patching helper
// ============================================================
// Overwrites a single vtable entry with our function pointer.
// Returns the original value for later restoration.
static void* PatchVtableEntry(uintptr_t vtableBase, int slotIndex, void* newFunc) {
    uintptr_t entryAddr = vtableBase + slotIndex * 8;
    void** entry = (void**)entryAddr;

    // Read original
    void* original = *entry;

    // Make writable, patch, restore protection
    DWORD oldProtect;
    VirtualProtect(entry, 8, PAGE_READWRITE, &oldProtect);
    *entry = newFunc;
    VirtualProtect(entry, 8, oldProtect, &oldProtect);

    return original;
}

static void RestoreVtableEntry(uintptr_t vtableBase, int slotIndex, void* original) {
    if (original == nullptr) return;
    uintptr_t entryAddr = vtableBase + slotIndex * 8;
    void** entry = (void**)entryAddr;

    DWORD oldProtect;
    VirtualProtect(entry, 8, PAGE_READWRITE, &oldProtect);
    *entry = original;
    VirtualProtect(entry, 8, oldProtect, &oldProtect);
}

// ============================================================
// Install / Remove — prologue hooks
// ============================================================

void InstallGameUpdateLoopDestroy() {
    uintptr_t target = ResolveAddr(Module::Exe, GameUpdateLoopAddr::Destroy);
    InstallHook(target, (void*)Reimpl_Destroy, &g_hookDestroy, 19);
#ifdef _DEBUG
    printf("[GameUpdateLoop] Destroy hooked at %p\n", (void*)target);
#endif
}
void RemoveGameUpdateLoopDestroy() { RemoveHook(&g_hookDestroy); }

void InstallGameUpdateLoopStartup() {
    uintptr_t target = ResolveAddr(Module::Exe, GameUpdateLoopAddr::Startup);
    InstallHook(target, (void*)Reimpl_Startup, &g_hookStartup, 14);
#ifdef _DEBUG
    printf("[GameUpdateLoop] Startup hooked at %p\n", (void*)target);
#endif
}
void RemoveGameUpdateLoopStartup() { RemoveHook(&g_hookStartup); }

void InstallGameUpdateLoopShutdown() {
    uintptr_t target = ResolveAddr(Module::Exe, GameUpdateLoopAddr::Shutdown);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 12);
#ifdef _DEBUG
    printf("[GameUpdateLoop] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveGameUpdateLoopShutdown() { RemoveHook(&g_hookShutdown); }

void InstallGameUpdateLoopLoad() {
    uintptr_t target = ResolveAddr(Module::Exe, GameUpdateLoopAddr::Load);
    InstallHook(target, (void*)Passthrough_Load, &g_hookLoad, 16);
#ifdef _DEBUG
    printf("[GameUpdateLoop] Load hooked at %p\n", (void*)target);
#endif
}
void RemoveGameUpdateLoopLoad() { RemoveHook(&g_hookLoad); }

// ============================================================
// Install / Remove — vtable patches (Bug Class 3)
// ============================================================

void InstallGameUpdateLoopUpdate() {
    g_vtableAddr = ResolveAddr(Module::Exe, GameUpdateLoopAddr::Vtable);
    // Vtable slot 3 = +0x18 = Update
    g_origUpdate = PatchVtableEntry(g_vtableAddr, 3, (void*)Reimpl_Update);
    printf("[GameUpdateLoop] Update vtable-patched (slot 3, orig=%p)\n", g_origUpdate);
}
void RemoveGameUpdateLoopUpdate() {
    if (g_vtableAddr && g_origUpdate) {
        RestoreVtableEntry(g_vtableAddr, 3, g_origUpdate);
        printf("[GameUpdateLoop] Update vtable restored\n");
        g_origUpdate = nullptr;
    }
}

void InstallGameUpdateLoopDraw() {
    if (!g_vtableAddr) {
        g_vtableAddr = ResolveAddr(Module::Exe, GameUpdateLoopAddr::Vtable);
    }
    // Vtable slot 4 = +0x20 = Draw
    g_origDraw = PatchVtableEntry(g_vtableAddr, 4, (void*)Reimpl_Draw);
    printf("[GameUpdateLoop] Draw vtable-patched (slot 4, orig=%p)\n", g_origDraw);
}
void RemoveGameUpdateLoopDraw() {
    if (g_vtableAddr && g_origDraw) {
        RestoreVtableEntry(g_vtableAddr, 4, g_origDraw);
        printf("[GameUpdateLoop] Draw vtable restored\n");
        g_origDraw = nullptr;
    }
}
