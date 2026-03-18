#pragma once
// Function declarations for deeply reimplemented ForesterWork methods.
// Called from engine/reimpl/foresterwork.cpp wrappers.
// No dependency on hooks.h -- portable to standalone exe.

#include "ForesterWork.h"
#include <cstdint>

// ============================================================
// ForesterWork — Forester resource gathering work action (0x60 bytes)
// Inherits: Object::Internal -> Work -> AutoPickupWork (0x48) -> ForesterWork
// Vtable: 0x1401613a0 (12 slots)
// ============================================================

// Slot4 (vtable +0x20): Main work-assignment logic (599 instructions).
// Checks auto-pickup assignments, finds nearby trees, checks terrain,
// assigns forester to cut or plant tasks.
// Returns: 0=continue, 1=done, 2=spawn. Uses int return type (bug class 7).
int ForesterWork_Slot4_Impl(ForesterWork* self, void* entityIBP, uint64_t param3, uint64_t param4);

// Resolve all function pointers needed by ForesterWork.
// Called once during hook installation.
bool ForesterWork_ResolveFuncs();
