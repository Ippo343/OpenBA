// code/Game/Player/placementtool.cpp
// Clean reimplementation of PlacementTool vtable methods from Application-x64.exe.
//
// PlacementTool (0x70 bytes) extends Tool (0x28 bytes) directly (NOT DragTool).
// Handles building placement: rotation, cost preview, mesh rendering.
//
// Vtable at 0x140161d20 (13 slots). Currently one is reimplemented here:
//   Slot0  Dtor     (0x14002dc90) — scalar deleting destructor
//
// Remaining hooked slots delegate to trampoline (too many unresolved engine APIs):
//   Slot4  Startup  (0x1400fe580) — find 4 components + setup mesh instance
//   Slot5  Shutdown (0x1400fe4e0) — cleanup scene objects + mesh + dialog
//   Slot6  OnKey    (0x1400fe2a0) — rotation keys + placement input
//   Slot7  Serialize(0x1400fdee0) — placement logic + cost UI update
//   Slot8  Draw     (0x1400fdcc0) — render preview geometry
//
// Decompiled from Application-x64.exe.

#include "Player/placementtool_funcs.h"
#include "PlacementTool.h"
#include "Tool.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// PlacementTool vtable addresses (for vtable chain in Dtor)
// ============================================================
static void* s_vtable_PlacementTool = nullptr;
static void* s_vtable_Tool          = nullptr;

static void EnsureVtables() {
    if (!s_vtable_PlacementTool) {
        s_vtable_PlacementTool = (void*)ResolveAddr(Module::Exe, PlacementToolAddr::Vtable);
        s_vtable_Tool          = (void*)ResolveAddr(Module::Exe, ToolAddr::Vtable);
    }
}

// ============================================================
// Object::Internal::~Internal — called from base dtor
// Resolved via the function pointer slot at 0x140152770
// ============================================================
typedef void (*fn_Internal_Dtor_t)(void* self);
static fn_Internal_Dtor_t s_fn_Internal_Dtor = nullptr;

static void EnsureInternalDtor() {
    if (!s_fn_Internal_Dtor) {
        uintptr_t slot = ResolveAddr(Module::Exe, 0x140152770);
        if (slot) s_fn_Internal_Dtor = *(fn_Internal_Dtor_t*)slot;
    }
}

// ============================================================
// PlacementTool::Dtor (vtable +0x00, Slot0) — Scalar deleting destructor
// ============================================================
// Resets vtable chain: PlacementTool -> Tool, then calls
// Object::Internal::~Internal for base cleanup. If flags & 1,
// frees the memory allocation.
//
// NOTE: PlacementTool does NOT inherit DragTool, so the vtable chain
// skips directly from PlacementTool to Tool (no intermediate DragTool step).
//
// Original: 0x14002dc90
void* PlacementTool_Dtor_Impl(PlacementTool* self, uint32_t flags) {
    EnsureVtables();
    EnsureInternalDtor();

    // Walk vtable chain down through inheritance (mirrors MSVC destructor pattern)
    self->vtable = s_vtable_PlacementTool;
    self->vtable = s_vtable_Tool;

    // Call Object::Internal::~Internal
    if (s_fn_Internal_Dtor) {
        s_fn_Internal_Dtor(self);
    }

    // Conditionally free memory
    if (flags & 1) {
        fn_MemFreeStatic(self);
    }

    return self;
}
