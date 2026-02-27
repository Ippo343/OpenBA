#pragma once
// Engine::UpdateLoop — base class for all loop types.
// Runtime-x64.dll, constructor at 0x1800DDD30, destructor at 0x1800DDDB0.
// MSVC single-inheritance layout: vtable pointer at offset 0.
// Struct size: 0x28 bytes. Vtable at 0x180155BA0 (12 entries).
//
// Known subclasses:
//   - LoaderLoop          (Runtime, 0x48 bytes)  — loading screen with background thread
//   - DialogLoaderLoopT   (Runtime, 0x188+ bytes) — loading screen with UI dialog
//   - DialogLoopT         (Runtime, 0x168+ bytes) — menu/dialog with UI
//   - Gameplay UpdateLoop (Exe, 0x160 bytes, ctor 0x140098060) — main game tick
//
// Vtable verified against actual binary at 0x180155BA0.

#include <cstdint>
#include <cstddef>

// Forward declarations
struct UpdateLoop;

// ============================================================
// UpdateLoop vtable — 12 entries, verified from binary
// ============================================================
// Slots 0-5: called by MainLoop::Run/Frame/Update/Draw
// Slots 6-11: called by LoadResources, SaveLoad, video recovery, etc.
//
// Note on Startup: the binary signature is void Startup() with no parameters.
// MainLoop::Run passes a remainder value in RDX, but all known implementations
// ignore it. The VCall helpers in mainloop.cpp pass it harmlessly via Win64 ABI.

struct UpdateLoopVtable {
    void* Destroy;              // +0x00: void(UpdateLoop* this, uint flags)  — scalar deleting destructor
    void* Startup;              // +0x08: void(UpdateLoop* this)              — called when loop becomes active
    void* Shutdown;             // +0x10: void(UpdateLoop* this)              — called when loop is deactivated
    void* Update;               // +0x18: bool(UpdateLoop* this, Time const&) — tick game logic, return false to quit
    void* Draw;                 // +0x20: void(UpdateLoop* this, float alpha) — render interpolated frame
    void* DrawFailed;           // +0x28: void(UpdateLoop* this)              — called on device lost (noop in base)
    void* Load;                 // +0x30: void(UpdateLoop* this)              — pure virtual: load resources
    void* SaveRestore;          // +0x38: void(UpdateLoop* this, bool save, void* savedState) — noop in base
    void* OnVideoError;         // +0x40: void(UpdateLoop* this)              — noop in base
    void* OnVideoRestart;       // +0x48: void(UpdateLoop* this)              — noop in base
    void* GetResourceFilename;  // +0x50: const char*(UpdateLoop* this)       — returns null in base
    void* AllowsModChanges;     // +0x58: bool(UpdateLoop* this)              — returns false in base
};
static_assert(sizeof(UpdateLoopVtable) == 0x60, "UpdateLoopVtable must be 12 pointers (0x60 bytes)");

// ============================================================
// UpdateLoop struct — 0x28 bytes
// ============================================================
// Fields deduced from constructor at 0x1800DDD30 and method access patterns.

struct UpdateLoop {
    UpdateLoopVtable* vtable;   // +0x00: MSVC vptr
    void* m_resourceList;       // +0x08: ExternalList* — loaded by LoadResources()
    void* m_internalListA;      // +0x10: managed by FUN_180010040/FUN_180010ab0
    void* m_internalListB;      // +0x18: cleared in ctor/dtor
    int32_t m_loadBit;          // +0x20: ExternalFactory bit mask index (-1 = unassigned)
    int32_t m_loadProgress;     // +0x24: load progress (0-100), accessed by Get/SetLoadProgress
};
static_assert(sizeof(UpdateLoop) == 0x28, "UpdateLoop must be 0x28 bytes");
static_assert(offsetof(UpdateLoop, vtable) == 0x00, "vtable at +0x00");
static_assert(offsetof(UpdateLoop, m_resourceList) == 0x08, "m_resourceList at +0x08");
static_assert(offsetof(UpdateLoop, m_internalListA) == 0x10, "m_internalListA at +0x10");
static_assert(offsetof(UpdateLoop, m_internalListB) == 0x18, "m_internalListB at +0x18");
static_assert(offsetof(UpdateLoop, m_loadBit) == 0x20, "m_loadBit at +0x20");
static_assert(offsetof(UpdateLoop, m_loadProgress) == 0x24, "m_loadProgress at +0x24");
