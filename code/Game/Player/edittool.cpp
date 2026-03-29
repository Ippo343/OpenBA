// code/Game/Player/edittool.cpp
// Clean reimplementation of EditTool vtable methods from Application-x64.exe.
//
// EditTool (0x38 bytes) extends Tool (0x28 bytes) directly.
// Handles entity selection and editing in-game.
//
// Vtable at 0x140161ca0 (13 slots). Two are reimplemented here:
//   Slot0   Dtor      (0x14002db00) — scalar deleting destructor
//   Slot10  Serialize  (0x1400fd290) — compare entity reference
//
// Remaining hooked slots delegate to trampoline (too many unresolved engine APIs):
//   Slot6  OnKey    (0x1400fd030) — entity selection via ray picking
//   Slot7  Startup  (0x1400fcda0) — dialog positioning near entity
//
// Decompiled from Application-x64.exe.

#include "Player/edittool_funcs.h"
#include "EditTool.h"
#include "Tool.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// EditTool vtable addresses (for vtable chain in Dtor)
// ============================================================
static void* s_vtable_EditTool = nullptr;
static void* s_vtable_Tool     = nullptr;

static void EnsureVtables() {
    if (!s_vtable_EditTool) {
        s_vtable_EditTool = (void*)ResolveAddr(Module::Exe, EditToolAddr::Vtable);
        s_vtable_Tool     = (void*)ResolveAddr(Module::Exe, ToolAddr::Vtable);
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
// EditTool::Dtor (vtable +0x00, Slot0) — Scalar deleting destructor
// ============================================================
// Resets vtable chain: EditTool -> Tool, then calls
// Object::Internal::~Internal for base cleanup. If flags & 1,
// frees the memory allocation.
//
// Original: 0x14002db00
void* EditTool_Dtor_Impl(EditTool* self, uint32_t flags) {
    EnsureVtables();
    EnsureInternalDtor();

    // Walk vtable chain down through inheritance (mirrors MSVC destructor pattern)
    self->vtable = s_vtable_EditTool;
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

// ============================================================
// EditTool::Serialize (vtable +0x50, Slot10) — Compare entity reference
// ============================================================
// Calls InternalBasePtr::CheckPtr on self->lastEntityRef (+0x30),
// then compares *param_2 with self->lastEntityRef.
// Returns true if the entity references match.
//
// This is used to check whether a given entity is the one currently
// being edited by this tool (e.g., for highlight/selection state).
//
// Original: 0x1400fd290
bool EditTool_Serialize_Impl(EditTool* self, uint64_t* param_2) {
    // CheckPtr on self->lastEntityRef to resolve lazy/weak pointers
    if (fn_InternalBasePtr_CheckPtr) {
        fn_InternalBasePtr_CheckPtr((void*)&self->lastEntityRef);
    }

    // Compare: does *param_2 match self->lastEntityRef?
    return *param_2 == self->lastEntityRef;
}
